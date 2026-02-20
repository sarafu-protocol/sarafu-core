#include <gtest/gtest.h>
#include <random>
#include <memory>
#include "sarafu/consensus/block_validator.h"
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/state/state_machine.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/merkle_tree.h"
#include "../test_utils.h"

using namespace sarafu;
using namespace sarafu::consensus;
using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Test for Block Validation
 * 
 * **Validates: Requirements 17.2, 17.3, 17.4, 17.7**
 * 
 * This test suite validates the following properties:
 * - Property 44: Block Height Increment
 * - Property 45: Block Hash Chain
 * - Property 46: Timestamp Monotonicity
 * - Property 48: Transaction Root Consistency
 * 
 * Testing Strategy:
 * 1. Generate random valid blocks and verify they pass validation
 * 2. Generate blocks with specific violations and verify they fail
 * 3. Test edge cases and boundary conditions
 * 4. Verify deterministic behavior across multiple runs
 */
class BlockValidationPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize random number generator with a fixed seed for reproducibility
        rng_.seed(42);
        
        // Create validator registry
        ValidatorRegistry::Config registry_config;
        registry_config.active_validator_count = 10;
        registry_config.minimum_self_bond = 100000;
        validator_registry_ = std::make_shared<ValidatorRegistry>(registry_config);
        
        // Create state machine
        state_machine_ = std::make_shared<StateMachine>(1);  // chain_id = 1
        
        // Create block validator
        block_validator_ = std::make_unique<BlockValidator>(
            validator_registry_,
            state_machine_
        );
        
        // Add some validators to the registry
        for (int i = 0; i < 10; ++i) {
            auto consensus_keypair = BLS12_381::generate_keypair();
            auto withdrawal_keypair = Ed25519::generate_keypair();
            ValidatorID id = test_utils::generate_random_address();
            validator_registry_->add_validator(
                id,
                consensus_keypair.first,
                withdrawal_keypair.first,
                100000 + i * 10000
            );
            validator_ids_.push_back(id);
        }
        
        // Transition to epoch 0 to activate validators
        validator_registry_->transition_epoch(0, 0);
    }

    // Generate a random block with valid structure
    Block generate_random_block(uint64_t height, const Blake3Hash& previous_hash, uint64_t timestamp) {
        BlockHeader header;
        header.height = height;
        header.timestamp = timestamp;
        header.previous_hash = previous_hash;
        header.state_root = generate_random_hash();
        header.transactions_root = Blake3Hash::zero();  // Empty transactions
        header.validator_set_root = validator_registry_->compute_validator_set_root(
            validator_registry_->current_set()
        );
        header.proposer = validator_ids_[rng_() % validator_ids_.size()];
        header.epoch = height / 10000;
        
        Block block;
        block.header = header;
        block.transactions = {};  // Empty transactions for simplicity
        block.justify = QuorumCertificate();  // Simplified QC
        
        return block;
    }

    // Generate a random hash
    Blake3Hash generate_random_hash() {
        std::vector<uint8_t> random_bytes(32);
        for (size_t i = 0; i < 32; ++i) {
            random_bytes[i] = static_cast<uint8_t>(rng_() % 256);
        }
        return Blake3Hash(random_bytes);
    }

    // Generate a random transaction
    Transaction generate_random_transaction() {
        auto sender_keypair = Ed25519::generate_keypair();
        auto recipient = test_utils::generate_random_address();
        
        Transaction tx;
        tx.from = test_utils::address_from_public_key(sender_keypair.first);
        tx.to = recipient;
        tx.amount = rng_() % 1000000;
        tx.nonce = rng_() % 1000;
        tx.fee = rng_() % 10000;
        tx.gas_limit = 21000;
        tx.chain_id = 1;
        
        // Sign the transaction
        auto tx_hash = tx.hash();
        tx.signature = Ed25519::sign(tx_hash.serialize(), sender_keypair.second);
        
        return tx;
    }

    std::mt19937_64 rng_;
    std::shared_ptr<ValidatorRegistry> validator_registry_;
    std::shared_ptr<StateMachine> state_machine_;
    std::unique_ptr<BlockValidator> block_validator_;
    std::vector<ValidatorID> validator_ids_;
};

/**
 * Property 44: Block Height Increment
 * 
 * **Validates: Requirements 17.2**
 * 
 * For any valid block B with parent P, B.height = P.height + 1.
 * 
 * This property ensures that the blockchain grows sequentially without gaps.
 */
TEST_F(BlockValidationPropertyTest, BlockHeightIncrement) {
    const int NUM_TRIALS = 1000;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random parent block
        uint64_t parent_height = rng_() % 1000000;
        uint64_t parent_timestamp = 1000000 + parent_height * 2;
        Blake3Hash parent_prev_hash = generate_random_hash();
        
        Block parent_block = generate_random_block(parent_height, parent_prev_hash, parent_timestamp);
        Blake3Hash parent_hash = parent_block.hash();
        
        // Generate child block with correct height
        Block valid_child = generate_random_block(
            parent_height + 1,
            parent_hash,
            parent_timestamp + 2
        );
        
        // Verify height validation passes
        std::string error = block_validator_->verify_block_height(valid_child, parent_block);
        EXPECT_TRUE(error.empty()) << "Trial " << trial << ": Valid height should pass: " << error;
        
        // Generate child block with incorrect height (too high)
        Block invalid_child_high = generate_random_block(
            parent_height + 2,
            parent_hash,
            parent_timestamp + 2
        );
        
        error = block_validator_->verify_block_height(invalid_child_high, parent_block);
        EXPECT_FALSE(error.empty()) << "Trial " << trial << ": Height too high should fail";
        
        // Generate child block with incorrect height (same as parent)
        Block invalid_child_same = generate_random_block(
            parent_height,
            parent_hash,
            parent_timestamp + 2
        );
        
        error = block_validator_->verify_block_height(invalid_child_same, parent_block);
        EXPECT_FALSE(error.empty()) << "Trial " << trial << ": Same height should fail";
        
        // Generate child block with incorrect height (lower than parent)
        if (parent_height > 0) {
            Block invalid_child_low = generate_random_block(
                parent_height - 1,
                parent_hash,
                parent_timestamp + 2
            );
            
            error = block_validator_->verify_block_height(invalid_child_low, parent_block);
            EXPECT_FALSE(error.empty()) << "Trial " << trial << ": Height lower than parent should fail";
        }
    }
}

/**
 * Property 45: Block Hash Chain
 * 
 * **Validates: Requirements 17.3**
 * 
 * For any valid block B with parent P, B.previous_hash = hash(P.header).
 * 
 * This property ensures that blocks are cryptographically linked in a chain.
 */
TEST_F(BlockValidationPropertyTest, BlockHashChain) {
    const int NUM_TRIALS = 1000;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random parent block
        uint64_t parent_height = rng_() % 1000000;
        uint64_t parent_timestamp = 1000000 + parent_height * 2;
        Blake3Hash parent_prev_hash = generate_random_hash();
        
        Block parent_block = generate_random_block(parent_height, parent_prev_hash, parent_timestamp);
        Blake3Hash parent_hash = parent_block.hash();
        
        // Generate child block with correct previous_hash
        Block valid_child = generate_random_block(
            parent_height + 1,
            parent_hash,
            parent_timestamp + 2
        );
        
        // Verify hash chain validation passes
        std::string error = block_validator_->verify_block_hash_chain(valid_child, parent_block);
        EXPECT_TRUE(error.empty()) << "Trial " << trial << ": Valid hash chain should pass: " << error;
        
        // Generate child block with incorrect previous_hash
        Blake3Hash wrong_hash = generate_random_hash();
        Block invalid_child = generate_random_block(
            parent_height + 1,
            wrong_hash,
            parent_timestamp + 2
        );
        
        error = block_validator_->verify_block_hash_chain(invalid_child, parent_block);
        EXPECT_FALSE(error.empty()) << "Trial " << trial << ": Wrong previous_hash should fail";
        
        // Verify that even a single bit difference is detected
        std::vector<uint8_t> parent_hash_bytes = parent_hash.serialize();
        parent_hash_bytes[0] ^= 0x01;  // Flip one bit
        Blake3Hash slightly_wrong_hash(parent_hash_bytes);
        
        Block invalid_child_bit = generate_random_block(
            parent_height + 1,
            slightly_wrong_hash,
            parent_timestamp + 2
        );
        
        error = block_validator_->verify_block_hash_chain(invalid_child_bit, parent_block);
        EXPECT_FALSE(error.empty()) << "Trial " << trial << ": Single bit difference should fail";
    }
}

/**
 * Property 46: Timestamp Monotonicity
 * 
 * **Validates: Requirements 17.4**
 * 
 * For any valid block B with parent P, B.timestamp > P.timestamp.
 * 
 * This property ensures that block timestamps are strictly increasing.
 */
TEST_F(BlockValidationPropertyTest, TimestampMonotonicity) {
    const int NUM_TRIALS = 1000;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random parent block
        uint64_t parent_height = rng_() % 1000000;
        uint64_t parent_timestamp = 1000000 + (rng_() % 1000000);
        Blake3Hash parent_prev_hash = generate_random_hash();
        
        Block parent_block = generate_random_block(parent_height, parent_prev_hash, parent_timestamp);
        Blake3Hash parent_hash = parent_block.hash();
        
        // Generate child block with timestamp > parent (valid)
        uint64_t time_delta = 1 + (rng_() % 100);  // 1 to 100 seconds
        Block valid_child = generate_random_block(
            parent_height + 1,
            parent_hash,
            parent_timestamp + time_delta
        );
        
        // Verify timestamp validation passes
        std::string error = block_validator_->verify_timestamp(valid_child, parent_block);
        EXPECT_TRUE(error.empty()) << "Trial " << trial << ": Valid timestamp should pass: " << error;
        
        // Generate child block with same timestamp (invalid)
        Block invalid_child_same = generate_random_block(
            parent_height + 1,
            parent_hash,
            parent_timestamp
        );
        
        error = block_validator_->verify_timestamp(invalid_child_same, parent_block);
        EXPECT_FALSE(error.empty()) << "Trial " << trial << ": Same timestamp should fail";
        
        // Generate child block with timestamp < parent (invalid)
        if (parent_timestamp > 0) {
            uint64_t earlier_timestamp = parent_timestamp - (1 + rng_() % 100);
            Block invalid_child_earlier = generate_random_block(
                parent_height + 1,
                parent_hash,
                earlier_timestamp
            );
            
            error = block_validator_->verify_timestamp(invalid_child_earlier, parent_block);
            EXPECT_FALSE(error.empty()) << "Trial " << trial << ": Earlier timestamp should fail";
        }
        
        // Test edge case: timestamp exactly 1 second after parent (valid)
        Block valid_child_min_delta = generate_random_block(
            parent_height + 1,
            parent_hash,
            parent_timestamp + 1
        );
        
        error = block_validator_->verify_timestamp(valid_child_min_delta, parent_block);
        EXPECT_TRUE(error.empty()) << "Trial " << trial << ": Timestamp +1 should pass: " << error;
    }
}

/**
 * Property 48: Transaction Root Consistency
 * 
 * **Validates: Requirements 17.7**
 * 
 * For any block B, B.transactions_root equals the Merkle root of all transactions in B.
 * 
 * This property ensures that the transaction list cannot be tampered with.
 */
TEST_F(BlockValidationPropertyTest, TransactionRootConsistency) {
    const int NUM_TRIALS = 500;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random block
        uint64_t height = rng_() % 1000000;
        uint64_t timestamp = 1000000 + height * 2;
        Blake3Hash previous_hash = generate_random_hash();
        
        Block block = generate_random_block(height, previous_hash, timestamp);
        
        // Test 1: Empty transaction list
        block.transactions.clear();
        block.header.transactions_root = Blake3Hash::zero();
        
        std::string error = block_validator_->verify_transactions_root(block);
        EXPECT_TRUE(error.empty()) << "Trial " << trial << ": Empty transactions should pass: " << error;
        
        // Test 2: Single transaction
        Transaction tx1 = generate_random_transaction();
        block.transactions = {tx1};
        
        // Compute correct Merkle root
        std::vector<Blake3Hash> tx_hashes = {tx1.hash()};
        MerkleTree merkle_tree;
        merkle_tree.build_tree(tx_hashes);
        block.header.transactions_root = merkle_tree.get_root();
        
        error = block_validator_->verify_transactions_root(block);
        EXPECT_TRUE(error.empty()) << "Trial " << trial << ": Single transaction should pass: " << error;
        
        // Test 3: Multiple transactions
        size_t num_txs = 2 + (rng_() % 10);  // 2 to 11 transactions
        block.transactions.clear();
        tx_hashes.clear();
        
        for (size_t i = 0; i < num_txs; ++i) {
            Transaction tx = generate_random_transaction();
            block.transactions.push_back(tx);
            tx_hashes.push_back(tx.hash());
        }
        
        // Compute correct Merkle root
        MerkleTree merkle_tree_multi;
        merkle_tree_multi.build_tree(tx_hashes);
        block.header.transactions_root = merkle_tree_multi.get_root();
        
        error = block_validator_->verify_transactions_root(block);
        EXPECT_TRUE(error.empty()) << "Trial " << trial << ": Multiple transactions should pass: " << error;
        
        // Test 4: Wrong transactions root (should fail)
        block.header.transactions_root = generate_random_hash();
        
        error = block_validator_->verify_transactions_root(block);
        EXPECT_FALSE(error.empty()) << "Trial " << trial << ": Wrong transactions root should fail";
        
        // Test 5: Transactions root doesn't match if transaction order changes
        if (block.transactions.size() >= 2) {
            // Swap first two transactions
            std::swap(block.transactions[0], block.transactions[1]);
            
            // Recompute correct root for swapped order
            tx_hashes.clear();
            for (const auto& tx : block.transactions) {
                tx_hashes.push_back(tx.hash());
            }
            MerkleTree merkle_tree_swapped;
            merkle_tree_swapped.build_tree(tx_hashes);
            Blake3Hash correct_swapped_root = merkle_tree_swapped.get_root();
            
            // Use the old root (before swap)
            block.header.transactions_root = merkle_tree_multi.get_root();
            
            error = block_validator_->verify_transactions_root(block);
            EXPECT_FALSE(error.empty()) << "Trial " << trial << ": Changed transaction order should fail";
        }
    }
}

/**
 * Additional Property: Deterministic Validation
 * 
 * Running the same validation multiple times should produce the same result.
 */
TEST_F(BlockValidationPropertyTest, DeterministicValidation) {
    const int NUM_TRIALS = 200;
    const int NUM_REPEATS = 5;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random parent and child blocks
        uint64_t parent_height = rng_() % 1000000;
        uint64_t parent_timestamp = 1000000 + parent_height * 2;
        Blake3Hash parent_prev_hash = generate_random_hash();
        
        Block parent_block = generate_random_block(parent_height, parent_prev_hash, parent_timestamp);
        Blake3Hash parent_hash = parent_block.hash();
        
        Block child_block = generate_random_block(
            parent_height + 1,
            parent_hash,
            parent_timestamp + 2
        );
        
        // Run validation multiple times
        std::vector<std::string> results;
        for (int repeat = 0; repeat < NUM_REPEATS; ++repeat) {
            std::string error = block_validator_->verify_block_height(child_block, parent_block);
            results.push_back(error);
        }
        
        // All results should be identical
        for (int i = 1; i < NUM_REPEATS; ++i) {
            EXPECT_EQ(results[0], results[i])
                << "Trial " << trial << ": Validation should be deterministic";
        }
    }
}

/**
 * Additional Property: Validation Independence
 * 
 * Each validation check should be independent - failing one check
 * should not affect the result of other checks.
 */
TEST_F(BlockValidationPropertyTest, ValidationIndependence) {
    const int NUM_TRIALS = 200;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random parent block
        uint64_t parent_height = rng_() % 1000000;
        uint64_t parent_timestamp = 1000000 + parent_height * 2;
        Blake3Hash parent_prev_hash = generate_random_hash();
        
        Block parent_block = generate_random_block(parent_height, parent_prev_hash, parent_timestamp);
        Blake3Hash parent_hash = parent_block.hash();
        
        // Create a block with multiple violations
        Block invalid_block = generate_random_block(
            parent_height + 2,  // Wrong height
            generate_random_hash(),  // Wrong previous_hash
            parent_timestamp - 1  // Wrong timestamp
        );
        
        // Each validation check should independently detect its violation
        std::string height_error = block_validator_->verify_block_height(invalid_block, parent_block);
        EXPECT_FALSE(height_error.empty()) << "Trial " << trial << ": Height check should fail";
        
        std::string hash_error = block_validator_->verify_block_hash_chain(invalid_block, parent_block);
        EXPECT_FALSE(hash_error.empty()) << "Trial " << trial << ": Hash chain check should fail";
        
        std::string timestamp_error = block_validator_->verify_timestamp(invalid_block, parent_block);
        EXPECT_FALSE(timestamp_error.empty()) << "Trial " << trial << ": Timestamp check should fail";
    }
}
