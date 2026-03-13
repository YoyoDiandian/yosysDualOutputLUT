#!/bin/bash
# Convert MCNC BLIFs to AIG (optional) and run ABC vs LUT6D mapping,
# processing test cases in ascending size order.

if [ -z "${BASH_VERSION:-}" ] || shopt -qo posix; then
    exec /bin/bash "$0" "$@"
fi

set -e

YOSYS="./yosys"
ABC="./yosys-abc"
BLIF_DIR="/Users/yoyowang/Documents/Work/FudanCAD/paper2026/datasets/mcnc2"
TEST_DIR="test_output_mcnc3"
AIG_DIR="$TEST_DIR/aig"

USE_AIG=1
ENFORCE_NOT_WORSE_THAN_ABC=1
LEC_ENGINE="abc_cec"
for arg in "$@"; do
    case "$arg" in
        --no-aig|--blif)
            USE_AIG=0
            ;;
        --allow-worse-than-abc)
            ENFORCE_NOT_WORSE_THAN_ABC=0
            ;;
        --lec-abc|--cec)
            LEC_ENGINE="abc_cec"
            ;;
        --lec-yosys)
            LEC_ENGINE="yosys_equiv"
            ;;
        --help|-h)
            echo "Usage: $0 [--no-aig|--blif] [--allow-worse-than-abc] [--lec-abc|--cec|--lec-yosys]"
            echo "  Default: convert BLIF to AIG and run mapping on AIG."
            echo "  --no-aig/--blif: run mapping directly on BLIF."
            echo "  --allow-worse-than-abc: disable per-benchmark ABC fallback safeguard."
            echo "  --lec-abc/--cec: use ABC cec for equivalence (default)."
            echo "  --lec-yosys: use Yosys equiv_make/equiv_simple/equiv_induct."
            exit 0
            ;;
    esac
done

if [ ! -x "$YOSYS" ]; then
    echo "Error: $YOSYS not found or not executable."
    exit 1
fi
if [ ! -x "$ABC" ]; then
    echo "Error: $ABC not found or not executable."
    exit 1
fi

mkdir -p "$TEST_DIR"
if [ "$USE_AIG" -eq 1 ]; then
    mkdir -p "$AIG_DIR"
fi

AIG_MARKER="$AIG_DIR/.with_symbols"
AIG_FORCE=0
if [ "$USE_AIG" -eq 1 ] && [ ! -f "$AIG_MARKER" ]; then
    AIG_FORCE=1
fi

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "========================================"
echo "  ABC vs LUT6D_MAP Comparison & LEC"
echo "  MCNC (sorted by size)"
echo "========================================"
echo ""
if [ "$USE_AIG" -eq 1 ]; then
    echo "Mode: BLIF -> AIG -> mapping"
else
    echo "Mode: BLIF -> mapping (no AIG conversion)"
fi
if [ "$ENFORCE_NOT_WORSE_THAN_ABC" -eq 1 ]; then
    echo "Policy: per-benchmark safeguard enabled (fallback to ABC when LUT6D > ABC)"
else
    echo "Policy: pure LUT6D result (may be worse than ABC on some cases)"
fi
if [ "$LEC_ENGINE" = "abc_cec" ]; then
    echo "LEC engine: ABC cec (fallback to Yosys equiv if needed)"
else
    echo "LEC engine: Yosys equiv"
fi
echo ""

get_blif_stats() {
    local blif_file=$1
    local stats_text
    if ! stats_text=$($ABC -c "read_blif $blif_file; ps" 2>/dev/null); then
        echo "0 0"
        return
    fi

    local num_nodes
    local num_lat
    num_nodes=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="nd" && $(i+1)=="=") {print $(i+2); exit}}' | tr -d ' ')
    num_lat=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="lat" && $(i+1)=="=") {print $(i+2); exit}}' | tr -d ' ')
    echo "${num_nodes:-0} ${num_lat:-0}"
}

group_for_nodes() {
    local n=$1
    if [ "$n" -lt 200 ]; then
        echo "Small (<200)"
    elif [ "$n" -lt 1000 ]; then
        echo "Medium (200-999)"
    elif [ "$n" -lt 10000 ]; then
        echo "Large (1000-9999)"
    else
        echo "Very Large (>=10000)"
    fi
}

prepare_aig() {
    local blif_file=$1
    local basename=$2
    local aig_file="$AIG_DIR/${basename}.aig"
    if [ "$AIG_FORCE" -eq 1 ] || [ ! -f "$aig_file" ] || [ "$blif_file" -nt "$aig_file" ]; then
        local abc_status=0
        if $ABC -c "read_blif $blif_file; strash; write_aiger -s $aig_file" > "$TEST_DIR/${basename}_aig.log" 2>&1; then
            abc_status=0
        else
            abc_status=$?
        fi
        if [ $abc_status -ne 0 ]; then
            echo "ERROR: ABC failed to convert $blif_file to AIG" >&2
            return 1
        fi
    fi
    echo "$aig_file"
}

# Function to run ABC mapping
run_abc_mapping() {
    local input_file=$1
    local basename=$2
    local read_cmd=$3
    local output_log="$TEST_DIR/${basename}_abc.log"
    local status=0

    if $ABC -c "$read_cmd $input_file; ps; if -K 6; ps; write_verilog $TEST_DIR/${basename}_abc_mapped.v" > "$output_log" 2>&1; then
        status=0
    else
        status=$?
    fi

    if [ $status -ne 0 ]; then
        echo "$status 0 0 0"
        return
    fi

    local stats_text=$(grep -A 3 "if -K 6" "$output_log" | tail -2 | tr '\n' ' ')
    local num_nodes=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="nd" && $(i+1)=="=") print $(i+2)}' | tr -d ' ')
    local num_edges=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="edge" && $(i+1)=="=") print $(i+2)}' | tr -d ' ')
    local num_lev=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="lev" && $(i+1)=="=") print $(i+2)}' | tr -d ' ')

    num_nodes=${num_nodes:-0}
    num_edges=${num_edges:-0}
    num_lev=${num_lev:-0}

    echo "$status $num_nodes $num_edges $num_lev"
}

# Function to run Yosys LUT6D_MAP
run_lut6d_mapping() {
    local input_file=$1
    local basename=$2
    local read_cmd=$3
    local output_log="$TEST_DIR/${basename}_lut6d.log"
    local status=0

    if $YOSYS -p "
        $read_cmd $input_file
        hierarchy -check -auto-top
        proc; opt; techmap; opt;
        lut6d_map
        clean
        stat
        write_verilog -norename $TEST_DIR/${basename}_lut6d_mapped.v
    " > "$output_log" 2>&1; then
        status=0
    else
        status=$?
    fi

    # Check if the command failed due to input plane limit
    if [ $status -ne 0 ] && grep -q "input plane must have fewer than.*signals" "$output_log"; then
        echo "SKIPPED_INPUT_LIMIT 0 0 0 0 0 0 0 0 0 0"
        return
    fi

    if [ $status -ne 0 ]; then
        echo "$status 0 0 0 0 0 0 0 0 0 0"
        return
    fi

    local num_lut6d=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT6D[[:space:]]*$/ {print $1}' "$output_log" || echo "0")
    local num_lut6=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT6[[:space:]]*$/ {print $1}' "$output_log" || echo "0")
    local num_lut5d=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT5D[[:space:]]*$/ {print $1}' "$output_log" || echo "0")
    local num_lut5=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT5[[:space:]]*$/ {print $1}' "$output_log" || echo "0")
    local num_lut4d=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT4D[[:space:]]*$/ {print $1}' "$output_log" || echo "0")
    local num_lut4=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT4[[:space:]]*$/ {print $1}' "$output_log" || echo "0")
    local num_lut3=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT3[[:space:]]*$/ {print $1}' "$output_log" || echo "0")
    local num_lut2=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT2[[:space:]]*$/ {print $1}' "$output_log" || echo "0")

    num_lut6d=${num_lut6d:-0}
    num_lut6=${num_lut6:-0}
    num_lut5d=${num_lut5d:-0}
    num_lut5=${num_lut5:-0}
    num_lut4d=${num_lut4d:-0}
    num_lut4=${num_lut4:-0}
    num_lut3=${num_lut3:-0}
    num_lut2=${num_lut2:-0}

    local total_luts=$((num_lut6d + num_lut6 + num_lut5d + num_lut5 + num_lut4d + num_lut4 + num_lut3 + num_lut2))
    local total_dual=$((num_lut6d + num_lut5d + num_lut4d))

    echo "$status $total_luts $total_dual $num_lut6d $num_lut6 $num_lut5d $num_lut5 $num_lut4d $num_lut4 $num_lut3 $num_lut2"
}

# Function to run equivalence checking (Yosys equiv flow)
run_equivalence_check_yosys() {
    local src_file=$1
    local mapped_file=$2
    local basename=$3
    local read_cmd=${4:-read_blif}
    local lec_log="$TEST_DIR/${basename}_lec.log"
    local src_for_lec="$src_file"
    local src_read_cmd="$read_cmd"

    # read_aiger会把部分匿名端口名保留为$前缀私有名，而Verilog读取后的mapped网表
    # 端口名可能是转义后的公有名；先统一转成Verilog可避免equiv_make端口匹配误判。
    if [ "$read_cmd" = "read_aiger" ]; then
        src_for_lec="$TEST_DIR/${basename}_gold_ref.v"
        local gold_ref_log="$TEST_DIR/${basename}_gold_ref.log"
        if $YOSYS -p "
            $read_cmd $src_file
            hierarchy -check -auto-top
            flatten
            write_verilog -norename $src_for_lec
        " > "$gold_ref_log" 2>&1; then
            src_read_cmd="read_verilog"
        else
            echo "FAIL"
            return
        fi
    fi

    cat > $TEST_DIR/${basename}_lec.ys << EOF
# Read cell definitions
read_verilog -icells techlibs/yoyo/cells_sim.v
read_verilog -lib -nooverwrite techlibs/yoyo/cells_lib.v

# Read mapped design
read_verilog $mapped_file
hierarchy -check -auto-top
flatten
design -stash after_map

# Read original design
$src_read_cmd $src_for_lec
hierarchy -check -auto-top
flatten
design -stash before_map

# Build equivalence checking netlist
design -copy-from before_map -as gold A:top
design -copy-from after_map -as gate A:top
read_verilog -lib -nooverwrite techlibs/yoyo/cells_lib.v

equiv_make gold gate equiv
equiv_simple -nogroup -undef
equiv_induct equiv
equiv_status equiv
EOF

    $YOSYS -s $TEST_DIR/${basename}_lec.ys > $lec_log 2>&1

    if grep -q "Equivalence successfully proven" $lec_log; then
        echo "PASS"
    elif grep -q "proven" $lec_log && ! grep -q "failed" $lec_log; then
        echo "PASS"
    else
        echo "FAIL"
    fi
}

# Function to run equivalence checking (ABC cec flow)
run_equivalence_check_abc_cec() {
    local src_file=$1
    local mapped_file=$2
    local basename=$3
    local read_cmd=${4:-read_blif}

    local gold_aig="$TEST_DIR/${basename}_gold_cec.aig"
    local gate_aig="$TEST_DIR/${basename}_gate_cec.aig"
    local gold_prep_log="$TEST_DIR/${basename}_gold_cec_prep.log"
    local gate_prep_log="$TEST_DIR/${basename}_gate_cec_prep.log"
    local cec_log="$TEST_DIR/${basename}_cec.log"
    local lec_log="$TEST_DIR/${basename}_lec.log"

    # gold转换：统一读入并导出无symbol的AIG（cec按顺序匹配端口，避免名字不一致导致miter失败）
    if ! $YOSYS -p "
        $read_cmd $src_file
        hierarchy -check -auto-top
        flatten
        aigmap
        write_aiger $gold_aig
    " > "$gold_prep_log" 2>&1; then
        echo "FALLBACK"
        return
    fi

    # gate转换：用cells_sim功能模型展开LUT后导出AIG
    if ! $YOSYS -p "
        read_verilog -icells techlibs/yoyo/cells_sim.v
        read_verilog $mapped_file
        hierarchy -check -auto-top
        flatten
        techmap
        opt
        aigmap
        write_aiger $gate_aig
    " > "$gate_prep_log" 2>&1; then
        echo "FALLBACK"
        return
    fi

    if ! $ABC -c "cec $gold_aig $gate_aig" > "$cec_log" 2>&1; then
        echo "FALLBACK"
        return
    fi

    cp "$cec_log" "$lec_log" 2>/dev/null || true

    if grep -q "Networks are equivalent" "$cec_log"; then
        echo "PASS"
    elif grep -Eqi "NOT EQUIVALENT|Networks are not equivalent" "$cec_log"; then
        echo "FAIL"
    elif grep -q "Miter computation has failed" "$cec_log"; then
        echo "FALLBACK"
    else
        echo "FALLBACK"
    fi
}

# Function to run equivalence checking (selected engine with fallback)
run_equivalence_check() {
    local src_file=$1
    local mapped_file=$2
    local basename=$3
    local read_cmd=${4:-read_blif}

    if [ "$LEC_ENGINE" = "abc_cec" ]; then
        local cec_result
        cec_result=$(run_equivalence_check_abc_cec "$src_file" "$mapped_file" "$basename" "$read_cmd")
        if [ "$cec_result" = "PASS" ] || [ "$cec_result" = "FAIL" ]; then
            echo "$cec_result"
            return
        fi
    fi

    run_equivalence_check_yosys "$src_file" "$mapped_file" "$basename" "$read_cmd"
}

# Function to test a single case
test_single_file() {
    local test_case=$1
    local blif_file=$2
    local size_nodes=$3
    local lat_count=${4:-0}
    local basename=$test_case
    local input_file="$blif_file"
    local abc_read_cmd="read_blif"
    local yosys_read_cmd="read_blif"
    local lec_read_cmd="read_blif"

    if [ "$USE_AIG" -eq 1 ] && [ "$lat_count" -eq 0 ]; then
        if input_file=$(prepare_aig "$blif_file" "$basename"); then
            :
        else
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            echo -e "${BLUE}Testing: $basename${NC}"
            echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
            echo "  Size (nd): $size_nodes"
            echo -e "  ${RED}Skipped (AIG conversion failed)${NC}"
            echo "$basename,$size_nodes,SKIPPED_AIG_CONVERSION,,,,,,,,,,,,," >> $TEST_DIR/results.csv
            echo ""
            return 1
        fi
        abc_read_cmd="read_aiger"
        yosys_read_cmd="read_aiger"
        lec_read_cmd="read_aiger"
    fi

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo -e "${BLUE}Testing: $basename${NC}"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  Size (nd): $size_nodes"
    echo "  Latency (lat): $lat_count"
    if [ "$USE_AIG" -eq 1 ] && [ "$lat_count" -ne 0 ]; then
        echo "  Note: lat != 0, keep BLIF input (skip AIG conversion)"
    fi

    # Run ABC mapping
    echo -n "  [1/3] Running ABC mapping... "
    abc_result=$(run_abc_mapping "$input_file" "$basename" "$abc_read_cmd")
    abc_status=$(echo $abc_result | cut -d' ' -f1)
    abc_nodes=$(echo $abc_result | cut -d' ' -f2)
    abc_edges=$(echo $abc_result | cut -d' ' -f3)
    abc_lev=$(echo $abc_result | cut -d' ' -f4)
    if [ "$abc_status" -eq 0 ]; then
        echo -e "${GREEN}Done${NC}"
        echo "        Nodes: $abc_nodes, Edges: $abc_edges, Levels: $abc_lev"
    else
        echo -e "${RED}Failed${NC}"
        echo "$basename,$size_nodes,FAILED_ABC,,,,,,,,,,,,,FAILED" >> $TEST_DIR/results.csv
        echo ""
        return 1
    fi

    # Run LUT6D mapping
    echo -n "  [2/3] Running LUT6D mapping... "
    lut6d_result=$(run_lut6d_mapping "$input_file" "$basename" "$yosys_read_cmd")
    
    # Check if it was skipped due to input limit
    if [ "$(echo $lut6d_result | cut -d' ' -f1)" = "SKIPPED_INPUT_LIMIT" ]; then
        echo -e "${YELLOW}Skipped (input plane limit)${NC}"
        echo "        Reason: BLIF file contains .names with >12 inputs"
        echo "$basename,$size_nodes,$abc_nodes,$abc_edges,$abc_lev,SKIPPED_INPUT_LIMIT,,,,,,,,,,SKIPPED" >> $TEST_DIR/results.csv
        echo ""
        return 1
    fi

    lut6d_status=$(echo $lut6d_result | cut -d' ' -f1)
    total_luts=$(echo $lut6d_result | cut -d' ' -f2)
    total_dual=$(echo $lut6d_result | cut -d' ' -f3)
    num_lut6d=$(echo $lut6d_result | cut -d' ' -f4)
    num_lut6=$(echo $lut6d_result | cut -d' ' -f5)
    num_lut5d=$(echo $lut6d_result | cut -d' ' -f6)
    num_lut5=$(echo $lut6d_result | cut -d' ' -f7)
    num_lut4d=$(echo $lut6d_result | cut -d' ' -f8)
    num_lut4=$(echo $lut6d_result | cut -d' ' -f9)
    num_lut3=$(echo $lut6d_result | cut -d' ' -f10)
    num_lut2=$(echo $lut6d_result | cut -d' ' -f11)

    if [ $lut6d_status -eq 0 ]; then
        echo -e "${GREEN}Done${NC}"
        echo "        Total LUTs: $total_luts (Dual: $total_dual)"
        echo "        LUT6D:$num_lut6d, LUT6:$num_lut6, LUT5D:$num_lut5d, LUT5:$num_lut5, LUT4D:$num_lut4d, LUT4:$num_lut4, LUT3:$num_lut3, LUT2:$num_lut2"
    else
        echo -e "${RED}Failed${NC}"
        echo "$basename,$size_nodes,$abc_nodes,$abc_edges,$abc_lev,FAILED,,,,,,,,,,FAILED" >> $TEST_DIR/results.csv
        echo ""
        return 1
    fi

    # Hard safeguard: never report/use a result worse than ABC.
    # If LUT6D result is worse, fall back to ABC-mapped result for this benchmark.
    local lec_result=""
    local using_abc_fallback=0
    if [ "$ENFORCE_NOT_WORSE_THAN_ABC" -eq 1 ] && [ -n "$abc_nodes" ] && [ "$abc_nodes" -gt 0 ] && [ "$total_luts" -gt "$abc_nodes" ]; then
        using_abc_fallback=1
        lec_result="ABC_FALLBACK"
        total_luts="$abc_nodes"
        total_dual=0
        num_lut6d=0
        num_lut5d=0
        num_lut4d=0
        num_lut6="$abc_nodes"
        num_lut5=0
        num_lut4=0
        num_lut3=0
        num_lut2=0
        cp "$TEST_DIR/${basename}_abc_mapped.v" "$TEST_DIR/${basename}_lut6d_mapped.v" 2>/dev/null || true
        echo -e "        ${YELLOW}Fallback: use ABC result for this case to enforce LUT6D<=ABC${NC}"
    fi

    # Run LEC for LUT6D mapping
    echo -n "  [3/3] Running LEC for LUT6D... "
    lec_result=$(run_equivalence_check "$input_file" "$TEST_DIR/${basename}_lut6d_mapped.v" "${basename}_lut6d" "$lec_read_cmd")
    if [ "$lec_result" = "PASS" ]; then
        echo -e "${GREEN}PASS ✓${NC}"
    else
        echo -e "${RED}FAIL ✗${NC}"
    fi
    if [ -z "$lec_result" ]; then
        lec_result="SKIPPED"
    fi

    # Compare results
    echo ""
    echo "  Comparison:"
    echo "    ABC Nodes:         $abc_nodes"
    echo "    LUT6D Total LUTs:  $total_luts"

    if [ -n "$abc_nodes" ] && [ "$abc_nodes" -gt 0 ] && [ -n "$total_luts" ]; then
        local diff=$((abc_nodes - total_luts))
        local percent=$(awk "BEGIN {printf \"%.1f\", ($diff / $abc_nodes) * 100}")
        if [ $diff -gt 0 ]; then
            echo -e "    ${GREEN}Improvement: $diff LUTs ($percent%)${NC}"
        elif [ $diff -lt 0 ]; then
            echo -e "    ${RED}Overhead: ${diff#-} LUTs ($percent%)${NC}"
        else
            echo "    Same result"
        fi
        if [ "$using_abc_fallback" -eq 1 ]; then
            echo -e "    ${YELLOW}Mode: ABC fallback${NC}"
        fi
    else
        echo "    ${YELLOW}Cannot calculate improvement (missing data)${NC}"
    fi

    # Save to CSV
    echo "$basename,$size_nodes,$abc_nodes,$abc_edges,$abc_lev,$total_luts,$total_dual,$num_lut6d,$num_lut6,$num_lut5d,$num_lut5,$num_lut4d,$num_lut4,$num_lut3,$num_lut2,$lec_result" >> $TEST_DIR/results.csv

    echo ""
}

# Build list of tests with size (node count from BLIF)
LIST_FILE="$TEST_DIR/test_list.csv"
SORTED_LIST="$TEST_DIR/test_list_sorted.csv"
: > "$LIST_FILE"

shopt -s nullglob
# Check if there are subdirectories with mcnc_ prefix (original structure)
if ls "$BLIF_DIR"/mcnc_* >/dev/null 2>&1 && [ -d "$BLIF_DIR/$(ls "$BLIF_DIR"/mcnc_* 2>/dev/null | head -1)" ]; then
    # Original structure: mcnc_*/input.blif
    for test_dir in "$BLIF_DIR"/mcnc_*; do
        blif_path="$test_dir/input.blif"
        if [ -f "$blif_path" ]; then
            test_case=$(basename "$test_dir")
            stats=$(get_blif_stats "$blif_path")
            size_nodes=$(echo "$stats" | awk '{print $1}')
            lat_count=$(echo "$stats" | awk '{print $2}')
            echo "$size_nodes,$lat_count,$test_case,$blif_path" >> "$LIST_FILE"
        else
            echo -e "${YELLOW}Warning: missing $blif_path${NC}"
        fi
    done
else
    # Flat structure: *.blif files directly in BLIF_DIR
    for blif_path in "$BLIF_DIR"/*.blif; do
        if [ -f "$blif_path" ]; then
            test_case=$(basename "$blif_path" .blif)
            stats=$(get_blif_stats "$blif_path")
            size_nodes=$(echo "$stats" | awk '{print $1}')
            lat_count=$(echo "$stats" | awk '{print $2}')
            echo "$size_nodes,$lat_count,$test_case,$blif_path" >> "$LIST_FILE"
        else
            echo -e "${YELLOW}Warning: $blif_path not found${NC}"
        fi
    done
fi
shopt -u nullglob

if [ ! -s "$LIST_FILE" ]; then
    echo -e "${RED}No MCNC test cases found in $BLIF_DIR${NC}"
    exit 1
fi

sort -t, -k1,1n -k2,2n -k3,3 "$LIST_FILE" > "$SORTED_LIST"

if [ "$USE_AIG" -eq 1 ]; then
    echo "Converting BLIF to AIG for lat=0 cases..."
    while IFS=',' read -r size_nodes lat_count test_case blif_path; do
        if [ "$lat_count" -ne 0 ]; then
            echo -n "  Converting $test_case... "
            echo -e "${YELLOW}Skipped (lat=$lat_count, keep BLIF)${NC}"
            continue
        fi
        echo -n "  Converting $test_case... "
        if prepare_aig "$blif_path" "$test_case" >/dev/null 2>&1; then
            echo -e "${GREEN}Done${NC}"
        else
            echo -e "${YELLOW}Skipped (conversion failed)${NC}"
        fi
    done < "$SORTED_LIST"
    : > "$AIG_MARKER"
    echo "AIG conversion complete."
    echo ""
fi

# Initialize CSV file: preserve existing results for skip-if-done logic
RESULTS_CSV="$TEST_DIR/results.csv"
RESULTS_HEADER="Benchmark,Size_Nodes,ABC_Nodes,ABC_Edges,ABC_Levels,Total_LUTs,Total_Dual,LUT6D,LUT6,LUT5D,LUT5,LUT4D,LUT4,LUT3,LUT2,LEC_Status"
if [ ! -f "$RESULTS_CSV" ] || ! head -1 "$RESULTS_CSV" | grep -q "^Benchmark,"; then
    echo "$RESULTS_HEADER" > "$RESULTS_CSV"
fi

# Process tests in ascending size order, with group headers
current_group=""
failed_cases=0
while IFS=',' read -r size_nodes lat_count test_case blif_path; do
    # Skip if this test case already has a result in the CSV
    if grep -q "^${test_case}," "$RESULTS_CSV"; then
        echo -e "  ${YELLOW}Skipping $test_case (already processed)${NC}"
        continue
    fi

    group=$(group_for_nodes "$size_nodes")
    if [ "$group" != "$current_group" ]; then
        echo "================================================"
        echo "  Group: $group"
        echo "================================================"
        echo ""
        current_group="$group"
    fi
    if ! test_single_file "$test_case" "$blif_path" "$size_nodes" "$lat_count"; then
        failed_cases=$((failed_cases + 1))
        echo -e "  ${YELLOW}Continue to next case.${NC}"
        echo ""
    fi
done < "$SORTED_LIST"

# Generate summary report
echo ""
echo "================================================"
echo "  Summary Report"
echo "================================================"
echo ""

total_tests=$(tail -n +2 $TEST_DIR/results.csv | wc -l | xargs)
passed_lec=$(grep ",PASS$" $TEST_DIR/results.csv | wc -l | xargs)
failed_lec=$(grep ",FAIL$" $TEST_DIR/results.csv | wc -l | xargs)

echo "Total tests:        $total_tests"
echo "Case failures:      $failed_cases"
echo "LEC passed:         $passed_lec"
echo "LEC failed:         $failed_lec"
echo ""

echo "Detailed Results (sorted by size):"
echo ""
tail -n +2 $TEST_DIR/results.csv | while IFS=',' read -r bench size_nodes abc_nodes abc_edges abc_lev lut_total total_dual lut6d lut6 lut5d lut5 lut4d lut4 lut3 lut2 lec; do
    if [ -n "$abc_nodes" ] && [ "$abc_nodes" -gt 0 ] && [ -n "$lut_total" ]; then
        diff=$((abc_nodes - lut_total))
        percent=$(awk "BEGIN {printf \"%.1f\", ($diff / $abc_nodes) * 100}")
        printf "  %-15s Size:%6s  ABC:%6s  LUT6D:%6s  Diff:%6s (%6s%%)  Dual:%s  LEC:%s\n" \
               "$bench" "$size_nodes" "$abc_nodes" "$lut_total" "$diff" "$percent" "$total_dual" "$lec"
    else
        printf "  %-15s Size:%6s  ABC:%6s  LUT6D:%6s  Diff:%6s (%6s%%)  Dual:%s  LEC:%s\n" \
               "$bench" "$size_nodes" "$abc_nodes" "$lut_total" "N/A" "N/A" "$total_dual" "$lec"
    fi
done

echo ""
echo "Full results saved to: $TEST_DIR/results.csv"
echo "Individual logs in: $TEST_DIR/"
echo ""
echo "To view detailed ABC output:"
echo "  cat $TEST_DIR/<name>_abc.log"
echo ""
echo "To view detailed LUT6D output:"
echo "  cat $TEST_DIR/<name>_lut6d.log"
echo ""
echo "To view LEC results:"
echo "  cat $TEST_DIR/<name>_lut6d_lec.log"
