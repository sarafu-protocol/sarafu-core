#pragma once

#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <cstdint>
#include <chrono>

namespace sarafu {
namespace monitoring {

struct StakeConcentration {
    std::string entity_id;
    uint64_t stake_amount;
    double stake_percentage;
};

struct GeographicDistribution {
    std::string jurisdiction;
    uint64_t validator_count;
    uint64_t total_stake;
    double stake_percentage;
};

struct BlockPropagationMetrics {
    uint64_t block_height;
    std::chrono::milliseconds propagation_time;
    uint64_t timestamp;
};

struct ValidatorLivenessMetrics {
    std::string validator_id;
    uint64_t blocks_signed;
    uint64_t blocks_missed;
    double liveness_percentage;
    bool is_online;
};

enum class AlertLevel {
    Info,
    Warning,
    Critical
};

struct SecurityAlert {
    AlertLevel level;
    std::string type;
    std::string message;
    uint64_t timestamp;
};

class SecurityMonitor {
public:
    SecurityMonitor();

    // Stake concentration monitoring
    void update_stake_concentration(const std::string& entity_id, uint64_t stake, uint64_t total_stake);
    std::vector<StakeConcentration> get_stake_concentration() const;
    bool check_stake_concentration_alert(double threshold = 0.20);

    // Geographic distribution monitoring
    void update_geographic_distribution(const std::string& jurisdiction, uint64_t validator_count, uint64_t stake);
    std::vector<GeographicDistribution> get_geographic_distribution() const;
    double calculate_nakamoto_coefficient() const;

    // Block propagation monitoring
    void record_block_propagation(uint64_t block_height, std::chrono::milliseconds propagation_time);
    double get_propagation_95th_percentile() const;
    bool check_propagation_alert(std::chrono::milliseconds threshold = std::chrono::milliseconds(500));

    // Validator liveness monitoring
    void update_validator_liveness(const std::string& validator_id, uint64_t blocks_signed, uint64_t blocks_missed);
    std::vector<ValidatorLivenessMetrics> get_validator_liveness() const;
    double calculate_offline_percentage() const;
    bool check_liveness_alert(double threshold = 0.10);

    // Double-signing monitoring
    void record_double_sign_attempt(const std::string& validator_id, uint64_t block_height);
    uint64_t get_double_sign_count() const;

    // Alert management
    void add_alert(AlertLevel level, const std::string& type, const std::string& message);
    std::vector<SecurityAlert> get_recent_alerts(size_t count = 100) const;
    void clear_old_alerts(uint64_t max_age_seconds = 86400);

    // Export security metrics
    std::string export_security_metrics() const;

    // Get singleton instance
    static SecurityMonitor& instance();

private:
    mutable std::mutex mutex_;
    
    // Stake concentration data
    std::map<std::string, StakeConcentration> stake_concentration_;
    uint64_t total_stake_;
    
    // Geographic distribution data
    std::map<std::string, GeographicDistribution> geographic_distribution_;
    
    // Block propagation data
    std::vector<BlockPropagationMetrics> propagation_history_;
    static constexpr size_t MAX_PROPAGATION_HISTORY = 1000;
    
    // Validator liveness data
    std::map<std::string, ValidatorLivenessMetrics> validator_liveness_;
    
    // Double-signing attempts
    std::map<std::string, std::vector<uint64_t>> double_sign_attempts_;
    
    // Alerts
    std::vector<SecurityAlert> alerts_;
    static constexpr size_t MAX_ALERTS = 1000;
    
    uint64_t get_current_timestamp() const;
};

} // namespace monitoring
} // namespace sarafu
