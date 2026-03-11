#!/bin/bash
# Directly read AIG files and run ABC vs LUT6D mapping,
# processing test cases in ascending size order.

if [ -z "${BASH_VERSION:-}" ] || shopt -qo posix; then
    exec /bin/bash "$0" "$@"
fi

set -e

YOSYS="./yosys"
ABC="./yosys-abc"
AIG_INPUT_DIR="/Users/yoyowang/Desktop/benchmarks/arithmetic"
TEST_DIR="test_output_epfl"

if [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    echo "Usage: $0 [AIG_DIR]"
    echo "  Default AIG directory: $AIG_INPUT_DIR"
    exit 0
fi

if [ -n "${1:-}" ]; then
    AIG_INPUT_DIR="$1"
fi

if [ ! -x "$YOSYS" ]; then
    echo "Error: $YOSYS not found or not executable."
    exit 1
fi
if [ ! -x "$ABC" ]; then
    echo "Error: $ABC not found or not executable."
    exit 1
fi

mkdir -p "$TEST_DIR"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "========================================"
echo "  ABC vs LUT6D_MAP Comparison & LEC"
echo "  EPFL (sorted by size)"
echo "========================================"
echo ""
echo "Mode: AIG -> mapping"
echo "AIG directory: $AIG_INPUT_DIR"
echo ""

get_aig_stats() {
    local aig_file=$1
    local stats_text
    if ! stats_text=$($ABC -c "read_aiger $aig_file; ps" 2>/dev/null); then
        echo "0"
        return
    fi

    local num_nodes
    num_nodes=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="nd" && $(i+1)=="=") {print $(i+2); exit}}' | tr -d ' ')
    echo "${num_nodes:-0}"
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
        write_verilog $TEST_DIR/${basename}_lut6d_mapped.v
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

# Function to run equivalence checking
run_equivalence_check() {
    local src_file=$1
    local mapped_file=$2
    local basename=$3
    local read_cmd=${4:-read_aiger}
    local lec_log="$TEST_DIR/${basename}_lec.log"

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
$read_cmd $src_file
hierarchy -check -auto-top
flatten
design -stash before_map

# Build equivalence checking netlist
design -copy-from before_map -as gold A:top
design -copy-from after_map -as gate A:top
read_verilog -lib -nooverwrite techlibs/yoyo/cells_lib.v

equiv_make -inames gold gate equiv
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

# Function to test a single case
test_single_file() {
    local test_case=$1
    local aig_file=$2
    local size_nodes=$3
    local basename=$test_case
    local input_file="$aig_file"
    local abc_read_cmd="read_aiger"
    local yosys_read_cmd="read_aiger"
    local lec_read_cmd="read_aiger"

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo -e "${BLUE}Testing: $basename${NC}"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  Size (nd): $size_nodes"

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

    # Run LEC for LUT6D mapping
    # echo -n "  [3/3] Running LEC for LUT6D... "
    # lec_result=$(run_equivalence_check "$input_file" "$TEST_DIR/${basename}_lut6d_mapped.v" "${basename}_lut6d" "$lec_read_cmd")
    # if [ "$lec_result" = "PASS" ]; then
    #     echo -e "${GREEN}PASS ✓${NC}"
    # else
    #     echo -e "${RED}FAIL ✗${NC}"
    # fi
    # lec_result="SKIPPED"

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
    else
        echo "    ${YELLOW}Cannot calculate improvement (missing data)${NC}"
    fi

    # Save to CSV
    echo "$basename,$size_nodes,$abc_nodes,$abc_edges,$abc_lev,$total_luts,$total_dual,$num_lut6d,$num_lut6,$num_lut5d,$num_lut5,$num_lut4d,$num_lut4,$num_lut3,$num_lut2,$lec_result" >> $TEST_DIR/results.csv

    echo ""
}

# Build list of tests with size (node count from AIG)
LIST_FILE="$TEST_DIR/test_list.csv"
SORTED_LIST="$TEST_DIR/test_list_sorted.csv"
: > "$LIST_FILE"

shopt -s nullglob
for aig_path in "$AIG_INPUT_DIR"/*.aig; do
    if [ -f "$aig_path" ]; then
        test_case=$(basename "$aig_path" .aig)
        size_nodes=$(get_aig_stats "$aig_path")
        echo "$size_nodes,$test_case,$aig_path" >> "$LIST_FILE"
    fi
done
shopt -u nullglob

if [ ! -s "$LIST_FILE" ]; then
    echo -e "${RED}No AIG files found in $AIG_INPUT_DIR${NC}"
    exit 1
fi

sort -t, -k1,1n -k2,2 "$LIST_FILE" > "$SORTED_LIST"

# Initialize CSV file
echo "Benchmark,Size_Nodes,ABC_Nodes,ABC_Edges,ABC_Levels,Total_LUTs,Total_Dual,LUT6D,LUT6,LUT5D,LUT5,LUT4D,LUT4,LUT3,LUT2,LEC_Status" > $TEST_DIR/results.csv

# Process tests in ascending size order, with group headers
current_group=""
failed_cases=0
while IFS=',' read -r size_nodes test_case aig_path; do
    group=$(group_for_nodes "$size_nodes")
    if [ "$group" != "$current_group" ]; then
        echo "================================================"
        echo "  Group: $group"
        echo "================================================"
        echo ""
        current_group="$group"
    fi
    if ! test_single_file "$test_case" "$aig_path" "$size_nodes"; then
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
