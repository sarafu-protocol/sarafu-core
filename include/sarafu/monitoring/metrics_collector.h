#pragma once

#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace sarafu {
namespace monitoring {

enum class ValidatorStatus {
    Active,
    Standby,
    Jailed,
    Tombstoned
};

enum class SyncStatus {
    Syncing,
    Synced,
    NotSynced
};

class MetricsCollector {
public:
    MetricsCollector();

    // Update metrics
    void set_block_height(uint64_t height);
    void set_finalized_height(uint64_t height);
    void set_peer_count(uint64_t count);
    void set_mempool_size(uint64_t size);
    void set_validator_status(ValidatorStatus status);
    void set_sync_status(SyncStatus status);

    // Get metrics
    uint64_t get_block_height() const;
    uint64_t get_finalized_height() const;
    uint64_t get_peer_count() const;
    uint64_t get_mempool_size() const;
    ValidatorStatus get_validator_status() const;
    SyncStatus get_sync_status() const;

    // Export metrics in Prometheus format
    std::string export_prometheus() const;

    // Get singleton instance
    static MetricsCollector& instance();

private:
    std::string validator_status_to_string(ValidatorStatus status) const;
    std::string sync_status_to_string(SyncStatus status) const;

    std::atomic<uint64_t> block_height_;
    std::atomic<uint64_t> finalized_height_;
    std::atomic<uint64_t> peer_count_;
    std::atomic<uint64_t> mempool_size_;
    std::atomic<ValidatorStatus> validator_status_;
    std::atomic<SyncStatus> sync_status_;
    
    mutable std::mutex mutex_;
};

} // namespace monitoring
} // namespace sarafu
