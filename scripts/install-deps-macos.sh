#!/bin/bash
# Dependency installation script for macOS
# This script installs all required dependencies for building Sarafu blockchain using Homebrew

set -e  # Exit on error

echo "=========================================="
echo "Sarafu Blockchain Dependency Installer"
echo "Platform: macOS"
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

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    print_error "This script is for macOS only."
    exit 1
fi

# Check macOS version
MACOS_VERSION=$(sw_vers -productVersion)
print_info "Detected macOS version: $MACOS_VERSION"

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    print_error "Homebrew is not installed."
    echo ""
    echo "Please install Homebrew first:"
    echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    echo ""
    exit 1
fi

print_success "Homebrew is installed"
BREW_VERSION=$(brew --version | head -n1)
print_info "$BREW_VERSION"

# Update Homebrew
print_info "Updating Homebrew..."
brew update || {
    print_error "Failed to update Homebrew. Check your internet connection."
    exit 1
}

# Install build essentials
print_info "Installing build essentials..."
brew install \
    cmake \
    git \
    pkg-config \
    autoconf \
    automake \
    libtool \
    wget || {
    print_error "Failed to install build essentials."
    exit 1
}

# Install Boost
print_info "Installing Boost libraries (>= 1.70)..."
brew install boost || {
    print_error "Failed to install Boost."
    exit 1
}

# Install libsodium
print_info "Installing libsodium (>= 1.0.18)..."
brew install libsodium || {
    print_error "Failed to install libsodium."
    exit 1
}

# Install Protobuf (optional - can be built from source via CMake)
print_info "Installing Protobuf (optional - CMake can fetch if not found)..."
brew install protobuf || {
    print_info "Protobuf installation failed. CMake will fetch it automatically."
}

# Install gRPC (optional - can be built from source via CMake)
print_info "Installing gRPC (optional - CMake can fetch if not found)..."
brew install grpc || {
    print_info "gRPC installation failed. CMake will fetch it automatically."
}

# Install RocksDB (optional - can be built from source via CMake)
print_info "Installing RocksDB (optional - CMake can fetch if not found)..."
brew install rocksdb || {
    print_info "RocksDB installation failed. CMake will fetch it automatically."
}

# Install additional dependencies
print_info "Installing additional dependencies..."
brew install \
    openssl \
    zlib \
    bzip2 \
    lz4 \
    zstd \
    snappy \
    gflags || {
    print_error "Failed to install additional dependencies."
    exit 1
}

# Install Python3 (for some build scripts)
print_info "Installing Python3..."
brew install python3 || {
    print_info "Python3 may already be installed."
}

# Install LLVM/Clang (if not already installed)
if ! command -v clang &> /dev/null; then
    print_info "Installing LLVM/Clang..."
    brew install llvm || {
        print_error "Failed to install LLVM/Clang."
        exit 1
    }
fi

# Verify installations
echo ""
print_info "Verifying installations..."

# Check CMake
if command -v cmake &> /dev/null; then
    CMAKE_VERSION=$(cmake --version | head -n1 | awk '{print $3}')
    print_success "CMake installed: $CMAKE_VERSION"
else
    print_error "CMake not found. Please install CMake >= 3.20"
    exit 1
fi

# Check Clang
if command -v clang++ &> /dev/null; then
    CLANG_VERSION=$(clang++ --version | head -n1 | awk '{print $4}')
    print_success "Clang installed: $CLANG_VERSION"
else
    print_error "Clang not found."
    exit 1
fi

# Check pkg-config
if command -v pkg-config &> /dev/null; then
    print_success "pkg-config installed"
else
    print_error "pkg-config not found."
    exit 1
fi

# Check libsodium
if pkg-config --exists libsodium; then
    SODIUM_VERSION=$(pkg-config --modversion libsodium)
    print_success "libsodium installed: $SODIUM_VERSION"
else
    print_error "libsodium not found via pkg-config."
    exit 1
fi

# Check Boost
if brew list boost &> /dev/null; then
    BOOST_VERSION=$(brew list --versions boost | awk '{print $2}')
    print_success "Boost installed: $BOOST_VERSION"
else
    print_error "Boost not found."
    exit 1
fi

echo ""
print_success "All required dependencies installed successfully!"
echo ""
print_info "Optional dependencies (Protobuf, gRPC, RocksDB) will be fetched by CMake if not found."
print_info "You can now build the project with:"
echo "  mkdir -p build && cd build"
echo "  cmake .."
echo "  make -j\$(sysctl -n hw.ncpu)"
echo ""
print_info "Note: On Apple Silicon (M1/M2), you may need to set:"
echo "  export CMAKE_OSX_ARCHITECTURES=arm64"
echo ""
