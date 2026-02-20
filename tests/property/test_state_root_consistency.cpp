#include <gtest/gtest.h>
#include "sarafu/state/state_machine.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/consensus/block.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/blake3_hash.h"
#include "test_utils.h"
#include <random>
#include <vector>

using namespace sarafu;
using namespace sarafu::state;
using namespace sarafu::consensus;
using namespace sarafu::crypto;

/**
 * Property 47: State Root Consistency
 * 
 * For any block B, B.state_root equals the Merkle root of all account states
 * after applying B's transactions.
 * 
 * **Validates: Requirements 17.6**
 * 
 * This property ensures that:
 * 1. The state root is deterministic (same transactions -> same state root)
 * 2. The state root accurately reflects the account state after execution
 * 3. Different transaction orders produce different state roots (if they affect state differently)
 */

class StateRootConsistencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        chain_id_ = 1;
        state_machine_ = std::make_unique<StateMachine>(chain_id_);
        base_fee_ = 100;
        
        // Create proposer account
        proposer_address_ = test_utils::generate_random_address();
        state_machine_->get_account_manager().create_account(proposer_address_, 0);
    }

    void TearDown() override {
        state_machine_.reset();
    }

    // Helper: Create a funded account with a key pair
    std::tuple<Address, Ed25519_PrivateKey, Ed25519_PublicKey> create_funded_account(uint64_t balance) {
        auto keypair = Ed25519::generate_keypair();
        Ed25519_PublicKey public_key = keypair.first;
        Ed25519_PrivateKey private_key = keypair.second;
        Address address = test_utils::address_from_public_key(public_key);
        state_machine_->get_account_manager().create_account(address, balance);
        return std::make_tuple(address, private_key, public_key);
    }

    // Helper: Create a signed transaction
    Transaction create_signed_transaction(
        const Address& from,
        const Address& to,
        uint64_t amount,
        uint64_t nonce,
        uint64_t fee,
        const Ed25519_PrivateKey& private_key
    ) {
        Transaction tx(from, to, amount, nonce, fee, 21000, chain_id_);
        tx.sign(private_key);
        return tx;
    }

    // Helper: Create a block with transactions
    Block create_block(
        uint64_t height,
        const std::vector<Transaction>& transactions
    ) {
        BlockHeader header;
        header.height = height;
        header.timestamp = 1000000 + height;
        header.previous_hash = Blake3Hash::zero();
        header.state_root = Blake3Hash::zero();
        header.transactions_root = Blake3Hash::zero();
        header.validator_set_root = Blake3Hash::zero();
        header.proposer = proposer_address_;
        header.epoch = 0;

        QuorumCertificate qc;
        return Block(header, transactions, qc);
    }

    uint32_t chain_id_;
    std::unique_ptr<StateMachine> state_machine_;
    uint64_t base_fee_;
    Address proposer_address_;
};

// Test 1: State root is deterministic for the same transactions
TEST_F(StateRootConsistencyTest, DeterministicStateRoot) {
    // Create two accounts
    auto [addr1, priv1, pub1] = create_funded_account(10000);
    auto [addr2, priv2, pub2] = create_funded_account(5000);

    // Create transactions
    std::vector<Transaction> transactions;
    transactions.push_back(create_signed_transaction(addr1, addr2, 100, 0, 200, priv1));
    transactions.push_back(create_signed_transaction(addr2, addr1, 50, 0, 200, priv2));

    // Execute block and get state root
    Block block1 = create_block(1, transactions);
    Blake3Hash state_root1 = state_machine_->execute_block(block1, base_fee_, proposer_address_);

    // Reset state machine
    state_machine_->clear();
    state_machine_->get_account_manager().create_account(proposer_address_, 0);
    state_machine_->get_account_manager().create_account(addr1, 10000);
    state_machine_->get_account_manager().create_account(addr2, 5000);

    // Execute same block again
    Block block2 = create_block(1, transactions);
    Blake3Hash state_root2 = state_machine_->execute_block(block2, base_fee_, proposer_address_);

    // State roots should be identical
    EXPECT_EQ(state_root1, state_root2)
        << "State root should be deterministic for the same transactions";
}

// Test 2: State root changes when account state changes
TEST_F(StateRootConsistencyTest, StateRootChangesWithState) {
    // Create account
    auto [addr1, priv1, pub1] = create_funded_account(10000);
    auto [addr2, priv2, pub2] = create_funded_account(5000);

    // Get initial state root
    Blake3Hash initial_root = state_machine_->compute_state_root();

    // Execute a transaction directly (not through a block)
    Transaction tx = create_signed_transaction(addr1, addr2, 100, 0, 200, priv1);
    TransactionReceipt receipt = state_machine_->execute_transaction(
        tx, pub1, 1, base_fee_, proposer_address_
    );

    // Verify transaction succeeded
    EXPECT_TRUE(receipt.success) << "Transaction should succeed: " << receipt.error_message;

    // Get new state root
    Blake3Hash new_root = state_machine_->compute_state_root();

    // State root should be different
    EXPECT_NE(initial_root, new_root)
        << "State root should change when account state changes";
}

// Test 3: State root matches computed state root after execution
TEST_F(StateRootConsistencyTest, StateRootMatchesComputedRoot) {
    // Create accounts
    auto [addr1, priv1, pub1] = create_funded_account(10000);
    auto [addr2, priv2, pub2] = create_funded_account(5000);

    // Create transactions
    std::vector<Transaction> transactions;
    transactions.push_back(create_signed_transaction(addr1, addr2, 100, 0, 200, priv1));
    transactions.push_back(create_signed_transaction(addr2, addr1, 50, 0, 200, priv2));

    // Execute block
    Block block = create_block(1, transactions);
    Blake3Hash state_root_from_execution = state_machine_->execute_block(block, base_fee_, proposer_address_);

    // Compute state root independently
    Blake3Hash computed_state_root = state_machine_->compute_state_root();

    // They should match
    EXPECT_EQ(state_root_from_execution, computed_state_root)
        << "State root from execution should match independently computed state root";
}

// Test 4: Empty block produces consistent state root
TEST_F(StateRootConsistencyTest, EmptyBlockConsistentStateRoot) {
    // Create accounts
    auto [addr1, priv1, pub1] = create_funded_account(10000);
    auto [addr2, priv2, pub2] = create_funded_account(5000);

    // Get initial state root
    Blake3Hash initial_root = state_machine_->compute_state_root();

    // Execute empty block
    std::vector<Transaction> empty_transactions;
    Block block = create_block(1, empty_transactions);
    Blake3Hash state_root_after_empty = state_machine_->execute_block(block, base_fee_, proposer_address_);

    // State root should be the same (no state changes)
    EXPECT_EQ(initial_root, state_root_after_empty)
        << "Empty block should not change state root";
}

// Test 5: Multiple blocks produce consistent state roots
TEST_F(StateRootConsistencyTest, MultipleBlocksConsistentStateRoots) {
    // Create accounts
    auto [addr1, priv1, pub1] = create_funded_account(10000);
    auto [addr2, priv2, pub2] = create_funded_account(5000);

    // Execute first transaction
    Transaction tx1 = create_signed_transaction(addr1, addr2, 100, 0, 200, priv1);
    TransactionReceipt receipt1 = state_machine_->execute_transaction(
        tx1, pub1, 1, base_fee_, proposer_address_
    );
    EXPECT_TRUE(receipt1.success) << "Transaction 1 should succeed: " << receipt1.error_message;
    Blake3Hash state_root1 = state_machine_->compute_state_root();

    // Execute second transaction
    Transaction tx2 = create_signed_transaction(addr2, addr1, 50, 0, 200, priv2);
    TransactionReceipt receipt2 = state_machine_->execute_transaction(
        tx2, pub2, 2, base_fee_, proposer_address_
    );
    EXPECT_TRUE(receipt2.success) << "Transaction 2 should succeed: " << receipt2.error_message;
    Blake3Hash state_root2 = state_machine_->compute_state_root();

    // State roots should be different
    EXPECT_NE(state_root1, state_root2)
        << "Different blocks should produce different state roots";

    // Verify state root 2 matches computed root
    Blake3Hash computed_root2 = state_machine_->compute_state_root();
    EXPECT_EQ(state_root2, computed_root2)
        << "State root after second block should match computed root";
}

// Property-based test: State root consistency across random transactions
TEST_F(StateRootConsistencyTest, PropertyBasedStateRootConsistency) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> amount_dist(1, 1000);
    std::uniform_int_distribution<uint64_t> fee_dist(100, 500);

    const int NUM_ITERATIONS = 100;
    const int NUM_ACCOUNTS = 5;
    const int NUM_TRANSACTIONS_PER_BLOCK = 3;

    for (int iteration = 0; iteration < NUM_ITERATIONS; ++iteration) {
        // Reset state machine
        state_machine_->clear();
        state_machine_->get_account_manager().create_account(proposer_address_, 0);

        // Create funded accounts
        std::vector<std::tuple<Address, Ed25519_PrivateKey, Ed25519_PublicKey>> accounts;
        for (int i = 0; i < NUM_ACCOUNTS; ++i) {
            accounts.push_back(create_funded_account(100000));
        }

        // Create random transactions
        std::vector<Transaction> transactions;
        for (int i = 0; i < NUM_TRANSACTIONS_PER_BLOCK; ++i) {
            int from_idx = gen() % NUM_ACCOUNTS;
            int to_idx = gen() % NUM_ACCOUNTS;
            if (from_idx == to_idx) {
                to_idx = (to_idx + 1) % NUM_ACCOUNTS;
            }

            auto& [from_addr, from_priv, from_pub] = accounts[from_idx];
            auto& [to_addr, to_priv, to_pub] = accounts[to_idx];

            uint64_t nonce = state_machine_->get_account_manager().get_nonce(from_addr);
            uint64_t amount = amount_dist(gen);
            uint64_t fee = fee_dist(gen);

            // Check if sender has sufficient balance
            uint64_t balance = state_machine_->get_account_manager().get_balance(from_addr);
            if (balance >= amount + fee) {
                transactions.push_back(create_signed_transaction(
                    from_addr, to_addr, amount, nonce, fee, from_priv
                ));
            }
        }

        // Execute block
        Block block = create_block(1, transactions);
        Blake3Hash state_root_from_execution = state_machine_->execute_block(block, base_fee_, proposer_address_);

        // Compute state root independently
        Blake3Hash computed_state_root = state_machine_->compute_state_root();

        // Verify consistency
        EXPECT_EQ(state_root_from_execution, computed_state_root)
            << "Iteration " << iteration << ": State root from execution should match computed state root";
    }
}

// Test 6: State root is independent of transaction order (for non-conflicting transactions)
TEST_F(StateRootConsistencyTest, StateRootIndependentOfOrderForNonConflicting) {
    // Create three accounts
    auto [addr1, priv1, pub1] = create_funded_account(10000);
    auto [addr2, priv2, pub2] = create_funded_account(10000);
    auto [addr3, priv3, pub3] = create_funded_account(10000);

    // Create non-conflicting transactions (different senders)
    Transaction tx1 = create_signed_transaction(addr1, addr3, 100, 0, 200, priv1);
    Transaction tx2 = create_signed_transaction(addr2, addr3, 200, 0, 200, priv2);

    // Execute in order 1, 2
    std::vector<Transaction> transactions1 = {tx1, tx2};
    Block block1 = create_block(1, transactions1);
    Blake3Hash state_root1 = state_machine_->execute_block(block1, base_fee_, proposer_address_);

    // Reset and execute in order 2, 1
    state_machine_->clear();
    state_machine_->get_account_manager().create_account(proposer_address_, 0);
    state_machine_->get_account_manager().create_account(addr1, 10000);
    state_machine_->get_account_manager().create_account(addr2, 10000);
    state_machine_->get_account_manager().create_account(addr3, 10000);

    std::vector<Transaction> transactions2 = {tx2, tx1};
    Block block2 = create_block(1, transactions2);
    Blake3Hash state_root2 = state_machine_->execute_block(block2, base_fee_, proposer_address_);

    // State roots should be the same (non-conflicting transactions)
    EXPECT_EQ(state_root1, state_root2)
        << "State root should be the same for non-conflicting transactions regardless of order";
}
