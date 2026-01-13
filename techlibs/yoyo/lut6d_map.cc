#include "kernel/celltypes.h"
#include "kernel/consteval.h"
#include "kernel/modtools.h"
#include "kernel/sigtools.h"
#include "kernel/yosys.h"
#include <queue>
#include <ranges>
#include <string.h>

USING_YOSYS_NAMESPACE
using namespace std;
PRIVATE_NAMESPACE_BEGIN

// 全局配置参数
const size_t INF = numeric_limits<size_t>::max();
int LUT_SIZE = 6;              // K-可行性约束：最大输入数量
static int MAX_CUTS_PER_NODE = 64;    // 每个节点保留的最大割集数量（ABC风格：增加到64）
static int MAX_TFO_DEPTH = 10;      // TFO搜索最大深度
static int MAX_TFO_CELLS = 512;    // TFO搜索最大节点数
static int MAX_CANDIDATES_PER_NODE = 32;  // 每个节点保留的最大候选数量（增加到32）
static int NUM_PATTERNS = 128;         // LUT映射中使用的预计算模式数量
static int MAX_CUTS_PER_CAND = INF;
static int MAPPING_ROUNDS = 3;        // ABC风格：多轮迭代优化
static float EPSILON = 0.005f;        // 浮点比较精度

// Cost计算权重参数
const double ALPHA = 1.0;   // 面积权重
const double BETA = 0.5;    // 深度权重
const double GAMMA = 0.3;   // 布线权重

struct Cut;
struct Benefit;
struct pairCandidate;
Cut MergeCuts(const Cut &cut1, const Cut &cut2);
bool is_feasible(Cell* node, Cell* cand, const Cut& node_cut, const Cut& cand_cut);
void EstimateBenefit(Cell* node, Cell* cand, const Cut& node_cut, const Cut& cand_cut, Benefit& benefit);
void CalculateCostDelta(Cell* node, Cell* cand, const Cut& node_cut, const Cut& cand_cut, size_t& cost_delta);
SigBit GetCellOutput(Cell *cell);

SigMap sigmap;
dict<SigBit, Cell *> bit2driver;
dict<SigBit, vector<Cell *>> bit2reader;
dict<SigBit, vector<State>> bit2states;
dict<Cell*, vector<SigBit>> cell2bits;
dict<Cell*, size_t> cell2cost;
dict<Cell*, vector<Cut>> cell2cuts;  // 每个节点的割集列表
dict<Cell*, size_t> cell2depth;  // 节点深度（level）
dict<Cell*, size_t> cell2level;  // 节点在拓扑排序中的层级
dict<Cell*, float> cell2arrival;  // 节点的arrival time
dict<Cell*, float> cell2required;  // 节点的required time
dict<Cell*, Cut> cell2bestcut;
dict<Cell*, pool<Cell *>> cell2mffc;
dict<Cell*, int> cell2index;
static dict<Cell*, int> orig_fanout;
static bool fanout_ready = false;
pool<Cell*> processed_nodes;
pool<Cell*> used_nodes;
pool<SigBit> prime_outputs;
pool<SigBit> prime_inputs;
pool<SigBit> protected_signals;  // 受保护的信号：这些信号的驱动节点不能被删除

// ABC风格：估计引用计数和精确引用计数
dict<Cell*, float> cell2est_refs;     // 估计的fanout引用数
dict<Cell*, int> cell2refs;           // 当前映射中的实际引用数

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
        int idx = 0;
        for (auto bit : leaves) {
            // 使用SigBit的hash值映射到64位
            // 使用wire指针和offset组合生成hash
            size_t h = 0;
            if (bit.wire) {
                h = (size_t)(bit.wire) ^ ((size_t)bit.offset << 16);
            } else {
                h = (size_t)bit.data ^ 0x12345678;
            }
            signature |= (1ULL << (h % 64));
            idx++;
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
        return leaves.size() < other.leaves.size();
    }
    
    // 面积优先的比较（用于Round 2+）
    bool betterAreaThan(const Cut &other, float required_time) const {
        // 必须满足timing约束
        if (arrival_time > required_time + EPSILON) return false;
        if (other.arrival_time > required_time + EPSILON) return true;
        // 在timing约束内，选择area更小的
        if (fabs(area_flow - other.area_flow) > EPSILON)
            return area_flow < other.area_flow;
        return arrival_time < other.arrival_time;
    }
    
    // 检查K-可行性
    bool is_feasible() const {
        return leaves.size() <= LUT_SIZE;
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

struct Benefit {
    size_t depth_reduction;
    size_t area_reduction;
    size_t routing_reduction;

    Benefit() : depth_reduction(0), area_reduction(0), routing_reduction(0) {}

    void setBenefit(size_t dr, size_t ar, size_t rr) {
        depth_reduction += dr;
        area_reduction += ar;
        routing_reduction += rr;
    }

    double totalBenefit() const {
        return ALPHA * area_reduction + BETA * depth_reduction + GAMMA * routing_reduction;
    }

    bool operator>(const Benefit &other) const {
        return totalBenefit() > other.totalBenefit();
    }
};

struct pairCandidate {
    Cell* node;
    Cell* cand;
    Cut cut;
    SigBit I5;  // 改为值类型，避免悬垂指针
    Benefit benefit;
    size_t cost_delta; 
    pool<Cell*> removedNodes;
    bool isZ;
    double score;

    pairCandidate() : node(nullptr), cand(nullptr), I5(State::Sx), benefit(),
                      cost_delta(0), isZ(false), score(0.0) {}

    void setPairCandidate(Cell* n, Cell* c, Benefit b, size_t cd, Cut mcut,
                          SigBit i5 = State::Sx, bool iz = false, double s = 0.0) {
        node = n;
        cand = c;
        benefit = b;
        cost_delta = cd;
        if (mcut.leaves.size() > 0) {
            cut = mcut;
        }
        I5 = i5;  // 直接赋值，不是取地址
        isZ = iz;
        score = s;
    }
};

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
        Cell* lutd = module->addCell(NEW_ID, type);
        // 设置INIT
        lutd->setParam(ID(INIT), Const(INIT, 64));
        
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

        // 移除节点（通过名称查找，避免悬垂指针）
        for (const IdString& cell_name : removedNodes) {
            if (module->cells_.count(cell_name)) {
                module->remove(module->cells_.at(cell_name));
                log_debug("Removed cell: %s\n", cell_name.c_str());
            }
        }
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
        
        // 移除原节点
        for (const IdString& cell_name : removedNodes) {
            if (module->cells_.count(cell_name)) {
                module->remove(module->cells_.at(cell_name));
                log_debug("Removed cell: %s\n", cell_name.c_str());
            }
        }
    }
};

bool IsNOT(Cell *cell) {
    return cell->type == ID($_NOT_);
}
bool IsAND(Cell *cell) {
    return cell->type == ID($_AND_);
}
bool IsCombinationalGate(Cell *cell) {
    return cell->type.in(ID($_AND_), ID($_NOT_));
}

void GetCellInputsSet(Cell *cell, pool<SigBit> &inputs)
{
	log_assert(cell && inputs.empty() && cell2bits.count(cell));
	auto &bits = cell2bits[cell];
	int offset = 1;
	// not support GTP_LUT6D now
	// if(cell is dual output)
	// offset=2
	for (auto it = bits.begin() + offset; it != bits.end(); ++it) {
		inputs.insert(*it);
	}
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

void GetTopoSortedGates(Module *module, vector<Cell *> &gates) {
    gates.clear();
    int i = 0;
    pool<SigBit> prime_inputs;
    pool<SigBit> prime_outputs;
    GetPrimeInputOutput(module, prime_inputs, prime_outputs);
	cell2level.clear();  // 清空level映射
	dict<Cell *, size_t> indegree;
	pool<Cell *> visited;
	queue<Cell *> zero_indegree_nodes;
	for (auto &it : cell2bits) {
		Cell *cell = it.first;
		log_assert(it.second.size() > 0);
		indegree[cell] = it.second.size() - 1; // the first is output bit
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
			zero_indegree_nodes.push(cell);
		}
	}
	// BFS
	while (!zero_indegree_nodes.empty()) {
		Cell *current = zero_indegree_nodes.front();
		zero_indegree_nodes.pop();
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
		for (Cell *neighbor : rds) {
			if (!IsCombinationalGate(current)) {
				continue;
			}
			if (visited.count(neighbor)) {
				log_error("toposort found loop, cell %s reader %s\n", current->name.c_str(), neighbor->name.c_str());
			}
			if (--indegree[neighbor] == 0) // update indegree
			{
				zero_indegree_nodes.push(neighbor);
			}
		}
	}
}

// 检查target是否在source的TFO（Transitive Fanout）中
bool IsInTFO(Cell *source, Cell *target) {
    if (!source || !target) {
        return false;
    }
    
    if (source == target) {
        return true;  // 节点在自己的TFO中
    }
    
    // BFS遍历source的所有fanout
    pool<Cell *> visited;
    queue<Cell *> to_visit;
    to_visit.push(source);
    visited.insert(source);
    
    while (!to_visit.empty()) {
        Cell *current = to_visit.front();
        to_visit.pop();
        
        // 获取当前节点的所有reader（fanout）
        if (!IsCombinationalGate(current)) {
            continue;
        }
        
        pool<Cell *> readers = GetReaders(current, ID(Y));
        for (Cell *reader : readers) {
            if (reader == target) {
                return true;  // 找到target
            }
            
            if (!visited.count(reader) && IsCombinationalGate(reader)) {
                visited.insert(reader);
                to_visit.push(reader);
            }
        }
    }
    
    return false;  // 未找到
}

// 检查target是否在source的TFI（Transitive Fanin）中
bool IsInTFI(Cell *source, Cell *target) {
    return IsInTFO(target, source);
}

static void BuildOrigFanout()
{
    orig_fanout.clear();
    for (auto &kv : cell2bits) {
        Cell *c = kv.first;
        if (!IsCombinationalGate(c)) continue;
        vector<SigBit> inbits; GetCellInputsVector(c, inbits);
        for (auto &b : inbits) {
            Cell *drv = bit2driver.count(b) ? bit2driver[b] : nullptr;
            if (drv && IsCombinationalGate(drv)) orig_fanout[drv]++;
        }
    }
    fanout_ready = true;
}

static void ComputeMFFC(Cell *root, pool<Cell*> &mffc)
{
    mffc.clear();
    if (!root || !IsCombinationalGate(root)) return;
    if (!fanout_ready) BuildOrigFanout();

    // 创建局部fanout计数的副本
    dict<Cell*, int> local_fanout;
    for (auto &kv : orig_fanout) {
        local_fanout[kv.first] = kv.second;
    }

    // 从root开始递归地添加节点到MFFC
    vector<Cell*> stack;
    stack.push_back(root);
    
    while (!stack.empty()) {
        Cell *c = stack.back();
        stack.pop_back();
        
        if (!c || !IsCombinationalGate(c) || mffc.count(c)) 
            continue;
        
        // 将当前节点加入MFFC
        mffc.insert(c);
        
        // 获取当前节点的所有输入
        vector<SigBit> inbits;
        GetCellInputsVector(c, inbits);
        
        for (auto &b : inbits) {
            Cell *drv = bit2driver.count(b) ? bit2driver[b] : nullptr;
            if (!drv || !IsCombinationalGate(drv) || mffc.count(drv)) 
                continue;
            
            // 获取驱动节点的fanout计数
            int fanout = local_fanout.count(drv) ? local_fanout[drv] : 0;
            
            // 只有当驱动节点的fanout为1（只被当前节点使用）时，才加入MFFC
            if (fanout == 1) {
                stack.push_back(drv);
            }
            
            // 递减驱动节点的fanout计数（因为当前节点已加入MFFC）
            if (fanout > 0) {
                local_fanout[drv] = fanout - 1;
            }
        }
    }
}

bool ComputeSharedMFFC(Cell *nodeshallow, Cell *nodedeep, pool<Cell*> &shared_mffc, Cell* reader, pool<Cell*> &visited, size_t max_depth, size_t min_depth) {
    // 优化：限制MFFC搜索，避免过深递归
    if (!IsCombinationalGate(reader)) return false;
    
    // Early termination: 如果visited集合过大，停止搜索
    if (visited.size() > 100) return false;
    
    if (visited.count(reader)) {
        if (shared_mffc.count(reader)) return true;
        // {
        //     log_debug("      visited node in shared mffc: %s\n", reader->name.c_str());
        //     return true;
        // }
        else return false;
        // {
        //     log_debug("      visited node not in shared mffc: %s\n", reader->name.c_str());
        //     return false;
        // }
    }
    visited.insert(reader);
    if (cell2level[reader] > max_depth) return false;
    // {
        // log_debug("      Node beyond max depth: %s\n", reader->name.c_str());
    //     return false;
    // }

    if (nodeshallow == reader || nodedeep == reader) return true;
    // {
    //     log_debug("      Reached root node: %s\n", reader->name.c_str());
    //     return true;
    // }

    if ((cell2level[reader] == max_depth && reader != nodedeep)) return false;
    // {
    //     log_debug("      Shared MFFC stop node: %s\n", reader->name.c_str());
    //     return false;
    // }

    // Only check output bit (index 0), not input bits
    SigBit output_bit = cell2bits[reader][0];
    // log_debug("      Checking output bit of node %s: %s\n", reader->name.c_str(), log_signal(output_bit));
    if (prime_outputs.count(output_bit)) return false;
    // {
    //     log_debug("      Output bit is in PO, stopping: %s\n", log_signal(output_bit));
    //     return false;
    // }

    vector<Cell *> outputs = bit2reader.count(output_bit) ? bit2reader[output_bit] : vector<Cell *>();
    for (Cell *output : outputs) {
        // log_debug("      Checking output node: %s of %s\n", output->name.c_str(), reader->name.c_str());
        if (!ComputeSharedMFFC(nodeshallow, nodedeep, shared_mffc, output, visited, max_depth, min_depth)) {
            // log_debug("      Shared MFFC transfer node: %s\n", reader->name.c_str());
            return false;
        }
    }
    shared_mffc.insert(reader);
    // log_debug("      Shared MFFC node: %s\n", reader->name.c_str());
    return true;
}

void ComputeSharedMFFCEngine(Cell *node1, Cell *node2, pool<Cell*> &shared_mffc, const pool<SigBit> &leaves) {
    pool<Cell*> visited;
    size_t max_depth = max(cell2level[node1], cell2level[node2]);
    size_t min_depth = min(cell2level[node1], cell2level[node2]);
    shared_mffc.clear();
    shared_mffc.insert(node1);
    shared_mffc.insert(node2);
    visited.insert(node1);
    visited.insert(node2);
    Cell* nodeshallow = (cell2level[node1] < cell2level[node2]) ? node1 : node2;
    Cell* nodedeep = (cell2level[node1] < cell2level[node2]) ? node2 : node1;
    // log_debug("    Compute Shared MFFC between %s (depth %zu) and %s (depth %zu)\n",
    //           node1->name.c_str(), cell2level[node1],
    //           node2->name.c_str(), cell2level[node2]);
              
    for (SigBit leaf : leaves) {
        vector<Cell *> readers = bit2reader[leaf];
        for (Cell *reader : readers) {
            if (!IsCombinationalGate(reader)) continue;
            if (visited.count(reader)) continue;
            if (cell2level[reader] > max_depth) continue;

            if (reader == node1 || reader == node2) {
                continue;
            }
            ComputeSharedMFFC(nodeshallow, nodedeep, shared_mffc, reader, visited, max_depth, min_depth);
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
	} else {
		log_error("unhandled cell %s \n", cell->type.c_str());
	}
	return State::Sx;
}

void partialSimulator(const pool<SigBit> &seed_inputs) {
    for (SigBit bit : seed_inputs) {
        vector<State> states;
        states.reserve(NUM_PATTERNS);
        for (int i = 0; i < NUM_PATTERNS; i++) {
            State random_state = (rand() % 2 == 0) ? State::S0 : State::S1;
            states.push_back(random_state);
        }
        bit2states[bit] = states;
    }
}

void GenerateBitState(Module *module, const pool<SigBit> &prime_inputs, bool incremental, vector<Cell *> gates) {
    // Only clear if not incremental update
    if (!incremental) {
        bit2states.clear();
    }

    partialSimulator(prime_inputs);
	
	// 得到部分真值表

	for (Cell *cell : gates) {
		if (!IsCombinationalGate(cell)) {
			continue;
		}
		
		SigBit output = GetCellOutput(cell);
        vector<State> output_states;
        output_states.reserve(NUM_PATTERNS);

		pool<SigBit> inputs;
		GetCellInputsSet(cell, inputs);
		
		// int num_of_zero = 0;
		// int num_of_one = 0;

        for (int i = 0; i < NUM_PATTERNS; i++) {
			dict<SigBit, State> bit_map;
			
			for (SigBit input_bit : inputs) {
				if (bit2states.count(input_bit) && bit2states[input_bit].size() > (size_t)i) {
					bit_map[input_bit] = bit2states[input_bit][i];
				}
			}
			
            State output_state = StateEval(bit_map, output);
			output_states.push_back(output_state);
			
			// if (output_state == State::S0) {
			// 	num_of_zero++;
			// } else if (output_state == State::S1) {
			// 	num_of_one++;
			// }
		}
		
		// 检查0和1的数量是否都大于5
		// if (num_of_zero > 5 && num_of_one > 5) {
		// 	log("Cell %s output has balanced states: %d zeros, %d ones\n", 
		// 		cell->name.c_str(), num_of_zero, num_of_one);
		// }
		// else {
		// 	log("Cell %s output has unbalanced states: %d zeros, %d ones\n", 
		// 		cell->name.c_str(), num_of_zero, num_of_one);
		// }
        bit2states[output] = output_states;
        
        string sig_name = log_signal(output);
	}
}

// 找到TFI中包含Cut叶子的节点
// 含义：这些节点的输入依赖于cut的某些叶子（但可能还依赖其他信号）
// max_depth: 最大搜索深度（-1表示无限制）
// 返回：所有在TFI中且使用了cut叶子的节点集合
pool<Cell*> FindNodesUsingCutLeaves(const Cut& cut, Cell* node, int max_depth=MAX_TFO_DEPTH) {
    pool<Cell*> tfo_nodes;
    pool<Cell*> visited;

    // BFS队列：pair<信号bit, 当前深度>
    queue<pair<SigBit, int>> to_visit;
    
    // 从cut的所有叶子开始搜索
    for (SigBit leaf : cut.leaves) {
        to_visit.push({leaf, 0});
    }
    
    while (!to_visit.empty()) {
        auto [current_bit, depth] = to_visit.front();
        to_visit.pop();
        
        // 检查深度限制
        if (max_depth >= 0 && depth >= max_depth) {
            continue;
        }
        
        // 找到所有使用这个信号的节点（readers）
        if (!bit2reader.count(current_bit)) continue;
        
        for (Cell* reader : bit2reader[current_bit]) {
            if (!IsCombinationalGate(reader)) continue;
            if (!cut.internal.count(reader) && !cell2mffc[reader].count(node)) {
                    tfo_nodes.insert(reader);
            }
            // 如果未访问过，继续向前搜索（沿着fanout方向）
            if (!visited.count(reader)) {
                visited.insert(reader);
                // 获取reader的输出，继续搜索
                SigBit output_bit = sigmap(reader->getPort(ID(Y)))[0];
                to_visit.push({output_bit, depth + 1});
            }
        }
        if (tfo_nodes.size() >= MAX_TFO_CELLS) {
            // log_warning("TFI search reached maximum cell limit (%d), stopping early.\n", MAX_TFO_CELLS);
            break;  // 达到最大节点数量，提前停止
        }
    }
    
    return tfo_nodes;
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

size_t CalculateCutCost(const pool<SigBit> &cut)
{
    // 计算该选项的代价 = cut的cost + 输入节点的最优cost
	size_t cost = 1;  // 当前LUT的代价
	for (auto bit : cut) {
		Cell *driver = bit2driver.count(bit) ? bit2driver[bit] : nullptr;
		if (driver && cell2cost.count(driver)) {
			cost += cell2cost[driver];
		}
	}
    if (cost < 1) {
        log_error("Calculated cut cost is less than 1\n");
    }
	return cost;
}

// ABC风格：计算cut的area flow
// area_flow = LUT_area + Σ(fanin_cut.area_flow / fanin_refs)
double ComputeAreaFlowABC(const pool<SigBit>& leaves) {
    double flow = 1.0;  // 当前LUT的面积
    
    for (auto leaf_bit : leaves) {
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
        if (cell2est_refs.count(leaf)) {
            refs = max(1.0f, cell2est_refs[leaf]);
        } else if (orig_fanout.count(leaf)) {
            refs = max(1.0f, (float)orig_fanout[leaf]);
        }
        
        flow += leaf_flow / refs;
    }
    return flow;
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
    vector<Cut> result;
    
    // 对于每个割集，检查是否被其他割集支配
    for (size_t i = 0; i < cuts.size(); i++) {
        bool dominated = false;
        for (size_t j = 0; j < cuts.size(); j++) {
            if (i != j && cuts[i].is_dominated_by(cuts[j])) {
                dominated = true;
                break;
            }
        }
        if (!dominated) {
            result.push_back(cuts[i]);
        }
    }
    
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
                new_cut.arrival_time = 0.0f;
                new_cut.area_flow = ComputeAreaFlowABC(new_cut.leaves);
                // signature不变（leaves相同）
                if (new_cut.is_feasible()) {
                    cuts.push_back(new_cut);
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
            cuts.push_back(pi_cut);
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
        
        vector<Cut> cuts1, cuts2;
        
        // 获取第一个输入的割集
        if (cell1 && IsCombinationalGate(cell1) && cell2cuts.count(cell1)) {
            cuts1 = cell2cuts[cell1];
        } else {
            Cut pi_cut;
            pi_cut.leaves.insert(input1);
            pi_cut.depth = 0;
            pi_cut.area_flow = 0.0;
            pi_cut.computeSignature();
            cuts1.push_back(pi_cut);
        }
        
        // 获取第二个输入的割集
        if (cell2 && IsCombinationalGate(cell2) && cell2cuts.count(cell2)) {
            cuts2 = cell2cuts[cell2];
        } else {
            Cut pi_cut;
            pi_cut.leaves.insert(input2);
            pi_cut.depth = 0;
            pi_cut.area_flow = 0.0;
            pi_cut.computeSignature();
            cuts2.push_back(pi_cut);
        }
        
        // 笛卡尔积合并（使用signature快速过滤）
        for (auto &c1 : cuts1) {
            for (auto &c2 : cuts2) {
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
                
                // 检查是否与已有的cut重复（通过signature快速检查）
                bool is_duplicate = false;
                for (auto &existing_cut : cuts) {
                    if (existing_cut.signature == merged.signature && 
                        existing_cut.leaves.size() == merged.leaves.size()) {
                        bool same = true;
                        for (auto bit : merged.leaves) {
                            if (!existing_cut.leaves.count(bit)) {
                                same = false;
                                break;
                            }
                        }
                        if (same) {
                            is_duplicate = true;
                            break;
                        }
                    }
                }
                
                if (!is_duplicate) {
                    cuts.push_back(merged);
                }
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
    
    // 6. N-Cut剪枝
    if (cuts.size() > MAX_CUTS_PER_NODE) {
        cuts.resize(MAX_CUTS_PER_NODE);
    }
    
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

bool is_feasible(Cell* node, Cell* cand, const Cut& node_cut, const Cut& cand_cut) {
    
    // 约束1：合并后的割集大小不超过K
    Cut merged_Cut;
    merged_Cut.leaves = node_cut.leaves;
    for (auto bit : cand_cut.leaves) {
        merged_Cut.leaves.insert(bit);
    }
    if (!merged_Cut.is_feasible()) {
        return false;
    }

    // 约束2：避免环路 - 检查TFO/TFI关系
    // 如果cand在node的TFO中，或node在cand的TFO中，则会形成环路
    if (IsInTFO(node, cand) || IsInTFO(cand, node)) {
        return false;
    }
    
    // 约束3：检查内部节点关系（双重保险）
    if (cand_cut.internal.count(node) || node_cut.internal.count(cand)) {
        return false;
    }
    
    return true;
}

void EstimateBenefit(Cell* node, Cell* cand, const Cut& node_cut, const Cut& cand_cut, Benefit& benefit) {
    // 1. 深度收益：合并前后的深度差异
    size_t current_depth = max(node_cut.depth, cand_cut.depth);
    
    // 计算合并后的深度：所有输入leaf的最大深度 + 1
    Cut merged_cut = MergeCuts(node_cut, cand_cut);
    size_t merged_depth = 0;
    for (auto bit : merged_cut.leaves) {
        Cell *driver = bit2driver.count(bit) ? bit2driver[bit] : nullptr;
        if (driver && cell2depth.count(driver)) {
            merged_depth = max(merged_depth, cell2depth[driver]);
        }
    }
    merged_depth += 1;  // LUT6D本身的深度
    
    benefit.depth_reduction = (current_depth > merged_depth) ? (current_depth - merged_depth) : 0;
    
    // 2. 面积收益：计算共享MFFC的大小
    // 共享MFFC包括：node的MFFC + cand的MFFC + 只扇出到这两个节点的中间节点
    pool<Cell*> shared_mffc;
    ComputeSharedMFFCEngine(node, cand, shared_mffc, merged_cut.leaves);
    benefit.area_reduction = shared_mffc.size();
    
    // log_debug("    Shared MFFC for (%s, %s): %zu nodes can be removed\n",
    //           node->name.c_str(), cand->name.c_str(), shared_mffc.size());
    
    // 3. 布线收益：计算减少的连线数
    // 原本node和cand各自的输出可能需要布线，合并后只需要LUT6D的两个输出
    // 计算共享的输入数量
    size_t shared_inputs = 0;
    for (auto bit : node_cut.leaves) {
        if (cand_cut.leaves.count(bit)) {
            shared_inputs++;
        }
    }
    
    // 布线收益 = 独立时总输入数 - 合并后输入数
    size_t separate_inputs = node_cut.leaves.size() + cand_cut.leaves.size();
    size_t merged_inputs = merged_cut.leaves.size();
    benefit.routing_reduction = (separate_inputs > merged_inputs) ? (separate_inputs - merged_inputs) : 0;
    
    // 或者用共享输入数作为布线收益
    // benefit.routing_reduction = shared_inputs;
    
    return;
}

void CalculateCostDelta(Cell* node, Cell* cand, const Cut& node_cut, const Cut& cand_cut, size_t& cost_delta) {
    // 计算单个节点的cost
    auto cost_of_node = [](Cell* cell, const Cut& cut) -> double {
        double area_cost = ALPHA * 1.0;  // 1个LUT的面积
        double depth_cost = BETA * cut.depth;  // 深度贡献
        
        // 布线成本：输入连线数
        double wire_cost = GAMMA * cut.leaves.size();
        
        return area_cost + depth_cost + wire_cost;
    };
    
    // 计算LUT6D的cost
    auto cost_of_lut6d = [](const Cut& merged_cut) -> double {
        double area_cost = ALPHA * 1.2;  // 1个LUT6D（比单LUT稍大）
        double depth_cost = BETA * merged_cut.depth;  // 合并后的深度
        
        // 布线成本：合并后的输入连线数（共享输入减少了布线）
        double wire_cost = GAMMA * merged_cut.leaves.size();
        
        return area_cost + depth_cost + wire_cost;
    };
    
    // 计算当前成本（两个独立节点）
    double current_cost = cost_of_node(node, node_cut) + cost_of_node(cand, cand_cut);
    
    // 计算合并后的成本
    Cut merged_cut = MergeCuts(node_cut, cand_cut);
    
    // 重新计算合并后的深度
    size_t merged_depth = 0;
    for (auto bit : merged_cut.leaves) {
        Cell *driver = bit2driver.count(bit) ? bit2driver[bit] : nullptr;
        if (driver && cell2depth.count(driver)) {
            merged_depth = max(merged_depth, cell2depth[driver]);
        }
    }
    merged_depth += 1;  // LUT6D本身的深度
    merged_cut.depth = merged_depth;
    
    double merged_cost = cost_of_lut6d(merged_cut);
    
    // cost_delta：负值表示减少（收益），正值表示增加（损失）
    double delta = merged_cost - current_cost;
    
    // 转换为size_t（如果是收益则存储为绝对值）
    if (delta < 0) {
        cost_delta = static_cast<size_t>(-delta * 100);  // 乘100保留精度
    } else {
        cost_delta = 0;  // 如果没有收益，设为0
    }
    
    return;
}

vector<SigBit> CombinablePreCheck(Cell* node, Cell* cand_node, const Cut& node_cut, const Cut& cand_cut, const pool<SigBit>& potential_I5) {
    // Early exit：如果没有候选I5信号，直接失败
    if (potential_I5.empty()) {
        return {};
    }
    
    // 优化1：缓存输出bit，避免每次循环都调用GetCellOutput
    SigBit node_out_bit = GetCellOutput(node);
    SigBit cand_out_bit = GetCellOutput(cand_node);
    
    // 优化2：提前检查bit2states是否存在，避免循环内重复检查
    if (!bit2states.count(node_out_bit) || !bit2states.count(cand_out_bit)) {
        return {};
    }
    
    const vector<State>& node_states = bit2states[node_out_bit];
    const vector<State>& cand_states = bit2states[cand_out_bit];
    
    // 检查vector大小是否足够
    if (node_states.size() < NUM_PATTERNS || cand_states.size() < NUM_PATTERNS) {
        return {};
    }
    
    vector<SigBit> candidate_I5_vec;
    candidate_I5_vec.reserve(potential_I5.size());
    for (auto bit : potential_I5) {
        candidate_I5_vec.push_back(bit);
    }

    // LUT6D语义：
    // - Z5 = f(I0-I4, I5=0)  -- 使用低32位INIT
    // - Z  = f(I0-I4, I5=1)  -- 使用高32位INIT（I5=1时）
    // 
    // 配对条件：
    // - 当I5=0时，LUT6D输出Z5应该等于node_output（或cand_output，取决于isZ标志）
    // - 当I5=1时，LUT6D输出Z应该等于另一个输出
    // 
    // 简化检查：对于每个pattern，如果两个输出不同，则I5必须能区分它们
    // 即：I5=0 → 选择一个输出，I5=1 → 选择另一个输出
    
    for (size_t i = 0; i < NUM_PATTERNS; i++) {
        State node_output = node_states[i];
        State cand_output = cand_states[i];
        
        // 只在输出不同时才需要I5来区分
        if (node_output != cand_output) {
            // 需要找一个I5信号，使得当输出不同时I5的值也能区分
            // 这里的逻辑：假设Z5对应cand（I5=0），Z对应node（I5=1）
            // 所以当node!=cand时，需要I5=1来选择node的输出
            candidate_I5_vec.erase(
                std::remove_if(candidate_I5_vec.begin(), candidate_I5_vec.end(),
                    [i](const SigBit& bit) {
                        if (!bit2states.count(bit)) return true;
                        const vector<State>& states = bit2states[bit];
                        if (states.size() <= i) return true;  // 越界保护
                        return states[i] == State::S0;  // I5必须为1来区分
                    }),
                candidate_I5_vec.end()
            );
            
            // Early exit：如果候选集为空，直接返回失败
            if (candidate_I5_vec.empty()) {
                // log_debug("    Combinable pre-check failed at pattern %d\n", i);
                return {};
            }
        }
        // 注意：当node_output == cand_output时，I5的值无所谓，两个输出相同
    }
    
    return candidate_I5_vec;
}

// 检查cut的leaves之间是否存在依赖关系
// 返回true表示存在依赖（不合法），false表示独立（合法）
bool HasDependencyInCutLeaves(const Cut& merged_cut) {
    // 将leaves转换为set以便快速查找
    pool<SigBit> leaves_set(merged_cut.leaves.begin(), merged_cut.leaves.end());
    
    // 对于每个leaf，使用BFS检查它的TFI（transitive fanin）是否包含其他leaf
    for (auto leaf : merged_cut.leaves) {
        Cell* driver = bit2driver.count(leaf) ? bit2driver[leaf] : nullptr;
        if (!driver || !IsCombinationalGate(driver)) continue;
        
        // 使用BFS遍历leaf的TFI（向后追溯）
        pool<Cell*> visited;
        vector<Cell*> queue;
        queue.push_back(driver);
        visited.insert(driver);
        
        while (!queue.empty()) {
            Cell* current = queue.back();
            queue.pop_back();
            
            // 获取当前节点的输入
            vector<SigBit> inputs;
            GetCellInputsVector(current, inputs);
            
            for (auto input : inputs) {
                // 检查input是否是cut的其他leaf
                if (leaves_set.count(input) && input != leaf) {
                    // log_debug("      Dependency detected: %s transitively depends on %s\n",
                    //          log_signal(leaf), log_signal(input));
                    return true;
                }
                
                // 继续向上追溯input的驱动
                Cell* input_driver = bit2driver.count(input) ? bit2driver[input] : nullptr;
                if (input_driver && IsCombinationalGate(input_driver) && !visited.count(input_driver)) {
                    visited.insert(input_driver);
                    queue.push_back(input_driver);
                }
            }
        }
    }
    
    return false;
}

bool SATVerification(Cell* node, Cell* cand_node, const Cut& node_cut, const Cut& cand_cut, pool<SigBit>& potential_I5) {
    vector<SigBit> candidate_I5_vec = CombinablePreCheck(node, cand_node, node_cut, cand_cut, potential_I5);
    if (candidate_I5_vec.empty()) return false;

    Cut merged = MergeCuts(cand_cut, node_cut);
    vector<SigBit> leaves;
    leaves.reserve(merged.leaves.size());  // 优化：预分配内存
    for (auto bit : merged.leaves) leaves.push_back(bit);
    size_t bits_num = size_t(1 << leaves.size());
    
    // 优化：限制SAT验证的pattern数量，如果leaves过多则跳过
    if (leaves.size() > LUT_SIZE) return false;
    
    // 优化：缓存节点输出，避免重复查找
    SigBit node_output = GetCellOutput(node);
    SigBit cand_output = GetCellOutput(cand_node);
    
    for (size_t i = 0; i < bits_num; i++) {
        dict<SigBit, State> bit_map;
        bit_map.reserve(leaves.size() + 10);  // 预分配空间
        for (size_t n = 0; n < leaves.size(); n++) {
            bit_map[leaves[n]] = ((i & (1 << n)) != 0) ? State::S1 : State::S0;
        }

        State node_out = StateEval(bit_map, node_output);
        State cand_out = StateEval(bit_map, cand_output);
        if (node_out != cand_out) {
            // log_debug("  SAT Verification mismatch found: node_out=%d, cand_out=%d\n",
            //           static_cast<int>(node_out), static_cast<int>(cand_out));
            // log_debug("    Current assignment:\n");
            // for (auto& [bit, state] : bit_map) {
            //     log_debug("      %s = %d\n", log_signal(bit), static_cast<int>(state));
            // }
            
            // 先收集需要保留的I5候选（避免在迭代中修改vector）
            vector<SigBit> remaining_candidates;
            for (auto bit : candidate_I5_vec) {
                // log_debug("    SAT Verification pattern %zu: node_out=%d, cand_out=%d, checking I5 %s\n",
                //           i, static_cast<int>(node_out), static_cast<int>(cand_out), log_signal(bit));
                State i5_state = bit_map[bit];
                // 只保留i5_state不为S0的候选
                if (i5_state != State::S0) {
                    remaining_candidates.push_back(bit);
                }
            }
            
            // 更新候选列表
            candidate_I5_vec = remaining_candidates;
            
            if (candidate_I5_vec.empty()) {
                return false;
            }
        }
    }
    potential_I5.clear();
    for (auto bit : candidate_I5_vec) {
        potential_I5.insert(bit);
    }
    return true;
}

// 检查是否满足Z5=Z在I5=0时的cofactor关系
bool RoleValid(Cell* z_node, Cell* z5_node, SigBit i5, const Cut& merged_cut) {
    if (!z_node || !z5_node) return false;
    if (i5.wire == nullptr) return true;
    if (!merged_cut.leaves.count(i5)) return true;

    vector<SigBit> other_leaves;
    other_leaves.reserve(merged_cut.leaves.size());
    for (auto bit : merged_cut.leaves) {
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

bool SelectI5AndRole(Cell* node, Cell* cand_node, const Cut& merged_cut,
                     const pool<SigBit>& potential_I5, SigBit& chosen_i5, bool& isZ) {
    for (auto bit : potential_I5) {
        if (RoleValid(node, cand_node, bit, merged_cut)) {
            chosen_i5 = bit;
            isZ = false;  // node -> Z, cand -> Z5
            return true;
        }
        if (RoleValid(cand_node, node, bit, merged_cut)) {
            chosen_i5 = bit;
            isZ = true;   // cand -> Z, node -> Z5
            return true;
        }
    }
    return false;
}

// 找到每个node的cut的候选合并节点
// node_slack: 节点的slack值，用于决定允许的cut深度范围
void FindCandidates4Cut(Cell* node, const Cut& node_cut, float node_slack, vector<pairCandidate>& candidates, bool mandatoryCut6) {
    pool<Cell*> tfo_nodes = FindNodesUsingCutLeaves(node_cut, node, MAX_TFO_DEPTH);
    
    // 扩展搜索：也考虑与node_cut共享输入的其他节点（不一定在TFO中）
    // 这可以找到独立逻辑锥中但共享输入的配对机会
    for (SigBit leaf : node_cut.leaves) {
        if (bit2reader.count(leaf)) {
            for (Cell* reader : bit2reader[leaf]) {
                if (IsCombinationalGate(reader) && reader != node) {
                    tfo_nodes.insert(reader);
                }
            }
        }
    }
    
    // 使用priority_queue保持最优的候选，避免评估所有候选
    auto cmp = [](const pairCandidate& a, const pairCandidate& b) {
        return a.score < b.score;
    };
    priority_queue<pairCandidate, vector<pairCandidate>, decltype(cmp)> top_candidates(cmp);
    
    int candidates_evaluated = 0;
    for (Cell* cand_node : tfo_nodes) {
        if (node_cut.internal.count(cand_node)) continue;
        if (!cell2cuts.count(cand_node)) continue;
        if (!cell2index.count(cand_node) || !cell2index.count(node)) continue;
        if (processed_nodes.count(cand_node)) continue;
        // if (cell2index[cand_node] <= cell2index[node]) continue;
        
        candidates_evaluated++;
        
        // 根据node的slack决定允许的cut深度范围（Pango启发）
        size_t min_cand_depth = INF;
        for (auto& cut : cell2cuts[cand_node]) {
            if (cut.depth < min_cand_depth) {
                min_cand_depth = cut.depth;
            }
        }
        
        size_t max_allowed_depth = INF;
        if (node_slack < 0.1f) {
            // 严格关键路径：只使用最浅的cut
            max_allowed_depth = min_cand_depth;
            // log_debug("    Cand %s: critical path (slack=%.2f), only depth=%zu allowed\n",
            //           cand_node->name.c_str(), node_slack, max_allowed_depth);
        } else if (node_slack < 0.5f) {
            // 接近关键路径：允许depth <= min_depth + 1
            max_allowed_depth = min_cand_depth + 1;
            // log_debug("    Cand %s: near-critical (slack=%.2f), depth<=%zu allowed\n",
            //           cand_node->name.c_str(), node_slack, max_allowed_depth);
        }
        
        // 找到真正最优的cand_cut（基于收益和成本，并受slack约束）
        Cut* best_cand_cut = nullptr;
        Benefit best_benefit;
        size_t best_cost_delta = 0;
        pool<SigBit> best_potential_I5;
        double best_score = -1.0;  // 综合评分：benefit - cost_penalty
        
        // 评估所有可行的cand_cut（受slack约束）
        int cuts_evaluated = 0;
        const vector<Cut>& cand_cuts = cell2cuts[cand_node];  // 优化：避免重复查找
        for (auto it = cand_cuts.rbegin(); it != cand_cuts.rend(); ++it) {
            const Cut& cand_cut = *it;
            // 根据slack过滤cut深度
            if (cand_cut.depth > max_allowed_depth) {
                continue;
            }
            
            // 优化：Early termination - 如果已经评估了足够多的cuts
            if (cuts_evaluated > MAX_CUTS_PER_CAND && best_score > 0) break;
            bool contains_processed = false;
            for (auto bit : cand_cut.leaves) {
                if (processed_nodes.count(bit2driver.count(bit) ? bit2driver[bit] : nullptr)) {
                    contains_processed = true;
                    // log_debug("    Skipping candidate cut: contains processed node %s\n",
                    //           bit2driver.count(bit) ? bit2driver[bit]->name.c_str() : "N/A");
                    break;
                }
            }
            if (contains_processed) continue;
            size_t node_leaves_size = node_cut.leaves.size();
            size_t cand_leaves_size = cand_cut.leaves.size();
            // Quick feasibility check: 合并后叶子数量不能超过LUT_SIZE
            size_t estimated_merged_leaves = node_leaves_size + cand_leaves_size;
            if (estimated_merged_leaves > LUT_SIZE * 2) continue;
            

            // log_debug("  Evaluating candidate cut for node %s with %zu leaves (cut %d/%zu)\n",
            //           cand_node->name.c_str(), cand_cut.leaves.size(), 
            //           cuts_evaluated, cand_cuts.size());
            // 计算共享的输入数量
            size_t shared = 0;
            pool<SigBit> potential_I5;
            potential_I5.reserve(LUT_SIZE);  // 优化：预分配空间
            // 预先计算集合大小,避免重复计算

            if (node_leaves_size == LUT_SIZE && cand_leaves_size == LUT_SIZE) {
                continue;
            }

            // 选择较小的集合进行遍历
            if (node_leaves_size <= cand_leaves_size) {
                for (auto bit : node_cut.leaves) {
                    if (cand_cut.leaves.count(bit)) {
                        shared++;
                    } else {
                        potential_I5.insert(bit);
                    }
                }
                // 只需遍历cand中不在node中的元素
                for (auto bit : cand_cut.leaves) {
                    if (!node_cut.leaves.count(bit)) {
                        potential_I5.insert(bit);
                    }
                }
            } else {
                for (auto bit : cand_cut.leaves) {
                    if (node_cut.leaves.count(bit)) {
                        shared++;
                    } else {
                        potential_I5.insert(bit);
                    }
                }
                for (auto bit : node_cut.leaves) {
                    if (!cand_cut.leaves.count(bit)) {
                        potential_I5.insert(bit);
                    }
                }
            }
            
            // 允许shared=0的情况，两个完全独立的逻辑锥也可以配对
            // if (!shared) continue;

            // K-可行性快速检查
            size_t total_inputs = node_leaves_size + cand_leaves_size - shared;
            if (potential_I5.empty() && total_inputs == LUT_SIZE) continue;
            if (total_inputs > LUT_SIZE) {
                // log_debug("    Skipping candidate cut: merged inputs %zu exceed K=%d\n",
                //           total_inputs, LUT_SIZE);
                continue;  // 合并后超过K
            }
            // 允许 total_inputs < LUT_SIZE 的情况，这样可以配对更多节点
            
            // 检查merged cut的leaves之间是否存在依赖关系
            Cut merged_cut_temp = MergeCuts(node_cut, cand_cut);
            if (HasDependencyInCutLeaves(merged_cut_temp)) {
                // log_debug("    Skipping candidate: dependency detected in merged cut leaves\n");
                continue;  // 存在依赖关系，跳过此候选
            }
            cuts_evaluated++;
            // 优化：只在真正需要时才做SAT验证（最耗时的操作）
            // 先用轻量级检查过滤掉明显不可行的候选
            if (total_inputs == LUT_SIZE) {
                // 快速检查：如果potential_I5太少，直接跳过SAT验证
                if (potential_I5.size() < 1) continue;
                if (!SATVerification(node, cand_node, node_cut, cand_cut, potential_I5)) continue;
            }
            else if (total_inputs < LUT_SIZE) {
                // 输入少于6个时，选择一个非共享信号作为I5（区分两个输出）
                // 或者使用常量1作为占位符
                // if (!potential_I5.empty()) {
                //     // 有非共享信号，可以用作I5来区分两个输出
                //     // 需要验证是否真的可以区分
                //     if (!SATVerification(node, cand_node, node_cut, cand_cut, potential_I5)) {
                //         potential_I5.clear();
                //         potential_I5.insert(State::S1);  // fallback到常量
                //     }
                // } else {
                    potential_I5.insert(State::S1);  // 无需区分，用常量
                // }
            } else continue;
            
            log_debug("    Node %s and Cand %s have %zu shared inputs, potential I5 count=%zu\n",
                      node->name.c_str(), cand_node->name.c_str(), shared, potential_I5.size());
            log_debug("      Potential I5 signals: ");
            for (auto bit : potential_I5) {
                log_debug("%s ", log_signal(bit));
            }
            log_debug("\n");
            log_debug("      Cand cut leaves: ");
            for (auto bit : cand_cut.leaves) {
                log_debug("%s ", log_signal(bit));
            }
            log_debug("\n");
            log_debug("      Node cut leaves: ");
            for (auto bit : node_cut.leaves) {  
                log_debug("%s ", log_signal(bit));
            }
            log_debug("\n");
            // 计算这个cut的收益和成本
            Benefit benefit;
            EstimateBenefit(node, cand_node, node_cut, cand_cut, benefit);
            
            size_t cost_delta = 0;
            CalculateCostDelta(node, cand_node, node_cut, cand_cut, cost_delta);
            
            // 计算综合评分：收益越高越好，成本增加越少越好
            // cost_delta越大表示收益越大（因为在CalculateCostDelta中已经转换）
            double score = benefit.totalBenefit() + (cost_delta / 100.0);
            
            // 更新最优cut
            if (score > best_score) {
                best_score = score;
                best_cand_cut = const_cast<Cut*>(&cand_cut);
                best_benefit = benefit;
                best_cost_delta = cost_delta;
                best_potential_I5 = potential_I5;
            }
        }
        
        // 如果找到了最优cut
        if (best_cand_cut && best_score > 0) {
            pairCandidate pc;
            SigBit I5 = State::Sx;
            bool isZ = false;
            Cut merged = MergeCuts(node_cut, *best_cand_cut);
            if (!SelectI5AndRole(node, cand_node, merged, best_potential_I5, I5, isZ)) {
                continue;
            }
            pc.setPairCandidate(node, cand_node, best_benefit, best_cost_delta,
                                merged, I5, isZ, best_score);
            top_candidates.push(pc);
            
            // 限制priority_queue大小，保持最优的MAX_CANDIDATES_PER_NODE个
            if (top_candidates.size() > MAX_CANDIDATES_PER_NODE * 2) {
                // 创建临时vector并重建priority_queue
                vector<pairCandidate> temp;
                for (int i = 0; i < MAX_CANDIDATES_PER_NODE && !top_candidates.empty(); i++) {
                    temp.push_back(top_candidates.top());
                    top_candidates.pop();
                }
                // 清空并重新填充
                while (!top_candidates.empty()) top_candidates.pop();
                for (auto& pc : temp) top_candidates.push(pc);
            }

            // log_debug("    I5: %s\n", log_signal(I5));
            
            log_debug("  Added candidate %s for node %s with score=%.2f, I5=%s, isZ=%d (queue size=%zu)\n",
                      cand_node->name.c_str(), node->name.c_str(), 
                      best_score, log_signal(I5), isZ, top_candidates.size());
        }
    }
    
    // 从priority_queue提取最优的MAX_CANDIDATES_PER_NODE个候选
    while (!top_candidates.empty() && candidates.size() < MAX_CANDIDATES_PER_NODE) {
        candidates.push_back(top_candidates.top());
        top_candidates.pop();
    }
}

void EnumerateCandidates(Cell* node, float node_slack, vector<pairCandidate>& pair_candidates, size_t minCutSize=0, bool mandatoryCut6=false) {
    for (auto& node_cut : cell2cuts[node]) {
        if (node_cut.leaves.size() < minCutSize) {
            continue;  // 只考虑有6个叶子的cut
        }
        // log_debug("  Using cut with %zu leaves: ", node_cut.leaves.size());
        bool contains_processed = false;
        for (auto bit : node_cut.leaves) {
            if (processed_nodes.count(bit2driver.count(bit) ? bit2driver[bit] : nullptr)) {
                // log_debug("\n Skipping candidate cut: contains processed node %s\n",
                //           bit2driver.count(bit) ? bit2driver[bit]->name.c_str() : "N/A");
                contains_processed = true;
                break;
            }
            // log_debug("%s ", log_signal(bit));
        }
        // log_debug("\n");
        if (contains_processed) {
            // log_debug(" - Skipping cut due to processed leaves\n");
            continue;
        }
        // log_debug("\n");
        vector<pairCandidate> node_candidates;
        FindCandidates4Cut(node, node_cut, node_slack, node_candidates, mandatoryCut6);
        
        // 合并到node的候选列表
        pair_candidates.insert(pair_candidates.end(), 
                                node_candidates.begin(), 
                                node_candidates.end());
        // log_debug("    Found %zu candidates for node %s using this cut\n",
        //           node_candidates.size(), node->name.c_str());
    }
}

void CalculateLUT6DINIT(Cell* node, pairCandidate candidate, LUT6DInfo& lut6d_info) {
    // 获取node和candidate的输出信号（直接存储SigBit）
    if (candidate.isZ) {
        lut6d_info.Z5 = GetCellOutput(node);
        lut6d_info.Z = GetCellOutput(candidate.cand);
    }
    else {
        lut6d_info.Z = GetCellOutput(node);
        lut6d_info.Z5 = GetCellOutput(candidate.cand);
    }
    
    vector<SigBit> leaves;
    uint64_t INIT = 0;
    
    // 获取合并后的cut的叶子节点（排除I5）
    Cut cut = candidate.cut;
    for (auto bit : cut.leaves) {
        if (bit != candidate.I5) leaves.push_back(bit);
    }

    // 填充虚拟输入，确保总共有5个输入（排除I5后）
    while (leaves.size() < LUT_SIZE - 1) {
        leaves.push_back(State::S0);
    }
    
    // 最后添加I5作为第6个输入
    SigBit I5 = candidate.I5;
    leaves.push_back(candidate.I5);
    lut6d_info.inputs = leaves;

    // log_debug("  BitMap for LUT6DINIT calculation:\n");
    // for (size_t idx = 0; idx < leaves.size(); idx++) {
    //     log_debug("    I%d: %s\n", idx, log_signal(leaves[idx]));
    // }

    size_t num_leaves = leaves.size();
    size_t lut_size = size_t(1 << num_leaves);
    
    // 初始化LUTINIT为0
    
    // 枚举所有输入组合，计算输出
    // LUT6D的INIT语义：
    // - Z  = INIT[{I5,I4,I3,I2,I1,I0}]  （完整6输入查表）
    // - Z5 = INIT[{0,I4,I3,I2,I1,I0}]   （I5固定为0，只看低32位）
    // 
    // 所以我们需要：
    // - 当I5=0时（i < 32），INIT[i]应该使Z5输出正确的值
    // - 当I5=1时（i >= 32），INIT[i]应该使Z输出正确的值
    for (size_t i = 0; i < lut_size; i++) {
        dict<SigBit, State> bit_map;
        for (size_t n = 0; n < num_leaves; n++) {
            State val = ((i & (1 << n)) != 0) ? State::S1 : State::S0;
            if (leaves[n].wire == nullptr) {
                val = leaves[n].data;
            }
            if (!bit_map.count(leaves[n])) {
                bit_map[leaves[n]] = val;
            }
        }
        
        // 判断I5的值（I5在leaves[5]位置，对应i的第5位）
        bool i5_is_one = (i & 32) != 0;
        
        State lut_output;
        if (i5_is_one) {
            // 当I5=1时，这个INIT条目影响Z的输出
            // 使用当前bit_map计算Z应该输出什么
            lut_output = StateEval(bit_map, lut6d_info.Z);
        } else {
            // 当I5=0时，这个INIT条目影响Z5的输出
            // 使用当前bit_map计算Z5应该输出什么
            lut_output = StateEval(bit_map, lut6d_info.Z5);
        }
        
        // 设置LUTINIT的相应位
        if (lut_output == State::S1) {
            INIT |= (uint64_t(1) << i);
        }
    }
    log_debug("  Calculated LUT6DINIT: 0x%016llX\n", INIT);
    lut6d_info.INIT = INIT;
    return;
}

bool VerifyLUT6DMapping(const LUT6DInfo& lut6d_info, Cell* node, Cell* cand, bool isZ) {
    if (!node || !cand) return false;
    if (lut6d_info.inputs.empty()) return false;

    const vector<SigBit>& inputs = lut6d_info.inputs;
    vector<int> var_pos;
    var_pos.reserve(inputs.size());
    vector<State> fixed_vals(inputs.size(), State::Sx);
    for (size_t i = 0; i < inputs.size(); i++) {
        if (inputs[i].wire == nullptr) {
            fixed_vals[i] = inputs[i].data;
            if (fixed_vals[i] != State::S0 && fixed_vals[i] != State::S1) {
                return false;
            }
        } else {
            var_pos.push_back(int(i));
        }
    }

    size_t bits_num = size_t(1 << var_pos.size());
    SigBit z_out = isZ ? GetCellOutput(cand) : GetCellOutput(node);
    SigBit z5_out = isZ ? GetCellOutput(node) : GetCellOutput(cand);

    for (size_t mask = 0; mask < bits_num; mask++) {
        dict<SigBit, State> bit_map;
        size_t idx = 0;
        for (size_t i = 0; i < inputs.size(); i++) {
            State val = State::S0;
            if (inputs[i].wire == nullptr) {
                val = fixed_vals[i];
            } else {
                int bit_idx = -1;
                for (size_t k = 0; k < var_pos.size(); k++) {
                    if ((size_t)var_pos[k] == i) {
                        bit_idx = int(k);
                        break;
                    }
                }
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
        size_t idx_z5 = idx & ((size_t(1) << (inputs.size() - 1)) - 1);
        bool act_z5 = ((lut6d_info.INIT >> idx_z5) & 1) != 0;

        if (act_z != (exp_z == State::S1)) return false;
        if (act_z5 != (exp_z5 == State::S1)) return false;
    }
    return true;
}

// 计算所有节点的Arrival Time（Forward Pass - 从PI到PO）
void ComputeArrivalTimes(vector<Cell*> &gates) {
    log("  Computing arrival times (forward pass)...\n");
    cell2arrival.clear();
    
    // 按拓扑顺序遍历（gates已经是拓扑排序的）
    for (Cell* node : gates) {
        if (!IsCombinationalGate(node)) continue;
        
        // 获取节点的所有输入信号
        pool<SigBit> inputs;
        GetCellInputsSet(node, inputs);
        
        // 计算arrival time = max(所有输入的arrival time) + 1.0
        float max_input_arrival = 0.0f;
        for (auto input : inputs) {
            Cell *driver = bit2driver.count(input) ? bit2driver[input] : nullptr;
            if (driver && cell2arrival.count(driver)) {
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
        
        // 获取该节点的输入
        vector<SigBit> inputs;
        GetCellInputsVector(node, inputs);
        
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

void ProcessNode(Cell* node, bool mandatoryCut6, vector<LUT6DInfo>& lut6d_infos, size_t lutSize=0) {
    // 计算节点的slack
    float node_slack = cell2required.count(node) && cell2arrival.count(node) 
                        ? cell2required[node] - cell2arrival[node] 
                        : 1.0f;  // 默认给一个正的slack
    
    // 使用基于slack的分级策略寻找候选节点
    // - slack < 0.1f: 只考虑最浅depth的cand_cut
    // - 0.1f <= slack < 0.5f: 考虑depth <= min_depth+1的cand_cut  
    // - slack >= 0.5f: 考虑所有深度的cand_cut
    vector<pairCandidate> pair_candidates;
    EnumerateCandidates(node, node_slack, pair_candidates, lutSize, mandatoryCut6);
    sort(pair_candidates.begin(), pair_candidates.end(), [](const pairCandidate& a, const pairCandidate& b) {
        return a.score > b.score;
    });
    if (pair_candidates.empty()) return;

    const pairCandidate& best_candidate = pair_candidates[0];
    log("    Applying best LUT6D mapping: node=%s, cand=%s, benefit=%.2f, I5=%s, isZ=%d\n",
        node->name.c_str(), best_candidate.cand->name.c_str(), 
        best_candidate.benefit.totalBenefit(), log_signal(best_candidate.I5), best_candidate.isZ);
    log_debug("      Cut leaves: ");
    for (auto bit : best_candidate.cut.leaves) {
        log_debug("%s ", log_signal(bit));
    }
    log_debug("\n");
    log_debug("      Node output: %s, Cand output: %s\n",
              log_signal(GetCellOutput(node)), log_signal(GetCellOutput(best_candidate.cand)));
    // 将node、cand和它们的shared MFFC中的节点都加入processed_nodes
    for (auto bit : best_candidate.cut.leaves) {
        Cell* driver = bit2driver.count(bit) ? bit2driver[bit] : nullptr;
        // log_debug("      Used node from leaves: %s\n", 
        //           driver ? driver->name.c_str() : "N/A");
        used_nodes.insert(driver);
    }
    vector<SigBit> inputs;
    uint64_t INIT = 0;

    LUT6DInfo lut6d_info;
    CalculateLUT6DINIT(node, best_candidate, lut6d_info);
    if (!VerifyLUT6DMapping(lut6d_info, node, best_candidate.cand, best_candidate.isZ)) {
        return;
    }

    processed_nodes.insert(node);
    processed_nodes.insert(best_candidate.cand);

    // 重要：node和cand本身也需要被删除，因为它们的输出将由LUT6D的Z/Z5端口驱动
    lut6d_info.removedNodes.insert(node->name);
    lut6d_info.removedNodes.insert(best_candidate.cand->name);
    log_debug("      Removed nodes: ");

    pool<Cell*> shared_mffc;
    ComputeSharedMFFCEngine(node, best_candidate.cand, shared_mffc, best_candidate.cut.leaves);
    for (Cell* mffc_node : shared_mffc) {
        if (used_nodes.count(mffc_node)) continue;
        processed_nodes.insert(mffc_node);
        lut6d_info.removedNodes.insert(mffc_node->name);  // 存储名称而不是指针
        log_debug("%s, ", mffc_node->name.c_str());
    }
    log_debug("\n");
    lut6d_infos.push_back(lut6d_info);
}

void ProcessGlobalMatching(Module *module, vector<Cell *> &gates, size_t minCutSize, bool mandatoryCut6,
                           vector<LUT6DInfo> &lut6d_infos)
{
    (void)module;
    (void)mandatoryCut6;
    vector<pairCandidate> all_candidates;
    all_candidates.reserve(gates.size() * 4);

    for (Cell* node : gates) {
        if (!IsCombinationalGate(node)) continue;
        if (processed_nodes.count(node)) continue;
        float node_slack = cell2required.count(node) && cell2arrival.count(node)
                            ? cell2required[node] - cell2arrival[node]
                            : 1.0f;
        EnumerateCandidates(node, node_slack, all_candidates, minCutSize, mandatoryCut6);
    }

    sort(all_candidates.begin(), all_candidates.end(),
         [](const pairCandidate& a, const pairCandidate& b) {
             return a.score > b.score;
         });

    size_t selected = 0;
    for (const auto& cand : all_candidates) {
        if (cand.score <= 0.0) continue;
        if (!cand.node || !cand.cand) continue;
        if (processed_nodes.count(cand.node) || processed_nodes.count(cand.cand)) continue;

        bool conflict = false;
        pool<Cell*> leaf_drivers;
        for (auto bit : cand.cut.leaves) {
            Cell* driver = bit2driver.count(bit) ? bit2driver[bit] : nullptr;
            if (!driver || !IsCombinationalGate(driver)) continue;
            leaf_drivers.insert(driver);
            if (processed_nodes.count(driver)) {
                conflict = true;
                break;
            }
        }
        if (conflict) continue;

        pool<Cell*> shared_mffc;
        ComputeSharedMFFCEngine(cand.node, cand.cand, shared_mffc, cand.cut.leaves);

        for (Cell* mffc_node : shared_mffc) {
            if (processed_nodes.count(mffc_node)) {
                conflict = true;
                break;
            }
        }
        if (conflict) continue;

        for (Cell* driver : leaf_drivers) {
            used_nodes.insert(driver);
        }

        LUT6DInfo lut6d_info;
        CalculateLUT6DINIT(cand.node, cand, lut6d_info);
        if (!VerifyLUT6DMapping(lut6d_info, cand.node, cand.cand, cand.isZ)) {
            continue;
        }

        processed_nodes.insert(cand.node);
        processed_nodes.insert(cand.cand);
        lut6d_info.removedNodes.insert(cand.node->name);
        lut6d_info.removedNodes.insert(cand.cand->name);

        for (Cell* mffc_node : shared_mffc) {
            if (used_nodes.count(mffc_node)) continue;
            SigBit out = GetCellOutput(mffc_node);
            if (prime_outputs.count(out)) continue;
            
            // 检查这个信号是否受保护（被其他节点的best_cut使用）
            if (protected_signals.count(out)) continue;
            
            bool external_use = false;
            if (bit2reader.count(out)) {
                for (Cell* reader : bit2reader[out]) {
                    if (!IsCombinationalGate(reader)) {
                        external_use = true;
                        break;
                    }
                    if (reader != cand.node && reader != cand.cand && !shared_mffc.count(reader)) {
                        external_use = true;
                        break;
                    }
                }
            }
            if (external_use) continue;
            processed_nodes.insert(mffc_node);
            lut6d_info.removedNodes.insert(mffc_node->name);
        }

        lut6d_infos.push_back(lut6d_info);
        selected++;
    }

    log("  Global matching selected %zu pairs (candidates=%zu, minCutSize=%zu)\n",
        selected, all_candidates.size(), minCutSize);
}

void ProcessLUTSize(Module *module, vector<Cell *> &gates, size_t lut_size, vector<LUT6DInfo> &lutd_infos) {
    LUT_SIZE = min(lut_size, size_t(6));
    // Phase 4: LUT6D映射（基于slack的分级策略）
    log("\nPhase 4: LUT%sD mapping with slack-based grading strategy, only for %zu-input cuts...\n",
        to_string(LUT_SIZE).c_str(), LUT_SIZE);
    ProcessGlobalMatching(module, gates, LUT_SIZE, true, lutd_infos);

    // Phase 5: LUT6D映射（基于其他cut大小的策略）
    log("\nPhase 5: LUT%sD mapping for other cut sizes...\n", to_string(LUT_SIZE).c_str());
    ProcessGlobalMatching(module, gates, 0, false, lutd_infos);

    // for (Cell* node : gates) {
    //     if (processed_nodes.count(node)) continue;
    //     ProcessNode(node, false, lutd_infos);
    // }

    log("\nRemained Nodes after LUT%sD mapping: %zu / %zu\n", 
        to_string(LUT_SIZE).c_str(), gates.size() - processed_nodes.size(), gates.size());
    log_debug("Remained Nodes: ");
    for (Cell* node : gates) {
        if (processed_nodes.count(node)) continue;
        log_debug("%s ", node->name.c_str());
    }
    log_debug("\n");
}

void LUT6DMapping(Module *module, vector<Cell *> &gates)
{  
    // Initialize prime inputs and outputs
    prime_inputs.clear();
    prime_outputs.clear();
    GetPrimeInputOutput(module, prime_inputs, prime_outputs);
    
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
    log("Phase 1: Generating cuts and MFFCs for all nodes...\n");

    for (Cell* node : gates) {
        vector<Cut> cuts;
        GenerateCutsForNode(node, cuts);
        cell2cuts[node] = cuts;

        pool<Cell*> mffc;
        ComputeMFFC(node, mffc);
        cell2mffc[node] = mffc;

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
    
    // ABC风格：多轮迭代优化
    log("\nPhase 2.5: Multi-round mapping optimization (ABC-style, %d rounds)...\n", MAPPING_ROUNDS);
    for (int round = 1; round <= MAPPING_ROUNDS; round++) {
        log("  Round %d: %s optimization...\n", round, round == 1 ? "delay" : "area");
        
        // 更新每个节点的best cut
        for (Cell* node : gates) {
            if (!IsCombinationalGate(node)) continue;
            if (!cell2cuts.count(node) || cell2cuts[node].empty()) continue;
            
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
                cell2bestcut[node] = *best;
            }
        }
        
        // 更新arrival time（基于新的best cuts）
        ComputeArrivalTimes(gates);
        
        // 更新估计引用计数（用于下一轮area flow计算）
        for (Cell* node : gates) {
            if (!IsCombinationalGate(node)) continue;
            // 简化：使用实际fanout的平均值
            float est = (cell2est_refs[node] + cell2refs[node]) / 2.0f;
            cell2est_refs[node] = max(1.0f, est);
        }
        
        // 统计当前轮次的结果
        size_t total_area = 0;
        float max_delay = 0.0f;
        for (Cell* node : gates) {
            if (cell2bestcut.count(node)) {
                total_area += 1;  // 每个best cut对应一个LUT
                max_delay = max(max_delay, cell2bestcut[node].arrival_time);
            }
        }
        log("    Round %d result: estimated LUTs=%zu, max_delay=%.1f\n", round, total_area, max_delay);
    }

    // Phase 3: 生成bit-level状态模拟
    log("\nPhase 3: Generating bit-level state simulation...\n");
    GenerateBitState(module, prime_inputs, false, gates);

    // 预先计算所有受保护的信号：所有节点的best_cut的leaves
    // 这些信号的驱动节点不能被删除，因为它们会被LUT使用
    log("\nPre-computing protected signals (all best cut leaves)...\n");
    protected_signals.clear();
    for (Cell* node : gates) {
        if (!IsCombinationalGate(node)) continue;
        const Cut* best_cut = nullptr;
        if (cell2bestcut.count(node)) {
            best_cut = &cell2bestcut[node];
        } else if (cell2cuts.count(node) && !cell2cuts[node].empty()) {
            best_cut = &cell2cuts[node][0];
        }
        if (best_cut) {
            for (auto bit : best_cut->leaves) {
                protected_signals.insert(bit);
            }
        }
    }
    log("  Total protected signals: %zu\n", protected_signals.size());

    vector<LUT6DInfo> lut_infos;
    
    // Phase 4 & 5: LUT6D映射
    ProcessLUTSize(module, gates, 6, lut_infos);
    ProcessLUTSize(module, gates, 5, lut_infos);
    ProcessLUTSize(module, gates, 4, lut_infos);

    // Phase 6: 为剩余未配对的节点生成单输出LUT（先计算INIT，再删除节点）
    log("\nPhase 6: Generating single-output LUTs for remaining nodes...\n");
    vector<SingleLUTInfo> single_luts;
    
    for (Cell* node : gates) {
        if (!IsCombinationalGate(node)) continue;
        if (processed_nodes.count(node)) continue;

        // 选择该节点的最优cut
        const Cut* best_cut = nullptr;
        if (cell2bestcut.count(node)) {
            best_cut = &cell2bestcut[node];
        } else if (cell2cuts.count(node) && !cell2cuts[node].empty()) {
            best_cut = &cell2cuts[node][0];
        } else {
            continue;
        }

        SingleLUTInfo single_lut;
        single_lut.output = GetCellOutput(node);

        // 收集输入
        for (auto bit : best_cut->leaves) {
            single_lut.inputs.push_back(bit);
        }

        // 计算INIT（通过模拟）- 必须在删除任何节点之前完成
        size_t num_inputs = single_lut.inputs.size();
        size_t lut_size = size_t(1 << num_inputs);
        uint64_t INIT = 0;

        for (size_t i = 0; i < lut_size && i < 64; i++) {
            dict<SigBit, State> bit_map;
            for (size_t j = 0; j < num_inputs; j++) {
                State val = ((i >> j) & 1) ? State::S1 : State::S0;
                if (single_lut.inputs[j].wire == nullptr) {
                    val = single_lut.inputs[j].data;
                }
                bit_map[single_lut.inputs[j]] = val;
            }
            State out = StateEval(bit_map, single_lut.output);
            if (out == State::S1) {
                INIT |= (uint64_t(1) << i);
            }
        }
        single_lut.INIT = INIT;

        // 标记leaves中被使用的节点（这些节点的输出会被LUT使用，不应被删除）
        pool<Cell*> used_leaf_cells;
        for (auto bit : best_cut->leaves) {
            Cell* driver = bit2driver.count(bit) ? bit2driver[bit] : nullptr;
            if (driver) {
                used_leaf_cells.insert(driver);
            }
        }

        // 必须移除root节点，避免多驱动
        single_lut.removedNodes.insert(node->name);
        processed_nodes.insert(node);

        // 计算并删除cut内部的MFFC节点
        // 这些节点的输出只被cut内部使用，可以安全删除
        pool<Cell*> mffc;
        ComputeMFFC(node, mffc);
        
        for (Cell* mffc_node : mffc) {
            // 跳过root节点（已经添加）
            if (mffc_node == node) continue;
            
            // 跳过叶子节点（它们的输出会被LUT使用）
            if (used_leaf_cells.count(mffc_node)) continue;
            
            // 检查这个MFFC节点的输出是否受保护（被任何节点的best_cut使用）
            SigBit mffc_output = GetCellOutput(mffc_node);
            if (protected_signals.count(mffc_output)) {
                // 这个节点的输出被其他LUT作为输入使用，不能删除
                continue;
            }
            
            // 检查这个节点的输出是否是primary output
            if (prime_outputs.count(mffc_output)) continue;
            
            // 这个MFFC节点可以安全删除
            single_lut.removedNodes.insert(mffc_node->name);
            processed_nodes.insert(mffc_node);
        }

        single_luts.push_back(single_lut);
        log_debug("  Single LUT for node %s: %zu inputs, INIT=0x%llX, removed %zu nodes\n",
                  node->name.c_str(), num_inputs, INIT, single_lut.removedNodes.size());
    }
    
    // 现在才开始添加LUT并删除原节点
    log("\nTotal LUT6D cells to be added: %zu\n", lut_infos.size());
    for (auto lut_info : lut_infos) {
        lut_info.addLUT6D(module);
    }
    
    log("Total single-output LUTs to be added: %zu\n", single_luts.size());
    for (auto& single_lut : single_luts) {
        single_lut.addSingleLUT(module);
    }
    
    log("\nFinal: %zu LUT6D + %zu single LUT = %zu total LUTs\n", 
        lut_infos.size(), single_luts.size(), lut_infos.size() + single_luts.size());
}

// 清理所有全局数据结构，释放内存
void CleanupGlobalDataStructures()
{
    log("Cleaning up global data structures...\n");
    
    // 清理所有dict和pool
    sigmap.clear();
    bit2driver.clear();
    bit2reader.clear();
    bit2states.clear();
    cell2bits.clear();
    cell2cost.clear();
    cell2cuts.clear();
    cell2depth.clear();
    cell2level.clear();
    cell2arrival.clear();
    cell2required.clear();
    cell2bestcut.clear();
    cell2mffc.clear();
    cell2index.clear();
    orig_fanout.clear();
    processed_nodes.clear();
    used_nodes.clear();
    prime_outputs.clear();
    prime_inputs.clear();
    protected_signals.clear();  // 清理受保护信号集合
    cell2est_refs.clear();   // ABC风格：清理估计引用计数
    cell2refs.clear();       // ABC风格：清理实际引用计数
    
    // 重置fanout标志
    fanout_ready = false;
    
    log("Memory cleanup complete.\n");
}

void LUT6DMapperMain(Module *module)
{
    // 在开始处理前先清理内存
    CleanupGlobalDataStructures();  
    vector<Cell *> gates;
    CheckCellWidth(module);
    GetTopoSortedGates(module, gates);
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
		log_header(design, "Continuing MapperPass pass.\n");
        LUT6DMapperMain(module);
		log_pop();
    }
} LUT6DMapPass;

PRIVATE_NAMESPACE_END
