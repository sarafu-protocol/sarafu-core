#include "sarafu/state/transaction_executor.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/state/transaction.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>

using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Test for Base Fee Burning
 * 
 * **Validates: Requirements 12.6**
 * 
 * Property 37: Base Fee Burning
 * For any transaction T executed in a block, the base fee portion of T.fee
 * is burned (removed from total supply).
 * 
 * This test validates that:
 * 1. Base fee portion is correctly calculated
 * 2. Base fee is burned (tracked separately from priority fee)
 * 3. Total burned amount accumulates correctly
 * 4. Burning is independent of transaction amount
 */
class BaseFeeBurningPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        
        // Create account manager with initial accounts
        account_manager_ = std::make_unique<AccountManager>();
        executor_ = std::make_unique<TransactionExecutor>(*account_manager_);
    }

    void TearDown() override {
        executor_.reset();
        account_manager_.reset();
    }

    // Generate random amount (1,000 to 1,000,000)
    uint64_t generate_random_amount() {
        std::uniform_int_distribution<uint64_t> dist(1000, 1000000);
        return dist(rng_);
    }

    // Generate random fee (100 to 10,000)
    uint64_t generate_random_fee() {
        std::uniform_int_distribution<uint64_t> dist(100, 10000);
        return dist(rng_);
    }

    // Generate random base fee (10 to 1,000)
    uint64_t generate_random_base_fee() {
        std::uniform_int_distribution<uint64_t> dist(10, 1000);
        return dist(rng_);
    }

    // Generate random gas limit (1,000 to 100,000)
    uint64_t generate_random_gas_limit() {
        std::uniform_int_distribution<uint64_t> dist(1000, 100000);
        return dist(rng_);
    }

    // Create a test account with balance
    Address create_account_with_balance(uint64_t balance) {
        // Generate a random address
        std::vector<uint8_t> addr_bytes(32);
        for (size_t i = 0; i < 32; ++i) {
            addr_bytes[i] = static_cast<uint8_t>(rng_() % 256);
        }
        Address addr(addr_bytes);
        
        // Create account with balance
        account_manager_->create_account(addr, balance);
        
        return addr;
    }

    std::mt19937 rng_;
    std::unique_ptr<AccountManager> account_manager_;
    std::unique_ptr<TransactionExecutor> executor_;
};

/**
 * Property: Base fee is burned for every transaction
 * 
 * For any transaction T with fee F and base_fee B,
 * the base fee portion min(F, B * gas_limit) is burned.
 */
TEST_F(BaseFeeBurningPropertyTest, BaseFeeBurnedForEveryTransaction) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Reset executor counters
        executor_->reset_counters();

        uint64_t amount = generate_random_amount();
        uint64_t fee = generate_random_fee();
        uint64_t base_fee = generate_random_base_fee();
        uint64_t gas_limit = generate_random_gas_limit();

        // Create sender and recipient accounts
        Address sender = create_account_with_balance(amount + fee + 10000);
        Address recipient = create_account_with_balance(0);
        Address proposer = create_account_with_balance(0);

        // Create transaction
        Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);

        // Execute transaction
        TransactionReceipt receipt = executor_->execute_transaction(tx, 1, base_fee, proposer);

        ASSERT_TRUE(receipt.success)
            << "Transaction should succeed on trial " << trial;

        // Calculate expected base fee burned
        uint64_t expected_base_fee = std::min(fee, base_fee * gas_limit);

        // Verify base fee was burned
        uint64_t total_burned = executor_->get_total_burned();
        ASSERT_EQ(total_burned, expected_base_fee)
            << "Base fee burned mismatch on trial " << trial
            << " (fee=" << fee << ", base_fee=" << base_fee
            << ", gas_limit=" << gas_limit << ")";
    }
}

/**
 * Property: Burned amount accumulates across transactions
 * 
 * For any sequence of transactions, the total burned amount equals
 * the sum of base fees from all transactions.
 */
TEST_F(BaseFeeBurningPropertyTest, BurnedAmountAccumulates) {
    const int NUM_TRIALS = 100;
    const int TXS_PER_TRIAL = 10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Reset executor counters
        executor_->reset_counters();

        uint64_t expected_total_burned = 0;

        for (int i = 0; i < TXS_PER_TRIAL; ++i) {
            uint64_t amount = generate_random_amount();
            uint64_t fee = generate_random_fee();
            uint64_t base_fee = generate_random_base_fee();
            uint64_t gas_limit = generate_random_gas_limit();

            // Create sender and recipient accounts
            Address sender = create_account_with_balance(amount + fee + 10000);
            Address recipient = create_account_with_balance(0);
            Address proposer = create_account_with_balance(0);

            // Create transaction
            Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);

            // Execute transaction
            executor_->execute_transaction(tx, 1, base_fee, proposer);

            // Calculate expected base fee burned for this transaction
            uint64_t tx_base_fee = std::min(fee, base_fee * gas_limit);
            expected_total_burned += tx_base_fee;
        }

        // Verify total burned amount
        uint64_t actual_total_burned = executor_->get_total_burned();
        ASSERT_EQ(actual_total_burned, expected_total_burned)
            << "Total burned amount mismatch on trial " << trial;
    }
}

/**
 * Property: Base fee burning is independent of transaction amount
 * 
 * For any two transactions with the same fee but different amounts,
 * the base fee burned is the same.
 */
TEST_F(BaseFeeBurningPropertyTest, BurningIndependentOfAmount) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t amount1 = generate_random_amount();
        uint64_t amount2 = generate_random_amount();
        uint64_t fee = generate_random_fee();
        uint64_t base_fee = generate_random_base_fee();
        uint64_t gas_limit = generate_random_gas_limit();

        // Transaction 1
        executor_->reset_counters();
        Address sender1 = create_account_with_balance(amount1 + fee + 10000);
        Address recipient1 = create_account_with_balance(0);
        Address proposer1 = create_account_with_balance(0);
        Transaction tx1(sender1, recipient1, amount1, 0, fee, gas_limit, 1);
        executor_->execute_transaction(tx1, 1, base_fee, proposer1);
        uint64_t burned1 = executor_->get_total_burned();

        // Transaction 2
        executor_->reset_counters();
        Address sender2 = create_account_with_balance(amount2 + fee + 10000);
        Address recipient2 = create_account_with_balance(0);
        Address proposer2 = create_account_with_balance(0);
        Transaction tx2(sender2, recipient2, amount2, 0, fee, gas_limit, 1);
        executor_->execute_transaction(tx2, 1, base_fee, proposer2);
        uint64_t burned2 = executor_->get_total_burned();

        ASSERT_EQ(burned1, burned2)
            << "Base fee burned should be independent of amount on trial " << trial
            << " (amount1=" << amount1 << ", amount2=" << amount2
            << ", burned1=" << burned1 << ", burned2=" << burned2 << ")";
    }
}

/**
 * Property: Base fee calculation is deterministic
 * 
 * For any transaction parameters, calculating the base fee multiple times
 * yields the same result.
 */
TEST_F(BaseFeeBurningPropertyTest, BaseFeCalculationIsDeterministic) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t amount = generate_random_amount();
        uint64_t fee = generate_random_fee();
        uint64_t base_fee = generate_random_base_fee();
        uint64_t gas_limit = generate_random_gas_limit();

        // Execute same transaction multiple times
        uint64_t burned1, burned2, burned3;

        {
            executor_->reset_counters();
            Address sender = create_account_with_balance(amount + fee + 10000);
            Address recipient = create_account_with_balance(0);
            Address proposer = create_account_with_balance(0);
            Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);
            executor_->execute_transaction(tx, 1, base_fee, proposer);
            burned1 = executor_->get_total_burned();
        }

        {
            executor_->reset_counters();
            Address sender = create_account_with_balance(amount + fee + 10000);
            Address recipient = create_account_with_balance(0);
            Address proposer = create_account_with_balance(0);
            Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);
            executor_->execute_transaction(tx, 1, base_fee, proposer);
            burned2 = executor_->get_total_burned();
        }

        {
            executor_->reset_counters();
            Address sender = create_account_with_balance(amount + fee + 10000);
            Address recipient = create_account_with_balance(0);
            Address proposer = create_account_with_balance(0);
            Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);
            executor_->execute_transaction(tx, 1, base_fee, proposer);
            burned3 = executor_->get_total_burned();
        }

        ASSERT_EQ(burned1, burned2)
            << "Base fee calculation not deterministic (burned1 != burned2) on trial " << trial;
        ASSERT_EQ(burned2, burned3)
            << "Base fee calculation not deterministic (burned2 != burned3) on trial " << trial;
    }
}

/**
 * Property: Higher base fee results in more burning
 * 
 * For any transaction with sufficient fee, if base_fee2 > base_fee1,
 * then burned2 >= burned1.
 */
TEST_F(BaseFeeBurningPropertyTest, HigherBaseFeeResultsInMoreBurning) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t amount = generate_random_amount();
        uint64_t fee = 10000;  // High fee to ensure it covers both base fees
        uint64_t base_fee1 = generate_random_base_fee();
        uint64_t base_fee2 = base_fee1 + 100;
        uint64_t gas_limit = generate_random_gas_limit();

        // Transaction with base_fee1
        executor_->reset_counters();
        Address sender1 = create_account_with_balance(amount + fee + 10000);
        Address recipient1 = create_account_with_balance(0);
        Address proposer1 = create_account_with_balance(0);
        Transaction tx1(sender1, recipient1, amount, 0, fee, gas_limit, 1);
        executor_->execute_transaction(tx1, 1, base_fee1, proposer1);
        uint64_t burned1 = executor_->get_total_burned();

        // Transaction with base_fee2
        executor_->reset_counters();
        Address sender2 = create_account_with_balance(amount + fee + 10000);
        Address recipient2 = create_account_with_balance(0);
        Address proposer2 = create_account_with_balance(0);
        Transaction tx2(sender2, recipient2, amount, 0, fee, gas_limit, 1);
        executor_->execute_transaction(tx2, 1, base_fee2, proposer2);
        uint64_t burned2 = executor_->get_total_burned();

        ASSERT_GE(burned2, burned1)
            << "Higher base fee should result in more burning on trial " << trial
            << " (base_fee1=" << base_fee1 << ", base_fee2=" << base_fee2
            << ", burned1=" << burned1 << ", burned2=" << burned2 << ")";
    }
}

/**
 * Property: Base fee is capped by transaction fee
 * 
 * For any transaction, the burned amount never exceeds the transaction fee.
 */
TEST_F(BaseFeeBurningPropertyTest, BurnedAmountCappedByTransactionFee) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t amount = generate_random_amount();
        uint64_t fee = generate_random_fee();
        uint64_t base_fee = generate_random_base_fee();
        uint64_t gas_limit = generate_random_gas_limit();

        executor_->reset_counters();
        Address sender = create_account_with_balance(amount + fee + 10000);
        Address recipient = create_account_with_balance(0);
        Address proposer = create_account_with_balance(0);

        Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);
        executor_->execute_transaction(tx, 1, base_fee, proposer);

        uint64_t burned = executor_->get_total_burned();

        ASSERT_LE(burned, fee)
            << "Burned amount should not exceed transaction fee on trial " << trial
            << " (fee=" << fee << ", burned=" << burned << ")";
    }
}
