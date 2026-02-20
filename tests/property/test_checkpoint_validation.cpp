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

using namespace sarafu::consensus;
using namespace sarafu::crypto;
using namespace sarafu::state;

/**
 * Property-Based Test for Checkpoint Validation
 * 
 * **Validates: Requirements 13.3, 13.4**
 * 
 * Property 57: Checkpoint Age Requirement
 * For any new node syncing, the trusted checkpoint must be less than 1,000,000 blocks old.
 * 
 * Property 58: Checkpoint Chain Inclusion
 * For any chain being synced from checkpoint C, the chain must include block C.
 * 
 * This test validates that:
 * 1. Checkpoints within the age limit are accepted
 * 2. Checkpoints exceeding the age limit are rejected
 * 3. Chains including the checkpoint block are accepted
 * 4. Chains not including the checkpoint block are rejected
 * 5. Checkpoint age calculation is correct
 * 6. Chain inclusion verification checks all relevant fields
 */
class CheckpointValidationPropertyTest : public ::testing::Test {
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
    std::unique_ptr<CheckpointManager> checkpoint_manager_;
};

/**
 * Property 57: Checkpoint Age Requirement - Within Age Limit
 * 
 * For any checkpoint less than 1,000,000 blocks old, check_checkpoint_age
 * should return true.
 */
TEST_F(CheckpointValidationPropertyTest, CheckpointWithinAgeLimitAccepted) {
    const int NUM_TRIALS = 200;
    const uint64_t MAX_AGE = 1000000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random checkpoint height
        std::uniform_int_distribution<uint64_t> checkpoint_dist(1000000, 10000000);
        uint64_t checkpoint_height = checkpoint_dist(rng_);
        
        // Generate current height within age limit
        std::uniform_int_distribution<uint64_t> age_dist(0, MAX_AGE - 1);
        uint64_t age = age_dist(rng_);
        uint64_t current_height = checkpoint_height + age;
        
        bool is_valid = checkpoint_manager_->check_checkpoint_age(checkpoint_height, current_height);
        
        ASSERT_TRUE(is_valid)
            << "Checkpoint at height " << checkpoint_height 
            << " with current height " << current_height 
            << " (age " << age << ") should be valid";
    }
}

/**
 * Property 57: Checkpoint Age Requirement - Exceeds Age Limit
 * 
 * For any checkpoint 1,000,000 or more blocks old, check_checkpoint_age
 * should return false.
 */
TEST_F(CheckpointValidationPropertyTest, CheckpointExceedingAgeLimitRejected) {
    const int NUM_TRIALS = 200;
    const uint64_t MAX_AGE = 1000000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random checkpoint height
        std::uniform_int_distribution<uint64_t> checkpoint_dist(1000000, 5000000);
        uint64_t checkpoint_height = checkpoint_dist(rng_);
        
        // Generate current height exceeding age limit
        std::uniform_int_distribution<uint64_t> age_dist(MAX_AGE, MAX_AGE + 1000000);
        uint64_t age = age_dist(rng_);
        uint64_t current_height = checkpoint_height + age;
        
        bool is_valid = checkpoint_manager_->check_checkpoint_age(checkpoint_height, current_height);
        
        ASSERT_FALSE(is_valid)
            << "Checkpoint at height " << checkpoint_height 
            << " with current height " << current_height 
            << " (age " << age << ") should be invalid";
    }
}

/**
 * Property 57: Checkpoint Age Requirement - Boundary Condition
 * 
 * A checkpoint at exactly the age limit (1,000,000 blocks) should be rejected.
 */
TEST_F(CheckpointValidationPropertyTest, CheckpointAtExactAgeLimitRejected) {
    const int NUM_TRIALS = 100;
    const uint64_t MAX_AGE = 1000000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random checkpoint height
        std::uniform_int_distribution<uint64_t> checkpoint_dist(1000000, 10000000);
        uint64_t checkpoint_height = checkpoint_dist(rng_);
        
        // Current height at exactly the age limit
        uint64_t current_height = checkpoint_height + MAX_AGE;
        
        bool is_valid = checkpoint_manager_->check_checkpoint_age(checkpoint_height, current_height);
        
        ASSERT_FALSE(is_valid)
            << "Checkpoint at height " << checkpoint_height 
            << " with current height " << current_height 
            << " (age exactly " << MAX_AGE << ") should be invalid";
    }
}

/**
 * Property 57: Checkpoint Age Requirement - Just Below Limit
 * 
 * A checkpoint at one block less than the age limit should be accepted.
 */
TEST_F(CheckpointValidationPropertyTest, CheckpointJustBelowAgeLimitAccepted) {
    const int NUM_TRIALS = 100;
    const uint64_t MAX_AGE = 1000000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random checkpoint height
        std::uniform_int_distribution<uint64_t> checkpoint_dist(1000000, 10000000);
        uint64_t checkpoint_height = checkpoint_dist(rng_);
        
        // Current height at one block less than age limit
        uint64_t current_height = checkpoint_height + MAX_AGE - 1;
        
        bool is_valid = checkpoint_manager_->check_checkpoint_age(checkpoint_height, current_height);
        
        ASSERT_TRUE(is_valid)
            << "Checkpoint at height " << checkpoint_height 
            << " with current height " << current_height 
            << " (age " << (MAX_AGE - 1) << ") should be valid";
    }
}

/**
 * Property 57: Checkpoint Age Requirement - Current Height Before Checkpoint
 * 
 * If current height is before checkpoint height, the checkpoint should be invalid.
 */
TEST_F(CheckpointValidationPropertyTest, CheckpointInFutureRejected) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random checkpoint height
        std::uniform_int_distribution<uint64_t> checkpoint_dist(1000000, 10000000);
        uint64_t checkpoint_height = checkpoint_dist(rng_);
        
        // Current height before checkpoint
        std::uniform_int_distribution<uint64_t> offset_dist(1, 100000);
        uint64_t current_height = checkpoint_height - offset_dist(rng_);
        
        bool is_valid = checkpoint_manager_->check_checkpoint_age(checkpoint_height, current_height);
        
        ASSERT_FALSE(is_valid)
            << "Checkpoint at height " << checkpoint_height 
            << " with current height " << current_height 
            << " (in future) should be invalid";
    }
}

/**
 * Property 57: Checkpoint Age Requirement - Zero Age
 * 
 * A checkpoint at the current height (age 0) should be valid.
 */
TEST_F(CheckpointValidationPropertyTest, CheckpointAtCurrentHeightAccepted) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random height
        std::uniform_int_distribution<uint64_t> height_dist(1000000, 10000000);
        uint64_t height = height_dist(rng_);
        
        bool is_valid = checkpoint_manager_->check_checkpoint_age(height, height);
        
        ASSERT_TRUE(is_valid)
            << "Checkpoint at height " << height 
            << " with current height " << height 
            << " (age 0) should be valid";
    }
}

/**
 * Property 58: Checkpoint Chain Inclusion - Matching Block
 * 
 * For any checkpoint C and chain block at the same height with matching data,
 * verify_chain_includes_checkpoint should return true.
 */
TEST_F(CheckpointValidationPropertyTest, ChainIncludingCheckpointAccepted) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate checkpoint height
        std::uniform_int_distribution<uint64_t> height_dist(1000000, 10000000);
        uint64_t height = height_dist(rng_);
        
        // Generate block
        Block block = generate_block(height);
        
        // Create checkpoint from block
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(block);
        
        // Verify chain includes checkpoint
        bool includes_checkpoint = checkpoint_manager_->verify_chain_includes_checkpoint(
            checkpoint,
            block
        );
        
        ASSERT_TRUE(includes_checkpoint)
            << "Chain should include checkpoint at height " << height;
    }
}

/**
 * Property 58: Checkpoint Chain Inclusion - Different Height
 * 
 * If the chain block is at a different height than the checkpoint,
 * verification should fail.
 */
TEST_F(CheckpointValidationPropertyTest, ChainWithDifferentHeightRejected) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate checkpoint
        std::uniform_int_distribution<uint64_t> height_dist(1000000, 10000000);
        uint64_t checkpoint_height = height_dist(rng_);
        Block checkpoint_block = generate_block(checkpoint_height);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(checkpoint_block);
        
        // Generate chain block at different height
        std::uniform_int_distribution<int64_t> offset_dist(-100000, 100000);
        int64_t offset = offset_dist(rng_);
        while (offset == 0) {
            offset = offset_dist(rng_);
        }
        uint64_t chain_height = checkpoint_height + offset;
        if (chain_height < 0) chain_height = checkpoint_height + 1;
        
        Block chain_block = generate_block(chain_height);
        
        // Verify chain does not include checkpoint
        bool includes_checkpoint = checkpoint_manager_->verify_chain_includes_checkpoint(
            checkpoint,
            chain_block
        );
        
        ASSERT_FALSE(includes_checkpoint)
            << "Chain block at height " << chain_height 
            << " should not match checkpoint at height " << checkpoint_height;
    }
}

/**
 * Property 58: Checkpoint Chain Inclusion - Different Block Hash
 * 
 * If the chain block has a different hash than the checkpoint,
 * verification should fail.
 */
TEST_F(CheckpointValidationPropertyTest, ChainWithDifferentHashRejected) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate checkpoint
        std::uniform_int_distribution<uint64_t> height_dist(1000000, 10000000);
        uint64_t height = height_dist(rng_);
        Block checkpoint_block = generate_block(height);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(checkpoint_block);
        
        // Generate different chain block at same height
        Block chain_block = generate_block(height);
        
        // Ensure blocks are different (they should be due to random data)
        if (chain_block.hash() == checkpoint_block.hash()) {
            // Regenerate if by chance they're the same
            chain_block = generate_block(height);
        }
        
        // Verify chain does not include checkpoint
        bool includes_checkpoint = checkpoint_manager_->verify_chain_includes_checkpoint(
            checkpoint,
            chain_block
        );
        
        ASSERT_FALSE(includes_checkpoint)
            << "Chain block with different hash should not match checkpoint at height " << height;
    }
}

/**
 * Property 58: Checkpoint Chain Inclusion - Different State Root
 * 
 * If the chain block has a different state root than the checkpoint,
 * verification should fail.
 */
TEST_F(CheckpointValidationPropertyTest, ChainWithDifferentStateRootRejected) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate checkpoint
        std::uniform_int_distribution<uint64_t> height_dist(1000000, 10000000);
        uint64_t height = height_dist(rng_);
        Block checkpoint_block = generate_block(height);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(checkpoint_block);
        
        // Create modified chain block with different state root
        Block chain_block = checkpoint_block;
        chain_block.header.state_root = Blake3Hash::hash(generate_random_data(64));
        
        // Verify chain does not include checkpoint
        bool includes_checkpoint = checkpoint_manager_->verify_chain_includes_checkpoint(
            checkpoint,
            chain_block
        );
        
        ASSERT_FALSE(includes_checkpoint)
            << "Chain block with different state root should not match checkpoint";
    }
}

/**
 * Property 58: Checkpoint Chain Inclusion - Different Validator Set Root
 * 
 * If the chain block has a different validator set root than the checkpoint,
 * verification should fail.
 */
TEST_F(CheckpointValidationPropertyTest, ChainWithDifferentValidatorSetRootRejected) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate checkpoint
        std::uniform_int_distribution<uint64_t> height_dist(1000000, 10000000);
        uint64_t height = height_dist(rng_);
        Block checkpoint_block = generate_block(height);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(checkpoint_block);
        
        // Create modified chain block with different validator set root
        Block chain_block = checkpoint_block;
        chain_block.header.validator_set_root = Blake3Hash::hash(generate_random_data(64));
        
        // Verify chain does not include checkpoint
        bool includes_checkpoint = checkpoint_manager_->verify_chain_includes_checkpoint(
            checkpoint,
            chain_block
        );
        
        ASSERT_FALSE(includes_checkpoint)
            << "Chain block with different validator set root should not match checkpoint";
    }
}

/**
 * Property: reject_chain_without_checkpoint is inverse of verify_chain_includes_checkpoint
 * 
 * For any checkpoint and chain block, reject_chain_without_checkpoint should
 * return the opposite of verify_chain_includes_checkpoint.
 */
TEST_F(CheckpointValidationPropertyTest, RejectChainIsInverseOfVerifyChain) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate checkpoint
        std::uniform_int_distribution<uint64_t> height_dist(1000000, 10000000);
        uint64_t height = height_dist(rng_);
        Block checkpoint_block = generate_block(height);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(checkpoint_block);
        
        // Generate chain block (sometimes matching, sometimes not)
        std::uniform_real_distribution<double> match_dist(0.0, 1.0);
        Block chain_block;
        if (match_dist(rng_) < 0.5) {
            // Use same block
            chain_block = checkpoint_block;
        } else {
            // Use different block
            chain_block = generate_block(height);
        }
        
        bool includes_checkpoint = checkpoint_manager_->verify_chain_includes_checkpoint(
            checkpoint,
            chain_block
        );
        
        bool should_reject = checkpoint_manager_->reject_chain_without_checkpoint(
            checkpoint,
            chain_block
        );
        
        ASSERT_EQ(includes_checkpoint, !should_reject)
            << "reject_chain_without_checkpoint should be inverse of verify_chain_includes_checkpoint";
    }
}

/**
 * Property: Checkpoint age calculation is consistent
 * 
 * For any checkpoint height and current height, the age should be
 * current_height - checkpoint_height.
 */
TEST_F(CheckpointValidationPropertyTest, CheckpointAgeCalculationConsistent) {
    const int NUM_TRIALS = 200;
    const uint64_t MAX_AGE = 1000000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random checkpoint height
        std::uniform_int_distribution<uint64_t> checkpoint_dist(1000000, 10000000);
        uint64_t checkpoint_height = checkpoint_dist(rng_);
        
        // Generate random age
        std::uniform_int_distribution<uint64_t> age_dist(0, MAX_AGE * 2);
        uint64_t age = age_dist(rng_);
        uint64_t current_height = checkpoint_height + age;
        
        bool is_valid = checkpoint_manager_->check_checkpoint_age(checkpoint_height, current_height);
        bool should_be_valid = (age < MAX_AGE);
        
        ASSERT_EQ(is_valid, should_be_valid)
            << "Checkpoint age validation inconsistent for age " << age;
    }
}

/**
 * Property: Chain inclusion requires exact match of all fields
 * 
 * For a chain to include a checkpoint, all relevant fields must match exactly.
 */
TEST_F(CheckpointValidationPropertyTest, ChainInclusionRequiresExactMatch) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate checkpoint
        std::uniform_int_distribution<uint64_t> height_dist(1000000, 10000000);
        uint64_t height = height_dist(rng_);
        Block checkpoint_block = generate_block(height);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(checkpoint_block);
        
        // Test with exact match (should pass)
        ASSERT_TRUE(checkpoint_manager_->verify_chain_includes_checkpoint(checkpoint, checkpoint_block))
            << "Exact match should verify";
        
        // Test with modified height (should fail)
        Block modified_block = checkpoint_block;
        modified_block.header.height += 1;
        ASSERT_FALSE(checkpoint_manager_->verify_chain_includes_checkpoint(checkpoint, modified_block))
            << "Modified height should fail";
        
        // Test with modified state root (should fail)
        modified_block = checkpoint_block;
        modified_block.header.state_root = Blake3Hash::hash(generate_random_data(64));
        ASSERT_FALSE(checkpoint_manager_->verify_chain_includes_checkpoint(checkpoint, modified_block))
            << "Modified state root should fail";
        
        // Test with modified validator set root (should fail)
        modified_block = checkpoint_block;
        modified_block.header.validator_set_root = Blake3Hash::hash(generate_random_data(64));
        ASSERT_FALSE(checkpoint_manager_->verify_chain_includes_checkpoint(checkpoint, modified_block))
            << "Modified validator set root should fail";
    }
}

/**
 * Property: Checkpoint validation is deterministic
 * 
 * Running the same validation multiple times should produce the same result.
 */
TEST_F(CheckpointValidationPropertyTest, CheckpointValidationDeterministic) {
    const int NUM_TRIALS = 100;
    const int NUM_REPEATS = 5;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate checkpoint and chain block
        std::uniform_int_distribution<uint64_t> height_dist(1000000, 10000000);
        uint64_t height = height_dist(rng_);
        Block checkpoint_block = generate_block(height);
        WeakSubjectivityCheckpoint checkpoint = checkpoint_manager_->produce_checkpoint(checkpoint_block);
        
        // Generate random current height
        std::uniform_int_distribution<uint64_t> age_dist(0, 2000000);
        uint64_t current_height = height + age_dist(rng_);
        
        // Run age check multiple times
        bool first_age_result = checkpoint_manager_->check_checkpoint_age(height, current_height);
        for (int repeat = 1; repeat < NUM_REPEATS; ++repeat) {
            bool age_result = checkpoint_manager_->check_checkpoint_age(height, current_height);
            ASSERT_EQ(age_result, first_age_result)
                << "Age check should be deterministic";
        }
        
        // Run chain inclusion check multiple times
        bool first_inclusion_result = checkpoint_manager_->verify_chain_includes_checkpoint(
            checkpoint,
            checkpoint_block
        );
        for (int repeat = 1; repeat < NUM_REPEATS; ++repeat) {
            bool inclusion_result = checkpoint_manager_->verify_chain_includes_checkpoint(
                checkpoint,
                checkpoint_block
            );
            ASSERT_EQ(inclusion_result, first_inclusion_result)
                << "Chain inclusion check should be deterministic";
        }
    }
}
