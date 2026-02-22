#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>
#include "sarafu/state/account.h"

namespace sarafu {
namespace consensus {
    using ValidatorID = state::Address;
}

namespace state {

/**
 * UnbondingEntry represents a single unbonding request.
 */
struct UnbondingEntry {
    consensus::ValidatorID validator_id;
    uint64_t amount;
    uint64_t completion_height;
    uint64_t creation_height;
    
    UnbondingEntry()
        : amount(0),
          completion_height(0),
          creation_height(0) {}
    
    UnbondingEntry(
        const consensus::ValidatorID& id,
        uint64_t amt,
        uint64_t completion,
        uint64_t creation
    ) : validator_id(id),
        amount(amt),
        completion_height(completion),
        creation_height(creation) {}
};

/**
 * StakingControlsConfig defines staking parameters.
 */
struct StakingControlsConfig {
    uint64_t unbonding_period_blocks;     // 28 days in blocks
    uint64_t blocks_per_day;              // Blocks per day (43200 for 2s blocks)
    
    StakingControlsConfig()
        : unbonding_period_blocks(1209600),  // 28 days * 43200 blocks/day
          blocks_per_day(43200) {}
};

/**
 * StakingControls implements staking security controls.
 * 
 * Features:
 * - 28-day unbonding period
 * - Only bonded stake eligible for governance voting
 * - Voting stake locked during proposal period
 * - Slashing applies to unbonding stake
 */
class StakingControls {
public:
    explicit StakingControls(const StakingControlsConfig& config = StakingControlsConfig());
    
    /**
     * Initiate unbonding of stake.
     * 
     * The stake enters a 28-day unbonding period during which:
     * - It cannot be used for voting
     * - It remains slashable
     * - It does not earn rewards
     * 
     * @param validator_id Validator ID
     * @param amount Amount to unbond
     * @param current_height Current block height
     * @return true if unbonding initiated successfully
     */
    bool initiate_unbonding(
        const consensus::ValidatorID& validator_id,
        uint64_t amount,
        uint64_t current_height
    );
    
    /**
     * Process completed unbonding entries.
     * 
     * Returns all unbonding entries that have completed their 28-day period.
     * 
     * @param current_height Current block height
     * @return Vector of completed unbonding entries
     */
    std::vector<UnbondingEntry> process_completed_unbonding(uint64_t current_height);
    
    /**
     * Get unbonding entries for a validator.
     * 
     * @param validator_id Validator ID
     * @return Vector of unbonding entries
     */
    std::vector<UnbondingEntry> get_unbonding_entries(
        const consensus::ValidatorID& validator_id
    ) const;
    
    /**
     * Get total unbonding amount for a validator.
     * 
     * @param validator_id Validator ID
     * @return Total amount currently unbonding
     */
    uint64_t get_unbonding_amount(const consensus::ValidatorID& validator_id) const;
    
    /**
     * Check if stake is eligible for governance voting.
     * 
     * Only bonded stake (not unbonding) is eligible.
     * 
     * @param validator_id Validator ID
     * @param bonded_stake Current bonded stake
     * @return Voting-eligible stake amount
     */
    uint64_t get_voting_eligible_stake(
        const consensus::ValidatorID& validator_id,
        uint64_t bonded_stake
    ) const;
    
    /**
     * Slash unbonding stake.
     * 
     * Applies slashing to unbonding entries for a validator.
     * 
     * @param validator_id Validator ID
     * @param slash_amount Amount to slash
     * @return Amount actually slashed
     */
    uint64_t slash_unbonding_stake(
        const consensus::ValidatorID& validator_id,
        uint64_t slash_amount
    );
    
    /**
     * Get all unbonding entries.
     * 
     * @return Vector of all unbonding entries
     */
    const std::vector<UnbondingEntry>& get_all_unbonding_entries() const {
        return unbonding_queue_;
    }
    
    /**
     * Get unbonding period in blocks.
     * 
     * @return Unbonding period (28 days in blocks)
     */
    uint64_t get_unbonding_period() const {
        return config_.unbonding_period_blocks;
    }

private:
    StakingControlsConfig config_;
    std::vector<UnbondingEntry> unbonding_queue_;
    std::map<consensus::ValidatorID, std::vector<size_t>> validator_unbonding_index_;
};

} // namespace state
} // namespace sarafu
