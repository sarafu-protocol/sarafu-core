#pragma once

#include <cstdint>
#include <vector>
#include "sarafu/consensus/block.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/state/account.h"

namespace sarafu {
namespace consensus {

/**
 * ValidatorStatus represents the current state of a validator.
 * 
 * - Active: Validator is in the current active set and participates in consensus
 * - Standby: Validator is ranked N+1 or below, not in active set
 * - Jailed: Temporarily suspended for downtime violations
 * - Tombstoned: Permanently ejected for safety violations (double-signing, etc.)
 */
enum class ValidatorStatus {
    Active,      // In current validator set
    Standby,     // Ranked N+1 or below
    Jailed,      // Temporarily suspended for downtime
    Tombstoned   // Permanently ejected for safety violation
};

/**
 * Validator represents a single validator in the Sarafu blockchain.
 * 
 * Each validator has:
 * - id: Unique identifier (derived from address)
 * - consensus_key: BLS12-381 public key for signing blocks and votes
 * - withdrawal_key: Ed25519 public key for receiving rewards and managing stake
 * - bonded_stake: Amount of tokens bonded by this validator
 * - status: Current validator status (Active, Standby, Jailed, Tombstoned)
 * - jailed_until_epoch: Epoch when jail period ends (0 if not jailed)
 * - consecutive_downtime_epochs: Number of consecutive epochs with downtime violations
 * - blocks_signed_this_epoch: Number of blocks signed in current epoch
 * - blocks_missed_this_epoch: Number of blocks missed in current epoch
 */
struct Validator {
    ValidatorID id;
    crypto::BLS12_381_PublicKey consensus_key;
    crypto::Ed25519_PublicKey withdrawal_key;
    uint64_t bonded_stake;
    ValidatorStatus status;
    uint64_t jailed_until_epoch;
    uint64_t consecutive_downtime_epochs;
    uint64_t blocks_signed_this_epoch;
    uint64_t blocks_missed_this_epoch;

    // Constructors
    Validator();
    Validator(
        const ValidatorID& validator_id,
        const crypto::BLS12_381_PublicKey& consensus_pk,
        const crypto::Ed25519_PublicKey& withdrawal_pk,
        uint64_t stake
    );
    Validator(
        const ValidatorID& validator_id,
        const crypto::BLS12_381_PublicKey& consensus_pk,
        const crypto::Ed25519_PublicKey& withdrawal_pk,
        uint64_t stake,
        ValidatorStatus stat,
        uint64_t jailed_until,
        uint64_t consecutive_downtime,
        uint64_t blocks_signed,
        uint64_t blocks_missed
    );

    /**
     * Serialize the validator for storage and Merkle tree construction.
     * 
     * @return Serialized validator bytes
     */
    std::vector<uint8_t> serialize() const;

    /**
     * Deserialize a validator from bytes.
     * 
     * @param data The serialized validator data
     * @return The deserialized validator
     * @throws std::invalid_argument if data is invalid
     */
    static Validator deserialize(const std::vector<uint8_t>& data);

    /**
     * Compute the hash of this validator (for Merkle tree).
     * 
     * @return Blake3 hash of the serialized validator
     */
    crypto::Blake3Hash hash() const;

    // Comparison operators
    bool operator==(const Validator& other) const;
    bool operator!=(const Validator& other) const;
    
    /**
     * Comparison operator for sorting validators by stake (descending).
     * If stakes are equal, uses lexicographic ordering by validator ID.
     * 
     * @param other The validator to compare with
     * @return true if this validator should be ranked higher
     */
    bool operator<(const Validator& other) const;
};

/**
 * ValidatorSet represents the set of validators for a specific epoch.
 * 
 * The validator set includes:
 * - epoch: The epoch number this set is valid for
 * - validators: List of all validators (active and standby)
 * - total_stake: Sum of bonded stake from all active validators
 * - merkle_root: Merkle root of all validators (for light client verification)
 * 
 * The validator set is updated every 10,000 blocks (epoch boundary).
 * The top N validators by stake become the active set.
 */
struct ValidatorSet {
    uint64_t epoch;
    std::vector<Validator> validators;
    uint64_t total_stake;
    crypto::Blake3Hash merkle_root;

    // Constructors
    ValidatorSet();
    ValidatorSet(
        uint64_t ep,
        const std::vector<Validator>& vals,
        uint64_t total_stk
    );
    ValidatorSet(
        uint64_t ep,
        const std::vector<Validator>& vals,
        uint64_t total_stk,
        const crypto::Blake3Hash& root
    );

    /**
     * Get all active validators (status == Active).
     * 
     * @return Vector of active validators
     */
    std::vector<Validator> get_active_validators() const;

    /**
     * Get all standby validators (status == Standby).
     * 
     * @return Vector of standby validators
     */
    std::vector<Validator> get_standby_validators() const;

    /**
     * Find a validator by ID.
     * 
     * @param id The validator ID to search for
     * @return Pointer to the validator if found, nullptr otherwise
     */
    const Validator* find_validator(const ValidatorID& id) const;

    /**
     * Check if a validator is in the active set.
     * 
     * @param id The validator ID to check
     * @return true if the validator is active, false otherwise
     */
    bool is_active(const ValidatorID& id) const;

    /**
     * Serialize the validator set for storage.
     * 
     * @return Serialized validator set bytes
     */
    std::vector<uint8_t> serialize() const;

    /**
     * Deserialize a validator set from bytes.
     * 
     * @param data The serialized validator set data
     * @return The deserialized validator set
     * @throws std::invalid_argument if data is invalid
     */
    static ValidatorSet deserialize(const std::vector<uint8_t>& data);

    // Comparison operators
    bool operator==(const ValidatorSet& other) const;
    bool operator!=(const ValidatorSet& other) const;
};

} // namespace consensus
} // namespace sarafu
