#!/bin/bash
# Run property-based tests for Sarafu blockchain
# This script runs all property tests with 1000 iterations each (requirement 4.2)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
ITERATIONS="${RC_PARAMS:-max_success=1000}"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Running Property-Based Tests${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Configuration: $ITERATIONS"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory '$BUILD_DIR' not found${NC}"
    echo -e "${YELLOW}Run 'make build' first${NC}"
    exit 1
fi

# Check if property tests executable exists
if [ ! -f "$BUILD_DIR/tests/property_tests" ]; then
    echo -e "${RED}Error: Property tests executable not found${NC}"
    echo -e "${YELLOW}Build the project with property tests enabled${NC}"
    exit 1
fi

# Record start time
START_TIME=$(date +%s)

# Run property tests
echo -e "${YELLOW}Running property tests (1000 iterations each)...${NC}"
echo -e "${YELLOW}⚠️  This may take several minutes${NC}"
echo ""

cd "$BUILD_DIR"

# Set RapidCheck parameters for 1000 iterations
export RC_PARAMS="max_success=1000"

# Run tests with CTest
if ctest -R "property_tests" --output-on-failure --verbose; then
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
echo -e "${BLUE}Property Test Results${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Execution time: ${MINUTES}m ${SECONDS}s"
echo -e "Iterations per test: 1000 (requirement 4.2)"

if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}✓ All property tests passed (100% success rate)${NC}"
    exit 0
else
    echo -e "${RED}✗ Property tests failed${NC}"
    echo -e "${YELLOW}Check output above for minimal failing examples${NC}"
    exit 1
fi
