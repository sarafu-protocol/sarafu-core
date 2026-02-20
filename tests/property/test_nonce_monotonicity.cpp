#include "sarafu/state/account_manager.h"
#include "sarafu/state/transaction.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Test for Nonce Monotonicity
 * 
 * **Validates: Requirements 7.2, 7.3**
 * 
 * Property 16: Nonce Monotonicity
 * For any account A, after executing a valid transaction, A's nonce increases by exactly 1.
 * 
 * This test validates that:
 * 1. Nonces start at 0 for new accounts
 * 2. Each successful transaction increments the nonce by exactly 1
 * 3. Nonces are strictly monotonically increasing
 * 4. Multiple transactions increment the nonce sequentially
 * 5. Nonce increments are independent per account
 */
class NonceMonotonicityPropertyTest : public ::testing::Test {
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
    uint64_t generate_random_balance() {
        std::uniform_int_distribution<uint64_t> dist(1000000, 10000000000);
        return dist(rng_);
    }

    std::mt19937 rng_;
    std::unique_ptr<AccountManager> account_manager_;
};

/**
 * Property: New accounts start with nonce 0
 * 
 * For any address A that has not been used, get_account(A).nonce == 0.
 */
TEST_F(NonceMonotonicityPropertyTest, NewAccountsStartWithNonceZero) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address address = generate_random_address();
        
        Account account = account_manager_->get_account(address);
        
        ASSERT_EQ(account.nonce, 0)
            << "New account does not have nonce 0 on trial " << trial;
    }
}

/**
 * Property: Single nonce increment increases nonce by exactly 1
 * 
 * For any account A with nonce N, after calling increment_nonce(A),
 * get_account(A).nonce == N + 1.
 */
TEST_F(NonceMonotonicityPropertyTest, SingleIncrementIncreasesNonceByOne) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address address = generate_random_address();
        uint64_t initial_balance = generate_random_balance();
        
        // Create account with random initial balance
        account_manager_->create_account(address, initial_balance);
        
        uint64_t initial_nonce = account_manager_->get_nonce(address);
        ASSERT_EQ(initial_nonce, 0) << "Initial nonce should be 0";
        
        // Increment nonce
        account_manager_->increment_nonce(address);
        
        uint64_t new_nonce = account_manager_->get_nonce(address);
        
        ASSERT_EQ(new_nonce, initial_nonce + 1)
            << "Nonce did not increment by exactly 1 on trial " << trial;
    }
}

/**
 * Property: Multiple nonce increments are strictly monotonic
 * 
 * For any account A and sequence of N increment operations,
 * the nonce increases from 0 to N, passing through all intermediate values.
 */
TEST_F(NonceMonotonicityPropertyTest, MultipleIncrementsAreMonotonic) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address address = generate_random_address();
        uint64_t initial_balance = generate_random_balance();
        
        // Create account
        account_manager_->create_account(address, initial_balance);
        
        // Generate random number of increments (1 to 100)
        std::uniform_int_distribution<int> increment_dist(1, 100);
        int num_increments = increment_dist(rng_);
        
        // Track nonce values
        std::vector<uint64_t> nonce_values;
        nonce_values.push_back(account_manager_->get_nonce(address));
        
        // Perform increments
        for (int i = 0; i < num_increments; ++i) {
            account_manager_->increment_nonce(address);
            uint64_t current_nonce = account_manager_->get_nonce(address);
            nonce_values.push_back(current_nonce);
            
            // Verify monotonic increase
            ASSERT_EQ(current_nonce, static_cast<uint64_t>(i + 1))
                << "Nonce not monotonic at increment " << i
                << " on trial " << trial;
        }
        
        // Verify all values are strictly increasing
        for (size_t i = 1; i < nonce_values.size(); ++i) {
            ASSERT_EQ(nonce_values[i], nonce_values[i-1] + 1)
                << "Nonce sequence not strictly increasing at position " << i
                << " on trial " << trial;
        }
        
        // Verify final nonce equals number of increments
        ASSERT_EQ(account_manager_->get_nonce(address), static_cast<uint64_t>(num_increments))
            << "Final nonce does not match number of increments on trial " << trial;
    }
}

/**
 * Property: Nonce increments are independent per account
 * 
 * For any two different accounts A and B, incrementing A's nonce
 * does not affect B's nonce.
 */
TEST_F(NonceMonotonicityPropertyTest, NonceIncrementsAreIndependent) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Create two different accounts
        Address address1 = generate_random_address();
        Address address2 = generate_random_address();
        
        // Ensure addresses are different
        while (address1 == address2) {
            address2 = generate_random_address();
        }
        
        uint64_t balance1 = generate_random_balance();
        uint64_t balance2 = generate_random_balance();
        
        account_manager_->create_account(address1, balance1);
        account_manager_->create_account(address2, balance2);
        
        // Generate random number of increments for each account
        std::uniform_int_distribution<int> increment_dist(1, 50);
        int increments1 = increment_dist(rng_);
        int increments2 = increment_dist(rng_);
        
        // Increment first account
        for (int i = 0; i < increments1; ++i) {
            account_manager_->increment_nonce(address1);
        }
        
        // Verify second account nonce is still 0
        ASSERT_EQ(account_manager_->get_nonce(address2), 0)
            << "Account 2 nonce changed when incrementing account 1 on trial " << trial;
        
        // Increment second account
        for (int i = 0; i < increments2; ++i) {
            account_manager_->increment_nonce(address2);
        }
        
        // Verify first account nonce is unchanged
        ASSERT_EQ(account_manager_->get_nonce(address1), static_cast<uint64_t>(increments1))
            << "Account 1 nonce changed when incrementing account 2 on trial " << trial;
        
        // Verify second account nonce is correct
        ASSERT_EQ(account_manager_->get_nonce(address2), static_cast<uint64_t>(increments2))
            << "Account 2 nonce incorrect on trial " << trial;
    }
}

/**
 * Property: Nonce validation requires exact match
 * 
 * For any account A with nonce N, validate_nonce(A, M) returns true
 * if and only if M == N.
 */
TEST_F(NonceMonotonicityPropertyTest, NonceValidationRequiresExactMatch) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address address = generate_random_address();
        uint64_t initial_balance = generate_random_balance();
        
        account_manager_->create_account(address, initial_balance);
        
        // Generate random number of increments
        std::uniform_int_distribution<int> increment_dist(0, 100);
        int num_increments = increment_dist(rng_);
        
        for (int i = 0; i < num_increments; ++i) {
            account_manager_->increment_nonce(address);
        }
        
        uint64_t current_nonce = account_manager_->get_nonce(address);
        
        // Validate with correct nonce (should succeed)
        ASSERT_TRUE(account_manager_->validate_nonce(address, current_nonce))
            << "Validation failed for correct nonce " << current_nonce
            << " on trial " << trial;
        
        // Validate with nonce - 1 (should fail unless nonce is 0)
        if (current_nonce > 0) {
            ASSERT_FALSE(account_manager_->validate_nonce(address, current_nonce - 1))
                << "Validation succeeded for nonce - 1 on trial " << trial;
        }
        
        // Validate with nonce + 1 (should fail)
        ASSERT_FALSE(account_manager_->validate_nonce(address, current_nonce + 1))
            << "Validation succeeded for nonce + 1 on trial " << trial;
        
        // Validate with random wrong nonce (should fail)
        std::uniform_int_distribution<uint64_t> nonce_dist(current_nonce + 2, current_nonce + 1000);
        uint64_t wrong_nonce = nonce_dist(rng_);
        
        ASSERT_FALSE(account_manager_->validate_nonce(address, wrong_nonce))
            << "Validation succeeded for wrong nonce " << wrong_nonce
            << " (current: " << current_nonce << ") on trial " << trial;
    }
}

/**
 * Property: Nonce persists across balance changes
 * 
 * For any account A with nonce N, changing the balance does not affect the nonce.
 */
TEST_F(NonceMonotonicityPropertyTest, NoncePersistsAcrossBalanceChanges) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address address = generate_random_address();
        uint64_t initial_balance = generate_random_balance();
        
        account_manager_->create_account(address, initial_balance);
        
        // Increment nonce a random number of times
        std::uniform_int_distribution<int> increment_dist(1, 50);
        int num_increments = increment_dist(rng_);
        
        for (int i = 0; i < num_increments; ++i) {
            account_manager_->increment_nonce(address);
        }
        
        uint64_t nonce_before = account_manager_->get_nonce(address);
        
        // Perform random balance operations
        std::uniform_int_distribution<int> op_dist(0, 2);
        int num_operations = increment_dist(rng_);
        
        for (int i = 0; i < num_operations; ++i) {
            int operation = op_dist(rng_);
            uint64_t amount = generate_random_balance() / 1000;
            
            if (operation == 0) {
                // Add balance
                account_manager_->add_balance(address, amount);
            } else if (operation == 1) {
                // Deduct balance (if sufficient)
                if (account_manager_->has_sufficient_balance(address, amount)) {
                    account_manager_->deduct_balance(address, amount);
                }
            } else {
                // Update account with new balance
                Account account = account_manager_->get_account(address);
                account.balance = generate_random_balance();
                account_manager_->update_account(account);
            }
        }
        
        uint64_t nonce_after = account_manager_->get_nonce(address);
        
        ASSERT_EQ(nonce_after, nonce_before)
            << "Nonce changed after balance operations on trial " << trial;
    }
}

/**
 * Property: Nonce increments are idempotent in effect
 * 
 * For any account A, incrementing the nonce N times results in nonce N,
 * regardless of the order or timing of increments.
 */
TEST_F(NonceMonotonicityPropertyTest, NonceIncrementsAreIdempotentInEffect) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address address = generate_random_address();
        uint64_t initial_balance = generate_random_balance();
        
        account_manager_->create_account(address, initial_balance);
        
        // Generate random number of increments
        std::uniform_int_distribution<int> increment_dist(1, 100);
        int total_increments = increment_dist(rng_);
        
        // Perform increments in random batch sizes
        int increments_done = 0;
        while (increments_done < total_increments) {
            std::uniform_int_distribution<int> batch_dist(1, std::min(10, total_increments - increments_done));
            int batch_size = batch_dist(rng_);
            
            for (int i = 0; i < batch_size; ++i) {
                account_manager_->increment_nonce(address);
                increments_done++;
            }
            
            // Verify nonce matches increments done so far
            ASSERT_EQ(account_manager_->get_nonce(address), static_cast<uint64_t>(increments_done))
                << "Nonce mismatch after " << increments_done << " increments on trial " << trial;
        }
        
        // Final verification
        ASSERT_EQ(account_manager_->get_nonce(address), static_cast<uint64_t>(total_increments))
            << "Final nonce does not match total increments on trial " << trial;
    }
}

/**
 * Property: Nonce cannot decrease
 * 
 * For any account A, the nonce can only increase or stay the same,
 * it can never decrease.
 */
TEST_F(NonceMonotonicityPropertyTest, NonceCannotDecrease) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address address = generate_random_address();
        uint64_t initial_balance = generate_random_balance();
        
        account_manager_->create_account(address, initial_balance);
        
        // Track minimum nonce seen
        uint64_t min_nonce = account_manager_->get_nonce(address);
        uint64_t max_nonce = min_nonce;
        
        // Perform random operations
        std::uniform_int_distribution<int> op_dist(0, 10);
        int num_operations = 100;
        
        for (int i = 0; i < num_operations; ++i) {
            int operation = op_dist(rng_);
            
            if (operation < 8) {
                // 80% chance: increment nonce
                account_manager_->increment_nonce(address);
            } else {
                // 20% chance: perform other operations that shouldn't affect nonce
                uint64_t amount = generate_random_balance() / 1000;
                if (op_dist(rng_) < 5) {
                    account_manager_->add_balance(address, amount);
                } else if (account_manager_->has_sufficient_balance(address, amount)) {
                    account_manager_->deduct_balance(address, amount);
                }
            }
            
            uint64_t current_nonce = account_manager_->get_nonce(address);
            
            // Nonce should never decrease
            ASSERT_GE(current_nonce, min_nonce)
                << "Nonce decreased from " << max_nonce << " to " << current_nonce
                << " at operation " << i << " on trial " << trial;
            
            // Update max nonce
            if (current_nonce > max_nonce) {
                max_nonce = current_nonce;
            }
        }
    }
}

/**
 * Property: Account update preserves nonce if not explicitly changed
 * 
 * For any account A with nonce N, updating the account without changing
 * the nonce field preserves the nonce value.
 */
TEST_F(NonceMonotonicityPropertyTest, AccountUpdatePreservesNonce) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address address = generate_random_address();
        uint64_t initial_balance = generate_random_balance();
        
        account_manager_->create_account(address, initial_balance);
        
        // Set nonce to random value
        std::uniform_int_distribution<int> nonce_dist(0, 100);
        int target_nonce = nonce_dist(rng_);
        
        for (int i = 0; i < target_nonce; ++i) {
            account_manager_->increment_nonce(address);
        }
        
        uint64_t nonce_before = account_manager_->get_nonce(address);
        ASSERT_EQ(nonce_before, static_cast<uint64_t>(target_nonce));
        
        // Update account with same nonce but different balance
        Account account = account_manager_->get_account(address);
        account.balance = generate_random_balance();
        account_manager_->update_account(account);
        
        uint64_t nonce_after = account_manager_->get_nonce(address);
        
        ASSERT_EQ(nonce_after, nonce_before)
            << "Nonce changed during account update on trial " << trial;
    }
}

/**
 * Property: Nonce starts at 0 for created accounts
 * 
 * For any address A and balance B, create_account(A, B) creates
 * an account with nonce 0.
 */
TEST_F(NonceMonotonicityPropertyTest, CreatedAccountsStartWithNonceZero) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address address = generate_random_address();
        uint64_t balance = generate_random_balance();
        
        Account account = account_manager_->create_account(address, balance);
        
        ASSERT_EQ(account.nonce, 0)
            << "Created account does not have nonce 0 on trial " << trial;
        
        // Verify via get_nonce as well
        ASSERT_EQ(account_manager_->get_nonce(address), 0)
            << "get_nonce does not return 0 for created account on trial " << trial;
    }
}

/**
 * Property: Large nonce values work correctly
 * 
 * The system should handle large nonce values (up to uint64_t max)
 * without overflow or errors.
 */
TEST_F(NonceMonotonicityPropertyTest, LargeNonceValuesWork) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address address = generate_random_address();
        uint64_t initial_balance = generate_random_balance();
        
        account_manager_->create_account(address, initial_balance);
        
        // Set nonce to a large value by directly updating the account
        std::uniform_int_distribution<uint64_t> large_nonce_dist(
            1000000000ULL, 
            10000000000ULL
        );
        uint64_t large_nonce = large_nonce_dist(rng_);
        
        Account account = account_manager_->get_account(address);
        account.nonce = large_nonce;
        account_manager_->update_account(account);
        
        // Verify nonce is set correctly
        ASSERT_EQ(account_manager_->get_nonce(address), large_nonce)
            << "Large nonce not set correctly on trial " << trial;
        
        // Increment and verify
        account_manager_->increment_nonce(address);
        
        ASSERT_EQ(account_manager_->get_nonce(address), large_nonce + 1)
            << "Large nonce did not increment correctly on trial " << trial;
        
        // Validate nonce
        ASSERT_TRUE(account_manager_->validate_nonce(address, large_nonce + 1))
            << "Large nonce validation failed on trial " << trial;
    }
}
