#!/bin/bash
# Compare ABC mapping vs LUT6D_MAP mapping and perform equivalence checking

# SIMPLE_TESTS=(
#     "example_mine.aig"
#     "dec.aig"
#     "ctrl.aig"
# )

# MEDIUM_TESTS=(
#     "int2float.aig"
#     "router.aig"
#     "cavlc.aig"
#     "priority.aig"
#     "adder.aig"
#     "i2c.aig"
# )

# LARGE_TESTS=(
#     "bar.aig"
#     "max.aig"
#     "sin.aig"
# )

# VERY_LARGE_TESTS=(
#     "arbiter.aig"
#     "voter.aig"
#     "square.aig"
#     "sqrt.aig"
#     "multiplier.aig"
#     "log2.aig"
#     "mem_ctrl.aig"
#     "div.aig"
#     "hyp.aig"
# )

set -e

YOSYS="./yosys"
ABC="./yosys-abc"
TEST_DIR="test_output"
AIG_DIR="tests/aig"

mkdir -p $TEST_DIR

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "========================================"
echo "  ABC vs LUT6D_MAP Comparison & LEC"
echo "========================================"
echo ""

# Function to run ABC mapping
run_abc_mapping() {
    local aig_file=$1
    local basename=$2
    local output_log="$TEST_DIR/${basename}_abc.log"
    
    # Use -c for command line instead of -f for script file
    $ABC -c "read_aiger $aig_file; ps; if -K 6; ps; write_verilog $TEST_DIR/${basename}_abc_mapped.v" > $output_log 2>&1
    
    # Extract statistics (after mapping) - handle multi-line output
    # Get all lines after "if -K 6" and combine them
    local stats_text=$(grep -A 3 "if -K 6" $output_log | tail -2 | tr '\n' ' ')
    local num_nodes=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="nd" && $(i+1)=="=") print $(i+2)}' | tr -d ' ')
    local num_edges=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="edge" && $(i+1)=="=") print $(i+2)}' | tr -d ' ')
    local num_lev=$(echo "$stats_text" | awk '{for(i=1;i<=NF;i++) if($i=="lev" && $(i+1)=="=") print $(i+2)}' | tr -d ' ')
    
    # Default to 0 if empty
    num_nodes=${num_nodes:-0}
    num_edges=${num_edges:-0}
    num_lev=${num_lev:-0}
    
    echo "$num_nodes $num_edges $num_lev"
}

# Function to run Yosys LUT6D_MAP
run_lut6d_mapping() {
    local aig_file=$1
    local basename=$2
    local output_log="$TEST_DIR/${basename}_lut6d.log"
    
    $YOSYS -p "
        read_aiger $aig_file
        hierarchy -check -auto-top
        proc; opt; techmap; opt;
        lut6d_map
        clean
        stat
        write_verilog $TEST_DIR/${basename}_lut6d_mapped.v
    " > $output_log 2>&1
    
    local status=$?
    
    # Extract statistics from stat output - format is "       47   GTP_LUT6D"
    # Use awk to match the line and extract the first field (the count)
    local num_lut6d=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT6D[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut6=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT6[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut5d=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT5D[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut5=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT5[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut4d=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT4D[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut4=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT4[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut3=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT3[[:space:]]*$/ {print $1}' $output_log || echo "0")
    local num_lut2=$(awk '/^[[:space:]]*[0-9]+[[:space:]]+GTP_LUT2[[:space:]]*$/ {print $1}' $output_log || echo "0")
    
    # Default to 0 if empty
    num_lut6d=${num_lut6d:-0}
    num_lut6=${num_lut6:-0}
    num_lut5d=${num_lut5d:-0}
    num_lut5=${num_lut5:-0}
    num_lut4d=${num_lut4d:-0}
    num_lut4=${num_lut4:-0}
    num_lut3=${num_lut3:-0}
    num_lut2=${num_lut2:-0}
    
    # Calculate total LUTs (include all single and dual-output LUTs)
    local total_luts=$((num_lut6d + num_lut6 + num_lut5d + num_lut5 + num_lut4d + num_lut4 + num_lut3 + num_lut2))
    local total_dual=$((num_lut6d + num_lut5d + num_lut4d))
    
    echo "$status $total_luts $total_dual $num_lut6d $num_lut6 $num_lut5d $num_lut5 $num_lut4d $num_lut4 $num_lut3 $num_lut2"
}

# Function to run equivalence checking
run_equivalence_check() {
    local aig_file=$1
    local mapped_file=$2
    local basename=$3
    local lec_log="$TEST_DIR/${basename}_lec.log"
    
    # Create temporary LEC script
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
read_aiger $aig_file
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
    
    local status=$?
    
    # Check if equivalence passed
    if grep -q "Equivalence successfully proven" $lec_log; then
        echo "PASS"
    elif grep -q "proven" $lec_log && ! grep -q "failed" $lec_log; then
        echo "PASS"
    else
        echo "FAIL"
    fi
}

# Function to test a single AIG file
test_single_file() {
    local aig_file=$1
    local basename=$(basename "$aig_file" .aig)
    
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo -e "${BLUE}Testing: $basename${NC}"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    # Run ABC mapping
    echo -n "  [1/3] Running ABC mapping... "
    abc_result=$(run_abc_mapping "$aig_file" "$basename")
    abc_nodes=$(echo $abc_result | cut -d' ' -f1)
    abc_edges=$(echo $abc_result | cut -d' ' -f2)
    abc_lev=$(echo $abc_result | cut -d' ' -f3)
    echo -e "${GREEN}Done${NC}"
    echo "        Nodes: $abc_nodes, Edges: $abc_edges, Levels: $abc_lev"
    
    # Run LUT6D mapping
    echo -n "  [2/3] Running LUT6D mapping... "
    lut6d_result=$(run_lut6d_mapping "$aig_file" "$basename")
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
    echo -n "  [3/3] Running LEC for LUT6D... "
    lec_result=$(run_equivalence_check "$aig_file" "$TEST_DIR/${basename}_lut6d_mapped.v" "${basename}_lut6d")
    if [ "$lec_result" = "PASS" ]; then
        echo -e "${GREEN}PASS ✓${NC}"
    else
        echo -e "${RED}FAIL ✗${NC}"
    fi
    
    # Compare results
    echo ""
    echo "  Comparison:"
    echo "    ABC Nodes:         $abc_nodes"
    echo "    LUT6D Total LUTs:  $total_luts"
    
    # Calculate improvement
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
    echo "$basename,$abc_nodes,$abc_edges,$abc_lev,$total_luts,$total_dual,$num_lut6d,$num_lut6,$num_lut5d,$num_lut5,$num_lut4d,$num_lut4,$num_lut3,$num_lut2,$lec_result" >> $TEST_DIR/results.csv
    
    echo ""
}

# Initialize CSV file
echo "Benchmark,ABC_Nodes,ABC_Edges,ABC_Levels,Total_LUTs,Total_Dual,LUT6D,LUT6,LUT5D,LUT5,LUT4D,LUT4,LUT3,LUT2,LEC_Status" > $TEST_DIR/results.csv

# Test Groups
echo "================================================"
echo "  Test Group 1: Simple Combinational Circuits"
echo "================================================"
echo ""

SIMPLE_TESTS=(
    "example_mine.aig"
    "dec.aig"
    "ctrl.aig"
)

for test_file in "${SIMPLE_TESTS[@]}"; do
    aig_path="$AIG_DIR/$test_file"
    if [ -f "$aig_path" ]; then
        test_single_file "$aig_path"
    else
        echo -e "${YELLOW}Skipping $test_file (not found)${NC}"
        echo ""
    fi
done

echo ""
echo "================================================"
echo "  Test Group 2: Medium Circuits"
echo "================================================"
echo ""

MEDIUM_TESTS=(
    "int2float.aig"
    "router.aig"
    "cavlc.aig"
    "priority.aig"
    "adder.aig"
    "i2c.aig"
)

for test_file in "${MEDIUM_TESTS[@]}"; do
    aig_path="$AIG_DIR/$test_file"
    if [ -f "$aig_path" ]; then
        test_single_file "$aig_path"
    else
        echo -e "${YELLOW}Skipping $test_file (not found)${NC}"
        echo ""
    fi
done

echo ""
echo "================================================"
echo "  Test Group 3: Large Circuits"
echo "================================================"
echo ""

LARGE_TESTS=(
    "bar.aig"
    "max.aig"
    "sin.aig"
)

for test_file in "${LARGE_TESTS[@]}"; do
    aig_path="$AIG_DIR/$test_file"
    if [ -f "$aig_path" ]; then
        echo -e "${YELLOW}Testing large circuit (may take time)...${NC}"
        test_single_file "$aig_path"
    else
        echo -e "${YELLOW}Skipping $test_file (not found)${NC}"
        echo ""
    fi
done

echo ""
echo "================================================"
echo "  Test Group 4: Very Large Circuits"
echo "================================================"
echo ""

VERY_LARGE_TESTS=(
    "arbiter.aig"
    "voter.aig"
    "square.aig"
    "sqrt.aig"
    "multiplier.aig"
    "log2.aig"
    "mem_ctrl.aig"
    "div.aig"
    "hyp.aig"
)

for test_file in "${VERY_LARGE_TESTS[@]}"; do
    aig_path="$AIG_DIR/$test_file"
    if [ -f "$aig_path" ]; then
        echo -e "${YELLOW}Testing very large circuit (may take significant time)...${NC}"
        test_single_file "$aig_path"
    else
        echo -e "${YELLOW}Skipping $test_file (not found)${NC}"
        echo ""
    fi
done



# Generate summary report
echo ""
echo "================================================"
echo "  Summary Report"
echo "================================================"
echo ""

# Calculate statistics
total_tests=$(tail -n +2 $TEST_DIR/results.csv | wc -l | xargs)
passed_lec=$(grep ",PASS$" $TEST_DIR/results.csv | wc -l | xargs)
failed_lec=$(grep ",FAIL$" $TEST_DIR/results.csv | wc -l | xargs)

echo "Total tests:        $total_tests"
echo "LEC passed:         $passed_lec"
echo "LEC failed:         $failed_lec"
echo ""

# Calculate average improvement
echo "Detailed Results:"
echo ""
tail -n +2 $TEST_DIR/results.csv | while IFS=',' read -r bench abc_nodes abc_edges abc_lev lut_total total_dual lut6d lut6 lut5d lut5 lut4d lut4 lut3 lut2 lec; do
    if [ "$abc_nodes" -gt 0 ]; then
        diff=$((abc_nodes - lut_total))
        percent=$(awk "BEGIN {printf \"%.1f\", ($diff / $abc_nodes) * 100}")
        printf "  %-20s ABC:%4d  Total:%4d  Diff:%4d (%6s%%)  Dual:%d (6D:%d 5D:%d 4D:%d)  LEC:%s\n" \
               "$bench" "$abc_nodes" "$lut_total" "$diff" "$percent" "$total_dual" "$lut6d" "$lut5d" "$lut4d" "$lec"
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
