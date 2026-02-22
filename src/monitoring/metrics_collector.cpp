#include "sarafu/monitoring/metrics_collector.h"
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>

namespace sarafu {
namespace monitoring {

MetricsCollector::MetricsCollector(const std::string& bind_address)
    : registry_(std::make_shared<prometheus::Registry>()),
      exposer_(std::make_unique<prometheus::Exposer>(bind_address)) {
    
    // Register the registry with the exposer
    exposer_->RegisterCollectable(registry_);
    
    // Initialize consensus metrics
    auto& block_height_family = prometheus::BuildGauge()
        .Name("sarafu_block_height")
        .Help("Current block height")
        .Register(*registry_);
    block_height_ = &block_height_family.Add({});
    
    auto& finalized_height_family = prometheus::BuildGauge()
        .Name("sarafu_finalized_height")
        .Help("Last finalized block height")
        .Register(*registry_);
    finalized_height_ = &finalized_height_family.Add({});
    
    auto& block_time_family = prometheus::BuildHistogram()
        .Name("sarafu_block_time_seconds")
        .Help("Time between blocks in seconds")
        .Register(*registry_);
    block_time_ = &block_time_family.Add({}, prometheus::Histogram::BucketBoundaries{0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 5.0, 10.0});
    
    auto& block_propagation_family = prometheus::BuildHistogram()
        .Name("sarafu_block_propagation_ms")
        .Help("Block propagation latency in milliseconds")
        .Register(*registry_);
    block_propagation_ = &block_propagation_family.Add({}, prometheus::Histogram::BucketBoundaries{50, 100, 150, 200, 250, 300, 400, 500, 1000});
    
    auto& view_changes_family = prometheus::BuildCounter()
        .Name("sarafu_view_changes_total")
        .Help("Total number of view changes")
        .Register(*registry_);
    view_changes_ = &view_changes_family.Add({});
    
    auto& qc_size_family = prometheus::BuildGauge()
        .Name("sarafu_qc_size_bytes")
        .Help("Quorum certificate size in bytes")
        .Register(*registry_);
    qc_size_ = &qc_size_family.Add({});
    
    // Initialize validator metrics
    auto& validator_count_family = prometheus::BuildGauge()
        .Name("sarafu_validator_count")
        .Help("Active validator count")
        .Register(*registry_);
    validator_count_ = &validator_count_family.Add({});
    
    validator_stake_family_ = &prometheus::BuildGauge()
        .Name("sarafu_validator_stake_sar")
        .Help("Stake per validator in SAR")
        .Register(*registry_);
    
    validator_uptime_family_ = &prometheus::BuildGauge()
        .Name("sarafu_validator_uptime_ratio")
        .Help("Validator uptime percentage")
        .Register(*registry_);
    
    validator_missed_blocks_family_ = &prometheus::BuildCounter()
        .Name("sarafu_validator_missed_blocks")
        .Help("Missed block count per validator")
        .Register(*registry_);
    
    auto& validator_slashed_family = prometheus::BuildCounter()
        .Name("sarafu_validator_slashed_total")
        .Help("Total number of slashing events")
        .Register(*registry_);
    validator_slashed_ = &validator_slashed_family.Add({});
    
    // Initialize transaction metrics
    auto& mempool_size_family = prometheus::BuildGauge()
        .Name("sarafu_mempool_size")
        .Help("Pending transaction count in mempool")
        .Register(*registry_);
    mempool_size_ = &mempool_size_family.Add({});
    
    auto& tx_throughput_family = prometheus::BuildGauge()
        .Name("sarafu_tx_throughput_tps")
        .Help("Transaction throughput in transactions per second")
        .Register(*registry_);
    tx_throughput_ = &tx_throughput_family.Add({});
    
    auto& tx_validation_duration_family = prometheus::BuildHistogram()
        .Name("sarafu_tx_validation_duration_ms")
        .Help("Transaction validation latency in milliseconds")
        .Register(*registry_);
    tx_validation_duration_ = &tx_validation_duration_family.Add({}, prometheus::Histogram::BucketBoundaries{0.1, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0});
    
    auto& tx_fee_family = prometheus::BuildHistogram()
        .Name("sarafu_tx_fee_sar")
        .Help("Transaction fees in SAR")
        .Register(*registry_);
    tx_fee_ = &tx_fee_family.Add({}, prometheus::Histogram::BucketBoundaries{0.001, 0.01, 0.1, 1.0, 10.0, 100.0});
    
    // Initialize network metrics
    auto& peer_count_family = prometheus::BuildGauge()
        .Name("sarafu_peer_count")
        .Help("Connected peer count")
        .Register(*registry_);
    peer_count_ = &peer_count_family.Add({});
    
    auto& network_bandwidth_family = prometheus::BuildCounter()
        .Name("sarafu_network_bandwidth_bytes")
        .Help("Network throughput in bytes")
        .Register(*registry_);
    network_bandwidth_ = &network_bandwidth_family.Add({});
    
    auto& message_latency_family = prometheus::BuildHistogram()
        .Name("sarafu_message_latency_ms")
        .Help("Message round-trip time in milliseconds")
        .Register(*registry_);
    message_latency_ = &message_latency_family.Add({}, prometheus::Histogram::BucketBoundaries{10, 25, 50, 100, 200, 500, 1000});
    
    // Initialize resource metrics
    auto& memory_usage_family = prometheus::BuildGauge()
        .Name("sarafu_memory_usage_bytes")
        .Help("Memory consumption in bytes")
        .Register(*registry_);
    memory_usage_ = &memory_usage_family.Add({});
    
    auto& cpu_usage_family = prometheus::BuildGauge()
        .Name("sarafu_cpu_usage_percent")
        .Help("CPU utilization percentage")
        .Register(*registry_);
    cpu_usage_ = &cpu_usage_family.Add({});
    
    auto& disk_usage_family = prometheus::BuildGauge()
        .Name("sarafu_disk_usage_bytes")
        .Help("Disk space used in bytes")
        .Register(*registry_);
    disk_usage_ = &disk_usage_family.Add({});
    
    auto& disk_io_family = prometheus::BuildCounter()
        .Name("sarafu_disk_io_bytes")
        .Help("Disk I/O throughput in bytes")
        .Register(*registry_);
    disk_io_ = &disk_io_family.Add({});
    
    // Initialize security metrics
    auto& nakamoto_coefficient_family = prometheus::BuildGauge()
        .Name("sarafu_nakamoto_coefficient")
        .Help("Minimum entities required for 1/3 attack")
        .Register(*registry_);
    nakamoto_coefficient_ = &nakamoto_coefficient_family.Add({});
    
    auto& jurisdictional_nakamoto_family = prometheus::BuildGauge()
        .Name("sarafu_jurisdictional_nakamoto")
        .Help("Jurisdictional decentralization coefficient")
        .Register(*registry_);
    jurisdictional_nakamoto_ = &jurisdictional_nakamoto_family.Add({});
    
    auto& attack_cost_family = prometheus::BuildGauge()
        .Name("sarafu_attack_cost_usd")
        .Help("Liquidity-adjusted attack cost in USD")
        .Register(*registry_);
    attack_cost_ = &attack_cost_family.Add({});
    
    auto& stake_concentration_hhi_family = prometheus::BuildGauge()
        .Name("sarafu_stake_concentration_hhi")
        .Help("Herfindahl-Hirschman Index for stake concentration")
        .Register(*registry_);
    stake_concentration_hhi_ = &stake_concentration_hhi_family.Add({});
}

MetricsCollector::~MetricsCollector() = default;

// Consensus metrics
void MetricsCollector::setBlockHeight(uint64_t height) {
    block_height_->Set(static_cast<double>(height));
}

void MetricsCollector::setFinalizedHeight(uint64_t height) {
    finalized_height_->Set(static_cast<double>(height));
}

void MetricsCollector::recordBlockTime(double seconds) {
    block_time_->Observe(seconds);
}

void MetricsCollector::recordBlockPropagation(double milliseconds) {
    block_propagation_->Observe(milliseconds);
}

void MetricsCollector::incrementViewChanges() {
    view_changes_->Increment();
}

void MetricsCollector::recordQCSize(uint64_t bytes) {
    qc_size_->Set(static_cast<double>(bytes));
}

// Validator metrics
void MetricsCollector::setValidatorCount(uint64_t count) {
    validator_count_->Set(static_cast<double>(count));
}

void MetricsCollector::setValidatorStake(const std::string& validator_id, double stake_sar) {
    auto& gauge = validator_stake_family_->Add({{"validator", validator_id}});
    gauge.Set(stake_sar);
}

void MetricsCollector::setValidatorUptime(const std::string& validator_id, double uptime_ratio) {
    auto& gauge = validator_uptime_family_->Add({{"validator", validator_id}});
    gauge.Set(uptime_ratio);
}

void MetricsCollector::incrementValidatorMissedBlocks(const std::string& validator_id) {
    auto& counter = validator_missed_blocks_family_->Add({{"validator", validator_id}});
    counter.Increment();
}

void MetricsCollector::incrementValidatorSlashed() {
    validator_slashed_->Increment();
}

// Transaction metrics
void MetricsCollector::setMempoolSize(uint64_t size) {
    mempool_size_->Set(static_cast<double>(size));
}

void MetricsCollector::recordTransactionThroughput(double tps) {
    tx_throughput_->Set(tps);
}

void MetricsCollector::recordTransactionValidationDuration(double milliseconds) {
    tx_validation_duration_->Observe(milliseconds);
}

void MetricsCollector::recordTransactionFee(double fee_sar) {
    tx_fee_->Observe(fee_sar);
}

// Network metrics
void MetricsCollector::setPeerCount(uint64_t count) {
    peer_count_->Set(static_cast<double>(count));
}

void MetricsCollector::recordNetworkBandwidth(uint64_t bytes) {
    network_bandwidth_->Increment(static_cast<double>(bytes));
}

void MetricsCollector::recordMessageLatency(double milliseconds) {
    message_latency_->Observe(milliseconds);
}

// Resource metrics
void MetricsCollector::setMemoryUsage(uint64_t bytes) {
    memory_usage_->Set(static_cast<double>(bytes));
}

void MetricsCollector::setCPUUsage(double percent) {
    cpu_usage_->Set(percent);
}

void MetricsCollector::setDiskUsage(uint64_t bytes) {
    disk_usage_->Set(static_cast<double>(bytes));
}

void MetricsCollector::recordDiskIO(uint64_t bytes) {
    disk_io_->Increment(static_cast<double>(bytes));
}

// Security metrics
void MetricsCollector::setNakamotoCoefficient(uint64_t coefficient) {
    nakamoto_coefficient_->Set(static_cast<double>(coefficient));
}

void MetricsCollector::setJurisdictionalNakamoto(uint64_t coefficient) {
    jurisdictional_nakamoto_->Set(static_cast<double>(coefficient));
}

void MetricsCollector::setAttackCost(double cost_usd) {
    attack_cost_->Set(cost_usd);
}

void MetricsCollector::setStakeConcentrationHHI(double hhi) {
    stake_concentration_hhi_->Set(hhi);
}

} // namespace monitoring
} // namespace sarafu
