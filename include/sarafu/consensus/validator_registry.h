#pragma once

#include "sarafu/consensus/validator.h"
#include "sarafu/consensus/block.h"
#include "sarafu/crypto/merkle_tree.h"
#include "sarafu/crypto/blake3_hash.h"
#include <map>
#include <optional>
#include <vector>

namespace sarafu {
namespace consensus {

// Forward declaration
enum class SlashReason;

/**
 * UnbondingRequest represents a pending stake unbonding request.
 * 
 * Stake unbonding has a 21-day delay to ensure validators remain
 * slashable for violations committed before unbonding.
 */
struct UnbondingRequest {
    ValidatorID validator_id;
    uint64_t amount;
    uint64_t completion_height;  // Block height when unbonding completes
    
    UnbondingRequest() : validator_id(state::Address::zero()), amount(0), completion_height(0) {}
    UnbondingRequest(const ValidatorID& id, uint64_t amt, uint64_t height)
        : validator_id(id), amount(amt), completion_height(height) {}
};

/**
 * ValidatorRegistry manages validator state and epoch transitions.
 * 
 * Responsibilities:
 * - Track validator stake, status, and performance
 * - Handle stake bonding and unbonding (21-day delay)
 * - Select top N validators by stake at epoch boundaries
 * - Compute validator set Merkle root for light clients
 * - Track downtime and apply tiered penalties
 * - Reset per-epoch counters at epoch transitions
 * 
 * Requirements: 2.2, 2.3, 2.4, 2.5, 2.6, 4.1, 4.2, 4.3, 4.4, 4.6, 21.1, 21.2, 21.6, 21.7
 */
class ValidatorRegistry {
public:
    /**
     * Configuration parameters for the validator registry.
     */
    struct Config {
        uint64_t active_validator_count;      // N: number of active validators
        uint64_t minimum_self_bond;            // Minimum stake required to be active
        uint64_t unbonding_period_blocks;      // 21 days in blocks (~362,880 blocks)
        uint64_t blocks_per_epoch;             // 10,000 blocks per epoch
        double downtime_threshold;             // 0.95 (95% signing rate required)
        
        Config()
            : active_validator_count(100),
              minimum_self_bond(100000),
              unbonding_period_blocks(362880),  // 21 days * 24 hours * 60 min * 60 sec / 2 sec per block
              blocks_per_epoch(10000),
              downtime_threshold(0.95) {}
    };

    // Constructor
    explicit ValidatorRegistry(const Config& config = Config());

    /**
     * Get the current validator set.
     * 
     * @return The current validator set
     */
    const ValidatorSet& current_set() const { return current_set_; }

    /**
     * Get a validator by ID.
     * 
     * @param id The validator ID
     * @return The validator if found, std::nullopt otherwise
     */
    std::optional<Validator> get_validator(const ValidatorID& id) const;

    /**
     * Add a new validator to the registry.
     * 
     * @param id The validator ID
     * @param consensus_key The BLS12-381 consensus key
     * @param withdrawal_key The Ed25519 withdrawal key
     * @param initial_stake The initial bonded stake
     * @return true if successful, false if validator already exists
     */
    bool add_validator(
        const ValidatorID& id,
        const crypto::BLS12_381_PublicKey& consensus_key,
        const crypto::Ed25519_PublicKey& withdrawal_key,
        uint64_t initial_stake
    );

    /**
     * Update an existing validator's keys or status.
     * 
     * @param id The validator ID
     * @param consensus_key Optional new consensus key
     * @param withdrawal_key Optional new withdrawal key
     * @param new_status Optional new status
     * @return true if successful, false if validator not found
     */
    bool update_validator(
        const ValidatorID& id,
        const std::optional<crypto::BLS12_381_PublicKey>& consensus_key = std::nullopt,
        const std::optional<crypto::Ed25519_PublicKey>& withdrawal_key = std::nullopt,
        const std::optional<ValidatorStatus>& new_status = std::nullopt
    );

    /**
     * Bond additional stake to a validator.
     * 
     * @param id The validator ID
     * @param amount The amount to bond
     * @return true if successful, false if validator not found
     * 
     * Requirements: 21.1
     */
    bool bond_stake(const ValidatorID& id, uint64_t amount);

    /**
     * Initiate unbonding of stake (21-day delay).
     * 
     * @param id The validator ID
     * @param amount The amount to unbond
     * @param current_height The current block height
     * @return true if successful, false if:
     *   - Validator not found
     *   - Insufficient bonded stake
     *   - Remaining stake would be below minimum (unless unbonding all)
     * 
     * Requirements: 21.2, 21.6
     */
    bool unbond_stake(const ValidatorID& id, uint64_t amount, uint64_t current_height);

    /**
     * Process completed unbonding requests.
     * 
     * @param current_height The current block height
     * @return Vector of completed unbonding requests
     * 
     * Requirements: 21.2
     */
    std::vector<UnbondingRequest> process_unbonding(uint64_t current_height);

    /**
     * Rank all validators by bonded stake (descending).
     * Ties are broken by lexicographic validator ID ordering.
     * 
     * @return Vector of validators sorted by stake
     * 
     * Requirements: 2.2, 2.3
     */
    std::vector<Validator> rank_by_stake() const;

    /**
     * Transition to a new epoch, selecting top N validators.
     * 
     * Algorithm:
     * 1. Rank all validators by bonded_stake (lexicographic ID for ties)
     * 2. Select top N as active set for epoch E+1
     * 3. Calculate total_stake for new set
     * 4. Compute validator_set_root (Merkle tree)
     * 5. Reset per-epoch counters (blocks_signed, blocks_missed)
     * 
     * @param new_epoch The new epoch number
     * @param total_blocks_in_epoch Total blocks in the completed epoch
     * @return The new validator set
     * 
     * Requirements: 2.2, 2.3, 2.4, 2.5, 2.6
     */
    ValidatorSet transition_epoch(uint64_t new_epoch, uint64_t total_blocks_in_epoch);

    /**
     * Compute the Merkle root of the validator set.
     * 
     * @param validator_set The validator set
     * @return The Merkle root hash
     * 
     * Requirements: 2.6
     */
    crypto::Blake3Hash compute_validator_set_root(const ValidatorSet& validator_set) const;

    /**
     * Verify that a QC has ≥2/3 stake signatures.
     * 
     * @param qc The quorum certificate to verify
     * @param validator_set The validator set to check against
     * @return true if QC has ≥2/3 stake, false otherwise
     * 
     * Requirements: 2.6
     */
    bool require_epoch_transition_qc(
        const QuorumCertificate& qc,
        const ValidatorSet& validator_set
    ) const;

    /**
     * Record a validator's signature on a block.
     * 
     * @param id The validator ID
     * @param block_height The block height
     * @return true if successful, false if validator not found
     * 
     * Requirements: 4.1
     */
    bool record_signature(const ValidatorID& id, uint64_t block_height);

    /**
     * Check for downtime violations at epoch boundary and apply penalties.
     * 
     * Downtime detection:
     * - If signing_rate < 0.95:
     *   - First violation: Jail for 1 epoch
     *   - Second consecutive: Slash 0.5%, move to Standby
     *   - Third+ consecutive: Slash 1%, move to Standby
     * - If signing_rate >= 0.95: Reset consecutive_downtime_epochs to 0
     * 
     * @param epoch The epoch that just ended
     * @param total_blocks_in_epoch Total blocks in the epoch
     * @return Vector of validators with downtime violations
     * 
     * Requirements: 4.1, 4.2, 4.3, 4.4, 4.6
     */
    std::vector<ValidatorID> check_downtime(uint64_t epoch, uint64_t total_blocks_in_epoch);

    /**
     * Get all validators in the registry.
     * 
     * @return Map of validator ID to validator
     */
    const std::map<ValidatorID, Validator>& get_all_validators() const {
        return validators_;
    }

    /**
     * Slash a validator's stake.
     * 
     * @param id The validator ID
     * @param amount The amount to slash
     * @param reason The reason for slashing
     * @return true if successful, false if validator not found or insufficient stake
     * 
     * Requirements: 3.4
     */
    bool slash_validator(const ValidatorID& id, uint64_t amount, SlashReason reason);

    /**
     * Distribute rewards to active validators proportional to their stake.
     * 
     * @param total_amount The total amount to distribute
     * @return Map of validator ID to reward amount
     * 
     * Requirements: 3.5
     */
    std::map<ValidatorID, uint64_t> distribute_rewards_proportional(uint64_t total_amount);

    /**
     * Get pending unbonding requests.
     * 
     * @return Vector of unbonding requests
     */
    const std::vector<UnbondingRequest>& get_unbonding_requests() const {
        return unbonding_requests_;
    }

private:
    Config config_;
    ValidatorSet current_set_;
    std::map<ValidatorID, Validator> validators_;
    std::vector<UnbondingRequest> unbonding_requests_;
    std::map<ValidatorID, uint64_t> last_downtime_epoch_checked_;
};

} // namespace consensus
} // namespace sarafu
