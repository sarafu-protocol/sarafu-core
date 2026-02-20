#include "sarafu/state/monetary_policy_engine.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace sarafu {
namespace state {

MonetaryPolicyEngine::MonetaryPolicyEngine(uint64_t initial_supply, uint64_t initial_bonded_stake) {
    state_.total_supply = initial_supply;
    state_.total_bonded_stake = initial_bonded_stake;
    state_.staking_ratio = 0.0;
    state_.issuance_rate = 0.0;
    state_.fees_burned_this_epoch = 0;
    state_.tokens_issued_this_epoch = 0;

    // Initialize parameters with defaults
    params_.k = 0.1;      // Default issuance coefficient
    params_.min_k = 0.05; // Minimum bound
    params_.max_k = 0.2;  // Maximum bound

    // Calculate initial staking ratio and issuance rate
    state_.staking_ratio = calculate_staking_ratio();
    state_.issuance_rate = calculate_issuance_rate();
}

double MonetaryPolicyEngine::calculate_staking_ratio() const {
    // σ = Stotal / Mt
    if (state_.total_supply == 0) {
        return 0.0;
    }
    return static_cast<double>(state_.total_bonded_stake) / static_cast<double>(state_.total_supply);
}

double MonetaryPolicyEngine::calculate_issuance_rate() const {
    // rt = k·σ
    double sigma = calculate_staking_ratio();
    return params_.k * sigma;
}

void MonetaryPolicyEngine::update_supply(uint64_t fees_burned, uint64_t blocks_per_year) {
    // Calculate per-block issuance: (rt * Mt) / blocks_per_year
    double annual_rate = calculate_issuance_rate();
    double per_block_issuance = (annual_rate * static_cast<double>(state_.total_supply)) / 
                                 static_cast<double>(blocks_per_year);
    
    uint64_t tokens_issued = static_cast<uint64_t>(std::round(per_block_issuance));

    // Update supply: Mt+1 = Mt + tokens_issued - fees_burned
    // This is equivalent to: Mt+1 = Mt·(1 + rt/blocks_per_year) - Bt
    state_.total_supply = state_.total_supply + tokens_issued - fees_burned;

    // Track epoch totals
    state_.fees_burned_this_epoch += fees_burned;
    state_.tokens_issued_this_epoch += tokens_issued;

    // Recalculate staking ratio and issuance rate
    state_.staking_ratio = calculate_staking_ratio();
    state_.issuance_rate = calculate_issuance_rate();
}

std::map<std::string, uint64_t> MonetaryPolicyEngine::distribute_rewards(
    const std::map<std::string, uint64_t>& validator_stakes,
    const std::set<std::string>& jailed_validators,
    const std::map<std::string, std::string>& withdrawal_addresses
) {
    std::map<std::string, uint64_t> rewards;

    // Calculate total stake of eligible validators (exclude jailed/slashed)
    uint64_t eligible_stake = 0;
    for (const auto& [validator_id, stake] : validator_stakes) {
        if (jailed_validators.find(validator_id) == jailed_validators.end()) {
            eligible_stake += stake;
        }
    }

    // If no eligible validators, return empty rewards
    if (eligible_stake == 0) {
        return rewards;
    }

    // Total rewards = tokens issued this epoch
    uint64_t total_rewards = state_.tokens_issued_this_epoch;

    // Distribute proportional to stake
    for (const auto& [validator_id, stake] : validator_stakes) {
        // Skip jailed/slashed validators
        if (jailed_validators.find(validator_id) != jailed_validators.end()) {
            continue;
        }

        // Calculate proportional reward
        double proportion = static_cast<double>(stake) / static_cast<double>(eligible_stake);
        uint64_t reward = static_cast<uint64_t>(std::round(proportion * static_cast<double>(total_rewards)));

        // Credit to withdrawal address
        auto it = withdrawal_addresses.find(validator_id);
        if (it != withdrawal_addresses.end()) {
            const std::string& withdrawal_addr = it->second;
            rewards[withdrawal_addr] += reward;
        }
    }

    return rewards;
}

std::optional<std::string> MonetaryPolicyEngine::set_issuance_coefficient(double new_k) {
    // Validate bounds: 0.05 ≤ k ≤ 0.2
    if (new_k < params_.min_k || new_k > params_.max_k) {
        return "Issuance coefficient must be between " + 
               std::to_string(params_.min_k) + " and " + 
               std::to_string(params_.max_k);
    }

    params_.k = new_k;

    // Recalculate issuance rate with new coefficient
    state_.issuance_rate = calculate_issuance_rate();

    return std::nullopt;
}

void MonetaryPolicyEngine::update_bonded_stake(uint64_t new_bonded_stake) {
    state_.total_bonded_stake = new_bonded_stake;

    // Recalculate staking ratio and issuance rate
    state_.staking_ratio = calculate_staking_ratio();
    state_.issuance_rate = calculate_issuance_rate();
}

void MonetaryPolicyEngine::reset_epoch_counters() {
    state_.fees_burned_this_epoch = 0;
    state_.tokens_issued_this_epoch = 0;
}

} // namespace state
} // namespace sarafu
