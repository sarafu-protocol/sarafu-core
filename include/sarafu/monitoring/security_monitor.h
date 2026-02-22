#pragma once

#include <vector>
#include <string>
#include <map>
#include <cstdint>

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
};

} // namespace monitoring
} // namespace sarafu
