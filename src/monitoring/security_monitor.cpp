#include "sarafu/monitoring/security_monitor.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>

namespace sarafu {
namespace monitoring {

SecurityMonitor& SecurityMonitor::instance() {
    static SecurityMonitor monitor;
    return monitor;
}

uint64_t SecurityMonitor::calculateNakamotoCoefficient(
    const std::vector<ValidatorInfo>& validators) {
    
    // Group validators by entity and sum their stake
    auto entity_distribution = calculateEntityStakeDistribution(validators);
    
    // Calculate total stake
    double total_stake = 0.0;
    for (const auto& [entity, stake] : entity_distribution) {
        total_stake += stake;
    }
    
    return calculateCoefficientFromDistribution(entity_distribution, total_stake);
}

uint64_t SecurityMonitor::calculateJurisdictionalNakamoto(
    const std::vector<ValidatorInfo>& validators) {
    
    // Group validators by jurisdiction and sum their stake
    auto jurisdiction_distribution = calculateJurisdictionStakeDistribution(validators);
    
    // Calculate total stake
    double total_stake = 0.0;
    for (const auto& [jurisdiction, stake] : jurisdiction_distribution) {
        total_stake += stake;
    }
    
    return calculateCoefficientFromDistribution(jurisdiction_distribution, total_stake);
}

double SecurityMonitor::calculateAttackCost(
    const std::vector<ValidatorInfo>& validators,
    double sar_price_usd,
    double liquidity_depth_usd,
    double slippage_factor) {
    
    // Calculate total stake
    double total_stake = 0.0;
    for (const auto& validator : validators) {
        total_stake += validator.stake_sar;
    }
    
    // Calculate 1/3 of total stake needed for attack
    double attack_stake = total_stake / 3.0;
    
    // Base cost without slippage
    double base_cost = attack_stake * sar_price_usd;
    
    // Apply slippage factor if purchase exceeds liquidity depth
    double adjusted_cost = base_cost;
    if (base_cost > liquidity_depth_usd) {
        // Apply slippage to the portion exceeding liquidity depth
        double excess = base_cost - liquidity_depth_usd;
        adjusted_cost = liquidity_depth_usd + (excess * slippage_factor);
    }
    
    return adjusted_cost;
}

double SecurityMonitor::calculateHHI(const std::vector<ValidatorInfo>& validators) {
    // Group validators by entity
    auto entity_distribution = calculateEntityStakeDistribution(validators);
    
    // Calculate total stake
    double total_stake = 0.0;
    for (const auto& [entity, stake] : entity_distribution) {
        total_stake += stake;
    }
    
    if (total_stake == 0.0) {
        return 0.0;
    }
    
    // Calculate HHI as sum of squared market shares (in percentage points)
    double hhi = 0.0;
    for (const auto& [entity, stake] : entity_distribution) {
        double market_share_percent = (stake / total_stake) * 100.0;
        hhi += market_share_percent * market_share_percent;
    }
    
    return hhi;
}

std::map<std::string, double> SecurityMonitor::calculateEntityStakeDistribution(
    const std::vector<ValidatorInfo>& validators) {
    
    std::map<std::string, double> distribution;
    
    for (const auto& validator : validators) {
        // Use validator ID as entity if entity_id is empty
        std::string entity = validator.entity_id.empty() ? validator.id : validator.entity_id;
        distribution[entity] += validator.stake_sar;
    }
    
    return distribution;
}

std::map<std::string, double> SecurityMonitor::calculateJurisdictionStakeDistribution(
    const std::vector<ValidatorInfo>& validators) {
    
    std::map<std::string, double> distribution;
    
    for (const auto& validator : validators) {
        // Use "UNKNOWN" if jurisdiction is empty
        std::string jurisdiction = validator.jurisdiction.empty() ? "UNKNOWN" : validator.jurisdiction;
        distribution[jurisdiction] += validator.stake_sar;
    }
    
    return distribution;
}

uint64_t SecurityMonitor::calculateCoefficientFromDistribution(
    const std::map<std::string, double>& stake_distribution,
    double total_stake) {
    
    if (total_stake == 0.0 || stake_distribution.empty()) {
        return 0;
    }
    
    // Create sorted vector of stakes (descending order)
    std::vector<double> stakes;
    stakes.reserve(stake_distribution.size());
    for (const auto& [entity, stake] : stake_distribution) {
        stakes.push_back(stake);
    }
    std::sort(stakes.begin(), stakes.end(), std::greater<double>());
    
    // Calculate threshold (1/3 of total stake)
    double threshold = total_stake / 3.0;
    
    // Count entities needed to exceed threshold
    double cumulative_stake = 0.0;
    uint64_t count = 0;
    
    for (double stake : stakes) {
        cumulative_stake += stake;
        count++;
        
        if (cumulative_stake > threshold) {
            break;
        }
    }
    
    return count;
}

void SecurityMonitor::update_stake_concentration(
    const std::string& entity_id,
    uint64_t stake,
    uint64_t total_stake) {
    std::lock_guard<std::mutex> lock(mutex_);
    stake_by_entity_[entity_id] = stake;
    total_stake_ = total_stake;
}

std::vector<SecurityMonitor::StakeConcentrationEntry> SecurityMonitor::get_stake_concentration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StakeConcentrationEntry> entries;
    if (total_stake_ == 0) {
        return entries;
    }
    for (const auto& [entity, stake] : stake_by_entity_) {
        entries.push_back({entity, static_cast<double>(stake) / static_cast<double>(total_stake_)});
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.stake_percentage > b.stake_percentage; });
    return entries;
}

bool SecurityMonitor::check_stake_concentration_alert(double threshold) const {
    auto entries = get_stake_concentration();
    for (const auto& entry : entries) {
        if (entry.stake_percentage > threshold) {
            return true;
        }
    }
    return false;
}

void SecurityMonitor::update_geographic_distribution(
    const std::string& jurisdiction,
    uint64_t validator_count,
    uint64_t stake) {
    std::lock_guard<std::mutex> lock(mutex_);
    validator_counts_by_jurisdiction_[jurisdiction] = validator_count;
    stake_by_jurisdiction_[jurisdiction] = stake;
}

std::vector<SecurityMonitor::GeographicDistributionEntry> SecurityMonitor::get_geographic_distribution() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<GeographicDistributionEntry> entries;
    uint64_t total = 0;
    for (const auto& [jurisdiction, stake] : stake_by_jurisdiction_) {
        total += stake;
    }
    if (total == 0) {
        return entries;
    }
    for (const auto& [jurisdiction, stake] : stake_by_jurisdiction_) {
        entries.push_back({jurisdiction, static_cast<double>(stake) / static_cast<double>(total)});
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.stake_percentage > b.stake_percentage; });
    return entries;
}

double SecurityMonitor::calculate_nakamoto_coefficient() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stake_by_jurisdiction_.empty()) {
        return 0.0;
    }
    std::vector<uint64_t> stakes;
    uint64_t total = 0;
    for (const auto& [jurisdiction, stake] : stake_by_jurisdiction_) {
        stakes.push_back(stake);
        total += stake;
    }
    if (total == 0) {
        return 0.0;
    }
    std::sort(stakes.begin(), stakes.end(), std::greater<uint64_t>());
    double threshold = static_cast<double>(total) * (2.0 / 3.0);
    double cumulative = 0.0;
    uint64_t count = 0;
    for (auto stake : stakes) {
        cumulative += stake;
        count++;
        if (cumulative > threshold) {
            break;
        }
    }
    return static_cast<double>(count);
}

void SecurityMonitor::record_block_propagation(
    uint64_t block_height,
    std::chrono::milliseconds latency) {
    (void)block_height;
    std::lock_guard<std::mutex> lock(mutex_);
    propagation_latencies_ms_.push_back(static_cast<double>(latency.count()));
}

double SecurityMonitor::get_propagation_95th_percentile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (propagation_latencies_ms_.empty()) {
        return 0.0;
    }
    std::vector<double> sorted = propagation_latencies_ms_;
    std::sort(sorted.begin(), sorted.end());
    size_t index = static_cast<size_t>(std::ceil(0.95 * sorted.size())) - 1;
    if (index >= sorted.size()) {
        index = sorted.size() - 1;
    }
    return sorted[index];
}

bool SecurityMonitor::check_propagation_alert(std::chrono::milliseconds threshold) const {
    return get_propagation_95th_percentile() > static_cast<double>(threshold.count());
}

void SecurityMonitor::update_validator_liveness(
    const std::string& validator_id,
    uint64_t signed_blocks,
    uint64_t missed_blocks) {
    std::lock_guard<std::mutex> lock(mutex_);
    liveness_by_validator_[validator_id] = std::make_pair(signed_blocks, missed_blocks);
}

std::vector<SecurityMonitor::ValidatorLivenessEntry> SecurityMonitor::get_validator_liveness() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ValidatorLivenessEntry> entries;
    for (const auto& [validator_id, counts] : liveness_by_validator_) {
        auto total = counts.first + counts.second;
        double percentage = total == 0 ? 0.0 : static_cast<double>(counts.first) / static_cast<double>(total);
        bool online = percentage > 0.95;
        entries.push_back({validator_id, percentage, online});
    }
    return entries;
}

double SecurityMonitor::calculate_offline_percentage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (liveness_by_validator_.empty()) {
        return 0.0;
    }
    uint64_t offline = 0;
    for (const auto& [validator_id, counts] : liveness_by_validator_) {
        auto total = counts.first + counts.second;
        double percentage = total == 0 ? 0.0 : static_cast<double>(counts.first) / static_cast<double>(total);
        if (percentage <= 0.95) {
            offline++;
        }
    }
    return static_cast<double>(offline) / static_cast<double>(liveness_by_validator_.size());
}

bool SecurityMonitor::check_liveness_alert(double threshold) const {
    return calculate_offline_percentage() > threshold;
}

void SecurityMonitor::record_double_sign_attempt(
    const std::string& validator_id,
    uint64_t block_height) {
    (void)validator_id;
    (void)block_height;
    std::lock_guard<std::mutex> lock(mutex_);
    double_sign_count_++;
}

uint64_t SecurityMonitor::get_double_sign_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return double_sign_count_;
}

void SecurityMonitor::add_alert(AlertLevel level, const std::string& source, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    alerts_.push_back({level, source, message, std::chrono::system_clock::now()});
}

std::vector<SecurityMonitor::AlertEntry> SecurityMonitor::get_recent_alerts(size_t limit) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (limit >= alerts_.size()) {
        return alerts_;
    }
    return std::vector<AlertEntry>(alerts_.end() - static_cast<long>(limit), alerts_.end());
}

void SecurityMonitor::clear_old_alerts(uint64_t max_age_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    alerts_.erase(
        std::remove_if(alerts_.begin(), alerts_.end(),
            [now, max_age_seconds](const AlertEntry& alert) {
                auto age_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                    now - alert.timestamp
                ).count();
                return age_seconds > static_cast<long long>(max_age_seconds);
            }),
        alerts_.end()
    );
}

std::string SecurityMonitor::export_security_metrics() const {
    std::ostringstream oss;
    double top_concentration = 0.0;
    auto concentration = get_stake_concentration();
    if (!concentration.empty()) {
        top_concentration = concentration.front().stake_percentage;
    }

    oss << "# HELP sarafu_stake_concentration Top entity stake percentage\n";
    oss << "# TYPE sarafu_stake_concentration gauge\n";
    oss << "sarafu_stake_concentration " << top_concentration << "\n";

    oss << "# HELP sarafu_nakamoto_coefficient Nakamoto coefficient (jurisdictional)\n";
    oss << "# TYPE sarafu_nakamoto_coefficient gauge\n";
    oss << "sarafu_nakamoto_coefficient " << calculate_nakamoto_coefficient() << "\n";

    oss << "# HELP sarafu_block_propagation_p95 Block propagation 95th percentile (ms)\n";
    oss << "# TYPE sarafu_block_propagation_p95 gauge\n";
    oss << "sarafu_block_propagation_p95 " << get_propagation_95th_percentile() << "\n";

    oss << "# HELP sarafu_validator_offline_percentage Validator offline percentage\n";
    oss << "# TYPE sarafu_validator_offline_percentage gauge\n";
    oss << "sarafu_validator_offline_percentage " << calculate_offline_percentage() << "\n";

    oss << "# HELP sarafu_double_sign_attempts Double sign attempts\n";
    oss << "# TYPE sarafu_double_sign_attempts counter\n";
    oss << "sarafu_double_sign_attempts " << get_double_sign_count() << "\n";

    oss << "# HELP sarafu_security_alerts Security alert count\n";
    oss << "# TYPE sarafu_security_alerts gauge\n";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        oss << "sarafu_security_alerts " << alerts_.size() << "\n";
    }

    return oss.str();
}

} // namespace monitoring
} // namespace sarafu
