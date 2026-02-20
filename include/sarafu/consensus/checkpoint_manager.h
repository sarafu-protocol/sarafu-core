#pragma once

#include "sarafu/consensus/block.h"
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/bls12_381.h"
#include <cstdint>
#include <optional>
#include <vector>
#include <map>

namespace sarafu {
namespace consensus {

/**
 * WeakSubjectivityCheckpoint represents a trusted checkpoint for syncing.
 * 
 * Checkpoints are produced every 1,000,000 blocks and signed by ≥2/3 of
 * validator stake to prevent long-range attacks.
 * 
 * Requirements: 13.1, 13.2, 13.3, 13.4, 13.6
 */
struct WeakSubjectivityCheckpoint {
    uint64_t block_height;
    crypto::Blake3Hash block_hash;
    uint64_t epoch;
    uint64_t timestamp;
    crypto::Blake3Hash state_root;
    crypto::Blake3Hash validator_set_root;
    
    // Signature data
    crypto::BLS12_381_Signature aggregated_signature;
    std::vector<ValidatorID> signers;
    uint64_t total_stake_signed;
    
    // Constructors
    WeakSubjectivityCheckpoint();
    WeakSubjectivityCheckpoint(
        uint64_t height,
        const crypto::Blake3Hash& hash,
        uint64_t ep,
        uint64_t ts,
        const crypto::Blake3Hash& state_rt,
        const crypto::Blake3Hash& val_rt
    );
    
    /**
     * Compute the checkpoint hash for signing.
     * 
     * @return Blake3 hash of the checkpoint data
     */
    crypto::Blake3Hash hash() const;
    
    /**
     * Serialize the checkpoint for distribution.
     * 
     * @return Serialized checkpoint bytes
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * Deserialize a checkpoint from bytes.
     * 
     * @param data The serialized checkpoint data
     * @return The deserialized checkpoint
     * @throws std::invalid_argument if data is invalid
     */
    static WeakSubjectivityCheckpoint deserialize(const std::vector<uint8_t>& data);
    
    // Comparison operators
    bool operator==(const WeakSubjectivityCheckpoint& other) const;
    bool operator!=(const WeakSubjectivityCheckpoint& other) const;
};

/**
 * CheckpointManager produces and verifies weak subjectivity checkpoints.
 * 
 * Responsibilities:
 * - Produce checkpoints every 1,000,000 blocks
 * - Collect validator signatures (≥2/3 stake required)
 * - Serialize checkpoints for distribution
 * - Verify checkpoint signatures
 * - Validate checkpoint age (<1M blocks old)
 * - Ensure chains include checkpoint blocks
 * 
 * Requirements: 13.1, 13.2, 13.3, 13.4, 13.5, 13.6
 */
class CheckpointManager {
public:
    /**
     * Configuration parameters for checkpoint management.
     */
    struct Config {
        uint64_t checkpoint_interval;  // Blocks between checkpoints (default: 1,000,000)
        uint64_t max_checkpoint_age;   // Maximum age in blocks (default: 1,000,000)
        
        Config()
            : checkpoint_interval(1000000),
              max_checkpoint_age(1000000) {}
    };
    
    // Constructor
    explicit CheckpointManager(const Config& config = Config());
    
    /**
     * Check if a block height should produce a checkpoint.
     * 
     * @param block_height The block height to check
     * @return true if height % checkpoint_interval == 0
     * 
     * Requirements: 13.1
     */
    bool should_produce_checkpoint(uint64_t block_height) const;
    
    /**
     * Produce a checkpoint from a block.
     * 
     * Creates an unsigned checkpoint that needs to be signed by validators.
     * 
     * @param block The block to create a checkpoint from
     * @return The unsigned checkpoint
     * 
     * Requirements: 13.1
     */
    WeakSubjectivityCheckpoint produce_checkpoint(const Block& block) const;
    
    /**
     * Sign a checkpoint with a validator's private key.
     * 
     * @param checkpoint The checkpoint to sign
     * @param validator_id The validator's ID
     * @param private_key The validator's BLS12-381 private key
     * @return The BLS12-381 signature
     * 
     * Requirements: 13.2
     */
    crypto::BLS12_381_Signature sign_checkpoint(
        const WeakSubjectivityCheckpoint& checkpoint,
        const ValidatorID& validator_id,
        const crypto::BLS12_381_PrivateKey& private_key
    ) const;
    
    /**
     * Aggregate validator signatures into a checkpoint.
     * 
     * Collects signatures from validators and aggregates them if ≥2/3 stake
     * has signed. Returns std::nullopt if insufficient stake.
     * 
     * @param checkpoint The unsigned checkpoint
     * @param signatures Map of validator ID to signature
     * @param validator_set The current validator set
     * @return The signed checkpoint if ≥2/3 stake, std::nullopt otherwise
     * 
     * Requirements: 13.2, 13.6
     */
    std::optional<WeakSubjectivityCheckpoint> aggregate_signatures(
        const WeakSubjectivityCheckpoint& checkpoint,
        const std::map<ValidatorID, crypto::BLS12_381_Signature>& signatures,
        const ValidatorSet& validator_set
    ) const;
    
    /**
     * Serialize a checkpoint for distribution.
     * 
     * @param checkpoint The checkpoint to serialize
     * @return Serialized checkpoint bytes
     * 
     * Requirements: 13.5
     */
    std::vector<uint8_t> serialize_checkpoint(
        const WeakSubjectivityCheckpoint& checkpoint
    ) const;
    
    /**
     * Verify a checkpoint's signature.
     * 
     * Checks that:
     * 1. Signers represent ≥2/3 of validator set stake
     * 2. Aggregated signature is valid for checkpoint hash
     * 3. All signers are in the validator set
     * 
     * @param checkpoint The checkpoint to verify
     * @param validator_set The validator set at the checkpoint epoch
     * @return true if checkpoint is valid, false otherwise
     * 
     * Requirements: 13.6
     */
    bool verify_checkpoint(
        const WeakSubjectivityCheckpoint& checkpoint,
        const ValidatorSet& validator_set
    ) const;
    
    /**
     * Check if a checkpoint is within the maximum age.
     * 
     * @param checkpoint_height The checkpoint block height
     * @param current_height The current block height
     * @return true if (current_height - checkpoint_height) < max_checkpoint_age
     * 
     * Requirements: 13.3
     */
    bool check_checkpoint_age(
        uint64_t checkpoint_height,
        uint64_t current_height
    ) const;
    
    /**
     * Verify that a chain includes a checkpoint block.
     * 
     * Checks that the chain being synced contains the checkpoint block
     * at the expected height with the expected hash.
     * 
     * @param checkpoint The trusted checkpoint
     * @param chain_block The block from the chain at checkpoint height
     * @return true if chain includes checkpoint, false otherwise
     * 
     * Requirements: 13.4
     */
    bool verify_chain_includes_checkpoint(
        const WeakSubjectivityCheckpoint& checkpoint,
        const Block& chain_block
    ) const;
    
    /**
     * Reject a chain that doesn't include the checkpoint.
     * 
     * This is a convenience method that returns false if the chain
     * doesn't include the checkpoint, indicating the chain should be rejected.
     * 
     * @param checkpoint The trusted checkpoint
     * @param chain_block The block from the chain at checkpoint height
     * @return false if chain should be rejected, true if valid
     * 
     * Requirements: 13.4
     */
    bool reject_chain_without_checkpoint(
        const WeakSubjectivityCheckpoint& checkpoint,
        const Block& chain_block
    ) const;
    
    /**
     * Get the checkpoint interval.
     * 
     * @return The number of blocks between checkpoints
     */
    uint64_t checkpoint_interval() const { return config_.checkpoint_interval; }
    
    /**
     * Get the maximum checkpoint age.
     * 
     * @return The maximum age in blocks
     */
    uint64_t max_checkpoint_age() const { return config_.max_checkpoint_age; }

private:
    Config config_;
};

} // namespace consensus
} // namespace sarafu
