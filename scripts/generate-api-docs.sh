#!/bin/bash
# API documentation generator for Sarafu blockchain
# Generates API reference documentation from code and proto files

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
OUTPUT_DIR="${OUTPUT_DIR:-docs/api}"
PROTO_DIR="${PROTO_DIR:-proto}"
SRC_DIR="${SRC_DIR:-src}"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}API Documentation Generator${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Generate API reference header
cat > "$OUTPUT_DIR/API_REFERENCE.md" << 'EOF'
# Sarafu Blockchain API Reference

This document provides a comprehensive reference for all RPC endpoints, request/response formats, and error codes.

**Generated:** $(date -u +"%Y-%m-%d %H:%M:%S UTC")

## Table of Contents

- [REST API](#rest-api)
  - [Node Endpoints](#node-endpoints)
  - [Chain Endpoints](#chain-endpoints)
  - [Transaction Endpoints](#transaction-endpoints)
  - [Validator Endpoints](#validator-endpoints)
- [gRPC API](#grpc-api)
- [Error Codes](#error-codes)
- [Data Types](#data-types)

---

## REST API

The Sarafu node exposes a REST API on port 8080 (configurable) for querying blockchain state and submitting transactions.

### Base URL

```
http://localhost:8080/api/v1
```

### Authentication

Currently, the API does not require authentication for read operations. Write operations (transaction submission) require valid signatures.

---

### Node Endpoints

#### GET /node/info

Get information about the node.

**Request:**
```bash
curl http://localhost:8080/api/v1/node/info
```

**Response:**
```json
{
  "node_id": "12D3KooW...",
  "version": "1.0.0",
  "chain_id": "sarafu-testnet-1",
  "latest_block_height": 12345,
  "latest_block_hash": "0x...",
  "peer_count": 42,
  "syncing": false
}
```

**Error Codes:**
- 500: Internal server error

---

#### GET /health

Health check endpoint.

**Request:**
```bash
curl http://localhost:8080/health
```

**Response:**
```json
{
  "status": "healthy",
  "timestamp": "2024-02-20T12:00:00Z"
}
```

**Error Codes:**
- 503: Service unavailable (node not ready)

---

### Chain Endpoints

#### GET /chain/status

Get current chain status.

**Request:**
```bash
curl http://localhost:8080/api/v1/chain/status
```

**Response:**
```json
{
  "chain_id": "sarafu-testnet-1",
  "latest_block": {
    "height": 12345,
    "hash": "0x...",
    "timestamp": "2024-02-20T12:00:00Z",
    "proposer": "0x..."
  },
  "finalized_block": {
    "height": 12344,
    "hash": "0x..."
  },
  "epoch": 123,
  "validator_count": 150
}
```

**Error Codes:**
- 500: Internal server error

---

#### GET /chain/block/:height

Get block by height.

**Request:**
```bash
curl http://localhost:8080/api/v1/chain/block/12345
```

**Response:**
```json
{
  "height": 12345,
  "hash": "0x...",
  "parent_hash": "0x...",
  "timestamp": "2024-02-20T12:00:00Z",
  "proposer": "0x...",
  "transactions": [...],
  "qc": {
    "block_hash": "0x...",
    "signatures": "0x...",
    "signers": [...]
  }
}
```

**Error Codes:**
- 404: Block not found
- 500: Internal server error

---

### Transaction Endpoints

#### POST /tx/submit

Submit a signed transaction.

**Request:**
```bash
curl -X POST http://localhost:8080/api/v1/tx/submit \
  -H "Content-Type: application/json" \
  -d '{
    "from": "0x...",
    "to": "0x...",
    "amount": "1000000000000000000",
    "nonce": 5,
    "signature": "0x..."
  }'
```

**Response:**
```json
{
  "tx_hash": "0x...",
  "status": "pending"
}
```

**Error Codes:**
- 400: Invalid transaction format
- 401: Invalid signature
- 429: Rate limit exceeded
- 500: Internal server error

---

#### GET /tx/:hash

Get transaction by hash.

**Request:**
```bash
curl http://localhost:8080/api/v1/tx/0x...
```

**Response:**
```json
{
  "hash": "0x...",
  "from": "0x...",
  "to": "0x...",
  "amount": "1000000000000000000",
  "nonce": 5,
  "block_height": 12345,
  "status": "finalized"
}
```

**Error Codes:**
- 404: Transaction not found
- 500: Internal server error

---

### Validator Endpoints

#### GET /validators

List all active validators.

**Request:**
```bash
curl http://localhost:8080/api/v1/validators
```

**Response:**
```json
{
  "validators": [
    {
      "consensus_pubkey": "0x...",
      "stake": "32000000000000000000",
      "uptime": 0.995,
      "missed_blocks": 5,
      "is_jailed": false
    }
  ],
  "total_stake": "4800000000000000000000"
}
```

**Error Codes:**
- 500: Internal server error

---

#### GET /validators/:pubkey

Get validator details.

**Request:**
```bash
curl http://localhost:8080/api/v1/validators/0x...
```

**Response:**
```json
{
  "consensus_pubkey": "0x...",
  "withdrawal_address": "0x...",
  "stake": "32000000000000000000",
  "uptime": 0.995,
  "missed_blocks": 5,
  "is_jailed": false,
  "joined_epoch": 100
}
```

**Error Codes:**
- 404: Validator not found
- 500: Internal server error

---

## gRPC API

The Sarafu node also exposes a gRPC API on port 9090 (configurable) for high-performance communication.

See `proto/sarafu.proto` for the complete gRPC service definitions.

### Service: SarafuNode

```protobuf
service SarafuNode {
  rpc GetNodeInfo(GetNodeInfoRequest) returns (GetNodeInfoResponse);
  rpc GetChainStatus(GetChainStatusRequest) returns (GetChainStatusResponse);
  rpc GetBlock(GetBlockRequest) returns (GetBlockResponse);
  rpc SubmitTransaction(SubmitTransactionRequest) returns (SubmitTransactionResponse);
  rpc GetValidators(GetValidatorsRequest) returns (GetValidatorsResponse);
}
```

---

## Error Codes

| Code | Description |
|------|-------------|
| 400  | Bad Request - Invalid request format or parameters |
| 401  | Unauthorized - Invalid signature or authentication |
| 404  | Not Found - Requested resource does not exist |
| 429  | Too Many Requests - Rate limit exceeded |
| 500  | Internal Server Error - Server-side error |
| 503  | Service Unavailable - Node not ready or syncing |

---

## Data Types

### Address

A 32-byte hex-encoded Ethereum-style address.

**Format:** `0x` followed by 64 hexadecimal characters

**Example:** `0x1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef`

### Amount

Token amounts are represented as strings to avoid precision loss.

**Unit:** Wei (10^-18 SAR)

**Example:** `"1000000000000000000"` = 1 SAR

### Hash

A 32-byte hex-encoded hash.

**Format:** `0x` followed by 64 hexadecimal characters

**Example:** `0xabcdef1234567890abcdef1234567890abcdef1234567890abcdef1234567890`

### Signature

A hex-encoded cryptographic signature.

**Format:** `0x` followed by signature bytes

**Example:** `0x...` (length varies by signature scheme)

---

## Rate Limits

The API implements rate limiting to prevent abuse:

- **Read endpoints:** 100 requests per minute per IP
- **Write endpoints:** 10 requests per minute per IP

Rate limit headers are included in responses:

```
X-RateLimit-Limit: 100
X-RateLimit-Remaining: 95
X-RateLimit-Reset: 1234567890
```

---

## Pagination

List endpoints support pagination using query parameters:

- `page`: Page number (default: 1)
- `limit`: Items per page (default: 50, max: 100)

**Example:**
```bash
curl "http://localhost:8080/api/v1/validators?page=2&limit=50"
```

---

## Versioning

The API uses URL versioning. The current version is `v1`.

Future versions will be available at `/api/v2`, `/api/v3`, etc.

---

## Support

For API support and questions:
- GitHub Issues: https://github.com/sarafu/sarafu-blockchain
- Documentation: https://docs.sarafu.network

EOF

echo -e "${GREEN}✓ Generated API reference: $OUTPUT_DIR/API_REFERENCE.md${NC}"

# Extract RPC endpoints from proto files if they exist
if [ -d "$PROTO_DIR" ]; then
    echo -e "${YELLOW}Extracting gRPC definitions from proto files...${NC}"
    
    for proto_file in "$PROTO_DIR"/*.proto; do
        if [ -f "$proto_file" ]; then
            echo -e "${BLUE}  Processing: $proto_file${NC}"
            # Extract service definitions
            grep -A 20 "^service " "$proto_file" || true
        fi
    done
fi

# Generate endpoint summary
cat > "$OUTPUT_DIR/ENDPOINTS_SUMMARY.md" << 'EOF'
# API Endpoints Summary

## REST API Endpoints

### Node Management
- `GET /health` - Health check
- `GET /api/v1/node/info` - Node information

### Chain Queries
- `GET /api/v1/chain/status` - Chain status
- `GET /api/v1/chain/block/:height` - Get block by height

### Transactions
- `POST /api/v1/tx/submit` - Submit transaction
- `GET /api/v1/tx/:hash` - Get transaction by hash

### Validators
- `GET /api/v1/validators` - List validators
- `GET /api/v1/validators/:pubkey` - Get validator details

## gRPC Endpoints

See `proto/sarafu.proto` for complete service definitions.

## Metrics

- `GET /metrics` - Prometheus metrics endpoint

EOF

echo -e "${GREEN}✓ Generated endpoints summary: $OUTPUT_DIR/ENDPOINTS_SUMMARY.md${NC}"

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}API Documentation Generated${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "Output directory: ${GREEN}$OUTPUT_DIR${NC}"
echo -e "Main reference: ${GREEN}$OUTPUT_DIR/API_REFERENCE.md${NC}"
echo -e "Endpoints summary: ${GREEN}$OUTPUT_DIR/ENDPOINTS_SUMMARY.md${NC}"
echo ""
echo -e "${YELLOW}Note: This is a template. Update with actual endpoint implementations.${NC}"
