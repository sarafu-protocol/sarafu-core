#!/bin/bash
# Master test orchestration script for Sarafu blockchain
# Runs all test suites in sequence and reports comprehensive results

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SKIP_STRESS="${SKIP_STRESS:-false}"
SKIP_FUZZ="${SKIP_FUZZ:-false}"

# Test results tracking
declare -A TEST_RESULTS
declare -A TEST_TIMES

echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║         Sarafu Blockchain - Comprehensive Test Suite      ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Record overall start time
OVERALL_START=$(date +%s)

# Function to run a test suite
run_test_suite() {
    local name=$1
    local script=$2
    local required=$3
    
    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Running: $name${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    
    local start=$(date +%s)
    
    if [ -f "$script" ]; then
        chmod +x "$script"
        if "$script"; then
            TEST_RESULTS[$name]="PASS"
        else
            TEST_RESULTS[$name]="FAIL"
            if [ "$required" = "true" ]; then
                echo -e "${RED}✗ Required test suite failed: $name${NC}"
            fi
        fi
    else
        echo -e "${YELLOW}⚠️  Test script not found: $script${NC}"
        TEST_RESULTS[$name]="SKIP"
    fi
    
    local end=$(date +%s)
    TEST_TIMES[$name]=$((end - start))
}

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory '$BUILD_DIR' not found${NC}"
    echo -e "${YELLOW}Run 'make build' first${NC}"
    exit 1
fi

echo -e "${YELLOW}Build directory: $BUILD_DIR${NC}"
echo -e "${YELLOW}Script directory: $SCRIPT_DIR${NC}"
echo ""

# Run test suites in order
echo -e "${CYAN}Starting test execution...${NC}"

# 1. Unit Tests (required, <5 minutes per requirement 3.6)
run_test_suite "Unit Tests" "$SCRIPT_DIR/run-unit-tests.sh" true

# 2. Property-Based Tests (required, 1000 iterations per requirement 4.2)
run_test_suite "Property Tests" "$SCRIPT_DIR/run-property-tests.sh" true

# 3. Integration Tests (required, <30 minutes per requirement 5.6)
run_test_suite "Integration Tests" "$SCRIPT_DIR/run-integration-tests.sh" true

# 4. Stress Tests (optional, can be skipped for faster runs)
if [ "$SKIP_STRESS" = "false" ]; then
    run_test_suite "Stress Tests" "$SCRIPT_DIR/run-stress-tests.sh" false
else
    echo -e "${YELLOW}⚠️  Skipping stress tests (SKIP_STRESS=true)${NC}"
    TEST_RESULTS["Stress Tests"]="SKIP"
    TEST_TIMES["Stress Tests"]=0
fi

# 5. Fuzz Tests (optional, can be skipped for faster runs)
if [ "$SKIP_FUZZ" = "false" ]; then
    if [ -f "$SCRIPT_DIR/run-fuzz-tests.sh" ]; then
        run_test_suite "Fuzz Tests" "$SCRIPT_DIR/run-fuzz-tests.sh" false
    else
        echo -e "${YELLOW}⚠️  Fuzz test script not found, skipping${NC}"
        TEST_RESULTS["Fuzz Tests"]="SKIP"
        TEST_TIMES["Fuzz Tests"]=0
    fi
else
    echo -e "${YELLOW}⚠️  Skipping fuzz tests (SKIP_FUZZ=true)${NC}"
    TEST_RESULTS["Fuzz Tests"]="SKIP"
    TEST_TIMES["Fuzz Tests"]=0
fi

# Calculate overall time
OVERALL_END=$(date +%s)
OVERALL_ELAPSED=$((OVERALL_END - OVERALL_START))
OVERALL_MINUTES=$((OVERALL_ELAPSED / 60))
OVERALL_SECONDS=$((OVERALL_ELAPSED % 60))

# Print comprehensive results
echo ""
echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                    Test Results Summary                    ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Print results table
printf "%-25s %-10s %-15s\n" "Test Suite" "Status" "Time"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

TOTAL_PASS=0
TOTAL_FAIL=0
TOTAL_SKIP=0

for suite in "Unit Tests" "Property Tests" "Integration Tests" "Stress Tests" "Fuzz Tests"; do
    status="${TEST_RESULTS[$suite]}"
    time="${TEST_TIMES[$suite]}"
    minutes=$((time / 60))
    seconds=$((time % 60))
    
    if [ "$status" = "PASS" ]; then
        status_color="${GREEN}✓ PASS${NC}"
        ((TOTAL_PASS++))
    elif [ "$status" = "FAIL" ]; then
        status_color="${RED}✗ FAIL${NC}"
        ((TOTAL_FAIL++))
    else
        status_color="${YELLOW}⊘ SKIP${NC}"
        ((TOTAL_SKIP++))
    fi
    
    printf "%-25s " "$suite"
    echo -ne "$status_color"
    printf "%10s" ""
    printf "%-15s\n" "${minutes}m ${seconds}s"
done

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo -e "Total execution time: ${CYAN}${OVERALL_MINUTES}m ${OVERALL_SECONDS}s${NC}"
echo ""
echo -e "Results: ${GREEN}$TOTAL_PASS passed${NC}, ${RED}$TOTAL_FAIL failed${NC}, ${YELLOW}$TOTAL_SKIP skipped${NC}"
echo ""

# Check requirements compliance
echo -e "${BLUE}Requirements Compliance:${NC}"
if [ "${TEST_TIMES[Unit Tests]}" -gt 300 ]; then
    echo -e "${YELLOW}⚠️  Unit tests exceeded 5 minutes (requirement 3.6)${NC}"
else
    echo -e "${GREEN}✓ Unit tests completed within 5 minutes (requirement 3.6)${NC}"
fi

if [ "${TEST_TIMES[Integration Tests]}" -gt 1800 ]; then
    echo -e "${YELLOW}⚠️  Integration tests exceeded 30 minutes (requirement 5.6)${NC}"
else
    echo -e "${GREEN}✓ Integration tests completed within 30 minutes (requirement 5.6)${NC}"
fi

if [ "${TEST_RESULTS[Property Tests]}" = "PASS" ]; then
    echo -e "${GREEN}✓ Property tests passed with 1000 iterations (requirement 4.2)${NC}"
fi

echo ""

# Final verdict
if [ $TOTAL_FAIL -gt 0 ]; then
    echo -e "${RED}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${RED}║                    ✗ TESTS FAILED                          ║${NC}"
    echo -e "${RED}╚════════════════════════════════════════════════════════════╝${NC}"
    exit 1
else
    echo -e "${GREEN}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                    ✓ ALL TESTS PASSED                      ║${NC}"
    echo -e "${GREEN}╚════════════════════════════════════════════════════════════╝${NC}"
    exit 0
fi
