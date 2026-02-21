# Sarafu Blockchain Core

[![CI](https://github.com/sarafu-protocol/sarafu-core/actions/workflows/ci.yml/badge.svg)](https://github.com/sarafu-protocol/sarafu-core/actions/workflows/ci.yml)

A politically neutral, jurisdiction-resilient settlement protocol implementing HotStuff BFT consensus with deterministic Proof-of-Stake.

## Overview

Sarafu is engineered for low-friction cross-border payments with:

- **HotStuff BFT Consensus**: O(n) communication complexity, 2-second block time
- **Instant Finality**: Blocks finalized via Quorum Certificates (≥2/3 stake)
- **Light Client Efficiency**: ≤5 KB proofs using BLS12-381 signature aggregation
- **Economic Security**: Quadratic correlated slashing deterring cartel formation
- **Predictable Issuance**: Security-budget-targeted monetary policy (rt = k·σ)
- **Low-Bandwidth Optimized**: Designed for African infrastructure constraints

## Architecture

- **Language**: C++17
- **Consensus**: HotStuff BFT
- **State Model**: Account-based with nonce-based replay protection
- **Validator Signatures**: BLS12-381 (enables aggregation)
- **Transaction Signatures**: Ed25519 (fast, hardware wallet support)
- **Hashing**: Blake3 (3-5x faster than SHA-256)
- **Networking**: Libp2p with QUIC transport
- **Storage**: RocksDB for state persistence

## Building from Source

### Prerequisites

#### System Requirements

- C++17 compatible compiler (GCC ≥7, Clang ≥5, MSVC ≥2017)
- CMake ≥3.20
- Make or Ninja build system
- Git

#### Required Dependencies

1. **Boost** (≥1.70)

   ```bash
   # macOS
   brew install boost

   # Ubuntu/Debian
   sudo apt-get install libboost-all-dev

   # Fedora/RHEL
   sudo dnf install boost-devel
   ```

2. **libsodium** (for Ed25519)

   ```bash
   # macOS
   brew install libsodium

   # Ubuntu/Debian
   sudo apt-get install libsodium-dev

   # Fedora/RHEL
   sudo dnf install libsodium-devel
   ```

3. **pkg-config**

   ```bash
   # macOS
   brew install pkg-config

   # Ubuntu/Debian
   sudo apt-get install pkg-config

   # Fedora/RHEL
   sudo dnf install pkgconfig
   ```

#### Auto-Fetched Dependencies

The build system automatically fetches and builds:

- **blst**: BLS12-381 signatures
- **Blake3**: Cryptographic hashing

#### Optional Dependencies (for full system)

- **gRPC**: RPC interface
- **Protocol Buffers**: Message serialization
- **RocksDB**: State storage

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/sarafu-protocol/sarafu-core.git
cd sarafu-blockchain

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build . -j$(nproc)

# Run tests
ctest --output-on-failure
```

### Build Options

```bash
# Debug build with sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build with optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..

# With custom Boost location
cmake -DBOOST_ROOT=/path/to/boost ..

# Install to system
sudo cmake --install .
```

### Platform-Specific Instructions

#### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install dependencies
brew install cmake boost libsodium pkg-config

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu)
```

#### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    libsodium-dev \
    pkg-config

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

#### Linux (Fedora/RHEL)

```bash
# Install dependencies
sudo dnf install -y \
    gcc-c++ \
    cmake \
    boost-devel \
    libsodium-devel \
    pkgconfig

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Troubleshooting

#### Boost Not Found

```bash
# Specify Boost location
cmake -DBOOST_ROOT=/usr/local ..

# Or set environment variable
export BOOST_ROOT=/usr/local
cmake ..
```

#### libsodium Not Found

```bash
# Install libsodium
brew install libsodium  # macOS
sudo apt-get install libsodium-dev  # Ubuntu

# Or specify pkg-config path
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

#### Compiler Version Too Old

```bash
# Ubuntu: Install newer GCC
sudo apt-get install gcc-9 g++-9
export CC=gcc-9
export CXX=g++-9

# Or use Clang
sudo apt-get install clang-10
export CC=clang-10
export CXX=clang++-10
```

## Docker Deployment

### Prerequisites

- Docker 20.10+
- Docker Compose 1.29+

### Quick Start

```bash
# 1. Generate validator keys and genesis
sar keygen --output-dir ./docker/keys --count 10
sar genesis --validators 10 --keys-dir ./docker/keys --output ./docker/genesis.json

# 2. Start testnet
docker-compose -f docker/docker-compose.yml up -d

# 3. Check status
docker-compose -f docker/docker-compose.yml ps

# 4. View logs
docker logs sarafu-validator-1
```

### CLI Commands

The `sar` CLI tool provides all blockchain operations:

```bash
# Generate validator keys
sar keygen --output-dir ./keys --count 10

# Generate genesis file
sar genesis --validators 10 --keys-dir ./keys --output ./genesis.json

# Run a node
sar node --config config.toml

# Show version and help
sar version
sar help
```

See [CLI Usage Guide](CLI_USAGE.md) for complete documentation.

### Validator Endpoints

The testnet runs 10 validators with the following endpoints:

| Validator | P2P Port | gRPC Port | REST Port | WebSocket Port |
| --------- | -------- | --------- | --------- | -------------- |
| 1         | 26656    | 9090      | 8080      | 8081           |
| 2         | 26657    | 9091      | 8082      | 8083           |
| 3-10      | ...      | ...       | ...       | ...            |

### Managing the Testnet

```bash
# View all logs
docker-compose -f docker/docker-compose.yml logs

# Follow specific validator
docker logs -f sarafu-validator-1

# Stop testnet (preserves data)
docker-compose -f docker/docker-compose.yml down

# Clean restart (removes all data)
docker-compose -f docker/docker-compose.yml down -v
rm -rf docker/keys docker/genesis.json
sar keygen --output-dir ./docker/keys --count 10
sar genesis --validators 10 --keys-dir ./docker/keys --output ./docker/genesis.json
docker-compose -f docker/docker-compose.yml up -d
```

### Production Container

Build and run the minimal production container:

```bash
docker build -f docker/Dockerfile.prod -t sarafu-node .
docker run -d \
  -p 26656:26656 \
  -p 9090:9090 \
  -v sarafu-data:/data \
  -v $(pwd)/config.toml:/data/config.toml:ro \
  sarafu-node
```

## Configuration

The Sarafu CLI (`sar`) provides all blockchain operations:

```bash
# Generate validator keys
sar keygen --output-dir ./keys --count 10

# Generate genesis file
sar genesis --validators 10 --keys-dir ./keys --output ./genesis.json

# Run a node
sar node --config config.toml

# Show version
sar version

# Show help
sar help
```

For complete CLI documentation, see [CLI_USAGE.md](CLI_USAGE.md).

### Configuration Files

Configuration is loaded from TOML files with support for environment variable and command-line overrides.

### Example Configuration

```toml
# config.toml
[network]
listen_address = "0.0.0.0:26656"
bootstrap_peers = [
    "/ip4/seed1.sarafu.network/tcp/26656/p2p/12D3KooW...",
    "/ip4/seed2.sarafu.network/tcp/26656/p2p/12D3KooW..."
]

[rpc]
grpc_port = 9090
rest_port = 8080
websocket_port = 8081

[storage]
data_dir = "/var/lib/sarafu"

[validator]
enabled = false
consensus_key_path = "/etc/sarafu/consensus_key.bin"
withdrawal_key_path = "/etc/sarafu/withdrawal_key.bin"

[logging]
level = "info"
format = "json"
```

### Running a Node

```bash
# Full node
sar node --config config.toml

# Validator node
sar node --config config.toml --validator

# Override configuration
sar node --config config.toml --data-dir /custom/path
```

## Testing

### Continuous Integration

The project uses GitHub Actions for automated testing and code quality checks:

- **Code Audit**: Scans for TODO/FIXME/HACK comments in production code (blocking)
- **Build**: Compiles on Ubuntu and macOS with Debug and Release configurations
- **Unit Tests**: Runs all unit tests with coverage reporting
- **Code Coverage**: Generates coverage reports for pull requests

All pull requests must pass the code audit check before merging.

### Unit Tests

```bash
cd build
ctest --output-on-failure
```

### Property-Based Tests

Property-based tests validate universal correctness properties:

```bash
# Run with 1000 iterations per property
ctest -R property_test --output-on-failure
```

### Integration Tests

```bash
# Multi-validator consensus test
ctest -R integration_consensus

# Epoch transition test
ctest -R integration_epoch

# Slashing test
ctest -R integration_slashing
```

### Stress Tests

```bash
# Correlated slashing (40% Byzantine validators)
ctest -R stress_slashing

# Mempool spam (10k tx/s)
ctest -R stress_mempool

# Large validator set (N=500)
ctest -R stress_validators
```

## Project Structure

```
sarafu-blockchain/
├── CMakeLists.txt           # Build configuration
├── README.md                # This file
├── CLI_USAGE.md            # CLI tool documentation
├── Makefile                # Build shortcuts
├── .gitignore              # Git ignore rules
├── config.*.toml           # Configuration examples
├── include/                # Public headers
│   └── sarafu/
│       ├── consensus/      # Consensus engine
│       ├── crypto/         # Cryptography primitives
│       ├── network/        # P2P networking
│       ├── state/          # State machine
│       ├── storage/        # Persistence layer
│       ├── rpc/            # RPC interfaces
│       ├── config/         # Configuration
│       ├── logging/        # Logging system
│       └── monitoring/     # Metrics & monitoring
├── src/                    # Implementation files
│   ├── cli/               # CLI tool (sar)
│   ├── consensus/
│   ├── crypto/
│   ├── network/
│   ├── state/
│   ├── storage/
│   ├── rpc/
│   ├── config/
│   ├── logging/
│   ├── monitoring/
│   ├── node.cpp           # Node implementation
│   └── main.cpp           # Entry point
├── tests/                  # Test files
│   ├── unit/              # Unit tests
│   ├── property/          # Property-based tests
│   ├── integration/       # Integration tests
│   └── stress/            # Stress tests
├── docker/                 # Docker configurations
│   ├── Dockerfile.dev     # Development container
│   ├── Dockerfile.prod    # Production container
│   ├── docker-compose.yml # Multi-node testnet
│   ├── configs/           # Validator configs
│   └── README.md          # Docker documentation
└── proto/                  # Protocol Buffer definitions
    └── sarafu.proto
```

## Development

### Code Style

- Follow C++ Core Guidelines
- Use clang-format for formatting (Google style)
- Use clang-tidy for static analysis

### Adding Dependencies

Dependencies are managed via CMake FetchContent. To add a new dependency:

1. Add FetchContent_Declare in CMakeLists.txt
2. Add to target_link_libraries
3. Update this README

### Running Linters

```bash
# Format code
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Static analysis
clang-tidy src/**/*.cpp -- -Iinclude
```

## Documentation

- [Requirements](.kiro/specs/sarafu-blockchain-core/requirements.md)
- [Design Document](.kiro/specs/sarafu-blockchain-core/design.md)
- [Implementation Plan](.kiro/specs/sarafu-blockchain-core/tasks.md)
- [Whitepaper](Sarafu-Whitepater.md)

## License

Apache 2.0

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

## Security

For security issues, please email security@grassrootseconomics.org

Do not open public issues for security vulnerabilities.

## Community

- NEXT
