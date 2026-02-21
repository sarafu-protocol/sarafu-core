#!/bin/bash
# Check if code coverage meets the minimum threshold
#
# This script checks if the code coverage percentage meets
# the required threshold (default: 80%)
#
# Usage: ./check-coverage.sh [threshold]
#   threshold: Minimum coverage percentage (default: 80)
#
# Exit codes:
#   0: Coverage meets or exceeds threshold
#   1: Coverage below threshold or error

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
COVERAGE_INFO="coverage/coverage.info"
THRESHOLD=${1:-80}

echo -e "${GREEN}=== Code Coverage Threshold Check ===${NC}"
echo ""

# Check if coverage data exists
if [ ! -f "${COVERAGE_INFO}" ]; then
    echo -e "${RED}Error: Coverage data not found at ${COVERAGE_INFO}${NC}"
    echo "Please run ./scripts/generate-coverage.sh first"
    exit 1
fi

# Check for lcov
if ! command -v lcov &> /dev/null; then
    echo -e "${RED}Error: lcov not found${NC}"
    exit 1
fi

# Get coverage summary
echo "Analyzing coverage data..."
SUMMARY=$(lcov --summary ${COVERAGE_INFO} 2>&1)

# Extract line coverage percentage
LINE_COVERAGE=$(echo "$SUMMARY" | grep "lines" | awk '{print $2}' | sed 's/%//')

# Extract function coverage percentage
FUNCTION_COVERAGE=$(echo "$SUMMARY" | grep "functions" | awk '{print $2}' | sed 's/%//')

# Extract branch coverage percentage (if available)
BRANCH_COVERAGE=$(echo "$SUMMARY" | grep "branches" | awk '{print $2}' | sed 's/%//' || echo "N/A")

echo ""
echo -e "${GREEN}=== Coverage Results ===${NC}"
echo "Line Coverage:     ${LINE_COVERAGE}%"
echo "Function Coverage: ${FUNCTION_COVERAGE}%"
if [ "$BRANCH_COVERAGE" != "N/A" ]; then
    echo "Branch Coverage:   ${BRANCH_COVERAGE}%"
fi
echo ""
echo "Threshold:         ${THRESHOLD}%"
echo ""

# Check if line coverage meets threshold
if [ -z "$LINE_COVERAGE" ]; then
    echo -e "${RED}Error: Could not extract coverage percentage${NC}"
    exit 1
fi

# Use bc for floating point comparison
if command -v bc &> /dev/null; then
    MEETS_THRESHOLD=$(echo "$LINE_COVERAGE >= $THRESHOLD" | bc -l)
else
    # Fallback to integer comparison if bc not available
    LINE_COVERAGE_INT=${LINE_COVERAGE%.*}
    THRESHOLD_INT=${THRESHOLD%.*}
    if [ "$LINE_COVERAGE_INT" -ge "$THRESHOLD_INT" ]; then
        MEETS_THRESHOLD=1
    else
        MEETS_THRESHOLD=0
    fi
fi

if [ "$MEETS_THRESHOLD" -eq 1 ]; then
    echo -e "${GREEN}✓ SUCCESS: Coverage ${LINE_COVERAGE}% meets threshold of ${THRESHOLD}%${NC}"
    exit 0
else
    DEFICIT=$(echo "$THRESHOLD - $LINE_COVERAGE" | bc -l)
    echo -e "${RED}✗ FAILURE: Coverage ${LINE_COVERAGE}% is below threshold of ${THRESHOLD}%${NC}"
    echo -e "${RED}  Need ${DEFICIT}% more coverage to meet threshold${NC}"
    echo ""
    echo "To improve coverage:"
    echo "  1. Review the HTML coverage report: coverage/html/index.html"
    echo "  2. Identify uncovered code paths"
    echo "  3. Add unit tests for uncovered functions and branches"
    echo "  4. Run ./scripts/generate-coverage.sh to regenerate the report"
    exit 1
fi
