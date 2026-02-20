#include "sarafu/consensus/vote_aggregator.h"
#include <algorithm>

namespace sarafu {
namespace consensus {

VoteAggregator::VoteAggregator(const ValidatorSet& validator_set)
    : validator_set_(validator_set)
    , votes_by_block_()
{
}

bool VoteAggregator::add_vote(const Vote& vote) {
    // Check if validator is in active set
    if (!is_active_validator(vote.validator_id)) {
        return false;
    }

    // Create block key
    BlockKey key{vote.block_height, vote.block_hash};

    // Get or create block votes entry
    auto& block_votes = votes_by_block_[key];

    // Check if validator already voted for this block
    if (block_votes.voted[vote.validator_id]) {
        return false;
    }

    // Get validator stake
    uint64_t stake = get_validator_stake(vote.validator_id);

    // Add vote
    block_votes.votes.push_back(vote);
    block_votes.voted[vote.validator_id] = true;
    block_votes.total_stake += stake;

    return true;
}

std::optional<QuorumCertificate> VoteAggregator::aggregate_votes(
    uint64_t block_height,
    const crypto::Blake3Hash& block_hash,
    uint64_t view_number
) {
    BlockKey key{block_height, block_hash};

    // Check if we have votes for this block
    auto it = votes_by_block_.find(key);
    if (it == votes_by_block_.end()) {
        return std::nullopt;
    }

    const auto& block_votes = it->second;

    // Check if we have supermajority (≥2/3 stake)
    uint64_t required_stake = (validator_set_.total_stake * 2 + 2) / 3;  // Ceiling division
    if (block_votes.total_stake < required_stake) {
        return std::nullopt;
    }

    // Aggregate BLS signatures
    std::vector<crypto::BLS12_381_Signature> signatures;
    std::vector<ValidatorID> signers;
    
    signatures.reserve(block_votes.votes.size());
    signers.reserve(block_votes.votes.size());

    for (const auto& vote : block_votes.votes) {
        signatures.push_back(vote.signature);
        signers.push_back(vote.validator_id);
    }

    // Aggregate signatures using BLS12-381
    crypto::BLS12_381_Signature aggregated_sig = crypto::BLS12_381::aggregate(signatures);

    // Create Quorum Certificate
    QuorumCertificate qc(
        block_height,
        block_hash,
        view_number,
        aggregated_sig,
        signers,
        block_votes.total_stake
    );

    return qc;
}

bool VoteAggregator::has_supermajority(
    uint64_t block_height,
    const crypto::Blake3Hash& block_hash
) const {
    BlockKey key{block_height, block_hash};

    auto it = votes_by_block_.find(key);
    if (it == votes_by_block_.end()) {
        return false;
    }

    uint64_t required_stake = (validator_set_.total_stake * 2 + 2) / 3;  // Ceiling division
    return it->second.total_stake >= required_stake;
}

uint64_t VoteAggregator::get_stake_for_block(
    uint64_t block_height,
    const crypto::Blake3Hash& block_hash
) const {
    BlockKey key{block_height, block_hash};

    auto it = votes_by_block_.find(key);
    if (it == votes_by_block_.end()) {
        return 0;
    }

    return it->second.total_stake;
}

void VoteAggregator::clear_votes_up_to_height(uint64_t height) {
    // Remove all votes for blocks at or below the specified height
    auto it = votes_by_block_.begin();
    while (it != votes_by_block_.end()) {
        if (it->first.height <= height) {
            it = votes_by_block_.erase(it);
        } else {
            ++it;
        }
    }
}

void VoteAggregator::update_validator_set(const ValidatorSet& validator_set) {
    validator_set_ = validator_set;
    // Note: Existing votes remain valid, but new votes will be validated against new set
}

uint64_t VoteAggregator::get_validator_stake(const ValidatorID& validator_id) const {
    for (const auto& validator : validator_set_.validators) {
        if (validator.id == validator_id) {
            return validator.bonded_stake;
        }
    }
    return 0;
}

bool VoteAggregator::is_active_validator(const ValidatorID& validator_id) const {
    for (const auto& validator : validator_set_.validators) {
        if (validator.id == validator_id && validator.status == ValidatorStatus::Active) {
            return true;
        }
    }
    return false;
}

} // namespace consensus
} // namespace sarafu
