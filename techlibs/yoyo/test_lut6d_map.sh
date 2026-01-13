#!/bin/bash
# Test script for lut6d_map pass

set -e

YOSYS="../../yosys"
TEST_DIR="test_output"
AIG_DIR="../../tests/aig"

mkdir -p $TEST_DIR

echo "=== Testing LUT6D_MAP Pass ==="
echo ""

# Function to test a single AIG file
test_aig_file() {
    local aig_file=$1
    local basename=$(basename "$aig_file" .aig)
    local output_name="${basename}_mapped"
    
    echo "Testing: $basename"
    
    # Run mapping
    $YOSYS -p "
        read_aiger $aig_file
        hierarchy -check -auto-top
        proc; opt; techmap; opt;
        logger -debug
        lut6d_map
        stat
        write_verilog $TEST_DIR/${output_name}.v
    " > $TEST_DIR/${basename}.log 2>&1
    
    local status=$?
    if [ $status -eq 0 ]; then
        # Extract statistics
        local num_gates=$(grep -o "Number of cells:" $TEST_DIR/${basename}.log | wc -l || echo "0")
        local num_wires=$(grep -o "Number of wires:" $TEST_DIR/${basename}.log | wc -l || echo "0")
        echo "  ✓ Success - Output: $TEST_DIR/${output_name}.v"
        echo "    Log: $TEST_DIR/${basename}.log"
    else
        echo "  ✗ Failed - Check log: $TEST_DIR/${basename}.log"
    fi
    echo ""
}

# Test Group 1: Simple combinational circuits
echo "=== Test Group 1: Simple Combinational Circuits ==="
echo ""

SIMPLE_TESTS=(
    "example_mine.aig"
    # "toggle-re.aig"
    "dec.aig"
    "ctrl.aig"
    "int2float.aig"
    "router.aig"
)

for test_file in "${SIMPLE_TESTS[@]}"; do
    aig_path="$AIG_DIR/$test_file"
    if [ -f "$aig_path" ]; then
        test_aig_file "$aig_path"
    else
        echo "Skipping $test_file (not found)"
        echo ""
    fi
done

# Test Group 2: Arithmetic circuits
echo ""

ARITH_TESTS=(
    "cavlc.aig"
    "priority.aig"
    "adder.aig"
    "i2c.aig"
    "bar.aig"
    "max.aig"
    "sin.aig"
)

for test_file in "${ARITH_TESTS[@]}"; do
    aig_path="$AIG_DIR/$test_file"
    if [ -f "$aig_path" ]; then
        test_aig_file "$aig_path"
    else
        echo "Skipping $test_file (not found)"
        echo ""
    fi
done

# Test Group 3: Larger benchmarks (with timeout)
echo "=== Test Group 3: Larger Benchmarks ==="
echo ""

LARGE_TESTS=(
    # "arbiter.aig"
    # "voter.aig"
    # "square.aig"
    # "sqrt.aig"
    # "multiplier.aig"
    # "log2.aig"
    # "mem_ctrl.aig"
    # "div.aig"
    # "hyp.aig"
)

for test_file in "${LARGE_TESTS[@]}"; do
    aig_path="$AIG_DIR/$test_file"
    if [ -f "$aig_path" ]; then
        basename=$(basename "$test_file" .aig)
        echo "Testing: $basename (large circuit)"
        
        # Use gtimeout on macOS or timeout on Linux
        TIMEOUT_CMD="timeout"
        if ! command -v timeout &> /dev/null; then
            if command -v gtimeout &> /dev/null; then
                TIMEOUT_CMD="gtimeout"
            else
                # No timeout available, run without it
                TIMEOUT_CMD=""
            fi
        fi
        
        if [ -n "$TIMEOUT_CMD" ]; then
            $TIMEOUT_CMD 600s $YOSYS -p "
                read_aiger $aig_path
                log -debug
                hierarchy -check -auto-top
                lut6d_map
                stat
                write_verilog $TEST_DIR/${basename}_mapped.v
            " > $TEST_DIR/${basename}.log 2>&1
            
            status=$?
            if [ $status -eq 0 ]; then
                echo "  ✓ Success - Log: $TEST_DIR/${basename}.log"
            elif [ $status -eq 124 ]; then
                echo "  ⏱ Timeout (circuit too large)"
            else
                echo "  ✗ Failed - Check log: $TEST_DIR/${basename}.log"
            fi
        else
            # Run without timeout
            $YOSYS -p "
                read_aiger $aig_path
                log -debug
                hierarchy -check -auto-top
                lut6d_map
                stat
                write_verilog $TEST_DIR/${basename}_mapped.v
            " > $TEST_DIR/${basename}.log 2>&1
            
            status=$?
            if [ $status -eq 0 ]; then
                echo "  ✓ Success - Log: $TEST_DIR/${basename}.log"
            else
                echo "  ✗ Failed - Check log: $TEST_DIR/${basename}.log"
            fi
        fi
        echo ""
    fi
done


echo "=== Summary ==="
echo "All test results are in: $TEST_DIR/"
echo ""
echo "Successful tests:"
ls -1 $TEST_DIR/*.v 2>/dev/null | wc -l | xargs echo "  Verilog outputs:"
echo ""
echo "To view detailed debug output:"
echo "  cat $TEST_DIR/<test_name>.log | grep -A 10 'Generating cuts'"
echo ""
echo "To view all cut statistics:"
echo "  grep 'Generated.*cuts' $TEST_DIR/*.log"
echo ""
echo "To check mapping quality:"
echo "  grep 'Number of cells' $TEST_DIR/*.log"
