#!/bin/bash
# Performance Profiling Script for Sarafu Blockchain
# Uses perf and valgrind to identify performance bottlenecks

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
PROFILE_DIR="${PROFILE_DIR:-profile_results}"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Profiling tools
USE_PERF=true
USE_VALGRIND=true
USE_CALLGRIND=true

# Parse command line arguments
BENCHMARK_NAME=""
TOOL=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --benchmark)
            BENCHMARK_NAME="$2"
            shift 2
            ;;
        --tool)
            TOOL="$2"
            shift 2
            ;;
        --perf-only)
            USE_VALGRIND=false
            USE_CALLGRIND=false
            shift
            ;;
        --valgrind-only)
            USE_PERF=false
            USE_CALLGRIND=false
            shift
            ;;
        --callgrind-only)
            USE_PERF=false
            USE_VALGRIND=false
            shift
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --benchmark NAME    Profile specific benchmark (crypto_benchmarks, transaction_benchmarks, etc.)"
            echo "  --tool TOOL         Use specific profiling tool (perf, valgrind, callgrind)"
            echo "  --perf-only         Only run perf profiling"
            echo "  --valgrind-only     Only run valgrind memory profiling"
            echo "  --callgrind-only    Only run callgrind call graph profiling"
            echo "  --help              Show this help message"
            echo ""
            echo "Examples:"
            echo "  $0                                    # Profile all benchmarks with all tools"
            echo "  $0 --benchmark crypto_benchmarks      # Profile crypto benchmarks only"
            echo "  $0 --perf-only                        # Use perf only"
            echo "  $0 --tool perf                        # Use perf only (alternative syntax)"
            exit 0
            ;;
        *)
            echo -e "${RED}Error: Unknown option $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Override tool selection if --tool specified
if [ -n "$TOOL" ]; then
    USE_PERF=false
    USE_VALGRIND=false
    USE_CALLGRIND=false
    
    case $TOOL in
        perf)
            USE_PERF=true
            ;;
        valgrind)
            USE_VALGRIND=true
            ;;
        callgrind)
            USE_CALLGRIND=true
            ;;
        *)
            echo -e "${RED}Error: Unknown tool '$TOOL'${NC}"
            echo "Valid tools: perf, valgrind, callgrind"
            exit 1
            ;;
    esac
fi

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Sarafu Blockchain Performance Profiling${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error: Build directory '$BUILD_DIR' not found${NC}"
    echo "Please build the project first with: cmake -B build && cmake --build build"
    exit 1
fi

# Create profile directory
mkdir -p "$PROFILE_DIR"

# Check for profiling tools
echo -e "${BLUE}Checking for profiling tools...${NC}"

if [ "$USE_PERF" = true ]; then
    if command -v perf &> /dev/null; then
        echo -e "  ${GREEN}✓${NC} perf found"
    else
        echo -e "  ${YELLOW}⚠${NC} perf not found (install with: apt-get install linux-tools-generic)"
        USE_PERF=false
    fi
fi

if [ "$USE_VALGRIND" = true ] || [ "$USE_CALLGRIND" = true ]; then
    if command -v valgrind &> /dev/null; then
        echo -e "  ${GREEN}✓${NC} valgrind found"
    else
        echo -e "  ${YELLOW}⚠${NC} valgrind not found (install with: apt-get install valgrind)"
        USE_VALGRIND=false
        USE_CALLGRIND=false
    fi
fi

echo ""

# List of benchmark executables
if [ -n "$BENCHMARK_NAME" ]; then
    BENCHMARKS=("$BENCHMARK_NAME")
else
    BENCHMARKS=(
        "crypto_benchmarks"
        "transaction_benchmarks"
        "block_benchmarks"
        "database_benchmarks"
    )
fi

# Profile each benchmark
for BENCHMARK in "${BENCHMARKS[@]}"; do
    BENCHMARK_PATH="$BUILD_DIR/benchmarks/$BENCHMARK"
    
    if [ ! -f "$BENCHMARK_PATH" ]; then
        echo -e "${YELLOW}Warning: Benchmark '$BENCHMARK' not found at $BENCHMARK_PATH${NC}"
        echo -e "${YELLOW}Skipping...${NC}"
        echo ""
        continue
    fi
    
    echo -e "${GREEN}Profiling $BENCHMARK...${NC}"
    echo ""
    
    # Create benchmark-specific directory
    BENCHMARK_PROFILE_DIR="$PROFILE_DIR/${BENCHMARK}_${TIMESTAMP}"
    mkdir -p "$BENCHMARK_PROFILE_DIR"
    
    # 1. Perf profiling (CPU hotspots)
    if [ "$USE_PERF" = true ]; then
        echo -e "${BLUE}  Running perf profiling...${NC}"
        
        PERF_DATA="$BENCHMARK_PROFILE_DIR/perf.data"
        PERF_REPORT="$BENCHMARK_PROFILE_DIR/perf_report.txt"
        PERF_FLAMEGRAPH="$BENCHMARK_PROFILE_DIR/flamegraph.svg"
        
        # Record performance data
        if perf record -F 99 -g -o "$PERF_DATA" -- "$BENCHMARK_PATH" --benchmark_filter=".*" --benchmark_min_time=1.0 2>&1 | tee "$BENCHMARK_PROFILE_DIR/perf_output.log"; then
            echo -e "  ${GREEN}✓${NC} perf recording completed"
            
            # Generate report
            perf report -i "$PERF_DATA" --stdio > "$PERF_REPORT" 2>&1
            echo -e "  ${GREEN}✓${NC} perf report generated: $PERF_REPORT"
            
            # Generate flamegraph if flamegraph.pl is available
            if command -v flamegraph.pl &> /dev/null; then
                perf script -i "$PERF_DATA" | flamegraph.pl > "$PERF_FLAMEGRAPH" 2>&1
                echo -e "  ${GREEN}✓${NC} flamegraph generated: $PERF_FLAMEGRAPH"
            else
                echo -e "  ${YELLOW}⚠${NC} flamegraph.pl not found (install from: https://github.com/brendangregg/FlameGraph)"
            fi
        else
            echo -e "  ${RED}✗${NC} perf profiling failed"
        fi
        
        echo ""
    fi
    
    # 2. Valgrind memory profiling
    if [ "$USE_VALGRIND" = true ]; then
        echo -e "${BLUE}  Running valgrind memory profiling...${NC}"
        
        VALGRIND_LOG="$BENCHMARK_PROFILE_DIR/valgrind_memcheck.log"
        
        if valgrind --tool=memcheck \
            --leak-check=full \
            --show-leak-kinds=all \
            --track-origins=yes \
            --verbose \
            --log-file="$VALGRIND_LOG" \
            "$BENCHMARK_PATH" --benchmark_filter=".*" --benchmark_min_time=0.1 2>&1 | tee "$BENCHMARK_PROFILE_DIR/valgrind_output.log"; then
            echo -e "  ${GREEN}✓${NC} valgrind memory profiling completed: $VALGRIND_LOG"
        else
            echo -e "  ${RED}✗${NC} valgrind memory profiling failed"
        fi
        
        echo ""
    fi
    
    # 3. Callgrind call graph profiling
    if [ "$USE_CALLGRIND" = true ]; then
        echo -e "${BLUE}  Running callgrind call graph profiling...${NC}"
        
        CALLGRIND_OUT="$BENCHMARK_PROFILE_DIR/callgrind.out"
        CALLGRIND_REPORT="$BENCHMARK_PROFILE_DIR/callgrind_report.txt"
        
        if valgrind --tool=callgrind \
            --callgrind-out-file="$CALLGRIND_OUT" \
            --dump-instr=yes \
            --collect-jumps=yes \
            "$BENCHMARK_PATH" --benchmark_filter=".*" --benchmark_min_time=0.1 2>&1 | tee "$BENCHMARK_PROFILE_DIR/callgrind_output.log"; then
            echo -e "  ${GREEN}✓${NC} callgrind profiling completed: $CALLGRIND_OUT"
            
            # Generate human-readable report
            if command -v callgrind_annotate &> /dev/null; then
                callgrind_annotate "$CALLGRIND_OUT" > "$CALLGRIND_REPORT" 2>&1
                echo -e "  ${GREEN}✓${NC} callgrind report generated: $CALLGRIND_REPORT"
            fi
            
            # Suggest visualization tool
            echo -e "  ${BLUE}ℹ${NC} Visualize with: kcachegrind $CALLGRIND_OUT"
        else
            echo -e "  ${RED}✗${NC} callgrind profiling failed"
        fi
        
        echo ""
    fi
    
    echo -e "${GREEN}✓ Profiling completed for $BENCHMARK${NC}"
    echo -e "  Results saved to: $BENCHMARK_PROFILE_DIR"
    echo ""
done

echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Performance profiling completed!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "Profile results saved to: $PROFILE_DIR/"
echo ""
echo -e "${BLUE}Analysis recommendations:${NC}"
echo ""

if [ "$USE_PERF" = true ]; then
    echo -e "${BLUE}1. CPU Hotspots (perf):${NC}"
    echo "   - Review perf_report.txt for top CPU-consuming functions"
    echo "   - Look for functions with high 'self' percentage"
    echo "   - Check flamegraph.svg for visual call stack analysis"
    echo ""
fi

if [ "$USE_VALGRIND" = true ]; then
    echo -e "${BLUE}2. Memory Issues (valgrind):${NC}"
    echo "   - Review valgrind_memcheck.log for memory leaks"
    echo "   - Check for 'definitely lost' and 'possibly lost' blocks"
    echo "   - Investigate 'invalid read/write' errors"
    echo ""
fi

if [ "$USE_CALLGRIND" = true ]; then
    echo -e "${BLUE}3. Call Graph Analysis (callgrind):${NC}"
    echo "   - Review callgrind_report.txt for function call costs"
    echo "   - Identify expensive call paths"
    echo "   - Use kcachegrind for interactive visualization"
    echo ""
fi

echo -e "${BLUE}Top 5 optimization targets:${NC}"
echo "1. Functions with highest CPU time (perf report)"
echo "2. Functions called most frequently (callgrind)"
echo "3. Memory allocation hotspots (valgrind)"
echo "4. Lock contention points (perf with --call-graph)"
echo "5. I/O bottlenecks (database operations)"
echo ""

exit 0
