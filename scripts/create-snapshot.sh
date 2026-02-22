#!/bin/bash
# Automated snapshot creation script
# This script creates a blockchain state snapshot at the current block height

set -e

# Configuration
DB_PATH="${SARAFU_DB_PATH:-$HOME/.sarafu/data}"
SNAPSHOT_DIR="${SARAFU_SNAPSHOT_DIR:-$HOME/.sarafu/snapshots}"
SIGNING_KEY="${SARAFU_SIGNING_KEY:-$HOME/.sarafu/keys/snapshot_key.bin}"
SAR_CLI="${SAR_CLI:-sar}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if required tools are available
check_requirements() {
    if ! command -v "$SAR_CLI" &> /dev/null; then
        log_error "sar CLI not found. Please install it or set SAR_CLI environment variable."
        exit 1
    fi

    if [ ! -f "$SIGNING_KEY" ]; then
        log_error "Signing key not found at: $SIGNING_KEY"
        log_error "Please generate a signing key or set SARAFU_SIGNING_KEY environment variable."
        exit 1
    fi

    if [ ! -d "$DB_PATH" ]; then
        log_error "Database path not found: $DB_PATH"
        log_error "Please set SARAFU_DB_PATH environment variable."
        exit 1
    fi
}

# Get current block height from the node
get_current_height() {
    # TODO: Implement RPC call to get current block height
    # For now, return a placeholder
    echo "0"
}

# Get block hash at height
get_block_hash() {
    local height=$1
    # TODO: Implement RPC call to get block hash
    # For now, return a placeholder
    echo "0000000000000000000000000000000000000000000000000000000000000000"
}

# Get state root at height
get_state_root() {
    local height=$1
    # TODO: Implement RPC call to get state root
    # For now, return a placeholder
    echo "0000000000000000000000000000000000000000000000000000000000000000"
}

# Create snapshot
create_snapshot() {
    local height=$1
    local block_hash=$2
    local state_root=$3

    log_info "Creating snapshot at height $height..."

    "$SAR_CLI" snapshot create \
        --db-path "$DB_PATH" \
        --snapshot-dir "$SNAPSHOT_DIR" \
        --height "$height" \
        --block-hash "$block_hash" \
        --state-root "$state_root" \
        --signing-key "$SIGNING_KEY"

    if [ $? -eq 0 ]; then
        log_info "Snapshot created successfully"
        return 0
    else
        log_error "Snapshot creation failed"
        return 1
    fi
}

# Clean up old snapshots (keep last N snapshots)
cleanup_old_snapshots() {
    local keep_count=${1:-10}

    log_info "Cleaning up old snapshots (keeping last $keep_count)..."

    # List all snapshots sorted by name (which includes height)
    local snapshots=($(ls -1 "$SNAPSHOT_DIR"/snapshot-*.tar.gz 2>/dev/null | sort -V))
    local total=${#snapshots[@]}

    if [ $total -le $keep_count ]; then
        log_info "No cleanup needed ($total snapshots, keeping $keep_count)"
        return 0
    fi

    local delete_count=$((total - keep_count))
    log_info "Deleting $delete_count old snapshot(s)..."

    for ((i=0; i<delete_count; i++)); do
        local snapshot="${snapshots[$i]}"
        log_info "Deleting: $(basename "$snapshot")"
        "$SAR_CLI" snapshot delete --snapshot "$snapshot"
    done

    log_info "Cleanup completed"
}

# Verify snapshot
verify_snapshot() {
    local snapshot_path=$1

    log_info "Verifying snapshot: $(basename "$snapshot_path")"

    "$SAR_CLI" snapshot verify --snapshot "$snapshot_path"

    if [ $? -eq 0 ]; then
        log_info "Snapshot verification passed"
        return 0
    else
        log_error "Snapshot verification failed"
        return 1
    fi
}

# Main function
main() {
    log_info "=========================================="
    log_info "Automated Snapshot Creation"
    log_info "=========================================="
    log_info "Database:     $DB_PATH"
    log_info "Snapshot dir: $SNAPSHOT_DIR"
    log_info "Signing key:  $SIGNING_KEY"
    log_info ""

    # Check requirements
    check_requirements

    # Get current blockchain state
    log_info "Fetching current blockchain state..."
    HEIGHT=$(get_current_height)
    BLOCK_HASH=$(get_block_hash "$HEIGHT")
    STATE_ROOT=$(get_state_root "$HEIGHT")

    log_info "Current height: $HEIGHT"
    log_info "Block hash:     $BLOCK_HASH"
    log_info "State root:     $STATE_ROOT"
    log_info ""

    # Create snapshot
    if ! create_snapshot "$HEIGHT" "$BLOCK_HASH" "$STATE_ROOT"; then
        exit 1
    fi

    # Find the created snapshot
    SNAPSHOT_PATH="$SNAPSHOT_DIR/snapshot-$HEIGHT-${BLOCK_HASH:0:8}.tar.gz"

    # Verify snapshot
    if [ -f "$SNAPSHOT_PATH" ]; then
        if ! verify_snapshot "$SNAPSHOT_PATH"; then
            log_warn "Snapshot verification failed, but snapshot was created"
        fi
    fi

    # Clean up old snapshots
    cleanup_old_snapshots 10

    log_info ""
    log_info "=========================================="
    log_info "Snapshot creation completed successfully"
    log_info "=========================================="
}

# Run main function
main "$@"
