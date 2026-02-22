#!/bin/bash
# Automated snapshot restoration script
# This script restores blockchain state from a snapshot

set -e

# Configuration
DB_PATH="${SARAFU_DB_PATH:-$HOME/.sarafu/data}"
SNAPSHOT_DIR="${SARAFU_SNAPSHOT_DIR:-$HOME/.sarafu/snapshots}"
SAR_CLI="${SAR_CLI:-sar}"
VERIFY_SIGNATURE="${SARAFU_VERIFY_SIGNATURE:-true}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Print usage
print_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Restore blockchain state from a snapshot

Options:
    --snapshot <path>       Path to snapshot file (required)
    --db-path <path>        Path to RocksDB database (default: ~/.sarafu/data)
    --no-verify-signature   Skip signature verification
    --backup                Create backup of existing database before restoration
    --help                  Show this help message

Environment Variables:
    SARAFU_DB_PATH          Database path
    SARAFU_SNAPSHOT_DIR     Snapshot directory
    SARAFU_VERIFY_SIGNATURE Verify signature (true/false)
    SAR_CLI                 Path to sar CLI binary

Examples:
    $0 --snapshot ./snapshots/snapshot-12345-abc.tar.gz
    $0 --snapshot ./snapshots/snapshot-12345-abc.tar.gz --backup
    $0 --snapshot ./snapshots/snapshot-12345-abc.tar.gz --no-verify-signature

EOF
}

# Check if required tools are available
check_requirements() {
    if ! command -v "$SAR_CLI" &> /dev/null; then
        log_error "sar CLI not found. Please install it or set SAR_CLI environment variable."
        exit 1
    fi
}

# Backup existing database
backup_database() {
    if [ ! -d "$DB_PATH" ]; then
        log_warn "Database path does not exist, skipping backup"
        return 0
    fi

    local backup_path="${DB_PATH}.backup.$(date +%Y%m%d_%H%M%S)"
    log_step "Creating backup of existing database..."
    log_info "Backup location: $backup_path"

    cp -r "$DB_PATH" "$backup_path"

    if [ $? -eq 0 ]; then
        log_info "Backup created successfully"
        echo "$backup_path" > "${DB_PATH}.last_backup"
        return 0
    else
        log_error "Backup creation failed"
        return 1
    fi
}

# Verify snapshot before restoration
verify_snapshot() {
    local snapshot_path=$1

    log_step "Verifying snapshot integrity..."

    local verify_args="--snapshot $snapshot_path"
    if [ "$VERIFY_SIGNATURE" != "true" ]; then
        verify_args="$verify_args --no-verify-signature"
    fi

    "$SAR_CLI" snapshot verify $verify_args

    if [ $? -eq 0 ]; then
        log_info "Snapshot verification passed"
        return 0
    else
        log_error "Snapshot verification failed"
        return 1
    fi
}

# Show snapshot information
show_snapshot_info() {
    local snapshot_path=$1

    log_step "Snapshot information:"
    "$SAR_CLI" snapshot info --snapshot "$snapshot_path"
    echo ""
}

# Stop node if running
stop_node() {
    log_step "Checking if node is running..."

    # Check if systemd service exists
    if systemctl is-active --quiet sarafu-node 2>/dev/null; then
        log_warn "Node is running as systemd service"
        log_info "Stopping sarafu-node service..."
        sudo systemctl stop sarafu-node
        log_info "Node stopped"
        return 0
    fi

    # Check if process is running
    if pgrep -x "sarafu-node" > /dev/null; then
        log_warn "Node process is running"
        log_info "Please stop the node manually before restoration"
        read -p "Press Enter when node is stopped, or Ctrl+C to cancel..."
    else
        log_info "Node is not running"
    fi
}

# Restore snapshot
restore_snapshot() {
    local snapshot_path=$1

    log_step "Restoring snapshot..."

    local restore_args="--snapshot $snapshot_path --db-path $DB_PATH"
    if [ "$VERIFY_SIGNATURE" != "true" ]; then
        restore_args="$restore_args --no-verify-signature"
    fi

    "$SAR_CLI" snapshot restore $restore_args

    if [ $? -eq 0 ]; then
        log_info "Snapshot restored successfully"
        return 0
    else
        log_error "Snapshot restoration failed"
        return 1
    fi
}

# Verify restored state
verify_restored_state() {
    log_step "Verifying restored state..."

    # TODO: Implement state verification
    # For now, just check if database directory exists
    if [ -d "$DB_PATH" ]; then
        log_info "Database directory exists"
        return 0
    else
        log_error "Database directory not found after restoration"
        return 1
    fi
}

# Main function
main() {
    local snapshot_path=""
    local create_backup=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --snapshot)
                snapshot_path="$2"
                shift 2
                ;;
            --db-path)
                DB_PATH="$2"
                shift 2
                ;;
            --no-verify-signature)
                VERIFY_SIGNATURE="false"
                shift
                ;;
            --backup)
                create_backup=true
                shift
                ;;
            --help|-h)
                print_usage
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                print_usage
                exit 1
                ;;
        esac
    done

    # Validate required arguments
    if [ -z "$snapshot_path" ]; then
        log_error "--snapshot is required"
        print_usage
        exit 1
    fi

    if [ ! -f "$snapshot_path" ]; then
        log_error "Snapshot file not found: $snapshot_path"
        exit 1
    fi

    log_info "=========================================="
    log_info "Automated Snapshot Restoration"
    log_info "=========================================="
    log_info "Snapshot:     $snapshot_path"
    log_info "Database:     $DB_PATH"
    log_info "Verify sig:   $VERIFY_SIGNATURE"
    log_info "Backup:       $create_backup"
    log_info ""

    # Check requirements
    check_requirements

    # Show snapshot information
    show_snapshot_info "$snapshot_path"

    # Confirm restoration
    log_warn "⚠️  WARNING: This will replace the current database!"
    read -p "Are you sure you want to continue? (yes/no): " confirm
    if [ "$confirm" != "yes" ]; then
        log_info "Restoration cancelled"
        exit 0
    fi
    echo ""

    # Stop node
    stop_node

    # Backup existing database if requested
    if [ "$create_backup" = true ]; then
        if ! backup_database; then
            log_error "Backup failed, aborting restoration"
            exit 1
        fi
        echo ""
    fi

    # Verify snapshot
    if ! verify_snapshot "$snapshot_path"; then
        log_error "Snapshot verification failed, aborting restoration"
        exit 1
    fi
    echo ""

    # Restore snapshot
    if ! restore_snapshot "$snapshot_path"; then
        log_error "Restoration failed"
        
        # Offer to restore backup if it exists
        if [ -f "${DB_PATH}.last_backup" ]; then
            local backup_path=$(cat "${DB_PATH}.last_backup")
            log_warn "A backup exists at: $backup_path"
            read -p "Do you want to restore the backup? (yes/no): " restore_backup
            if [ "$restore_backup" = "yes" ]; then
                log_info "Restoring backup..."
                rm -rf "$DB_PATH"
                cp -r "$backup_path" "$DB_PATH"
                log_info "Backup restored"
            fi
        fi
        
        exit 1
    fi
    echo ""

    # Verify restored state
    if ! verify_restored_state; then
        log_error "State verification failed"
        exit 1
    fi
    echo ""

    log_info "=========================================="
    log_info "✓ Restoration completed successfully!"
    log_info "=========================================="
    log_info ""
    log_info "Next steps:"
    log_info "  1. Verify the restored block height matches expectations"
    log_info "  2. Check database integrity"
    log_info "  3. Restart the node to resume consensus"
    log_info ""
    log_info "To restart the node:"
    log_info "  systemctl start sarafu-node"
    log_info ""
}

# Run main function
main "$@"
