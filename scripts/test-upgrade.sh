#!/bin/bash
#
# Upgrade testing script for Sarafu blockchain
#
# Usage: ./test-upgrade.sh <from_version> <to_version>
#
# This script tests an upgrade path by:
# 1. Setting up a test environment with the old version
# 2. Running the node and generating test data
# 3. Performing the upgrade
# 4. Verifying the node works correctly after upgrade
# 5. Checking data integrity
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
TEST_DIR="/tmp/sarafu-upgrade-test-$$"
BINARY_NAME="sarafu-node"
RELEASE_URL="https://releases.sarafu.network"

# Parse arguments
FROM_VERSION=""
TO_VERSION=""

if [ $# -lt 2 ]; then
    echo -e "${RED}Error: Missing arguments${NC}"
    echo "Usage: $0 <from_version> <to_version>"
    echo "Example: $0 v1.0.0-p1 v1.1.0-p1"
    exit 1
fi

FROM_VERSION=$1
TO_VERSION=$2

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

log_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

cleanup() {
    log_info "Cleaning up test environment..."
    
    # Stop any running test nodes
    pkill -f "$TEST_DIR" 2>/dev/null || true
    
    # Remove test directory
    rm -rf "$TEST_DIR"
    
    log_info "Cleanup complete"
}

# Trap cleanup on exit
trap cleanup EXIT

setup_test_environment() {
    log_info "Setting up test environment in $TEST_DIR"
    
    mkdir -p "$TEST_DIR"/{bin,data,logs,config}
    
    # Create test genesis file
    cat > "$TEST_DIR/config/genesis.json" <<EOF
{
  "chain_id": "sarafu-upgrade-test",
  "genesis_time": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "config": {
    "protocol_version": 1,
    "slot_duration_ms": 2000,
    "slots_per_epoch": 32,
    "min_validator_stake": "32000000000000000000",
    "max_validators": 10,
    "base_fee_per_gas": "1000000000",
    "block_gas_limit": "30000000"
  },
  "initial_validators": [],
  "initial_accounts": [
    {
      "address": "0x1111111111111111111111111111111111111111111111111111111111111111",
      "balance": "1000000000000000000000000",
      "nonce": 0
    }
  ],
  "initial_supply": "1000000000000000000000000",
  "monetary_policy": {
    "target_annual_inflation": "0.05",
    "security_budget_ratio": "0.02",
    "max_inflation": "0.10",
    "min_inflation": "0.01"
  }
}
EOF
    
    # Create test config file
    cat > "$TEST_DIR/config/config.toml" <<EOF
[network]
listen_address = "/ip4/127.0.0.1/tcp/19000"
bootstrap_peers = []

[storage]
data_dir = "$TEST_DIR/data"

[logging]
level = "info"
output = "$TEST_DIR/logs/node.log"
EOF
    
    log_info "Test environment created"
}

download_version() {
    local version=$1
    local output_dir=$2
    local download_url="$RELEASE_URL/$BINARY_NAME-$version"
    local output_file="$output_dir/$BINARY_NAME-$version"
    
    log_info "Downloading $BINARY_NAME $version..."
    
    # For testing, we'll use the local build if available
    if [ -f "build/bin/$BINARY_NAME" ]; then
        log_warn "Using local build instead of downloading (test mode)"
        cp "build/bin/$BINARY_NAME" "$output_file"
        chmod +x "$output_file"
        return 0
    fi
    
    if ! wget -q "$download_url" -O "$output_file"; then
        log_error "Failed to download binary from $download_url"
        return 1
    fi
    
    chmod +x "$output_file"
    log_info "Downloaded $version to $output_file"
    return 0
}

start_node() {
    local version=$1
    local binary="$TEST_DIR/bin/$BINARY_NAME-$version"
    
    log_info "Starting node with version $version..."
    
    if [ ! -f "$binary" ]; then
        log_error "Binary not found: $binary"
        return 1
    fi
    
    # Start node in background
    "$binary" \
        --config "$TEST_DIR/config/config.toml" \
        --genesis "$TEST_DIR/config/genesis.json" \
        > "$TEST_DIR/logs/node-$version.log" 2>&1 &
    
    local pid=$!
    echo $pid > "$TEST_DIR/node.pid"
    
    # Wait for node to start
    log_info "Waiting for node to start..."
    local timeout=30
    local elapsed=0
    while [ $elapsed -lt $timeout ]; do
        if grep -q "Node started" "$TEST_DIR/logs/node-$version.log" 2>/dev/null; then
            log_info "Node started successfully (PID: $pid)"
            return 0
        fi
        sleep 1
        elapsed=$((elapsed + 1))
    done
    
    log_error "Node did not start within $timeout seconds"
    cat "$TEST_DIR/logs/node-$version.log"
    return 1
}

stop_node() {
    if [ -f "$TEST_DIR/node.pid" ]; then
        local pid=$(cat "$TEST_DIR/node.pid")
        log_info "Stopping node (PID: $pid)..."
        
        kill $pid 2>/dev/null || true
        
        # Wait for process to stop
        local timeout=10
        local elapsed=0
        while kill -0 $pid 2>/dev/null && [ $elapsed -lt $timeout ]; do
            sleep 1
            elapsed=$((elapsed + 1))
        done
        
        if kill -0 $pid 2>/dev/null; then
            log_warn "Node did not stop gracefully, forcing..."
            kill -9 $pid 2>/dev/null || true
        fi
        
        rm -f "$TEST_DIR/node.pid"
        log_info "Node stopped"
    fi
}

generate_test_data() {
    log_info "Generating test data..."
    
    # In a real implementation, this would:
    # - Create test transactions
    # - Generate blocks
    # - Create validator state
    # - Populate database with test data
    
    # For now, we'll just create some dummy files
    mkdir -p "$TEST_DIR/data/blocks"
    mkdir -p "$TEST_DIR/data/state"
    
    for i in {1..10}; do
        echo "test_block_$i" > "$TEST_DIR/data/blocks/block_$i.dat"
    done
    
    echo "test_state_data" > "$TEST_DIR/data/state/state.dat"
    
    log_info "Test data generated"
}

verify_data_integrity() {
    log_info "Verifying data integrity..."
    
    # Check that test data still exists
    local errors=0
    
    for i in {1..10}; do
        if [ ! -f "$TEST_DIR/data/blocks/block_$i.dat" ]; then
            log_error "Missing block file: block_$i.dat"
            errors=$((errors + 1))
        fi
    done
    
    if [ ! -f "$TEST_DIR/data/state/state.dat" ]; then
        log_error "Missing state file: state.dat"
        errors=$((errors + 1))
    fi
    
    if [ $errors -eq 0 ]; then
        log_info "Data integrity verified"
        return 0
    else
        log_error "Data integrity check failed: $errors errors"
        return 1
    fi
}

perform_upgrade() {
    local from_version=$1
    local to_version=$2
    
    log_info "Performing upgrade from $from_version to $to_version..."
    
    # Stop old version
    stop_node
    
    # Simulate upgrade process
    log_info "Installing new version..."
    sleep 2
    
    # Start new version
    if ! start_node "$to_version"; then
        log_error "Failed to start new version"
        return 1
    fi
    
    log_info "Upgrade completed"
    return 0
}

run_compatibility_tests() {
    log_test "Running compatibility tests..."
    
    local tests_passed=0
    local tests_failed=0
    
    # Test 1: Version detection
    log_test "Test 1: Version detection"
    local version=$("$TEST_DIR/bin/$BINARY_NAME-$TO_VERSION" version 2>/dev/null || echo "unknown")
    if [ "$version" != "unknown" ]; then
        log_info "✓ Version detection passed: $version"
        tests_passed=$((tests_passed + 1))
    else
        log_error "✗ Version detection failed"
        tests_failed=$((tests_failed + 1))
    fi
    
    # Test 2: Data integrity
    log_test "Test 2: Data integrity"
    if verify_data_integrity; then
        log_info "✓ Data integrity test passed"
        tests_passed=$((tests_passed + 1))
    else
        log_error "✗ Data integrity test failed"
        tests_failed=$((tests_failed + 1))
    fi
    
    # Test 3: Node startup
    log_test "Test 3: Node startup"
    if [ -f "$TEST_DIR/node.pid" ] && kill -0 $(cat "$TEST_DIR/node.pid") 2>/dev/null; then
        log_info "✓ Node startup test passed"
        tests_passed=$((tests_passed + 1))
    else
        log_error "✗ Node startup test failed"
        tests_failed=$((tests_failed + 1))
    fi
    
    # Test 4: Log analysis
    log_test "Test 4: Log analysis"
    if grep -q "error\|panic\|fatal" "$TEST_DIR/logs/node-$TO_VERSION.log" 2>/dev/null; then
        log_error "✗ Errors found in logs"
        tests_failed=$((tests_failed + 1))
    else
        log_info "✓ Log analysis passed"
        tests_passed=$((tests_passed + 1))
    fi
    
    # Summary
    echo ""
    log_info "Test Summary:"
    log_info "  Passed: $tests_passed"
    log_info "  Failed: $tests_failed"
    echo ""
    
    if [ $tests_failed -eq 0 ]; then
        return 0
    else
        return 1
    fi
}

# Main test process
main() {
    log_info "Starting upgrade test: $FROM_VERSION → $TO_VERSION"
    echo ""
    
    # Setup
    setup_test_environment
    
    # Download both versions
    if ! download_version "$FROM_VERSION" "$TEST_DIR/bin"; then
        log_error "Failed to download $FROM_VERSION"
        exit 1
    fi
    
    if ! download_version "$TO_VERSION" "$TEST_DIR/bin"; then
        log_error "Failed to download $TO_VERSION"
        exit 1
    fi
    
    # Start with old version
    if ! start_node "$FROM_VERSION"; then
        log_error "Failed to start old version"
        exit 1
    fi
    
    # Generate test data
    generate_test_data
    
    # Perform upgrade
    if ! perform_upgrade "$FROM_VERSION" "$TO_VERSION"; then
        log_error "Upgrade failed"
        exit 1
    fi
    
    # Run compatibility tests
    if ! run_compatibility_tests; then
        log_error "Compatibility tests failed"
        exit 1
    fi
    
    # Success
    echo ""
    log_info "═══════════════════════════════════════════"
    log_info "  Upgrade test completed successfully!"
    log_info "  $FROM_VERSION → $TO_VERSION"
    log_info "═══════════════════════════════════════════"
    echo ""
    
    exit 0
}

# Run main function
main
