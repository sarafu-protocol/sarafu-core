#include "sarafu/state/account_manager.h"
#include "sarafu/state/transaction.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Test for Balance Conservation
 * 
 * **Validates: Requirements 7.6, 7.7, 12.6**
 * 
 * Property 17: Balance Conservation
 * For any valid transaction T from A to B with amount M and fee F,
 * the sum of all account balances decreases by exactly F (the burned base fee),
 * with A decreasing by M+F and B increasing by M.
 * 
 * This test validates that:
 * 1. Sender balance decreases by amount + fee
 * 2. Recipient balance increases by amount
 * 3. Total balance decreases by fee (burned)
 * 4. No tokens are created or destroyed except for burned fees
 * 5. Balance conservation holds for all transaction amounts
 */
class BalanceConservationPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        account_manager_ = std::make_unique<AccountManager>();
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

    // Generate random balance
    uint64_t generate_random_balance(uint64_t min = 1000000, uint64_t max = 10000000000) {
        std::uniform_int_distribution<uint64_t> dist(min, max);
        return dist(rng_);
    }

    // Simulate transaction execution (simplified for testing)
    struct TransactionResult {
        bool success;
        uint64_t fee_burned;
    };

    TransactionResult execute_transaction(
        const Address& from,
        const Address& to,
        uint64_t amount,
        uint64_t fee
    ) {
        // Check sufficient balance
        if (!account_manager_->has_sufficient_balance(from, amount + fee)) {
            return {false, 0};
        }

        // Deduct from sender
        account_manager_->deduct_balance(from, amount + fee);
        
        // Add to recipient
        account_manager_->add_balance(to, amount);
        
        // Increment sender nonce
        account_manager_->increment_nonce(from);
        
        // Fee is burned (not added to anyone)
        return {true, fee};
    }

    // Calculate total balance across all accounts
    uint64_t calculate_total_balance() {
        uint64_t total = 0;
        for (const auto& [address, account] : account_manager_->get_all_accounts()) {
            total += account.balance;
        }
        return total;
    }

    std::mt19937 rng_;
    std::unique_ptr<AccountManager> account_manager_;
};

/**
 * Property: Sender balance decreases by amount + fee
 * 
 * For any transaction T from A to B with amount M and fee F,
 * A's balance decreases by exactly M + F.
 */
TEST_F(BalanceConservationPropertyTest, SenderBalanceDecreasesCorrectly) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        Address sender = generate_random_address();
        Address recipient = generate_random_address();
        
        // Ensure addresses are different
        while (sender == recipient) {
            recipient = generate_random_address();
        }
        
        uint64_t sender_initial_balance = generate_random_balance();
        uint64_t recipient_initial_balance = generate_random_balance();
        
        account_manager_->create_account(sender, sender_initial_balance);
        account_manager_->create_account(recipient, recipient_initial_balance);
        
        // Generate random transaction amount and fee
        uint64_t max_amount = sender_initial_balance / 2;
        uint64_t amount = generate_random_balance(1, max_amount);
        uint64_t fee = generate_random_balance(1, sender_initial_balance - amount);
        
        uint64_t sender_balance_before = account_manager_->get_balance(sender);
        
        auto result = execute_transaction(sender, recipient, amount, fee);
        
        ASSERT_TRUE(result.success)
            << "Transaction failed on trial " << trial;
        
        uint64_t sender_balance_after = account_manager_->get_balance(sender);
        uint64_t balance_decrease = sender_balance_before - sender_balance_after;
        
        ASSERT_EQ(balance_decrease, amount + fee)
            << "Sender balance did not decrease by amount + fee on trial " << trial
            << " (expected: " << (amount + fee) << ", actual: " << balance_decrease << ")";
    }
}

/**
 * Property: Recipient balance increases by amount
 * 
 * For any transaction T from A to B with amount M,
 * B's balance increases by exactly M.
 */
TEST_F(BalanceConservationPropertyTest, RecipientBalanceIncreasesCorrectly) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        Address sender = generate_random_address();
        Address recipient = generate_random_address();
        
        while (sender == recipient) {
            recipient = generate_random_address();
        }
        
        uint64_t sender_initial_balance = generate_random_balance();
        uint64_t recipient_initial_balance = generate_random_balance();
        
        account_manager_->create_account(sender, sender_initial_balance);
        account_manager_->create_account(recipient, recipient_initial_balance);
        
        uint64_t max_amount = sender_initial_balance / 2;
        uint64_t amount = generate_random_balance(1, max_amount);
        uint64_t fee = generate_random_balance(1, sender_initial_balance - amount);
        
        uint64_t recipient_balance_before = account_manager_->get_balance(recipient);
        
        auto result = execute_transaction(sender, recipient, amount, fee);
        
        ASSERT_TRUE(result.success);
        
        uint64_t recipient_balance_after = account_manager_->get_balance(recipient);
        uint64_t balance_increase = recipient_balance_after - recipient_balance_before;
        
        ASSERT_EQ(balance_increase, amount)
            << "Recipient balance did not increase by amount on trial " << trial
            << " (expected: " << amount << ", actual: " << balance_increase << ")";
    }
}

/**
 * Property: Total balance decreases by fee (burned)
 * 
 * For any transaction T with fee F, the sum of all account balances
 * decreases by exactly F.
 */
TEST_F(BalanceConservationPropertyTest, TotalBalanceDecreasesByFee) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        Address sender = generate_random_address();
        Address recipient = generate_random_address();
        
        while (sender == recipient) {
            recipient = generate_random_address();
        }
        
        uint64_t sender_initial_balance = generate_random_balance();
        uint64_t recipient_initial_balance = generate_random_balance();
        
        account_manager_->create_account(sender, sender_initial_balance);
        account_manager_->create_account(recipient, recipient_initial_balance);
        
        uint64_t total_balance_before = calculate_total_balance();
        
        uint64_t max_amount = sender_initial_balance / 2;
        uint64_t amount = generate_random_balance(1, max_amount);
        uint64_t fee = generate_random_balance(1, sender_initial_balance - amount);
        
        auto result = execute_transaction(sender, recipient, amount, fee);
        
        ASSERT_TRUE(result.success);
        
        uint64_t total_balance_after = calculate_total_balance();
        uint64_t total_decrease = total_balance_before - total_balance_after;
        
        ASSERT_EQ(total_decrease, fee)
            << "Total balance did not decrease by fee on trial " << trial
            << " (expected: " << fee << ", actual: " << total_decrease << ")";
    }
}

/**
 * Property: Balance conservation holds for multiple transactions
 * 
 * For any sequence of transactions, the total balance decrease
 * equals the sum of all fees burned.
 */
TEST_F(BalanceConservationPropertyTest, MultipleTransactionsConserveBalance) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        // Create multiple accounts
        std::uniform_int_distribution<int> account_dist(3, 10);
        int num_accounts = account_dist(rng_);
        
        std::vector<Address> addresses;
        for (int i = 0; i < num_accounts; ++i) {
            Address addr = generate_random_address();
            uint64_t balance = generate_random_balance();
            account_manager_->create_account(addr, balance);
            addresses.push_back(addr);
        }
        
        uint64_t total_balance_before = calculate_total_balance();
        uint64_t total_fees_burned = 0;
        
        // Execute random transactions
        std::uniform_int_distribution<int> tx_dist(5, 20);
        int num_transactions = tx_dist(rng_);
        
        for (int i = 0; i < num_transactions; ++i) {
            // Select random sender and recipient
            std::uniform_int_distribution<int> addr_dist(0, num_accounts - 1);
            int sender_idx = addr_dist(rng_);
            int recipient_idx = addr_dist(rng_);
            
            // Ensure different accounts
            while (sender_idx == recipient_idx) {
                recipient_idx = addr_dist(rng_);
            }
            
            Address sender = addresses[sender_idx];
            Address recipient = addresses[recipient_idx];
            
            uint64_t sender_balance = account_manager_->get_balance(sender);
            
            if (sender_balance < 2) {
                continue; // Skip if insufficient balance
            }
            
            uint64_t max_amount = sender_balance / 2;
            uint64_t amount = generate_random_balance(1, max_amount);
            uint64_t fee = generate_random_balance(1, sender_balance - amount);
            
            auto result = execute_transaction(sender, recipient, amount, fee);
            
            if (result.success) {
                total_fees_burned += result.fee_burned;
            }
        }
        
        uint64_t total_balance_after = calculate_total_balance();
        uint64_t total_decrease = total_balance_before - total_balance_after;
        
        ASSERT_EQ(total_decrease, total_fees_burned)
            << "Total balance decrease does not match fees burned on trial " << trial
            << " (expected: " << total_fees_burned << ", actual: " << total_decrease << ")";
    }
}

/**
 * Property: Zero amount transfers only burn fee
 * 
 * For any transaction with amount 0 and fee F,
 * sender balance decreases by F, recipient balance unchanged,
 * total balance decreases by F.
 */
TEST_F(BalanceConservationPropertyTest, ZeroAmountTransfersOnlyBurnFee) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        Address sender = generate_random_address();
        Address recipient = generate_random_address();
        
        while (sender == recipient) {
            recipient = generate_random_address();
        }
        
        uint64_t sender_initial_balance = generate_random_balance();
        uint64_t recipient_initial_balance = generate_random_balance();
        
        account_manager_->create_account(sender, sender_initial_balance);
        account_manager_->create_account(recipient, recipient_initial_balance);
        
        uint64_t fee = generate_random_balance(1, sender_initial_balance);
        uint64_t amount = 0;
        
        uint64_t sender_balance_before = account_manager_->get_balance(sender);
        uint64_t recipient_balance_before = account_manager_->get_balance(recipient);
        uint64_t total_balance_before = calculate_total_balance();
        
        auto result = execute_transaction(sender, recipient, amount, fee);
        
        ASSERT_TRUE(result.success);
        
        uint64_t sender_balance_after = account_manager_->get_balance(sender);
        uint64_t recipient_balance_after = account_manager_->get_balance(recipient);
        uint64_t total_balance_after = calculate_total_balance();
        
        // Sender decreases by fee only
        ASSERT_EQ(sender_balance_before - sender_balance_after, fee)
            << "Sender balance change incorrect for zero amount on trial " << trial;
        
        // Recipient unchanged
        ASSERT_EQ(recipient_balance_after, recipient_balance_before)
            << "Recipient balance changed for zero amount on trial " << trial;
        
        // Total decreases by fee
        ASSERT_EQ(total_balance_before - total_balance_after, fee)
            << "Total balance change incorrect for zero amount on trial " << trial;
    }
}

/**
 * Property: Self-transfers conserve balance correctly
 * 
 * For any transaction from A to A with amount M and fee F,
 * A's balance decreases by F (amount cancels out), total decreases by F.
 */
TEST_F(BalanceConservationPropertyTest, SelfTransfersConserveBalance) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        Address account = generate_random_address();
        uint64_t initial_balance = generate_random_balance();
        
        account_manager_->create_account(account, initial_balance);
        
        uint64_t max_amount = initial_balance / 2;
        uint64_t amount = generate_random_balance(1, max_amount);
        uint64_t fee = generate_random_balance(1, initial_balance - amount);
        
        uint64_t balance_before = account_manager_->get_balance(account);
        uint64_t total_balance_before = calculate_total_balance();
        
        auto result = execute_transaction(account, account, amount, fee);
        
        ASSERT_TRUE(result.success);
        
        uint64_t balance_after = account_manager_->get_balance(account);
        uint64_t total_balance_after = calculate_total_balance();
        
        // Balance decreases by fee only (amount sent to self)
        ASSERT_EQ(balance_before - balance_after, fee)
            << "Self-transfer balance change incorrect on trial " << trial;
        
        // Total decreases by fee
        ASSERT_EQ(total_balance_before - total_balance_after, fee)
            << "Self-transfer total balance change incorrect on trial " << trial;
    }
}

/**
 * Property: Insufficient balance transactions fail without changing balances
 * 
 * For any transaction where sender balance < amount + fee,
 * the transaction fails and no balances change.
 */
TEST_F(BalanceConservationPropertyTest, InsufficientBalanceTransactionsFail) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        Address sender = generate_random_address();
        Address recipient = generate_random_address();
        
        while (sender == recipient) {
            recipient = generate_random_address();
        }
        
        uint64_t sender_initial_balance = generate_random_balance(1, 1000000);
        uint64_t recipient_initial_balance = generate_random_balance();
        
        account_manager_->create_account(sender, sender_initial_balance);
        account_manager_->create_account(recipient, recipient_initial_balance);
        
        // Create transaction that exceeds balance
        uint64_t amount = sender_initial_balance + 1;
        uint64_t fee = generate_random_balance(1, 1000);
        
        uint64_t sender_balance_before = account_manager_->get_balance(sender);
        uint64_t recipient_balance_before = account_manager_->get_balance(recipient);
        uint64_t total_balance_before = calculate_total_balance();
        
        auto result = execute_transaction(sender, recipient, amount, fee);
        
        ASSERT_FALSE(result.success)
            << "Transaction with insufficient balance succeeded on trial " << trial;
        
        uint64_t sender_balance_after = account_manager_->get_balance(sender);
        uint64_t recipient_balance_after = account_manager_->get_balance(recipient);
        uint64_t total_balance_after = calculate_total_balance();
        
        // No balances should change
        ASSERT_EQ(sender_balance_after, sender_balance_before)
            << "Sender balance changed on failed transaction on trial " << trial;
        
        ASSERT_EQ(recipient_balance_after, recipient_balance_before)
            << "Recipient balance changed on failed transaction on trial " << trial;
        
        ASSERT_EQ(total_balance_after, total_balance_before)
            << "Total balance changed on failed transaction on trial " << trial;
    }
}

/**
 * Property: Maximum amount transfers work correctly
 * 
 * For any account with balance B, a transaction with amount = B - fee
 * should succeed and leave sender with balance 0.
 */
TEST_F(BalanceConservationPropertyTest, MaximumAmountTransfersWork) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        Address sender = generate_random_address();
        Address recipient = generate_random_address();
        
        while (sender == recipient) {
            recipient = generate_random_address();
        }
        
        uint64_t sender_initial_balance = generate_random_balance();
        uint64_t recipient_initial_balance = generate_random_balance();
        
        account_manager_->create_account(sender, sender_initial_balance);
        account_manager_->create_account(recipient, recipient_initial_balance);
        
        // Transfer maximum possible amount
        uint64_t fee = generate_random_balance(1, sender_initial_balance / 2);
        uint64_t amount = sender_initial_balance - fee;
        
        uint64_t recipient_balance_before = account_manager_->get_balance(recipient);
        uint64_t total_balance_before = calculate_total_balance();
        
        auto result = execute_transaction(sender, recipient, amount, fee);
        
        ASSERT_TRUE(result.success)
            << "Maximum amount transfer failed on trial " << trial;
        
        uint64_t sender_balance_after = account_manager_->get_balance(sender);
        uint64_t recipient_balance_after = account_manager_->get_balance(recipient);
        uint64_t total_balance_after = calculate_total_balance();
        
        // Sender should have 0 balance
        ASSERT_EQ(sender_balance_after, 0)
            << "Sender balance not zero after maximum transfer on trial " << trial;
        
        // Recipient increases by amount
        ASSERT_EQ(recipient_balance_after - recipient_balance_before, amount)
            << "Recipient balance increase incorrect on trial " << trial;
        
        // Total decreases by fee
        ASSERT_EQ(total_balance_before - total_balance_after, fee)
            << "Total balance change incorrect on trial " << trial;
    }
}

/**
 * Property: Balance conservation is independent of transaction order
 * 
 * For any set of valid transactions, the final total balance decrease
 * equals the sum of fees regardless of execution order.
 */
TEST_F(BalanceConservationPropertyTest, BalanceConservationIndependentOfOrder) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Create accounts
        std::vector<Address> addresses;
        std::vector<uint64_t> initial_balances;
        
        for (int i = 0; i < 5; ++i) {
            Address addr = generate_random_address();
            uint64_t balance = generate_random_balance();
            addresses.push_back(addr);
            initial_balances.push_back(balance);
        }
        
        // Create transaction sequence
        struct TxData {
            int sender_idx;
            int recipient_idx;
            uint64_t amount;
            uint64_t fee;
        };
        
        std::vector<TxData> transactions;
        for (int i = 0; i < 10; ++i) {
            std::uniform_int_distribution<int> addr_dist(0, 4);
            int sender_idx = addr_dist(rng_);
            int recipient_idx = addr_dist(rng_);
            
            while (sender_idx == recipient_idx) {
                recipient_idx = addr_dist(rng_);
            }
            
            uint64_t amount = generate_random_balance(1, 100000);
            uint64_t fee = generate_random_balance(1, 10000);
            
            transactions.push_back({sender_idx, recipient_idx, amount, fee});
        }
        
        // Execute in original order
        account_manager_->clear();
        for (size_t i = 0; i < addresses.size(); ++i) {
            account_manager_->create_account(addresses[i], initial_balances[i]);
        }
        
        uint64_t total_before_1 = calculate_total_balance();
        uint64_t fees_burned_1 = 0;
        
        for (const auto& tx : transactions) {
            auto result = execute_transaction(
                addresses[tx.sender_idx],
                addresses[tx.recipient_idx],
                tx.amount,
                tx.fee
            );
            if (result.success) {
                fees_burned_1 += result.fee_burned;
            }
        }
        
        uint64_t total_after_1 = calculate_total_balance();
        uint64_t decrease_1 = total_before_1 - total_after_1;
        
        // Execute in shuffled order
        account_manager_->clear();
        for (size_t i = 0; i < addresses.size(); ++i) {
            account_manager_->create_account(addresses[i], initial_balances[i]);
        }
        
        std::vector<TxData> shuffled_transactions = transactions;
        std::shuffle(shuffled_transactions.begin(), shuffled_transactions.end(), rng_);
        
        uint64_t total_before_2 = calculate_total_balance();
        uint64_t fees_burned_2 = 0;
        
        for (const auto& tx : shuffled_transactions) {
            auto result = execute_transaction(
                addresses[tx.sender_idx],
                addresses[tx.recipient_idx],
                tx.amount,
                tx.fee
            );
            if (result.success) {
                fees_burned_2 += result.fee_burned;
            }
        }
        
        uint64_t total_after_2 = calculate_total_balance();
        uint64_t decrease_2 = total_before_2 - total_after_2;
        
        // Both should conserve balance correctly
        ASSERT_EQ(decrease_1, fees_burned_1)
            << "Balance conservation failed in original order on trial " << trial;
        
        ASSERT_EQ(decrease_2, fees_burned_2)
            << "Balance conservation failed in shuffled order on trial " << trial;
    }
}

/**
 * Property: Large balance values work correctly
 * 
 * Balance conservation should work for very large balance values
 * (up to uint64_t max).
 */
TEST_F(BalanceConservationPropertyTest, LargeBalanceValuesWork) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        Address sender = generate_random_address();
        Address recipient = generate_random_address();
        
        while (sender == recipient) {
            recipient = generate_random_address();
        }
        
        // Use very large balances
        std::uniform_int_distribution<uint64_t> large_dist(
            1000000000000ULL,
            10000000000000ULL
        );
        
        uint64_t sender_initial_balance = large_dist(rng_);
        uint64_t recipient_initial_balance = large_dist(rng_);
        
        account_manager_->create_account(sender, sender_initial_balance);
        account_manager_->create_account(recipient, recipient_initial_balance);
        
        uint64_t amount = large_dist(rng_) / 10;
        uint64_t fee = generate_random_balance(1, 1000000);
        
        uint64_t total_balance_before = calculate_total_balance();
        
        auto result = execute_transaction(sender, recipient, amount, fee);
        
        ASSERT_TRUE(result.success)
            << "Large balance transaction failed on trial " << trial;
        
        uint64_t total_balance_after = calculate_total_balance();
        uint64_t total_decrease = total_balance_before - total_balance_after;
        
        ASSERT_EQ(total_decrease, fee)
            << "Balance conservation failed for large values on trial " << trial;
    }
}
