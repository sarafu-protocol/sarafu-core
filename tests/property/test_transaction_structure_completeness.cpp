#include "sarafu/state/transaction.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Test for Transaction Structure Completeness
 * 
 * **Validates: Requirements 7.5**
 * 
 * Property 20: Transaction Structure Completeness
 * For any valid transaction T, T must contain all required fields: 
 * from, to, amount, nonce, fee, gas_limit, chain_id, and signature.
 * 
 * This test validates that:
 * 1. All required fields are present in the transaction structure
 * 2. All fields are properly serialized and deserialized
 * 3. Serialization is complete (no data loss)
 * 4. Deserialization reconstructs the exact original transaction
 * 5. All fields maintain their values through serialization round-trip
 */
class TransactionStructureCompletenessPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
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

    // Generate random uint64_t
    uint64_t generate_random_uint64() {
        std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX);
        return dist(rng_);
    }

    // Generate random uint32_t
    uint32_t generate_random_uint32() {
        std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
        return dist(rng_);
    }

    // Generate random transaction with all fields populated
    Transaction generate_random_transaction() {
        Address from = generate_random_address();
        Address to = generate_random_address();
        uint64_t amount = generate_random_uint64();
        uint64_t nonce = generate_random_uint64();
        uint64_t fee = generate_random_uint64();
        uint64_t gas_limit = generate_random_uint64();
        uint32_t chain_id = generate_random_uint32();

        Transaction tx(from, to, amount, nonce, fee, gas_limit, chain_id);
        
        // Sign the transaction with a random keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        tx.sign(private_key);

        return tx;
    }

    std::mt19937 rng_;
};

/**
 * Property: All required fields are present and accessible
 * 
 * For any transaction T, all required fields (from, to, amount, nonce, 
 * fee, gas_limit, chain_id, signature) must be accessible.
 */
TEST_F(TransactionStructureCompletenessPropertyTest, AllFieldsAccessible) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Transaction tx = generate_random_transaction();

        // Verify all fields are accessible (compilation test + runtime check)
        ASSERT_NO_THROW({
            auto from = tx.from;
            auto to = tx.to;
            auto amount = tx.amount;
            auto nonce = tx.nonce;
            auto fee = tx.fee;
            auto gas_limit = tx.gas_limit;
            auto chain_id = tx.chain_id;
            auto signature = tx.signature;
            
            // Suppress unused variable warnings
            (void)from;
            (void)to;
            (void)amount;
            (void)nonce;
            (void)fee;
            (void)gas_limit;
            (void)chain_id;
            (void)signature;
        }) << "Failed to access transaction fields on trial " << trial;
    }
}

/**
 * Property: Serialization includes all fields
 * 
 * For any transaction T, the serialized form must include all fields.
 * The serialized size should be exactly 164 bytes:
 * - from: 32 bytes
 * - to: 32 bytes
 * - amount: 8 bytes
 * - nonce: 8 bytes
 * - fee: 8 bytes
 * - gas_limit: 8 bytes
 * - chain_id: 4 bytes
 * - signature: 64 bytes
 * Total: 164 bytes
 */
TEST_F(TransactionStructureCompletenessPropertyTest, SerializationIncludesAllFields) {
    const int NUM_TRIALS = 1000;
    const size_t EXPECTED_SIZE = 164; // 32+32+8+8+8+8+4+64

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Transaction tx = generate_random_transaction();

        // Serialize the transaction
        std::vector<uint8_t> serialized = tx.serialize();

        // Verify size is exactly 164 bytes
        ASSERT_EQ(serialized.size(), EXPECTED_SIZE)
            << "Serialized transaction size incorrect on trial " << trial
            << " (expected " << EXPECTED_SIZE << ", got " << serialized.size() << ")";
    }
}

/**
 * Property: Deserialization reconstructs all fields
 * 
 * For any transaction T, deserialize(serialize(T)) must equal T.
 * All fields must be preserved through the serialization round-trip.
 */
TEST_F(TransactionStructureCompletenessPropertyTest, DeserializationReconstructsAllFields) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Transaction original = generate_random_transaction();

        // Serialize
        std::vector<uint8_t> serialized = original.serialize();

        // Deserialize
        Transaction deserialized = Transaction::deserialize(serialized);

        // Verify all fields match
        ASSERT_EQ(original.from, deserialized.from)
            << "Field 'from' not preserved on trial " << trial;
        ASSERT_EQ(original.to, deserialized.to)
            << "Field 'to' not preserved on trial " << trial;
        ASSERT_EQ(original.amount, deserialized.amount)
            << "Field 'amount' not preserved on trial " << trial;
        ASSERT_EQ(original.nonce, deserialized.nonce)
            << "Field 'nonce' not preserved on trial " << trial;
        ASSERT_EQ(original.fee, deserialized.fee)
            << "Field 'fee' not preserved on trial " << trial;
        ASSERT_EQ(original.gas_limit, deserialized.gas_limit)
            << "Field 'gas_limit' not preserved on trial " << trial;
        ASSERT_EQ(original.chain_id, deserialized.chain_id)
            << "Field 'chain_id' not preserved on trial " << trial;
        ASSERT_EQ(original.signature, deserialized.signature)
            << "Field 'signature' not preserved on trial " << trial;

        // Verify equality operator works
        ASSERT_EQ(original, deserialized)
            << "Transaction equality check failed on trial " << trial;
    }
}

/**
 * Property: Serialization for signing excludes signature
 * 
 * For any transaction T, serialize_for_signing() must include all fields
 * EXCEPT the signature. The size should be exactly 100 bytes:
 * - from: 32 bytes
 * - to: 32 bytes
 * - amount: 8 bytes
 * - nonce: 8 bytes
 * - fee: 8 bytes
 * - gas_limit: 8 bytes
 * - chain_id: 4 bytes
 * Total: 100 bytes
 */
TEST_F(TransactionStructureCompletenessPropertyTest, SerializationForSigningExcludesSignature) {
    const int NUM_TRIALS = 1000;
    const size_t EXPECTED_SIZE = 100; // 32+32+8+8+8+8+4

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Transaction tx = generate_random_transaction();

        // Serialize for signing
        std::vector<uint8_t> serialized = tx.serialize_for_signing();

        // Verify size is exactly 100 bytes (without signature)
        ASSERT_EQ(serialized.size(), EXPECTED_SIZE)
            << "Serialized transaction (for signing) size incorrect on trial " << trial
            << " (expected " << EXPECTED_SIZE << ", got " << serialized.size() << ")";
    }
}

/**
 * Property: Transaction hash is deterministic and includes all fields
 * 
 * For any transaction T, hash(T) must be deterministic and must change
 * if any field (except signature) changes.
 */
TEST_F(TransactionStructureCompletenessPropertyTest, HashIncludesAllFields) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Transaction tx = generate_random_transaction();

        // Compute hash multiple times
        Blake3Hash hash1 = tx.hash();
        Blake3Hash hash2 = tx.hash();
        Blake3Hash hash3 = tx.hash();

        // Hash should be deterministic
        ASSERT_EQ(hash1, hash2)
            << "Transaction hash not deterministic on trial " << trial;
        ASSERT_EQ(hash2, hash3)
            << "Transaction hash not deterministic on trial " << trial;

        // Create modified transactions and verify hash changes
        Transaction tx_modified_from = tx;
        tx_modified_from.from = generate_random_address();
        ASSERT_NE(tx.hash(), tx_modified_from.hash())
            << "Hash did not change when 'from' field changed on trial " << trial;

        Transaction tx_modified_to = tx;
        tx_modified_to.to = generate_random_address();
        ASSERT_NE(tx.hash(), tx_modified_to.hash())
            << "Hash did not change when 'to' field changed on trial " << trial;

        Transaction tx_modified_amount = tx;
        tx_modified_amount.amount = tx.amount + 1;
        ASSERT_NE(tx.hash(), tx_modified_amount.hash())
            << "Hash did not change when 'amount' field changed on trial " << trial;

        Transaction tx_modified_nonce = tx;
        tx_modified_nonce.nonce = tx.nonce + 1;
        ASSERT_NE(tx.hash(), tx_modified_nonce.hash())
            << "Hash did not change when 'nonce' field changed on trial " << trial;

        Transaction tx_modified_fee = tx;
        tx_modified_fee.fee = tx.fee + 1;
        ASSERT_NE(tx.hash(), tx_modified_fee.hash())
            << "Hash did not change when 'fee' field changed on trial " << trial;

        Transaction tx_modified_gas_limit = tx;
        tx_modified_gas_limit.gas_limit = tx.gas_limit + 1;
        ASSERT_NE(tx.hash(), tx_modified_gas_limit.hash())
            << "Hash did not change when 'gas_limit' field changed on trial " << trial;

        Transaction tx_modified_chain_id = tx;
        tx_modified_chain_id.chain_id = tx.chain_id + 1;
        ASSERT_NE(tx.hash(), tx_modified_chain_id.hash())
            << "Hash did not change when 'chain_id' field changed on trial " << trial;
    }
}

/**
 * Property: Signature field does not affect transaction hash
 * 
 * For any transaction T, changing the signature should not change the hash.
 * This is critical because the hash is what gets signed.
 */
TEST_F(TransactionStructureCompletenessPropertyTest, SignatureDoesNotAffectHash) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Create transaction without signature
        Address from = generate_random_address();
        Address to = generate_random_address();
        uint64_t amount = generate_random_uint64();
        uint64_t nonce = generate_random_uint64();
        uint64_t fee = generate_random_uint64();
        uint64_t gas_limit = generate_random_uint64();
        uint32_t chain_id = generate_random_uint32();

        Transaction tx(from, to, amount, nonce, fee, gas_limit, chain_id);

        // Compute hash before signing
        Blake3Hash hash_before = tx.hash();

        // Sign with first keypair
        auto [public_key1, private_key1] = Ed25519::generate_keypair();
        tx.sign(private_key1);

        // Compute hash after signing
        Blake3Hash hash_after1 = tx.hash();

        // Hash should be the same
        ASSERT_EQ(hash_before, hash_after1)
            << "Hash changed after signing on trial " << trial;

        // Sign with different keypair
        auto [public_key2, private_key2] = Ed25519::generate_keypair();
        tx.sign(private_key2);

        // Compute hash after re-signing
        Blake3Hash hash_after2 = tx.hash();

        // Hash should still be the same
        ASSERT_EQ(hash_before, hash_after2)
            << "Hash changed after re-signing on trial " << trial;
        ASSERT_EQ(hash_after1, hash_after2)
            << "Hash changed between different signatures on trial " << trial;
    }
}

/**
 * Property: All fields support boundary values
 * 
 * For any transaction T, all numeric fields should support their full range
 * of values (0 to MAX) without errors.
 */
TEST_F(TransactionStructureCompletenessPropertyTest, FieldsSupportBoundaryValues) {
    // Test with minimum values (all zeros)
    {
        Address zero_addr = Address::zero();
        Transaction tx_min(zero_addr, zero_addr, 0, 0, 0, 0, 0);
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        tx_min.sign(private_key);

        // Should serialize and deserialize without error
        std::vector<uint8_t> serialized = tx_min.serialize();
        ASSERT_EQ(serialized.size(), 164);
        
        Transaction deserialized = Transaction::deserialize(serialized);
        ASSERT_EQ(tx_min, deserialized);
    }

    // Test with maximum values
    {
        Address max_addr = generate_random_address();
        Transaction tx_max(
            max_addr, 
            max_addr, 
            UINT64_MAX, 
            UINT64_MAX, 
            UINT64_MAX, 
            UINT64_MAX, 
            UINT32_MAX
        );
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        tx_max.sign(private_key);

        // Should serialize and deserialize without error
        std::vector<uint8_t> serialized = tx_max.serialize();
        ASSERT_EQ(serialized.size(), 164);
        
        Transaction deserialized = Transaction::deserialize(serialized);
        ASSERT_EQ(tx_max, deserialized);
    }

    // Test with mixed boundary values
    const int NUM_TRIALS = 100;
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        std::uniform_int_distribution<int> choice_dist(0, 1);
        
        Address from = choice_dist(rng_) ? Address::zero() : generate_random_address();
        Address to = choice_dist(rng_) ? Address::zero() : generate_random_address();
        uint64_t amount = choice_dist(rng_) ? 0 : UINT64_MAX;
        uint64_t nonce = choice_dist(rng_) ? 0 : UINT64_MAX;
        uint64_t fee = choice_dist(rng_) ? 0 : UINT64_MAX;
        uint64_t gas_limit = choice_dist(rng_) ? 0 : UINT64_MAX;
        uint32_t chain_id = choice_dist(rng_) ? 0 : UINT32_MAX;

        Transaction tx(from, to, amount, nonce, fee, gas_limit, chain_id);
        
        auto [public_key, private_key] = Ed25519::generate_keypair();
        tx.sign(private_key);

        // Should serialize and deserialize without error
        std::vector<uint8_t> serialized = tx.serialize();
        ASSERT_EQ(serialized.size(), 164);
        
        Transaction deserialized = Transaction::deserialize(serialized);
        ASSERT_EQ(tx, deserialized);
    }
}

/**
 * Property: Chain ID is included in transaction hash
 * 
 * For any two transactions T1 and T2 that differ only in chain_id,
 * their hashes must be different. This prevents cross-chain replay attacks.
 */
TEST_F(TransactionStructureCompletenessPropertyTest, ChainIdIncludedInHash) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Address from = generate_random_address();
        Address to = generate_random_address();
        uint64_t amount = generate_random_uint64();
        uint64_t nonce = generate_random_uint64();
        uint64_t fee = generate_random_uint64();
        uint64_t gas_limit = generate_random_uint64();

        // Create two transactions with different chain IDs
        uint32_t chain_id1 = generate_random_uint32();
        uint32_t chain_id2 = chain_id1 + 1; // Ensure different

        Transaction tx1(from, to, amount, nonce, fee, gas_limit, chain_id1);
        Transaction tx2(from, to, amount, nonce, fee, gas_limit, chain_id2);

        // Hashes must be different
        ASSERT_NE(tx1.hash(), tx2.hash())
            << "Transaction hashes identical despite different chain_id on trial " << trial;
    }
}

/**
 * Property: Deserialization rejects invalid data
 * 
 * For any data that is not a valid serialized transaction,
 * deserialization should throw an exception.
 */
TEST_F(TransactionStructureCompletenessPropertyTest, DeserializationRejectsInvalidData) {
    // Test with wrong size (too small)
    {
        std::vector<uint8_t> too_small(163); // One byte short
        ASSERT_THROW(Transaction::deserialize(too_small), std::invalid_argument)
            << "Deserialization should reject data that is too small";
    }

    // Test with wrong size (too large)
    {
        std::vector<uint8_t> too_large(165); // One byte extra
        ASSERT_THROW(Transaction::deserialize(too_large), std::invalid_argument)
            << "Deserialization should reject data that is too large";
    }

    // Test with empty data
    {
        std::vector<uint8_t> empty;
        ASSERT_THROW(Transaction::deserialize(empty), std::invalid_argument)
            << "Deserialization should reject empty data";
    }

    // Test with various wrong sizes
    std::vector<size_t> wrong_sizes = {1, 10, 50, 100, 150, 163, 165, 200, 1000};
    for (size_t size : wrong_sizes) {
        if (size == 164) continue; // Skip the correct size
        
        std::vector<uint8_t> wrong_size_data(size);
        ASSERT_THROW(Transaction::deserialize(wrong_size_data), std::invalid_argument)
            << "Deserialization should reject data of size " << size;
    }
}

/**
 * Property: Transaction equality is based on all fields
 * 
 * For any two transactions T1 and T2, T1 == T2 if and only if
 * all fields are equal.
 */
TEST_F(TransactionStructureCompletenessPropertyTest, EqualityBasedOnAllFields) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        Transaction tx1 = generate_random_transaction();
        Transaction tx2 = tx1; // Copy

        // Should be equal
        ASSERT_EQ(tx1, tx2)
            << "Copied transaction not equal on trial " << trial;
        ASSERT_FALSE(tx1 != tx2)
            << "Inequality operator incorrect on trial " << trial;

        // Modify each field and verify inequality
        Transaction tx_diff_from = tx1;
        tx_diff_from.from = generate_random_address();
        ASSERT_NE(tx1, tx_diff_from)
            << "Transactions equal despite different 'from' on trial " << trial;

        Transaction tx_diff_to = tx1;
        tx_diff_to.to = generate_random_address();
        ASSERT_NE(tx1, tx_diff_to)
            << "Transactions equal despite different 'to' on trial " << trial;

        Transaction tx_diff_amount = tx1;
        tx_diff_amount.amount = tx1.amount + 1;
        ASSERT_NE(tx1, tx_diff_amount)
            << "Transactions equal despite different 'amount' on trial " << trial;

        Transaction tx_diff_nonce = tx1;
        tx_diff_nonce.nonce = tx1.nonce + 1;
        ASSERT_NE(tx1, tx_diff_nonce)
            << "Transactions equal despite different 'nonce' on trial " << trial;

        Transaction tx_diff_fee = tx1;
        tx_diff_fee.fee = tx1.fee + 1;
        ASSERT_NE(tx1, tx_diff_fee)
            << "Transactions equal despite different 'fee' on trial " << trial;

        Transaction tx_diff_gas_limit = tx1;
        tx_diff_gas_limit.gas_limit = tx1.gas_limit + 1;
        ASSERT_NE(tx1, tx_diff_gas_limit)
            << "Transactions equal despite different 'gas_limit' on trial " << trial;

        Transaction tx_diff_chain_id = tx1;
        tx_diff_chain_id.chain_id = tx1.chain_id + 1;
        ASSERT_NE(tx1, tx_diff_chain_id)
            << "Transactions equal despite different 'chain_id' on trial " << trial;

        // Note: We don't test signature difference here because
        // signature is part of the transaction but doesn't affect
        // the logical identity for most purposes
    }
}
