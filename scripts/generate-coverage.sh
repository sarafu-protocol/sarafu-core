#!/bin/bash
# Generate code coverage report using lcov/gcov
#
# This script:
# 1. Builds the project with coverage enabled
# 2. Runs all unit tests
# 3. Collects coverage data
# 4. Generates HTML coverage report
#
# Requirements: lcov, genhtml, gcov (or llvm-cov for Clang)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
COVERAGE_DIR="coverage"
COVERAGE_INFO="${COVERAGE_DIR}/coverage.info"
COVERAGE_HTML="${COVERAGE_DIR}/html"

echo -e "${GREEN}=== Sarafu Blockchain Code Coverage Report Generator ===${NC}"
echo ""

# Check for required tools
echo "Checking for required tools..."
if ! command -v lcov &> /dev/null; then
    echo -e "${RED}Error: lcov not found. Please install lcov:${NC}"
    echo "  Ubuntu/Debian: sudo apt-get install lcov"
    echo "  macOS: brew install lcov"
    exit 1
fi

if ! command -v genhtml &> /dev/null; then
    echo -e "${RED}Error: genhtml not found. Please install lcov (includes genhtml):${NC}"
    echo "  Ubuntu/Debian: sudo apt-get install lcov"
    echo "  macOS: brew install lcov"
    exit 1
fi

# Detect compiler
if command -v g++ &> /dev/null; then
    COMPILER="g++"
    GCOV_TOOL="gcov"
elif command -v clang++ &> /dev/null; then
    COMPILER="clang++"
    GCOV_TOOL="llvm-cov gcov"
else
    echo -e "${RED}Error: No suitable C++ compiler found${NC}"
    exit 1
fi

echo -e "${GREEN}Using compiler: ${COMPILER}${NC}"
echo -e "${GREEN}Using coverage tool: ${GCOV_TOOL}${NC}"
echo ""

# Clean previous coverage data
echo "Cleaning previous coverage data..."
rm -rf ${COVERAGE_DIR}
mkdir -p ${COVERAGE_DIR}

# Clean and rebuild with coverage enabled
echo "Building project with coverage enabled..."
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

cmake -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_COVERAGE=ON \
      -DCMAKE_CXX_COMPILER=${COMPILER} \
      ..

make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

echo -e "${GREEN}Build successful${NC}"
echo ""

# Run tests
echo "Running unit tests..."
cd tests
ctest --output-on-failure

if [ $? -ne 0 ]; then
    echo -e "${YELLOW}Warning: Some tests failed. Coverage report may be incomplete.${NC}"
fi

cd ../..
echo ""

# Capture coverage data
echo "Capturing coverage data..."
lcov --capture \
     --directory ${BUILD_DIR} \
     --output-file ${COVERAGE_INFO} \
     --gcov-tool "${GCOV_TOOL}" \
     --rc lcov_branch_coverage=1

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to capture coverage data${NC}"
    exit 1
fi

# Remove coverage data for external libraries and test files
echo "Filtering coverage data..."
lcov --remove ${COVERAGE_INFO} \
     '/usr/*' \
     '*/build/*' \
     '*/tests/*' \
     '*/examples/*' \
     '*/googletest/*' \
     '*/protobuf/*' \
     '*/grpc/*' \
     '*/rocksdb/*' \
     '*/libsodium/*' \
     '*/blst/*' \
     '*/blake3/*' \
     '*/boost/*' \
     --output-file ${COVERAGE_INFO} \
     --rc lcov_branch_coverage=1

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to filter coverage data${NC}"
    exit 1
fi

# Generate HTML report
echo "Generating HTML coverage report..."
genhtml ${COVERAGE_INFO} \
        --output-directory ${COVERAGE_HTML} \
        --title "Sarafu Blockchain Code Coverage" \
        --legend \
        --show-details \
        --rc lcov_branch_coverage=1

if [ $? -ne 0 ]; then
    echo -e "${RED}Failed to generate HTML report${NC}"
    exit 1
fi

# Generate summary
echo ""
echo -e "${GREEN}=== Coverage Summary ===${NC}"
lcov --summary ${COVERAGE_INFO} --rc lcov_branch_coverage=1

# Extract coverage percentage
COVERAGE_PERCENT=$(lcov --summary ${COVERAGE_INFO} 2>&1 | grep "lines" | awk '{print $2}' | sed 's/%//')

echo ""
echo -e "${GREEN}Coverage report generated successfully!${NC}"
echo -e "HTML report: ${COVERAGE_HTML}/index.html"
echo ""

# Check if coverage meets threshold
THRESHOLD=80
if (( $(echo "$COVERAGE_PERCENT >= $THRESHOLD" | bc -l) )); then
    echo -e "${GREEN}✓ Coverage ${COVERAGE_PERCENT}% meets threshold of ${THRESHOLD}%${NC}"
    exit 0
else
    echo -e "${YELLOW}⚠ Coverage ${COVERAGE_PERCENT}% is below threshold of ${THRESHOLD}%${NC}"
    exit 0  # Don't fail, just warn
fi
