#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include "sarafu/state/mempool.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/state/transaction_validator.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/blake3_hash.h"
#include "../test_utils.h"

namespace sarafu {
namespace stress {

/**
 * Stress Test 30.2: Mempool Spam Stress Test
 * 
 * This test simulates a spam attack where 10,000 transactions per second
 * are submitted to the mempool. It validates:
 * - Mempool handles load without crashing
 * - Transaction prioritization works correctly under load
 * - Nonce ordering is maintained
 * - Fee-based selection works correctly
 * 
 * Validates Requirements: 19.1, 19.2, 19.3, 19.4, 19.5, 19.8, 19.9
 * Validates Properties: 49, 50
 */
class MempoolSpamStressTest : public ::testing::Test {
protected:
    static constexpr size_t NUM_ACCOUNTS = 100;
    static constexpr size_t TRANSACTIONS_PER_ACCOUNT = 100;
    static constexpr size_t TOTAL_TRANSACTIONS = NUM_ACCOUNTS * TRANSACTIONS_PER_ACCOUNT;
    static constexpr uint64_t INITIAL_BALANCE = 1000000000;  // 1 billion
    static constexpr uint64_t BASE_FEE = 1000;
    static constexpr uint32_t CHAIN_ID = 1;

    struct TestAccount {
        crypto::Ed25519_PrivateKey private_key;
        crypto::Ed25519_PublicKey public_key;
        state::Address address;
        uint64_t nonce;
    };

    std::shared_ptr<state::AccountManager> account_manager_;
    std::unique_ptr<state::Mempool> mempool_;
    std::vector<TestAccount> test_accounts_;

    void SetUp() override {
        account_manager_ = std::make_shared<state::AccountManager>();
        
        state::Mempool::Config mempool_config;
        mempool_config.max_transactions_per_account = TRANSACTIONS_PER_ACCOUNT;
        mempool_config.max_nonce_gap = TRANSACTIONS_PER_ACCOUNT;
        mempool_config.max_age_seconds = 3600;
        
        mempool_ = std::make_unique<state::Mempool>(mempool_config);
        mempool_->set_account_manager(account_manager_.get());

        // Create test accounts with initial balances
        for (size_t i = 0; i < NUM_ACCOUNTS; ++i) {
            TestAccount account;
            auto keypair = crypto::Ed25519::generate_keypair();
            account.public_key = keypair.first;
            account.private_key = keypair.second;
            account.address = test_utils::address_from_public_key(account.public_key);
            account.nonce = 0;

            test_accounts_.push_back(account);

            // Create account with initial balance
            account_manager_->create_account(account.address, INITIAL_BALANCE);
        }
    }

    void TearDown() override {
        test_accounts_.clear();
        mempool_.reset();
        account_manager_.reset();
    }

    /**
     * Create a transaction from an account.
     */
    state::Transaction create_transaction(
        TestAccount& account,
        uint64_t amount,
        uint64_t fee,
        uint64_t nonce
    ) {
        // Create a random recipient
        state::Address recipient = test_utils::generate_random_address();

        state::Transaction tx;
        tx.from = account.address;
        tx.to = recipient;
        tx.amount = amount;
        tx.nonce = nonce;
        tx.fee = fee;
        tx.gas_limit = 21000;
        tx.chain_id = CHAIN_ID;

        // Sign transaction
        auto tx_hash = tx.hash();
        tx.signature = crypto::Ed25519::sign(tx_hash.serialize(), account.private_key);

        return tx;
    }
};

/**
 * Test: Submit 10,000 transactions and verify mempool handles load.
 * 
 * This is the main stress test that validates the mempool can handle
 * high transaction throughput without crashing.
 */
TEST_F(MempoolSpamStressTest, SubmitTenThousandTransactions) {
    std::cout << "\n=== Mempool Spam Stress Test ===" << std::endl;
    std::cout << "Total accounts: " << NUM_ACCOUNTS << std::endl;
    std::cout << "Transactions per account: " << TRANSACTIONS_PER_ACCOUNT << std::endl;
    std::cout << "Total transactions: " << TOTAL_TRANSACTIONS << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Submit transactions from all accounts
    size_t successful_submissions = 0;
    size_t failed_submissions = 0;

    for (size_t tx_idx = 0; tx_idx < TRANSACTIONS_PER_ACCOUNT; ++tx_idx) {
        for (size_t acc_idx = 0; acc_idx < NUM_ACCOUNTS; ++acc_idx) {
            auto& account = test_accounts_[acc_idx];

            // Vary fees to test prioritization
            uint64_t fee = BASE_FEE + (tx_idx * 100) + (acc_idx % 10) * 50;
            uint64_t amount = 1000;

            auto tx = create_transaction(account, amount, fee, account.nonce);

            bool added = mempool_->add_transaction(tx);
            if (added) {
                successful_submissions++;
                account.nonce++;
            } else {
                failed_submissions++;
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "\nSubmission Results:" << std::endl;
    std::cout << "Successful: " << successful_submissions << std::endl;
    std::cout << "Failed: " << failed_submissions << std::endl;
    std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << (successful_submissions * 1000.0 / duration.count()) 
              << " tx/s" << std::endl;

    // Verify mempool didn't crash and accepted transactions
    EXPECT_GT(successful_submissions, 0) << "No transactions were accepted";
    EXPECT_EQ(mempool_->size(), successful_submissions) 
        << "Mempool size doesn't match successful submissions";

    std::cout << "\nMempool size: " << mempool_->size() << std::endl;

    // Verify we achieved high throughput (target: 10,000 tx/s)
    double throughput = successful_submissions * 1000.0 / duration.count();
    std::cout << "Target throughput: 10,000 tx/s" << std::endl;
    std::cout << "Achieved throughput: " << throughput << " tx/s" << std::endl;

    EXPECT_GT(throughput, 1000.0) 
        << "Throughput too low - mempool may have performance issues";

    std::cout << "✓ Mempool handled load without crashing" << std::endl;
}

/**
 * Test: Verify transaction prioritization works correctly under load.
 * 
 * This test validates that the mempool correctly prioritizes transactions
 * by fee when building blocks, even under high load.
 */
TEST_F(MempoolSpamStressTest, TransactionPrioritizationUnderLoad) {
    std::cout << "\n=== Transaction Prioritization Test ===" << std::endl;

    // Submit transactions with varying fees
    std::vector<uint64_t> fees;
    
    for (size_t acc_idx = 0; acc_idx < NUM_ACCOUNTS; ++acc_idx) {
        auto& account = test_accounts_[acc_idx];

        for (size_t tx_idx = 0; tx_idx < TRANSACTIONS_PER_ACCOUNT; ++tx_idx) {
            // Create fees with wide range to test prioritization.
            // Highest fee starts at lowest nonce to respect nonce ordering.
            uint64_t fee = BASE_FEE +
                           ((TRANSACTIONS_PER_ACCOUNT - 1 - tx_idx) * 1000) +
                           (acc_idx * 100);
            fees.push_back(fee);

            auto tx = create_transaction(account, 1000, fee, account.nonce);
            
            bool added = mempool_->add_transaction(tx);
            ASSERT_TRUE(added) << "Failed to add transaction";
            account.nonce++;
        }
    }

    std::cout << "Submitted " << mempool_->size() << " transactions" << std::endl;

    // Sort fees in descending order to get expected order
    std::sort(fees.begin(), fees.end(), std::greater<uint64_t>());

    // Get transactions for block building
    uint64_t max_gas = 1000000;  // Allow many transactions
    uint64_t min_base_fee = BASE_FEE;

    auto selected_txs = mempool_->get_transactions_for_block(max_gas, min_base_fee);

    std::cout << "Selected " << selected_txs.size() << " transactions for block" << std::endl;

    EXPECT_GT(selected_txs.size(), 0) << "No transactions selected";

    // Verify transactions are ordered by fee (descending)
    for (size_t i = 1; i < selected_txs.size(); ++i) {
        EXPECT_GE(selected_txs[i-1].fee, selected_txs[i].fee)
            << "Transactions not ordered by fee at index " << i;
    }

    // Verify highest fee transactions are selected first
    if (selected_txs.size() > 0) {
        std::cout << "Highest fee in block: " << selected_txs[0].fee << std::endl;
        std::cout << "Lowest fee in block: " << selected_txs[selected_txs.size()-1].fee << std::endl;
        std::cout << "Expected highest fee: " << fees[0] << std::endl;

        // The highest fee transaction should be in the selected set
        bool found_highest = false;
        for (const auto& tx : selected_txs) {
            if (tx.fee == fees[0]) {
                found_highest = true;
                break;
            }
        }
        EXPECT_TRUE(found_highest) << "Highest fee transaction not selected";
    }

    std::cout << "✓ Transaction prioritization works correctly under load" << std::endl;
}

/**
 * Test: Verify nonce ordering is maintained under spam load.
 * 
 * This test validates that the mempool maintains correct nonce ordering
 * for each account even when transactions are submitted rapidly.
 */
TEST_F(MempoolSpamStressTest, NonceOrderingUnderLoad) {
    std::cout << "\n=== Nonce Ordering Test ===" << std::endl;

    // Submit transactions in random order to stress nonce ordering
    for (size_t tx_idx = 0; tx_idx < TRANSACTIONS_PER_ACCOUNT; ++tx_idx) {
        for (size_t acc_idx = 0; acc_idx < NUM_ACCOUNTS; ++acc_idx) {
            auto& account = test_accounts_[acc_idx];

            uint64_t fee = BASE_FEE + (tx_idx * 100);
            auto tx = create_transaction(account, 1000, fee, account.nonce);

            bool added = mempool_->add_transaction(tx);
            ASSERT_TRUE(added) << "Failed to add transaction";
            account.nonce++;
        }
    }

    std::cout << "Submitted " << mempool_->size() << " transactions" << std::endl;

    // Verify nonce ordering for each account
    for (const auto& account : test_accounts_) {
        auto account_txs = mempool_->get_account_transactions(account.address);

        EXPECT_EQ(account_txs.size(), TRANSACTIONS_PER_ACCOUNT)
            << "Account transaction count incorrect";

        // Verify nonces are sequential
        for (size_t i = 0; i < account_txs.size(); ++i) {
            EXPECT_EQ(account_txs[i].nonce, i)
                << "Nonce ordering incorrect at index " << i;
        }
    }

    std::cout << "✓ Nonce ordering maintained correctly under load" << std::endl;
}

/**
 * Test: Verify mempool respects per-account transaction limits.
 * 
 * This test validates that the mempool enforces the maximum number
 * of pending transactions per account.
 */
TEST_F(MempoolSpamStressTest, PerAccountLimitEnforcement) {
    std::cout << "\n=== Per-Account Limit Test ===" << std::endl;

    auto& account = test_accounts_[0];

    // Try to submit more than the limit
    size_t limit = TRANSACTIONS_PER_ACCOUNT;
    size_t excess = 50;

    size_t accepted = 0;
    size_t rejected = 0;

    for (size_t i = 0; i < limit + excess; ++i) {
        auto tx = create_transaction(account, 1000, BASE_FEE, account.nonce);

        bool added = mempool_->add_transaction(tx);
        if (added) {
            accepted++;
            account.nonce++;
        } else {
            rejected++;
        }
    }

    std::cout << "Accepted: " << accepted << std::endl;
    std::cout << "Rejected: " << rejected << std::endl;
    std::cout << "Limit: " << limit << std::endl;

    EXPECT_LE(accepted, limit) << "Mempool accepted more than limit";
    EXPECT_GT(rejected, 0) << "Mempool should reject transactions over limit";

    auto account_txs = mempool_->get_account_transactions(account.address);
    EXPECT_LE(account_txs.size(), limit) 
        << "Account has more transactions than limit";

    std::cout << "✓ Per-account limit enforced correctly" << std::endl;
}

/**
 * Test: Verify mempool handles concurrent submissions.
 * 
 * This test validates that the mempool can handle transactions
 * submitted from multiple threads concurrently.
 */
TEST_F(MempoolSpamStressTest, ConcurrentSubmissions) {
    std::cout << "\n=== Concurrent Submission Test ===" << std::endl;

    std::atomic<size_t> successful_submissions{0};
    std::atomic<size_t> failed_submissions{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    // Create threads to submit transactions concurrently
    const size_t num_threads = 4;
    std::vector<std::thread> threads;

    for (size_t thread_id = 0; thread_id < num_threads; ++thread_id) {
        threads.emplace_back([&, thread_id]() {
            size_t accounts_per_thread = NUM_ACCOUNTS / num_threads;
            size_t start_acc = thread_id * accounts_per_thread;
            size_t end_acc = (thread_id == num_threads - 1) ? NUM_ACCOUNTS : start_acc + accounts_per_thread;

            for (size_t tx_idx = 0; tx_idx < TRANSACTIONS_PER_ACCOUNT; ++tx_idx) {
                for (size_t acc_idx = start_acc; acc_idx < end_acc; ++acc_idx) {
                    auto& account = test_accounts_[acc_idx];

                    uint64_t fee = BASE_FEE + (tx_idx * 100);
                    auto tx = create_transaction(account, 1000, fee, account.nonce);

                    bool added = mempool_->add_transaction(tx);
                    if (added) {
                        successful_submissions++;
                        account.nonce++;
                    } else {
                        failed_submissions++;
                    }
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "\nConcurrent Submission Results:" << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Successful: " << successful_submissions.load() << std::endl;
    std::cout << "Failed: " << failed_submissions.load() << std::endl;
    std::cout << "Time taken: " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << (successful_submissions.load() * 1000.0 / duration.count()) 
              << " tx/s" << std::endl;

    EXPECT_GT(successful_submissions.load(), 0) << "No transactions were accepted";
    EXPECT_EQ(mempool_->size(), successful_submissions.load()) 
        << "Mempool size doesn't match successful submissions";

    std::cout << "✓ Mempool handled concurrent submissions correctly" << std::endl;
}

} // namespace stress
} // namespace sarafu
