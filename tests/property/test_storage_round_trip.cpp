#include "sarafu/storage/database.h"
#include "sarafu/storage/state_storage.h"
#include "sarafu/state/account.h"
#include "sarafu/state/transaction.h"
#include "sarafu/consensus/block.h"
#include "sarafu/crypto/blake3_hash.h"
#include <gtest/gtest.h>
#include <random>
#include <filesystem>
#include <memory>

using namespace sarafu::storage;
using namespace sarafu::state;
using namespace sarafu::consensus;
using namespace sarafu::crypto;

/**
 * Property-Based Tests for Storage Round-Trip
 * 
 * **Validates: Requirements 18.1, 18.2, 18.3, 18.4**
 * 
 * Property 65: Account State Persistence
 * For any account state S, storing S to the database and then retrieving it
 * produces an equivalent state S'.
 * 
 * Property 66: Block Storage Round-Trip
 * For any block B, storing B to the database and then retrieving it by height
 * or hash produces an equivalent block B'.
 * 
 * Property 67: Transaction Receipt Persistence
 * For any transaction receipt R, storing R and retrieving it by transaction hash
 * produces an equivalent receipt R'.
 */
class StorageRoundTripPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        
        // Create temporary database directory
        test_db_path_ = std::filesystem::temp_directory_path() / "sarafu_test_db";
        std::filesystem::create_directories(test_db_path_);
        
        // Open database
        auto db_result = Database::open(test_db_path_.string());
        ASSERT_TRUE(db_result.is_ok()) << "Failed to open database: " << db_result.error();
        
        db_ = std::move(db_result.value());
        storage_ = std::make_unique<StateStorage>(db_);
    }

    void TearDown() override {
        // Clean up database
        storage_.reset();
        db_.reset();
        std::filesystem::remove_all(test_db_path_);
    }

    // Generate random bytes
    std::vector<uint8_t> generate_random_bytes(size_t size) {
        std::vector<uint8_t> data(size);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return data;
    }

    // Generate random address
    Address generate_random_address() {
        return Address(generate_random_bytes(32));
    }

    // Generate random hash
    Blake3Hash generate_random_hash() {
        return Blake3Hash(generate_random_bytes(32));
    }

    // Generate random account
    Account generate_random_account() {
        Address addr = generate_random_address();
        std::uniform_int_distribution<uint64_t> balance_dist(0, 1000000000000ULL);
        std::uniform_int_distribution<uint64_t> nonce_dist(0, 1000000);
        
        uint64_t balance = balance_dist(rng_);
        uint64_t nonce = nonce_dist(rng_);
        Blake3Hash code_hash = generate_random_hash();
        
        return Account(addr, balance, nonce, code_hash);
    }

    // Generate random transaction
    Transaction generate_random_transaction() {
        Address from = generate_random_address();
        Address to = generate_random_address();
        
        std::uniform_int_distribution<uint64_t> amount_dist(1, 1000000);
        std::uniform_int_distribution<uint64_t> nonce_dist(0, 1000);
        std::uniform_int_distribution<uint64_t> fee_dist(100, 10000);
        std::uniform_int_distribution<uint64_t> gas_dist(21000, 1000000);
        std::uniform_int_distribution<uint32_t> chain_dist(1, 1000);
        
        Transaction tx(
            from,
            to,
            amount_dist(rng_),
            nonce_dist(rng_),
            fee_dist(rng_),
            gas_dist(rng_),
            chain_dist(rng_)
        );
        
        // Set a dummy signature (96 bytes)
        tx.signature = Ed25519_Signature(generate_random_bytes(64));
        
        return tx;
    }

    // Generate random block header
    BlockHeader generate_random_block_header() {
        std::uniform_int_distribution<uint64_t> height_dist(0, 1000000);
        std::uniform_int_distribution<uint64_t> timestamp_dist(1000000, 2000000);
        std::uniform_int_distribution<uint64_t> epoch_dist(0, 100);
        
        return BlockHeader(
            height_dist(rng_),
            timestamp_dist(rng_),
            generate_random_hash(),
            generate_random_hash(),
            generate_random_hash(),
            generate_random_hash(),
            generate_random_address(),
            epoch_dist(rng_)
        );
    }

    // Generate random quorum certificate
    QuorumCertificate generate_random_qc() {
        std::uniform_int_distribution<uint64_t> height_dist(0, 1000000);
        std::uniform_int_distribution<uint64_t> view_dist(0, 1000);
        std::uniform_int_distribution<uint64_t> stake_dist(1000000, 10000000);
        std::uniform_int_distribution<int> signer_count_dist(1, 10);
        
        int num_signers = signer_count_dist(rng_);
        std::vector<ValidatorID> signers;
        for (int i = 0; i < num_signers; ++i) {
            signers.push_back(generate_random_address());
        }
        
        // Create dummy BLS signature (96 bytes)
        BLS12_381_Signature sig(generate_random_bytes(96));
        
        return QuorumCertificate(
            height_dist(rng_),
            generate_random_hash(),
            view_dist(rng_),
            sig,
            signers,
            stake_dist(rng_)
        );
    }

    // Generate random block
    Block generate_random_block() {
        BlockHeader header = generate_random_block_header();
        
        std::uniform_int_distribution<int> tx_count_dist(0, 10);
        int num_txs = tx_count_dist(rng_);
        
        std::vector<Transaction> transactions;
        for (int i = 0; i < num_txs; ++i) {
            transactions.push_back(generate_random_transaction());
        }
        
        QuorumCertificate qc = generate_random_qc();
        
        return Block(header, transactions, qc);
    }

    // Generate random transaction receipt
    TransactionReceipt generate_random_receipt() {
        Blake3Hash tx_hash = generate_random_hash();
        
        std::uniform_int_distribution<uint64_t> height_dist(0, 1000000);
        std::uniform_int_distribution<uint64_t> gas_dist(21000, 1000000);
        std::uniform_int_distribution<int> success_dist(0, 1);
        
        bool success = success_dist(rng_) == 1;
        std::string error_msg = success ? "" : "Execution failed";
        
        return TransactionReceipt(
            tx_hash,
            height_dist(rng_),
            success,
            gas_dist(rng_),
            error_msg
        );
    }

    std::mt19937 rng_;
    std::filesystem::path test_db_path_;
    std::shared_ptr<Database> db_;
    std::unique_ptr<StateStorage> storage_;
};

/**
 * Property 65: Account State Persistence
 * 
 * For any account state S, storing S to the database and then retrieving it
 * produces an equivalent state S'.
 */
TEST_F(StorageRoundTripPropertyTest, AccountStatePersistence) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random account
        Account original = generate_random_account();
        
        // Store account
        auto store_result = storage_->store_account(original);
        ASSERT_TRUE(store_result.is_ok())
            << "Failed to store account on trial " << trial
            << ": " << store_result.error();
        
        // Retrieve account
        auto get_result = storage_->get_account(original.address);
        ASSERT_TRUE(get_result.is_ok())
            << "Failed to retrieve account on trial " << trial
            << ": " << get_result.error();
        
        Account retrieved = get_result.value();
        
        // Verify equivalence
        ASSERT_EQ(retrieved.address, original.address)
            << "Address mismatch on trial " << trial;
        ASSERT_EQ(retrieved.balance, original.balance)
            << "Balance mismatch on trial " << trial;
        ASSERT_EQ(retrieved.nonce, original.nonce)
            << "Nonce mismatch on trial " << trial;
        ASSERT_EQ(retrieved.code_hash, original.code_hash)
            << "Code hash mismatch on trial " << trial;
        
        // Verify using operator==
        ASSERT_EQ(retrieved, original)
            << "Account not equal after round-trip on trial " << trial;
    }
}

/**
 * Property 65 Extension: Multiple accounts persist independently
 * 
 * Storing multiple accounts and retrieving them produces the correct
 * account for each address.
 */
TEST_F(StorageRoundTripPropertyTest, MultipleAccountsPersistIndependently) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate multiple accounts
        std::uniform_int_distribution<int> count_dist(2, 20);
        int num_accounts = count_dist(rng_);
        
        std::vector<Account> accounts;
        for (int i = 0; i < num_accounts; ++i) {
            accounts.push_back(generate_random_account());
        }
        
        // Store all accounts
        for (const auto& account : accounts) {
            auto result = storage_->store_account(account);
            ASSERT_TRUE(result.is_ok())
                << "Failed to store account on trial " << trial;
        }
        
        // Retrieve and verify each account
        for (const auto& original : accounts) {
            auto result = storage_->get_account(original.address);
            ASSERT_TRUE(result.is_ok())
                << "Failed to retrieve account on trial " << trial;
            
            Account retrieved = result.value();
            ASSERT_EQ(retrieved, original)
                << "Account mismatch on trial " << trial;
        }
    }
}

/**
 * Property 65 Extension: Account updates persist correctly
 * 
 * Updating an account and retrieving it produces the updated state.
 */
TEST_F(StorageRoundTripPropertyTest, AccountUpdatesPersist) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate and store initial account
        Account original = generate_random_account();
        auto store_result = storage_->store_account(original);
        ASSERT_TRUE(store_result.is_ok());
        
        // Generate random number of updates
        std::uniform_int_distribution<int> update_dist(1, 10);
        int num_updates = update_dist(rng_);
        
        Account current = original;
        for (int i = 0; i < num_updates; ++i) {
            // Modify account
            std::uniform_int_distribution<uint64_t> balance_dist(0, 1000000000);
            current.balance = balance_dist(rng_);
            current.nonce++;
            
            // Store updated account
            auto update_result = storage_->store_account(current);
            ASSERT_TRUE(update_result.is_ok());
            
            // Retrieve and verify
            auto get_result = storage_->get_account(current.address);
            ASSERT_TRUE(get_result.is_ok());
            
            Account retrieved = get_result.value();
            ASSERT_EQ(retrieved, current)
                << "Account update " << i << " not persisted on trial " << trial;
        }
    }
}

/**
 * Property 66: Block Storage Round-Trip
 * 
 * For any block B, storing B to the database and then retrieving it by height
 * or hash produces an equivalent block B'.
 */
TEST_F(StorageRoundTripPropertyTest, BlockStorageRoundTrip) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random block
        Block original = generate_random_block();
        
        // Store block
        auto store_result = storage_->store_block(original);
        ASSERT_TRUE(store_result.is_ok())
            << "Failed to store block on trial " << trial
            << ": " << store_result.error();
        
        // Retrieve by height
        auto get_by_height_result = storage_->get_block_by_height(original.header.height);
        ASSERT_TRUE(get_by_height_result.is_ok())
            << "Failed to retrieve block by height on trial " << trial
            << ": " << get_by_height_result.error();
        
        Block retrieved_by_height = get_by_height_result.value();
        
        // Verify equivalence
        ASSERT_EQ(retrieved_by_height, original)
            << "Block not equal after round-trip by height on trial " << trial;
        
        // Retrieve by hash
        Blake3Hash block_hash = original.hash();
        auto get_by_hash_result = storage_->get_block_by_hash(block_hash);
        ASSERT_TRUE(get_by_hash_result.is_ok())
            << "Failed to retrieve block by hash on trial " << trial
            << ": " << get_by_hash_result.error();
        
        Block retrieved_by_hash = get_by_hash_result.value();
        
        // Verify equivalence
        ASSERT_EQ(retrieved_by_hash, original)
            << "Block not equal after round-trip by hash on trial " << trial;
        
        // Verify both retrieval methods return the same block
        ASSERT_EQ(retrieved_by_height, retrieved_by_hash)
            << "Block retrieved by height differs from block retrieved by hash on trial " << trial;
    }
}

/**
 * Property 66 Extension: Block height index works correctly
 * 
 * For any block B with hash H and height N, get_block_height(H) returns N.
 */
TEST_F(StorageRoundTripPropertyTest, BlockHeightIndexWorks) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate and store block
        Block block = generate_random_block();
        auto store_result = storage_->store_block(block);
        ASSERT_TRUE(store_result.is_ok());
        
        // Get height from hash
        Blake3Hash block_hash = block.hash();
        auto height_result = storage_->get_block_height(block_hash);
        ASSERT_TRUE(height_result.is_ok())
            << "Failed to get block height on trial " << trial;
        
        // Verify height matches
        ASSERT_EQ(height_result.value(), block.header.height)
            << "Block height mismatch on trial " << trial;
    }
}

/**
 * Property 66 Extension: Multiple blocks persist independently
 * 
 * Storing multiple blocks at different heights allows retrieving each
 * block correctly.
 */
TEST_F(StorageRoundTripPropertyTest, MultipleBlocksPersistIndependently) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate multiple blocks with unique heights
        std::uniform_int_distribution<int> count_dist(2, 20);
        int num_blocks = count_dist(rng_);
        
        std::vector<Block> blocks;
        std::set<uint64_t> used_heights;
        
        for (int i = 0; i < num_blocks; ++i) {
            Block block = generate_random_block();
            
            // Ensure unique height
            while (used_heights.count(block.header.height) > 0) {
                block.header.height++;
            }
            used_heights.insert(block.header.height);
            
            blocks.push_back(block);
        }
        
        // Store all blocks
        for (const auto& block : blocks) {
            auto result = storage_->store_block(block);
            ASSERT_TRUE(result.is_ok())
                << "Failed to store block on trial " << trial;
        }
        
        // Retrieve and verify each block
        for (const auto& original : blocks) {
            auto result = storage_->get_block_by_height(original.header.height);
            ASSERT_TRUE(result.is_ok())
                << "Failed to retrieve block on trial " << trial;
            
            Block retrieved = result.value();
            ASSERT_EQ(retrieved, original)
                << "Block mismatch on trial " << trial;
        }
    }
}

/**
 * Property 67: Transaction Receipt Persistence
 * 
 * For any transaction receipt R, storing R and retrieving it by transaction hash
 * produces an equivalent receipt R'.
 */
TEST_F(StorageRoundTripPropertyTest, TransactionReceiptPersistence) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random receipt
        TransactionReceipt original = generate_random_receipt();
        
        // Store receipt
        auto store_result = storage_->store_transaction_receipt(original);
        ASSERT_TRUE(store_result.is_ok())
            << "Failed to store receipt on trial " << trial
            << ": " << store_result.error();
        
        // Retrieve receipt
        auto get_result = storage_->get_transaction_receipt(original.tx_hash);
        ASSERT_TRUE(get_result.is_ok())
            << "Failed to retrieve receipt on trial " << trial
            << ": " << get_result.error();
        
        TransactionReceipt retrieved = get_result.value();
        
        // Verify equivalence
        ASSERT_EQ(retrieved.tx_hash, original.tx_hash)
            << "Transaction hash mismatch on trial " << trial;
        ASSERT_EQ(retrieved.block_height, original.block_height)
            << "Block height mismatch on trial " << trial;
        ASSERT_EQ(retrieved.success, original.success)
            << "Success flag mismatch on trial " << trial;
        ASSERT_EQ(retrieved.gas_used, original.gas_used)
            << "Gas used mismatch on trial " << trial;
        ASSERT_EQ(retrieved.error_message, original.error_message)
            << "Error message mismatch on trial " << trial;
        
        // Verify using operator==
        ASSERT_EQ(retrieved, original)
            << "Receipt not equal after round-trip on trial " << trial;
    }
}

/**
 * Property 67 Extension: Multiple receipts persist independently
 * 
 * Storing multiple transaction receipts allows retrieving each receipt
 * by its transaction hash.
 */
TEST_F(StorageRoundTripPropertyTest, MultipleReceiptsPersistIndependently) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate multiple receipts
        std::uniform_int_distribution<int> count_dist(2, 20);
        int num_receipts = count_dist(rng_);
        
        std::vector<TransactionReceipt> receipts;
        for (int i = 0; i < num_receipts; ++i) {
            receipts.push_back(generate_random_receipt());
        }
        
        // Store all receipts
        for (const auto& receipt : receipts) {
            auto result = storage_->store_transaction_receipt(receipt);
            ASSERT_TRUE(result.is_ok())
                << "Failed to store receipt on trial " << trial;
        }
        
        // Retrieve and verify each receipt
        for (const auto& original : receipts) {
            auto result = storage_->get_transaction_receipt(original.tx_hash);
            ASSERT_TRUE(result.is_ok())
                << "Failed to retrieve receipt on trial " << trial;
            
            TransactionReceipt retrieved = result.value();
            ASSERT_EQ(retrieved, original)
                << "Receipt mismatch on trial " << trial;
        }
    }
}

/**
 * Property: Batch operations are atomic
 * 
 * Storing a block with receipts atomically ensures all data is stored together.
 */
TEST_F(StorageRoundTripPropertyTest, BatchOperationsAreAtomic) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate block and receipts
        Block block = generate_random_block();
        
        std::vector<TransactionReceipt> receipts;
        for (const auto& tx : block.transactions) {
            TransactionReceipt receipt(
                tx.hash(),
                block.header.height,
                true,
                21000,
                ""
            );
            receipts.push_back(receipt);
        }
        
        // Store block with receipts atomically
        auto store_result = storage_->store_block_with_receipts(block, receipts);
        ASSERT_TRUE(store_result.is_ok())
            << "Failed to store block with receipts on trial " << trial;
        
        // Verify block was stored
        auto block_result = storage_->get_block_by_height(block.header.height);
        ASSERT_TRUE(block_result.is_ok());
        ASSERT_EQ(block_result.value(), block);
        
        // Verify all receipts were stored
        for (const auto& original_receipt : receipts) {
            auto receipt_result = storage_->get_transaction_receipt(original_receipt.tx_hash);
            ASSERT_TRUE(receipt_result.is_ok())
                << "Receipt not found after batch store on trial " << trial;
            ASSERT_EQ(receipt_result.value(), original_receipt);
        }
    }
}

/**
 * Property: Existence checks work correctly
 * 
 * After storing data, existence checks return true. Before storing, they return false.
 */
TEST_F(StorageRoundTripPropertyTest, ExistenceChecksWork) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate data
        Account account = generate_random_account();
        Block block = generate_random_block();
        TransactionReceipt receipt = generate_random_receipt();
        
        // Verify data doesn't exist initially
        ASSERT_FALSE(storage_->account_exists(account.address))
            << "Account exists before storing on trial " << trial;
        ASSERT_FALSE(storage_->block_exists(block.header.height))
            << "Block exists before storing on trial " << trial;
        ASSERT_FALSE(storage_->receipt_exists(receipt.tx_hash))
            << "Receipt exists before storing on trial " << trial;
        
        // Store data
        storage_->store_account(account);
        storage_->store_block(block);
        storage_->store_transaction_receipt(receipt);
        
        // Verify data exists after storing
        ASSERT_TRUE(storage_->account_exists(account.address))
            << "Account doesn't exist after storing on trial " << trial;
        ASSERT_TRUE(storage_->block_exists(block.header.height))
            << "Block doesn't exist after storing on trial " << trial;
        ASSERT_TRUE(storage_->receipt_exists(receipt.tx_hash))
            << "Receipt doesn't exist after storing on trial " << trial;
    }
}

/**
 * Property: Metadata storage works correctly
 * 
 * Storing and retrieving metadata produces the original value.
 */
TEST_F(StorageRoundTripPropertyTest, MetadataStorageWorks) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random metadata
        std::string key = "test_key_" + std::to_string(trial);
        std::vector<uint8_t> value = generate_random_bytes(32);
        
        // Store metadata
        auto store_result = storage_->store_metadata(key, value);
        ASSERT_TRUE(store_result.is_ok())
            << "Failed to store metadata on trial " << trial;
        
        // Retrieve metadata
        auto get_result = storage_->get_metadata(key);
        ASSERT_TRUE(get_result.is_ok())
            << "Failed to retrieve metadata on trial " << trial;
        
        // Verify equivalence
        ASSERT_EQ(get_result.value(), value)
            << "Metadata mismatch on trial " << trial;
    }
}
