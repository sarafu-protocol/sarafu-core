#include "sarafu/consensus/validator_registry.h"
#include "sarafu/consensus/slashing_detector.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace sarafu {
namespace consensus {

ValidatorRegistry::ValidatorRegistry(const Config& config)
    : config_(config),
      current_set_(),
      validators_(),
      unbonding_requests_() {
}

std::optional<Validator> ValidatorRegistry::get_validator(const ValidatorID& id) const {
    auto it = validators_.find(id);
    if (it != validators_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool ValidatorRegistry::add_validator(
    const ValidatorID& id,
    const crypto::BLS12_381_PublicKey& consensus_key,
    const crypto::Ed25519_PublicKey& withdrawal_key,
    uint64_t initial_stake
) {
    // Check if validator already exists
    if (validators_.find(id) != validators_.end()) {
        return false;
    }

    // Create new validator with Standby status
    Validator validator(id, consensus_key, withdrawal_key, initial_stake);
    validator.status = ValidatorStatus::Standby;
    
    validators_[id] = validator;
    return true;
}

bool ValidatorRegistry::update_validator(
    const ValidatorID& id,
    const std::optional<crypto::BLS12_381_PublicKey>& consensus_key,
    const std::optional<crypto::Ed25519_PublicKey>& withdrawal_key,
    const std::optional<ValidatorStatus>& new_status
) {
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }

    if (consensus_key.has_value()) {
        it->second.consensus_key = consensus_key.value();
    }
    
    if (withdrawal_key.has_value()) {
        it->second.withdrawal_key = withdrawal_key.value();
    }
    
    if (new_status.has_value()) {
        it->second.status = new_status.value();
    }
    
    // Update the validator in current_set_ if they're in it
    for (auto& v : current_set_.validators) {
        if (v.id == id) {
            if (consensus_key.has_value()) {
                v.consensus_key = consensus_key.value();
            }
            if (withdrawal_key.has_value()) {
                v.withdrawal_key = withdrawal_key.value();
            }
            if (new_status.has_value()) {
                // If validator is being tombstoned and was active, update total stake
                if (new_status.value() == ValidatorStatus::Tombstoned && v.status == ValidatorStatus::Active) {
                    if (current_set_.total_stake >= v.bonded_stake) {
                        current_set_.total_stake -= v.bonded_stake;
                    } else {
                        current_set_.total_stake = 0;
                    }
                }
                v.status = new_status.value();
            }
            break;
        }
    }

    return true;
}

bool ValidatorRegistry::bond_stake(const ValidatorID& id, uint64_t amount) {
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }

    it->second.bonded_stake += amount;
    
    // Update the validator in current_set_ if they're in it
    for (auto& v : current_set_.validators) {
        if (v.id == id) {
            v.bonded_stake += amount;
            // Update total stake if validator is active
            if (v.status == ValidatorStatus::Active) {
                current_set_.total_stake += amount;
            }
            break;
        }
    }
    
    return true;
}

bool ValidatorRegistry::unbond_stake(
    const ValidatorID& id,
    uint64_t amount,
    uint64_t current_height
) {
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }

    // Check if validator has sufficient bonded stake
    if (it->second.bonded_stake < amount) {
        return false;
    }

    // Check if remaining stake would be below minimum (unless unbonding all)
    uint64_t remaining_stake = it->second.bonded_stake - amount;
    if (remaining_stake > 0 && remaining_stake < config_.minimum_self_bond) {
        return false;
    }

    // Deduct from bonded stake
    it->second.bonded_stake -= amount;

    // Create unbonding request with 21-day delay
    uint64_t completion_height = current_height + config_.unbonding_period_blocks;
    unbonding_requests_.push_back(UnbondingRequest(id, amount, completion_height));

    // If unbonding all stake, move to Standby status
    if (remaining_stake == 0) {
        it->second.status = ValidatorStatus::Standby;
    }

    return true;
}

std::vector<UnbondingRequest> ValidatorRegistry::process_unbonding(uint64_t current_height) {
    std::vector<UnbondingRequest> completed;

    // Find all completed unbonding requests
    auto it = unbonding_requests_.begin();
    while (it != unbonding_requests_.end()) {
        if (it->completion_height <= current_height) {
            completed.push_back(*it);
            it = unbonding_requests_.erase(it);
        } else {
            ++it;
        }
    }

    return completed;
}

std::vector<Validator> ValidatorRegistry::rank_by_stake() const {
    std::vector<Validator> ranked;
    ranked.reserve(validators_.size());

    // Copy all validators
    for (const auto& pair : validators_) {
        ranked.push_back(pair.second);
    }

    // Sort by stake (descending), with lexicographic ID for ties
    std::sort(ranked.begin(), ranked.end());

    return ranked;
}

ValidatorSet ValidatorRegistry::transition_epoch(
    uint64_t new_epoch,
    uint64_t total_blocks_in_epoch
) {
    // Check for downtime violations BEFORE resetting counters
    if (new_epoch > 1) {
        check_downtime(new_epoch - 1, total_blocks_in_epoch);
    }

    // Rank all validators by stake
    std::vector<Validator> ranked = rank_by_stake();

    // Select top N validators as active set
    std::vector<Validator> new_validators;
    uint64_t new_total_stake = 0;

    for (size_t i = 0; i < ranked.size(); ++i) {
        Validator& v = ranked[i];

        // Reset per-epoch counters
        v.blocks_signed_this_epoch = 0;
        v.blocks_missed_this_epoch = 0;

        // Check if validator is jailed
        if (v.status == ValidatorStatus::Jailed && v.jailed_until_epoch <= new_epoch) {
            // Release from jail
            v.status = ValidatorStatus::Standby;
            v.jailed_until_epoch = 0;
        }

        // Determine if validator should be active
        // IMPORTANT: Tombstoned validators can NEVER become active again
        if (v.status == ValidatorStatus::Tombstoned) {
            // Tombstoned validators stay tombstoned forever
            // Don't add them to the new validator set or change their status
        } else if (i < config_.active_validator_count &&
                   v.bonded_stake >= config_.minimum_self_bond &&
                   v.status != ValidatorStatus::Jailed) {
            v.status = ValidatorStatus::Active;
            new_total_stake += v.bonded_stake;
        } else if (v.status == ValidatorStatus::Active) {
            // Was active but no longer qualifies
            v.status = ValidatorStatus::Standby;
        }

        new_validators.push_back(v);

        // Update in registry
        validators_[v.id] = v;
    }

    // Compute validator set root
    ValidatorSet new_set(new_epoch, new_validators, new_total_stake);
    new_set.merkle_root = compute_validator_set_root(new_set);

    // Update current set
    current_set_ = new_set;

    return new_set;
}

crypto::Blake3Hash ValidatorRegistry::compute_validator_set_root(
    const ValidatorSet& validator_set
) const {
    if (validator_set.validators.empty()) {
        return crypto::Blake3Hash::zero();
    }

    // Build Merkle tree from validator hashes
    std::vector<crypto::Blake3Hash> leaf_hashes;
    leaf_hashes.reserve(validator_set.validators.size());

    for (const auto& validator : validator_set.validators) {
        leaf_hashes.push_back(validator.hash());
    }

    crypto::MerkleTree tree;
    tree.build_tree(leaf_hashes);

    return tree.get_root();
}

bool ValidatorRegistry::require_epoch_transition_qc(
    const QuorumCertificate& qc,
    const ValidatorSet& validator_set
) const {
    // Calculate total stake of signers
    uint64_t total_stake_signed = 0;

    for (const auto& signer_id : qc.signers) {
        const Validator* validator = validator_set.find_validator(signer_id);
        if (validator != nullptr && validator->status == ValidatorStatus::Active) {
            total_stake_signed += validator->bonded_stake;
        }
    }

    // Check if ≥2/3 of total stake signed
    uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;  // Ceiling division
    return total_stake_signed >= required_stake;
}

bool ValidatorRegistry::record_signature(const ValidatorID& id, uint64_t /* block_height */) {
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }

    it->second.blocks_signed_this_epoch++;
    return true;
}

std::vector<ValidatorID> ValidatorRegistry::check_downtime(
    uint64_t epoch,
    uint64_t total_blocks_in_epoch
) {
    std::vector<ValidatorID> violators;

    if (total_blocks_in_epoch == 0) {
        return violators;
    }

    // Get the active validators from the current set to know who should have been signing
    auto active_validators = current_set_.get_active_validators();
    std::set<ValidatorID> active_ids;
    for (const auto& v : active_validators) {
        active_ids.insert(v.id);
    }

    for (auto& pair : validators_) {
        Validator& v = pair.second;

        // Only check validators who were active during the epoch
        if (active_ids.find(v.id) == active_ids.end()) {
            continue;
        }
        auto last_check = last_downtime_epoch_checked_.find(v.id);
        if (last_check != last_downtime_epoch_checked_.end() && last_check->second == epoch) {
            continue;
        }

        // Calculate signing rate based on blocks signed vs total blocks in epoch
        // Active validators have opportunity to sign all blocks
        double signing_rate = static_cast<double>(v.blocks_signed_this_epoch) / 
                             static_cast<double>(total_blocks_in_epoch);

        if (signing_rate < config_.downtime_threshold) {
            // Downtime violation detected
            violators.push_back(v.id);

            // Check if validator has already been penalized for this epoch
            // This can happen if check_downtime() is called multiple times for the same epoch
            bool already_penalized = false;
            if (v.consecutive_downtime_epochs > 0) {
                // If validator is jailed and jailed_until_epoch matches this epoch + 1,
                // they were just jailed in a previous call to check_downtime() for this epoch
                if (v.status == ValidatorStatus::Jailed && v.jailed_until_epoch == epoch + 1) {
                    already_penalized = true;
                }
            }

            if (!already_penalized) {
                if (v.consecutive_downtime_epochs == 0) {
                    // First violation: Jail for 1 epoch
                    v.status = ValidatorStatus::Jailed;
                    v.jailed_until_epoch = epoch + 1;
                    v.consecutive_downtime_epochs = 1;
                } else if (v.consecutive_downtime_epochs == 1) {
                    // Second consecutive violation: Slash 0.5%, move to Standby
                    uint64_t slash_amount = (v.bonded_stake * 5) / 1000;  // 0.5%
                    v.bonded_stake -= slash_amount;
                    v.status = ValidatorStatus::Standby;
                    v.consecutive_downtime_epochs = 2;
                } else {
                    // Third+ consecutive violation: Slash 1%, move to Standby
                    uint64_t slash_amount = v.bonded_stake / 100;  // 1%
                    v.bonded_stake -= slash_amount;
                    v.status = ValidatorStatus::Standby;
                    v.consecutive_downtime_epochs++;
                }
            }
        } else {
            // Good performance: Reset consecutive downtime counter
            v.consecutive_downtime_epochs = 0;
        }

        last_downtime_epoch_checked_[v.id] = epoch;
    }

    return violators;
}

bool ValidatorRegistry::slash_validator(
    const ValidatorID& id,
    uint64_t amount,
    SlashReason /* reason */
) {
    auto it = validators_.find(id);
    if (it == validators_.end()) {
        return false;
    }

    uint64_t actual_slash = amount;
    // Check if validator has sufficient stake
    if (it->second.bonded_stake < amount) {
        // Slash all remaining stake
        actual_slash = it->second.bonded_stake;
        it->second.bonded_stake = 0;
    } else {
        it->second.bonded_stake -= amount;
    }
    
    // Update the validator in current_set_ if they're in it
    for (auto& v : current_set_.validators) {
        if (v.id == id) {
            if (v.bonded_stake < actual_slash) {
                actual_slash = v.bonded_stake;
                v.bonded_stake = 0;
            } else {
                v.bonded_stake -= actual_slash;
            }
            // Update total stake if validator is active
            if (v.status == ValidatorStatus::Active) {
                if (current_set_.total_stake >= actual_slash) {
                    current_set_.total_stake -= actual_slash;
                } else {
                    current_set_.total_stake = 0;
                }
            }
            break;
        }
    }

    return true;
}

std::map<ValidatorID, uint64_t> ValidatorRegistry::distribute_rewards_proportional(
    uint64_t total_amount
) {
    std::map<ValidatorID, uint64_t> rewards;

    if (total_amount == 0 || current_set_.total_stake == 0) {
        return rewards;
    }

    // Get all active validators
    auto active_validators = current_set_.get_active_validators();

    if (active_validators.empty()) {
        return rewards;
    }

    // Distribute proportionally to stake
    uint64_t distributed = 0;
    for (size_t i = 0; i < active_validators.size(); ++i) {
        const auto& validator = active_validators[i];
        
        uint64_t reward;
        if (i == active_validators.size() - 1) {
            // Last validator gets remainder to avoid rounding issues
            reward = total_amount - distributed;
        } else {
            // Calculate proportional reward
            // reward = (validator_stake / total_stake) * total_amount
            // Use 128-bit arithmetic to avoid overflow
            __uint128_t numerator = static_cast<__uint128_t>(validator.bonded_stake) * 
                                   static_cast<__uint128_t>(total_amount);
            reward = static_cast<uint64_t>(numerator / current_set_.total_stake);
        }

        if (reward > 0) {
            rewards[validator.id] = reward;
            distributed += reward;
        }
    }

    return rewards;
}

} // namespace consensus
} // namespace sarafu
