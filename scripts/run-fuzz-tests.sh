#!/bin/bash
# Script to run fuzz tests for 24 hours
# This script orchestrates long-running fuzzing campaigns to discover crashes and hangs

set -e

# Configuration
FUZZ_DURATION=${FUZZ_DURATION:-86400}  # 24 hours in seconds (can be overridden)
BUILD_DIR=${BUILD_DIR:-build}
CORPUS_DIR=${CORPUS_DIR:-fuzz_corpus}
ARTIFACTS_DIR=${ARTIFACTS_DIR:-fuzz_artifacts}
LOG_DIR=${LOG_DIR:-fuzz_logs}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Create directories
mkdir -p "$CORPUS_DIR"
mkdir -p "$ARTIFACTS_DIR"
mkdir -p "$LOG_DIR"

echo "========================================="
echo "Sarafu Blockchain Fuzz Testing Campaign"
echo "========================================="
echo "Duration: $FUZZ_DURATION seconds ($(($FUZZ_DURATION / 3600)) hours)"
echo "Build directory: $BUILD_DIR"
echo "Corpus directory: $CORPUS_DIR"
echo "Artifacts directory: $ARTIFACTS_DIR"
echo "Log directory: $LOG_DIR"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory $BUILD_DIR does not exist${NC}"
    echo "Please build the project with fuzzing enabled:"
    echo "  cmake -B build -DENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++"
    echo "  cmake --build build"
    exit 1
fi

# Find all fuzzer executables
FUZZERS=$(find "$BUILD_DIR/tests" -name "*_fuzzer" -type f 2>/dev/null || true)

if [ -z "$FUZZERS" ]; then
    echo -e "${RED}Error: No fuzzer executables found in $BUILD_DIR/tests${NC}"
    echo "Please build the project with fuzzing enabled:"
    echo "  cmake -B build -DENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++"
    echo "  cmake --build build"
    exit 1
fi

echo "Found fuzzers:"
for fuzzer in $FUZZERS; do
    echo "  - $(basename $fuzzer)"
done
echo ""

# Function to run a single fuzzer
run_fuzzer() {
    local fuzzer=$1
    local fuzzer_name=$(basename "$fuzzer")
    local corpus_dir="$CORPUS_DIR/$fuzzer_name"
    local log_file="$LOG_DIR/${fuzzer_name}.log"
    
    mkdir -p "$corpus_dir"
    
    echo -e "${YELLOW}Starting fuzzer: $fuzzer_name${NC}"
    echo "  Corpus: $corpus_dir"
    echo "  Log: $log_file"
    echo "  Duration: $FUZZ_DURATION seconds"
    
    # Run fuzzer with timeout
    # -max_total_time: Maximum time to run in seconds
    # -artifact_prefix: Where to save crash/hang artifacts
    # -print_final_stats: Print statistics at the end
    # -timeout: Timeout for a single test input (30 seconds)
    # -rss_limit_mb: Memory limit (4GB)
    "$fuzzer" \
        "$corpus_dir" \
        -max_total_time="$FUZZ_DURATION" \
        -artifact_prefix="$ARTIFACTS_DIR/${fuzzer_name}_" \
        -print_final_stats=1 \
        -timeout=30 \
        -rss_limit_mb=4096 \
        2>&1 | tee "$log_file"
    
    local exit_code=$?
    
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ $fuzzer_name completed successfully${NC}"
        return 0
    else
        echo -e "${RED}✗ $fuzzer_name found issues (exit code: $exit_code)${NC}"
        return 1
    fi
}

# Track results
declare -a FAILED_FUZZERS
declare -a PASSED_FUZZERS

# Run each fuzzer sequentially
for fuzzer in $FUZZERS; do
    fuzzer_name=$(basename "$fuzzer")
    
    echo ""
    echo "========================================="
    echo "Running: $fuzzer_name"
    echo "========================================="
    
    if run_fuzzer "$fuzzer"; then
        PASSED_FUZZERS+=("$fuzzer_name")
    else
        FAILED_FUZZERS+=("$fuzzer_name")
    fi
    
    echo ""
done

# Print summary
echo ""
echo "========================================="
echo "Fuzzing Campaign Summary"
echo "========================================="
echo "Total fuzzers: ${#FUZZERS[@]}"
echo -e "${GREEN}Passed: ${#PASSED_FUZZERS[@]}${NC}"
echo -e "${RED}Failed: ${#FAILED_FUZZERS[@]}${NC}"
echo ""

if [ ${#PASSED_FUZZERS[@]} -gt 0 ]; then
    echo "Passed fuzzers:"
    for fuzzer in "${PASSED_FUZZERS[@]}"; do
        echo -e "  ${GREEN}✓${NC} $fuzzer"
    done
    echo ""
fi

if [ ${#FAILED_FUZZERS[@]} -gt 0 ]; then
    echo "Failed fuzzers (found crashes/hangs):"
    for fuzzer in "${FAILED_FUZZERS[@]}"; do
        echo -e "  ${RED}✗${NC} $fuzzer"
    done
    echo ""
    echo "Check artifacts in: $ARTIFACTS_DIR"
    echo "Check logs in: $LOG_DIR"
    exit 1
fi

echo -e "${GREEN}All fuzzers completed successfully!${NC}"
echo ""
echo "Corpus saved in: $CORPUS_DIR"
echo "Logs saved in: $LOG_DIR"
exit 0
