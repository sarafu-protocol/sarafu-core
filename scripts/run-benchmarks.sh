#!/bin/bash
# Benchmark Orchestration Script for Sarafu Blockchain
# Runs all benchmark executables and aggregates results

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
RESULTS_DIR="${RESULTS_DIR:-benchmark_results}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="${RESULTS_DIR}/benchmark_results_${TIMESTAMP}.json"

# Performance targets from whitepaper
TARGET_BLOCK_TIME=2.0  # seconds
TARGET_BLOCK_PROPAGATION=300  # milliseconds
TARGET_TPS=1000  # transactions per second
TARGET_MEMORY_GB=4  # GB per validator
TARGET_CPU_PERCENT=50  # average on 8-core systems
TARGET_DISK_IO_MBS=100  # MB/s sustained

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Sarafu Blockchain Benchmark Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory '$BUILD_DIR' not found${NC}"
    echo "Please build the project first with: cmake -B build && cmake --build build"
    exit 1
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

# Collect system information
echo -e "${BLUE}Collecting system information...${NC}"
HOSTNAME=$(hostname)
OS=$(uname -s)
KERNEL=$(uname -r)
CPU_MODEL=$(grep "model name" /proc/cpuinfo 2>/dev/null | head -1 | cut -d: -f2 | xargs || sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "Unknown")
CPU_CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo "Unknown")
TOTAL_RAM=$(free -h 2>/dev/null | grep Mem | awk '{print $2}' || sysctl -n hw.memsize 2>/dev/null | awk '{print $1/1024/1024/1024 " GB"}' || echo "Unknown")

echo "  Hostname: $HOSTNAME"
echo "  OS: $OS $KERNEL"
echo "  CPU: $CPU_MODEL"
echo "  Cores: $CPU_CORES"
echo "  RAM: $TOTAL_RAM"
echo ""

# List of benchmark executables
BENCHMARKS=(
    "crypto_benchmarks"
    "transaction_benchmarks"
    "block_benchmarks"
    "database_benchmarks"
)

# Initialize results JSON
cat > "$RESULTS_FILE" <<EOF
{
  "timestamp": "$TIMESTAMP",
  "system_info": {
    "hostname": "$HOSTNAME",
    "os": "$OS",
    "kernel": "$KERNEL",
    "cpu_model": "$CPU_MODEL",
    "cpu_cores": "$CPU_CORES",
    "total_ram": "$TOTAL_RAM"
  },
  "performance_targets": {
    "block_time_seconds": $TARGET_BLOCK_TIME,
    "block_propagation_ms": $TARGET_BLOCK_PROPAGATION,
    "transaction_throughput_tps": $TARGET_TPS,
    "memory_usage_gb": $TARGET_MEMORY_GB,
    "cpu_usage_percent": $TARGET_CPU_PERCENT,
    "disk_io_mbs": $TARGET_DISK_IO_MBS
  },
  "benchmarks": [
EOF

FIRST_BENCHMARK=true

# Run each benchmark
for BENCHMARK in "${BENCHMARKS[@]}"; do
    BENCHMARK_PATH="$BUILD_DIR/benchmarks/$BENCHMARK"
    
    if [ ! -f "$BENCHMARK_PATH" ]; then
        echo -e "${YELLOW}Warning: Benchmark '$BENCHMARK' not found at $BENCHMARK_PATH${NC}"
        echo -e "${YELLOW}Skipping...${NC}"
        echo ""
        continue
    fi
    
    echo -e "${GREEN}Running $BENCHMARK...${NC}"
    
    # Run benchmark with JSON output
    BENCHMARK_OUTPUT="${RESULTS_DIR}/${BENCHMARK}_${TIMESTAMP}.json"
    
    if "$BENCHMARK_PATH" --benchmark_format=json --benchmark_out="$BENCHMARK_OUTPUT"; then
        echo -e "${GREEN}✓ $BENCHMARK completed${NC}"
        
        # Add comma separator if not first benchmark
        if [ "$FIRST_BENCHMARK" = false ]; then
            echo "," >> "$RESULTS_FILE"
        fi
        FIRST_BENCHMARK=false
        
        # Extract benchmark name and append to results
        echo "    {" >> "$RESULTS_FILE"
        echo "      \"name\": \"$BENCHMARK\"," >> "$RESULTS_FILE"
        echo "      \"output_file\": \"$BENCHMARK_OUTPUT\"," >> "$RESULTS_FILE"
        echo "      \"status\": \"success\"" >> "$RESULTS_FILE"
        echo -n "    }" >> "$RESULTS_FILE"
    else
        echo -e "${RED}✗ $BENCHMARK failed${NC}"
        
        # Add comma separator if not first benchmark
        if [ "$FIRST_BENCHMARK" = false ]; then
            echo "," >> "$RESULTS_FILE"
        fi
        FIRST_BENCHMARK=false
        
        echo "    {" >> "$RESULTS_FILE"
        echo "      \"name\": \"$BENCHMARK\"," >> "$RESULTS_FILE"
        echo "      \"status\": \"failed\"" >> "$RESULTS_FILE"
        echo -n "    }" >> "$RESULTS_FILE"
    fi
    
    echo ""
done

# Close results JSON
cat >> "$RESULTS_FILE" <<EOF

  ]
}
EOF

echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Benchmark suite completed!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Results saved to: $RESULTS_FILE"
echo ""
echo -e "${BLUE}Next steps:${NC}"
echo "1. Review individual benchmark results in: $RESULTS_DIR/"
echo "2. Generate performance report: ./scripts/generate-performance-report.sh $RESULTS_FILE"
echo "3. Profile performance bottlenecks: ./scripts/profile-performance.sh"
echo ""

# Display summary
echo -e "${BLUE}Summary:${NC}"
SUCCESS_COUNT=0
FAIL_COUNT=0

for BENCHMARK in "${BENCHMARKS[@]}"; do
    BENCHMARK_PATH="$BUILD_DIR/benchmarks/$BENCHMARK"
    if [ -f "$BENCHMARK_PATH" ]; then
        BENCHMARK_OUTPUT="${RESULTS_DIR}/${BENCHMARK}_${TIMESTAMP}.json"
        if [ -f "$BENCHMARK_OUTPUT" ]; then
            SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
            echo -e "  ${GREEN}✓${NC} $BENCHMARK"
        else
            FAIL_COUNT=$((FAIL_COUNT + 1))
            echo -e "  ${RED}✗${NC} $BENCHMARK"
        fi
    fi
done

echo ""
echo "Total: $SUCCESS_COUNT succeeded, $FAIL_COUNT failed"
echo ""

if [ $FAIL_COUNT -gt 0 ]; then
    exit 1
fi

exit 0
