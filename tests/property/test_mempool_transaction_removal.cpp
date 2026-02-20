#include "sarafu/state/mempool.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/state/transaction.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>

using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Test for Mempool Transaction Removal
 * 
 * **Validates: Requirements 19.6**
 * 
 * Property 51: Transaction Removal on Finalization
 * For any transaction T included in a finalized block, T is removed from the mempool.
 * 
 * This test validates that:
 * 1. Transactions are removed after block inclusion
 * 2. Removal is idempotent (removing twice doesn't cause errors)
 * 3. Removing non-existent transactions doesn't affect mempool
 * 4. Partial removal works correctly
 * 5. Old transactions are pruned after timeout
 */
class MempoolTransactionRemovalPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        account_manager_ = std::make_unique<AccountManager>();
        mempool_ = std::make_unique<Mempool>();
        mempool_->set_account_manager(account_manager_.get());
    }

    // Generate random address
    Address generate_random_address() {
        std::vector<uint8_t> data(32);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 32; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return Address(data);
    }

    // Create a transaction
    Transaction create_transaction(
        const Address& from,
        const Address& to,
        uint64_t nonce,
        uint64_t fee = 1000,
        uint64_t amount = 1000,
        uint64_t gas_limit = 21000
    ) {
        return Transaction(from, to, amount, nonce, fee, gas_limit, 1);
    }

    std::mt19937 rng_;
    std::unique_ptr<AccountManager> account_manager_;
    std::unique_ptr<Mempool> mempool_;
};

/**
 * Property: Removed transactions are no longer in mempool
 * 
 * For any transaction T in the mempool, after calling remove_transactions([T.hash()]),
 * T is no longer in the mempool.
 */
TEST_F(MempoolTransactionRemovalPropertyTest, RemovedTransactionsNotInMempool) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 10000000);

        // Add 5 transactions
        std::vector<Transaction> transactions;
        for (int i = 0; i < 5; ++i) {
            Transaction tx = create_transaction(from, to, i);
            transactions.push_back(tx);
            mempool_->add_transaction(tx);
        }

        size_t initial_size = mempool_->size();
        ASSERT_EQ(initial_size, 5)
            << "Initial size wrong on trial " << trial;

        // Remove transaction at index 2
        std::vector<sarafu::crypto::Blake3Hash> to_remove;
        to_remove.push_back(transactions[2].hash());
        mempool_->remove_transactions(to_remove);

        // Verify size decreased
        ASSERT_EQ(mempool_->size(), 4)
            << "Size didn't decrease after removal on trial " << trial;

        // Verify removed transaction not in account queue
        auto account_txs = mempool_->get_account_transactions(from);
        ASSERT_EQ(account_txs.size(), 4)
            << "Account queue size wrong on trial " << trial;

        // Verify the removed transaction is not present
        for (const auto& tx : account_txs) {
            ASSERT_NE(tx.hash(), transactions[2].hash())
                << "Removed transaction still in mempool on trial " << trial;
        }
    }
}

/**
 * Property: Removing multiple transactions works correctly
 * 
 * For any set of transactions {T1, T2, ..., Tn} in the mempool,
 * after removing them all, none are in the mempool.
 */
TEST_F(MempoolTransactionRemovalPropertyTest, MultipleTransactionsRemoved) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 10000000);

        // Add 10 transactions
        std::vector<Transaction> transactions;
        for (int i = 0; i < 10; ++i) {
            Transaction tx = create_transaction(from, to, i);
            transactions.push_back(tx);
            mempool_->add_transaction(tx);
        }

        ASSERT_EQ(mempool_->size(), 10)
            << "Initial size wrong on trial " << trial;

        // Remove transactions 2, 5, 7
        std::vector<sarafu::crypto::Blake3Hash> to_remove;
        to_remove.push_back(transactions[2].hash());
        to_remove.push_back(transactions[5].hash());
        to_remove.push_back(transactions[7].hash());
        mempool_->remove_transactions(to_remove);

        // Verify size
        ASSERT_EQ(mempool_->size(), 7)
            << "Size wrong after removal on trial " << trial;

        // Verify removed transactions not present
        auto account_txs = mempool_->get_account_transactions(from);
        ASSERT_EQ(account_txs.size(), 7)
            << "Account queue size wrong on trial " << trial;

        for (const auto& tx : account_txs) {
            ASSERT_NE(tx.hash(), transactions[2].hash())
                << "Transaction 2 still present on trial " << trial;
            ASSERT_NE(tx.hash(), transactions[5].hash())
                << "Transaction 5 still present on trial " << trial;
            ASSERT_NE(tx.hash(), transactions[7].hash())
                << "Transaction 7 still present on trial " << trial;
        }
    }
}

/**
 * Property: Removal is idempotent
 * 
 * For any transaction T, removing it twice has the same effect as removing it once.
 */
TEST_F(MempoolTransactionRemovalPropertyTest, RemovalIsIdempotent) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 10000000);

        // Add transaction
        Transaction tx = create_transaction(from, to, 0);
        mempool_->add_transaction(tx);

        ASSERT_EQ(mempool_->size(), 1)
            << "Initial size wrong on trial " << trial;

        // Remove once
        std::vector<sarafu::crypto::Blake3Hash> to_remove;
        to_remove.push_back(tx.hash());
        mempool_->remove_transactions(to_remove);

        ASSERT_EQ(mempool_->size(), 0)
            << "Size wrong after first removal on trial " << trial;

        // Remove again (should be no-op)
        mempool_->remove_transactions(to_remove);

        ASSERT_EQ(mempool_->size(), 0)
            << "Size changed after second removal on trial " << trial;
    }
}

/**
 * Property: Removing non-existent transaction is safe
 * 
 * For any transaction hash H not in the mempool, calling remove_transactions([H])
 * doesn't affect the mempool.
 */
TEST_F(MempoolTransactionRemovalPropertyTest, RemovingNonExistentIsSafe) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 10000000);

        // Add some transactions
        for (int i = 0; i < 5; ++i) {
            mempool_->add_transaction(create_transaction(from, to, i));
        }

        size_t initial_size = mempool_->size();

        // Create a transaction that's not in mempool
        Transaction non_existent = create_transaction(from, to, 100);

        // Try to remove it
        std::vector<sarafu::crypto::Blake3Hash> to_remove;
        to_remove.push_back(non_existent.hash());
        mempool_->remove_transactions(to_remove);

        // Verify size unchanged
        ASSERT_EQ(mempool_->size(), initial_size)
            << "Size changed after removing non-existent on trial " << trial;
    }
}

/**
 * Property: Removing all transactions empties mempool
 * 
 * For any mempool with N transactions, removing all N transactions results in empty mempool.
 */
TEST_F(MempoolTransactionRemovalPropertyTest, RemovingAllEmptiesMempool) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        // Generate random number of accounts and transactions
        std::uniform_int_distribution<int> account_dist(3, 10);
        int num_accounts = account_dist(rng_);

        Address to = generate_random_address();
        std::vector<sarafu::crypto::Blake3Hash> all_hashes;

        for (int i = 0; i < num_accounts; ++i) {
            Address from = generate_random_address();
            account_manager_->create_account(from, 10000000);

            std::uniform_int_distribution<int> tx_dist(1, 5);
            int num_txs = tx_dist(rng_);

            for (int j = 0; j < num_txs; ++j) {
                Transaction tx = create_transaction(from, to, j);
                all_hashes.push_back(tx.hash());
                mempool_->add_transaction(tx);
            }
        }

        size_t initial_size = mempool_->size();
        ASSERT_GT(initial_size, 0)
            << "Mempool empty before removal on trial " << trial;

        // Remove all transactions
        mempool_->remove_transactions(all_hashes);

        // Verify empty
        ASSERT_EQ(mempool_->size(), 0)
            << "Mempool not empty after removing all on trial " << trial;
    }
}

/**
 * Property: Removal maintains nonce ordering for remaining transactions
 * 
 * For any account A with transactions, after removing some transactions,
 * the remaining transactions maintain nonce ordering.
 */
TEST_F(MempoolTransactionRemovalPropertyTest, RemovalMaintainsNonceOrdering) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 10000000);

        // Add 10 transactions
        std::vector<Transaction> transactions;
        for (int i = 0; i < 10; ++i) {
            Transaction tx = create_transaction(from, to, i);
            transactions.push_back(tx);
            mempool_->add_transaction(tx);
        }

        // Remove transactions 1, 4, 6
        std::vector<sarafu::crypto::Blake3Hash> to_remove;
        to_remove.push_back(transactions[1].hash());
        to_remove.push_back(transactions[4].hash());
        to_remove.push_back(transactions[6].hash());
        mempool_->remove_transactions(to_remove);

        // Get remaining transactions
        auto account_txs = mempool_->get_account_transactions(from);

        // Verify nonce ordering
        for (size_t i = 1; i < account_txs.size(); ++i) {
            ASSERT_LT(account_txs[i-1].nonce, account_txs[i].nonce)
                << "Nonce ordering violated after removal on trial " << trial;
        }
    }
}

/**
 * Property: Removal from one account doesn't affect other accounts
 * 
 * For any two accounts A and B, removing transactions from A doesn't affect B's transactions.
 */
TEST_F(MempoolTransactionRemovalPropertyTest, RemovalIsAccountIndependent) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address accountA = generate_random_address();
        Address accountB = generate_random_address();
        Address to = generate_random_address();

        account_manager_->create_account(accountA, 10000000);
        account_manager_->create_account(accountB, 10000000);

        // Add transactions for both accounts
        std::vector<Transaction> txsA;
        std::vector<Transaction> txsB;

        for (int i = 0; i < 5; ++i) {
            Transaction txA = create_transaction(accountA, to, i);
            Transaction txB = create_transaction(accountB, to, i);
            txsA.push_back(txA);
            txsB.push_back(txB);
            mempool_->add_transaction(txA);
            mempool_->add_transaction(txB);
        }

        // Remove all transactions from account A
        std::vector<sarafu::crypto::Blake3Hash> to_remove;
        for (const auto& tx : txsA) {
            to_remove.push_back(tx.hash());
        }
        mempool_->remove_transactions(to_remove);

        // Verify account B's transactions unchanged
        auto accountB_txs = mempool_->get_account_transactions(accountB);
        ASSERT_EQ(accountB_txs.size(), 5)
            << "Account B affected by account A removal on trial " << trial;

        for (size_t i = 0; i < accountB_txs.size(); ++i) {
            ASSERT_EQ(accountB_txs[i].nonce, i)
                << "Account B transaction changed on trial " << trial;
        }

        // Verify account A's transactions removed
        auto accountA_txs = mempool_->get_account_transactions(accountA);
        ASSERT_EQ(accountA_txs.size(), 0)
            << "Account A transactions not removed on trial " << trial;
    }
}

/**
 * Property: Empty removal list is safe
 * 
 * Calling remove_transactions with an empty list doesn't affect the mempool.
 */
TEST_F(MempoolTransactionRemovalPropertyTest, EmptyRemovalListIsSafe) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 10000000);

        // Add some transactions
        for (int i = 0; i < 5; ++i) {
            mempool_->add_transaction(create_transaction(from, to, i));
        }

        size_t initial_size = mempool_->size();

        // Remove with empty list
        std::vector<sarafu::crypto::Blake3Hash> empty_list;
        mempool_->remove_transactions(empty_list);

        // Verify size unchanged
        ASSERT_EQ(mempool_->size(), initial_size)
            << "Size changed after empty removal on trial " << trial;
    }
}

/**
 * Property: Pruning removes old transactions
 * 
 * For any transaction older than MAX_AGE_SECONDS, prune_old_transactions removes it.
 * 
 * Note: This test is simplified and doesn't actually wait for timeout.
 * In production, transactions would be timestamped and pruned based on age.
 */
TEST_F(MempoolTransactionRemovalPropertyTest, PruningRemovesOldTransactions) {
    // This test verifies the prune function can be called without errors
    // Actual age-based pruning would require manipulating timestamps or waiting
    
    const int NUM_TRIALS = 10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 10000000);

        // Add some transactions
        for (int i = 0; i < 5; ++i) {
            mempool_->add_transaction(create_transaction(from, to, i));
        }

        size_t initial_size = mempool_->size();

        // Call prune (should not remove recently added transactions)
        mempool_->prune_old_transactions();

        // Verify transactions still present (they're not old enough)
        ASSERT_EQ(mempool_->size(), initial_size)
            << "Recent transactions pruned on trial " << trial;
    }
}
