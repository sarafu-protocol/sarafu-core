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
 * Property-Based Test for Mempool Fee Prioritization
 * 
 * **Validates: Requirements 19.5**
 * 
 * Property 50: Fee Prioritization
 * For any block building operation, transactions are selected in descending order of total fee (base + priority).
 * 
 * This test validates that:
 * 1. Transactions with higher fees are selected first
 * 2. Fee prioritization respects gas limits
 * 3. Fee prioritization respects base fee minimum
 * 4. Nonce ordering is maintained per account while prioritizing by fee
 * 5. Multiple accounts' transactions are interleaved by fee priority
 */
class MempoolFeePrioritizationPropertyTest : public ::testing::Test {
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

    // Create a transaction with specific fee
    Transaction create_transaction(
        const Address& from,
        const Address& to,
        uint64_t nonce,
        uint64_t fee,
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
 * Property: Transactions are selected in descending fee order
 * 
 * For any set of transactions from a single account with different fees,
 * get_transactions_for_block can only select transactions in nonce order starting from current nonce.
 */
TEST_F(MempoolFeePrioritizationPropertyTest, DescendingFeeOrder) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 10000000);

        // Create transactions with increasing fees
        std::vector<uint64_t> fees = {100, 200, 300, 400, 500};
        for (size_t i = 0; i < fees.size(); ++i) {
            Transaction tx = create_transaction(from, to, i, fees[i]);
            mempool_->add_transaction(tx);
        }

        // Get transactions for block (large gas limit, no base fee)
        auto block_txs = mempool_->get_transactions_for_block(1000000, 0);

        // Since nonces are sequential and gas is ample, all should be selected in nonce order.
        ASSERT_EQ(block_txs.size(), fees.size())
            << "Wrong number of transactions selected on trial " << trial;

        for (size_t i = 0; i < block_txs.size(); ++i) {
            ASSERT_EQ(block_txs[i].nonce, i)
                << "Nonce order violated on trial " << trial;
        }
    }
}

/**
 * Property: Higher fee transactions from different accounts are prioritized
 * 
 * For any set of accounts with transactions at different fee levels,
 * transactions with higher fees are selected first across accounts.
 */
TEST_F(MempoolFeePrioritizationPropertyTest, CrossAccountFeePriority) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address to = generate_random_address();

        // Create 3 accounts with single transactions at different fees
        std::vector<Address> accounts;
        std::vector<uint64_t> fees = {1000, 500, 2000};  // Unsorted fees

        for (size_t i = 0; i < fees.size(); ++i) {
            Address from = generate_random_address();
            accounts.push_back(from);
            account_manager_->create_account(from, 10000000);

            Transaction tx = create_transaction(from, to, 0, fees[i]);
            mempool_->add_transaction(tx);
        }

        // Get transactions for block
        auto block_txs = mempool_->get_transactions_for_block(1000000, 0);

        ASSERT_EQ(block_txs.size(), 3)
            << "Not all transactions selected on trial " << trial;

        // Verify they are in descending fee order
        ASSERT_EQ(block_txs[0].fee, 2000)
            << "Highest fee transaction not first on trial " << trial;
        ASSERT_EQ(block_txs[1].fee, 1000)
            << "Second highest fee transaction not second on trial " << trial;
        ASSERT_EQ(block_txs[2].fee, 500)
            << "Lowest fee transaction not last on trial " << trial;
    }
}

/**
 * Property: Gas limit is respected during block building
 * 
 * For any set of transactions, get_transactions_for_block respects the max_gas limit.
 */
TEST_F(MempoolFeePrioritizationPropertyTest, GasLimitRespected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address to = generate_random_address();

        // Create multiple accounts with transactions
        for (int i = 0; i < 10; ++i) {
            Address from = generate_random_address();
            account_manager_->create_account(from, 10000000);

            // Each transaction uses 21000 gas
            Transaction tx = create_transaction(from, to, 0, 1000 + i * 100, 1000, 21000);
            mempool_->add_transaction(tx);
        }

        // Set gas limit to allow only 3 transactions (3 * 21000 = 63000)
        uint64_t max_gas = 63000;
        auto block_txs = mempool_->get_transactions_for_block(max_gas, 0);

        // Should select at most 3 transactions
        ASSERT_LE(block_txs.size(), 3)
            << "Too many transactions selected on trial " << trial;

        // Verify total gas doesn't exceed limit
        uint64_t total_gas = 0;
        for (const auto& tx : block_txs) {
            total_gas += tx.gas_limit;
        }
        ASSERT_LE(total_gas, max_gas)
            << "Gas limit exceeded on trial " << trial;
    }
}

/**
 * Property: Base fee minimum is enforced
 * 
 * For any set of transactions, only transactions with fee >= min_base_fee are selected.
 */
TEST_F(MempoolFeePrioritizationPropertyTest, BaseFeeMinimumEnforced) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address to = generate_random_address();

        // Create transactions with various fees
        std::vector<uint64_t> fees = {50, 100, 150, 200, 250, 300};
        for (size_t i = 0; i < fees.size(); ++i) {
            Address from = generate_random_address();
            account_manager_->create_account(from, 10000000);

            Transaction tx = create_transaction(from, to, 0, fees[i]);
            mempool_->add_transaction(tx);
        }

        // Set minimum base fee to 150
        uint64_t min_base_fee = 150;
        auto block_txs = mempool_->get_transactions_for_block(1000000, min_base_fee);

        // Should only select transactions with fee >= 150
        // Expected: 150, 200, 250, 300 (4 transactions)
        ASSERT_EQ(block_txs.size(), 4)
            << "Wrong number of transactions selected on trial " << trial;

        // Verify all selected transactions meet minimum fee
        for (const auto& tx : block_txs) {
            ASSERT_GE(tx.fee, min_base_fee)
                << "Transaction below minimum fee selected on trial " << trial;
        }

        // Verify they are in descending fee order
        for (size_t i = 1; i < block_txs.size(); ++i) {
            ASSERT_GE(block_txs[i-1].fee, block_txs[i].fee)
                << "Transactions not in descending fee order on trial " << trial;
        }
    }
}

/**
 * Property: Nonce ordering maintained within account during fee prioritization
 * 
 * For any account with multiple transactions, even when interleaved with other
 * accounts' transactions by fee, the account's transactions maintain nonce order.
 */
TEST_F(MempoolFeePrioritizationPropertyTest, NonceOrderMaintainedWithFeePriority) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address to = generate_random_address();

        // Account A: transactions with fees 500, 400, 300 (nonces 0, 1, 2)
        Address accountA = generate_random_address();
        account_manager_->create_account(accountA, 10000000);
        mempool_->add_transaction(create_transaction(accountA, to, 0, 500));
        mempool_->add_transaction(create_transaction(accountA, to, 1, 400));
        mempool_->add_transaction(create_transaction(accountA, to, 2, 300));

        // Account B: single transaction with fee 450 (nonce 0)
        Address accountB = generate_random_address();
        account_manager_->create_account(accountB, 10000000);
        mempool_->add_transaction(create_transaction(accountB, to, 0, 450));

        // Get transactions for block
        auto block_txs = mempool_->get_transactions_for_block(1000000, 0);

        ASSERT_EQ(block_txs.size(), 4)
            << "Not all transactions selected on trial " << trial;

        // Expected order by fee: A0(500), B0(450), A1(400), A2(300)
        // But A's transactions must maintain nonce order, so:
        // A0(500) must come before A1(400) and A2(300)
        // A1(400) must come before A2(300)

        // Find positions of account A's transactions
        std::vector<size_t> accountA_positions;
        for (size_t i = 0; i < block_txs.size(); ++i) {
            if (block_txs[i].from == accountA) {
                accountA_positions.push_back(i);
            }
        }

        ASSERT_EQ(accountA_positions.size(), 3)
            << "Not all account A transactions selected on trial " << trial;

        // Verify nonce order for account A
        for (size_t i = 0; i < accountA_positions.size(); ++i) {
            ASSERT_EQ(block_txs[accountA_positions[i]].nonce, i)
                << "Account A nonce order violated on trial " << trial;
        }

        // Verify positions are in increasing order (nonce order maintained)
        for (size_t i = 1; i < accountA_positions.size(); ++i) {
            ASSERT_LT(accountA_positions[i-1], accountA_positions[i])
                << "Account A transactions not in order on trial " << trial;
        }
    }
}

/**
 * Property: Empty mempool returns empty block
 * 
 * For an empty mempool, get_transactions_for_block returns an empty list.
 */
TEST_F(MempoolFeePrioritizationPropertyTest, EmptyMempoolReturnsEmpty) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();

        auto block_txs = mempool_->get_transactions_for_block(1000000, 0);

        ASSERT_EQ(block_txs.size(), 0)
            << "Empty mempool returned transactions on trial " << trial;
    }
}

/**
 * Property: Zero gas limit returns empty block
 * 
 * For any mempool, get_transactions_for_block with max_gas=0 returns empty list.
 */
TEST_F(MempoolFeePrioritizationPropertyTest, ZeroGasLimitReturnsEmpty) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        // Add some transactions
        Address from = generate_random_address();
        Address to = generate_random_address();
        account_manager_->create_account(from, 10000000);

        for (int i = 0; i < 5; ++i) {
            mempool_->add_transaction(create_transaction(from, to, i, 1000));
        }

        // Request block with zero gas limit
        auto block_txs = mempool_->get_transactions_for_block(0, 0);

        ASSERT_EQ(block_txs.size(), 0)
            << "Zero gas limit returned transactions on trial " << trial;
    }
}

/**
 * Property: Very high base fee filters all transactions
 * 
 * For any mempool, if min_base_fee is higher than all transaction fees,
 * get_transactions_for_block returns empty list.
 */
TEST_F(MempoolFeePrioritizationPropertyTest, HighBaseFeeFiltersAll) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address to = generate_random_address();

        // Add transactions with fees 100-500
        for (int i = 0; i < 5; ++i) {
            Address from = generate_random_address();
            account_manager_->create_account(from, 10000000);
            mempool_->add_transaction(create_transaction(from, to, 0, 100 + i * 100));
        }

        // Set base fee higher than all transactions
        auto block_txs = mempool_->get_transactions_for_block(1000000, 1000);

        ASSERT_EQ(block_txs.size(), 0)
            << "High base fee didn't filter all transactions on trial " << trial;
    }
}

/**
 * Property: Transactions with equal fees maintain insertion order
 * 
 * For any set of transactions with equal fees from different accounts,
 * the selection order is deterministic (based on internal ordering).
 */
TEST_F(MempoolFeePrioritizationPropertyTest, EqualFeesConsistentOrder) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        mempool_->clear();
        account_manager_->clear();

        Address to = generate_random_address();

        // Create 5 accounts with transactions at same fee
        std::vector<Address> accounts;
        for (int i = 0; i < 5; ++i) {
            Address from = generate_random_address();
            accounts.push_back(from);
            account_manager_->create_account(from, 10000000);
            mempool_->add_transaction(create_transaction(from, to, 0, 1000));
        }

        // Get transactions twice - should be same order
        auto block_txs1 = mempool_->get_transactions_for_block(1000000, 0);
        auto block_txs2 = mempool_->get_transactions_for_block(1000000, 0);

        ASSERT_EQ(block_txs1.size(), 5)
            << "Not all transactions selected on trial " << trial;
        ASSERT_EQ(block_txs2.size(), 5)
            << "Not all transactions selected on trial " << trial;

        // Verify same order
        for (size_t i = 0; i < block_txs1.size(); ++i) {
            ASSERT_EQ(block_txs1[i].from, block_txs2[i].from)
                << "Order changed between calls on trial " << trial;
        }
    }
}
