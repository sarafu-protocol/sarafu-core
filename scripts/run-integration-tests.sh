#!/bin/bash
# Run integration tests for Sarafu blockchain
# This script runs all integration tests (requirement 5.6)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
TIMEOUT="${TIMEOUT:-1800}" # 30 minutes timeout per requirement 5.6

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Running Integration Tests${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory '$BUILD_DIR' not found${NC}"
    echo -e "${YELLOW}Run 'make build' first${NC}"
    exit 1
fi

# Check if integration tests executable exists
if [ ! -f "$BUILD_DIR/tests/integration_tests" ]; then
    echo -e "${RED}Error: Integration tests executable not found${NC}"
    echo -e "${YELLOW}Build the project with integration tests enabled${NC}"
    exit 1
fi

# Record start time
START_TIME=$(date +%s)

# Run integration tests with timeout
echo -e "${YELLOW}Running integration tests (timeout: ${TIMEOUT}s)...${NC}"
echo -e "${YELLOW}⚠️  This may take up to 30 minutes${NC}"
echo ""

cd "$BUILD_DIR"

# Run tests with CTest for better output
if timeout "$TIMEOUT" ctest -R "integration_tests" --output-on-failure --verbose; then
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
echo -e "${BLUE}Integration Test Results${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Execution time: ${MINUTES}m ${SECONDS}s"

# Check if tests completed within 30 minutes (requirement 5.6)
if [ $ELAPSED -gt 1800 ]; then
    echo -e "${YELLOW}⚠️  Warning: Integration tests took longer than 30 minutes (requirement 5.6)${NC}"
fi

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓ All integration tests passed${NC}"
    exit 0
elif [ $TEST_RESULT -eq 124 ]; then
    echo -e "${RED}✗ Integration tests timed out after ${TIMEOUT}s${NC}"
    exit 1
else
    echo -e "${RED}✗ Integration tests failed${NC}"
    exit 1
fi
