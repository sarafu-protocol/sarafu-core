#!/bin/bash
# Dependency installation script for Ubuntu/Debian
# This script installs all required dependencies for building Sarafu blockchain

set -e  # Exit on error

echo "=========================================="
echo "Sarafu Blockchain Dependency Installer"
echo "Platform: Ubuntu/Debian"
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

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    print_error "Please do not run this script as root. Use sudo when prompted."
    exit 1
fi

# Check Ubuntu/Debian version
if [ -f /etc/os-release ]; then
    . /etc/os-release
    print_info "Detected OS: $NAME $VERSION"
else
    print_error "Cannot detect OS version. This script is for Ubuntu/Debian only."
    exit 1
fi

# Update package list
print_info "Updating package list..."
sudo apt-get update || {
    print_error "Failed to update package list. Check your internet connection."
    exit 1
}

# Install build essentials
print_info "Installing build essentials..."
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    autoconf \
    automake \
    libtool \
    curl \
    wget \
    unzip || {
    print_error "Failed to install build essentials."
    exit 1
}

# Install Boost
print_info "Installing Boost libraries (>= 1.70)..."
sudo apt-get install -y \
    libboost-all-dev || {
    print_error "Failed to install Boost. Required version: >= 1.70"
    exit 1
}

# Verify Boost version
BOOST_VERSION=$(dpkg -s libboost-dev | grep '^Version:' | awk '{print $2}' | cut -d'.' -f1,2)
print_info "Installed Boost version: $BOOST_VERSION"

# Install libsodium
print_info "Installing libsodium (>= 1.0.18)..."
sudo apt-get install -y \
    libsodium-dev || {
    print_error "Failed to install libsodium. Required version: >= 1.0.18"
    exit 1
}

# Install Protobuf (optional - can be built from source via CMake)
print_info "Installing Protobuf (optional - CMake can fetch if not found)..."
sudo apt-get install -y \
    libprotobuf-dev \
    protobuf-compiler || {
    print_info "Protobuf installation failed. CMake will fetch it automatically."
}

# Install gRPC (optional - can be built from source via CMake)
print_info "Installing gRPC (optional - CMake can fetch if not found)..."
sudo apt-get install -y \
    libgrpc++-dev \
    libgrpc-dev \
    protobuf-compiler-grpc || {
    print_info "gRPC installation failed. CMake will fetch it automatically."
}

# Install RocksDB (optional - can be built from source via CMake)
print_info "Installing RocksDB (optional - CMake can fetch if not found)..."
sudo apt-get install -y \
    librocksdb-dev || {
    print_info "RocksDB installation failed. CMake will fetch it automatically."
}

# Install additional dependencies
print_info "Installing additional dependencies..."
sudo apt-get install -y \
    libssl-dev \
    zlib1g-dev \
    libbz2-dev \
    liblz4-dev \
    libzstd-dev \
    libsnappy-dev \
    libgflags-dev || {
    print_error "Failed to install additional dependencies."
    exit 1
}

# Install Python3 (for some build scripts)
print_info "Installing Python3..."
sudo apt-get install -y \
    python3 \
    python3-pip || {
    print_error "Failed to install Python3."
    exit 1
}

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

# Check GCC
if command -v g++ &> /dev/null; then
    GCC_VERSION=$(g++ --version | head -n1 | awk '{print $3}')
    print_success "GCC installed: $GCC_VERSION"
else
    print_error "GCC not found."
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

echo ""
print_success "All required dependencies installed successfully!"
echo ""
print_info "Optional dependencies (Protobuf, gRPC, RocksDB) will be fetched by CMake if not found."
print_info "You can now build the project with:"
echo "  mkdir -p build && cd build"
echo "  cmake .."
echo "  make -j\$(nproc)"
echo ""
