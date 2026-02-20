# Sarafu CLI Usage Guide

The Sarafu blockchain provides a unified CLI tool called `sar` for all blockchain operations.

## Installation

The `sar` binary is built alongside the node and installed to `/usr/local/bin/sar` in Docker containers.

## Commands

### Key Generation

Generate validator keys (BLS12-381 consensus + Ed25519 withdrawal):

```bash
# Generate keys for a single validator
sar keygen --output-dir ./keys --validator-id 1

# Generate keys for multiple validators
sar keygen --output-dir ./docker/keys --count 10

# Generate keys starting from a specific ID
sar keygen --output-dir ./keys --validator-id 5 --count 3
```

Options:

- `--output-dir <path>`: Output directory (default: ./keys)
- `--validator-id <id>`: Starting validator ID (default: 1)
- `--count <n>`: Number of validators to generate keys for (default: 1)

Output structure:

```
keys/
├── validator-1/
│   ├── consensus_key.bin      # 32-byte BLS12-381 private key
│   ├── consensus_key.json     # Public key metadata
│   ├── withdrawal_key.bin     # 32-byte Ed25519 private key
│   └── withdrawal_key.json    # Public key metadata
├── validator-2/
│   └── ...
```

### Genesis File Generation

Generate genesis file from validator keys:

```bash
# Generate genesis for 10 validators
sar genesis --validators 10 --keys-dir ./docker/keys --output ./docker/genesis.json

# Custom chain ID
sar genesis --validators 5 --keys-dir ./keys --chain-id sarafu-mainnet-1
```

Options:

- `--output <path>`: Output file path (default: ./genesis.json)
- `--keys-dir <path>`: Directory containing validator keys (default: ./keys)
- `--validators <n>`: Number of validators (default: 10)
- `--chain-id <id>`: Chain ID (default: sarafu-testnet-1)

### Running a Node

Run a blockchain node:

```bash
# Run with config file
sar node --config /data/config.toml

# Run in validator mode
sar node --config config.toml --validator

# Override data directory
sar node --config config.toml --data-dir /custom/data
```

Options:

- `--config <path>`: Configuration file path
- `--data-dir <path>`: Data directory path
- `--validator`: Run in validator mode
- `--help`: Show help message

### Version Information

```bash
sar version
```

### Help

```bash
# General help
sar help

# Command-specific help
sar keygen --help
sar genesis --help
sar node --help
```

## Complete Testnet Setup Example

```bash
# 1. Generate keys for 10 validators
sar keygen --output-dir ./docker/keys --count 10

# 2. Generate genesis file
sar genesis --validators 10 --keys-dir ./docker/keys --output ./docker/genesis.json

# 3. Start validators using Docker Compose
docker-compose -f docker/docker-compose.yml up -d
```

## Docker Usage

Inside Docker containers, the `sar` command is available:

```bash
# Generate keys inside container
docker run --rm -v $(pwd)/keys:/keys sarafu-node sar keygen --output-dir /keys --count 10

# Generate genesis inside container
docker run --rm -v $(pwd)/keys:/keys -v $(pwd):/output sarafu-node \
  sar genesis --validators 10 --keys-dir /keys --output /output/genesis.json
```

## Security Notes

- Private keys are stored as 32-byte binary files with restrictive permissions (0600)
- Never commit private keys to version control
- Backup keys securely in multiple locations
- Use proper key management systems in production
- BLS12-381 keys currently use placeholder generation - use proper BLS library for production

## Development

The CLI is built from C++ source files in `src/cli/`:

- `main.cpp`: CLI entry point and command routing
- `cmd_keygen.cpp`: Key generation implementation
- `cmd_genesis.cpp`: Genesis file generation
- `cmd_node.cpp`: Node runner (delegates to main node code)

Build the CLI:

```bash
cmake -B build -S .
cmake --build build --target sar
```
