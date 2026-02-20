# Sarafu Blockchain Core

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

- CMake 3.20 or later
- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- Git

### Dependencies

The build system automatically fetches and builds the following dependencies:

- **libp2p**: Peer-to-peer networking
- **libsodium**: Ed25519 signatures
- **blst**: BLS12-381 signatures
- **Blake3**: Cryptographic hashing
- **gRPC**: RPC interface
- **Protocol Buffers**: Message serialization
- **RocksDB**: State storage

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/grassrootseconomics/sarafu-blockchain.git
cd sarafu-blockchain

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

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

# Install to system
sudo cmake --install .
```

## Docker Deployment

### Development Container

Build and run the development container with all dependencies:

```bash
docker build -f docker/Dockerfile.dev -t sarafu-dev .
docker run -it -v $(pwd):/workspace sarafu-dev
```

### Production Container

Build the minimal production container:

```bash
docker build -f docker/Dockerfile.prod -t sarafu-node .
docker run -d -p 26656:26656 -p 9090:9090 -v sarafu-data:/data sarafu-node
```

### Multi-Node Testnet

Launch a local testnet with 10 validators:

```bash
cd docker
docker-compose up -d
```

View logs:

```bash
docker-compose logs -f validator-1
```

Stop testnet:

```bash
docker-compose down
```

## Configuration

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
consensus_key_path = "/etc/sarafu/consensus_key.json"
withdrawal_key_path = "/etc/sarafu/withdrawal_key.json"

[logging]
level = "info"
format = "json"
```

### Running a Node

```bash
# Full node
./sarafu-node --config config.toml

# Validator node
./sarafu-node --config config.toml --validator

# Override configuration
./sarafu-node --config config.toml --rpc.grpc_port 9091 --logging.level debug
```

## Testing

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
├── .gitignore              # Git ignore rules
├── include/                # Public headers
│   └── sarafu/
│       ├── consensus/      # Consensus engine
│       ├── crypto/         # Cryptography primitives
│       ├── network/        # P2P networking
│       ├── state/          # State machine
│       └── storage/        # Persistence layer
├── src/                    # Implementation files
│   ├── consensus/
│   ├── crypto/
│   ├── network/
│   ├── state/
│   ├── storage/
│   └── main.cpp           # Entry point
├── tests/                  # Test files
│   ├── unit/              # Unit tests
│   ├── property/          # Property-based tests
│   ├── integration/       # Integration tests
│   └── stress/            # Stress tests
├── docker/                 # Docker configurations
│   ├── Dockerfile.dev     # Development container
│   ├── Dockerfile.prod    # Production container
│   └── docker-compose.yml # Multi-node testnet
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

- Website: https://sarafu.network
- Discord: https://discord.gg/sarafu
- Twitter: @SarafuNetwork
- Forum: https://forum.sarafu.network
