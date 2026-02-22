#!/bin/bash
# Run stress tests for Sarafu blockchain
# This script runs all stress tests under adversarial conditions

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
TIMEOUT="${TIMEOUT:-3600}" # 1 hour timeout for stress tests

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Running Stress Tests${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory '$BUILD_DIR' not found${NC}"
    echo -e "${YELLOW}Run 'make build' first${NC}"
    exit 1
fi

# Check if stress tests executable exists
if [ ! -f "$BUILD_DIR/tests/stress_tests" ]; then
    echo -e "${RED}Error: Stress tests executable not found${NC}"
    echo -e "${YELLOW}Build the project with stress tests enabled${NC}"
    exit 1
fi

# Record start time
START_TIME=$(date +%s)

# Run stress tests with timeout
echo -e "${YELLOW}Running stress tests (timeout: ${TIMEOUT}s)...${NC}"
echo -e "${YELLOW}⚠️  This may take a while - testing adversarial conditions${NC}"
echo ""

cd "$BUILD_DIR"

# Run tests with CTest for better output
if timeout "$TIMEOUT" ctest -R "stress_tests" --output-on-failure --verbose; then
    TEST_RESULT=0
else
    TEST_RESULT=$?
fi

# Calculate elapsed time
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
MINUTES=$((ELAPSED / 60))
SECONDS=$((ELAPSED % 60))

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Stress Test Results${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Execution time: ${MINUTES}m ${SECONDS}s"

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓ All stress tests passed${NC}"
    echo -e "${GREEN}✓ System maintains finality under adversarial conditions${NC}"
    exit 0
elif [ $TEST_RESULT -eq 124 ]; then
    echo -e "${RED}✗ Stress tests timed out after ${TIMEOUT}s${NC}"
    exit 1
else
    echo -e "${RED}✗ Stress tests failed${NC}"
    exit 1
fi
