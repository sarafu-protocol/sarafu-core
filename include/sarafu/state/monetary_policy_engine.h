#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <map>
#include <set>
#include "sarafu/state/adaptive_inflation.h"

namespace sarafu {
namespace state {

/**
 * Monetary state tracking supply, stake, and issuance
 */
struct MonetaryState {
    uint64_t total_supply;
    uint64_t total_bonded_stake;
    double staking_ratio;
    double issuance_rate;
    uint64_t fees_burned_this_epoch;
    uint64_t tokens_issued_this_epoch;
};

/**
 * Issuance parameters with governance bounds
 */
struct IssuanceParameters {
    double k;       // Issuance coefficient (default 0.1)
    double min_k;   // Minimum k (0.05)
    double max_k;   // Maximum k (0.2)
};

/**
 * MonetaryPolicyEngine implements dynamic issuance and fee burning
 * 
 * The engine calculates issuance based on the staking ratio:
 * - Issuance rate: rt = k·σ where σ = Stotal / Mt
 * - Supply update: Mt+1 = Mt·(1 + rt/blocks_per_year) - Bt
 * - Validator gross APR = k (independent of staking ratio)
 * 
 * Requirements: 11.1, 11.2, 11.4, 11.5, 11.6, 11.7
 */
class MonetaryPolicyEngine {
public:
    /**
     * Constructor with initial supply and stake
     */
    MonetaryPolicyEngine(uint64_t initial_supply, uint64_t initial_bonded_stake);

    /**
     * Calculate staking ratio σ = Stotal / Mt
     * 
     * Requirements: 11.1, 11.6
     */
    double calculate_staking_ratio() const;

    /**
     * Calculate annual issuance rate rt = k·σ
     * 
     * Requirements: 11.1
     */
    double calculate_issuance_rate() const;

    /**
     * Update supply after block: Mt+1 = Mt·(1 + rt/blocks_per_year) - Bt
     * 
     * @param fees_burned Base fees burned in this block
     * @param blocks_per_year Number of blocks per year (for per-block issuance calculation)
     * 
     * Requirements: 11.2
     */
    void update_supply(uint64_t fees_burned, uint64_t blocks_per_year);

    /**
     * Distribute rewards to validators at epoch end
     * 
     * @param validator_stakes Map of validator IDs to their stake amounts
     * @param jailed_validators Set of validator IDs that are jailed/slashed
     * @param withdrawal_addresses Map of validator IDs to withdrawal addresses
     * @return Map of withdrawal addresses to reward amounts
     * 
     * Requirements: 11.3, 20.1, 20.2, 20.3, 20.4, 20.6
     */
    std::map<std::string, uint64_t> distribute_rewards(
        const std::map<std::string, uint64_t>& validator_stakes,
        const std::set<std::string>& jailed_validators,
        const std::map<std::string, std::string>& withdrawal_addresses
    );

    /**
     * Update issuance coefficient k (via governance)
     * 
     * @param new_k New issuance coefficient
     * @return Error message if new_k is out of bounds, empty optional otherwise
     * 
     * Requirements: 11.5
     */
    std::optional<std::string> set_issuance_coefficient(double new_k);

    /**
     * Update total bonded stake (called when validators bond/unbond)
     */
    void update_bonded_stake(uint64_t new_bonded_stake);

    /**
     * Get current monetary state
     */
    const MonetaryState& state() const { return state_; }

    /**
     * Get issuance parameters
     */
    const IssuanceParameters& parameters() const { return params_; }
    
    /**
     * Get adaptive inflation engine.
     * 
     * @return Reference to adaptive inflation engine
     */
    AdaptiveInflationEngine& adaptive_inflation() { return adaptive_inflation_; }
    const AdaptiveInflationEngine& adaptive_inflation() const { return adaptive_inflation_; }
    
    /**
     * Update inflation rate based on staking ratio (called at epoch boundary).
     * 
     * @param current_epoch Current epoch number
     * @return New inflation rate
     */
    double update_adaptive_inflation(uint64_t current_epoch);

    /**
     * Reset epoch counters (called at epoch boundary)
     */
    void reset_epoch_counters();

private:
    MonetaryState state_;
    IssuanceParameters params_;
    AdaptiveInflationEngine adaptive_inflation_;
};

} // namespace state
} // namespace sarafu
