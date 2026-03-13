#include "kernel/celltypes.h"
#include "kernel/consteval.h"
#include "kernel/modtools.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"
#include <algorithm>
#include <limits>
#include <numeric>
#include <queue>

USING_YOSYS_NAMESPACE
using namespace std;
PRIVATE_NAMESPACE_BEGIN

// 全局配置参数
int LUT_SIZE = 6;              // K-可行性约束：最大输入数量
static int MAX_CUTS_PER_NODE = 64;    // 每个节点保留的最大割集数量（ABC风格：增加到64）
static int MAPPING_ROUNDS = 4;        // ABC风格：多轮迭代优化
static int MAX_PAIRS_PER_NODE = 32;   // 每个节点最多评估的双输出配对数量
static int MAX_DISJOINT_PAIRS_PER_NODE = 24; // 每节点保留的”无共享输入”配对上限
static int LAYER_CUTS_PER_NODE = 4;   // 层覆盖阶段每节点参与配对的cut数量
static int MAX_I5_CANDIDATES = 6;     // 每个候选最多尝试的I5数量
static int PAIR_SEED_MULTIPLIER = 10; // 每节点候选配对预筛上限倍数（相对MAX_PAIRS_PER_NODE）
static int MIN_SHARED_INPUTS_FOR_DUAL = 0; // 双输出候选的最小共享输入数
static double MIN_AREA_GAIN_FOR_DUAL = -0.15;      // 双输出最小面积收益
static double MIN_AREA_GAIN_FOR_WEAK_SHARE = -0.25; // 共享输入较少时阈值
static double MIN_AREA_GAIN_FOR_NO_SHARE = -0.80;   // 无共享输入时阈值（面积估计不稳，大幅放宽）
static int EXTRA_MODE_GATE_LIMIT = 1500; // 仅中小规模网络运行最贵的附加覆盖模式
static float AREA_ROUND_REQUIRED_SLACK = 1.0f; // area轮允许的绝对时序松弛（层级）
static float AREA_ROUND_REQUIRED_RELAX = 0.05f; // area轮允许的相对时序松弛
static int FINAL_AREA_RECOVERY_ROUNDS = 2; // 末尾额外面积恢复轮次（不再强约束timing）
static int POSTPACK_PASS1_SCAN_WINDOW = 0; // 第一轮后处理配对扫描窗口，0表示全量扫描
static int POSTPACK_PASS2_MAX_SINGLE = 512; // 二次后处理候选上限（按输入数排序截断），0表示不截断
static int POSTPACK_PASS2_SCAN_WINDOW = 0; // 第二轮后处理配对扫描窗口，0表示全量扫描
static int POSTPACK_PASS2_MAX_CHUNKS = 1; // 第二轮后处理最多处理的候选分块数
static float EPSILON = 0.005f;        // 浮点比较精度
static int layer_invalid_log_count = 0;

struct Cut;
Cut MergeCuts(const Cut &cut1, const Cut &cut2);
bool RoleValid(Cell* z_node, Cell* z5_node, SigBit i5, const Cut& merged_cut);

SigBit GetCellOutput(Cell *cell);

static size_t DualLutInitWidth(size_t input_count)
{
    if (input_count < 4 || input_count > 6)
        return 0;
    return size_t(1) << input_count;
}

SigMap sigmap;
dict<SigBit, Cell *> bit2driver;
dict<SigBit, vector<Cell *>> bit2reader;
dict<Cell*, vector<SigBit>> cell2bits;
dict<Cell*, vector<Cut>> cell2cuts;  // 每个节点的割集列表
dict<Cell*, size_t> cell2depth;  // 节点深度（level）
dict<Cell*, size_t> cell2level;  // 节点在拓扑排序中的层级
dict<Cell*, float> cell2arrival;  // 节点的arrival time
dict<Cell*, float> cell2required;  // 节点的required time
dict<Cell*, Cut> cell2bestcut;
dict<Cell*, vector<const Cut*>> layer_candidate_cut_cache; // 缓存层覆盖阶段候选cut
dict<Cell*, int> cell2index;
pool<Cell*> processed_nodes;
pool<Cell*> used_nodes;
pool<SigBit> prime_outputs;
pool<SigBit> prime_inputs;
pool<SigBit> mapped_lut_outputs; // 所有LUT输出信号（需要替换驱动）

// ABC风格：估计引用计数和精确引用计数
dict<Cell*, float> cell2est_refs;     // 估计的fanout引用数
dict<Cell*, int> cell2refs;           // 当前映射中的实际引用数

static bool LessSigBitStable(const SigBit &a, const SigBit &b)
{
    bool a_is_wire = a.wire != nullptr;
    bool b_is_wire = b.wire != nullptr;
    if (a_is_wire != b_is_wire)
        return a_is_wire;

    if (a_is_wire) {
        if (a.wire->name != b.wire->name)
            return a.wire->name < b.wire->name;
        if (a.offset != b.offset)
            return a.offset < b.offset;
        return false;
    }

    if (a.data != b.data)
        return int(a.data) < int(b.data);
    return false;
}

static vector<SigBit> SortedSigBits(const pool<SigBit> &bits)
{
    vector<SigBit> ordered;
    ordered.reserve(bits.size());
    for (auto bit : bits)
        ordered.push_back(bit);
    sort(ordered.begin(), ordered.end(), LessSigBitStable);
    return ordered;
}

static bool LessCellStable(const Cell *a, const Cell *b)
{
    if (a == b)
        return false;
    if (!a)
        return true;
    if (!b)
        return false;

    int a_idx = -1;
    int b_idx = -1;
    Cell *a_mut = const_cast<Cell*>(a);
    Cell *b_mut = const_cast<Cell*>(b);
    if (cell2index.count(a_mut))
        a_idx = cell2index[a_mut];
    if (cell2index.count(b_mut))
        b_idx = cell2index[b_mut];
    if (a_idx >= 0 && b_idx >= 0 && a_idx != b_idx)
        return a_idx < b_idx;

    if (a->name != b->name)
        return a->name < b->name;
    return a->type < b->type;
}

// Cut结构体：表示一个割集
using pairState = vector<Cell*>;

struct Cut {
    pool<SigBit> leaves;      // 割集的叶子节点（输入信号）
    pool<Cell *> internal;     // 割集内部的单元集合
    uint64_t signature;        // ABC风格：64位signature用于快速过滤
    size_t depth;              // 割集的深度（level）
    float arrival_time;        // 割集的arrival time
    double area_flow;          // ABC风格的面积流
    float exact_area;          // 精确面积（用于后续轮次）
    
    Cut() : signature(0), depth(0), arrival_time(0.0f), area_flow(0.0), exact_area(0.0f) {}
    
    // 计算signature（用于快速K-feasibility检查）
    void computeSignature() {
        signature = 0;
        for (auto bit : leaves) {
            // 使用SigBit的hash值映射到64位
            // 使用wire指针和offset组合生成hash
            size_t h = 0;
            if (bit.wire) {
                h = hashlib::legacy::djb2_add(bit.wire->name.index_, bit.offset);
            } else {
                h = (size_t)bit.data ^ 0x12345678;
            }
            signature |= (1ULL << (h % 64));
        }
    }
    
    // 快速检查合并后是否可能满足K-feasibility
    static bool canMergeFast(const Cut& c1, const Cut& c2, int limit) {
        uint64_t combined = c1.signature | c2.signature;
        return __builtin_popcountll(combined) <= limit;
    }
    
    // ABC风格的优先级比较
    // Round 1: delay优先; Round 2+: area优先（在timing约束内）
    bool operator<(const Cut &other) const {
        // 1. 优先选择arrival time小的cut（控制关键路径）
        if (fabs(arrival_time - other.arrival_time) > EPSILON)
            return arrival_time < other.arrival_time;
        // 2. 其次选择area_flow小的cut（ABC核心指标）
        if (fabs(area_flow - other.area_flow) > EPSILON)
            return area_flow < other.area_flow;
        // 3. 最后选择leaves数量少的cut（更紧凑）
        if (leaves.size() != other.leaves.size())
            return leaves.size() < other.leaves.size();

        if (depth != other.depth)
            return depth < other.depth;

        if (signature != other.signature)
            return signature < other.signature;

        vector<SigBit> lhs = SortedSigBits(leaves);
        vector<SigBit> rhs = SortedSigBits(other.leaves);
        return lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), LessSigBitStable);
    }
    
    // 面积优先的比较（用于Round 2+）
    bool betterAreaThan(const Cut &other, float required_time) const {
        float relaxed_required = required_time;
        if (required_time < 1e8f) {
            relaxed_required += AREA_ROUND_REQUIRED_SLACK;
            relaxed_required += required_time * AREA_ROUND_REQUIRED_RELAX;
        }
        // 必须满足timing约束
        if (arrival_time > relaxed_required + EPSILON) return false;
        if (other.arrival_time > relaxed_required + EPSILON) return true;
        // 在timing约束内，选择area更小的
        if (fabs(area_flow - other.area_flow) > EPSILON)
            return area_flow < other.area_flow;
        if (fabs(arrival_time - other.arrival_time) > EPSILON)
            return arrival_time < other.arrival_time;
        // 面积轮次下，优先更“深/大”的cut，通常可减少总覆盖节点数
        if (leaves.size() != other.leaves.size())
            return leaves.size() > other.leaves.size();
        if (depth != other.depth)
            return depth > other.depth;
        return signature < other.signature;
    }
    
    // 检查K-可行性
    bool is_feasible() const {
        return leaves.size() <= size_t(LUT_SIZE);
    }
    
    // 检查是否被另一个割集支配
    bool is_dominated_by(const Cut &other) const {
        if (leaves.size() <= other.leaves.size())
            return false;
        // 检查 other.leaves 是否是 this->leaves 的子集
        for (auto bit : other.leaves) {
            if (!leaves.count(bit))
                return false;
        }
        return true;
    }
};

static size_t DualLutSelectorMask(size_t input_count)
{
    if (input_count == 0)
        return 0;
    return size_t(1) << (input_count - 1);
}

static uint64_t HashSigBitStable(const SigBit &bit)
{
    if (bit.wire) {
        uint64_t h = uint64_t(hashlib::legacy::djb2_add(bit.wire->name.index_, bit.offset));
        h ^= (uint64_t(bit.offset) << 32);
        h ^= (h >> 33);
        h *= 0xff51afd7ed558ccdULL;
        h ^= (h >> 33);
        return h;
    }

    uint64_t h = 0x9e3779b97f4a7c15ULL ^ uint64_t(int(bit.data) + 1);
    h ^= (h >> 33);
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= (h >> 33);
    return h;
}

static uint64_t HashCutLeaves(const pool<SigBit> &leaves)
{
    vector<SigBit> ordered = SortedSigBits(leaves);
    uint64_t h = 1469598103934665603ULL;
    for (auto bit : ordered) {
        h ^= HashSigBitStable(bit);
        h *= 1099511628211ULL;
    }
    h ^= uint64_t(ordered.size()) * 0x9e3779b97f4a7c15ULL;
    h ^= (h >> 32);
    return h;
}

static bool CutLeavesEqual(const pool<SigBit> &a, const pool<SigBit> &b)
{
    if (a.size() != b.size())
        return false;
    for (auto bit : a) {
        if (!b.count(bit))
            return false;
    }
    return true;
}

static bool IsLeafSubset(const pool<SigBit> &small, const pool<SigBit> &large)
{
    if (small.size() > large.size())
        return false;
    for (auto bit : small) {
        if (!large.count(bit))
            return false;
    }
    return true;
}

static size_t DualLutSizeFromCut(const Cut &cut, const SigBit &selector)
{
    size_t data_inputs = cut.leaves.size();
    if (selector.wire && cut.leaves.count(selector) && data_inputs > 0)
        data_inputs -= 1;
    size_t lut_size = data_inputs + 1;
    if (lut_size < 4)
        lut_size = 4;
    if (lut_size > 6)
        lut_size = 6;
    return lut_size;
}

static bool BuildDualLutInputs(const Cut &cut, const SigBit &selector,
                               size_t lut_size, vector<SigBit> &inputs)
{
    inputs.clear();
    vector<SigBit> ordered_leaves = SortedSigBits(cut.leaves);
    for (auto bit : ordered_leaves) {
        if (bit != selector)
            inputs.push_back(bit);
    }
    if (inputs.size() > lut_size - 1)
        return false;
    while (inputs.size() < lut_size - 1)
        inputs.push_back(State::S0);
    inputs.push_back(selector);
    return inputs.size() == lut_size;
}

struct LUT6DInfo {
    SigBit Z5;
    SigBit Z;
    uint64_t INIT;
    vector<SigBit> inputs;
    pool<IdString> removedNodes;  // 存储cell名称而不是指针，避免悬垂指针

    LUT6DInfo() : Z5(State::Sx), Z(State::Sx), INIT(0) {}

    void setLUT6DInfo(SigBit z5, SigBit z, uint64_t init, const vector<SigBit>& inps) {
        Z5 = z5;
        Z = z;
        INIT = init;
        inputs = inps;
    }

    void addLUT6D(Module* module) {
        // 验证inputs数组大小
        // log_assert(inputs.size() == LUT_SIZE);
        IdString type;
        size_t init_width = DualLutInitWidth(inputs.size());
        if (inputs.size() == 6) {
            type = ID(GTP_LUT6D);
        }
        else if (inputs.size() == 5) {
            type = ID(GTP_LUT5D);
        }
        else if (inputs.size() == 4) {
            type = ID(GTP_LUT4D);
        }
        else {
            log_error("LUT6DInfo inputs size is invalid: %zu\n", inputs.size());
        }
        if (init_width == 0) {
            log_error("LUT6DInfo INIT width is invalid for inputs size %zu\n", inputs.size());
        }
        Cell* lutd = module->addCell(NEW_ID, type);
        // 设置INIT
        lutd->setParam(ID(INIT), Const(INIT, init_width));
        
        // 设置输入端口
        lutd->setPort(ID(I0), inputs[0]);
        lutd->setPort(ID(I1), inputs[1]);
        lutd->setPort(ID(I2), inputs[2]);
        lutd->setPort(ID(I3), inputs[3]);
        if (inputs.size() > 4)
            lutd->setPort(ID(I4), inputs[4]);
        if (inputs.size() > 5)
            lutd->setPort(ID(I5), inputs[5]);

        // 设置输出端口（直接使用存储的SigBit）
        lutd->setPort(ID(Z), Z);
        lutd->setPort(ID(Z5), Z5);
    }

};

// 单输出LUT结构体，用于处理未能配对的节点
struct SingleLUTInfo {
    SigBit output;
    uint64_t INIT;
    vector<SigBit> inputs;
    pool<IdString> removedNodes;

    SingleLUTInfo() : output(State::Sx), INIT(0) {}

    void addSingleLUT(Module* module) {
        if (inputs.empty()) {
            log_warning("SingleLUT with no inputs, skipping\n");
            return;
        }
        
        IdString type;
        size_t init_width = 64;
        if (inputs.size() <= 2) {
            type = ID(GTP_LUT2);
            init_width = 4;
            // 填充到2个输入
            while (inputs.size() < 2) inputs.push_back(State::S0);
        } else if (inputs.size() <= 3) {
            type = ID(GTP_LUT3);
            init_width = 8;
            while (inputs.size() < 3) inputs.push_back(State::S0);
        } else if (inputs.size() <= 4) {
            type = ID(GTP_LUT4);
            init_width = 16;
            while (inputs.size() < 4) inputs.push_back(State::S0);
        } else if (inputs.size() <= 5) {
            type = ID(GTP_LUT5);
            init_width = 32;
            while (inputs.size() < 5) inputs.push_back(State::S0);
        } else {
            type = ID(GTP_LUT6);
            init_width = 64;
            while (inputs.size() < 6) inputs.push_back(State::S0);
        }
        
        Cell* lut = module->addCell(NEW_ID, type);
        lut->setParam(ID(INIT), Const(INIT, init_width));
        
        // 设置输入端口
        lut->setPort(ID(I0), inputs[0]);
        lut->setPort(ID(I1), inputs[1]);
        if (inputs.size() > 2) lut->setPort(ID(I2), inputs[2]);
        if (inputs.size() > 3) lut->setPort(ID(I3), inputs[3]);
        if (inputs.size() > 4) lut->setPort(ID(I4), inputs[4]);
        if (inputs.size() > 5) lut->setPort(ID(I5), inputs[5]);
        
        // 设置输出
        lut->setPort(ID(Z), output);
    }
};

bool IsNOT(Cell *cell) {
    return cell->type.in(ID($_NOT_), ID($not));
}
bool IsAND(Cell *cell) {
    return cell->type.in(ID($_AND_), ID($and));
}
bool IsCombinationalGate(Cell *cell) {
    return cell->type.in(ID($_AND_), ID($_NOT_), ID($and), ID($not), ID($lut));
}

void GetCellInputsVector(Cell *cell, vector<SigBit> &inputs)
{
	log_assert(cell && inputs.empty() && cell2bits.count(cell));
	auto &bits = cell2bits[cell];
	int offset = 1;
	// not support LUT6D now
	// if(cell is dual output)
	// offset=2
	for (auto it = bits.begin() + offset; it != bits.end(); ++it) {
		inputs.push_back(*it);
	}
}

SigBit GetCellOutput(Cell *cell)
{
	log_assert(cell && cell2bits.count(cell));
	auto &bits = cell2bits[cell];
	return bits[0];
}

static void CollectMappedLUTOutputs(Module *module, pool<SigBit> &outputs)
{
    outputs.clear();
    for (Cell *cell : module->cells()) {
        if (!cell)
            continue;
        if (!cell->type.in(ID(GTP_LUT2), ID(GTP_LUT3), ID(GTP_LUT4), ID(GTP_LUT5), ID(GTP_LUT6),
                           ID(GTP_LUT6D), ID(GTP_LUT5D), ID(GTP_LUT4D)))
            continue;
        for (auto &conn : cell->connections()) {
            IdString portname = conn.first;
            if (!(portname == ID(Z) || portname == ID(Z5)))
                continue;
            SigSpec sig = sigmap(conn.second);
            for (int i = 0; i < sig.size(); i++) {
                outputs.insert(sig[i]);
            }
        }
    }
}

pool<Cell *> GetReaders(Cell *cell, RTLIL::IdString port)
{
	pool<Cell *> ret;
	log_assert(cell && !port.empty() && cell->connections().count(port));
	SigSpec sig = cell->getPort(port);
	sig = sigmap(sig);
	for (int i = 0; i < sig.size(); i++) {
		SigBit bit = sig[i];
		vector<Cell *> readers = bit2reader[bit];
		for (size_t i = 0; i < readers.size(); i++) {
			ret.insert(readers[i]);
		}
	}
	return ret;
}

bool GetPrimeInputOutput(Module *module, pool<SigBit> &inputs, pool<SigBit> &outputs)
{
	for (auto &cell_iter : module->cells_) {
		Cell *cell = cell_iter.second;
		if (!cell || !IsCombinationalGate(cell)) // only connsider the prime input and output connect to the combinational gate
		{
			continue;
		}
		for (auto &conn : cell->connections()) {
			IdString portname = conn.first;
			RTLIL::SigSpec sig = sigmap(conn.second);
			if (yosys_celltypes.cell_output(cell->type, portname)) {
				pool<Cell *> readers = GetReaders(cell, portname);
				for (Cell *reader : readers) {
					if (!IsCombinationalGate(reader)) {
						outputs.insert(sig[0]);
						break;
					}
				}
				if (readers.size() < 1) {
					outputs.insert(sig[0]);
				}
			} else {
				for (int i = 0; i < sig.size(); i++) {
					SigBit bit = sig[i];
					Cell *drv = bit2driver.count(bit) ? bit2driver[bit] : nullptr;
					if (!drv || !IsCombinationalGate(drv)) {
						inputs.insert(bit);
						break;
					}
				}
			}
		}
	}
	return true;
}

void GetTopoSortedGates(vector<Cell *> &gates) {
    gates.clear();
    int i = 0;
	cell2level.clear();  // 清空level映射
	dict<Cell *, size_t> indegree;
	pool<Cell *> visited;
	vector<Cell*> sorted_cells;
	sorted_cells.reserve(cell2bits.size());
	for (auto &it : cell2bits) {
		Cell *cell = it.first;
		sorted_cells.push_back(cell);
	}
	sort(sorted_cells.begin(), sorted_cells.end(), LessCellStable);

	vector<Cell *> zero_indegree_nodes;
	auto push_ready = [&](Cell *cell) {
		auto it = lower_bound(zero_indegree_nodes.begin(), zero_indegree_nodes.end(), cell, LessCellStable);
		zero_indegree_nodes.insert(it, cell);
	};

	for (Cell *cell : sorted_cells) {
		log_assert(cell2bits[cell].size() > 0);
		indegree[cell] = cell2bits[cell].size() - 1; // the first is output bit
		if (!IsCombinationalGate(cell)) {
			continue;
		}
		vector<SigBit> inputs;
		GetCellInputsVector(cell, inputs);
		for (auto b : inputs) {
			Cell *drv = bit2driver[b];
			if (!drv || !IsCombinationalGate(drv)) {
				indegree[cell]--;
			}
		}
		if (indegree[cell] == 0) {
			push_ready(cell);
		}
	}
	// BFS
	while (!zero_indegree_nodes.empty()) {
		Cell *current = zero_indegree_nodes.front();
		zero_indegree_nodes.erase(zero_indegree_nodes.begin());
		if (!IsCombinationalGate(current) || visited.find(current) != visited.end()) {
			continue;
		}
		
		// 计算当前节点的level：max(所有输入节点的level) + 1
		size_t max_input_level = 0;
		vector<SigBit> inputs;
		GetCellInputsVector(current, inputs);
		for (auto input_bit : inputs) {
			Cell* driver = bit2driver.count(input_bit) ? bit2driver[input_bit] : nullptr;
			if (driver && cell2level.count(driver)) {
				max_input_level = max(max_input_level, cell2level[driver]);
			}
		}
		cell2level[current] = max_input_level + 1;
		
		gates.push_back(current);
        cell2index[current] = ++i;
		visited.insert(current);
		auto rds = GetReaders(current, ID(Y)); // combination gate output pin are Y
		vector<Cell*> sorted_neighbors;
		sorted_neighbors.reserve(rds.size());
		for (Cell *neighbor : rds) {
			sorted_neighbors.push_back(neighbor);
		}
		sort(sorted_neighbors.begin(), sorted_neighbors.end(), LessCellStable);

		for (Cell *neighbor : sorted_neighbors) {
			if (!IsCombinationalGate(current)) {
				continue;
			}
			if (visited.count(neighbor)) {
				log_error("toposort found loop, cell %s reader %s\n", current->name.c_str(), neighbor->name.c_str());
			}
			if (--indegree[neighbor] == 0) // update indegree
			{
				push_ready(neighbor);
			}
		}
	}
}

// 优化：添加缓存避免重复计算
State StateEval(dict<SigBit, State> &bit_map, SigBit out)
{
	if (bit_map.count(out)) {
		return bit_map[out];
	}
	Cell *cell = bit2driver[out];
	if (!cell) {
		return State::Sx;
	}

	vector<SigBit> bits = cell2bits[cell];
	if (IsAND(cell)) {
		SigBit tout = bits[0];
		for (size_t i = 1; i < bits.size(); i++) {
			State tmp = StateEval(bit_map, bits[i]);
			if (tmp == State::S0) {
				bit_map[tout] = State::S0;
				return State::S0;
			} else if (tmp == State::S1) {
				continue;
			} else {
				log_error("Cannot evaluate %s \n", log_signal(tout));
				bit_map[tout] = State::Sx;
				return State::Sx;
			}
		}
		return State::S1;
	} else if (IsNOT(cell)) {
		SigBit tout = bits[0];
		State tmp = StateEval(bit_map, bits[1]);
		if (tmp == State::S0) {
			bit_map[tout] = State::S1;
			return State::S1;
		} else if (tmp == State::S1) {
			bit_map[tout] = State::S0;
			return State::S0;
		} else {
			log_error("Cannot evaluate %s \n", log_signal(tout));
		}
	} else if (cell->type == ID($lut)) {
        int width = 0;
        if (cell->parameters.count(ID::WIDTH))
            width = cell->getParam(ID::WIDTH).as_int();
        if (width < 0)
            width = 0;
        if (width > 30)
            width = 30;

        Const lut_mask;
        if (cell->parameters.count(ID::LUT))
            lut_mask = cell->getParam(ID::LUT);

        int idx = 0;
        int max_input = std::min<int>(width, int(bits.size()) - 1);
        for (int i = 0; i < max_input; i++) {
            State tmp = StateEval(bit_map, bits[i + 1]);
            if (tmp != State::S0 && tmp != State::S1) {
                bit_map[bits[0]] = State::Sx;
                return State::Sx;
            }
            if (tmp == State::S1)
                idx |= (1 << i);
        }

        State y_val = State::S0;
        if (idx < lut_mask.size())
            y_val = lut_mask[idx];
        if (y_val != State::S0 && y_val != State::S1)
            y_val = State::S0;
        bit_map[bits[0]] = y_val;
        return y_val;
	} else {
		log_error("unhandled cell %s \n", cell->type.c_str());
	}
	return State::Sx;
}

bool CheckCellWidth(Module *module)
{
	// 初始化sigmap
	sigmap.set(module);
	
	for (Cell *cell : module->cells()) {
		if (IsCombinationalGate(cell)) {

		} else if (cell) {
			log_error("find unsupported cell %s (%s).\n", cell->name.c_str(), cell->type.c_str());
		}
	}

	for (auto &cell_iter : module->cells_) {
		Cell *cell = cell_iter.second;
		if (!yosys_celltypes.cell_known(cell->type)) {
			log_warning("cell %s (%s) is not a know type.\n", cell->name.c_str(), cell->type.c_str());
			continue;
		}

		vector<SigBit> all_bits;
		vector<SigBit> input_bits;
		for (auto &conn : cell->connections()) {
			IdString portname = conn.first;
			// (use sigmap to get a uniqe signal name)
			RTLIL::SigSpec sig = sigmap(conn.second);
			if (yosys_celltypes.cell_output(cell->type, portname)) {
				if (sig.size() == 0) {
					continue;
				}
				if (IsCombinationalGate(cell) && !sig.is_bit()) {
					log_error("cell %s %s  sig %s is not a bit=%d\n", cell->name.c_str(), cell->type.c_str(),
						  sig.as_string().c_str(), sig.size());
				}
				for (int i = 0; i < sig.size(); i++) {
					bit2driver[sig[i]] = cell;
					all_bits.push_back(sig[i]);
				}
			} else if (yosys_celltypes.cell_input(cell->type, portname)) {
				for (int i = 0; i < sig.size(); i++) {
					bit2reader[sig[i]].push_back(cell);
					input_bits.push_back(sig[i]);
				}
			}
		}
		if (all_bits.size() < 1 && cell->type != ID(GTP_GRS)) {
			log_warning(" cell %s(%s) have not any output.\n", cell->name.c_str(), cell->type.c_str());
		}
		for (auto bit : input_bits) {
			all_bits.push_back(bit);
		}
		cell2bits[cell] = all_bits;
	}
	return true;
}

// ABC风格：计算cut的area flow
// area_flow = LUT_area + Σ(fanin_cut.area_flow / fanin_refs)
double ComputeAreaFlowABC(const pool<SigBit>& leaves) {
    double flow = 1.0;  // 当前LUT的面积

    vector<SigBit> ordered_leaves = SortedSigBits(leaves);
    for (auto leaf_bit : ordered_leaves) {
        Cell* leaf = bit2driver.count(leaf_bit) ? bit2driver[leaf_bit] : nullptr;
        if (!leaf || !IsCombinationalGate(leaf)) continue;
        
        // 获取leaf的best cut的area flow
        double leaf_flow = 1.0;
        if (cell2bestcut.count(leaf)) {
            leaf_flow = cell2bestcut[leaf].area_flow;
        } else if (cell2cuts.count(leaf) && !cell2cuts[leaf].empty()) {
            leaf_flow = cell2cuts[leaf][0].area_flow;
        }
        
        // 获取估计的引用数
        float refs = 1.0f;
        if (cell2refs.count(leaf) && cell2refs[leaf] > 0) {
            refs = (float)cell2refs[leaf];
        } else if (cell2est_refs.count(leaf)) {
            refs = max(1.0f, cell2est_refs[leaf]);
        }
        
        flow += leaf_flow / refs;
    }
    return flow;
}

// ABC风格：ref-count驱动的面积恢复
static void RefNode(Cell *node)
{
    if (!node || !IsCombinationalGate(node))
        return;

    int prev = cell2refs[node]++;
    if (prev > 0)
        return;  // 只有第一次引用才向下递归

    const Cut *cut = nullptr;
    if (cell2bestcut.count(node)) {
        cut = &cell2bestcut[node];
    } else if (cell2cuts.count(node) && !cell2cuts[node].empty()) {
        cut = &cell2cuts[node][0];
    }
    if (!cut)
        return;

    for (auto leaf : cut->leaves) {
        Cell *driver = bit2driver.count(leaf) ? bit2driver[leaf] : nullptr;
        if (driver && IsCombinationalGate(driver))
            RefNode(driver);
    }
}

static void RecomputeRefsFromPOs(Module *module, const vector<Cell *> &gates)
{
    for (Cell *node : gates) {
        if (IsCombinationalGate(node))
            cell2refs[node] = 0;
    }

    pool<SigBit> pi, po;
    GetPrimeInputOutput(module, pi, po);
    for (auto bit : po) {
        Cell *driver = bit2driver.count(bit) ? bit2driver[bit] : nullptr;
        if (driver && IsCombinationalGate(driver))
            RefNode(driver);
    }

    for (Cell *node : gates) {
        if (!IsCombinationalGate(node))
            continue;
        cell2est_refs[node] = max(1.0f, (float)cell2refs[node]);
    }
}

// 仅对cut叶子依赖做ref/deref（不改变root本身），用于精确面积恢复评估
static int RefSupportFromCut(const Cut *cut)
{
    if (!cut)
        return 0;

    int area = 0;
    for (auto leaf : cut->leaves) {
        Cell *driver = bit2driver.count(leaf) ? bit2driver[leaf] : nullptr;
        if (!driver || !IsCombinationalGate(driver))
            continue;

        int prev = cell2refs.count(driver) ? cell2refs[driver] : 0;
        cell2refs[driver] = prev + 1;
        if (prev > 0)
            continue;

        area += 1;
        const Cut *driver_cut = nullptr;
        if (cell2bestcut.count(driver))
            driver_cut = &cell2bestcut[driver];
        else if (cell2cuts.count(driver) && !cell2cuts[driver].empty())
            driver_cut = &cell2cuts[driver][0];
        area += RefSupportFromCut(driver_cut);
    }
    return area;
}

static int DerefSupportFromCut(const Cut *cut)
{
    if (!cut)
        return 0;

    int area = 0;
    for (auto leaf : cut->leaves) {
        Cell *driver = bit2driver.count(leaf) ? bit2driver[leaf] : nullptr;
        if (!driver || !IsCombinationalGate(driver))
            continue;
        if (!cell2refs.count(driver))
            continue;

        int prev = cell2refs[driver];
        if (prev <= 0)
            continue;

        cell2refs[driver] = prev - 1;
        if (prev > 1)
            continue;

        area += 1;
        const Cut *driver_cut = nullptr;
        if (cell2bestcut.count(driver))
            driver_cut = &cell2bestcut[driver];
        else if (cell2cuts.count(driver) && !cell2cuts[driver].empty())
            driver_cut = &cell2cuts[driver][0];
        area += DerefSupportFromCut(driver_cut);
    }
    return area;
}

// 合并两个割集（用于AND门的两个输入）
Cut MergeCuts(const Cut &cut1, const Cut &cut2) {
    Cut result;
    
    // 合并叶子节点
    result.leaves = cut1.leaves;
    for (auto bit : cut2.leaves) {
        result.leaves.insert(bit);
    }
    
    // 合并signature（快速操作）
    result.signature = cut1.signature | cut2.signature;
    
    // 合并内部节点
    result.internal = cut1.internal;
    for (auto cell : cut2.internal) {
        result.internal.insert(cell);
    }
    // 删除由内部节点驱动的leaf（保证leaf是cut边界）
    for (auto cell : result.internal) {
        if (!cell || !cell2bits.count(cell)) continue;
        SigBit out = GetCellOutput(cell);
        if (result.leaves.count(out)) {
            result.leaves.erase(out);
        }
    }
    
    // arrival time将在Phase 2重新计算，这里暂时设为0
    result.arrival_time = 0.0f;
    
    // 计算深度（取最大值）
    result.depth = max(cut1.depth, cut2.depth);
    
    // ABC风格的area flow计算
    result.area_flow = ComputeAreaFlowABC(result.leaves);
    
    // 重新计算精确的signature
    result.computeSignature();
    
    return result;
}

// 移除被支配的割集
vector<Cut> RemoveDominatedCuts(vector<Cut> &cuts) {
    if (cuts.size() <= 1)
        return cuts;

    vector<size_t> order(cuts.size());
    std::iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(),
         [&](size_t a, size_t b) {
             if (cuts[a].leaves.size() != cuts[b].leaves.size())
                 return cuts[a].leaves.size() < cuts[b].leaves.size();
             if (cuts[a].depth != cuts[b].depth)
                 return cuts[a].depth < cuts[b].depth;
             return cuts[a].signature < cuts[b].signature;
         });

    vector<char> dominated(cuts.size(), 0);
    for (size_t oi = 0; oi < order.size(); oi++) {
        size_t i = order[oi];
        if (dominated[i])
            continue;

        for (size_t oj = oi + 1; oj < order.size(); oj++) {
            size_t j = order[oj];
            if (dominated[j])
                continue;
            if (cuts[j].leaves.size() == cuts[i].leaves.size())
                continue;

            // Signature quick reject: if i cannot be subset of j, skip exact check.
            if ((cuts[i].signature & ~cuts[j].signature) != 0)
                continue;

            if (IsLeafSubset(cuts[i].leaves, cuts[j].leaves))
                dominated[j] = 1;
        }
    }

    vector<Cut> result;
    result.reserve(cuts.size());
    for (size_t i = 0; i < cuts.size(); i++) {
        if (!dominated[i])
            result.push_back(cuts[i]);
    }
    return result;
}

static bool AreaRankBetter(const Cut &a, const Cut &b)
{
    if (fabs(a.area_flow - b.area_flow) > EPSILON)
        return a.area_flow < b.area_flow;
    if (fabs(a.arrival_time - b.arrival_time) > EPSILON)
        return a.arrival_time < b.arrival_time;
    if (a.leaves.size() != b.leaves.size())
        return a.leaves.size() > b.leaves.size();
    if (a.depth != b.depth)
        return a.depth > b.depth;
    return a.signature < b.signature;
}

static vector<Cut> PruneCutsDiverse(const vector<Cut> &sorted_cuts, size_t limit)
{
    if (sorted_cuts.size() <= limit)
        return sorted_cuts;

    vector<char> picked(sorted_cuts.size(), 0);
    vector<size_t> keep;
    keep.reserve(limit);

    auto pick_index = [&](size_t idx) {
        if (idx >= sorted_cuts.size() || picked[idx] || keep.size() >= limit)
            return false;
        picked[idx] = 1;
        keep.push_back(idx);
        return true;
    };

    // 1) 保留一半的“延迟优先”cut，维持时序质量。
    size_t delay_quota = std::max<size_t>(1, limit / 2);
    for (size_t i = 0; i < sorted_cuts.size() && keep.size() < delay_quota; i++)
        pick_index(i);

    // 2) 每个leaf-size至少保留一个area更优cut，避免大cut被完全挤掉。
    for (int leaf_sz = LUT_SIZE; leaf_sz >= 1 && keep.size() < limit; leaf_sz--) {
        size_t best_idx = SIZE_MAX;
        for (size_t i = 0; i < sorted_cuts.size(); i++) {
            if (picked[i]) continue;
            if ((int)sorted_cuts[i].leaves.size() != leaf_sz) continue;
            if (best_idx == SIZE_MAX || AreaRankBetter(sorted_cuts[i], sorted_cuts[best_idx]))
                best_idx = i;
        }
        if (best_idx != SIZE_MAX)
            pick_index(best_idx);
    }

    // 3) 补充少量深cut，增加跨层覆盖机会。
    size_t depth_quota = std::max<size_t>(1, limit / 6);
    vector<size_t> depth_order(sorted_cuts.size());
    iota(depth_order.begin(), depth_order.end(), 0);
    sort(depth_order.begin(), depth_order.end(),
         [&](size_t a, size_t b) {
             if (sorted_cuts[a].depth != sorted_cuts[b].depth)
                 return sorted_cuts[a].depth > sorted_cuts[b].depth;
             return AreaRankBetter(sorted_cuts[a], sorted_cuts[b]);
         });
    size_t picked_depth = 0;
    for (size_t idx : depth_order) {
        if (keep.size() >= limit || picked_depth >= depth_quota)
            break;
        if (pick_index(idx))
            picked_depth++;
    }

    // 4) 剩余名额按area排序补齐。
    vector<size_t> area_order(sorted_cuts.size());
    iota(area_order.begin(), area_order.end(), 0);
    sort(area_order.begin(), area_order.end(),
         [&](size_t a, size_t b) {
             return AreaRankBetter(sorted_cuts[a], sorted_cuts[b]);
         });
    for (size_t idx : area_order) {
        if (keep.size() >= limit)
            break;
        pick_index(idx);
    }

    // 5) 最后兜底，保证数量达到limit。
    for (size_t i = 0; i < sorted_cuts.size() && keep.size() < limit; i++)
        pick_index(i);

    vector<Cut> result;
    result.reserve(keep.size());
    for (size_t idx : keep)
        result.push_back(sorted_cuts[idx]);
    return result;
}

// 为节点生成割集（ABC风格）
void GenerateCutsForNode(Cell *node, vector<Cut> &cuts) {

    // 1. 添加平凡割集（Trivial Cut）：叶子是当前节点的直接输入
    Cut trivial_cut;
    vector<SigBit> inputs;
    GetCellInputsVector(node, inputs);
    for (auto input : inputs) {
        trivial_cut.leaves.insert(input);
    }
    trivial_cut.internal.insert(node);
    trivial_cut.depth = 1;  // 单个门的深度为1
    trivial_cut.arrival_time = 0.0f;
    trivial_cut.area_flow = ComputeAreaFlowABC(trivial_cut.leaves);
    trivial_cut.computeSignature();  // 计算signature
    cuts.push_back(trivial_cut);

    // 使用叶子哈希桶做重复检测，避免对已有cuts的全量线性扫描
    dict<uint64_t, vector<size_t>> cut_hash_buckets;
    cut_hash_buckets[HashCutLeaves(trivial_cut.leaves)].push_back(0);
    auto add_unique_cut = [&](const Cut &candidate) {
        uint64_t h = HashCutLeaves(candidate.leaves);
        if (cut_hash_buckets.count(h)) {
            for (size_t idx : cut_hash_buckets[h]) {
                if (CutLeavesEqual(cuts[idx].leaves, candidate.leaves))
                    return false;
            }
        }
        cut_hash_buckets[h].push_back(cuts.size());
        cuts.push_back(candidate);
        return true;
    };
    
    // 2. 根据门类型生成其他割集
    if (IsNOT(node)) {
        // NOT门：直接复制输入节点的割集
        vector<SigBit> inputs;
        GetCellInputsVector(node, inputs);
        if (inputs.size() != 1) {
            log_error("NOT gate %s should have exactly 1 input\n", node->name.c_str());
            return;
        }
        
        SigBit input_bit = inputs[0];
        Cell *input_cell = bit2driver.count(input_bit) ? bit2driver[input_bit] : nullptr;
        
        if (input_cell && IsCombinationalGate(input_cell) && cell2cuts.count(input_cell)) {
            // 复制输入节点的割集，并更新内部节点
            for (auto &input_cut : cell2cuts[input_cell]) {
                Cut new_cut = input_cut;
                new_cut.internal.insert(node);
                new_cut.depth = input_cut.depth + 1;
                new_cut.arrival_time = 0.0f;
                new_cut.area_flow = ComputeAreaFlowABC(new_cut.leaves);
                // signature不变（leaves相同）
                if (new_cut.is_feasible()) {
                    add_unique_cut(new_cut);
                }
            }
        } else {
            // 输入是主输入（Primary Input）
            Cut pi_cut;
            pi_cut.leaves.insert(input_bit);
            pi_cut.internal.insert(node);
            pi_cut.depth = 1;
            pi_cut.arrival_time = 0.0f;
            pi_cut.area_flow = 1.0;
            pi_cut.computeSignature();
            add_unique_cut(pi_cut);
        }
    } else if (IsAND(node)) {
        // AND门：合并两个输入的割集
        vector<SigBit> inputs;
        GetCellInputsVector(node, inputs);
        if (inputs.size() != 2) {
            log_error("AND gate %s should have exactly 2 inputs\n", node->name.c_str());
            return;
        }
        
        SigBit input1 = inputs[0];
        SigBit input2 = inputs[1];
        Cell *cell1 = bit2driver.count(input1) ? bit2driver[input1] : nullptr;
        Cell *cell2 = bit2driver.count(input2) ? bit2driver[input2] : nullptr;

        vector<Cut> cuts1_fallback, cuts2_fallback;
        const vector<Cut> *cuts1 = nullptr;
        const vector<Cut> *cuts2 = nullptr;

        // 获取第一个输入的割集
        if (cell1 && IsCombinationalGate(cell1) && cell2cuts.count(cell1)) {
            cuts1 = &cell2cuts[cell1];
        } else {
            Cut pi_cut;
            pi_cut.leaves.insert(input1);
            pi_cut.depth = 0;
            pi_cut.area_flow = 0.0;
            pi_cut.computeSignature();
            cuts1_fallback.push_back(pi_cut);
            cuts1 = &cuts1_fallback;
        }

        // 获取第二个输入的割集
        if (cell2 && IsCombinationalGate(cell2) && cell2cuts.count(cell2)) {
            cuts2 = &cell2cuts[cell2];
        } else {
            Cut pi_cut;
            pi_cut.leaves.insert(input2);
            pi_cut.depth = 0;
            pi_cut.area_flow = 0.0;
            pi_cut.computeSignature();
            cuts2_fallback.push_back(pi_cut);
            cuts2 = &cuts2_fallback;
        }

        // 笛卡尔积合并（使用signature快速过滤）
        for (const auto &c1 : *cuts1) {
            for (const auto &c2 : *cuts2) {
                // ABC风格：使用signature快速过滤不可能的合并
                if (!Cut::canMergeFast(c1, c2, LUT_SIZE)) {
                    continue;  // 快速跳过
                }
                
                Cut merged = MergeCuts(c1, c2);
                merged.internal.insert(node);
                merged.depth++;
                // area_flow已在MergeCuts中计算
                
                // K-可行性精确检查
                if (!merged.is_feasible()) {
                    continue;
                }

                add_unique_cut(merged);
            }
        }
    }
    
    // 3. 为所有cuts计算signature
    for (auto &cut : cuts) {
        cut.computeSignature();
    }
    
    // 4. 优势剪枝（Dominance Pruning）
    cuts = RemoveDominatedCuts(cuts);
    
    // 5. 按优先级排序
    sort(cuts.begin(), cuts.end());
    
    // 6. N-Cut剪枝（保留一定的cut多样性，避免仅保留延迟最优cut）
    if (cuts.size() > size_t(MAX_CUTS_PER_NODE))
        cuts = PruneCutsDiverse(cuts, size_t(MAX_CUTS_PER_NODE));
    
    log_debug("Node %s generated %zu cuts\n", node->name.c_str(), cuts.size());
    for (size_t i = 0; i < cuts.size(); i++) {
        log_debug("  Cut %zu: leaves=%zu, internal=%zu, depth=%zu, area_flow=%.2f, sig=0x%llX\n",
                  i, cuts[i].leaves.size(), cuts[i].internal.size(),
                  cuts[i].depth, cuts[i].area_flow, (unsigned long long)cuts[i].signature);
        log_debug("    Leaves: ");
        for (auto bit : cuts[i].leaves) {
            log_debug("%s ", log_signal(bit));
        }
        log_debug("\n");
    }
    
    return;
}

// 检查是否满足Z5=Z在I5=0时的cofactor关系
bool RoleValid(Cell* z_node, Cell* z5_node, SigBit i5, const Cut& merged_cut) {
    if (!z_node || !z5_node) return false;
    if (i5.wire == nullptr) return true;
    if (!merged_cut.leaves.count(i5)) return true;

    vector<SigBit> other_leaves;
    vector<SigBit> ordered_leaves = SortedSigBits(merged_cut.leaves);
    other_leaves.reserve(ordered_leaves.size());
    for (auto bit : ordered_leaves) {
        if (bit != i5) other_leaves.push_back(bit);
    }

    size_t bits_num = size_t(1 << other_leaves.size());
    SigBit z_out = GetCellOutput(z_node);
    SigBit z5_out = GetCellOutput(z5_node);
    for (size_t i = 0; i < bits_num; i++) {
        dict<SigBit, State> bit_map;
        for (size_t n = 0; n < other_leaves.size(); n++) {
            bit_map[other_leaves[n]] = ((i & (1 << n)) != 0) ? State::S1 : State::S0;
        }
        bit_map[i5] = State::S0;
        State z_val = StateEval(bit_map, z_out);
        State z5_val = StateEval(bit_map, z5_out);
        if (z_val != z5_val) return false;
    }
    return true;
}

static void CollectPreferredSelectors(const pool<SigBit>& potential_I5, const Cut& merged_cut,
                                      vector<SigBit>& ordered)
{
    ordered.clear();
    ordered.reserve(potential_I5.size());
    vector<SigBit> sorted_i5 = SortedSigBits(potential_I5);
    for (auto bit : sorted_i5) {
        if (bit.wire && merged_cut.leaves.count(bit))
            ordered.push_back(bit);
    }
    for (auto bit : sorted_i5) {
        if (bit.wire && !merged_cut.leaves.count(bit))
            ordered.push_back(bit);
    }
    for (auto bit : sorted_i5) {
        if (!bit.wire)
            ordered.push_back(bit);
    }
}

bool VerifyLUT6DMapping(const LUT6DInfo& lut6d_info, Cell* node, Cell* cand, bool isZ) {
    if (!node || !cand) return false;
    if (lut6d_info.inputs.empty()) return false;
    if (lut6d_info.inputs.size() < 4 || lut6d_info.inputs.size() > 6) return false;

    SigBit z_out = isZ ? GetCellOutput(cand) : GetCellOutput(node);
    SigBit z5_out = isZ ? GetCellOutput(node) : GetCellOutput(cand);

    const vector<SigBit>& inputs = lut6d_info.inputs;
    vector<int> var_pos;
    var_pos.reserve(inputs.size());
    vector<int> var_idx(inputs.size(), -1);
    vector<State> fixed_vals(inputs.size(), State::Sx);
    for (size_t i = 0; i < inputs.size(); i++) {
        // 拒绝输出回灌到输入：会形成组合环，导致映射不等价。
        if (inputs[i] == z_out || inputs[i] == z5_out)
            return false;

        if (inputs[i].wire == nullptr) {
            fixed_vals[i] = inputs[i].data;
            if (fixed_vals[i] != State::S0 && fixed_vals[i] != State::S1) {
                return false;
            }
        } else {
            var_idx[i] = int(var_pos.size());
            var_pos.push_back(int(i));
        }
    }

    size_t bits_num = size_t(1) << var_pos.size();
    size_t selector_mask = DualLutSelectorMask(inputs.size());

    for (size_t mask = 0; mask < bits_num; mask++) {
        dict<SigBit, State> bit_map;
        size_t idx = 0;
        for (size_t i = 0; i < inputs.size(); i++) {
            State val = State::S0;
            if (inputs[i].wire == nullptr) {
                val = fixed_vals[i];
            } else {
                int bit_idx = var_idx[i];
                if (bit_idx < 0)
                    return false;
                val = ((mask >> bit_idx) & 1) ? State::S1 : State::S0;
            }
            bit_map[inputs[i]] = val;
            if (val == State::S1) idx |= (size_t(1) << i);
        }

        State exp_z = StateEval(bit_map, z_out);
        State exp_z5 = StateEval(bit_map, z5_out);
        if (exp_z != State::S0 && exp_z != State::S1) return false;
        if (exp_z5 != State::S0 && exp_z5 != State::S1) return false;

        bool act_z = ((lut6d_info.INIT >> idx) & 1) != 0;
        size_t idx_z5 = idx & (selector_mask - 1);
        bool act_z5 = ((lut6d_info.INIT >> idx_z5) & 1) != 0;

        if (act_z != (exp_z == State::S1)) return false;
        if (act_z5 != (exp_z5 == State::S1)) return false;
    }
    return true;
}

static void GetTimingLeaves(Cell *node, vector<SigBit> &leaves)
{
    leaves.clear();
    if (cell2bestcut.count(node)) {
        for (auto bit : cell2bestcut[node].leaves) {
            leaves.push_back(bit);
        }
        return;
    }
    GetCellInputsVector(node, leaves);
}

// 计算所有节点的Arrival Time（Forward Pass - 从PI到PO）
void ComputeArrivalTimes(vector<Cell*> &gates) {
    log("  Computing arrival times (forward pass)...\n");
    cell2arrival.clear();
    
    // 按拓扑顺序遍历（gates已经是拓扑排序的）
    for (Cell* node : gates) {
        if (!IsCombinationalGate(node)) continue;
        
        // 获取节点的所有输入信号（使用best cut的leaves）
        vector<SigBit> inputs;
        GetTimingLeaves(node, inputs);
        
        // 计算arrival time = max(所有输入的arrival time) + 1.0
        float max_input_arrival = 0.0f;
        for (auto input : inputs) {
            Cell *driver = bit2driver.count(input) ? bit2driver[input] : nullptr;
            if (driver && IsCombinationalGate(driver) && cell2arrival.count(driver)) {
                max_input_arrival = max(max_input_arrival, cell2arrival[driver]);
            }
            // 如果driver不存在或没有arrival time，说明是primary input，其arrival time为0
        }
        
        // 当前节点的arrival time = 输入最大arrival time + 1.0（LUT延迟）
        float arrival = max_input_arrival + 1.0f;
        cell2arrival[node] = arrival;
    }
}

// 计算所有节点的Required Time（Backward Pass - 从PO到PI）
void ComputeRequiredTimes(Module *module, vector<Cell*> &gates) {
    log("  Computing required times (backward pass)...\n");
    cell2required.clear();
    
    // 1. 找到全局最大arrival time (关键路径延迟)
    float global_max_arrival = 0.0f;
    pool<SigBit> prime_outputs;
    pool<SigBit> prime_inputs;
    GetPrimeInputOutput(module, prime_inputs, prime_outputs);
    
    for (auto bit : prime_outputs) {
        Cell *driver = bit2driver.count(bit) ? bit2driver[bit] : nullptr;
        if (driver && cell2arrival.count(driver)) {
            global_max_arrival = max(global_max_arrival, cell2arrival[driver]);
        }
    }

    global_max_arrival += 1.0f;
    
    log("    Global max arrival time (critical path): %.2f\n", global_max_arrival);
    
    // 2. 初始化所有节点的required time为无穷大
    const float INF_TIME = 1e9;
    for (Cell* node : gates) {
        if (IsCombinationalGate(node)) {
            cell2required[node] = INF_TIME;
        }
    }
    
    // 3. 设置输出节点的required time为global_max_arrival
    for (auto bit : prime_outputs) {
        Cell *driver = bit2driver.count(bit) ? bit2driver[bit] : nullptr;
        if (driver && IsCombinationalGate(driver)) {
            cell2required[driver] = global_max_arrival;
        }
    }
    
    // 4. 反向拓扑遍历，传播required time
    // 从后向前遍历gates（因为gates已经是拓扑排序的）
    for (auto it = gates.rbegin(); it != gates.rend(); ++it) {
        Cell* node = *it;
        if (!IsCombinationalGate(node)) continue;
        if (!cell2required.count(node)) continue;
        
        float node_required = cell2required[node];
        
        // 获取该节点的输入（使用best cut的leaves）
        vector<SigBit> inputs;
        GetTimingLeaves(node, inputs);
        
        for (auto input_bit : inputs) {
            Cell *driver = bit2driver.count(input_bit) ? bit2driver[input_bit] : nullptr;
            if (driver && IsCombinationalGate(driver)) {
                // 输入节点的required time = 当前节点的required - LUT延迟
                float fanin_required = node_required - 1.0f;  // LUT单位延迟
                
                // 取所有fanout的最小required time
                if (cell2required.count(driver)) {
                    cell2required[driver] = min(cell2required[driver], fanin_required);
                } else {
                    cell2required[driver] = fanin_required;
                }
            }
        }
    }
    
    // 5. 打印一些统计信息
    int critical_nodes = 0;
    for (Cell* node : gates) {
        if (!IsCombinationalGate(node)) continue;
        if (!cell2arrival.count(node) || !cell2required.count(node)) continue;
        
        float slack = cell2required[node] - cell2arrival[node];
        log_debug("    Node %s: arrival=%.2f, required=%.2f, slack=%.2f\n",
                  node->name.c_str(), cell2arrival[node], cell2required[node], slack);
        
        if (slack < 0.5f) {  // 接近关键路径
            critical_nodes++;
        }
    }
    
    log("    Found %d nodes on or near critical path (slack < 0.5)\n", critical_nodes);
}

static const Cut *GetBestCutForCover(Cell *node)
{
    if (cell2bestcut.count(node))
        return &cell2bestcut[node];
    if (cell2cuts.count(node) && !cell2cuts[node].empty())
        return &cell2cuts[node][0];
    return nullptr;
}

// ============================================================================
// 层级覆盖算法 (Layer-Based Covering Algorithm)
// 核心思想：从PO反向逐层构建，每层选择能覆盖最多信号的多输出LUT组合
// ============================================================================

// 多输出Cut候选结构
struct DualOutputCandidate {
    Cell* root1;           // 第一个输出节点
    Cell* root2;           // 第二个输出节点（可为nullptr表示单输出）
    Cut merged_cut;        // 合并后的cut
    SigBit I5;             // 区分两个输出的信号
    bool isZ;              // root1是否为Z输出
    double score;          // 候选得分
    double area_gain;      // 与两个单输出相比的面积收益估计（越大越好）
    pool<SigBit> covers;   // 该候选能覆盖的当前层信号
    size_t shared_inputs;  // 共享输入数量（仅对双输出候选有效）
    
    DualOutputCandidate() : root1(nullptr), root2(nullptr), I5(State::Sx), isZ(false),
                            score(0.0), area_gain(0.0), shared_inputs(0) {}
    
    bool isDual() const { return root2 != nullptr; }
    
    size_t coverageCount() const { return covers.size(); }
};

static void BuildLUT6DInfoFromCandidate(const DualOutputCandidate& cand, LUT6DInfo& lut_info)
{
    Cell* z_node = cand.isZ ? cand.root2 : cand.root1;
    Cell* z5_node = cand.isZ ? cand.root1 : cand.root2;

    lut_info.Z = GetCellOutput(z_node);
    lut_info.Z5 = GetCellOutput(z5_node);

    // 双输出LUT不允许把任一输出作为输入，否则会形成组合反馈环。
    if (cand.merged_cut.leaves.count(lut_info.Z) || cand.merged_cut.leaves.count(lut_info.Z5)) {
        lut_info.inputs.clear();
        lut_info.INIT = 0;
        return;
    }

    size_t lut_size = DualLutSizeFromCut(cand.merged_cut, cand.I5);
    if (!BuildDualLutInputs(cand.merged_cut, cand.I5, lut_size, lut_info.inputs)) {
        log_warning("Layer covering: failed to build dual LUT inputs for %s/%s\n",
                    cand.root1 ? cand.root1->name.c_str() : "N/A",
                    cand.root2 ? cand.root2->name.c_str() : "N/A");
        return;
    }

    for (auto bit : lut_info.inputs) {
        if (bit == lut_info.Z || bit == lut_info.Z5) {
            lut_info.inputs.clear();
            lut_info.INIT = 0;
            return;
        }
    }

    // 计算INIT
    // Dual-LUT的INIT语义：
    // - Z  = INIT[{Ik,...,I0}]  （完整K输入查表）
    // - Z5 = INIT[{0,Ik-1,...,I0}]   （最高位输入固定为0）
    size_t bits_num = size_t(1) << lut_info.inputs.size();
    size_t selector_mask = DualLutSelectorMask(lut_info.inputs.size());
    uint64_t INIT = 0;
    for (size_t i = 0; i < bits_num; i++) {
        dict<SigBit, State> bit_map;
        for (size_t n = 0; n < lut_info.inputs.size(); n++) {
            State val = ((i >> n) & 1) ? State::S1 : State::S0;
            if (lut_info.inputs[n].wire == nullptr) {
                val = lut_info.inputs[n].data;
            }
            bit_map[lut_info.inputs[n]] = val;
        }

        // 判断selector的值：最高位输入在最后一位
        bool selector_is_one = (i & selector_mask) != 0;

        State lut_output;
        if (selector_is_one) {
            // 当selector=1时，这个INIT条目影响Z的输出
            lut_output = StateEval(bit_map, lut_info.Z);
        } else {
            // 当selector=0时，这个INIT条目影响Z5的输出
            lut_output = StateEval(bit_map, lut_info.Z5);
        }

        if (lut_output == State::S1) {
            INIT |= (uint64_t(1) << i);
        }
    }
    lut_info.INIT = INIT;
}

static void LogSigBitSetWarning(const char* label, const pool<SigBit>& bits)
{
    std::string line = stringf("    %s:", label);
    for (auto bit : bits) {
        line += " ";
        line += log_signal(bit);
    }
    log_warning("%s\n", line.c_str());
}

static bool SelectI5AndRoleWithVerify(Cell* node, Cell* cand_node, const Cut& merged_cut,
                                      const pool<SigBit>& potential_I5, SigBit& chosen_i5, bool& isZ,
                                      Cut& chosen_cut)
{
    SigBit node_out = GetCellOutput(node);
    SigBit cand_out = GetCellOutput(cand_node);
    if (merged_cut.leaves.count(node_out) || merged_cut.leaves.count(cand_out))
        return false;

    chosen_cut = merged_cut;
    vector<SigBit> ordered;
    CollectPreferredSelectors(potential_I5, merged_cut, ordered);
    vector<SigBit> trial_bits;
    trial_bits.reserve(ordered.size());
    if (potential_I5.count(State::S1))
        trial_bits.push_back(State::S1); // 优先尝试常量selector，支持“输出独立”打包
    for (auto bit : ordered) {
        if (!(bit.wire == nullptr && bit.data == State::S1))
            trial_bits.push_back(bit);
    }

    int tried = 0;
    for (auto bit : trial_bits) {
        if (bit == node_out || bit == cand_out)
            continue;
        if (++tried > MAX_I5_CANDIDATES)
            break;

        if (RoleValid(node, cand_node, bit, merged_cut)) {
            DualOutputCandidate temp;
            temp.root1 = node;
            temp.root2 = cand_node;
            temp.merged_cut = merged_cut;
            temp.I5 = bit;
            temp.isZ = false;
            LUT6DInfo lut_info;
            BuildLUT6DInfoFromCandidate(temp, lut_info);
            if (VerifyLUT6DMapping(lut_info, node, cand_node, false)) {
                chosen_i5 = bit;
                isZ = false;
                chosen_cut = merged_cut;
                return true;
            }
        }
        if (RoleValid(cand_node, node, bit, merged_cut)) {
            DualOutputCandidate temp;
            temp.root1 = node;
            temp.root2 = cand_node;
            temp.merged_cut = merged_cut;
            temp.I5 = bit;
            temp.isZ = true;
            LUT6DInfo lut_info;
            BuildLUT6DInfoFromCandidate(temp, lut_info);
            if (VerifyLUT6DMapping(lut_info, node, cand_node, true)) {
                chosen_i5 = bit;
                isZ = true;
                chosen_cut = merged_cut;
                return true;
            }
        }
    }
    return false;
}

// 计算一个候选能覆盖当前层的哪些信号
static void ComputeCoverage(DualOutputCandidate& cand, const pool<SigBit>& current_layer) {
    cand.covers.clear();
    if (cand.root1) {
        SigBit out1 = GetCellOutput(cand.root1);
        if (current_layer.count(out1)) {
            cand.covers.insert(out1);
        }
    }
    if (cand.root2) {
        SigBit out2 = GetCellOutput(cand.root2);
        if (current_layer.count(out2)) {
            cand.covers.insert(out2);
        }
    }
}

static size_t CountSharedLeaves(const Cut &a, const Cut &b)
{
    const pool<SigBit> *small = &a.leaves;
    const pool<SigBit> *large = &b.leaves;
    if (small->size() > large->size())
        std::swap(small, large);

    size_t shared = 0;
    for (auto bit : *small) {
        if (large->count(bit))
            shared++;
    }
    return shared;
}

// 计算候选的综合得分
static double ComputeCandidateScore(const DualOutputCandidate& cand, const pool<SigBit>& current_layer) {
    (void)current_layer;
    double coverage_score = (double)cand.covers.size() * 40.0;  // 覆盖优先
    double input_score = (double)(LUT_SIZE - cand.merged_cut.leaves.size()) * 1.2;
    double depth_score = -0.08 * cand.merged_cut.depth;
    double shared_score = cand.isDual() ? (double)cand.shared_inputs * 2.5 : 0.0;
    double area_flow_score = -0.35 * cand.merged_cut.area_flow;  // 面积流惩罚（越小越好）
    double area_gain_score = cand.isDual() ? cand.area_gain * 8.0 : 0.0; // 和单输出相比的收益
    double dual_bias = cand.isDual() ? 3.0 : 0.0;
    if (cand.isDual() && cand.area_gain < -0.05) {
        if (cand.shared_inputs == 0 && cand.merged_cut.leaves.size() <= 5)
            dual_bias = 2.0;  // constant I5 disjoint pair: always saves 1 LUT
        else
            dual_bias -= 4.0;
    }
    return coverage_score + input_score + depth_score + shared_score + area_flow_score + area_gain_score + dual_bias;
}

static void CollectLayerCandidateCuts(Cell* node, vector<const Cut*>& cuts)
{
    cuts.clear();
    if (!node) return;
    if (!cell2cuts.count(node) || cell2cuts[node].empty()) return;
    if (layer_candidate_cut_cache.count(node)) {
        cuts = layer_candidate_cut_cache[node];
        return;
    }

    const vector<Cut>& all = cell2cuts[node];
    const Cut* best = nullptr;
    if (cell2bestcut.count(node)) {
        best = &cell2bestcut[node];
    } else {
        best = &all[0];
    }
    int depth_delta = 2;
    int leaf_delta = 2;
    int limit = std::max(1, LAYER_CUTS_PER_NODE);
    if (limit <= 1) {
        cuts.push_back(best);
        return;
    }

    vector<const Cut*> filtered;
    filtered.reserve(all.size());
    for (const Cut& cut : all) {
        if ((int)cut.depth > (int)best->depth + depth_delta &&
            cut.leaves.size() > best->leaves.size() + size_t(leaf_delta)) {
            continue;
        }
        filtered.push_back(&cut);
    }

    if (filtered.empty()) {
        cuts.push_back(best);
        return;
    }

    sort(filtered.begin(), filtered.end(),
         [](const Cut* a, const Cut* b) {
             if (a->leaves.size() != b->leaves.size())
                 return a->leaves.size() < b->leaves.size();
             if (a->depth != b->depth)
                 return a->depth < b->depth;
             if (fabs(a->area_flow - b->area_flow) > EPSILON)
                 return a->area_flow < b->area_flow;
             if (fabs(a->arrival_time - b->arrival_time) > EPSILON)
                 return a->arrival_time < b->arrival_time;
             return a->signature < b->signature;
         });

    cuts.push_back(best);
    for (const Cut* cut : filtered) {
        if (cut == best) continue;
        cuts.push_back(cut);
        if ((int)cuts.size() >= limit) break;
    }

    if (cuts.empty()) {
        cuts.push_back(best);
    }
    layer_candidate_cut_cache[node] = cuts;
}

// 为单个节点生成所有可能的多输出配对候选
static void GenerateDualCandidatesForNode(Cell* node, const pool<SigBit>& current_layer,
                                          vector<DualOutputCandidate>& candidates,
                                          bool allow_dual) {
    if (!node || !IsCombinationalGate(node)) return;
    if (processed_nodes.count(node)) return;
    if (!cell2cuts.count(node) || cell2cuts[node].empty()) return;
    
    SigBit node_output = GetCellOutput(node);
    vector<const Cut*> node_cuts;
    CollectLayerCandidateCuts(node, node_cuts);
    if (node_cuts.empty()) return;
    
    // 首先添加单输出候选（作为fallback）
    const Cut* best_cut = node_cuts[0];
    DualOutputCandidate single_cand;
    single_cand.root1 = node;
    single_cand.root2 = nullptr;
    single_cand.merged_cut = *best_cut;
    single_cand.I5 = State::Sx;
    single_cand.isZ = false;
    single_cand.area_gain = 0.0;
    single_cand.shared_inputs = 0;
    ComputeCoverage(single_cand, current_layer);
    single_cand.score = ComputeCandidateScore(single_cand, current_layer);
    candidates.push_back(single_cand);
    double score_margin = 0.0;

    if (!allow_dual) {
        return;
    }
    
    // 搜索可配对的候选节点：优先搜索共享输入的节点
    pool<Cell*> potential_pairs;
    size_t pair_seed_limit = size_t(std::max(4, MAX_PAIRS_PER_NODE * PAIR_SEED_MULTIPLIER));
    auto try_insert_pair = [&](Cell* cand) {
        if (!cand || cand == node) return false;
        if (!IsCombinationalGate(cand)) return false;
        if (processed_nodes.count(cand)) return false;
        potential_pairs.insert(cand);
        return potential_pairs.size() >= pair_seed_limit;
    };
    bool seed_limit_hit = false;
    for (const Cut* node_cut : node_cuts) {
        for (SigBit leaf : node_cut->leaves) {
            if (bit2reader.count(leaf)) {
                for (Cell* reader : bit2reader[leaf]) {
                    if (try_insert_pair(reader)) {
                        seed_limit_hit = true;
                        break;
                    }
                }
            }
            if (seed_limit_hit) break;
        }
        if (seed_limit_hit) break;
    }

    // 也考虑输出在当前层的其他节点（已达到预筛上限时跳过）
    if (!seed_limit_hit) {
        for (SigBit sig : current_layer) {
            if (sig == node_output) continue;
            Cell* driver = bit2driver.count(sig) ? bit2driver[sig] : nullptr;
            if (try_insert_pair(driver)) {
                seed_limit_hit = true;
                break;
            }
        }
    }
    
    // 评估每个配对候选
    vector<Cell*> ordered_pairs;
    ordered_pairs.reserve(potential_pairs.size());
    for (Cell* cand : potential_pairs)
        ordered_pairs.push_back(cand);
    sort(ordered_pairs.begin(), ordered_pairs.end(), LessCellStable);

    struct PairRank {
        Cell* cand;
        int score;
        size_t shared;
        size_t total_inputs;
        int level_gap;
    };
    vector<PairRank> shared_ranked_pairs;
    vector<PairRank> disjoint_ranked_pairs;
    shared_ranked_pairs.reserve(ordered_pairs.size());
    disjoint_ranked_pairs.reserve(ordered_pairs.size());
    for (Cell* cand_node : ordered_pairs) {
        const Cut* cand_best = GetBestCutForCover(cand_node);
        if (!cand_best)
            continue;
        size_t shared = CountSharedLeaves(*best_cut, *cand_best);
        if (shared < size_t(MIN_SHARED_INPUTS_FOR_DUAL))
            continue;
        size_t total_inputs = best_cut->leaves.size() + cand_best->leaves.size() - shared;
        if (total_inputs > size_t(LUT_SIZE))
            continue;
        int level_gap = 0;
        if (cell2level.count(node) && cell2level.count(cand_node))
            level_gap = std::abs((int)cell2level[node] - (int)cell2level[cand_node]);
        int rank = int(shared) * 100 - int(total_inputs) * 10 - level_gap * 4;
        SigBit cand_out = GetCellOutput(cand_node);
        if (current_layer.count(cand_out))
            rank += 10;
        if (shared == 0)
            rank += (total_inputs <= size_t(LUT_SIZE - 1)) ? 5 : 0;
        PairRank item{cand_node, rank, shared, total_inputs, level_gap};
        if (shared == 0)
            disjoint_ranked_pairs.push_back(item);
        else
            shared_ranked_pairs.push_back(item);
    }

    auto pair_rank_less = [](const PairRank& a, const PairRank& b) {
        if (a.score != b.score)
            return a.score > b.score;
        if (a.shared != b.shared)
            return a.shared > b.shared;
        if (a.total_inputs != b.total_inputs)
            return a.total_inputs < b.total_inputs;
        if (a.level_gap != b.level_gap)
            return a.level_gap < b.level_gap;
        return LessCellStable(a.cand, b.cand);
    };

    sort(shared_ranked_pairs.begin(), shared_ranked_pairs.end(), pair_rank_less);
    sort(disjoint_ranked_pairs.begin(), disjoint_ranked_pairs.end(), pair_rank_less);

    vector<PairRank> ranked_pairs;
    size_t total_budget = size_t(MAX_PAIRS_PER_NODE);
    ranked_pairs.reserve(std::min(total_budget, shared_ranked_pairs.size() + disjoint_ranked_pairs.size()));

    size_t disjoint_cap = std::min(size_t(MAX_DISJOINT_PAIRS_PER_NODE), total_budget / 2);
    size_t take_disjoint = std::min(disjoint_ranked_pairs.size(), disjoint_cap);
    size_t take_shared = std::min(shared_ranked_pairs.size(), total_budget - take_disjoint);

    if (take_shared + take_disjoint < total_budget) {
        size_t rem = total_budget - take_shared - take_disjoint;
        size_t extra_disjoint = 0;
        if (disjoint_ranked_pairs.size() > take_disjoint)
            extra_disjoint = std::min(rem, disjoint_ranked_pairs.size() - take_disjoint);
        take_disjoint += extra_disjoint;
        rem -= extra_disjoint;
        if (rem > 0 && shared_ranked_pairs.size() > take_shared)
            take_shared += std::min(rem, shared_ranked_pairs.size() - take_shared);
    }

    for (size_t i = 0; i < take_shared; i++)
        ranked_pairs.push_back(shared_ranked_pairs[i]);
    for (size_t i = 0; i < take_disjoint; i++)
        ranked_pairs.push_back(disjoint_ranked_pairs[i]);

    sort(ranked_pairs.begin(), ranked_pairs.end(),
         [](const PairRank& a, const PairRank& b) {
             if (a.score != b.score)
                 return a.score > b.score;
             if (a.shared != b.shared)
                 return a.shared > b.shared;
             if (a.total_inputs != b.total_inputs)
                 return a.total_inputs < b.total_inputs;
             if (a.level_gap != b.level_gap)
                 return a.level_gap < b.level_gap;
             return LessCellStable(a.cand, b.cand);
         });

    for (const auto &pair_rank : ranked_pairs) {
        Cell* cand_node = pair_rank.cand;
        if (!cell2cuts.count(cand_node) || cell2cuts[cand_node].empty()) continue;
        SigBit cand_output = GetCellOutput(cand_node);
        bool pair_covers_both = current_layer.count(node_output) && current_layer.count(cand_output);
        vector<const Cut*> cand_cuts;
        CollectLayerCandidateCuts(cand_node, cand_cuts);
        if (cand_cuts.empty()) continue;

        DualOutputCandidate best_dual;
        bool has_best_dual = false;
        for (const Cut* node_cut : node_cuts) {
            DualOutputCandidate single_for_cut = single_cand;
            single_for_cut.merged_cut = *node_cut;
            single_for_cut.shared_inputs = 0;
            ComputeCoverage(single_for_cut, current_layer);
            single_for_cut.score = ComputeCandidateScore(single_for_cut, current_layer);
            for (const Cut* cand_cut : cand_cuts) {
                if (!Cut::canMergeFast(*node_cut, *cand_cut, LUT_SIZE)) continue;

                // 快速K-feasibility检查
                size_t shared = 0;
                pool<SigBit> shared_bits;
                for (auto bit : node_cut->leaves) {
                    if (cand_cut->leaves.count(bit)) {
                        shared++;
                        shared_bits.insert(bit);
                    }
                }
                size_t total_inputs = node_cut->leaves.size() + cand_cut->leaves.size() - shared;
                if (total_inputs > size_t(LUT_SIZE)) continue;
                int min_shared = MIN_SHARED_INPUTS_FOR_DUAL;
                if ((int)shared < min_shared) continue;
                
                // 合并cut
                Cut merged = MergeCuts(*node_cut, *cand_cut);
                if (merged.leaves.size() > size_t(LUT_SIZE)) continue;
                if (merged.leaves.count(node_output) || merged.leaves.count(cand_output)) continue;
                
                // 估计双输出相对“两个单输出”的面积收益，收益不足则直接剪枝
                double single_area = node_cut->area_flow + cand_cut->area_flow;
                double area_gain = single_area - merged.area_flow;
                double min_gain = MIN_AREA_GAIN_FOR_DUAL;
                if (shared == 0)
                    min_gain = MIN_AREA_GAIN_FOR_NO_SHARE;
                else if (shared <= 1)
                    min_gain = MIN_AREA_GAIN_FOR_WEAK_SHARE;
                // 两个输出都在当前层时优先压缩数量，进一步放宽面积估计门槛
                if (pair_covers_both) {
                    if (shared == 0 && total_inputs <= size_t(LUT_SIZE - 1))
                        min_gain = -1.5;  // constant I5 disjoint covering both: always saves 1 LUT
                    else if (shared == 0)
                        min_gain = std::min(min_gain, MIN_AREA_GAIN_FOR_NO_SHARE);
                    else
                        min_gain = std::min(min_gain, MIN_AREA_GAIN_FOR_WEAK_SHARE);
                }
                if (area_gain + EPSILON < min_gain) continue;
                
                // 先做轻量评估，不值得时跳过昂贵的Role/SAT风格验证
                DualOutputCandidate coarse_dual;
                coarse_dual.root1 = node;
                coarse_dual.root2 = cand_node;
                coarse_dual.merged_cut = merged;
                coarse_dual.area_gain = area_gain;
                coarse_dual.shared_inputs = shared;
                ComputeCoverage(coarse_dual, current_layer);
                coarse_dual.score = ComputeCandidateScore(coarse_dual, current_layer);
                if (coarse_dual.coverageCount() < single_for_cut.coverageCount() ||
                    coarse_dual.score + EPSILON < single_for_cut.score + score_margin) {
                    continue;
                }
                if (has_best_dual &&
                    coarse_dual.coverageCount() <= best_dual.coverageCount() &&
                    coarse_dual.score <= best_dual.score + EPSILON) {
                    continue;
                }

                // 确定potential I5
                pool<SigBit> potential_I5;
                for (auto bit : node_cut->leaves) {
                    if (!cand_cut->leaves.count(bit)) potential_I5.insert(bit);
                }
                for (auto bit : cand_cut->leaves) {
                    if (!node_cut->leaves.count(bit)) potential_I5.insert(bit);
                }
                // 允许共享输入作为I5，增加可配对机会
                for (auto bit : shared_bits) {
                    potential_I5.insert(bit);
                }
                
                // 如果输入少于K，允许使用常量作为selector（保留真实信号作为优先选项）
                if (total_inputs + 1 <= size_t(LUT_SIZE)) {
                    potential_I5.insert(State::S1);
                }
                
                if (potential_I5.empty()) continue;
                
                
                // 选择I5和角色（同时验证LUT6D映射）
                SigBit chosen_i5 = State::Sx;
                bool isZ = false;
                Cut chosen_cut = merged;
                if (!SelectI5AndRoleWithVerify(node, cand_node, merged, potential_I5, chosen_i5, isZ, chosen_cut)) {
                    bool covers_node = current_layer.count(node_output);
                    bool covers_cand = current_layer.count(cand_output);
                    if (covers_node && covers_cand && (layer_invalid_log_count++ < 20)) {
                        log_warning("Layer covering: no valid LUT6D mapping for %s/%s (shared=%zu, total_inputs=%zu)\n",
                                    node->name.c_str(), cand_node->name.c_str(), shared, total_inputs);
                        LogSigBitSetWarning("shared inputs", shared_bits);
                        LogSigBitSetWarning("node cut leaves", node_cut->leaves);
                        LogSigBitSetWarning("cand cut leaves", cand_cut->leaves);
                        LogSigBitSetWarning("merged cut leaves", merged.leaves);
                        LogSigBitSetWarning("potential I5", potential_I5);
                    }
                    continue;
                }
                
                // 创建双输出候选
                DualOutputCandidate dual_cand;
                dual_cand.root1 = node;
                dual_cand.root2 = cand_node;
                dual_cand.merged_cut = chosen_cut;
                dual_cand.I5 = chosen_i5;
                dual_cand.isZ = isZ;
                dual_cand.area_gain = single_area - chosen_cut.area_flow;
                dual_cand.shared_inputs = shared;
                ComputeCoverage(dual_cand, current_layer);
                dual_cand.score = ComputeCandidateScore(dual_cand, current_layer);

                // 覆盖不低于单输出且得分有优势时才考虑
                if (dual_cand.coverageCount() >= single_for_cut.coverageCount() &&
                    dual_cand.score >= single_for_cut.score + score_margin) {
                    if (!has_best_dual || dual_cand.score > best_dual.score + EPSILON ||
                        (fabs(dual_cand.score - best_dual.score) <= EPSILON &&
                         dual_cand.area_gain > best_dual.area_gain + EPSILON)) {
                        best_dual = dual_cand;
                        has_best_dual = true;
                    }
                }
            }
        }

        if (has_best_dual) {
            candidates.push_back(best_dual);
        }
    }
}

// 层级覆盖：选择能最大化覆盖的候选集合
static void LayerCoveringGreedy(const pool<SigBit>& current_layer,
                                vector<DualOutputCandidate>& selected_candidates,
                                pool<SigBit>& next_layer_signals,
                                bool allow_dual) {
    pool<SigBit> uncovered = current_layer;
    selected_candidates.clear();
    next_layer_signals.clear();
    
    // 为当前层所有节点生成候选
    vector<DualOutputCandidate> all_candidates;
    all_candidates.reserve(uncovered.size() * 4);
    
    size_t dual_cand_count = 0, single_cand_count = 0;
    vector<SigBit> ordered_layer = SortedSigBits(current_layer);
    for (SigBit sig : ordered_layer) {
        Cell* driver = bit2driver.count(sig) ? bit2driver[sig] : nullptr;
        if (!driver || !IsCombinationalGate(driver)) continue;
        if (processed_nodes.count(driver)) {
            uncovered.erase(sig);
            continue;
        }
        size_t before = all_candidates.size();
        GenerateDualCandidatesForNode(driver, current_layer, all_candidates, allow_dual);
        for (size_t i = before; i < all_candidates.size(); i++) {
            if (all_candidates[i].isDual()) dual_cand_count++;
            else single_cand_count++;
        }
    }
    
    log("    Generated %zu candidates: %zu dual, %zu single\n", 
        all_candidates.size(), dual_cand_count, single_cand_count);
    
    // 按综合得分排序（得分中已包含覆盖项）
    sort(all_candidates.begin(), all_candidates.end(),
         [](const DualOutputCandidate& a, const DualOutputCandidate& b) {
             if (fabs(a.score - b.score) > EPSILON) {
                 return a.score > b.score;
             }
             if (a.coverageCount() != b.coverageCount())
                 return a.coverageCount() > b.coverageCount();
             if (fabs(a.area_gain - b.area_gain) > EPSILON)
                 return a.area_gain > b.area_gain;
             if (a.isDual() != b.isDual())
                 return a.isDual() > b.isDual();
             if (a.merged_cut.leaves.size() != b.merged_cut.leaves.size())
                 return a.merged_cut.leaves.size() < b.merged_cut.leaves.size();
             if (a.shared_inputs != b.shared_inputs)
                 return a.shared_inputs > b.shared_inputs;
             if (a.root1 != b.root1)
                 return LessCellStable(a.root1, b.root1);
             if (a.root2 != b.root2)
                 return LessCellStable(a.root2, b.root2);
             if (a.isZ != b.isZ)
                 return a.isZ < b.isZ;
             if (a.I5 != b.I5)
                 return LessSigBitStable(a.I5, b.I5);
             return a.merged_cut.signature < b.merged_cut.signature;
         });
    
    // 贪心选择
    pool<Cell*> used_nodes;
    for (const auto& cand : all_candidates) {
        if (uncovered.empty()) break;
        
        // 检查候选是否与已选冲突
        if (used_nodes.count(cand.root1)) continue;
        if (cand.root2 && used_nodes.count(cand.root2)) continue;
        
        // 检查候选是否能覆盖任何未覆盖的信号
        bool covers_something = false;
        for (SigBit sig : cand.covers) {
            if (uncovered.count(sig)) {
                covers_something = true;
                break;
            }
        }
        if (!covers_something) continue;
        
        // 选择这个候选
        selected_candidates.push_back(cand);
        used_nodes.insert(cand.root1);
        if (cand.root2) used_nodes.insert(cand.root2);
        
        // 标记为已覆盖
        for (SigBit sig : cand.covers) {
            uncovered.erase(sig);
        }
        
        // 将cut的leaves添加到下一层
        for (SigBit leaf : cand.merged_cut.leaves) {
            if (!prime_inputs.count(leaf)) {
                next_layer_signals.insert(leaf);
            }
        }
    }
    
    // 处理剩余未覆盖的信号（强制添加单输出LUT）
    vector<SigBit> ordered_uncovered = SortedSigBits(uncovered);
    for (SigBit sig : ordered_uncovered) {
        Cell* driver = bit2driver.count(sig) ? bit2driver[sig] : nullptr;
        if (!driver || !IsCombinationalGate(driver)) continue;
        if (processed_nodes.count(driver)) continue;
        if (used_nodes.count(driver)) continue;
        
        const Cut* best_cut = GetBestCutForCover(driver);
        if (!best_cut) continue;
        
        DualOutputCandidate fallback;
        fallback.root1 = driver;
        fallback.root2 = nullptr;
        fallback.merged_cut = *best_cut;
        fallback.I5 = State::Sx;
        fallback.isZ = false;
        fallback.covers.insert(sig);
        
        selected_candidates.push_back(fallback);
        used_nodes.insert(driver);
        
        for (SigBit leaf : best_cut->leaves) {
            if (!prime_inputs.count(leaf)) {
                next_layer_signals.insert(leaf);
            }
        }
    }
}

static bool BuildSingleLUTFromCut(Cell* root, const Cut& cut, SingleLUTInfo& single_lut)
{
    if (!root)
        return false;

    single_lut = SingleLUTInfo();
    single_lut.output = GetCellOutput(root);

    vector<SigBit> ordered_inputs = SortedSigBits(cut.leaves);
    for (auto bit : ordered_inputs)
        single_lut.inputs.push_back(bit);

    size_t num_inputs = single_lut.inputs.size();
    if (num_inputs > 6)
        return false;

    size_t lut_size = size_t(1) << num_inputs;
    uint64_t init = 0;
    for (size_t i = 0; i < lut_size && i < 64; i++) {
        dict<SigBit, State> bit_map;
        for (size_t j = 0; j < num_inputs; j++) {
            State val = ((i >> j) & 1) ? State::S1 : State::S0;
            if (single_lut.inputs[j].wire == nullptr)
                val = single_lut.inputs[j].data;
            bit_map[single_lut.inputs[j]] = val;
        }
        State out = StateEval(bit_map, single_lut.output);
        if (out == State::S1)
            init |= (uint64_t(1) << i);
    }

    single_lut.INIT = init;
    single_lut.removedNodes.insert(root->name);
    return true;
}

// 将层级覆盖候选转换为LUT6DInfo或SingleLUTInfo
static void ConvertCandidateToLUT(const DualOutputCandidate& cand,
                                  vector<LUT6DInfo>& dual_luts,
                                  vector<SingleLUTInfo>& single_luts) {
    auto build_single_lut = [&](Cell* root, const Cut& cut) {
        SingleLUTInfo single_lut;
        if (BuildSingleLUTFromCut(root, cut, single_lut))
            single_luts.push_back(single_lut);
    };

    if (cand.isDual()) {
        // 双输出LUT
        const Cut* cut1 = GetBestCutForCover(cand.root1);
        const Cut* cut2 = GetBestCutForCover(cand.root2);

        LUT6DInfo lut_info;
        BuildLUT6DInfoFromCandidate(cand, lut_info);

        if (!VerifyLUT6DMapping(lut_info, cand.root1, cand.root2, cand.isZ)) {
            log_warning("Layer covering: invalid LUT6D mapping for %s/%s (I5=%s, isZ=%d), fallback to single LUTs\n",
                        cand.root1 ? cand.root1->name.c_str() : "N/A",
                        cand.root2 ? cand.root2->name.c_str() : "N/A",
                        log_signal(cand.I5), cand.isZ);
            if (cut1) build_single_lut(cand.root1, *cut1);
            if (cut2) build_single_lut(cand.root2, *cut2);
            return;
        }
        
        // 标记要删除的节点
        lut_info.removedNodes.insert(cand.root1->name);
        lut_info.removedNodes.insert(cand.root2->name);

        dual_luts.push_back(lut_info);
    } else {
        // 单输出LUT
        build_single_lut(cand.root1, cand.merged_cut);
    }
}

// 主层级覆盖算法入口
void LayerBasedCoveringMain(Module* module, vector<Cell*>& gates,
                            vector<LUT6DInfo>& dual_luts,
                            vector<SingleLUTInfo>& single_luts,
                            bool allow_dual) {
    log("\n=== Layer-Based Covering Algorithm ===\n");
    log("  Dual-output pairing: %s\n", allow_dual ? "enabled" : "disabled");
    layer_candidate_cut_cache.clear();
    
    // 初始化：从PO开始
    pool<SigBit> current_layer = prime_outputs;
    int layer_num = 0;
    size_t total_dual = 0, total_single = 0;
    
    while (!current_layer.empty()) {
        layer_num++;
        log("  Layer %d: %zu signals to cover\n", layer_num, current_layer.size());
        
        // 移除已经覆盖的信号和PI
        pool<SigBit> filtered_layer;
        for (SigBit sig : current_layer) {
            if (prime_inputs.count(sig)) continue;
            Cell* driver = bit2driver.count(sig) ? bit2driver[sig] : nullptr;
            if (!driver || !IsCombinationalGate(driver)) continue;
            if (processed_nodes.count(driver)) continue;
            filtered_layer.insert(sig);
        }
        
        if (filtered_layer.empty()) {
            log("    All signals covered or are PIs\n");
            break;
        }
        
        // 层级覆盖
        vector<DualOutputCandidate> selected;
        pool<SigBit> next_layer;
        LayerCoveringGreedy(filtered_layer, selected, next_layer, allow_dual);
        
        size_t layer_dual = 0, layer_single = 0;
        
        // 转换并添加LUT
        for (const auto& cand : selected) {
            size_t dual_before = dual_luts.size();
            size_t single_before = single_luts.size();
            ConvertCandidateToLUT(cand, dual_luts, single_luts);
            size_t dual_added = dual_luts.size() - dual_before;
            size_t single_added = single_luts.size() - single_before;
            if (dual_added == 0 && single_added == 0) {
                continue;
            }
            
            // 标记已处理
            processed_nodes.insert(cand.root1);
            if (cand.root2) {
                processed_nodes.insert(cand.root2);
            }
            layer_dual += dual_added;
            layer_single += single_added;
            
            // 更新used_nodes（保护叶子驱动）
            for (SigBit leaf : cand.merged_cut.leaves) {
                Cell* driver = bit2driver.count(leaf) ? bit2driver[leaf] : nullptr;
                if (driver && IsCombinationalGate(driver)) {
                    used_nodes.insert(driver);
                }
            }
        }
        
        log("    Selected: %zu dual-output, %zu single-output LUTs\n", layer_dual, layer_single);
        total_dual += layer_dual;
        total_single += layer_single;
        
        current_layer = next_layer;
        
        // 防止无限循环
        if (layer_num > 1000) {
            log("  WARNING: Layer limit reached, stopping\n");
            break;
        }
    }

    // 兜底：只补齐仍被已选LUT/PO使用到的节点
    pool<Cell*> gate_set;
    for (Cell* node : gates) {
        gate_set.insert(node);
    }

    pool<SigBit> planned_outputs;
    for (const auto& lut_info : dual_luts) {
        planned_outputs.insert(lut_info.Z);
        planned_outputs.insert(lut_info.Z5);
    }
    for (const auto& single_lut : single_luts) {
        planned_outputs.insert(single_lut.output);
    }

    pool<SigBit> required_signals;
    for (SigBit sig : prime_outputs) {
        required_signals.insert(sig);
    }
    for (auto &wire_it : module->wires_) {
        Wire *wire = wire_it.second;
        if (!wire || !wire->port_output) continue;
        SigSpec sig = sigmap(wire);
        for (int i = 0; i < sig.size(); i++) {
            required_signals.insert(sig[i]);
        }
    }
    for (const auto& lut_info : dual_luts) {
        for (auto bit : lut_info.inputs) {
            required_signals.insert(bit);
        }
    }
    for (const auto& single_lut : single_luts) {
        for (auto bit : single_lut.inputs) {
            required_signals.insert(bit);
        }
    }
    for (auto &cell_iter : module->cells_) {
        Cell *cell = cell_iter.second;
        if (!cell || IsCombinationalGate(cell)) continue;
        for (auto &conn : cell->connections()) {
            IdString portname = conn.first;
            if (!yosys_celltypes.cell_input(cell->type, portname)) continue;
            SigSpec sig = sigmap(conn.second);
            for (int i = 0; i < sig.size(); i++) {
                required_signals.insert(sig[i]);
            }
        }
    }

    queue<SigBit> worklist;
    vector<SigBit> ordered_required = SortedSigBits(required_signals);
    for (SigBit sig : ordered_required) {
        worklist.push(sig);
    }

    size_t forced_single = 0;
    pool<SigBit> visited_required;
    while (!worklist.empty()) {
        SigBit sig = worklist.front();
        worklist.pop();
        if (visited_required.count(sig)) continue;
        visited_required.insert(sig);

        if (prime_inputs.count(sig) || sig.wire == nullptr) continue;
        if (planned_outputs.count(sig)) continue;

        Cell* driver = bit2driver.count(sig) ? bit2driver[sig] : nullptr;
        if (!driver || !gate_set.count(driver)) continue;

        const Cut* best_cut = GetBestCutForCover(driver);
        if (!best_cut) continue;

        DualOutputCandidate fallback;
        fallback.root1 = driver;
        fallback.root2 = nullptr;
        fallback.merged_cut = *best_cut;
        fallback.I5 = State::Sx;
        fallback.isZ = false;

        size_t before = single_luts.size();
        ConvertCandidateToLUT(fallback, dual_luts, single_luts);
        if (single_luts.size() == before) {
            continue;
        }

        processed_nodes.insert(driver);
        planned_outputs.insert(sig);
        forced_single++;

        vector<SigBit> ordered_leaves = SortedSigBits(best_cut->leaves);
        for (SigBit leaf : ordered_leaves) {
            if (!visited_required.count(leaf)) {
                worklist.push(leaf);
            }
            Cell* leaf_driver = bit2driver.count(leaf) ? bit2driver[leaf] : nullptr;
            if (leaf_driver && IsCombinationalGate(leaf_driver)) {
                used_nodes.insert(leaf_driver);
            }
        }
    }

    if (forced_single > 0) {
        log("  Forced %zu remaining nodes into single-output LUTs\n", forced_single);
        total_single += forced_single;
    }
    
    log("  Total layers: %d\n", layer_num);
    log("  Total LUTs: %zu dual-output + %zu single-output = %zu\n",
        total_dual, total_single, total_dual + total_single);
}

static void BuildBestCutSingleCover(const vector<Cell *> &gates,
                                    vector<SingleLUTInfo> &single_luts)
{
    single_luts.clear();
    for (Cell *node : gates) {
        if (!node || !IsCombinationalGate(node))
            continue;
        if (!cell2refs.count(node) || cell2refs[node] <= 0)
            continue;

        const Cut *best_cut = GetBestCutForCover(node);
        if (!best_cut)
            continue;

        SingleLUTInfo single_lut;
        if (BuildSingleLUTFromCut(node, *best_cut, single_lut))
            single_luts.push_back(single_lut);
    }
}

struct PostPackEntry {
    size_t idx;
    size_t real_inputs;
    pool<SigBit> leaves;
    uint64_t signature;
};

static void BuildPostPackEntries(const vector<SingleLUTInfo> &single_luts,
                                 vector<PostPackEntry> &entries)
{
    entries.clear();
    entries.reserve(single_luts.size());
    for (size_t i = 0; i < single_luts.size(); i++) {
        PostPackEntry entry;
        entry.idx = i;
        entry.real_inputs = 0;
        entry.signature = 0;
        for (auto inp : single_luts[i].inputs) {
            if (inp.wire == nullptr)
                continue;
            entry.real_inputs++;
            entry.leaves.insert(inp);
            size_t h = hashlib::legacy::djb2_add(inp.wire->name.index_, inp.offset);
            entry.signature |= (1ULL << (h % 64));
        }
        entries.push_back(std::move(entry));
    }

    sort(entries.begin(), entries.end(),
         [](const PostPackEntry &a, const PostPackEntry &b) {
             if (a.real_inputs != b.real_inputs)
                 return a.real_inputs < b.real_inputs;
             return a.idx < b.idx;
         });
}

static void CountModuleMappingCells(Module *module, size_t &aig_cells,
                                    size_t &lut_cells, size_t &gtp_lut_cells)
{
    aig_cells = 0;
    lut_cells = 0;
    gtp_lut_cells = 0;
    for (Cell *cell : module->cells()) {
        if (!cell)
            continue;
        if (cell->type.in(ID($_AND_), ID($_NOT_), ID($and), ID($not))) {
            aig_cells++;
            continue;
        }
        if (cell->type == ID($lut)) {
            lut_cells++;
            continue;
        }
        if (cell->type.in(ID(GTP_LUT2), ID(GTP_LUT3), ID(GTP_LUT4), ID(GTP_LUT5), ID(GTP_LUT6),
                          ID(GTP_LUT4D), ID(GTP_LUT5D), ID(GTP_LUT6D))) {
            gtp_lut_cells++;
            continue;
        }
    }
}

static void MaybePreMapAigWithAbc(Design *design, Module *module)
{
    if (!design || !module)
        return;

    size_t aig_cells = 0, lut_cells = 0, gtp_lut_cells = 0;
    CountModuleMappingCells(module, aig_cells, lut_cells, gtp_lut_cells);

    if (aig_cells == 0)
        return;
    if (lut_cells > 0 || gtp_lut_cells > 0)
        return;

    log("AIG-dominant network detected (%zu AND/NOT cells). Running `abc -lut 6` baseline before LUT6D packing.\n",
        aig_cells);
    Pass::call(design, "abc -lut 6");
    Pass::call(design, "clean");
}

// 后处理：将剩余的单输出LUT配对打包为双输出LUT6D
static void PostPackSingleLUTs(vector<LUT6DInfo> &dual_luts,
                               vector<SingleLUTInfo> &single_luts)
{
    if (single_luts.size() < 2) return;

    vector<PostPackEntry> entries;
    BuildPostPackEntries(single_luts, entries);

    pool<size_t> consumed;
    size_t packed = 0;

    for (size_t i = 0; i < entries.size(); i++) {
        if (consumed.count(entries[i].idx)) continue;
        size_t a_idx = entries[i].idx;
        size_t a_real = entries[i].real_inputs;
        if (a_real > 5) continue;

        // 搜索所有可能的配对候选
        int best_j = -1;
        LUT6DInfo best_lut_info;
        size_t best_merged_size = 999;
        size_t j_end = entries.size();
        if (POSTPACK_PASS1_SCAN_WINDOW > 0)
            j_end = std::min(entries.size(), i + 1 + size_t(POSTPACK_PASS1_SCAN_WINDOW));

        for (size_t j = i + 1; j < j_end; j++) {
            if (consumed.count(entries[j].idx)) continue;
            // 快速过滤：signature合并后popcount检查
            uint64_t combined_sig = entries[i].signature | entries[j].signature;
            if (__builtin_popcountll(combined_sig) > LUT_SIZE) continue;

            // 合并叶子
            pool<SigBit> merged_leaves = entries[i].leaves;
            for (auto &leaf : entries[j].leaves)
                merged_leaves.insert(leaf);
            size_t merged_size = merged_leaves.size();

            // 必须能装入LUT6D
            // 共享输入：merged_size <= 6，可用共享信号做I5
            // 析取输入：merged_size <= 5，用常量State::S1做I5
            if (merged_size > 6) continue;
            if (merged_size > best_merged_size) continue;  // 只找最紧凑的配对

            SingleLUTInfo &lutA = single_luts[a_idx];
            SingleLUTInfo &lutB = single_luts[entries[j].idx];
            SigBit outA = lutA.output;
            SigBit outB = lutB.output;

            // 依赖对不能直接打包（例如 A 依赖 B 输出），否则容易形成输入=输出自环。
            if (entries[i].leaves.count(outB) || entries[j].leaves.count(outA))
                continue;

            Cell *cellA = bit2driver.count(lutA.output) ? bit2driver[lutA.output] : nullptr;
            Cell *cellB = bit2driver.count(lutB.output) ? bit2driver[lutB.output] : nullptr;
            if (!cellA || !cellB) continue;

            // 构建候选I5集合
            pool<SigBit> potential_I5;
            if (merged_size + 1 <= (size_t)LUT_SIZE)
                potential_I5.insert(State::S1);  // 常量selector

            // 共享信号也可以做I5
            for (auto &leaf : entries[i].leaves) {
                if (entries[j].leaves.count(leaf))
                    potential_I5.insert(leaf);
            }

            // 没有任何可用的I5，跳过
            if (potential_I5.empty()) continue;

            // 用既有基础设施尝试I5和角色分配
            Cut fake_cut;
            fake_cut.leaves = merged_leaves;
            SigBit chosen_i5;
            bool isZ;
            Cut chosen_cut;
            if (!SelectI5AndRoleWithVerify(cellA, cellB, fake_cut, potential_I5,
                                           chosen_i5, isZ, chosen_cut))
                continue;

            // 构建LUT6DInfo
            DualOutputCandidate temp;
            temp.root1 = cellA;
            temp.root2 = cellB;
            temp.merged_cut = chosen_cut;
            temp.I5 = chosen_i5;
            temp.isZ = isZ;
            LUT6DInfo lut_info;
            BuildLUT6DInfoFromCandidate(temp, lut_info);

            // 再次全验证
            if (!VerifyLUT6DMapping(lut_info, cellA, cellB, isZ))
                continue;

            // 找到有效配对
            if (best_j < 0 || merged_size < best_merged_size) {
                best_j = (int)j;
                best_lut_info = lut_info;
                best_merged_size = merged_size;
            }
        }

        if (best_j >= 0) {
            size_t b_idx = entries[best_j].idx;
            SingleLUTInfo &lutA = single_luts[a_idx];
            SingleLUTInfo &lutB = single_luts[b_idx];

            best_lut_info.removedNodes.insert(lutA.removedNodes.begin(), lutA.removedNodes.end());
            best_lut_info.removedNodes.insert(lutB.removedNodes.begin(), lutB.removedNodes.end());
            dual_luts.push_back(best_lut_info);
            consumed.insert(a_idx);
            consumed.insert(b_idx);
            packed++;
        }
    }

    if (packed > 0) {
        vector<SingleLUTInfo> remaining;
        remaining.reserve(single_luts.size() - consumed.size());
        for (size_t i = 0; i < single_luts.size(); i++) {
            if (!consumed.count(i))
                remaining.push_back(std::move(single_luts[i]));
        }
        single_luts = std::move(remaining);
        log("  Post-packing: merged %zu single-LUT pairs into dual LUTs\n", packed);
    }

    // 第二轮：利用备选cuts尝试更多配对机会
    // 对于6输入的LUT，如果节点有更小的备选cut，用备选cut可能使配对可行
    if (single_luts.size() < 2) return;

    // 重新构建entries
    consumed.clear();
    BuildPostPackEntries(single_luts, entries);

    // 对超大集合做分块：每块限制规模，避免一次性O(N^2)爆炸；
    // 通过多块覆盖提升大型网络（如hyp）的剩余可配对机会。
    size_t pass2_entry_cap = 0;
    if (POSTPACK_PASS2_MAX_SINGLE > 0)
        pass2_entry_cap = size_t(POSTPACK_PASS2_MAX_SINGLE);
    size_t pass2_chunk_size = entries.size();
    if (pass2_entry_cap > 0)
        pass2_chunk_size = pass2_entry_cap;
    if (pass2_chunk_size == 0)
        pass2_chunk_size = entries.size();

    vector<pair<size_t, size_t>> pass2_chunks;
    size_t full_chunk_count = (entries.size() + pass2_chunk_size - 1) / pass2_chunk_size;
    size_t max_chunk_count = full_chunk_count;
    if (POSTPACK_PASS2_MAX_CHUNKS > 0)
        max_chunk_count = std::min(max_chunk_count, size_t(POSTPACK_PASS2_MAX_CHUNKS));
    if (max_chunk_count == 0)
        max_chunk_count = 1;

    if (full_chunk_count <= max_chunk_count) {
        for (size_t start = 0; start < entries.size(); start += pass2_chunk_size) {
            size_t end = std::min(entries.size(), start + pass2_chunk_size);
            if (end > start)
                pass2_chunks.emplace_back(start, end);
        }
    } else {
        if (max_chunk_count == 1) {
            size_t end = std::min(entries.size(), pass2_chunk_size);
            pass2_chunks.emplace_back(0, end);
        } else {
            size_t prefix_count = max_chunk_count - 1;
            for (size_t c = 0; c < prefix_count; c++) {
                size_t start = c * pass2_chunk_size;
                size_t end = std::min(entries.size(), start + pass2_chunk_size);
                if (end > start)
                    pass2_chunks.emplace_back(start, end);
            }
            size_t tail_start = (full_chunk_count - 1) * pass2_chunk_size;
            if (tail_start < entries.size())
                pass2_chunks.emplace_back(tail_start, entries.size());
        }
        log("  Post-packing pass 2: chunking candidates total=%zu chunk=%zu full_chunks=%zu run_chunks=%zu\n",
            entries.size(), pass2_chunk_size, full_chunk_count, pass2_chunks.size());
    }
    if (entries.size() < 2) return;

    size_t packed2 = 0;
    size_t pass2_cut_limit = size_t(std::max(4, MAX_I5_CANDIDATES + 2));
    for (size_t chunk_idx = 0; chunk_idx < pass2_chunks.size(); chunk_idx++) {
        size_t chunk_begin = pass2_chunks[chunk_idx].first;
        size_t chunk_end = pass2_chunks[chunk_idx].second;
        if (chunk_end <= chunk_begin + 1)
            continue;
        size_t packed_before_chunk = packed2;
	    for (size_t i = chunk_begin; i < chunk_end; i++) {
	        if (consumed.count(entries[i].idx)) continue;
	        size_t a_idx = entries[i].idx;
            SigBit outA = single_luts[a_idx].output;

	        Cell *cellA = bit2driver.count(single_luts[a_idx].output)
	                          ? bit2driver[single_luts[a_idx].output] : nullptr;
	        if (!cellA) continue;

        // 收集A的备选cuts（包括best cut和更小的cuts）
	        vector<const Cut*> cuts_a;
	        if (cell2cuts.count(cellA)) {
	            for (auto &c : cell2cuts[cellA]) {
	                if (c.leaves.size() <= 5) cuts_a.push_back(&c);
	            }
	        }
            if (cuts_a.size() > pass2_cut_limit)
                cuts_a.resize(pass2_cut_limit);
	        if (cuts_a.empty()) continue;

        int best_j = -1;
        LUT6DInfo best_lut_info;
        size_t best_merged_size = 999;
        size_t j_end_pass2 = chunk_end;
        if (POSTPACK_PASS2_SCAN_WINDOW > 0)
            j_end_pass2 = std::min(chunk_end, i + 1 + size_t(POSTPACK_PASS2_SCAN_WINDOW));

	        for (size_t j = i + 1; j < j_end_pass2; j++) {
	            if (consumed.count(entries[j].idx)) continue;
	            size_t b_idx_inner = entries[j].idx;
                SigBit outB = single_luts[b_idx_inner].output;

                if (entries[i].leaves.count(outB) || entries[j].leaves.count(outA))
                    continue;

	            Cell *cellB = bit2driver.count(single_luts[b_idx_inner].output)
	                              ? bit2driver[single_luts[b_idx_inner].output] : nullptr;
	            if (!cellB) continue;

	            vector<const Cut*> cuts_b;
	            if (cell2cuts.count(cellB)) {
	                for (auto &c : cell2cuts[cellB]) {
	                    if (c.leaves.size() <= 5) cuts_b.push_back(&c);
	                }
	            }
                if (cuts_b.size() > pass2_cut_limit)
                    cuts_b.resize(pass2_cut_limit);
	            if (cuts_b.empty()) continue;

            // 尝试所有cut组合
            bool found_pair = false;
	            for (auto *ca : cuts_a) {
	                if (found_pair) break;
	                for (auto *cb : cuts_b) {
                        if (ca->leaves.count(outB) || cb->leaves.count(outA))
                            continue;

	                    // 快速feasibility检查
	                    if (!Cut::canMergeFast(*ca, *cb, LUT_SIZE)) continue;

                    pool<SigBit> merged_leaves = ca->leaves;
                    for (auto &leaf : cb->leaves) merged_leaves.insert(leaf);
                    size_t merged_size = merged_leaves.size();
                    if (merged_size > 6) continue;
                    if (merged_size >= best_merged_size) continue;

                    pool<SigBit> potential_I5;
                    if (merged_size + 1 <= (size_t)LUT_SIZE)
                        potential_I5.insert(State::S1);
                    for (auto &leaf : ca->leaves) {
                        if (cb->leaves.count(leaf)) potential_I5.insert(leaf);
                    }
                    if (potential_I5.empty()) continue;

                    Cut fake_cut;
                    fake_cut.leaves = merged_leaves;
                    SigBit chosen_i5;
                    bool isZ;
                    Cut chosen_cut;
                    if (!SelectI5AndRoleWithVerify(cellA, cellB, fake_cut, potential_I5,
                                                   chosen_i5, isZ, chosen_cut))
                        continue;

                    DualOutputCandidate temp;
                    temp.root1 = cellA;
                    temp.root2 = cellB;
                    temp.merged_cut = chosen_cut;
                    temp.I5 = chosen_i5;
                    temp.isZ = isZ;
                    LUT6DInfo lut_info;
                    BuildLUT6DInfoFromCandidate(temp, lut_info);

                    if (!VerifyLUT6DMapping(lut_info, cellA, cellB, isZ))
                        continue;

                    best_j = (int)j;
                    best_lut_info = lut_info;
                    best_merged_size = merged_size;
                    found_pair = true;
                    break;
                }
            }
        }

        if (best_j >= 0) {
            size_t b_idx2 = entries[best_j].idx;
            best_lut_info.removedNodes.insert(single_luts[a_idx].removedNodes.begin(),
                                               single_luts[a_idx].removedNodes.end());
            best_lut_info.removedNodes.insert(single_luts[b_idx2].removedNodes.begin(),
                                               single_luts[b_idx2].removedNodes.end());
            dual_luts.push_back(best_lut_info);
            consumed.insert(a_idx);
            consumed.insert(b_idx2);
            packed2++;
        }
    }
        size_t merged_in_chunk = packed2 - packed_before_chunk;
        if (merged_in_chunk > 0) {
            log("  Post-packing pass 2 chunk %zu/%zu: merged %zu pairs\n",
                chunk_idx + 1, pass2_chunks.size(), merged_in_chunk);
        }
    }

    if (packed2 > 0) {
        vector<SingleLUTInfo> remaining;
        remaining.reserve(single_luts.size() - consumed.size());
        for (size_t i = 0; i < single_luts.size(); i++) {
            if (!consumed.count(i))
                remaining.push_back(std::move(single_luts[i]));
        }
        single_luts = std::move(remaining);
        log("  Post-packing pass 2 (alt-cuts): merged %zu more pairs\n", packed2);
    }
}

void LUT6DMapping(Module *module, vector<Cell *> &gates)
{  
    // Initialize prime inputs and outputs
    prime_inputs.clear();
    prime_outputs.clear();
    GetPrimeInputOutput(module, prime_inputs, prime_outputs);

    // 基于网络规模做自适应预算，优先控制复杂度的增长速度。
    int saved_max_cuts_per_node = MAX_CUTS_PER_NODE;
    int saved_mapping_rounds = MAPPING_ROUNDS;
    int saved_max_pairs_per_node = MAX_PAIRS_PER_NODE;
    int saved_max_disjoint_pairs_per_node = MAX_DISJOINT_PAIRS_PER_NODE;
    int saved_layer_cuts_per_node = LAYER_CUTS_PER_NODE;
    int saved_max_i5_candidates = MAX_I5_CANDIDATES;
    int saved_postpack_pass1_scan_window = POSTPACK_PASS1_SCAN_WINDOW;
    int saved_postpack_pass2_max_single = POSTPACK_PASS2_MAX_SINGLE;
    int saved_postpack_pass2_scan_window = POSTPACK_PASS2_SCAN_WINDOW;
    int saved_postpack_pass2_max_chunks = POSTPACK_PASS2_MAX_CHUNKS;
    int saved_pair_seed_multiplier = PAIR_SEED_MULTIPLIER;

    size_t gate_count = gates.size();
    if (gate_count >= 200000) {
        // 超大网络：提升post-pack覆盖，重点改善hyp类算例。
        MAX_CUTS_PER_NODE = 40;
        MAX_PAIRS_PER_NODE = 14;
        MAX_DISJOINT_PAIRS_PER_NODE = 10;
        LAYER_CUTS_PER_NODE = 2;
        MAX_I5_CANDIDATES = 6;
        MAPPING_ROUNDS = 4;
        POSTPACK_PASS1_SCAN_WINDOW = 320;
        POSTPACK_PASS2_MAX_SINGLE = 24000;
        POSTPACK_PASS2_SCAN_WINDOW = 80;
        POSTPACK_PASS2_MAX_CHUNKS = 2;
        PAIR_SEED_MULTIPLIER = 8;
    } else if (gate_count >= 50000) {
        // 很大网络：重点给mode3/post-pack更多空间，覆盖multiplier一类。
        MAX_CUTS_PER_NODE = 56;
        MAX_PAIRS_PER_NODE = 20;
        MAX_DISJOINT_PAIRS_PER_NODE = 14;
        LAYER_CUTS_PER_NODE = 3;
        MAX_I5_CANDIDATES = 6;
        MAPPING_ROUNDS = 6;
        POSTPACK_PASS1_SCAN_WINDOW = 512;
        POSTPACK_PASS2_MAX_SINGLE = 12000;
        POSTPACK_PASS2_SCAN_WINDOW = 128;
        POSTPACK_PASS2_MAX_CHUNKS = 1;
        PAIR_SEED_MULTIPLIER = 10;
    } else if (gate_count >= 30000) {
        // 中大网络：sqrt常落在此区间，提升切割质量并允许更充分二次配对。
        MAX_CUTS_PER_NODE = 72;
        MAX_PAIRS_PER_NODE = 24;
        MAX_DISJOINT_PAIRS_PER_NODE = 16;
        LAYER_CUTS_PER_NODE = 3;
        MAX_I5_CANDIDATES = 6;
        MAPPING_ROUNDS = 6;
        POSTPACK_PASS1_SCAN_WINDOW = 640;
        POSTPACK_PASS2_MAX_SINGLE = 10000;
        POSTPACK_PASS2_SCAN_WINDOW = 160;
        POSTPACK_PASS2_MAX_CHUNKS = 1;
        PAIR_SEED_MULTIPLIER = 11;
    } else if (gate_count >= 3000) {
        MAX_CUTS_PER_NODE = 144;
        MAX_PAIRS_PER_NODE = 32;
        MAX_DISJOINT_PAIRS_PER_NODE = 20;
        LAYER_CUTS_PER_NODE = 3;
        MAX_I5_CANDIDATES = 8;
        MAPPING_ROUNDS = 8;
        POSTPACK_PASS1_SCAN_WINDOW = 640;
        POSTPACK_PASS2_MAX_SINGLE = 6144;
        POSTPACK_PASS2_SCAN_WINDOW = 256;
        POSTPACK_PASS2_MAX_CHUNKS = 2;
        PAIR_SEED_MULTIPLIER = 14;
    } else if (gate_count >= 1500) {
        MAX_CUTS_PER_NODE = 64;
        MAX_PAIRS_PER_NODE = 20;
        MAX_DISJOINT_PAIRS_PER_NODE = 12;
        LAYER_CUTS_PER_NODE = 3;
        MAX_I5_CANDIDATES = 6;
        MAPPING_ROUNDS = 5;
        POSTPACK_PASS1_SCAN_WINDOW = 256;
        POSTPACK_PASS2_MAX_SINGLE = 2048;
        POSTPACK_PASS2_SCAN_WINDOW = 192;
        POSTPACK_PASS2_MAX_CHUNKS = 1;
        PAIR_SEED_MULTIPLIER = 9;
    }

    log("Runtime budget: gates=%zu cuts=%d pairs=%d disjoint=%d layer_cuts=%d i5=%d rounds=%d postpack1_win=%d postpack2_limit=%d postpack2_win=%d postpack2_chunks=%d seed_mul=%d\n",
        gate_count, MAX_CUTS_PER_NODE, MAX_PAIRS_PER_NODE, MAX_DISJOINT_PAIRS_PER_NODE,
        LAYER_CUTS_PER_NODE, MAX_I5_CANDIDATES, MAPPING_ROUNDS, POSTPACK_PASS1_SCAN_WINDOW,
        POSTPACK_PASS2_MAX_SINGLE, POSTPACK_PASS2_SCAN_WINDOW, POSTPACK_PASS2_MAX_CHUNKS, PAIR_SEED_MULTIPLIER);
    
    // ABC风格：初始化估计引用计数
    log("Initializing estimated reference counts (ABC-style)...\n");
    cell2est_refs.clear();
    cell2refs.clear();
    for (Cell* node : gates) {
        if (!IsCombinationalGate(node)) continue;
        SigBit output = GetCellOutput(node);
        int fanout = bit2reader.count(output) ? bit2reader[output].size() : 0;
        // 对于PO，增加一个虚拟引用
        if (prime_outputs.count(output)) fanout++;
        cell2est_refs[node] = max(1.0f, (float)fanout);
        cell2refs[node] = fanout;
    }
    
    // Phase 1: 为每个节点生成割集
    log("Phase 1: Generating cuts for all nodes...\n");

    for (Cell* node : gates) {
        vector<Cut> cuts;
        GenerateCutsForNode(node, cuts);
        cell2cuts[node] = cuts;

        // 计算节点深度
        if (!cuts.empty()) {
            cell2depth[node] = cuts[0].depth;
            for (auto &cut : cuts) {
                cell2depth[node] = min(cell2depth[node], cut.depth);
            }
        } else {
            cell2depth[node] = 0;
        }
        
        // 选择初始best cut（delay优先）
        if (!cuts.empty()) {
            cell2bestcut[node] = cuts[0];
        }
    }

    for (Cell* cell : gates) {
        log_debug("Node %s output: %s, depth: %zu, cuts: %zu\n",
                  cell->name.c_str(), log_signal(GetCellOutput(cell)),
                  cell2depth[cell], cell2cuts[cell].size());
    }

    // Phase 2: 计算Arrival Time和Required Time（ABC风格）
    log("\nPhase 2: Computing arrival and required times (ABC-style)...\n");
    ComputeArrivalTimes(gates);
    ComputeRequiredTimes(module, gates);

    // 使用当前best cut恢复引用计数，作为下一步area flow的基础
    log("\nPhase 2.25: Recomputing ref-counts from best cuts...\n");
    RecomputeRefsFromPOs(module, gates);
    
    // ABC风格：多轮迭代优化
    int mapping_rounds = MAPPING_ROUNDS;
    if ((int)gates.size() >= 900 && (int)gates.size() < EXTRA_MODE_GATE_LIMIT)
        mapping_rounds += 1;
    log("\nPhase 2.5: Multi-round mapping optimization (ABC-style, %d rounds)...\n", mapping_rounds);
    for (int round = 1; round <= mapping_rounds; round++) {
        log("  Round %d: %s optimization...\n", round, round == 1 ? "delay" : "area");
        size_t changed_nodes = 0;

        // 更新每个节点的best cut
        for (Cell* node : gates) {
            if (!IsCombinationalGate(node)) continue;
            if (!cell2cuts.count(node) || cell2cuts[node].empty()) continue;
            bool had_prev = cell2bestcut.count(node);
            Cut prev_cut;
            if (had_prev)
                prev_cut = cell2bestcut[node];

            float required = cell2required.count(node) ? cell2required[node] : 1e9f;
            
            Cut* best = nullptr;
            for (auto& cut : cell2cuts[node]) {
                // 计算这个cut的arrival time
                float max_arr = 0.0f;
                for (auto leaf : cut.leaves) {
                    Cell* driver = bit2driver.count(leaf) ? bit2driver[leaf] : nullptr;
                    if (driver && cell2arrival.count(driver)) {
                        max_arr = max(max_arr, cell2arrival[driver]);
                    }
                }
                // 临时更新cut的arrival time
                Cut& mutable_cut = const_cast<Cut&>(cut);
                mutable_cut.arrival_time = max_arr + 1.0f;
                
                // 更新area flow
                mutable_cut.area_flow = ComputeAreaFlowABC(cut.leaves);
                
                if (round == 1) {
                    // Round 1: delay优先
                    if (!best || cut < *best) {
                        best = &mutable_cut;
                    }
                } else {
                    // Round 2+: area优先（在timing约束内）
                    if (!best || cut.betterAreaThan(*best, required)) {
                        best = &mutable_cut;
                    }
                }
            }
            
            if (best) {
                bool changed = true;
                if (had_prev &&
                    prev_cut.signature == best->signature &&
                    prev_cut.leaves.size() == best->leaves.size() &&
                    CutLeavesEqual(prev_cut.leaves, best->leaves))
                    changed = false;
                cell2bestcut[node] = *best;
                if (changed)
                    changed_nodes++;
            }
        }
        
        // 更新arrival time（基于新的best cuts）
        ComputeArrivalTimes(gates);
        
        // 更新ref-count（用于下一轮area flow计算）
        RecomputeRefsFromPOs(module, gates);
        
        // 统计当前轮次的结果
        size_t total_area = 0;
        float max_delay = 0.0f;
        for (Cell* node : gates) {
            if (cell2bestcut.count(node)) {
                total_area += 1;  // 每个best cut对应一个LUT
                max_delay = max(max_delay, cell2bestcut[node].arrival_time);
            }
        }
        log("    Round %d result: estimated LUTs=%zu, max_delay=%.1f, changed=%zu\n",
            round, total_area, max_delay, changed_nodes);
        if (round >= 2 && changed_nodes == 0) {
            log("    Early stop: best cuts converged at round %d\n", round);
            break;
        }
    }

    // 末尾精确面积恢复：在轻度timing约束下，按ref/deref真实代价选择best cut。
    int final_area_rounds = FINAL_AREA_RECOVERY_ROUNDS;
    if (gate_count >= 50000)
        final_area_rounds = 1;
    if (final_area_rounds > 0)
        log("\nPhase 2.75: Exact area-recovery rounds (%d rounds)...\n", final_area_rounds);

    for (int round = 1; round <= final_area_rounds; round++) {
        size_t changed_nodes = 0;

        for (auto it = gates.rbegin(); it != gates.rend(); ++it) {
            Cell* node = *it;
            if (!IsCombinationalGate(node)) continue;
            if (!cell2cuts.count(node) || cell2cuts[node].empty()) continue;
            if (!cell2refs.count(node) || cell2refs[node] <= 0) continue;
            if (!cell2bestcut.count(node)) continue;

            Cut old_cut = cell2bestcut[node];
            float required = cell2required.count(node) ? cell2required[node] : 1e9f;
            float relaxed_required = required;
            if (gate_count >= 10000) {
                relaxed_required = 1e9f;
            } else if (required < 1e8f) {
                relaxed_required += AREA_ROUND_REQUIRED_SLACK;
                relaxed_required += required * AREA_ROUND_REQUIRED_RELAX;
            }

            DerefSupportFromCut(&old_cut);

            const Cut* best_cut = nullptr;
            int best_added = std::numeric_limits<int>::max();
            double best_flow = std::numeric_limits<double>::infinity();

            for (auto& cut : cell2cuts[node]) {
                float max_arr = 0.0f;
                for (auto leaf : cut.leaves) {
                    Cell* driver = bit2driver.count(leaf) ? bit2driver[leaf] : nullptr;
                    if (driver && cell2arrival.count(driver))
                        max_arr = max(max_arr, cell2arrival[driver]);
                }

                Cut& mutable_cut = const_cast<Cut&>(cut);
                mutable_cut.arrival_time = max_arr + 1.0f;
                if (mutable_cut.arrival_time > relaxed_required + EPSILON)
                    continue;

                mutable_cut.area_flow = ComputeAreaFlowABC(cut.leaves);
                int added = RefSupportFromCut(&mutable_cut);
                DerefSupportFromCut(&mutable_cut);

                bool better = false;
                if (added < best_added)
                    better = true;
                else if (added == best_added && mutable_cut.area_flow < best_flow - EPSILON)
                    better = true;
                else if (added == best_added && fabs(mutable_cut.area_flow - best_flow) <= EPSILON &&
                         best_cut && mutable_cut.leaves.size() > best_cut->leaves.size())
                    better = true;
                else if (added == best_added && fabs(mutable_cut.area_flow - best_flow) <= EPSILON &&
                         best_cut && mutable_cut.leaves.size() == best_cut->leaves.size() &&
                         mutable_cut.depth > best_cut->depth)
                    better = true;

                if (better) {
                    best_cut = &mutable_cut;
                    best_added = added;
                    best_flow = mutable_cut.area_flow;
                }
            }

            if (!best_cut)
                best_cut = &old_cut;

            RefSupportFromCut(best_cut);
            bool changed =
                old_cut.signature != best_cut->signature ||
                old_cut.leaves.size() != best_cut->leaves.size() ||
                !CutLeavesEqual(old_cut.leaves, best_cut->leaves);
            cell2bestcut[node] = *best_cut;
            if (changed)
                changed_nodes++;
        }

        ComputeArrivalTimes(gates);
        RecomputeRefsFromPOs(module, gates);
        log("    Final area round %d: changed=%zu\n", round, changed_nodes);
        if (changed_nodes == 0)
            break;
    }

    // Phase 3: bit-level随机仿真目前不参与映射决策，跳过以提升稳定性和速度
    log("\nPhase 3: Skipping non-decision bit-state simulation...\n");

    vector<LUT6DInfo> lut_infos;
    vector<SingleLUTInfo> single_luts;
    vector<LUT6DInfo> lut_infos_dual_mode;
    vector<SingleLUTInfo> single_luts_dual_mode;
    vector<LUT6DInfo> lut_infos_disjoint_mode;
    vector<SingleLUTInfo> single_luts_disjoint_mode;
    vector<SingleLUTInfo> single_luts_bestcut_mode;

    auto run_dual_covering_mode = [&](vector<LUT6DInfo>& out_dual_luts,
                                      vector<SingleLUTInfo>& out_single_luts) {
        processed_nodes.clear();
        used_nodes.clear();
        layer_invalid_log_count = 0;
        LayerBasedCoveringMain(module, gates, out_dual_luts, out_single_luts, true);
    };

    auto run_disjoint_aggressive_mode = [&](vector<LUT6DInfo>& out_dual_luts,
                                             vector<SingleLUTInfo>& out_single_luts) {
        int orig_layer_cuts = LAYER_CUTS_PER_NODE;
        int orig_disjoint_pairs = MAX_DISJOINT_PAIRS_PER_NODE;
        double orig_no_share_gain = MIN_AREA_GAIN_FOR_NO_SHARE;

        LAYER_CUTS_PER_NODE = 6;
        MAX_DISJOINT_PAIRS_PER_NODE = 32;
        MIN_AREA_GAIN_FOR_NO_SHARE = -1.5;

        processed_nodes.clear();
        used_nodes.clear();
        layer_invalid_log_count = 0;
        LayerBasedCoveringMain(module, gates, out_dual_luts, out_single_luts, true);

        LAYER_CUTS_PER_NODE = orig_layer_cuts;
        MAX_DISJOINT_PAIRS_PER_NODE = orig_disjoint_pairs;
        MIN_AREA_GAIN_FOR_NO_SHARE = orig_no_share_gain;
    };

    bool run_layer_modes = (gates.size() < size_t(50000));

    // Mode 1: 标准双输出覆盖
    if (run_layer_modes) {
        log("\n=== Covering Mode 1: Standard dual-enabled ===\n");
        run_dual_covering_mode(lut_infos_dual_mode, single_luts_dual_mode);
    } else {
        log("\n=== Covering Mode 1: skipped for very large network (%zu gates)\n", gates.size());
    }

    // Mode 2: 激进析取配对覆盖（仅中小规模网络）
    bool run_extra_modes = run_layer_modes && (gates.size() <= (size_t)EXTRA_MODE_GATE_LIMIT);
    if (run_extra_modes) {
        log("\n=== Covering Mode 2: Disjoint-aggressive ===\n");
        run_disjoint_aggressive_mode(lut_infos_disjoint_mode, single_luts_disjoint_mode);
    }

    // Mode 3: 纯单输出覆盖
    log("\n=== Covering Mode 3: Best-cut single-output ===\n");
    BuildBestCutSingleCover(gates, single_luts_bestcut_mode);

    // 对所有模式执行后处理打包
    if (run_layer_modes)
        PostPackSingleLUTs(lut_infos_dual_mode, single_luts_dual_mode);
    if (run_extra_modes) {
        PostPackSingleLUTs(lut_infos_disjoint_mode, single_luts_disjoint_mode);
    }
    vector<LUT6DInfo> dual_from_bestcut;
    PostPackSingleLUTs(dual_from_bestcut, single_luts_bestcut_mode);

    // 比较所有模式的总LUT数，选最优
    size_t total_mode1 = run_layer_modes
        ? (lut_infos_dual_mode.size() + single_luts_dual_mode.size())
        : SIZE_MAX;
    size_t total_mode2 = run_extra_modes
        ? (lut_infos_disjoint_mode.size() + single_luts_disjoint_mode.size())
        : SIZE_MAX;
    size_t total_mode3 = dual_from_bestcut.size() + single_luts_bestcut_mode.size();

    size_t best_total = std::min({total_mode1, total_mode2, total_mode3});
    const char* picked_name = "unknown";

    if (best_total == total_mode1) {
        lut_infos = std::move(lut_infos_dual_mode);
        single_luts = std::move(single_luts_dual_mode);
        picked_name = "standard-dual";
    } else if (best_total == total_mode2) {
        lut_infos = std::move(lut_infos_disjoint_mode);
        single_luts = std::move(single_luts_disjoint_mode);
        picked_name = "disjoint-aggressive";
    } else {
        lut_infos = std::move(dual_from_bestcut);
        single_luts = std::move(single_luts_bestcut_mode);
        picked_name = "bestcut-single+postpack";
    }

    log("  Covering mode decision: mode1=%zu, mode2=%s, mode3=%zu, picked=%s (%zu LUTs)\n",
        total_mode1,
        run_extra_modes ? std::to_string(total_mode2).c_str() : "skipped",
        total_mode3, picked_name, best_total);
    
    // 先添加所有LUT，再基于实际输出删除原节点
    mapped_lut_outputs.clear();
    log("\nTotal LUT6D cells to be added: %zu\n", lut_infos.size());
    for (auto lut_info : lut_infos) {
        lut_info.addLUT6D(module);
    }
    
    log("Total single-output LUTs to be added: %zu\n", single_luts.size());
    for (auto& single_lut : single_luts) {
        single_lut.addSingleLUT(module);
    }

    CollectMappedLUTOutputs(module, mapped_lut_outputs);
    size_t removed_cells = 0;
    auto remove_if_mapped = [&](const IdString &cell_name) {
        if (!module->cells_.count(cell_name))
            return;
        Cell *cell = module->cells_.at(cell_name);
        if (!cell)
            return;
        SigBit out = GetCellOutput(cell);
        if (!mapped_lut_outputs.count(out))
            return;
        module->remove(cell);
        removed_cells++;
    };
    for (const auto &lut_info : lut_infos) {
        for (const auto &cell_name : lut_info.removedNodes) {
            remove_if_mapped(cell_name);
        }
    }
    for (const auto &single_lut : single_luts) {
        for (const auto &cell_name : single_lut.removedNodes) {
            remove_if_mapped(cell_name);
        }
    }
    log("Removed %zu original cells covered by LUT outputs\n", removed_cells);
    
    log("\nFinal: %zu LUT6D + %zu single LUT = %zu total LUTs\n", 
        lut_infos.size(), single_luts.size(), lut_infos.size() + single_luts.size());

    // 恢复全局预算，避免影响后续module。
    MAX_CUTS_PER_NODE = saved_max_cuts_per_node;
    MAPPING_ROUNDS = saved_mapping_rounds;
    MAX_PAIRS_PER_NODE = saved_max_pairs_per_node;
    MAX_DISJOINT_PAIRS_PER_NODE = saved_max_disjoint_pairs_per_node;
    LAYER_CUTS_PER_NODE = saved_layer_cuts_per_node;
    MAX_I5_CANDIDATES = saved_max_i5_candidates;
    POSTPACK_PASS1_SCAN_WINDOW = saved_postpack_pass1_scan_window;
    POSTPACK_PASS2_MAX_SINGLE = saved_postpack_pass2_max_single;
    POSTPACK_PASS2_SCAN_WINDOW = saved_postpack_pass2_scan_window;
    POSTPACK_PASS2_MAX_CHUNKS = saved_postpack_pass2_max_chunks;
    PAIR_SEED_MULTIPLIER = saved_pair_seed_multiplier;
}

// 清理所有全局数据结构，释放内存
void CleanupGlobalDataStructures()
{
    log("Cleaning up global data structures...\n");
    
    // 清理所有dict和pool
    sigmap.clear();
    bit2driver.clear();
    bit2reader.clear();
    cell2bits.clear();
    cell2cuts.clear();
    cell2depth.clear();
    cell2level.clear();
    cell2arrival.clear();
    cell2required.clear();
    cell2bestcut.clear();
    layer_candidate_cut_cache.clear();
    cell2index.clear();
    processed_nodes.clear();
    used_nodes.clear();
    prime_outputs.clear();
    prime_inputs.clear();
    mapped_lut_outputs.clear();
    cell2est_refs.clear();   // ABC风格：清理估计引用计数
    cell2refs.clear();       // ABC风格：清理实际引用计数
    
    log("Memory cleanup complete.\n");
}

void LUT6DMapperMain(Module *module)
{
    // 在开始处理前先清理内存
    CleanupGlobalDataStructures();  
    vector<Cell *> gates;
    CheckCellWidth(module);
    GetTopoSortedGates(gates);
    LUT6DMapping(module, gates);
    CleanupGlobalDataStructures();
}

struct LUT6DMapPass : public Pass {
    LUT6DMapPass() : Pass("lut6d_map", "Map logic to LUT6D cells for Yosys-to-Yosys flow.") {}
    void help() override
    {
        log("\n");
        log("\tlut6d_map [options] [selection]\n");
        log("\n");
        log("\tThis pass maps logic to LUT6D cells, which are 6-input LUTs with dual outputs.\n");
        log("\n");
    }
    void execute(std::vector<std::string> args, RTLIL::Design *design) override
    {
        log_header(design, "Start MapperPass\n");

		size_t argidx = 1;

		extra_args(args, argidx, design);

		Module *module = design->top_module();
		if (module == nullptr)
			log_cmd_error("No top module found.\n");
        MaybePreMapAigWithAbc(design, module);
        module = design->top_module();
        if (module == nullptr)
            log_cmd_error("No top module found after ABC pre-mapping.\n");
		log_header(design, "Continuing MapperPass pass.\n");
        LUT6DMapperMain(module);
		log_pop();
    }
} LUT6DMapPass;

PRIVATE_NAMESPACE_END
