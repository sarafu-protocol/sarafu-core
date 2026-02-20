#include "sarafu/state/transaction_validator.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/state/transaction.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Tests for Transaction Validation
 * 
 * **Validates: Requirements 7.4, 7.8, 8.3, 30.1, 30.2, 30.3, 30.4**
 * 
 * Property 18: Nonce Gap Rejection
 * Property 19: Insufficient Balance Rejection
 * Property 22: Invalid Signature Rejection
 * Property 61: Zero Address Rejection
 * Property 62: Non-Negative Amounts
 * 
 * This test suite validates that:
 * 1. Transactions with nonce gaps are rejected
 * 2. Transactions with insufficient balance are rejected
 * 3. Transactions with invalid signatures are rejected
 * 4. Transactions with zero addresses are rejected
 * 5. Transaction amounts are non-negative (always true for uint64_t)
 */
class TransactionValidationPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        account_manager_ = std::make_unique<AccountManager>();
        validator_ = std::make_unique<TransactionValidator>(CHAIN_ID);
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

    // Create a valid signed transaction
    Transaction create_signed_transaction(
        const Address& from,
        const Address& to,
        uint64_t amount,
        uint64_t nonce,
        uint64_t fee,
        const Ed25519_PrivateKey& private_key
    ) {
        Transaction tx(from, to, amount, nonce, fee, 21000, CHAIN_ID);
        tx.sign(private_key);
        return tx;
    }

    std::mt19937 rng_;
    std::unique_ptr<AccountManager> account_manager_;
    std::unique_ptr<TransactionValidator> validator_;
    static constexpr uint32_t CHAIN_ID = 1;
};

/**
 * Property 18: Nonce Gap Rejection
 * 
 * For any transaction T with nonce N submitted to account A with current nonce C,
 * if N ≠ C, the transaction is rejected.
 */
TEST_F(TransactionValidationPropertyTest, NonceGapRejection) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        // Generate keypair and address
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        // Create account with random balance
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        // Set account to random nonce
        std::uniform_int_distribution<int> nonce_dist(0, 100);
        int current_nonce = nonce_dist(rng_);
        for (int i = 0; i < current_nonce; ++i) {
            account_manager_->increment_nonce(address);
        }
        
        // Create transaction with wrong nonce
        std::uniform_int_distribution<int> gap_dist(-50, 50);
        int nonce_gap = gap_dist(rng_);
        
        // Skip if nonce happens to be correct
        if (nonce_gap == 0) {
            continue;
        }
        
        int wrong_nonce = current_nonce + nonce_gap;
        if (wrong_nonce < 0) {
            wrong_nonce = 0;
        }
        
        // Skip if nonce is accidentally correct
        if (static_cast<uint64_t>(wrong_nonce) == static_cast<uint64_t>(current_nonce)) {
            continue;
        }
        
        Address recipient = generate_random_address();
        uint64_t amount = balance / 10;
        uint64_t fee = 1000;
        
        Transaction tx = create_signed_transaction(
            address, recipient, amount, wrong_nonce, fee, private_key
        );
        
        // Validate transaction
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        ASSERT_TRUE(error.has_value())
            << "Transaction with nonce gap was not rejected on trial " << trial
            << " (current nonce: " << current_nonce << ", tx nonce: " << wrong_nonce << ")";
        
        ASSERT_NE(error->find("nonce"), std::string::npos)
            << "Error message does not mention nonce: " << *error;
    }
}

/**
 * Property 18 (continued): Correct nonce is accepted
 * 
 * For any transaction T with nonce N equal to account's current nonce,
 * the nonce check passes.
 */
TEST_F(TransactionValidationPropertyTest, CorrectNonceAccepted) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        // Set random nonce
        std::uniform_int_distribution<int> nonce_dist(0, 100);
        int current_nonce = nonce_dist(rng_);
        for (int i = 0; i < current_nonce; ++i) {
            account_manager_->increment_nonce(address);
        }
        
        Address recipient = generate_random_address();
        uint64_t amount = balance / 10;
        uint64_t fee = 1000;
        
        Transaction tx = create_signed_transaction(
            address, recipient, amount, current_nonce, fee, private_key
        );
        
        // Validate transaction
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        // Should not fail due to nonce (may fail for other reasons in edge cases)
        if (error.has_value()) {
            ASSERT_EQ(error->find("nonce"), std::string::npos)
                << "Transaction with correct nonce failed nonce check on trial " << trial
                << ": " << *error;
        }
    }
}

/**
 * Property 19: Insufficient Balance Rejection
 * 
 * For any transaction T from account A with amount M and fee F,
 * if A.balance < M + F, the transaction is rejected.
 */
TEST_F(TransactionValidationPropertyTest, InsufficientBalanceRejection) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        // Create account with limited balance
        uint64_t balance = generate_random_balance(1, 1000000);
        account_manager_->create_account(address, balance);
        
        Address recipient = generate_random_address();
        
        // Create transaction that exceeds balance
        uint64_t amount = balance + 1;
        uint64_t fee = generate_random_balance(1, 10000);
        
        Transaction tx = create_signed_transaction(
            address, recipient, amount, 0, fee, private_key
        );
        
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        ASSERT_TRUE(error.has_value())
            << "Transaction with insufficient balance was not rejected on trial " << trial
            << " (balance: " << balance << ", required: " << (amount + fee) << ")";
        
        ASSERT_NE(error->find("balance"), std::string::npos)
            << "Error message does not mention balance: " << *error;
    }
}

/**
 * Property 19 (continued): Sufficient balance is accepted
 * 
 * For any transaction T from account A with amount M and fee F,
 * if A.balance >= M + F, the balance check passes.
 */
TEST_F(TransactionValidationPropertyTest, SufficientBalanceAccepted) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        Address recipient = generate_random_address();
        
        // Create transaction within balance
        uint64_t amount = balance / 2;
        uint64_t fee = balance / 4;
        
        Transaction tx = create_signed_transaction(
            address, recipient, amount, 0, fee, private_key
        );
        
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        // Should not fail due to balance
        if (error.has_value()) {
            ASSERT_EQ(error->find("balance"), std::string::npos)
                << "Transaction with sufficient balance failed balance check on trial " << trial
                << ": " << *error;
        }
    }
}

/**
 * Property 22: Invalid Signature Rejection
 * 
 * For any transaction T with an invalid Ed25519 signature,
 * the transaction is rejected.
 */
TEST_F(TransactionValidationPropertyTest, InvalidSignatureRejection) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        // Generate two different keypairs
        auto [public_key1, private_key1] = Ed25519::generate_keypair();
        auto [public_key2, private_key2] = Ed25519::generate_keypair();
        
        Address address = Address::from_public_key(public_key1.serialize());
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        Address recipient = generate_random_address();
        uint64_t amount = balance / 10;
        uint64_t fee = 1000;
        
        // Sign with wrong key
        Transaction tx = create_signed_transaction(
            address, recipient, amount, 0, fee, private_key2
        );
        
        // Validate with correct public key
        auto error = validator_->validate_transaction(tx, public_key1, *account_manager_);
        
        ASSERT_TRUE(error.has_value())
            << "Transaction with invalid signature was not rejected on trial " << trial;
        
        ASSERT_NE(error->find("signature"), std::string::npos)
            << "Error message does not mention signature: " << *error;
    }
}

/**
 * Property 22 (continued): Valid signature is accepted
 * 
 * For any transaction T with a valid Ed25519 signature,
 * the signature check passes.
 */
TEST_F(TransactionValidationPropertyTest, ValidSignatureAccepted) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        Address recipient = generate_random_address();
        uint64_t amount = balance / 10;
        uint64_t fee = 1000;
        
        Transaction tx = create_signed_transaction(
            address, recipient, amount, 0, fee, private_key
        );
        
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        // Should not fail due to signature
        if (error.has_value()) {
            ASSERT_EQ(error->find("signature"), std::string::npos)
                << "Transaction with valid signature failed signature check on trial " << trial
                << ": " << *error;
        }
    }
}

/**
 * Property 22 (continued): Modified signature is rejected
 * 
 * For any valid transaction T, if the signature is modified,
 * the transaction is rejected.
 */
TEST_F(TransactionValidationPropertyTest, ModifiedSignatureRejection) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        Address recipient = generate_random_address();
        uint64_t amount = balance / 10;
        uint64_t fee = 1000;
        
        Transaction tx = create_signed_transaction(
            address, recipient, amount, 0, fee, private_key
        );
        
        // Modify signature by flipping a random bit
        std::uniform_int_distribution<size_t> byte_dist(0, Ed25519_Signature::SIGNATURE_SIZE - 1);
        std::uniform_int_distribution<int> bit_dist(0, 7);
        
        auto sig_data = tx.signature.serialize();
        size_t byte_index = byte_dist(rng_);
        int bit_index = bit_dist(rng_);
        sig_data[byte_index] ^= (1 << bit_index);
        
        tx.signature = Ed25519_Signature(sig_data);
        
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        ASSERT_TRUE(error.has_value())
            << "Transaction with modified signature was not rejected on trial " << trial;
        
        ASSERT_NE(error->find("signature"), std::string::npos)
            << "Error message does not mention signature: " << *error;
    }
}

/**
 * Property 61: Zero Address Rejection
 * 
 * For any transaction T, if T.from = zero_address OR T.to = zero_address,
 * the transaction is rejected.
 */
TEST_F(TransactionValidationPropertyTest, ZeroFromAddressRejection) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Use zero address as sender
        Address zero_address = Address::zero();
        Address recipient = generate_random_address();
        
        // Create account for zero address (for testing purposes)
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(zero_address, balance);
        
        uint64_t amount = balance / 10;
        uint64_t fee = 1000;
        
        Transaction tx = create_signed_transaction(
            zero_address, recipient, amount, 0, fee, private_key
        );
        
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        ASSERT_TRUE(error.has_value())
            << "Transaction with zero from address was not rejected on trial " << trial;
        
        ASSERT_NE(error->find("zero"), std::string::npos)
            << "Error message does not mention zero address: " << *error;
    }
}

/**
 * Property 61 (continued): Zero to address rejection
 */
TEST_F(TransactionValidationPropertyTest, ZeroToAddressRejection) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        // Use zero address as recipient
        Address zero_address = Address::zero();
        
        uint64_t amount = balance / 10;
        uint64_t fee = 1000;
        
        Transaction tx = create_signed_transaction(
            address, zero_address, amount, 0, fee, private_key
        );
        
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        ASSERT_TRUE(error.has_value())
            << "Transaction with zero to address was not rejected on trial " << trial;
        
        ASSERT_NE(error->find("zero"), std::string::npos)
            << "Error message does not mention zero address: " << *error;
    }
}

/**
 * Property 61 (continued): Non-zero addresses are accepted
 */
TEST_F(TransactionValidationPropertyTest, NonZeroAddressesAccepted) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        // Ensure address is not zero
        ASSERT_FALSE(address.is_zero());
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        Address recipient = generate_random_address();
        
        // Ensure recipient is not zero
        while (recipient.is_zero()) {
            recipient = generate_random_address();
        }
        
        uint64_t amount = balance / 10;
        uint64_t fee = 1000;
        
        Transaction tx = create_signed_transaction(
            address, recipient, amount, 0, fee, private_key
        );
        
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        // Should not fail due to zero address
        if (error.has_value()) {
            ASSERT_EQ(error->find("zero"), std::string::npos)
                << "Transaction with non-zero addresses failed zero address check on trial " << trial
                << ": " << *error;
        }
    }
}

/**
 * Property 62: Non-Negative Amounts
 * 
 * For any transaction T, T.amount >= 0 and T.fee >= 0.
 * 
 * Note: This is always true for uint64_t, but we test for completeness.
 */
TEST_F(TransactionValidationPropertyTest, NonNegativeAmounts) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        Address recipient = generate_random_address();
        
        // Generate random amounts (always non-negative for uint64_t)
        uint64_t amount = generate_random_balance(0, balance / 2);
        uint64_t fee = generate_random_balance(0, balance / 2);
        
        Transaction tx = create_signed_transaction(
            address, recipient, amount, 0, fee, private_key
        );
        
        // Verify amounts are non-negative
        ASSERT_GE(tx.amount, 0);
        ASSERT_GE(tx.fee, 0);
        
        // Validator should accept non-negative amounts
        ASSERT_TRUE(TransactionValidator::has_non_negative_amounts(tx))
            << "Non-negative amounts check failed on trial " << trial;
    }
}

/**
 * Property: Chain ID mismatch rejection
 * 
 * For any transaction T with chain_id != expected_chain_id,
 * the transaction is rejected.
 */
TEST_F(TransactionValidationPropertyTest, ChainIdMismatchRejection) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        Address recipient = generate_random_address();
        uint64_t amount = balance / 10;
        uint64_t fee = 1000;
        
        // Create transaction with wrong chain ID
        std::uniform_int_distribution<uint32_t> chain_dist(2, 1000);
        uint32_t wrong_chain_id = chain_dist(rng_);
        
        Transaction tx(address, recipient, amount, 0, fee, 21000, wrong_chain_id);
        tx.sign(private_key);
        
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        ASSERT_TRUE(error.has_value())
            << "Transaction with wrong chain ID was not rejected on trial " << trial
            << " (expected: " << CHAIN_ID << ", got: " << wrong_chain_id << ")";
        
        ASSERT_NE(error->find("chain"), std::string::npos)
            << "Error message does not mention chain ID: " << *error;
    }
}

/**
 * Property: Valid transactions pass all checks
 * 
 * For any transaction T that satisfies all validation rules,
 * validate_transaction returns std::nullopt (no error).
 */
TEST_F(TransactionValidationPropertyTest, ValidTransactionsPass) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        // Ensure address is not zero
        if (address.is_zero()) {
            continue;
        }
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        Address recipient = generate_random_address();
        
        // Ensure recipient is not zero
        while (recipient.is_zero()) {
            recipient = generate_random_address();
        }
        
        uint64_t amount = balance / 10;
        uint64_t fee = balance / 20;
        
        Transaction tx = create_signed_transaction(
            address, recipient, amount, 0, fee, private_key
        );
        
        auto error = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        ASSERT_FALSE(error.has_value())
            << "Valid transaction was rejected on trial " << trial
            << ": " << (error.has_value() ? *error : "");
    }
}

/**
 * Property: Multiple validation errors are detected
 * 
 * For any transaction with multiple validation failures,
 * at least one error is detected.
 */
TEST_F(TransactionValidationPropertyTest, MultipleErrorsDetected) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key1, private_key1] = Ed25519::generate_keypair();
        auto [public_key2, private_key2] = Ed25519::generate_keypair();
        
        Address address = Address::from_public_key(public_key1.serialize());
        
        uint64_t balance = generate_random_balance(1, 1000000);
        account_manager_->create_account(address, balance);
        
        // Create transaction with multiple errors:
        // 1. Wrong signature (signed with key2)
        // 2. Insufficient balance
        // 3. Wrong nonce
        
        Address recipient = generate_random_address();
        uint64_t amount = balance + 1000; // Exceeds balance
        uint64_t fee = 1000;
        uint64_t wrong_nonce = 999; // Wrong nonce
        
        Transaction tx(address, recipient, amount, wrong_nonce, fee, 21000, CHAIN_ID);
        tx.sign(private_key2); // Wrong key
        
        auto error = validator_->validate_transaction(tx, public_key1, *account_manager_);
        
        ASSERT_TRUE(error.has_value())
            << "Transaction with multiple errors was not rejected on trial " << trial;
        
        // At least one error should be mentioned
        bool has_error_message = 
            error->find("signature") != std::string::npos ||
            error->find("balance") != std::string::npos ||
            error->find("nonce") != std::string::npos;
        
        ASSERT_TRUE(has_error_message)
            << "Error message does not mention any validation failure: " << *error;
    }
}

/**
 * Property: Validation is deterministic
 * 
 * For any transaction T, validating it multiple times produces
 * the same result.
 */
TEST_F(TransactionValidationPropertyTest, ValidationIsDeterministic) {
    const int NUM_TRIALS = 500;
    const int NUM_REPETITIONS = 10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        account_manager_->clear();
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        Address address = Address::from_public_key(public_key.serialize());
        
        uint64_t balance = generate_random_balance();
        account_manager_->create_account(address, balance);
        
        Address recipient = generate_random_address();
        uint64_t amount = balance / 10;
        uint64_t fee = 1000;
        
        Transaction tx = create_signed_transaction(
            address, recipient, amount, 0, fee, private_key
        );
        
        // Validate multiple times
        auto first_result = validator_->validate_transaction(tx, public_key, *account_manager_);
        
        for (int rep = 1; rep < NUM_REPETITIONS; ++rep) {
            auto subsequent_result = validator_->validate_transaction(tx, public_key, *account_manager_);
            
            ASSERT_EQ(first_result.has_value(), subsequent_result.has_value())
                << "Validation result changed on repetition " << rep
                << " on trial " << trial;
            
            if (first_result.has_value() && subsequent_result.has_value()) {
                ASSERT_EQ(*first_result, *subsequent_result)
                    << "Error message changed on repetition " << rep
                    << " on trial " << trial;
            }
        }
    }
}
