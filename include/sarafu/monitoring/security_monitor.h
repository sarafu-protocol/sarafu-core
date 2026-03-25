#pragma once

#include <vector>
#include <string>
#include <map>
#include <cstdint>
#include <chrono>
#include <mutex>

namespace sarafu {
namespace monitoring {

/**
 * @brief Security metric calculations for network decentralization monitoring
 * 
 * Implements calculations for:
 * - Nakamoto coefficient (minimum entities to control 1/3 of stake)
 * - Jurisdictional Nakamoto coefficient (geographic decentralization)
 * - Liquidity-adjusted attack cost (real-world cost to acquire 1/3 stake)
 * - HHI (Herfindahl-Hirschman Index) for stake concentration
 */
class SecurityMonitor {
public:
    enum class AlertLevel {
        Info,
        Warning,
        Critical
    };

    struct StakeConcentrationEntry {
        std::string entity_id;
        double stake_percentage;
    };

    struct GeographicDistributionEntry {
        std::string jurisdiction;
        double stake_percentage;
    };

    struct ValidatorLivenessEntry {
        std::string validator_id;
        double liveness_percentage;
        bool is_online;
    };

    struct AlertEntry {
        AlertLevel level;
        std::string source;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
    };

    static SecurityMonitor& instance();

    /**
     * @brief Validator information for security calculations
     */
    struct ValidatorInfo {
        std::string id;
        double stake_sar;
        std::string jurisdiction;  // ISO country code
        std::string entity_id;     // Controlling entity identifier
    };
    
    /**
     * @brief Calculate Nakamoto coefficient
     * 
     * The Nakamoto coefficient is the minimum number of entities required
     * to control more than 1/3 of the total stake.
     * 
     * @param validators List of validators with stake information
     * @return Nakamoto coefficient (minimum entities for 1/3 attack)
     */
    static uint64_t calculateNakamotoCoefficient(const std::vector<ValidatorInfo>& validators);
    
    /**
     * @brief Calculate jurisdictional Nakamoto coefficient
     * 
     * Similar to Nakamoto coefficient but groups validators by jurisdiction.
     * Measures geographic decentralization.
     * 
     * @param validators List of validators with jurisdiction information
     * @return Jurisdictional Nakamoto coefficient
     */
    static uint64_t calculateJurisdictionalNakamoto(const std::vector<ValidatorInfo>& validators);
    
    /**
     * @brief Calculate liquidity-adjusted attack cost
     * 
     * Estimates the real-world USD cost to acquire 1/3 of total stake,
     * accounting for market liquidity and slippage.
     * 
     * @param validators List of validators with stake information
     * @param sar_price_usd Current SAR price in USD
     * @param liquidity_depth_usd Available liquidity in USD
     * @param slippage_factor Slippage multiplier (e.g., 1.5 for 50% slippage)
     * @return Estimated attack cost in USD
     */
    static double calculateAttackCost(
        const std::vector<ValidatorInfo>& validators,
        double sar_price_usd,
        double liquidity_depth_usd,
        double slippage_factor = 1.5
    );
    
    /**
     * @brief Calculate Herfindahl-Hirschman Index (HHI)
     * 
     * HHI measures market concentration. Calculated as the sum of squared
     * market shares (in percentage points).
     * 
     * HHI ranges:
     * - < 1500: Unconcentrated market
     * - 1500-2500: Moderate concentration
     * - > 2500: High concentration
     * 
     * @param validators List of validators with stake information
     * @return HHI value
     */
    static double calculateHHI(const std::vector<ValidatorInfo>& validators);
    
    /**
     * @brief Calculate stake distribution by entity
     * 
     * Groups validators by controlling entity and sums their stake.
     * 
     * @param validators List of validators with entity information
     * @return Map of entity_id to total stake
     */
    static std::map<std::string, double> calculateEntityStakeDistribution(
        const std::vector<ValidatorInfo>& validators
    );
    
    /**
     * @brief Calculate stake distribution by jurisdiction
     * 
     * Groups validators by jurisdiction and sums their stake.
     * 
     * @param validators List of validators with jurisdiction information
     * @return Map of jurisdiction to total stake
     */
    static std::map<std::string, double> calculateJurisdictionStakeDistribution(
        const std::vector<ValidatorInfo>& validators
    );

    // Live monitoring APIs
    void update_stake_concentration(const std::string& entity_id, uint64_t stake, uint64_t total_stake);
    std::vector<StakeConcentrationEntry> get_stake_concentration() const;
    bool check_stake_concentration_alert(double threshold) const;

    void update_geographic_distribution(const std::string& jurisdiction, uint64_t validator_count, uint64_t stake);
    std::vector<GeographicDistributionEntry> get_geographic_distribution() const;

    double calculate_nakamoto_coefficient() const;

    void record_block_propagation(uint64_t block_height, std::chrono::milliseconds latency);
    double get_propagation_95th_percentile() const;
    bool check_propagation_alert(std::chrono::milliseconds threshold) const;

    void update_validator_liveness(const std::string& validator_id, uint64_t signed_blocks, uint64_t missed_blocks);
    std::vector<ValidatorLivenessEntry> get_validator_liveness() const;
    double calculate_offline_percentage() const;
    bool check_liveness_alert(double threshold) const;

    void record_double_sign_attempt(const std::string& validator_id, uint64_t block_height);
    uint64_t get_double_sign_count() const;

    void add_alert(AlertLevel level, const std::string& source, const std::string& message);
    std::vector<AlertEntry> get_recent_alerts(size_t limit) const;
    void clear_old_alerts(uint64_t max_age_seconds);

    std::string export_security_metrics() const;

private:
    /**
     * @brief Calculate coefficient from stake distribution
     * 
     * Generic helper for calculating Nakamoto-style coefficients.
     * 
     * @param stake_distribution Map of entity/jurisdiction to stake
     * @param total_stake Total stake across all entities
     * @return Coefficient (minimum entities for 1/3 control)
     */
    static uint64_t calculateCoefficientFromDistribution(
        const std::map<std::string, double>& stake_distribution,
        double total_stake
    );

    mutable std::mutex mutex_;
    std::map<std::string, uint64_t> stake_by_entity_;
    std::map<std::string, uint64_t> stake_by_jurisdiction_;
    std::map<std::string, uint64_t> validator_counts_by_jurisdiction_;
    std::map<std::string, std::pair<uint64_t, uint64_t>> liveness_by_validator_;
    std::vector<double> propagation_latencies_ms_;
    std::vector<AlertEntry> alerts_;
    uint64_t total_stake_ = 0;
    uint64_t double_sign_count_ = 0;
};

using AlertLevel = SecurityMonitor::AlertLevel;

} // namespace monitoring
} // namespace sarafu
