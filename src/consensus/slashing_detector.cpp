#include "sarafu/consensus/slashing_detector.h"
#include "sarafu/consensus/validator_registry.h"
#include <algorithm>
#include <cmath>

namespace sarafu {
namespace consensus {

// SignatureRecord implementation
SignatureRecord::SignatureRecord()
    : validator_id(state::Address::zero()),
      block_height(0),
      block_hash(crypto::Blake3Hash::zero()),
      signature(),
      timestamp(0) {
}

SignatureRecord::SignatureRecord(
    const ValidatorID& id,
    uint64_t height,
    const crypto::Blake3Hash& hash,
    const crypto::BLS12_381_Signature& sig,
    uint64_t ts
) : validator_id(id),
    block_height(height),
    block_hash(hash),
    signature(sig),
    timestamp(ts) {
}

bool SignatureRecord::operator==(const SignatureRecord& other) const {
    return validator_id == other.validator_id &&
           block_height == other.block_height &&
           block_hash == other.block_hash &&
           signature == other.signature &&
           timestamp == other.timestamp;
}

bool SignatureRecord::operator!=(const SignatureRecord& other) const {
    return !(*this == other);
}

// Vote implementation
Vote::Vote()
    : validator_id(state::Address::zero()),
      block_height(0),
      block_hash(crypto::Blake3Hash::zero()),
      view_number(0),
      signature() {
}

Vote::Vote(
    const ValidatorID& id,
    uint64_t height,
    const crypto::Blake3Hash& hash,
    uint64_t view,
    const crypto::BLS12_381_Signature& sig
) : validator_id(id),
    block_height(height),
    block_hash(hash),
    view_number(view),
    signature(sig) {
}

bool Vote::operator==(const Vote& other) const {
    return validator_id == other.validator_id &&
           block_height == other.block_height &&
           block_hash == other.block_hash &&
           view_number == other.view_number &&
           signature == other.signature;
}

bool Vote::operator!=(const Vote& other) const {
    return !(*this == other);
}

// SlashingEvent implementation
SlashingEvent::SlashingEvent()
    : validator_id(state::Address::zero()),
      block_height(0),
      reason(SlashReason::DoubleSign),
      penalty_amount(0),
      co_violators() {
}

SlashingEvent::SlashingEvent(
    const ValidatorID& id,
    uint64_t height,
    SlashReason rsn,
    uint64_t penalty,
    const std::vector<ValidatorID>& co_viol
) : validator_id(id),
    block_height(height),
    reason(rsn),
    penalty_amount(penalty),
    co_violators(co_viol) {
}

bool SlashingEvent::operator==(const SlashingEvent& other) const {
    return validator_id == other.validator_id &&
           block_height == other.block_height &&
           reason == other.reason &&
           penalty_amount == other.penalty_amount &&
           co_violators == other.co_violators;
}

bool SlashingEvent::operator!=(const SlashingEvent& other) const {
    return !(*this == other);
}

// SlashingDetector implementation
SlashingDetector::SlashingDetector(const Config& config)
    : config_(config),
      signature_history_(),
      vote_history_() {
}

std::optional<SlashingEvent> SlashingDetector::detect_double_sign(
    const SignatureRecord& sig1,
    const SignatureRecord& sig2
) {
    // Check if both signatures are from the same validator
    if (sig1.validator_id != sig2.validator_id) {
        return std::nullopt;
    }

    // Check if both signatures are at the same height
    if (sig1.block_height != sig2.block_height) {
        return std::nullopt;
    }

    // Check if the block hashes are different (this is the violation)
    if (sig1.block_hash == sig2.block_hash) {
        // Same block, not a violation
        return std::nullopt;
    }

    // Double-signing detected!
    // Create slashing event (penalty will be calculated later with full context)
    SlashingEvent event(
        sig1.validator_id,
        sig1.block_height,
        SlashReason::DoubleSign,
        0  // Penalty calculated separately with calculate_penalty()
    );

    return event;
}

std::optional<SlashingEvent> SlashingDetector::detect_surround_vote(
    const Vote& vote1,
    const Vote& vote2
) {
    // Check if both votes are from the same validator
    if (vote1.validator_id != vote2.validator_id) {
        return std::nullopt;
    }

    // Surround vote detection:
    // A surround vote occurs when a validator votes for two conflicting blocks.
    // In HotStuff BFT, this typically means:
    // 1. Voting for different blocks at the same height (similar to double-sign)
    // 2. Voting for a block that conflicts with the chain of a previous vote
    
    // Check for same-height conflict (most common case)
    if (vote1.block_height == vote2.block_height && vote1.block_hash != vote2.block_hash) {
        // Validator voted for two different blocks at the same height
        SlashingEvent event(
            vote1.validator_id,
            vote1.block_height,
            SlashReason::SurroundVote,
            0  // Penalty calculated separately
        );
        return event;
    }

    // Check for view-based conflict
    // If votes are in the same view but for different blocks at different heights,
    // this could indicate a surround vote
    if (vote1.view_number == vote2.view_number && 
        vote1.block_height != vote2.block_height &&
        vote1.block_hash != vote2.block_hash) {
        // Validator voted for conflicting blocks in the same view
        SlashingEvent event(
            vote1.validator_id,
            std::max(vote1.block_height, vote2.block_height),
            SlashReason::SurroundVote,
            0  // Penalty calculated separately
        );
        return event;
    }

    // No surround vote detected
    return std::nullopt;
}

uint64_t SlashingDetector::calculate_penalty(
    uint64_t validator_stake,
    uint64_t total_stake,
    const std::vector<uint64_t>& co_violator_stakes
) const {
    if (total_stake == 0 || validator_stake == 0) {
        return 0;
    }

    // Calculate total violating stake (sum of all co-violator stakes)
    uint64_t total_violating_stake = 0;
    for (uint64_t stake : co_violator_stakes) {
        total_violating_stake += stake;
    }

    // Convert to double for calculation
    double si = static_cast<double>(validator_stake);
    double S_total = static_cast<double>(total_stake);
    double S_violating = static_cast<double>(total_violating_stake);

    // Calculate penalty using quadratic correlated slashing formula:
    // Penalty_i = min(1.0, α·si/Stotal + β·si·Sviolating/Stotal²)·si
    
    double individual_term = config_.alpha * (si / S_total);
    double correlation_term = config_.beta * si * S_violating / (S_total * S_total);
    double penalty_fraction = std::min(1.0, individual_term + correlation_term);
    
    // Calculate penalty amount
    uint64_t penalty = static_cast<uint64_t>(penalty_fraction * si);

    // Ensure penalty doesn't exceed validator's stake
    penalty = std::min(penalty, validator_stake);

    return penalty;
}

void SlashingDetector::record_signature(const SignatureRecord& record) {
    auto& history = signature_history_[record.validator_id];
    
    // Add the new signature
    history.push_back(record);
    
    // Limit history size to prevent unbounded growth
    if (history.size() > config_.max_signature_history) {
        // Remove oldest signatures
        history.erase(history.begin(), history.begin() + (history.size() - config_.max_signature_history));
    }
}

void SlashingDetector::record_vote(const Vote& vote) {
    auto& history = vote_history_[vote.validator_id];
    
    // Add the new vote
    history.push_back(vote);
    
    // Limit history size to prevent unbounded growth
    if (history.size() > config_.max_signature_history) {
        // Remove oldest votes
        history.erase(history.begin(), history.begin() + (history.size() - config_.max_signature_history));
    }
}

std::vector<SignatureRecord> SlashingDetector::get_signature_history(
    const ValidatorID& validator_id
) const {
    auto it = signature_history_.find(validator_id);
    if (it != signature_history_.end()) {
        return it->second;
    }
    return {};
}

std::vector<Vote> SlashingDetector::get_vote_history(
    const ValidatorID& validator_id
) const {
    auto it = vote_history_.find(validator_id);
    if (it != vote_history_.end()) {
        return it->second;
    }
    return {};
}

void SlashingDetector::prune_old_signatures(
    uint64_t current_height,
    uint64_t retention_period
) {
    if (current_height < retention_period) {
        return;  // Not enough history to prune
    }

    uint64_t cutoff_height = current_height - retention_period;

    for (auto& pair : signature_history_) {
        auto& history = pair.second;
        
        // Remove signatures older than cutoff
        history.erase(
            std::remove_if(
                history.begin(),
                history.end(),
                [cutoff_height](const SignatureRecord& record) {
                    return record.block_height < cutoff_height;
                }
            ),
            history.end()
        );
    }
}

void SlashingDetector::prune_old_votes(
    uint64_t current_height,
    uint64_t retention_period
) {
    if (current_height < retention_period) {
        return;  // Not enough history to prune
    }

    uint64_t cutoff_height = current_height - retention_period;

    for (auto& pair : vote_history_) {
        auto& history = pair.second;
        
        // Remove votes older than cutoff
        history.erase(
            std::remove_if(
                history.begin(),
                history.end(),
                [cutoff_height](const Vote& vote) {
                    return vote.block_height < cutoff_height;
                }
            ),
            history.end()
        );
    }
}

bool SlashingDetector::apply_slashing(
    ValidatorRegistry& validator_registry,
    const SlashingEvent& event,
    uint64_t& total_supply
) {
    // Get the validator
    auto validator_opt = validator_registry.get_validator(event.validator_id);
    if (!validator_opt.has_value()) {
        return false;
    }

    uint64_t penalty = event.penalty_amount;
    if (penalty == 0) {
        return false;
    }

    // Ensure penalty doesn't exceed validator's stake
    if (penalty > validator_opt->bonded_stake) {
        penalty = validator_opt->bonded_stake;
    }

    // Calculate distribution: 50% burn, 50% to active validators
    uint64_t burn_amount = penalty / 2;
    uint64_t distribute_amount = penalty - burn_amount;  // Remainder goes to distribution

    // Distribute 50% to active validators proportional to stake BEFORE slashing
    // IMPORTANT: Exclude the slashed validator from receiving rewards
    // Get active validators and calculate total stake excluding slashed validator
    auto active_validators = validator_registry.current_set().get_active_validators();
    uint64_t total_stake_excluding_slashed = 0;
    for (const auto& v : active_validators) {
        if (v.id != event.validator_id) {
            total_stake_excluding_slashed += v.bonded_stake;
        }
    }
    
    // Manually distribute rewards proportionally, excluding slashed validator
    if (total_stake_excluding_slashed > 0 && distribute_amount > 0) {
        uint64_t distributed = 0;
        size_t count = 0;
        size_t total_count = 0;
        
        // Count non-slashed validators
        for (const auto& v : active_validators) {
            if (v.id != event.validator_id) {
                total_count++;
            }
        }
        
        for (const auto& v : active_validators) {
            if (v.id != event.validator_id) {
                uint64_t reward;
                if (count == total_count - 1) {
                    // Last validator gets remainder to avoid rounding issues
                    reward = distribute_amount - distributed;
                } else {
                    // Calculate proportional reward
                    __uint128_t numerator = static_cast<__uint128_t>(v.bonded_stake) * 
                                           static_cast<__uint128_t>(distribute_amount);
                    reward = static_cast<uint64_t>(numerator / total_stake_excluding_slashed);
                }
                
                if (reward > 0) {
                    validator_registry.bond_stake(v.id, reward);
                    distributed += reward;
                }
                count++;
            }
        }
    }

    // Apply the slash to the validator AFTER distribution
    if (!validator_registry.slash_validator(event.validator_id, penalty, event.reason)) {
        return false;
    }

    // Burn 50% of slashed stake (remove from total supply)
    if (total_supply >= burn_amount) {
        total_supply -= burn_amount;
    } else {
        total_supply = 0;
    }

    // Mark validator with tombstone status for safety violations
    if (event.reason == SlashReason::DoubleSign || event.reason == SlashReason::SurroundVote) {
        validator_registry.update_validator(
            event.validator_id,
            std::nullopt,  // Don't change consensus key
            std::nullopt,  // Don't change withdrawal key
            ValidatorStatus::Tombstoned  // Set to tombstoned
        );
    }

    return true;
}

} // namespace consensus
} // namespace sarafu
