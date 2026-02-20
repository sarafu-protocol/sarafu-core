#pragma once

#include "sarafu/consensus/block.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/bls12_381.h"
#include <map>
#include <vector>
#include <optional>

namespace sarafu {
namespace consensus {

// Forward declarations
class ValidatorRegistry;

/**
 * SlashReason represents the type of protocol violation.
 * 
 * - DoubleSign: Validator signed two different blocks at the same height
 * - SurroundVote: Validator voted for a block that conflicts with a previous vote
 * - Downtime: Validator missed too many blocks (handled by ValidatorRegistry)
 */
enum class SlashReason {
    DoubleSign,
    SurroundVote,
    Downtime
};

/**
 * SignatureRecord tracks a validator's signature on a block.
 * 
 * Used for detecting double-signing violations by comparing
 * signatures at the same height.
 */
struct SignatureRecord {
    ValidatorID validator_id;
    uint64_t block_height;
    crypto::Blake3Hash block_hash;
    crypto::BLS12_381_Signature signature;
    uint64_t timestamp;

    SignatureRecord();
    SignatureRecord(
        const ValidatorID& id,
        uint64_t height,
        const crypto::Blake3Hash& hash,
        const crypto::BLS12_381_Signature& sig,
        uint64_t ts
    );

    bool operator==(const SignatureRecord& other) const;
    bool operator!=(const SignatureRecord& other) const;
};

/**
 * Vote represents a validator's vote on a block.
 * 
 * Used for detecting surround vote violations where a validator
 * votes for conflicting blocks.
 */
struct Vote {
    ValidatorID validator_id;
    uint64_t block_height;
    crypto::Blake3Hash block_hash;
    uint64_t view_number;
    crypto::BLS12_381_Signature signature;

    Vote();
    Vote(
        const ValidatorID& id,
        uint64_t height,
        const crypto::Blake3Hash& hash,
        uint64_t view,
        const crypto::BLS12_381_Signature& sig
    );

    bool operator==(const Vote& other) const;
    bool operator!=(const Vote& other) const;
};

/**
 * SlashingEvent represents a detected protocol violation.
 * 
 * Contains all information needed to apply the slashing penalty:
 * - validator_id: The validator who committed the violation
 * - block_height: The height where the violation occurred
 * - reason: The type of violation (DoubleSign, SurroundVote, Downtime)
 * - penalty_amount: The calculated penalty in tokens
 * - co_violators: List of other validators who committed the same violation (for correlated slashing)
 */
struct SlashingEvent {
    ValidatorID validator_id;
    uint64_t block_height;
    SlashReason reason;
    uint64_t penalty_amount;
    std::vector<ValidatorID> co_violators;

    SlashingEvent();
    SlashingEvent(
        const ValidatorID& id,
        uint64_t height,
        SlashReason rsn,
        uint64_t penalty,
        const std::vector<ValidatorID>& co_viol = {}
    );

    bool operator==(const SlashingEvent& other) const;
    bool operator!=(const SlashingEvent& other) const;
};

/**
 * SlashingDetector monitors for protocol violations and calculates penalties.
 * 
 * Responsibilities:
 * - Detect double-signing (two blocks at same height)
 * - Detect surround votes (conflicting votes)
 * - Calculate quadratic correlated slashing penalties
 * - Track signature history for violation detection
 * 
 * Quadratic Correlated Slashing Formula:
 * Penalty_i = min(1.0, α·si/Stotal + β·si·Sviolating/Stotal²)·si
 * 
 * Where:
 * - si = stake of validator i
 * - Stotal = total bonded stake
 * - Sviolating = sum of stake of all violators
 * - α = 0.05 (individual penalty coefficient)
 * - β = 0.5 (correlation penalty coefficient)
 * 
 * Distribution:
 * - 50% burned (removed from supply)
 * - 50% distributed to active validators proportional to stake
 * 
 * Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7
 */
class SlashingDetector {
public:
    /**
     * Configuration parameters for slashing.
     */
    struct Config {
        double alpha;  // Individual penalty coefficient (default: 0.05)
        double beta;   // Correlation penalty coefficient (default: 0.5)
        size_t max_signature_history;  // Maximum signatures to track per validator
        
        Config()
            : alpha(0.05),
              beta(0.5),
              max_signature_history(10000) {}
    };

    // Constructor
    explicit SlashingDetector(const Config& config = Config());

    /**
     * Detect double-signing violation.
     * 
     * Double-signing occurs when a validator signs two different blocks
     * at the same height. This is a safety violation.
     * 
     * @param sig1 First signature record
     * @param sig2 Second signature record
     * @return SlashingEvent if violation detected, std::nullopt otherwise
     * 
     * Requirements: 3.1
     */
    std::optional<SlashingEvent> detect_double_sign(
        const SignatureRecord& sig1,
        const SignatureRecord& sig2
    );

    /**
     * Detect surround vote violation.
     * 
     * A surround vote occurs when a validator votes for a block that
     * conflicts with a previously signed block. This is a safety violation.
     * 
     * @param vote1 First vote
     * @param vote2 Second vote
     * @return SlashingEvent if violation detected, std::nullopt otherwise
     * 
     * Requirements: 3.2
     */
    std::optional<SlashingEvent> detect_surround_vote(
        const Vote& vote1,
        const Vote& vote2
    );

    /**
     * Calculate quadratic correlated slashing penalty.
     * 
     * Formula: Penalty_i = min(1.0, α·si/Stotal + β·si·Sviolating/Stotal²)·si
     * 
     * @param validator_stake Stake of the validator being slashed
     * @param total_stake Total bonded stake in the network
     * @param co_violator_stakes Stakes of all co-violators (including this validator)
     * @return Penalty amount in tokens
     * 
     * Requirements: 3.3, 3.7
     */
    uint64_t calculate_penalty(
        uint64_t validator_stake,
        uint64_t total_stake,
        const std::vector<uint64_t>& co_violator_stakes
    ) const;

    /**
     * Record a signature for double-sign detection.
     * 
     * Maintains a history of signatures per validator to detect
     * double-signing violations.
     * 
     * @param record The signature record to store
     */
    void record_signature(const SignatureRecord& record);

    /**
     * Record a vote for surround vote detection.
     * 
     * Maintains a history of votes per validator to detect
     * surround vote violations.
     * 
     * @param vote The vote to store
     */
    void record_vote(const Vote& vote);

    /**
     * Get signature history for a validator.
     * 
     * @param validator_id The validator ID
     * @return Vector of signature records
     */
    std::vector<SignatureRecord> get_signature_history(const ValidatorID& validator_id) const;

    /**
     * Get vote history for a validator.
     * 
     * @param validator_id The validator ID
     * @return Vector of votes
     */
    std::vector<Vote> get_vote_history(const ValidatorID& validator_id) const;

    /**
     * Clear old signature history to prevent unbounded growth.
     * 
     * @param current_height Current block height
     * @param retention_period Number of blocks to retain history for
     */
    void prune_old_signatures(uint64_t current_height, uint64_t retention_period);

    /**
     * Clear old vote history to prevent unbounded growth.
     * 
     * @param current_height Current block height
     * @param retention_period Number of blocks to retain history for
     */
    void prune_old_votes(uint64_t current_height, uint64_t retention_period);

    /**
     * Apply slashing penalty to a validator.
     * 
     * This method:
     * 1. Reduces validator's bonded stake by the penalty amount
     * 2. Burns 50% of the slashed stake (removed from supply)
     * 3. Distributes 50% to active validators proportional to stake
     * 4. Marks validator with tombstone status for safety violations
     * 
     * @param validator_registry The validator registry to apply slashing to
     * @param event The slashing event containing violation details
     * @param total_supply Reference to total supply (for burning)
     * @return true if slashing was applied successfully, false otherwise
     * 
     * Requirements: 3.4, 3.5, 3.6
     */
    bool apply_slashing(
        ValidatorRegistry& validator_registry,
        const SlashingEvent& event,
        uint64_t& total_supply
    );

private:
    Config config_;
    
    // Track all signatures for double-sign detection
    // Map: validator_id -> vector of signature records
    std::map<ValidatorID, std::vector<SignatureRecord>> signature_history_;
    
    // Track all votes for surround vote detection
    // Map: validator_id -> vector of votes
    std::map<ValidatorID, std::vector<Vote>> vote_history_;
};

} // namespace consensus
} // namespace sarafu
