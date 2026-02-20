# Sarafu Blockchain Docker Deployment

This directory contains Docker configurations for deploying the Sarafu blockchain in development and production environments.

## Contents

- `Dockerfile.dev` - Multi-stage development container with all build tools and debugging utilities
- `Dockerfile.prod` - Minimal production container with only runtime dependencies
- `docker-compose.yml` - Multi-node testnet configuration with 10 validators
- `configs/` - Configuration files for each validator node

## Quick Start

All Docker operations are managed through the Makefile in the root directory.

### Start Testnet (10 Validators)

```bash
# From project root
make docker-build    # Build images
make start           # Start testnet
make status          # Check validator health
```

### View Logs

```bash
make docker-logs        # All validators
make docker-logs-1      # Specific validator
```

### Stop/Restart

```bash
make stop               # Stop testnet (preserves data)
make restart            # Restart testnet
make docker-clean       # Remove all data and containers
```

## Network Configuration

The testnet uses a custom Docker network with the following configuration:

- Network: `sarafu-testnet` (172.25.0.0/16)
- Gateway: 172.25.0.1
- Validators: 172.25.0.11 - 172.25.0.20

### Validator Endpoints

| Validator    | REST API              | gRPC           | P2P             | Container IP |
| ------------ | --------------------- | -------------- | --------------- | ------------ |
| validator-1  | http://localhost:8080 | localhost:9090 | localhost:26656 | 172.25.0.11  |
| validator-2  | http://localhost:8081 | localhost:9091 | localhost:26657 | 172.25.0.12  |
| validator-3  | http://localhost:8082 | localhost:9092 | localhost:26658 | 172.25.0.13  |
| validator-4  | http://localhost:8083 | localhost:9093 | localhost:26659 | 172.25.0.14  |
| validator-5  | http://localhost:8084 | localhost:9094 | localhost:26660 | 172.25.0.15  |
| validator-6  | http://localhost:8085 | localhost:9095 | localhost:26661 | 172.25.0.16  |
| validator-7  | http://localhost:8086 | localhost:9096 | localhost:26662 | 172.25.0.17  |
| validator-8  | http://localhost:8087 | localhost:9097 | localhost:26663 | 172.25.0.18  |
| validator-9  | http://localhost:8088 | localhost:9098 | localhost:26664 | 172.25.0.19  |
| validator-10 | http://localhost:8089 | localhost:9099 | localhost:26665 | 172.25.0.20  |

## Development Container

For interactive development:

```bash
make dev-shell
```

This starts a container with all build tools and mounts your workspace.

## Testing API Endpoints

```bash
make api-health        # Check health
make api-info          # Node info
make api-status        # Chain status
make api-validators    # List validators
make api-test-all      # Test all endpoints
```

## Troubleshooting

### Containers Won't Start

```bash
make docker-clean      # Clean everything
make docker-build      # Rebuild images
make start             # Start fresh
```

### View Container Logs

```bash
make docker-logs-1     # Validator 1 logs
make docker-logs       # All logs
```

### Check Status

```bash
make status            # Health check all validators
make endpoints         # Show all endpoints
```

## Data Persistence

Blockchain data is stored in Docker volumes:

- `validator-1-data` through `validator-10-data`
- Data persists across container restarts
- Use `make docker-clean` to remove all data

## Configuration Files

Each validator has its own configuration file in `configs/`:

- `validator-1.toml` through `validator-10.toml`
- Configurations include network settings, RPC ports, and validator keys
- Mounted as read-only into containers

## Requirements

- Docker 20.10+
- Docker Compose 1.29+
- 8GB RAM minimum (for 10-validator testnet)
- 20GB disk space

## Related Documentation

- [Main README](../README.md)
- [Makefile Commands](../Makefile) - Run `make help` for all commands
