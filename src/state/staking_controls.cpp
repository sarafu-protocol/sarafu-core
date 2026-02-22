#include "sarafu/state/staking_controls.h"
#include <algorithm>

namespace sarafu {
namespace state {

StakingControls::StakingControls(const StakingControlsConfig& config)
    : config_(config) {}

bool StakingControls::initiate_unbonding(
    const consensus::ValidatorID& validator_id,
    uint64_t amount,
    uint64_t current_height
) {
    if (amount == 0) {
        return false;
    }
    
    // Calculate completion height (28 days from now)
    uint64_t completion_height = current_height + config_.unbonding_period_blocks;
    
    // Create unbonding entry
    UnbondingEntry entry(validator_id, amount, completion_height, current_height);
    
    // Add to queue
    size_t index = unbonding_queue_.size();
    unbonding_queue_.push_back(entry);
    
    // Update validator index
    validator_unbonding_index_[validator_id].push_back(index);
    
    return true;
}

std::vector<UnbondingEntry> StakingControls::process_completed_unbonding(
    uint64_t current_height
) {
    std::vector<UnbondingEntry> completed;
    std::vector<UnbondingEntry> remaining;
    
    // Separate completed and remaining entries
    for (const auto& entry : unbonding_queue_) {
        if (current_height >= entry.completion_height) {
            completed.push_back(entry);
        } else {
            remaining.push_back(entry);
        }
    }
    
    // Update queue with remaining entries
    unbonding_queue_ = remaining;
    
    // Rebuild validator index
    validator_unbonding_index_.clear();
    for (size_t i = 0; i < unbonding_queue_.size(); ++i) {
        validator_unbonding_index_[unbonding_queue_[i].validator_id].push_back(i);
    }
    
    return completed;
}

std::vector<UnbondingEntry> StakingControls::get_unbonding_entries(
    const consensus::ValidatorID& validator_id
) const {
    std::vector<UnbondingEntry> entries;
    
    auto it = validator_unbonding_index_.find(validator_id);
    if (it != validator_unbonding_index_.end()) {
        for (size_t index : it->second) {
            if (index < unbonding_queue_.size()) {
                entries.push_back(unbonding_queue_[index]);
            }
        }
    }
    
    return entries;
}

uint64_t StakingControls::get_unbonding_amount(
    const consensus::ValidatorID& validator_id
) const {
    uint64_t total = 0;
    
    auto entries = get_unbonding_entries(validator_id);
    for (const auto& entry : entries) {
        total += entry.amount;
    }
    
    return total;
}

uint64_t StakingControls::get_voting_eligible_stake(
    const consensus::ValidatorID& validator_id,
    uint64_t bonded_stake
) const {
    // Only bonded stake (not unbonding) is eligible for voting
    // Unbonding stake is excluded from governance
    return bonded_stake;
}

uint64_t StakingControls::slash_unbonding_stake(
    const consensus::ValidatorID& validator_id,
    uint64_t slash_amount
) {
    uint64_t remaining_slash = slash_amount;
    uint64_t total_slashed = 0;
    
    auto it = validator_unbonding_index_.find(validator_id);
    if (it == validator_unbonding_index_.end()) {
        return 0;
    }
    
    // Slash unbonding entries in order
    for (size_t index : it->second) {
        if (remaining_slash == 0) {
            break;
        }
        
        if (index >= unbonding_queue_.size()) {
            continue;
        }
        
        UnbondingEntry& entry = unbonding_queue_[index];
        uint64_t to_slash = std::min(entry.amount, remaining_slash);
        
        entry.amount -= to_slash;
        total_slashed += to_slash;
        remaining_slash -= to_slash;
    }
    
    // Remove entries with zero amount
    std::vector<UnbondingEntry> filtered;
    for (const auto& entry : unbonding_queue_) {
        if (entry.amount > 0) {
            filtered.push_back(entry);
        }
    }
    unbonding_queue_ = filtered;
    
    // Rebuild validator index
    validator_unbonding_index_.clear();
    for (size_t i = 0; i < unbonding_queue_.size(); ++i) {
        validator_unbonding_index_[unbonding_queue_[i].validator_id].push_back(i);
    }
    
    return total_slashed;
}

} // namespace state
} // namespace sarafu
