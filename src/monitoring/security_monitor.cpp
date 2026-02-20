#include "sarafu/monitoring/security_monitor.h"
#include "sarafu/logging/logger.h"
#include <algorithm>
#include <sstream>
#include <cmath>

namespace sarafu {
namespace monitoring {

SecurityMonitor::SecurityMonitor() : total_stake_(0) {
}

void SecurityMonitor::update_stake_concentration(const std::string& entity_id, uint64_t stake, uint64_t total_stake) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    total_stake_ = total_stake;
    double percentage = total_stake > 0 ? (static_cast<double>(stake) / total_stake) : 0.0;
    
    stake_concentration_[entity_id] = StakeConcentration{
        entity_id,
        stake,
        percentage
    };
    
    // Check for concentration alert
    if (percentage > 0.20) {
        std::ostringstream msg;
        msg << "Entity " << entity_id << " controls " << (percentage * 100) 
            << "% of total stake (threshold: 20%)";
        add_alert(AlertLevel::Critical, "stake_concentration", msg.str());
    }
}

std::vector<StakeConcentration> SecurityMonitor::get_stake_concentration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<StakeConcentration> result;
    for (const auto& pair : stake_concentration_) {
        result.push_back(pair.second);
    }
    
    // Sort by stake percentage descending
    std::sort(result.begin(), result.end(), 
        [](const StakeConcentration& a, const StakeConcentration& b) {
            return a.stake_percentage > b.stake_percentage;
        });
    
    return result;
}

bool SecurityMonitor::check_stake_concentration_alert(double threshold) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& pair : stake_concentration_) {
        if (pair.second.stake_percentage > threshold) {
            return true;
        }
    }
    return false;
}

void SecurityMonitor::update_geographic_distribution(const std::string& jurisdiction, 
                                                     uint64_t validator_count, 
                                                     uint64_t stake) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    double percentage = total_stake_ > 0 ? (static_cast<double>(stake) / total_stake_) : 0.0;
    
    geographic_distribution_[jurisdiction] = GeographicDistribution{
        jurisdiction,
        validator_count,
        stake,
        percentage
    };
}

std::vector<GeographicDistribution> SecurityMonitor::get_geographic_distribution() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<GeographicDistribution> result;
    for (const auto& pair : geographic_distribution_) {
        result.push_back(pair.second);
    }
    
    // Sort by stake percentage descending
    std::sort(result.begin(), result.end(), 
        [](const GeographicDistribution& a, const GeographicDistribution& b) {
            return a.stake_percentage > b.stake_percentage;
        });
    
    return result;
}

double SecurityMonitor::calculate_nakamoto_coefficient() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Nakamoto coefficient: minimum number of jurisdictions needed to control >50% stake
    std::vector<GeographicDistribution> sorted_dist;
    for (const auto& pair : geographic_distribution_) {
        sorted_dist.push_back(pair.second);
    }
    
    std::sort(sorted_dist.begin(), sorted_dist.end(), 
        [](const GeographicDistribution& a, const GeographicDistribution& b) {
            return a.stake_percentage > b.stake_percentage;
        });
    
    double cumulative_stake = 0.0;
    size_t count = 0;
    
    for (const auto& dist : sorted_dist) {
        cumulative_stake += dist.stake_percentage;
        count++;
        if (cumulative_stake > 0.50) {
            break;
        }
    }
    
    return static_cast<double>(count);
}

void SecurityMonitor::record_block_propagation(uint64_t block_height, 
                                               std::chrono::milliseconds propagation_time) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    propagation_history_.push_back(BlockPropagationMetrics{
        block_height,
        propagation_time,
        get_current_timestamp()
    });
    
    // Keep only recent history
    if (propagation_history_.size() > MAX_PROPAGATION_HISTORY) {
        propagation_history_.erase(propagation_history_.begin());
    }
}

double SecurityMonitor::get_propagation_95th_percentile() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (propagation_history_.empty()) {
        return 0.0;
    }
    
    std::vector<int64_t> times;
    for (const auto& metric : propagation_history_) {
        times.push_back(metric.propagation_time.count());
    }
    
    std::sort(times.begin(), times.end());
    
    size_t index = static_cast<size_t>(std::ceil(times.size() * 0.95)) - 1;
    if (index >= times.size()) {
        index = times.size() - 1;
    }
    
    return static_cast<double>(times[index]);
}

bool SecurityMonitor::check_propagation_alert(std::chrono::milliseconds threshold) {
    double p95 = get_propagation_95th_percentile();
    
    if (p95 > threshold.count()) {
        std::ostringstream msg;
        msg << "Block propagation 95th percentile (" << p95 
            << "ms) exceeds threshold (" << threshold.count() << "ms)";
        add_alert(AlertLevel::Warning, "block_propagation", msg.str());
        return true;
    }
    
    return false;
}

void SecurityMonitor::update_validator_liveness(const std::string& validator_id, 
                                                uint64_t blocks_signed, 
                                                uint64_t blocks_missed) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t total_blocks = blocks_signed + blocks_missed;
    double liveness = total_blocks > 0 ? 
        (static_cast<double>(blocks_signed) / total_blocks) : 0.0;
    
    validator_liveness_[validator_id] = ValidatorLivenessMetrics{
        validator_id,
        blocks_signed,
        blocks_missed,
        liveness,
        liveness > 0.95  // Consider online if >95% liveness
    };
}

std::vector<ValidatorLivenessMetrics> SecurityMonitor::get_validator_liveness() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<ValidatorLivenessMetrics> result;
    for (const auto& pair : validator_liveness_) {
        result.push_back(pair.second);
    }
    
    return result;
}

double SecurityMonitor::calculate_offline_percentage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (validator_liveness_.empty()) {
        return 0.0;
    }
    
    size_t offline_count = 0;
    for (const auto& pair : validator_liveness_) {
        if (!pair.second.is_online) {
            offline_count++;
        }
    }
    
    return static_cast<double>(offline_count) / validator_liveness_.size();
}

bool SecurityMonitor::check_liveness_alert(double threshold) {
    double offline_pct = calculate_offline_percentage();
    
    if (offline_pct > threshold) {
        std::ostringstream msg;
        msg << "Validator offline percentage (" << (offline_pct * 100) 
            << "%) exceeds threshold (" << (threshold * 100) << "%)";
        add_alert(AlertLevel::Critical, "validator_liveness", msg.str());
        return true;
    }
    
    return false;
}

void SecurityMonitor::record_double_sign_attempt(const std::string& validator_id, 
                                                 uint64_t block_height) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    double_sign_attempts_[validator_id].push_back(block_height);
    
    std::ostringstream msg;
    msg << "Double-sign attempt detected: validator=" << validator_id 
        << " height=" << block_height;
    add_alert(AlertLevel::Critical, "double_sign", msg.str());
    
    // Log the violation
    logging::Logger::instance().error("SecurityMonitor", msg.str());
}

uint64_t SecurityMonitor::get_double_sign_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t total = 0;
    for (const auto& pair : double_sign_attempts_) {
        total += pair.second.size();
    }
    return total;
}

void SecurityMonitor::add_alert(AlertLevel level, const std::string& type, const std::string& message) {
    // Note: mutex should already be locked by caller
    
    alerts_.push_back(SecurityAlert{
        level,
        type,
        message,
        get_current_timestamp()
    });
    
    // Keep only recent alerts
    if (alerts_.size() > MAX_ALERTS) {
        alerts_.erase(alerts_.begin());
    }
    
    // Log the alert
    std::string level_str;
    switch (level) {
        case AlertLevel::Info: level_str = "INFO"; break;
        case AlertLevel::Warning: level_str = "WARNING"; break;
        case AlertLevel::Critical: level_str = "CRITICAL"; break;
    }
    
    std::ostringstream log_msg;
    log_msg << "[" << level_str << "] " << type << ": " << message;
    logging::Logger::instance().warn("SecurityMonitor", log_msg.str());
}

std::vector<SecurityAlert> SecurityMonitor::get_recent_alerts(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SecurityAlert> result;
    size_t start = alerts_.size() > count ? alerts_.size() - count : 0;
    
    for (size_t i = start; i < alerts_.size(); i++) {
        result.push_back(alerts_[i]);
    }
    
    return result;
}

void SecurityMonitor::clear_old_alerts(uint64_t max_age_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t current_time = get_current_timestamp();
    
    alerts_.erase(
        std::remove_if(alerts_.begin(), alerts_.end(),
            [current_time, max_age_seconds](const SecurityAlert& alert) {
                return (current_time - alert.timestamp) > max_age_seconds;
            }),
        alerts_.end()
    );
}

std::string SecurityMonitor::export_security_metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    
    // Stake concentration metrics
    oss << "# HELP sarafu_stake_concentration Stake concentration by entity\n";
    oss << "# TYPE sarafu_stake_concentration gauge\n";
    for (const auto& pair : stake_concentration_) {
        oss << "sarafu_stake_concentration{entity=\"" << pair.first << "\"} " 
            << pair.second.stake_percentage << "\n";
    }
    oss << "\n";
    
    // Nakamoto coefficient
    double nakamoto = calculate_nakamoto_coefficient();
    oss << "# HELP sarafu_nakamoto_coefficient Nakamoto coefficient (jurisdictional)\n";
    oss << "# TYPE sarafu_nakamoto_coefficient gauge\n";
    oss << "sarafu_nakamoto_coefficient " << nakamoto << "\n\n";
    
    // Block propagation 95th percentile
    double p95 = get_propagation_95th_percentile();
    oss << "# HELP sarafu_block_propagation_p95 Block propagation 95th percentile (ms)\n";
    oss << "# TYPE sarafu_block_propagation_p95 gauge\n";
    oss << "sarafu_block_propagation_p95 " << p95 << "\n\n";
    
    // Validator offline percentage
    double offline_pct = calculate_offline_percentage();
    oss << "# HELP sarafu_validator_offline_percentage Percentage of offline validators\n";
    oss << "# TYPE sarafu_validator_offline_percentage gauge\n";
    oss << "sarafu_validator_offline_percentage " << offline_pct << "\n\n";
    
    // Double-sign attempts
    uint64_t double_signs = get_double_sign_count();
    oss << "# HELP sarafu_double_sign_attempts Total double-sign attempts detected\n";
    oss << "# TYPE sarafu_double_sign_attempts counter\n";
    oss << "sarafu_double_sign_attempts " << double_signs << "\n\n";
    
    // Active alerts by level
    size_t critical_count = 0, warning_count = 0, info_count = 0;
    for (const auto& alert : alerts_) {
        switch (alert.level) {
            case AlertLevel::Critical: critical_count++; break;
            case AlertLevel::Warning: warning_count++; break;
            case AlertLevel::Info: info_count++; break;
        }
    }
    
    oss << "# HELP sarafu_security_alerts Active security alerts by level\n";
    oss << "# TYPE sarafu_security_alerts gauge\n";
    oss << "sarafu_security_alerts{level=\"critical\"} " << critical_count << "\n";
    oss << "sarafu_security_alerts{level=\"warning\"} " << warning_count << "\n";
    oss << "sarafu_security_alerts{level=\"info\"} " << info_count << "\n\n";
    
    return oss.str();
}

uint64_t SecurityMonitor::get_current_timestamp() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

SecurityMonitor& SecurityMonitor::instance() {
    static SecurityMonitor monitor;
    return monitor;
}

} // namespace monitoring
} // namespace sarafu
