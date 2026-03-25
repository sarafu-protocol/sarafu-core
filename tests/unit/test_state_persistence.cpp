#include "sarafu/storage/database.h"
#include "sarafu/storage/state_storage.h"
#include "sarafu/storage/state_persistence.h"
#include "sarafu/state/account.h"
#include "sarafu/consensus/block.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <memory>
#include <chrono>

using namespace sarafu::storage;
using namespace sarafu::state;
using namespace sarafu::consensus;
using namespace sarafu::crypto;

class StatePersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary database directory
        auto unique_suffix = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        test_db_path_ = std::filesystem::temp_directory_path() /
                        ("sarafu_persistence_test_db_" + std::to_string(unique_suffix));
        std::filesystem::create_directories(test_db_path_);
        
        // Open database
        auto db_result = Database::open(test_db_path_.string());
        ASSERT_TRUE(db_result.is_ok());
        
        db_ = std::move(db_result.value());
        storage_ = std::make_shared<StateStorage>(db_);
        persistence_ = std::make_unique<StatePersistence>(storage_);
    }

    void TearDown() override {
        // Clean up database
        persistence_.reset();
        storage_.reset();
        db_.reset();
        std::error_code ec;
        std::filesystem::remove_all(test_db_path_, ec);
    }

    // Helper to create a test account
    Account create_test_account(uint8_t seed) {
        std::vector<uint8_t> addr_bytes(32, seed);
        Address addr(addr_bytes);
        return Account(addr, 1000000 + seed, seed);
    }

    // Helper to create a test block
    Block create_test_block(uint64_t height) {
        std::vector<uint8_t> hash_bytes(32, static_cast<uint8_t>(height));
        Blake3Hash hash(hash_bytes);
        
        BlockHeader header(
            height,
            1000000 + height,
            hash,
            hash,
            hash,
            hash,
            Address(hash_bytes),
            height / 10000
        );
        
        return Block(header, {}, QuorumCertificate());
    }

    std::filesystem::path test_db_path_;
    std::shared_ptr<Database> db_;
    std::shared_ptr<StateStorage> storage_;
    std::unique_ptr<StatePersistence> persistence_;
};

/**
 * Test: State doesn't exist initially
 */
TEST_F(StatePersistenceTest, StateDoesNotExistInitially) {
    EXPECT_FALSE(persistence_->state_exists());
}

/**
 * Test: Save and load state
 */
TEST_F(StatePersistenceTest, SaveAndLoadState) {
    // Create test accounts
    std::vector<Account> accounts;
    for (uint8_t i = 0; i < 10; ++i) {
        accounts.push_back(create_test_account(i));
    }

    // Save state
    uint64_t height = 12345;
    auto save_result = persistence_->save_state(height, accounts);
    ASSERT_TRUE(save_result.is_ok()) << save_result.error();

    // Verify state exists
    EXPECT_TRUE(persistence_->state_exists());

    // Load state
    auto load_result = persistence_->load_state();
    ASSERT_TRUE(load_result.is_ok()) << load_result.error();
    EXPECT_EQ(load_result.value(), height);

    // Verify accounts were saved
    for (const auto& account : accounts) {
        auto get_result = storage_->get_account(account.address);
        ASSERT_TRUE(get_result.is_ok());
        EXPECT_EQ(get_result.value(), account);
    }
}

/**
 * Test: Get last height
 */
TEST_F(StatePersistenceTest, GetLastHeight) {
    // Initially no height
    auto result = persistence_->get_last_height();
    EXPECT_TRUE(result.is_error());

    // Save state
    uint64_t height = 54321;
    std::vector<Account> accounts;
    persistence_->save_state(height, accounts);

    // Get last height
    result = persistence_->get_last_height();
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), height);
}

/**
 * Test: Update state (save multiple times)
 */
TEST_F(StatePersistenceTest, UpdateState) {
    std::vector<Account> accounts;
    accounts.push_back(create_test_account(1));

    // Save initial state
    persistence_->save_state(100, accounts);
    
    auto result = persistence_->get_last_height();
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 100);

    // Update state
    accounts[0].balance = 2000000;
    accounts[0].nonce = 5;
    persistence_->save_state(200, accounts);

    // Verify updated height
    result = persistence_->get_last_height();
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 200);

    // Verify updated account
    auto account_result = storage_->get_account(accounts[0].address);
    ASSERT_TRUE(account_result.is_ok());
    EXPECT_EQ(account_result.value().balance, 2000000);
    EXPECT_EQ(account_result.value().nonce, 5);
}

/**
 * Test: Prune old state - no pruning when height is low
 */
TEST_F(StatePersistenceTest, PruneOldStateNoPruningWhenHeightLow) {
    // Create and store some blocks
    for (uint64_t i = 0; i < 100; ++i) {
        Block block = create_test_block(i);
        storage_->store_block(block);
    }

    // Try to prune at height 100 (below weak subjectivity period)
    auto result = persistence_->prune_old_state(100);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 0); // No blocks pruned

    // Verify blocks still exist
    for (uint64_t i = 0; i < 100; ++i) {
        EXPECT_TRUE(storage_->block_exists(i));
    }
}

/**
 * Test: Prune old state - prunes blocks beyond weak subjectivity period
 */
TEST_F(StatePersistenceTest, PruneOldStatePrunesOldBlocks) {
    uint64_t ws_period = StatePersistence::weak_subjectivity_period();
    uint64_t current_height = ws_period + 1000;

    // Create and store blocks
    // Store some old blocks (should be pruned)
    for (uint64_t i = 0; i < 500; ++i) {
        Block block = create_test_block(i);
        storage_->store_block(block);
    }

    // Store some recent blocks (should be kept)
    for (uint64_t i = current_height - 100; i < current_height; ++i) {
        Block block = create_test_block(i);
        storage_->store_block(block);
    }

    // Prune old state
    auto result = persistence_->prune_old_state(current_height);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 500); // 500 old blocks pruned

    // Verify old blocks are gone
    for (uint64_t i = 0; i < 500; ++i) {
        EXPECT_FALSE(storage_->block_exists(i));
    }

    // Verify recent blocks still exist
    for (uint64_t i = current_height - 100; i < current_height; ++i) {
        EXPECT_TRUE(storage_->block_exists(i));
    }
}

/**
 * Test: Prune old state - handles missing blocks gracefully
 */
TEST_F(StatePersistenceTest, PruneOldStateHandlesMissingBlocks) {
    uint64_t ws_period = StatePersistence::weak_subjectivity_period();
    uint64_t current_height = ws_period + 1000;

    // Store only some blocks (sparse)
    for (uint64_t i = 0; i < 500; i += 10) {
        Block block = create_test_block(i);
        storage_->store_block(block);
    }

    // Prune should handle missing blocks gracefully
    auto result = persistence_->prune_old_state(current_height);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 50); // 50 blocks pruned (every 10th block)

    // Verify pruned blocks are gone
    for (uint64_t i = 0; i < 500; i += 10) {
        EXPECT_FALSE(storage_->block_exists(i));
    }
}

/**
 * Test: Prune old state - multiple pruning operations
 */
TEST_F(StatePersistenceTest, PruneOldStateMultiplePruningOperations) {
    uint64_t ws_period = StatePersistence::weak_subjectivity_period();
    
    // First batch of blocks
    for (uint64_t i = 0; i < 1000; ++i) {
        Block block = create_test_block(i);
        storage_->store_block(block);
    }

    // First pruning at height ws_period + 500
    uint64_t height1 = ws_period + 500;
    auto result1 = persistence_->prune_old_state(height1);
    ASSERT_TRUE(result1.is_ok());
    EXPECT_EQ(result1.value(), 500); // Prune blocks 0-499

    // Add more blocks
    for (uint64_t i = 1000; i < 2000; ++i) {
        Block block = create_test_block(i);
        storage_->store_block(block);
    }

    // Second pruning at height ws_period + 1500
    uint64_t height2 = ws_period + 1500;
    auto result2 = persistence_->prune_old_state(height2);
    ASSERT_TRUE(result2.is_ok());
    EXPECT_EQ(result2.value(), 1000); // Prune blocks 500-1499

    // Verify correct blocks are pruned
    for (uint64_t i = 0; i < 1500; ++i) {
        EXPECT_FALSE(storage_->block_exists(i));
    }
    for (uint64_t i = 1500; i < 2000; ++i) {
        EXPECT_TRUE(storage_->block_exists(i));
    }
}

/**
 * Test: Weak subjectivity period constant
 */
TEST_F(StatePersistenceTest, WeakSubjectivityPeriodConstant) {
    EXPECT_EQ(StatePersistence::weak_subjectivity_period(), 1000000);
}

/**
 * Test: Save state with empty accounts
 */
TEST_F(StatePersistenceTest, SaveStateWithEmptyAccounts) {
    std::vector<Account> empty_accounts;
    
    auto result = persistence_->save_state(100, empty_accounts);
    ASSERT_TRUE(result.is_ok());

    // Verify height was saved
    auto height_result = persistence_->get_last_height();
    ASSERT_TRUE(height_result.is_ok());
    EXPECT_EQ(height_result.value(), 100);
}

/**
 * Test: Save state with many accounts
 */
TEST_F(StatePersistenceTest, SaveStateWithManyAccounts) {
    // Create 1000 accounts
    std::vector<Account> accounts;
    for (uint16_t i = 0; i < 1000; ++i) {
        accounts.push_back(create_test_account(static_cast<uint8_t>(i % 256)));
    }

    // Save state
    auto result = persistence_->save_state(5000, accounts);
    ASSERT_TRUE(result.is_ok());

    // Verify all accounts were saved
    for (const auto& account : accounts) {
        auto get_result = storage_->get_account(account.address);
        ASSERT_TRUE(get_result.is_ok());
    }
}

/**
 * Test: Load state when no state exists
 */
TEST_F(StatePersistenceTest, LoadStateWhenNoStateExists) {
    auto result = persistence_->load_state();
    EXPECT_TRUE(result.is_error());
    EXPECT_FALSE(persistence_->state_exists());
}

/**
 * Test: Prune deletes transaction receipts
 */
TEST_F(StatePersistenceTest, PruneDeletesTransactionReceipts) {
    uint64_t ws_period = StatePersistence::weak_subjectivity_period();
    uint64_t current_height = ws_period + 100;

    // Create block with transactions
    Block block = create_test_block(50);
    
    // Add some transactions
    for (int i = 0; i < 5; ++i) {
        Transaction tx;
        tx.from = create_test_account(i).address;
        tx.to = create_test_account(i + 1).address;
        tx.amount = 1000;
        tx.nonce = i;
        block.transactions.push_back(tx);
    }

    // Store block
    storage_->store_block(block);

    // Store receipts for transactions
    std::vector<Blake3Hash> tx_hashes;
    for (const auto& tx : block.transactions) {
        Blake3Hash tx_hash = tx.hash();
        tx_hashes.push_back(tx_hash);
        
        TransactionReceipt receipt(tx_hash, block.header.height, true, 21000, "");
        storage_->store_transaction_receipt(receipt);
    }

    // Verify receipts exist
    for (const auto& tx_hash : tx_hashes) {
        EXPECT_TRUE(storage_->receipt_exists(tx_hash));
    }

    // Prune old state
    auto result = persistence_->prune_old_state(current_height);
    ASSERT_TRUE(result.is_ok());

    // Verify receipts are deleted
    for (const auto& tx_hash : tx_hashes) {
        EXPECT_FALSE(storage_->receipt_exists(tx_hash));
    }
}
