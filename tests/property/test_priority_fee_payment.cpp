#include "sarafu/state/transaction_executor.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/state/transaction.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>

using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Test for Priority Fee Payment
 * 
 * **Validates: Requirements 12.7**
 * 
 * Property 38: Priority Fee Payment
 * For any transaction T with priority fee P, P is paid to the block proposer.
 * 
 * This test validates that:
 * 1. Priority fee = total fee - base fee
 * 2. Priority fee is paid to block proposer
 * 3. Proposer balance increases by priority fee
 * 4. Total priority fees accumulate correctly
 */
class PriorityFeePaymentPropertyTest : public ::testing::Test {
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
 * Property: Priority fee is paid to block proposer
 * 
 * For any transaction T with fee F and base_fee B,
 * the priority fee (F - min(F, B * gas_limit)) is paid to the proposer.
 */
TEST_F(PriorityFeePaymentPropertyTest, PriorityFeePaidToProposer) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        executor_->reset_counters();

        uint64_t amount = generate_random_amount();
        uint64_t fee = generate_random_fee();
        uint64_t base_fee = generate_random_base_fee();
        uint64_t gas_limit = generate_random_gas_limit();

        // Create sender, recipient, and proposer accounts
        Address sender = create_account_with_balance(amount + fee + 10000);
        Address recipient = create_account_with_balance(0);
        Address proposer = create_account_with_balance(1000);

        uint64_t proposer_balance_before = account_manager_->get_account(proposer).balance;

        // Create transaction
        Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);

        // Execute transaction
        TransactionReceipt receipt = executor_->execute_transaction(tx, 1, base_fee, proposer);

        ASSERT_TRUE(receipt.success)
            << "Transaction should succeed on trial " << trial;

        // Calculate expected priority fee
        uint64_t base_fee_amount = std::min(fee, base_fee * gas_limit);
        uint64_t expected_priority_fee = (fee > base_fee_amount) ? (fee - base_fee_amount) : 0;

        // Verify proposer balance increased by priority fee
        uint64_t proposer_balance_after = account_manager_->get_account(proposer).balance;
        uint64_t proposer_increase = proposer_balance_after - proposer_balance_before;

        ASSERT_EQ(proposer_increase, expected_priority_fee)
            << "Proposer balance increase mismatch on trial " << trial
            << " (fee=" << fee << ", base_fee_amount=" << base_fee_amount
            << ", expected_priority=" << expected_priority_fee << ")";

        // Verify total priority fees tracked correctly
        uint64_t total_priority_fees = executor_->get_total_priority_fees();
        ASSERT_EQ(total_priority_fees, expected_priority_fee)
            << "Total priority fees mismatch on trial " << trial;
    }
}

/**
 * Property: Priority fee accumulates across transactions
 * 
 * For any sequence of transactions, the total priority fees equal
 * the sum of priority fees from all transactions.
 */
TEST_F(PriorityFeePaymentPropertyTest, PriorityFeeAccumulates) {
    const int NUM_TRIALS = 100;
    const int TXS_PER_TRIAL = 10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        executor_->reset_counters();

        Address proposer = create_account_with_balance(1000);
        uint64_t proposer_balance_before = account_manager_->get_account(proposer).balance;
        uint64_t expected_total_priority = 0;

        for (int i = 0; i < TXS_PER_TRIAL; ++i) {
            uint64_t amount = generate_random_amount();
            uint64_t fee = generate_random_fee();
            uint64_t base_fee = generate_random_base_fee();
            uint64_t gas_limit = generate_random_gas_limit();

            // Create sender and recipient accounts
            Address sender = create_account_with_balance(amount + fee + 10000);
            Address recipient = create_account_with_balance(0);

            // Create transaction
            Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);

            // Execute transaction
            executor_->execute_transaction(tx, 1, base_fee, proposer);

            // Calculate expected priority fee for this transaction
            uint64_t base_fee_amount = std::min(fee, base_fee * gas_limit);
            uint64_t tx_priority_fee = (fee > base_fee_amount) ? (fee - base_fee_amount) : 0;
            expected_total_priority += tx_priority_fee;
        }

        // Verify total priority fees
        uint64_t actual_total_priority = executor_->get_total_priority_fees();
        ASSERT_EQ(actual_total_priority, expected_total_priority)
            << "Total priority fees mismatch on trial " << trial;

        // Verify proposer balance increased by total priority fees
        uint64_t proposer_balance_after = account_manager_->get_account(proposer).balance;
        uint64_t proposer_increase = proposer_balance_after - proposer_balance_before;
        ASSERT_EQ(proposer_increase, expected_total_priority)
            << "Proposer balance increase mismatch on trial " << trial;
    }
}

/**
 * Property: Priority fee is zero when fee equals base fee
 * 
 * For any transaction where fee = base_fee * gas_limit,
 * the priority fee is zero.
 */
TEST_F(PriorityFeePaymentPropertyTest, ZeroPriorityFeeWhenFeeEqualsBaseFee) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        executor_->reset_counters();

        uint64_t amount = generate_random_amount();
        uint64_t base_fee = generate_random_base_fee();
        uint64_t gas_limit = generate_random_gas_limit();
        uint64_t fee = base_fee * gas_limit;  // Exactly base fee

        Address sender = create_account_with_balance(amount + fee + 10000);
        Address recipient = create_account_with_balance(0);
        Address proposer = create_account_with_balance(1000);

        uint64_t proposer_balance_before = account_manager_->get_account(proposer).balance;

        Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);
        executor_->execute_transaction(tx, 1, base_fee, proposer);

        // Verify proposer balance did not increase
        uint64_t proposer_balance_after = account_manager_->get_account(proposer).balance;
        ASSERT_EQ(proposer_balance_after, proposer_balance_before)
            << "Proposer balance should not increase when priority fee is zero on trial " << trial;

        // Verify total priority fees is zero
        uint64_t total_priority_fees = executor_->get_total_priority_fees();
        ASSERT_EQ(total_priority_fees, 0)
            << "Total priority fees should be zero on trial " << trial;
    }
}

/**
 * Property: Priority fee is independent of transaction amount
 * 
 * For any two transactions with the same fee but different amounts,
 * the priority fee paid is the same.
 */
TEST_F(PriorityFeePaymentPropertyTest, PriorityFeeIndependentOfAmount) {
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
        Address proposer1 = create_account_with_balance(1000);
        uint64_t proposer1_before = account_manager_->get_account(proposer1).balance;
        Transaction tx1(sender1, recipient1, amount1, 0, fee, gas_limit, 1);
        executor_->execute_transaction(tx1, 1, base_fee, proposer1);
        uint64_t proposer1_after = account_manager_->get_account(proposer1).balance;
        uint64_t priority1 = proposer1_after - proposer1_before;

        // Transaction 2
        executor_->reset_counters();
        Address sender2 = create_account_with_balance(amount2 + fee + 10000);
        Address recipient2 = create_account_with_balance(0);
        Address proposer2 = create_account_with_balance(1000);
        uint64_t proposer2_before = account_manager_->get_account(proposer2).balance;
        Transaction tx2(sender2, recipient2, amount2, 0, fee, gas_limit, 1);
        executor_->execute_transaction(tx2, 1, base_fee, proposer2);
        uint64_t proposer2_after = account_manager_->get_account(proposer2).balance;
        uint64_t priority2 = proposer2_after - proposer2_before;

        ASSERT_EQ(priority1, priority2)
            << "Priority fee should be independent of amount on trial " << trial
            << " (amount1=" << amount1 << ", amount2=" << amount2
            << ", priority1=" << priority1 << ", priority2=" << priority2 << ")";
    }
}

/**
 * Property: Higher fee results in higher priority fee
 * 
 * For any two transactions with the same base_fee and gas_limit,
 * if fee2 > fee1, then priority_fee2 >= priority_fee1.
 */
TEST_F(PriorityFeePaymentPropertyTest, HigherFeeResultsInHigherPriority) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t amount = generate_random_amount();
        uint64_t fee1 = generate_random_fee();
        uint64_t fee2 = fee1 + 100;  // Higher fee
        uint64_t base_fee = generate_random_base_fee();
        uint64_t gas_limit = generate_random_gas_limit();

        // Transaction 1
        executor_->reset_counters();
        Address sender1 = create_account_with_balance(amount + fee1 + 10000);
        Address recipient1 = create_account_with_balance(0);
        Address proposer1 = create_account_with_balance(1000);
        uint64_t proposer1_before = account_manager_->get_account(proposer1).balance;
        Transaction tx1(sender1, recipient1, amount, 0, fee1, gas_limit, 1);
        executor_->execute_transaction(tx1, 1, base_fee, proposer1);
        uint64_t proposer1_after = account_manager_->get_account(proposer1).balance;
        uint64_t priority1 = proposer1_after - proposer1_before;

        // Transaction 2
        executor_->reset_counters();
        Address sender2 = create_account_with_balance(amount + fee2 + 10000);
        Address recipient2 = create_account_with_balance(0);
        Address proposer2 = create_account_with_balance(1000);
        uint64_t proposer2_before = account_manager_->get_account(proposer2).balance;
        Transaction tx2(sender2, recipient2, amount, 0, fee2, gas_limit, 1);
        executor_->execute_transaction(tx2, 1, base_fee, proposer2);
        uint64_t proposer2_after = account_manager_->get_account(proposer2).balance;
        uint64_t priority2 = proposer2_after - proposer2_before;

        ASSERT_GE(priority2, priority1)
            << "Higher fee should result in higher priority fee on trial " << trial
            << " (fee1=" << fee1 << ", fee2=" << fee2
            << ", priority1=" << priority1 << ", priority2=" << priority2 << ")";
    }
}

/**
 * Property: Priority fee calculation is deterministic
 * 
 * For any transaction parameters, calculating the priority fee multiple times
 * yields the same result.
 */
TEST_F(PriorityFeePaymentPropertyTest, PriorityFeeCalculationIsDeterministic) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t amount = generate_random_amount();
        uint64_t fee = generate_random_fee();
        uint64_t base_fee = generate_random_base_fee();
        uint64_t gas_limit = generate_random_gas_limit();

        // Execute same transaction multiple times
        uint64_t priority1, priority2, priority3;

        {
            executor_->reset_counters();
            Address sender = create_account_with_balance(amount + fee + 10000);
            Address recipient = create_account_with_balance(0);
            Address proposer = create_account_with_balance(1000);
            uint64_t proposer_before = account_manager_->get_account(proposer).balance;
            Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);
            executor_->execute_transaction(tx, 1, base_fee, proposer);
            uint64_t proposer_after = account_manager_->get_account(proposer).balance;
            priority1 = proposer_after - proposer_before;
        }

        {
            executor_->reset_counters();
            Address sender = create_account_with_balance(amount + fee + 10000);
            Address recipient = create_account_with_balance(0);
            Address proposer = create_account_with_balance(1000);
            uint64_t proposer_before = account_manager_->get_account(proposer).balance;
            Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);
            executor_->execute_transaction(tx, 1, base_fee, proposer);
            uint64_t proposer_after = account_manager_->get_account(proposer).balance;
            priority2 = proposer_after - proposer_before;
        }

        {
            executor_->reset_counters();
            Address sender = create_account_with_balance(amount + fee + 10000);
            Address recipient = create_account_with_balance(0);
            Address proposer = create_account_with_balance(1000);
            uint64_t proposer_before = account_manager_->get_account(proposer).balance;
            Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);
            executor_->execute_transaction(tx, 1, base_fee, proposer);
            uint64_t proposer_after = account_manager_->get_account(proposer).balance;
            priority3 = proposer_after - proposer_before;
        }

        ASSERT_EQ(priority1, priority2)
            << "Priority fee calculation not deterministic (priority1 != priority2) on trial " << trial;
        ASSERT_EQ(priority2, priority3)
            << "Priority fee calculation not deterministic (priority2 != priority3) on trial " << trial;
    }
}

/**
 * Property: Total fee equals base fee plus priority fee
 * 
 * For any transaction, total_burned + total_priority_fees <= transaction_fee.
 */
TEST_F(PriorityFeePaymentPropertyTest, TotalFeeEqualsBasePlusPriority) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        executor_->reset_counters();

        uint64_t amount = generate_random_amount();
        uint64_t fee = generate_random_fee();
        uint64_t base_fee = generate_random_base_fee();
        uint64_t gas_limit = generate_random_gas_limit();

        Address sender = create_account_with_balance(amount + fee + 10000);
        Address recipient = create_account_with_balance(0);
        Address proposer = create_account_with_balance(1000);

        Transaction tx(sender, recipient, amount, 0, fee, gas_limit, 1);
        executor_->execute_transaction(tx, 1, base_fee, proposer);

        uint64_t total_burned = executor_->get_total_burned();
        uint64_t total_priority = executor_->get_total_priority_fees();

        ASSERT_EQ(total_burned + total_priority, fee)
            << "Total fee should equal base fee plus priority fee on trial " << trial
            << " (fee=" << fee << ", burned=" << total_burned
            << ", priority=" << total_priority << ")";
    }
}
