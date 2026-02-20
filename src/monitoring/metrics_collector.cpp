#include "sarafu/monitoring/metrics_collector.h"
#include <sstream>

namespace sarafu {
namespace monitoring {

MetricsCollector::MetricsCollector() 
    : block_height_(0),
      finalized_height_(0),
      peer_count_(0),
      mempool_size_(0),
      validator_status_(ValidatorStatus::Standby),
      sync_status_(SyncStatus::NotSynced) {
}

void MetricsCollector::set_block_height(uint64_t height) {
    block_height_.store(height, std::memory_order_relaxed);
}

void MetricsCollector::set_finalized_height(uint64_t height) {
    finalized_height_.store(height, std::memory_order_relaxed);
}

void MetricsCollector::set_peer_count(uint64_t count) {
    peer_count_.store(count, std::memory_order_relaxed);
}

void MetricsCollector::set_mempool_size(uint64_t size) {
    mempool_size_.store(size, std::memory_order_relaxed);
}

void MetricsCollector::set_validator_status(ValidatorStatus status) {
    validator_status_.store(status, std::memory_order_relaxed);
}

void MetricsCollector::set_sync_status(SyncStatus status) {
    sync_status_.store(status, std::memory_order_relaxed);
}

uint64_t MetricsCollector::get_block_height() const {
    return block_height_.load(std::memory_order_relaxed);
}

uint64_t MetricsCollector::get_finalized_height() const {
    return finalized_height_.load(std::memory_order_relaxed);
}

uint64_t MetricsCollector::get_peer_count() const {
    return peer_count_.load(std::memory_order_relaxed);
}

uint64_t MetricsCollector::get_mempool_size() const {
    return mempool_size_.load(std::memory_order_relaxed);
}

ValidatorStatus MetricsCollector::get_validator_status() const {
    return validator_status_.load(std::memory_order_relaxed);
}

SyncStatus MetricsCollector::get_sync_status() const {
    return sync_status_.load(std::memory_order_relaxed);
}

std::string MetricsCollector::export_prometheus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    
    // Block height metric
    oss << "# HELP sarafu_block_height Current block height\n";
    oss << "# TYPE sarafu_block_height gauge\n";
    oss << "sarafu_block_height " << get_block_height() << "\n\n";
    
    // Finalized height metric
    oss << "# HELP sarafu_finalized_height Current finalized block height\n";
    oss << "# TYPE sarafu_finalized_height gauge\n";
    oss << "sarafu_finalized_height " << get_finalized_height() << "\n\n";
    
    // Peer count metric
    oss << "# HELP sarafu_peer_count Number of connected peers\n";
    oss << "# TYPE sarafu_peer_count gauge\n";
    oss << "sarafu_peer_count " << get_peer_count() << "\n\n";
    
    // Mempool size metric
    oss << "# HELP sarafu_mempool_size Number of transactions in mempool\n";
    oss << "# TYPE sarafu_mempool_size gauge\n";
    oss << "sarafu_mempool_size " << get_mempool_size() << "\n\n";
    
    // Validator status metric
    oss << "# HELP sarafu_validator_status Validator status (0=Active, 1=Standby, 2=Jailed, 3=Tombstoned)\n";
    oss << "# TYPE sarafu_validator_status gauge\n";
    oss << "sarafu_validator_status{status=\"" << validator_status_to_string(get_validator_status()) << "\"} ";
    oss << static_cast<int>(get_validator_status()) << "\n\n";
    
    // Sync status metric
    oss << "# HELP sarafu_sync_status Sync status (0=Syncing, 1=Synced, 2=NotSynced)\n";
    oss << "# TYPE sarafu_sync_status gauge\n";
    oss << "sarafu_sync_status{status=\"" << sync_status_to_string(get_sync_status()) << "\"} ";
    oss << static_cast<int>(get_sync_status()) << "\n\n";
    
    return oss.str();
}

std::string MetricsCollector::validator_status_to_string(ValidatorStatus status) const {
    switch (status) {
        case ValidatorStatus::Active: return "active";
        case ValidatorStatus::Standby: return "standby";
        case ValidatorStatus::Jailed: return "jailed";
        case ValidatorStatus::Tombstoned: return "tombstoned";
        default: return "unknown";
    }
}

std::string MetricsCollector::sync_status_to_string(SyncStatus status) const {
    switch (status) {
        case SyncStatus::Syncing: return "syncing";
        case SyncStatus::Synced: return "synced";
        case SyncStatus::NotSynced: return "not_synced";
        default: return "unknown";
    }
}

MetricsCollector& MetricsCollector::instance() {
    static MetricsCollector collector;
    return collector;
}

} // namespace monitoring
} // namespace sarafu
