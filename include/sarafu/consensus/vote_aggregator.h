#pragma once

#include <map>
#include <vector>
#include <optional>
#include "sarafu/consensus/block.h"
#include "sarafu/consensus/slashing_detector.h"
#include "sarafu/consensus/validator.h"

namespace sarafu {
namespace consensus {

/**
 * VoteAggregator collects validator votes and creates Quorum Certificates.
 * 
 * The aggregator:
 * - Tracks votes per block from validators
 * - Aggregates BLS signatures when ≥2/3 stake is reached
 * - Creates QuorumCertificates proving supermajority consensus
 * 
 * Requirements: 1.2, 9.2
 */
class VoteAggregator {
public:
    /**
     * Construct a VoteAggregator with a validator set.
     * 
     * @param validator_set The current validator set with stake information
     */
    explicit VoteAggregator(const ValidatorSet& validator_set);

    /**
     * Add a vote from a validator for a specific block.
     * 
     * The vote is validated to ensure:
     * - The validator is in the active set
     * - The validator hasn't already voted for this block
     * - The signature is valid
     * 
     * @param vote The vote to add
     * @return true if vote was added, false if rejected
     */
    bool add_vote(const Vote& vote);

    /**
     * Attempt to aggregate votes for a block into a Quorum Certificate.
     * 
     * This checks if the votes for the specified block represent ≥2/3 of
     * total stake. If so, it aggregates the BLS signatures and creates a QC.
     * 
     * @param block_height The height of the block
     * @param block_hash The hash of the block
     * @param view_number The view number
     * @return QuorumCertificate if ≥2/3 stake reached, std::nullopt otherwise
     */
    std::optional<QuorumCertificate> aggregate_votes(
        uint64_t block_height,
        const crypto::Blake3Hash& block_hash,
        uint64_t view_number
    );

    /**
     * Check if a block has reached supermajority (≥2/3 stake).
     * 
     * @param block_height The height of the block
     * @param block_hash The hash of the block
     * @return true if ≥2/3 stake has voted for this block
     */
    bool has_supermajority(
        uint64_t block_height,
        const crypto::Blake3Hash& block_hash
    ) const;

    /**
     * Get the total stake that has voted for a specific block.
     * 
     * @param block_height The height of the block
     * @param block_hash The hash of the block
     * @return Total stake of validators who voted for this block
     */
    uint64_t get_stake_for_block(
        uint64_t block_height,
        const crypto::Blake3Hash& block_hash
    ) const;

    /**
     * Clear all votes for blocks at or below a given height.
     * 
     * This is used to prune old votes after finalization.
     * 
     * @param height Clear votes for blocks at or below this height
     */
    void clear_votes_up_to_height(uint64_t height);

    /**
     * Update the validator set.
     * 
     * This should be called at epoch boundaries when the validator set changes.
     * 
     * @param validator_set The new validator set
     */
    void update_validator_set(const ValidatorSet& validator_set);

private:
    /**
     * Key for identifying a specific block in the vote tracking map.
     */
    struct BlockKey {
        uint64_t height;
        crypto::Blake3Hash hash;

        bool operator<(const BlockKey& other) const {
            if (height != other.height) {
                return height < other.height;
            }
            return hash < other.hash;
        }
    };

    /**
     * Votes collected for a specific block.
     */
    struct BlockVotes {
        std::vector<Vote> votes;
        std::map<ValidatorID, bool> voted;  // Track which validators voted
        uint64_t total_stake;
    };

    // Current validator set
    ValidatorSet validator_set_;

    // Map from block to votes
    std::map<BlockKey, BlockVotes> votes_by_block_;

    /**
     * Get the stake of a validator.
     * 
     * @param validator_id The validator ID
     * @return Stake amount, or 0 if validator not found
     */
    uint64_t get_validator_stake(const ValidatorID& validator_id) const;

    /**
     * Check if a validator is in the active set.
     * 
     * @param validator_id The validator ID
     * @return true if validator is active
     */
    bool is_active_validator(const ValidatorID& validator_id) const;
};

} // namespace consensus
} // namespace sarafu
