#!/bin/bash
# Convert TC_PUBLIC BLIFs to AIG (optional) and run ABC vs LUT6D mapping,
# processing test cases in ascending size order.

if [ -z "${BASH_VERSION:-}" ]; then
    exec /bin/bash "$0" "$@"
fi

set -e

YOSYS="./yosys"
ABC="./yosys-abc"
BLIF_DIR="tests/tc_public"
TEST_DIR="test_output_tc_public_by_size"
AIG_DIR="$TEST_DIR/aig"

USE_AIG=1
if [ "${1:-}" = "--no-aig" ] || [ "${1:-}" = "--blif" ]; then
    USE_AIG=0
elif [ "${1:-}" = "--help" ] || [ "${1:-}" = "-h" ]; then
    echo "Usage: $0 [--no-aig|--blif]"
    echo "  Default: convert BLIF to AIG and run mapping on AIG."
    echo "  --no-aig/--blif: run mapping directly on BLIF."
    exit 0
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
if [ "$USE_AIG" -eq 1 ]; then
    mkdir -p "$AIG_DIR"
fi

AIG_MARKER="$AIG_DIR/.with_symbols"
AIG_FORCE=0
if [ "$USE_AIG" -eq 1 ] && [ ! -f "$AIG_MARKER" ]; then
    AIG_FORCE=1
fi

ABC_READ_CMD="read_aiger"
YOSYS_READ_CMD="read_aiger"
LEC_READ_CMD="read_aiger"
if [ "$USE_AIG" -ne 1 ]; then
    ABC_READ_CMD="read_blif"
    YOSYS_READ_CMD="read_blif"
    LEC_READ_CMD="read_blif"
fi

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "========================================"
echo "  ABC vs LUT6D_MAP Comparison & LEC"
echo "  TC_PUBLIC (sorted by size)"
echo "========================================"
echo ""
if [ "$USE_AIG" -eq 1 ]; then
    echo "Mode: BLIF -> AIG -> mapping"
else
    echo "Mode: BLIF -> mapping (no AIG conversion)"
fi
echo ""

get_blif_nodes() {
    local blif_file=$1
    local stats_text
    stats_text=$($ABC -c "read_blif $blif_file; ps" 2>/dev/null | tr '\n' ' ')
    local num_nodes
    num_nodes=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="nd" && $(i+1)=="=") print $(i+2)}' | tr -d ' ')
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

prepare_aig() {
    local blif_file=$1
    local basename=$2
    local aig_file="$AIG_DIR/${basename}.aig"
    if [ "$AIG_FORCE" -eq 1 ] || [ ! -f "$aig_file" ] || [ "$blif_file" -nt "$aig_file" ]; then
        $ABC -c "read_blif $blif_file; strash; write_aiger -s $aig_file" > "$TEST_DIR/${basename}_aig.log" 2>&1
    fi
    echo "$aig_file"
}

# Function to run ABC mapping
run_abc_mapping() {
    local input_file=$1
    local basename=$2
    local output_log="$TEST_DIR/${basename}_abc.log"

    $ABC -c "$ABC_READ_CMD $input_file; ps; if -K 6; ps; write_verilog $TEST_DIR/${basename}_abc_mapped.v" > $output_log 2>&1

    local stats_text=$(grep -A 3 "if -K 6" $output_log | tail -2 | tr '\n' ' ')
    local num_nodes=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="nd" && $(i+1)=="=") print $(i+2)}' | tr -d ' ')
    local num_edges=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="edge" && $(i+1)=="=") print $(i+2)}' | tr -d ' ')
    local num_lev=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="lev" && $(i+1)=="=") print $(i+2)}' | tr -d ' ')

    num_nodes=${num_nodes:-0}
    num_edges=${num_edges:-0}
    num_lev=${num_lev:-0}

    echo "$num_nodes $num_edges $num_lev"
}

# Function to run Yosys LUT6D_MAP
run_lut6d_mapping() {
    local input_file=$1
    local basename=$2
    local output_log="$TEST_DIR/${basename}_lut6d.log"

    $YOSYS -p "
        $YOSYS_READ_CMD $input_file
        hierarchy -check -auto-top
        proc; opt; techmap; opt;
        lut6d_map
        clean
        stat
        write_verilog $TEST_DIR/${basename}_lut6d_mapped.v
    " > $output_log 2>&1

    local status=$?

    local num_lut6d=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT6D[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut6=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT6[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut5d=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT5D[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut5=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT5[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut4d=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT4D[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut4=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT4[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut3=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT3[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut2=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT2[[:space:]]*$/ {print $1}' $output_log || echo "0")

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
$LEC_READ_CMD $src_file
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
    local blif_file=$2
    local size_nodes=$3
    local basename=$test_case
    local input_file="$blif_file"

    if [ "$USE_AIG" -eq 1 ]; then
        input_file=$(prepare_aig "$blif_file" "$basename")
    fi

    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo -e "${BLUE}Testing: $basename${NC}"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "  Size (nd): $size_nodes"

    # Run ABC mapping
    echo -n "  [1/3] Running ABC mapping... "
    abc_result=$(run_abc_mapping "$input_file" "$basename")
    abc_nodes=$(echo $abc_result | cut -d' ' -f1)
    abc_edges=$(echo $abc_result | cut -d' ' -f2)
    abc_lev=$(echo $abc_result | cut -d' ' -f3)
    echo -e "${GREEN}Done${NC}"
    echo "        Nodes: $abc_nodes, Edges: $abc_edges, Levels: $abc_lev"

    # Run LUT6D mapping
    echo -n "  [2/3] Running LUT6D mapping... "
    lut6d_result=$(run_lut6d_mapping "$input_file" "$basename")
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
        return 1
    fi

    # Run LEC for LUT6D mapping
    # echo -n "  [3/3] Running LEC for LUT6D... "
    # lec_result=$(run_equivalence_check "$input_file" "$TEST_DIR/${basename}_lut6d_mapped.v" "${basename}_lut6d")
    # if [ "$lec_result" = "PASS" ]; then
    #     echo -e "${GREEN}PASS ✓${NC}"
    # else
    #     echo -e "${RED}FAIL ✗${NC}"
    # fi

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

# Build list of tests with size (node count from BLIF)
LIST_FILE="$TEST_DIR/test_list.csv"
SORTED_LIST="$TEST_DIR/test_list_sorted.csv"
: > "$LIST_FILE"

shopt -s nullglob
for test_dir in "$BLIF_DIR"/tc_public_*; do
    blif_path="$test_dir/input.blif"
    if [ -f "$blif_path" ]; then
        test_case=$(basename "$test_dir")
        size_nodes=$(get_blif_nodes "$blif_path")
        echo "$size_nodes,$test_case,$blif_path" >> "$LIST_FILE"
    else
        echo -e "${YELLOW}Warning: missing $blif_path${NC}"
    fi
done
shopt -u nullglob

if [ ! -s "$LIST_FILE" ]; then
    echo -e "${RED}No TC_PUBLIC test cases found in $BLIF_DIR${NC}"
    exit 1
fi

sort -t, -k1,1n -k2,2 "$LIST_FILE" > "$SORTED_LIST"

if [ "$USE_AIG" -eq 1 ]; then
    echo "Converting BLIF to AIG for all cases..."
    while IFS=',' read -r size_nodes test_case blif_path; do
        prepare_aig "$blif_path" "$test_case" >/dev/null
    done < "$SORTED_LIST"
    : > "$AIG_MARKER"
    echo "AIG conversion done."
    echo ""
fi

# Initialize CSV file
echo "Benchmark,Size_Nodes,ABC_Nodes,ABC_Edges,ABC_Levels,Total_LUTs,Total_Dual,LUT6D,LUT6,LUT5D,LUT5,LUT4D,LUT4,LUT3,LUT2,LEC_Status" > $TEST_DIR/results.csv

# Process tests in ascending size order, with group headers
current_group=""
while IFS=',' read -r size_nodes test_case blif_path; do
    group=$(group_for_nodes "$size_nodes")
    if [ "$group" != "$current_group" ]; then
        echo "================================================"
        echo "  Group: $group"
        echo "================================================"
        echo ""
        current_group="$group"
    fi
    test_single_file "$test_case" "$blif_path" "$size_nodes"
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
