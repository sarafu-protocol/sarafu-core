# Sarafu Validator Node - Quick Reference Guide

## Current Status

Your validator node is running successfully!

- **Process ID**: Check with `./check-validator.sh`
- **Validator ID**: `a9d4428afb44035c07278ef6b4079f09ea5f1e1e4400e45f9dd7f86c5c5477c7`
- **Chain ID**: 1337 (local development)
- **Mode**: Validator

## Starting the Validator

```bash
./build/sarafu-node --config config.local.toml --validator \
  --consensus-key ./docker/keys/validator-1/consensus_key.bin \
  --withdrawal-key ./docker/keys/validator-1/withdrawal_key.bin \
  --log-level INFO &
```

## Managing the Validator

### Check Status

```bash
./check-validator.sh
```

### View Process

```bash
ps aux | grep sarafu-node | grep -v grep
```

### Check Network Ports

```bash
lsof -i :9000    # P2P network
lsof -i :8080    # REST API (stub)
lsof -i :50051   # gRPC API (stub)
```

### Stop the Validator

```bash
pkill sarafu-node
```

### Restart the Validator

```bash
pkill sarafu-node
sleep 1
./build/sarafu-node --config config.local.toml --validator \
  --consensus-key ./docker/keys/validator-1/consensus_key.bin \
  --withdrawal-key ./docker/keys/validator-1/withdrawal_key.bin \
  --log-level INFO &
```

## Configuration Files

- **Node Config**: `config.local.toml`
- **Genesis**: `build/genesis.local.json`
- **Consensus Key**: `docker/keys/validator-1/consensus_key.bin`
- **Withdrawal Key**: `docker/keys/validator-1/withdrawal_key.bin`
- **Data Directory**: `./data/local`

## Network Configuration

- **P2P Listen**: 127.0.0.1:30303 (configured)
- **P2P Actual**: 0.0.0.0:9000 (actual binding)
- **gRPC**: 127.0.0.1:50051 (stub)
- **REST**: 127.0.0.1:8080 (stub)
- **WebSocket**: 127.0.0.1:8081 (stub)

## Current Limitations

The node is running with some stub implementations:

1. **RPC Server**: Stub implementation (gRPC, REST, WebSocket not fully functional)
2. **Genesis Block**: Using placeholder implementation
3. **State Machine**: Not fully initialized from genesis

These are expected for the current development version (0.1.0-alpha, 92% complete).

## Running a Full Testnet

To run a complete multi-validator testnet with 10 validators:

```bash
# Start the full testnet
make docker-up

# Check status
make docker-status

# View logs
make docker-logs-1

# Stop testnet
make docker-down
```

## Useful Make Commands

```bash
make build              # Build the project
make test               # Run tests
make run-local          # Run local node
make run-validator      # Run validator node
make docker-up          # Start testnet
make docker-status      # Check testnet status
make docker-logs        # View all logs
make docker-down        # Stop testnet
```

## Troubleshooting

### Port Already in Use

```bash
# Find what's using the port
lsof -i :9000

# Kill the process
pkill sarafu-node
```

### Check if Node is Running

```bash
ps aux | grep sarafu-node | grep -v grep
```

### View Node Logs

```bash
# If logs are being written to file
tail -f ./data/local/node.log

# Or check the process output
# (Currently logs go to stdout/stderr)
```

## Next Steps

1. **Multi-Validator Setup**: Use `make docker-up` to run a full testnet
2. **API Development**: The RPC endpoints are stubs and need full implementation
3. **Genesis Initialization**: Complete genesis block creation from file
4. **Production Keys**: Generate production-ready keys for mainnet

## Security Notes

⚠️ **Important**: The current keys are for development only!

- Never use development keys in production
- Backup validator keys securely
- Use proper key management for mainnet
- Restrict file permissions on key files (0600)

## Support

For issues or questions:

- Check the main README.md
- Review CLI_USAGE.md
- See the whitepaper: Sarafu-Whitepater.md
