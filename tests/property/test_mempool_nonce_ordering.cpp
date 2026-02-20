#include "sarafu/state/mempool.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/state/transaction.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <algorithm>

using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Test for Mempool Nonce Ordering
 * 
 * **Validates: Requirements 19.4, 7.9**
 * 
 * Property 49: Nonce Ordering
 * For any account A in the mempool, A's transactions are ordered by increasing nonce.
 * 
 * This test validates that:
 * 1. Transactions are inserted in nonce order regardless of insertion order
 * 2. Multiple transactions from the same account maintain nonce ordering
 * 3. Nonce ordering is maintained across different accounts independently
 * 4. Transactions with gaps in nonces are still ordered correctly
 * 5. Nonce ordering persists after transaction removal
 */
class MempoolNonceOrderingPropertyTest : public ::testing::Test {
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

    // Generate random Ed25519 key pair
    std::pair<Ed25519_PublicKey, Ed25519_PrivateKey> generate_keypair() {
        return Ed25519::generate_keypair();
    }

    // Create a transaction with specific nonce
    Transaction create_transaction(
        const Address& from,
        const Address& to,
        uint64_t nonce,
        uint64_t amount = 1000,
        uint64_t fee = 100,
        uint64_t gas_limit = 21000
    ) {
        return Transaction(from, to, amount, nonce, fee, gas_limit, 1);
    }

    std::mt19937 rng_;
    std::unique_ptr<AccountManager> account_manager_;
    std::unique_ptr<Mempool> mempool_;
};

/**
 * Property: Transactions inserted in random order are stored in nonce order
 * 
 * For any account A and set of transactions with nonces {n1, n2, ..., nk},
 * regardless of insertion order, the mempool stores them in increasing nonce order.
 */
TEST_F(MempoolNonceOrderingPropertyTest, RandomInsertionMaintainsNonceOrder) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        // Create account with sufficient balance
        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 1000000);

        // Generate random number of transactions (5-20)
        std::uniform_int_distribution<int> count_dist(5, 20);
        int num_txs = count_dist(rng_);

        // Create transactions with sequential nonces
        std::vector<Transaction> transactions;
        for (int i = 0; i < num_txs; ++i) {
            transactions.push_back(create_transaction(from, to, i));
        }

        // Shuffle transactions to insert in random order
        std::shuffle(transactions.begin(), transactions.end(), rng_);

        // Insert all transactions
        for (const auto& tx : transactions) {
            mempool_->add_transaction(tx);
        }

        // Get account transactions from mempool
        auto mempool_txs = mempool_->get_account_transactions(from);

        // Verify they are in nonce order
        ASSERT_EQ(mempool_txs.size(), num_txs)
            << "Not all transactions were added on trial " << trial;

        for (size_t i = 0; i < mempool_txs.size(); ++i) {
            ASSERT_EQ(mempool_txs[i].nonce, i)
                << "Transaction at index " << i << " has wrong nonce on trial " << trial;
        }

        // Verify strict ordering
        for (size_t i = 1; i < mempool_txs.size(); ++i) {
            ASSERT_LT(mempool_txs[i-1].nonce, mempool_txs[i].nonce)
                << "Transactions not in strict nonce order on trial " << trial;
        }
    }
}

/**
 * Property: Multiple accounts maintain independent nonce ordering
 * 
 * For any set of accounts {A1, A2, ..., An}, each account's transactions
 * are ordered by nonce independently of other accounts.
 */
TEST_F(MempoolNonceOrderingPropertyTest, MultipleAccountsIndependentOrdering) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        // Generate random number of accounts (3-10)
        std::uniform_int_distribution<int> account_dist(3, 10);
        int num_accounts = account_dist(rng_);

        std::vector<Address> accounts;
        Address to = generate_random_address();

        // Create accounts and transactions
        for (int i = 0; i < num_accounts; ++i) {
            Address from = generate_random_address();
            accounts.push_back(from);
            account_manager_->create_account(from, 1000000);

            // Generate random number of transactions per account (3-10)
            std::uniform_int_distribution<int> tx_dist(3, 10);
            int num_txs = tx_dist(rng_);

            // Create and insert transactions in random order
            std::vector<Transaction> transactions;
            for (int j = 0; j < num_txs; ++j) {
                transactions.push_back(create_transaction(from, to, j));
            }
            std::shuffle(transactions.begin(), transactions.end(), rng_);

            for (const auto& tx : transactions) {
                mempool_->add_transaction(tx);
            }
        }

        // Verify each account's transactions are in nonce order
        for (const auto& account : accounts) {
            auto mempool_txs = mempool_->get_account_transactions(account);

            ASSERT_GT(mempool_txs.size(), 0)
                << "Account has no transactions on trial " << trial;

            for (size_t i = 1; i < mempool_txs.size(); ++i) {
                ASSERT_LT(mempool_txs[i-1].nonce, mempool_txs[i].nonce)
                    << "Account transactions not in nonce order on trial " << trial;
            }
        }
    }
}

/**
 * Property: Nonce ordering maintained with gaps
 * 
 * For any account A with transactions having non-consecutive nonces,
 * the transactions are still ordered by increasing nonce.
 */
TEST_F(MempoolNonceOrderingPropertyTest, NonceOrderingWithGaps) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 1000000);

        // Create transactions with gaps in nonces
        std::vector<uint64_t> nonces = {0, 2, 5, 7, 10, 15, 20};
        std::vector<Transaction> transactions;

        for (uint64_t nonce : nonces) {
            transactions.push_back(create_transaction(from, to, nonce));
        }

        // Shuffle and insert
        std::shuffle(transactions.begin(), transactions.end(), rng_);
        for (const auto& tx : transactions) {
            mempool_->add_transaction(tx);
        }

        // Verify ordering
        auto mempool_txs = mempool_->get_account_transactions(from);
        ASSERT_EQ(mempool_txs.size(), nonces.size())
            << "Not all transactions added on trial " << trial;

        for (size_t i = 0; i < mempool_txs.size(); ++i) {
            ASSERT_EQ(mempool_txs[i].nonce, nonces[i])
                << "Transaction nonce mismatch at index " << i << " on trial " << trial;
        }
    }
}

/**
 * Property: Nonce ordering persists after transaction removal
 * 
 * For any account A with transactions, after removing some transactions,
 * the remaining transactions are still in nonce order.
 */
TEST_F(MempoolNonceOrderingPropertyTest, NonceOrderingAfterRemoval) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 1000000);

        // Create 10 transactions
        std::vector<Transaction> transactions;
        for (int i = 0; i < 10; ++i) {
            transactions.push_back(create_transaction(from, to, i));
        }

        // Insert all
        for (const auto& tx : transactions) {
            mempool_->add_transaction(tx);
        }

        // Remove some transactions (e.g., nonces 2, 5, 7)
        std::vector<sarafu::crypto::Blake3Hash> to_remove;
        to_remove.push_back(transactions[2].hash());
        to_remove.push_back(transactions[5].hash());
        to_remove.push_back(transactions[7].hash());
        mempool_->remove_transactions(to_remove);

        // Verify remaining transactions are still in order
        auto mempool_txs = mempool_->get_account_transactions(from);
        ASSERT_EQ(mempool_txs.size(), 7)
            << "Wrong number of transactions after removal on trial " << trial;

        // Expected nonces: 0, 1, 3, 4, 6, 8, 9
        std::vector<uint64_t> expected_nonces = {0, 1, 3, 4, 6, 8, 9};
        for (size_t i = 0; i < mempool_txs.size(); ++i) {
            ASSERT_EQ(mempool_txs[i].nonce, expected_nonces[i])
                << "Nonce mismatch after removal on trial " << trial;
        }

        // Verify strict ordering
        for (size_t i = 1; i < mempool_txs.size(); ++i) {
            ASSERT_LT(mempool_txs[i-1].nonce, mempool_txs[i].nonce)
                << "Ordering violated after removal on trial " << trial;
        }
    }
}

/**
 * Property: Duplicate nonce insertion is rejected
 * 
 * For any account A with a transaction at nonce N, attempting to insert
 * another transaction with nonce N is rejected (same hash).
 */
TEST_F(MempoolNonceOrderingPropertyTest, DuplicateNonceRejected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 1000000);

        // Create transaction with nonce 5
        Transaction tx = create_transaction(from, to, 5);

        // Insert first time - should succeed
        bool first_result = mempool_->add_transaction(tx);
        ASSERT_TRUE(first_result)
            << "First insertion failed on trial " << trial;

        // Insert same transaction again - should fail (duplicate)
        bool second_result = mempool_->add_transaction(tx);
        ASSERT_FALSE(second_result)
            << "Duplicate insertion succeeded on trial " << trial;

        // Verify only one transaction in mempool
        auto mempool_txs = mempool_->get_account_transactions(from);
        ASSERT_EQ(mempool_txs.size(), 1)
            << "Duplicate transaction was added on trial " << trial;
    }
}

/**
 * Property: Nonce ordering with large nonce values
 * 
 * For any account A with transactions having large nonce values within acceptable gap,
 * the ordering is maintained correctly.
 */
TEST_F(MempoolNonceOrderingPropertyTest, LargeNonceValuesOrdered) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 1000000);

        // Use nonce values within acceptable gap (MAX_NONCE_GAP = 100)
        // Start from current nonce (0) and use values within 100
        std::vector<uint64_t> nonces = {
            0, 1, 2, 5, 10, 20, 50, 99
        };

        std::vector<Transaction> transactions;
        for (uint64_t nonce : nonces) {
            transactions.push_back(create_transaction(from, to, nonce));
        }

        // Shuffle and insert
        std::shuffle(transactions.begin(), transactions.end(), rng_);
        for (const auto& tx : transactions) {
            mempool_->add_transaction(tx);
        }

        // Verify ordering
        auto mempool_txs = mempool_->get_account_transactions(from);
        ASSERT_EQ(mempool_txs.size(), nonces.size())
            << "Not all transactions added on trial " << trial;

        for (size_t i = 0; i < mempool_txs.size(); ++i) {
            ASSERT_EQ(mempool_txs[i].nonce, nonces[i])
                << "Large nonce ordering failed on trial " << trial;
        }
    }
}

/**
 * Property: Empty mempool has no ordering violations
 * 
 * For an empty mempool, querying any account returns an empty list.
 */
TEST_F(MempoolNonceOrderingPropertyTest, EmptyMempoolNoViolations) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        
        Address random_addr = generate_random_address();
        auto txs = mempool_->get_account_transactions(random_addr);
        
        ASSERT_EQ(txs.size(), 0)
            << "Empty mempool returned transactions on trial " << trial;
    }
}
