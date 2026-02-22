#!/bin/bash
#
# Automated node upgrade script for Sarafu blockchain
#
# Usage: ./upgrade-node.sh <new_version> [--skip-backup] [--skip-verification]
#
# This script performs a safe upgrade of a Sarafu node:
# 1. Downloads and verifies the new binary
# 2. Creates a backup of the current binary
# 3. Stops the node
# 4. Installs the new binary
# 5. Starts the node
# 6. Verifies the upgrade was successful
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
INSTALL_DIR="/usr/local/bin"
BINARY_NAME="sarafu-node"
BACKUP_DIR="/var/backups/sarafu"
SERVICE_NAME="sarafu-node"
RELEASE_URL="https://releases.sarafu.network"
GPG_KEY_URL="https://releases.sarafu.network/signing-key.asc"

# Parse arguments
NEW_VERSION=""
SKIP_BACKUP=false
SKIP_VERIFICATION=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-backup)
            SKIP_BACKUP=true
            shift
            ;;
        --skip-verification)
            SKIP_VERIFICATION=true
            shift
            ;;
        *)
            if [ -z "$NEW_VERSION" ]; then
                NEW_VERSION=$1
            fi
            shift
            ;;
    esac
done

if [ -z "$NEW_VERSION" ]; then
    echo -e "${RED}Error: No version specified${NC}"
    echo "Usage: $0 <new_version> [--skip-backup] [--skip-verification]"
    echo "Example: $0 v1.1.0-p1"
    exit 1
fi

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

check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "This script must be run as root"
        exit 1
    fi
}

get_current_version() {
    if [ -f "$INSTALL_DIR/$BINARY_NAME" ]; then
        $INSTALL_DIR/$BINARY_NAME version 2>/dev/null || echo "unknown"
    else
        echo "not_installed"
    fi
}

download_binary() {
    local version=$1
    local download_url="$RELEASE_URL/$BINARY_NAME-$version"
    local sig_url="$download_url.sig"
    local output_file="/tmp/$BINARY_NAME-$version"
    
    log_info "Downloading $BINARY_NAME $version..."
    
    if ! wget -q "$download_url" -O "$output_file"; then
        log_error "Failed to download binary from $download_url"
        return 1
    fi
    
    if [ "$SKIP_VERIFICATION" = false ]; then
        log_info "Downloading signature..."
        if ! wget -q "$sig_url" -O "$output_file.sig"; then
            log_error "Failed to download signature from $sig_url"
            rm -f "$output_file"
            return 1
        fi
        
        log_info "Verifying signature..."
        if ! verify_signature "$output_file"; then
            log_error "Signature verification failed"
            rm -f "$output_file" "$output_file.sig"
            return 1
        fi
        
        log_info "Signature verification passed"
    else
        log_warn "Skipping signature verification (not recommended)"
    fi
    
    chmod +x "$output_file"
    echo "$output_file"
}

verify_signature() {
    local file=$1
    
    # Import GPG key if not already imported
    if ! gpg --list-keys sarafu-releases@example.com >/dev/null 2>&1; then
        log_info "Importing GPG signing key..."
        if ! wget -q "$GPG_KEY_URL" -O - | gpg --import; then
            log_error "Failed to import GPG key"
            return 1
        fi
    fi
    
    # Verify signature
    if gpg --verify "$file.sig" "$file" 2>/dev/null; then
        return 0
    else
        return 1
    fi
}

backup_current_binary() {
    if [ "$SKIP_BACKUP" = true ]; then
        log_warn "Skipping backup (not recommended)"
        return 0
    fi
    
    if [ ! -f "$INSTALL_DIR/$BINARY_NAME" ]; then
        log_info "No existing binary to backup"
        return 0
    fi
    
    local current_version=$(get_current_version)
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local backup_file="$BACKUP_DIR/${BINARY_NAME}_${current_version}_${timestamp}"
    
    log_info "Creating backup of current binary..."
    
    mkdir -p "$BACKUP_DIR"
    
    if cp "$INSTALL_DIR/$BINARY_NAME" "$backup_file"; then
        log_info "Backup created: $backup_file"
        return 0
    else
        log_error "Failed to create backup"
        return 1
    fi
}

stop_node() {
    log_info "Stopping $SERVICE_NAME service..."
    
    if systemctl is-active --quiet "$SERVICE_NAME"; then
        if systemctl stop "$SERVICE_NAME"; then
            log_info "Service stopped successfully"
            
            # Wait for service to fully stop
            local timeout=30
            local elapsed=0
            while systemctl is-active --quiet "$SERVICE_NAME" && [ $elapsed -lt $timeout ]; do
                sleep 1
                elapsed=$((elapsed + 1))
            done
            
            if systemctl is-active --quiet "$SERVICE_NAME"; then
                log_error "Service did not stop within $timeout seconds"
                return 1
            fi
            
            return 0
        else
            log_error "Failed to stop service"
            return 1
        fi
    else
        log_info "Service is not running"
        return 0
    fi
}

install_binary() {
    local new_binary=$1
    
    log_info "Installing new binary..."
    
    if mv "$new_binary" "$INSTALL_DIR/$BINARY_NAME"; then
        chmod +x "$INSTALL_DIR/$BINARY_NAME"
        log_info "Binary installed successfully"
        return 0
    else
        log_error "Failed to install binary"
        return 1
    fi
}

start_node() {
    log_info "Starting $SERVICE_NAME service..."
    
    if systemctl start "$SERVICE_NAME"; then
        log_info "Service started successfully"
        
        # Wait for service to be active
        local timeout=30
        local elapsed=0
        while ! systemctl is-active --quiet "$SERVICE_NAME" && [ $elapsed -lt $timeout ]; do
            sleep 1
            elapsed=$((elapsed + 1))
        done
        
        if systemctl is-active --quiet "$SERVICE_NAME"; then
            return 0
        else
            log_error "Service did not start within $timeout seconds"
            return 1
        fi
    else
        log_error "Failed to start service"
        return 1
    fi
}

verify_upgrade() {
    log_info "Verifying upgrade..."
    
    # Check version
    local installed_version=$(get_current_version)
    if [ "$installed_version" = "$NEW_VERSION" ]; then
        log_info "Version verified: $installed_version"
    else
        log_error "Version mismatch: expected $NEW_VERSION, got $installed_version"
        return 1
    fi
    
    # Check service status
    if systemctl is-active --quiet "$SERVICE_NAME"; then
        log_info "Service is running"
    else
        log_error "Service is not running"
        return 1
    fi
    
    # Check logs for errors
    log_info "Checking logs for errors..."
    if journalctl -u "$SERVICE_NAME" --since "1 minute ago" | grep -i "error" >/dev/null; then
        log_warn "Errors found in logs. Please review:"
        journalctl -u "$SERVICE_NAME" --since "1 minute ago" | grep -i "error" | tail -5
    else
        log_info "No errors found in recent logs"
    fi
    
    return 0
}

rollback() {
    log_error "Upgrade failed. Rolling back..."
    
    # Find most recent backup
    local latest_backup=$(ls -t "$BACKUP_DIR/${BINARY_NAME}_"* 2>/dev/null | head -1)
    
    if [ -z "$latest_backup" ]; then
        log_error "No backup found for rollback"
        return 1
    fi
    
    log_info "Restoring from backup: $latest_backup"
    
    # Stop service
    systemctl stop "$SERVICE_NAME" 2>/dev/null || true
    
    # Restore backup
    if cp "$latest_backup" "$INSTALL_DIR/$BINARY_NAME"; then
        chmod +x "$INSTALL_DIR/$BINARY_NAME"
        log_info "Backup restored"
        
        # Start service
        if systemctl start "$SERVICE_NAME"; then
            log_info "Service restarted with previous version"
            return 0
        else
            log_error "Failed to restart service"
            return 1
        fi
    else
        log_error "Failed to restore backup"
        return 1
    fi
}

# Main upgrade process
main() {
    log_info "Starting upgrade to $NEW_VERSION"
    
    # Check root privileges
    check_root
    
    # Get current version
    local current_version=$(get_current_version)
    log_info "Current version: $current_version"
    
    if [ "$current_version" = "$NEW_VERSION" ]; then
        log_info "Already running version $NEW_VERSION"
        exit 0
    fi
    
    # Download new binary
    local new_binary=$(download_binary "$NEW_VERSION")
    if [ $? -ne 0 ]; then
        log_error "Failed to download binary"
        exit 1
    fi
    
    # Backup current binary
    if ! backup_current_binary; then
        log_error "Backup failed"
        rm -f "$new_binary"
        exit 1
    fi
    
    # Stop node
    if ! stop_node; then
        log_error "Failed to stop node"
        rm -f "$new_binary"
        exit 1
    fi
    
    # Install new binary
    if ! install_binary "$new_binary"; then
        log_error "Installation failed"
        rollback
        exit 1
    fi
    
    # Start node
    if ! start_node; then
        log_error "Failed to start node"
        rollback
        exit 1
    fi
    
    # Verify upgrade
    if ! verify_upgrade; then
        log_error "Verification failed"
        rollback
        exit 1
    fi
    
    log_info "Upgrade completed successfully!"
    log_info "Node is now running version $NEW_VERSION"
    
    # Show status
    systemctl status "$SERVICE_NAME" --no-pager
}

# Run main function
main
