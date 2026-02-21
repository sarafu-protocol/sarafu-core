#!/bin/bash
# Reproducible build verification script
# This script verifies that building the same source twice produces identical binaries

set -e  # Exit on error

echo "=========================================="
echo "Sarafu Blockchain Reproducible Build Verifier"
echo "=========================================="
echo ""

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored messages
print_error() {
    echo -e "${RED}ERROR: $1${NC}"
}

print_success() {
    echo -e "${GREEN}SUCCESS: $1${NC}"
}

print_info() {
    echo -e "${YELLOW}INFO: $1${NC}"
}

# Configuration
BUILD_DIR_1="build-verify-1"
BUILD_DIR_2="build-verify-2"
BINARIES=("sarafu-node" "sar")
LIBRARIES=("libsarafu_core.a")

# Parse command line arguments
CLEAN_AFTER=true
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-clean)
            CLEAN_AFTER=false
            shift
            ;;
        --verbose|-v)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --no-clean    Don't clean build directories after verification"
            echo "  --verbose,-v  Show detailed build output"
            echo "  --help,-h     Show this help message"
            echo ""
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Check if we're in the project root
if [ ! -f "CMakeLists.txt" ]; then
    print_error "CMakeLists.txt not found. Please run this script from the project root."
    exit 1
fi

# Check if git is available and get commit info
if command -v git &> /dev/null && git rev-parse --git-dir > /dev/null 2>&1; then
    GIT_COMMIT=$(git rev-parse --short HEAD)
    GIT_DIRTY=$(git diff --quiet || echo "-dirty")
    print_info "Git commit: $GIT_COMMIT$GIT_DIRTY"
    
    # Set SOURCE_DATE_EPOCH from git
    export SOURCE_DATE_EPOCH=$(git log -1 --format=%ct)
    print_info "SOURCE_DATE_EPOCH: $SOURCE_DATE_EPOCH ($(date -u -d @$SOURCE_DATE_EPOCH 2>/dev/null || date -u -r $SOURCE_DATE_EPOCH 2>/dev/null))"
else
    print_info "Not in a git repository. Using fixed SOURCE_DATE_EPOCH."
    export SOURCE_DATE_EPOCH=1609459200  # 2021-01-01 00:00:00 UTC
fi

# Function to build in a directory
build_in_directory() {
    local build_dir=$1
    local build_num=$2
    
    print_info "Building in $build_dir (Build #$build_num)..."
    
    # Clean and create build directory
    rm -rf "$build_dir"
    mkdir -p "$build_dir"
    cd "$build_dir"
    
    # Configure with reproducible build enabled
    if [ "$VERBOSE" = true ]; then
        cmake -DCMAKE_BUILD_TYPE=Release -DREPRODUCIBLE_BUILD=ON ..
    else
        cmake -DCMAKE_BUILD_TYPE=Release -DREPRODUCIBLE_BUILD=ON .. > /dev/null 2>&1
    fi
    
    if [ $? -ne 0 ]; then
        print_error "CMake configuration failed for $build_dir"
        cd ..
        return 1
    fi
    
    # Build
    if [ "$VERBOSE" = true ]; then
        make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    else
        make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4) > /dev/null 2>&1
    fi
    
    if [ $? -ne 0 ]; then
        print_error "Build failed for $build_dir"
        cd ..
        return 1
    fi
    
    cd ..
    print_success "Build #$build_num completed"
    return 0
}

# Perform first build
print_info "Starting first build..."
if ! build_in_directory "$BUILD_DIR_1" 1; then
    print_error "First build failed"
    exit 1
fi

# Small delay to ensure different timestamps if not reproducible
sleep 2

# Perform second build
print_info "Starting second build..."
if ! build_in_directory "$BUILD_DIR_2" 2; then
    print_error "Second build failed"
    exit 1
fi

# Compare binaries
echo ""
print_info "Comparing binaries..."
echo ""

ALL_IDENTICAL=true
COMPARISON_RESULTS=()

# Compare each binary
for binary in "${BINARIES[@]}"; do
    file1="$BUILD_DIR_1/$binary"
    file2="$BUILD_DIR_2/$binary"
    
    if [ ! -f "$file1" ]; then
        print_error "Binary not found: $file1"
        ALL_IDENTICAL=false
        continue
    fi
    
    if [ ! -f "$file2" ]; then
        print_error "Binary not found: $file2"
        ALL_IDENTICAL=false
        continue
    fi
    
    # Get file sizes
    size1=$(stat -f%z "$file1" 2>/dev/null || stat -c%s "$file1" 2>/dev/null)
    size2=$(stat -f%z "$file2" 2>/dev/null || stat -c%s "$file2" 2>/dev/null)
    
    # Compare using sha256sum
    if command -v sha256sum &> /dev/null; then
        hash1=$(sha256sum "$file1" | awk '{print $1}')
        hash2=$(sha256sum "$file2" | awk '{print $1}')
    elif command -v shasum &> /dev/null; then
        hash1=$(shasum -a 256 "$file1" | awk '{print $1}')
        hash2=$(shasum -a 256 "$file2" | awk '{print $1}')
    else
        print_error "No SHA256 tool found (sha256sum or shasum)"
        exit 1
    fi
    
    echo "Binary: $binary"
    echo "  Build 1: $hash1 ($size1 bytes)"
    echo "  Build 2: $hash2 ($size2 bytes)"
    
    if [ "$hash1" = "$hash2" ]; then
        print_success "  ✓ Identical"
        COMPARISON_RESULTS+=("$binary: IDENTICAL")
    else
        print_error "  ✗ Different"
        ALL_IDENTICAL=false
        COMPARISON_RESULTS+=("$binary: DIFFERENT")
        
        # Show byte-level differences if verbose
        if [ "$VERBOSE" = true ]; then
            echo "  Byte differences:"
            cmp -l "$file1" "$file2" | head -n 10
        fi
    fi
    echo ""
done

# Compare libraries
for library in "${LIBRARIES[@]}"; do
    file1="$BUILD_DIR_1/$library"
    file2="$BUILD_DIR_2/$library"
    
    if [ ! -f "$file1" ] || [ ! -f "$file2" ]; then
        continue
    fi
    
    # Get file sizes
    size1=$(stat -f%z "$file1" 2>/dev/null || stat -c%s "$file1" 2>/dev/null)
    size2=$(stat -f%z "$file2" 2>/dev/null || stat -c%s "$file2" 2>/dev/null)
    
    # Compare using sha256sum
    if command -v sha256sum &> /dev/null; then
        hash1=$(sha256sum "$file1" | awk '{print $1}')
        hash2=$(sha256sum "$file2" | awk '{print $1}')
    elif command -v shasum &> /dev/null; then
        hash1=$(shasum -a 256 "$file1" | awk '{print $1}')
        hash2=$(shasum -a 256 "$file2" | awk '{print $1}')
    fi
    
    echo "Library: $library"
    echo "  Build 1: $hash1 ($size1 bytes)"
    echo "  Build 2: $hash2 ($size2 bytes)"
    
    if [ "$hash1" = "$hash2" ]; then
        print_success "  ✓ Identical"
        COMPARISON_RESULTS+=("$library: IDENTICAL")
    else
        print_error "  ✗ Different"
        ALL_IDENTICAL=false
        COMPARISON_RESULTS+=("$library: DIFFERENT")
    fi
    echo ""
done

# Summary
echo "=========================================="
echo "VERIFICATION SUMMARY"
echo "=========================================="
echo ""

for result in "${COMPARISON_RESULTS[@]}"; do
    echo "  $result"
done

echo ""

if [ "$ALL_IDENTICAL" = true ]; then
    print_success "✓ All binaries are byte-identical!"
    print_success "✓ Reproducible build verification PASSED"
    RESULT=0
else
    print_error "✗ Some binaries differ!"
    print_error "✗ Reproducible build verification FAILED"
    echo ""
    echo "Possible causes:"
    echo "  - Timestamps embedded in binaries (__DATE__, __TIME__ macros)"
    echo "  - Absolute paths in debug information"
    echo "  - Non-deterministic code generation"
    echo "  - Build ID sections in ELF binaries"
    echo ""
    echo "Troubleshooting:"
    echo "  - Ensure REPRODUCIBLE_BUILD=ON is set in CMake"
    echo "  - Check for __DATE__ and __TIME__ usage in source code"
    echo "  - Verify compiler flags include -ffile-prefix-map and -fmacro-prefix-map"
    echo "  - Run with --verbose to see detailed differences"
    RESULT=1
fi

# Clean up build directories
if [ "$CLEAN_AFTER" = true ]; then
    print_info "Cleaning up build directories..."
    rm -rf "$BUILD_DIR_1" "$BUILD_DIR_2"
    print_info "Build directories removed"
else
    print_info "Build directories preserved: $BUILD_DIR_1, $BUILD_DIR_2"
fi

echo ""
exit $RESULT
