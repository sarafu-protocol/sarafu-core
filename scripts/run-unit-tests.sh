#!/bin/bash
# Run unit tests for Sarafu blockchain
# This script runs all unit tests and reports results

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
TIMEOUT="${TIMEOUT:-300}" # 5 minutes timeout per requirement 3.6

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Running Unit Tests${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory '$BUILD_DIR' not found${NC}"
    echo -e "${YELLOW}Run 'make build' first${NC}"
    exit 1
fi

# Check if unit tests executable exists
if [ ! -f "$BUILD_DIR/tests/unit_tests" ]; then
    echo -e "${RED}Error: Unit tests executable not found${NC}"
    echo -e "${YELLOW}Build the project with tests enabled${NC}"
    exit 1
fi

# Record start time
START_TIME=$(date +%s)

# Run unit tests with timeout
echo -e "${YELLOW}Running unit tests (timeout: ${TIMEOUT}s)...${NC}"
echo ""

cd "$BUILD_DIR"

# Run tests with CTest for better output
if timeout "$TIMEOUT" ctest -R "unit_tests" --output-on-failure --verbose; then
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
echo -e "${BLUE}Unit Test Results${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Execution time: ${MINUTES}m ${SECONDS}s"

# Check if tests completed within 5 minutes (requirement 3.6)
if [ $ELAPSED -gt 300 ]; then
    echo -e "${YELLOW}⚠️  Warning: Unit tests took longer than 5 minutes (requirement 3.6)${NC}"
fi

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓ All unit tests passed${NC}"
    exit 0
elif [ $TEST_RESULT -eq 124 ]; then
    echo -e "${RED}✗ Unit tests timed out after ${TIMEOUT}s${NC}"
    exit 1
else
    echo -e "${RED}✗ Unit tests failed${NC}"
    exit 1
fi
