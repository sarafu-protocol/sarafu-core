#include "sarafu/consensus/checkpoint_manager.h"
#include "sarafu/consensus/block.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <map>
#include <algorithm>

using namespace sarafu::consensus;
using namespace sarafu::crypto;
using namespace sarafu::state;

/**
 * Property-Based Test for Checkpoint Production
 * 
 * **Validates: Requirements 13.1, 13.2, 13.6**
 * 
 * Property 55: Checkpoint Periodicity
 * For any block height H where H % 1,000,000 = 0, a weak subjectivity checkpoint is produced.
 * 
 * Property 56: Checkpoint Signature Validity
 * For any checkpoint C, C is signed by ≥2/3 of the validator set stake at the checkpoint epoch.
 * 
 * This test validates that:
 * 1. Checkpoints are produced at correct intervals (every 1,000,000 blocks)
 * 2. Checkpoints are not produced at non-checkpoint heights
 * 3. Checkpoint signatures require ≥2/3 validator stake
 * 4. Checkpoint signatures verify correctly
 * 5. Checkpoints with <2/3 stake cannot be created
 * 6. Checkpoint data matches the source block
 */
class CheckpointProductionPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        
        // Initialize checkpoint manager with default config
        checkpoint_manager_ = std::make_unique<CheckpointManager>();
    }

    // Generate random data of specified size
    std::vector<uint8_t> generate_random_data(size_t size) {
        std::vector<uint8_t> data(size);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return data;
    }

    // Generate a random validator set with specified number of validators
    ValidatorSet generate_validator_set(size_t num_validators, uint64_t stake_per_validator = 1000000) {
        std::vector<Validator> validators;
        validators.reserve(num_validators);

        uint64_t total_stake = 0;

        for (size_t i = 0; i < num_validators; ++i) {
            // Generate keys
            auto [bls_public_key, bls_private_key] = BLS12_381::generate_keypair();
            auto [ed_public_key, ed_private_key] = Ed25519::generate_keypair();

            // Create validator ID (address)
            std::vector<uint8_t> id_bytes = generate_random_data(32);
            ValidatorID id(id_bytes);

            // Vary stake slightly for realism
            std::uniform_int_distribution<uint64_t> stake_dist(
                stake_per_validator * 8 / 10,
                stake_per_validator * 12 / 10
            );
            uint64_t stake = stake_dist(rng_);

            Validator validator(id, bls_public_key, ed_public_key, stake);
            validator.status = ValidatorStatus::Active;

            validators.push_back(validator);
            total_stake += stake;

            // Store private key for signing
            validator_private_keys_[id] = bls_private_key;
        }

        return ValidatorSet(0, validators, total_stake);
    }

    // Generate a random block at specified height
    Block generate_block(uint64_t height, uint64_t epoch = 0) {
        BlockHeader header;
        header.height = height;
        header.timestamp = 1000000 + height * 2;  // 2 seconds per block
        header.previous_hash = Blake3Hash::hash(generate_random_data(64));
        header.state_root = Blake3Hash::hash(generate_random_data(64));
        header.transactions_root = Blake3Hash::hash(generate_random_data(64));
        header.validator_set_root = Blake3Hash::hash(generate_random_data(64));
        
        // Generate address properly (32 bytes)
        std::vector<uint8_t> proposer_bytes = generate_random_data(32);
        header.proposer = Address(proposer_bytes);
        header.epoch = epoch;

        Block block;
        block.header = header;
        // Empty transactions for simplicity
        block.justify = QuorumCertificate();

        return block;
    }

    std::mt19937 rng_;
    std::map<ValidatorID, BLS12_381_PrivateKey> validator_private_keys_;
    std::unique_ptr<CheckpointManager> checkpoint_manager_;
};

/**
 * Property 55: Checkpoint Periodicity
 * 
 * For any block height H where H % 1,000,000 = 0, a weak subjectivity
 * checkpoint is produced.
 */
TEST_F(CheckpointProductionPropertyTest, CheckpointProducedAtCorrectIntervals) {
    const int NUM_TRIALS = 200;
    const uint64_t CHECKPOINT_INTERVAL = 1000000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random block heights
        std::uniform_int_distribution<uint64_t> height_dist(0, 10000000);
        uint64_t height = height_dist(rng_);

        bool should_produce = checkpoint_manager_->should_produce_checkpoint(height);
        bool is_checkpoint_height = (height > 0) && (height % CHECKPOINT_INTERVAL == 0);

        ASSERT_EQ(should_produce, is_checkpoint_height)
            << "Checkpoint production mismatch at height " << height;
    }
}

/**
 * Property 55: Checkpoint Periodicity - Exact Multiples
 * 
 * Checkpoints should be produced at exactly 1,000,000, 2,000,000, 3,000,000, etc.
 */
TEST_F(CheckpointProductionPropertyTest, CheckpointAtExactMultiples) {
    const uint64_t CHECKPOINT_INTERVAL = 1000000;
    const int NUM_CHECKPOINTS = 100;

    for (int i = 1; i <= NUM_CHECKPOINTS; ++i) {
        uint64_t checkpoint_height = i * CHECKPOINT_INTERVAL;
        
        ASSERT_TRUE(checkpoint_manager_->should_produce_checkpoint(checkpoint_height))
            << "Failed to produce checkpoint at height " << checkpoint_height;
    }
}

/**
 * Property 55: Checkpoint Periodicity - No Checkpoint at Non-Multiples
 * 
 * Checkpoints should NOT be produced at heights that are not multiples of 1,000,000.
 */
TEST_F(CheckpointProductionPropertyTest, NoCheckpointAtNonMultiples) {
    const int NUM_TRIALS = 200;
    const uint64_t CHECKPOINT_INTERVAL = 1000000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate heights that are NOT multiples of checkpoint interval
        std::uniform_int_distribution<uint64_t> offset_dist(1, CHECKPOINT_INTERVAL - 1);
        std::uniform_int_distribution<uint64_t> multiple_dist(0, 10);
        
        uint64_t height = multiple_dist(rng_) * CHECKPOINT_INTERVAL + offset_dist(rng_);
        
        ASSERT_FALSE(checkpoint_manager_->should_produce_checkpoint(height))
            << "Incorrectly produced checkpoint at non-checkpoint height " << height;
    }
}

/**
 * Property 55: Checkpoint Periodicity - Height Zero
 * 
 * No checkpoint should be produced at height 0 (genesis block).
 */
TEST_F(CheckpointProductionPropertyTest, NoCheckpointAtHeightZero) {
    ASSERT_FALSE(checkpoint_manager_->should_produce_checkpoint(0))
        << "Incorrectly produced checkpoint at height 0";
}

/**
 * Property: Checkpoint Data Matches Block
 * 
 * A checkpoint produced from a block should contain the correct block data.
 */
TEST_F(CheckpointProductionPropertyTest, CheckpointDataMatchesBlock) {
    const int NUM_TRIALS = 150;
    const uint64_t CHECKPOINT_INTERVAL = 1000000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate checkpoint height
        std::uniform_int_distribution<uint64_t> multiple_dist(1, 100);
        uint64_t height = multiple_dist(rng_) * CHECKPOINT_INTERVAL;
        
        // Generate random epoch
        std::uniform_int_distribution<uint64_t> epoch_dist(0, 1000);
        uint64_t epoch = epoch_dist(rng_);
        
        // Generate block
        Block block = generate_block(height, epoch);
        
        // Produce checkpoint
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(block);
        
        // Verify checkpoint data matches block
        ASSERT_EQ(checkpoint.block_height, block.header.height)
            << "Checkpoint height mismatch";
        ASSERT_EQ(checkpoint.block_hash, block.hash())
            << "Checkpoint block hash mismatch";
        ASSERT_EQ(checkpoint.epoch, block.header.epoch)
            << "Checkpoint epoch mismatch";
        ASSERT_EQ(checkpoint.timestamp, block.header.timestamp)
            << "Checkpoint timestamp mismatch";
        ASSERT_EQ(checkpoint.state_root, block.header.state_root)
            << "Checkpoint state root mismatch";
        ASSERT_EQ(checkpoint.validator_set_root, block.header.validator_set_root)
            << "Checkpoint validator set root mismatch";
    }
}

/**
 * Property 56: Checkpoint Signature Validity - Supermajority Requirement
 * 
 * For any checkpoint C, C is signed by ≥2/3 of the validator set stake
 * at the checkpoint epoch.
 */
TEST_F(CheckpointProductionPropertyTest, CheckpointRequiresSupermajoritySignatures) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set (10-100 validators)
        std::uniform_int_distribution<size_t> validator_dist(10, 100);
        size_t num_validators = validator_dist(rng_);
        
        ValidatorSet validator_set = generate_validator_set(num_validators);
        
        // Generate checkpoint block
        Block block = generate_block(1000000, 0);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(block);
        
        // Calculate required stake
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;
        
        // Collect signatures from random subset of validators
        std::uniform_real_distribution<double> fraction_dist(0.5, 1.0);
        double signing_fraction = fraction_dist(rng_);
        size_t num_signers = static_cast<size_t>(num_validators * signing_fraction);
        
        // Shuffle validators and select first num_signers
        std::vector<Validator> shuffled_validators = validator_set.validators;
        std::shuffle(shuffled_validators.begin(), shuffled_validators.end(), rng_);
        
        std::map<ValidatorID, BLS12_381_Signature> signatures;
        uint64_t accumulated_stake = 0;
        
        for (size_t i = 0; i < num_signers; ++i) {
            const auto& validator = shuffled_validators[i];
            auto sig = checkpoint_manager_->sign_checkpoint(
                checkpoint,
                validator.id,
                validator_private_keys_[validator.id]
            );
            signatures[validator.id] = sig;
            accumulated_stake += validator.bonded_stake;
        }
        
        // Try to aggregate signatures
        auto signed_checkpoint_opt = checkpoint_manager_->aggregate_signatures(
            checkpoint,
            signatures,
            validator_set
        );
        
        if (accumulated_stake >= required_stake) {
            // Should create signed checkpoint
            ASSERT_TRUE(signed_checkpoint_opt.has_value())
                << "Failed to create signed checkpoint with " << accumulated_stake 
                << " stake (required: " << required_stake << ")";
            
            WeakSubjectivityCheckpoint signed_checkpoint = signed_checkpoint_opt.value();
            
            // Verify checkpoint has correct stake
            ASSERT_GE(signed_checkpoint.total_stake_signed, required_stake)
                << "Checkpoint total_stake_signed is below supermajority threshold";
            
            // Verify checkpoint has correct number of signers
            ASSERT_EQ(signed_checkpoint.signers.size(), num_signers)
                << "Checkpoint signers count mismatch";
        } else {
            // Should not create signed checkpoint
            ASSERT_FALSE(signed_checkpoint_opt.has_value())
                << "Created signed checkpoint with insufficient stake: " << accumulated_stake 
                << " (required: " << required_stake << ")";
        }
    }
}

/**
 * Property 56: Checkpoint Signature Validity - Signature Verification
 * 
 * For any checkpoint with ≥2/3 stake signatures, the aggregated signature
 * must verify correctly.
 */
TEST_F(CheckpointProductionPropertyTest, CheckpointSignatureVerification) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set (10-100 validators)
        std::uniform_int_distribution<size_t> validator_dist(10, 100);
        size_t num_validators = validator_dist(rng_);
        
        ValidatorSet validator_set = generate_validator_set(num_validators);
        
        // Generate checkpoint block
        Block block = generate_block(1000000, 0);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(block);
        
        // Calculate required stake
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;
        
        // Collect signatures from enough validators to reach supermajority
        std::map<ValidatorID, BLS12_381_Signature> signatures;
        uint64_t accumulated_stake = 0;
        
        for (const auto& validator : validator_set.validators) {
            auto sig = checkpoint_manager_->sign_checkpoint(
                checkpoint,
                validator.id,
                validator_private_keys_[validator.id]
            );
            signatures[validator.id] = sig;
            accumulated_stake += validator.bonded_stake;
            
            if (accumulated_stake >= required_stake) {
                break;
            }
        }
        
        // Aggregate signatures
        auto signed_checkpoint_opt = checkpoint_manager_->aggregate_signatures(
            checkpoint,
            signatures,
            validator_set
        );
        
        ASSERT_TRUE(signed_checkpoint_opt.has_value())
            << "Failed to create signed checkpoint with " << accumulated_stake 
            << " stake (required: " << required_stake << ")";
        
        WeakSubjectivityCheckpoint signed_checkpoint = signed_checkpoint_opt.value();
        
        // Verify checkpoint signature
        bool is_valid = checkpoint_manager_->verify_checkpoint(signed_checkpoint, validator_set);
        ASSERT_TRUE(is_valid)
            << "Checkpoint signature verification failed";
    }
}

/**
 * Property: Checkpoint with less than 2/3 stake cannot be created
 * 
 * Attempting to aggregate signatures with <2/3 stake should fail.
 */
TEST_F(CheckpointProductionPropertyTest, CheckpointRequiresAtLeastTwoThirdsStake) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> validator_dist(10, 50);
        size_t num_validators = validator_dist(rng_);
        
        ValidatorSet validator_set = generate_validator_set(num_validators);
        
        // Generate checkpoint block
        Block block = generate_block(1000000, 0);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(block);
        
        // Calculate required stake
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;
        
        // Collect signatures from subset that's definitely less than 2/3
        std::uniform_real_distribution<double> fraction_dist(0.3, 0.65);
        double signing_fraction = fraction_dist(rng_);
        size_t num_signers = static_cast<size_t>(num_validators * signing_fraction);
        
        std::map<ValidatorID, BLS12_381_Signature> signatures;
        uint64_t accumulated_stake = 0;
        
        for (size_t i = 0; i < num_signers; ++i) {
            const auto& validator = validator_set.validators[i];
            auto sig = checkpoint_manager_->sign_checkpoint(
                checkpoint,
                validator.id,
                validator_private_keys_[validator.id]
            );
            signatures[validator.id] = sig;
            accumulated_stake += validator.bonded_stake;
        }
        
        // Only test if we're actually below threshold
        if (accumulated_stake < required_stake) {
            // Should not create signed checkpoint
            auto signed_checkpoint_opt = checkpoint_manager_->aggregate_signatures(
                checkpoint,
                signatures,
                validator_set
            );
            
            ASSERT_FALSE(signed_checkpoint_opt.has_value())
                << "Created signed checkpoint with insufficient stake: " << accumulated_stake 
                << " (required: " << required_stake << ")";
        }
    }
}

/**
 * Property: Checkpoint signature is bound to checkpoint data
 * 
 * A checkpoint signature should not verify if the checkpoint data is modified.
 */
TEST_F(CheckpointProductionPropertyTest, CheckpointSignatureBoundToData) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> validator_dist(10, 50);
        size_t num_validators = validator_dist(rng_);
        
        ValidatorSet validator_set = generate_validator_set(num_validators);
        
        // Generate checkpoint block
        Block block = generate_block(1000000, 0);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(block);
        
        // Collect signatures from all validators
        std::map<ValidatorID, BLS12_381_Signature> signatures;
        for (const auto& validator : validator_set.validators) {
            auto sig = checkpoint_manager_->sign_checkpoint(
                checkpoint,
                validator.id,
                validator_private_keys_[validator.id]
            );
            signatures[validator.id] = sig;
        }
        
        // Aggregate signatures
        auto signed_checkpoint_opt = checkpoint_manager_->aggregate_signatures(
            checkpoint,
            signatures,
            validator_set
        );
        
        ASSERT_TRUE(signed_checkpoint_opt.has_value());
        WeakSubjectivityCheckpoint signed_checkpoint = signed_checkpoint_opt.value();
        
        // Verify original checkpoint (should succeed)
        ASSERT_TRUE(checkpoint_manager_->verify_checkpoint(signed_checkpoint, validator_set))
            << "Original checkpoint verification failed";
        
        // Modify checkpoint data
        WeakSubjectivityCheckpoint modified_checkpoint = signed_checkpoint;
        modified_checkpoint.block_height += 1;
        
        // Verify modified checkpoint (should fail)
        bool is_valid_modified = checkpoint_manager_->verify_checkpoint(modified_checkpoint, validator_set);
        ASSERT_FALSE(is_valid_modified)
            << "Modified checkpoint incorrectly verified";
    }
}

/**
 * Property: Checkpoint serialization preserves validity
 * 
 * Serializing and deserializing a checkpoint should preserve its validity.
 */
TEST_F(CheckpointProductionPropertyTest, CheckpointSerializationPreservesValidity) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> validator_dist(10, 50);
        size_t num_validators = validator_dist(rng_);
        
        ValidatorSet validator_set = generate_validator_set(num_validators);
        
        // Generate checkpoint block
        Block block = generate_block(1000000, 0);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(block);
        
        // Collect signatures from all validators
        std::map<ValidatorID, BLS12_381_Signature> signatures;
        for (const auto& validator : validator_set.validators) {
            auto sig = checkpoint_manager_->sign_checkpoint(
                checkpoint,
                validator.id,
                validator_private_keys_[validator.id]
            );
            signatures[validator.id] = sig;
        }
        
        // Aggregate signatures
        auto signed_checkpoint_opt = checkpoint_manager_->aggregate_signatures(
            checkpoint,
            signatures,
            validator_set
        );
        
        ASSERT_TRUE(signed_checkpoint_opt.has_value());
        WeakSubjectivityCheckpoint original_checkpoint = signed_checkpoint_opt.value();
        
        // Verify original checkpoint
        ASSERT_TRUE(checkpoint_manager_->verify_checkpoint(original_checkpoint, validator_set));
        
        // Serialize
        std::vector<uint8_t> serialized = checkpoint_manager_->serialize_checkpoint(original_checkpoint);
        
        // Deserialize
        WeakSubjectivityCheckpoint deserialized_checkpoint = 
            WeakSubjectivityCheckpoint::deserialize(serialized);
        
        // Verify deserialized checkpoint
        ASSERT_TRUE(checkpoint_manager_->verify_checkpoint(deserialized_checkpoint, validator_set))
            << "Deserialized checkpoint failed verification";
        
        // Checkpoints should be equal
        ASSERT_EQ(original_checkpoint, deserialized_checkpoint);
    }
}

/**
 * Property: Checkpoint with exactly 2/3 stake is valid
 * 
 * A checkpoint with exactly the minimum required stake (2/3) should be valid.
 */
TEST_F(CheckpointProductionPropertyTest, CheckpointWithExactlyTwoThirdsStake) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set with uniform stakes
        std::uniform_int_distribution<size_t> validator_dist(10, 50);
        size_t num_validators = validator_dist(rng_);
        
        uint64_t stake_per_validator = 1000000;
        ValidatorSet validator_set = generate_validator_set(num_validators, stake_per_validator);
        
        // Recalculate total stake
        uint64_t actual_total_stake = 0;
        for (const auto& v : validator_set.validators) {
            actual_total_stake += v.bonded_stake;
        }
        validator_set.total_stake = actual_total_stake;
        
        // Generate checkpoint block
        Block block = generate_block(1000000, 0);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(block);
        
        // Calculate required stake
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;
        
        // Add signatures until we reach or just exceed required stake
        std::map<ValidatorID, BLS12_381_Signature> signatures;
        uint64_t accumulated_stake = 0;
        
        for (const auto& validator : validator_set.validators) {
            if (accumulated_stake >= required_stake) {
                break;
            }
            auto sig = checkpoint_manager_->sign_checkpoint(
                checkpoint,
                validator.id,
                validator_private_keys_[validator.id]
            );
            signatures[validator.id] = sig;
            accumulated_stake += validator.bonded_stake;
        }
        
        // Should be able to create checkpoint
        auto signed_checkpoint_opt = checkpoint_manager_->aggregate_signatures(
            checkpoint,
            signatures,
            validator_set
        );
        
        ASSERT_TRUE(signed_checkpoint_opt.has_value())
            << "Failed to create checkpoint with stake " << accumulated_stake 
            << " (required: " << required_stake << ")";
        
        WeakSubjectivityCheckpoint signed_checkpoint = signed_checkpoint_opt.value();
        
        // Verify checkpoint is valid
        ASSERT_TRUE(checkpoint_manager_->verify_checkpoint(signed_checkpoint, validator_set))
            << "Checkpoint with exactly 2/3 stake failed verification";
    }
}

/**
 * Property: Checkpoint works with maximum validator set size
 * 
 * The system should handle checkpoint creation and verification for up to 500 validators.
 */
TEST_F(CheckpointProductionPropertyTest, CheckpointWithMaximumValidatorSetSize) {
    const int NUM_TRIALS = 3;  // Fewer trials due to computational cost
    const size_t MAX_VALIDATORS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorSet validator_set = generate_validator_set(MAX_VALIDATORS);
        
        // Generate checkpoint block
        Block block = generate_block(1000000, 0);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(block);
        
        // Collect signatures from all validators
        std::map<ValidatorID, BLS12_381_Signature> signatures;
        for (const auto& validator : validator_set.validators) {
            auto sig = checkpoint_manager_->sign_checkpoint(
                checkpoint,
                validator.id,
                validator_private_keys_[validator.id]
            );
            signatures[validator.id] = sig;
        }
        
        // Aggregate signatures
        auto signed_checkpoint_opt = checkpoint_manager_->aggregate_signatures(
            checkpoint,
            signatures,
            validator_set
        );
        
        ASSERT_TRUE(signed_checkpoint_opt.has_value())
            << "Failed to create checkpoint with 500 validators";
        
        WeakSubjectivityCheckpoint signed_checkpoint = signed_checkpoint_opt.value();
        
        // Verify checkpoint
        ASSERT_TRUE(checkpoint_manager_->verify_checkpoint(signed_checkpoint, validator_set))
            << "Checkpoint verification failed for 500 validators";
        
        // Verify all validators are in signers list
        ASSERT_EQ(signed_checkpoint.signers.size(), MAX_VALIDATORS)
            << "Checkpoint signers count incorrect for 500 validators";
    }
}
