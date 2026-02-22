#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <prometheus/exposer.h>

namespace sarafu {
namespace monitoring {

/**
 * @brief Centralized metrics collection for Prometheus monitoring
 * 
 * Exports metrics in Prometheus format for:
 * - Consensus (block height, finalization, block time, propagation)
 * - Validators (count, stake, uptime, missed blocks)
 * - Transactions (mempool size, TPS, fees)
 * - Network (peer count, bandwidth, latency)
 * - Resources (memory, CPU, disk usage)
 * - Security (Nakamoto coefficient, attack cost, HHI)
 */
class MetricsCollector {
public:
    /**
     * @brief Initialize metrics collector with Prometheus exposer
     * @param bind_address Address to bind Prometheus HTTP server (e.g., "0.0.0.0:9090")
     */
    explicit MetricsCollector(const std::string& bind_address = "0.0.0.0:9090");
    
    ~MetricsCollector();
    
    // Consensus metrics
    void setBlockHeight(uint64_t height);
    void setFinalizedHeight(uint64_t height);
    void recordBlockTime(double seconds);
    void recordBlockPropagation(double milliseconds);
    void incrementViewChanges();
    void recordQCSize(uint64_t bytes);
    
    // Validator metrics
    void setValidatorCount(uint64_t count);
    void setValidatorStake(const std::string& validator_id, double stake_sar);
    void setValidatorUptime(const std::string& validator_id, double uptime_ratio);
    void incrementValidatorMissedBlocks(const std::string& validator_id);
    void incrementValidatorSlashed();
    
    // Transaction metrics
    void setMempoolSize(uint64_t size);
    void recordTransactionThroughput(double tps);
    void recordTransactionValidationDuration(double milliseconds);
    void recordTransactionFee(double fee_sar);
    
    // Network metrics
    void setPeerCount(uint64_t count);
    void recordNetworkBandwidth(uint64_t bytes);
    void recordMessageLatency(double milliseconds);
    
    // Resource metrics
    void setMemoryUsage(uint64_t bytes);
    void setCPUUsage(double percent);
    void setDiskUsage(uint64_t bytes);
    void recordDiskIO(uint64_t bytes);
    
    // Security metrics
    void setNakamotoCoefficient(uint64_t coefficient);
    void setJurisdictionalNakamoto(uint64_t coefficient);
    void setAttackCost(double cost_usd);
    void setStakeConcentrationHHI(double hhi);
    
    /**
     * @brief Get the Prometheus registry for advanced usage
     */
    std::shared_ptr<prometheus::Registry> getRegistry() const { return registry_; }

private:
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer> exposer_;
    
    // Consensus metrics
    prometheus::Gauge* block_height_;
    prometheus::Gauge* finalized_height_;
    prometheus::Histogram* block_time_;
    prometheus::Histogram* block_propagation_;
    prometheus::Counter* view_changes_;
    prometheus::Gauge* qc_size_;
    
    // Validator metrics
    prometheus::Gauge* validator_count_;
    prometheus::Family<prometheus::Gauge>* validator_stake_family_;
    prometheus::Family<prometheus::Gauge>* validator_uptime_family_;
    prometheus::Family<prometheus::Counter>* validator_missed_blocks_family_;
    prometheus::Counter* validator_slashed_;
    
    // Transaction metrics
    prometheus::Gauge* mempool_size_;
    prometheus::Gauge* tx_throughput_;
    prometheus::Histogram* tx_validation_duration_;
    prometheus::Histogram* tx_fee_;
    
    // Network metrics
    prometheus::Gauge* peer_count_;
    prometheus::Counter* network_bandwidth_;
    prometheus::Histogram* message_latency_;
    
    // Resource metrics
    prometheus::Gauge* memory_usage_;
    prometheus::Gauge* cpu_usage_;
    prometheus::Gauge* disk_usage_;
    prometheus::Counter* disk_io_;
    
    // Security metrics
    prometheus::Gauge* nakamoto_coefficient_;
    prometheus::Gauge* jurisdictional_nakamoto_;
    prometheus::Gauge* attack_cost_;
    prometheus::Gauge* stake_concentration_hhi_;
};

} // namespace monitoring
} // namespace sarafu
