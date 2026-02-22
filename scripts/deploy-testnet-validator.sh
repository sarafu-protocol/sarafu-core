#!/bin/bash
# Sarafu Blockchain - Testnet Validator Deployment Script
# 
# This script deploys a Sarafu validator node for the Kilimanjaro testnet.
# Supports both Docker containerized deployment and native systemd deployment.
#
# Usage:
#   ./deploy-testnet-validator.sh [docker|native] [validator-id]
#
# Requirements:
#   - Docker (for containerized deployment)
#   - systemd (for native deployment)
#   - Validator keys generated and placed in keys/ directory

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
DEPLOYMENT_TYPE="${1:-docker}"
VALIDATOR_ID="${2:-1}"
TESTNET_GENESIS="docker/testnet-genesis.json"
TESTNET_CONFIG="config.testnet.toml"
DATA_DIR="./data/testnet-validator-${VALIDATOR_ID}"
KEYS_DIR="./keys/validator-${VALIDATOR_ID}"
LOG_DIR="./logs/validator-${VALIDATOR_ID}"

# Docker configuration
DOCKER_IMAGE="sarafu-node:latest"
DOCKER_CONTAINER="sarafu-validator-${VALIDATOR_ID}"
DOCKER_NETWORK="sarafu-testnet"

# Systemd configuration
SYSTEMD_SERVICE="sarafu-validator-${VALIDATOR_ID}"
BINARY_PATH="/usr/local/bin/sarafu-node"

# Helper functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check if genesis file exists
    if [ ! -f "$TESTNET_GENESIS" ]; then
        log_error "Testnet genesis file not found: $TESTNET_GENESIS"
        log_info "Run: python3 scripts/generate-testnet-genesis.py"
        exit 1
    fi
    
    # Check if config file exists
    if [ ! -f "$TESTNET_CONFIG" ]; then
        log_error "Testnet config file not found: $TESTNET_CONFIG"
        exit 1
    fi
    
    # Check if validator keys exist
    if [ ! -d "$KEYS_DIR" ]; then
        log_error "Validator keys directory not found: $KEYS_DIR"
        log_info "Generate keys with: sarafu-node keygen --validator-id $VALIDATOR_ID"
        exit 1
    fi
    
    if [ ! -f "$KEYS_DIR/consensus.key" ] || [ ! -f "$KEYS_DIR/withdrawal.key" ]; then
        log_error "Validator keys not found in $KEYS_DIR"
        log_info "Required files: consensus.key, withdrawal.key"
        exit 1
    fi
    
    log_info "✅ Prerequisites check passed"
}

create_directories() {
    log_info "Creating directories..."
    mkdir -p "$DATA_DIR"
    mkdir -p "$LOG_DIR"
    log_info "✅ Directories created"
}

deploy_docker() {
    log_info "Deploying validator using Docker..."
    
    # Check if Docker is installed
    if ! command -v docker &> /dev/null; then
        log_error "Docker is not installed"
        exit 1
    fi
    
    # Create Docker network if it doesn't exist
    if ! docker network inspect "$DOCKER_NETWORK" &> /dev/null; then
        log_info "Creating Docker network: $DOCKER_NETWORK"
        docker network create "$DOCKER_NETWORK"
    fi
    
    # Stop and remove existing container if it exists
    if docker ps -a --format '{{.Names}}' | grep -q "^${DOCKER_CONTAINER}$"; then
        log_warn "Stopping existing container: $DOCKER_CONTAINER"
        docker stop "$DOCKER_CONTAINER" || true
        docker rm "$DOCKER_CONTAINER" || true
    fi
    
    # Build Docker image if it doesn't exist
    if ! docker images --format '{{.Repository}}:{{.Tag}}' | grep -q "^${DOCKER_IMAGE}$"; then
        log_info "Building Docker image: $DOCKER_IMAGE"
        docker build -t "$DOCKER_IMAGE" -f docker/Dockerfile.prod .
    fi
    
    # Run validator container
    log_info "Starting validator container: $DOCKER_CONTAINER"
    docker run -d \
        --name "$DOCKER_CONTAINER" \
        --network "$DOCKER_NETWORK" \
        -v "$(pwd)/$DATA_DIR:/data" \
        -v "$(pwd)/$KEYS_DIR:/keys:ro" \
        -v "$(pwd)/$LOG_DIR:/logs" \
        -v "$(pwd)/$TESTNET_GENESIS:/genesis.json:ro" \
        -v "$(pwd)/$TESTNET_CONFIG:/config.toml:ro" \
        -p "$((30303 + VALIDATOR_ID)):30303" \
        -p "$((50051 + VALIDATOR_ID)):50051" \
        -p "$((8080 + VALIDATOR_ID)):8080" \
        -e VALIDATOR_ID="$VALIDATOR_ID" \
        -e IS_VALIDATOR="true" \
        -e CONSENSUS_KEY_PATH="/keys/consensus.key" \
        -e WITHDRAWAL_KEY_PATH="/keys/withdrawal.key" \
        --restart unless-stopped \
        "$DOCKER_IMAGE" \
        --config /config.toml \
        --genesis /genesis.json \
        --data-dir /data \
        --validator
    
    log_info "✅ Validator container started"
    log_info "Container name: $DOCKER_CONTAINER"
    log_info "Network port: $((30303 + VALIDATOR_ID))"
    log_info "RPC port: $((50051 + VALIDATOR_ID))"
    log_info "REST port: $((8080 + VALIDATOR_ID))"
    log_info ""
    log_info "View logs: docker logs -f $DOCKER_CONTAINER"
    log_info "Stop validator: docker stop $DOCKER_CONTAINER"
}

deploy_native() {
    log_info "Deploying validator using systemd..."
    
    # Check if systemd is available
    if ! command -v systemctl &> /dev/null; then
        log_error "systemd is not available on this system"
        exit 1
    fi
    
    # Check if binary exists
    if [ ! -f "$BINARY_PATH" ]; then
        log_error "Sarafu node binary not found: $BINARY_PATH"
        log_info "Build and install with: make install"
        exit 1
    fi
    
    # Create systemd service file
    log_info "Creating systemd service: $SYSTEMD_SERVICE"
    
    sudo tee "/etc/systemd/system/${SYSTEMD_SERVICE}.service" > /dev/null <<EOF
[Unit]
Description=Sarafu Testnet Validator ${VALIDATOR_ID}
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=$USER
WorkingDirectory=$(pwd)
ExecStart=$BINARY_PATH \\
    --config $(pwd)/$TESTNET_CONFIG \\
    --genesis $(pwd)/$TESTNET_GENESIS \\
    --data-dir $(pwd)/$DATA_DIR \\
    --validator \\
    --consensus-key $(pwd)/$KEYS_DIR/consensus.key \\
    --withdrawal-key $(pwd)/$KEYS_DIR/withdrawal.key
Restart=on-failure
RestartSec=10
StandardOutput=append:$(pwd)/$LOG_DIR/validator.log
StandardError=append:$(pwd)/$LOG_DIR/validator.error.log

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=$(pwd)/$DATA_DIR $(pwd)/$LOG_DIR

# Resource limits
LimitNOFILE=65536
LimitNPROC=4096

[Install]
WantedBy=multi-user.target
EOF
    
    # Reload systemd
    log_info "Reloading systemd daemon..."
    sudo systemctl daemon-reload
    
    # Enable and start service
    log_info "Enabling and starting service..."
    sudo systemctl enable "$SYSTEMD_SERVICE"
    sudo systemctl start "$SYSTEMD_SERVICE"
    
    # Wait a moment for service to start
    sleep 2
    
    # Check service status
    if sudo systemctl is-active --quiet "$SYSTEMD_SERVICE"; then
        log_info "✅ Validator service started successfully"
        log_info "Service name: $SYSTEMD_SERVICE"
        log_info ""
        log_info "View logs: journalctl -u $SYSTEMD_SERVICE -f"
        log_info "Check status: sudo systemctl status $SYSTEMD_SERVICE"
        log_info "Stop validator: sudo systemctl stop $SYSTEMD_SERVICE"
        log_info "Restart validator: sudo systemctl restart $SYSTEMD_SERVICE"
    else
        log_error "Failed to start validator service"
        log_info "Check logs: journalctl -u $SYSTEMD_SERVICE -n 50"
        exit 1
    fi
}

show_usage() {
    cat <<EOF
Sarafu Testnet Validator Deployment Script

Usage:
    $0 [docker|native] [validator-id]

Arguments:
    deployment-type    Deployment method: 'docker' or 'native' (default: docker)
    validator-id       Validator ID number (default: 1)

Examples:
    # Deploy validator 1 using Docker
    $0 docker 1

    # Deploy validator 2 using systemd
    $0 native 2

Prerequisites:
    - Testnet genesis file: $TESTNET_GENESIS
    - Testnet config file: $TESTNET_CONFIG
    - Validator keys in: ./keys/validator-{id}/

For Docker deployment:
    - Docker installed and running
    - Docker image built or will be built automatically

For native deployment:
    - systemd available
    - Sarafu node binary installed at: $BINARY_PATH
    - sudo access for systemd service management

EOF
}

# Main execution
main() {
    log_info "Sarafu Testnet Validator Deployment"
    log_info "===================================="
    log_info "Deployment type: $DEPLOYMENT_TYPE"
    log_info "Validator ID: $VALIDATOR_ID"
    log_info ""
    
    # Show usage if help requested
    if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
        show_usage
        exit 0
    fi
    
    # Validate deployment type
    if [ "$DEPLOYMENT_TYPE" != "docker" ] && [ "$DEPLOYMENT_TYPE" != "native" ]; then
        log_error "Invalid deployment type: $DEPLOYMENT_TYPE"
        log_info "Must be 'docker' or 'native'"
        show_usage
        exit 1
    fi
    
    # Check prerequisites
    check_prerequisites
    
    # Create directories
    create_directories
    
    # Deploy based on type
    if [ "$DEPLOYMENT_TYPE" = "docker" ]; then
        deploy_docker
    else
        deploy_native
    fi
    
    log_info ""
    log_info "✅ Deployment complete!"
}

# Run main function
main "$@"
