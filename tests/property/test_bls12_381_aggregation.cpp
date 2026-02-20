#include "sarafu/crypto/bls12_381.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <set>

using namespace sarafu::crypto;

/**
 * Property-Based Test for BLS12-381 Signature Aggregation
 * 
 * **Validates: Requirements 9.2, 9.3**
 * 
 * Property 25: BLS Signature Aggregation Size
 * For any set of BLS12-381 signatures from multiple validators signing the same message,
 * the aggregated signature is exactly 48 bytes.
 * 
 * Property 26: Aggregated Signature Verification
 * For any Quorum Certificate QC with aggregated signature S and signing validators V,
 * BLS_aggregate_verify(S, block_hash, V.public_keys) must return true.
 * 
 * This test validates that:
 * 1. Aggregated signatures have constant size (96 bytes) regardless of number of signers
 * 2. Aggregated signatures can be verified correctly against multiple public keys
 * 3. Aggregation works for any number of validators (1 to 500+)
 * 4. Invalid aggregated signatures are rejected
 * 5. Aggregated signatures are bound to the specific message
 */
class BLS12_381_AggregationPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random data of specified size
    std::vector<uint8_t> generate_random_data(size_t size) {
        std::vector<uint8_t> data(size);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return data;
    }

    // Generate random message of specified length
    std::string generate_random_message(size_t length) {
        std::string msg;
        msg.reserve(length);
        std::uniform_int_distribution<int> dist(32, 126); // Printable ASCII
        for (size_t i = 0; i < length; ++i) {
            msg += static_cast<char>(dist(rng_));
        }
        return msg;
    }

    std::mt19937 rng_;
};

/**
 * Property 25: BLS Signature Aggregation Size
 * 
 * For any number of validators N signing the same message,
 * the aggregated signature must be exactly 96 bytes (constant size).
 * This is the key property that makes BLS signatures efficient for light clients.
 */
TEST_F(BLS12_381_AggregationPropertyTest, AggregatedSignatureConstantSize) {
    const int NUM_TRIALS = 100;
    
    // Test with various numbers of signers
    std::vector<size_t> signer_counts = {
        1,      // Single signer (edge case)
        2,      // Minimum for aggregation
        3,      // Small group
        10,     // Medium group
        50,     // Large group
        100,    // Very large group (typical validator set)
        150,    // Target validator set size
        500     // Maximum validator set size
    };

    for (size_t num_signers : signer_counts) {
        for (size_t trial = 0; trial < NUM_TRIALS / signer_counts.size(); ++trial) {
            // Generate random message
            std::uniform_int_distribution<size_t> size_dist(32, 1024);
            size_t message_size = size_dist(rng_);
            std::vector<uint8_t> message = generate_random_data(message_size);
            
            // Generate keypairs for all signers
            std::vector<BLS12_381_PublicKey> public_keys;
            std::vector<BLS12_381_Signature> signatures;
            
            for (size_t i = 0; i < num_signers; ++i) {
                auto [public_key, private_key] = BLS12_381::generate_keypair();
                public_keys.push_back(public_key);
                
                // Each validator signs the same message
                BLS12_381_Signature signature = BLS12_381::sign(message, private_key);
                signatures.push_back(signature);
            }
            
            // Aggregate all signatures
            BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
            
            // Verify aggregated signature size is exactly 96 bytes
            ASSERT_EQ(aggregated.size(), 96)
                << "Aggregated signature size incorrect for " << num_signers << " signers";
            
            // Verify serialized size is also 96 bytes
            std::vector<uint8_t> serialized = aggregated.serialize();
            ASSERT_EQ(serialized.size(), 96)
                << "Serialized aggregated signature size incorrect for " << num_signers << " signers";
        }
    }
}

/**
 * Property 26: Aggregated Signature Verification
 * 
 * For any set of validators signing the same message, the aggregated signature
 * must verify correctly against all their public keys.
 */
TEST_F(BLS12_381_AggregationPropertyTest, AggregatedSignatureVerification) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random number of signers (1 to 100)
        std::uniform_int_distribution<size_t> signer_dist(1, 100);
        size_t num_signers = signer_dist(rng_);
        
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypairs and signatures
        std::vector<BLS12_381_PublicKey> public_keys;
        std::vector<BLS12_381_Signature> signatures;
        
        for (size_t i = 0; i < num_signers; ++i) {
            auto [public_key, private_key] = BLS12_381::generate_keypair();
            public_keys.push_back(public_key);
            
            BLS12_381_Signature signature = BLS12_381::sign(message, private_key);
            signatures.push_back(signature);
        }
        
        // Aggregate signatures
        BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
        
        // Verify aggregated signature
        bool is_valid = BLS12_381::verify_aggregated(aggregated, message, public_keys);
        
        ASSERT_TRUE(is_valid)
            << "Aggregated signature verification failed for " << num_signers << " signers";
    }
}

/**
 * Property: Aggregated signature verification fails with wrong message
 * 
 * An aggregated signature should only verify with the exact message
 * that was signed, not with any other message.
 */
TEST_F(BLS12_381_AggregationPropertyTest, AggregatedSignatureBoundToMessage) {
    const int NUM_TRIALS = 300;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random number of signers
        std::uniform_int_distribution<size_t> signer_dist(2, 50);
        size_t num_signers = signer_dist(rng_);
        
        // Generate two different messages
        std::uniform_int_distribution<size_t> size_dist(32, 512);
        size_t message_size = size_dist(rng_);
        
        std::vector<uint8_t> message1 = generate_random_data(message_size);
        std::vector<uint8_t> message2 = generate_random_data(message_size);
        
        // Ensure messages are different
        if (message1 == message2) {
            message2[0] ^= 0xFF;
        }
        
        // Generate keypairs and sign message1
        std::vector<BLS12_381_PublicKey> public_keys;
        std::vector<BLS12_381_Signature> signatures;
        
        for (size_t i = 0; i < num_signers; ++i) {
            auto [public_key, private_key] = BLS12_381::generate_keypair();
            public_keys.push_back(public_key);
            
            BLS12_381_Signature signature = BLS12_381::sign(message1, private_key);
            signatures.push_back(signature);
        }
        
        // Aggregate signatures
        BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
        
        // Verify with correct message (should succeed)
        ASSERT_TRUE(BLS12_381::verify_aggregated(aggregated, message1, public_keys))
            << "Aggregated signature verification failed with correct message";
        
        // Verify with wrong message (should fail)
        bool is_valid_wrong_message = BLS12_381::verify_aggregated(aggregated, message2, public_keys);
        
        ASSERT_FALSE(is_valid_wrong_message)
            << "Aggregated signature incorrectly verified with wrong message";
    }
}

/**
 * Property: Aggregated signature verification fails with wrong public keys
 * 
 * An aggregated signature should only verify with the exact set of
 * public keys that signed the message.
 */
TEST_F(BLS12_381_AggregationPropertyTest, AggregatedSignatureBoundToPublicKeys) {
    const int NUM_TRIALS = 300;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random number of signers
        std::uniform_int_distribution<size_t> signer_dist(2, 50);
        size_t num_signers = signer_dist(rng_);
        
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(32, 512);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypairs and sign
        std::vector<BLS12_381_PublicKey> correct_public_keys;
        std::vector<BLS12_381_PublicKey> wrong_public_keys;
        std::vector<BLS12_381_Signature> signatures;
        
        for (size_t i = 0; i < num_signers; ++i) {
            auto [public_key, private_key] = BLS12_381::generate_keypair();
            correct_public_keys.push_back(public_key);
            
            BLS12_381_Signature signature = BLS12_381::sign(message, private_key);
            signatures.push_back(signature);
            
            // Generate different public key
            auto [wrong_public_key, _] = BLS12_381::generate_keypair();
            wrong_public_keys.push_back(wrong_public_key);
        }
        
        // Aggregate signatures
        BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
        
        // Verify with correct public keys (should succeed)
        ASSERT_TRUE(BLS12_381::verify_aggregated(aggregated, message, correct_public_keys))
            << "Aggregated signature verification failed with correct public keys";
        
        // Verify with wrong public keys (should fail)
        bool is_valid_wrong_keys = BLS12_381::verify_aggregated(aggregated, message, wrong_public_keys);
        
        ASSERT_FALSE(is_valid_wrong_keys)
            << "Aggregated signature incorrectly verified with wrong public keys";
    }
}

/**
 * Property: Aggregated signature verification fails with subset of signers
 * 
 * An aggregated signature from N signers should not verify with only
 * a subset of the public keys (e.g., N-1 keys).
 */
TEST_F(BLS12_381_AggregationPropertyTest, AggregatedSignatureRequiresAllSigners) {
    const int NUM_TRIALS = 300;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random number of signers (at least 2)
        std::uniform_int_distribution<size_t> signer_dist(2, 50);
        size_t num_signers = signer_dist(rng_);
        
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(32, 512);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypairs and sign
        std::vector<BLS12_381_PublicKey> public_keys;
        std::vector<BLS12_381_Signature> signatures;
        
        for (size_t i = 0; i < num_signers; ++i) {
            auto [public_key, private_key] = BLS12_381::generate_keypair();
            public_keys.push_back(public_key);
            
            BLS12_381_Signature signature = BLS12_381::sign(message, private_key);
            signatures.push_back(signature);
        }
        
        // Aggregate all signatures
        BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
        
        // Verify with all public keys (should succeed)
        ASSERT_TRUE(BLS12_381::verify_aggregated(aggregated, message, public_keys))
            << "Aggregated signature verification failed with all public keys";
        
        // Create subset of public keys (remove last one)
        std::vector<BLS12_381_PublicKey> subset_keys(public_keys.begin(), public_keys.end() - 1);
        
        // Verify with subset (should fail)
        bool is_valid_subset = BLS12_381::verify_aggregated(aggregated, message, subset_keys);
        
        ASSERT_FALSE(is_valid_subset)
            << "Aggregated signature incorrectly verified with subset of public keys";
    }
}

/**
 * Property: Single signature aggregation works correctly
 * 
 * Aggregating a single signature should produce a valid aggregated signature
 * that verifies correctly (edge case).
 */
TEST_F(BLS12_381_AggregationPropertyTest, SingleSignatureAggregation) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate single keypair
        auto [public_key, private_key] = BLS12_381::generate_keypair();
        
        // Sign message
        BLS12_381_Signature signature = BLS12_381::sign(message, private_key);
        
        // Aggregate single signature
        std::vector<BLS12_381_Signature> signatures = {signature};
        BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
        
        // Verify aggregated signature
        std::vector<BLS12_381_PublicKey> public_keys = {public_key};
        bool is_valid = BLS12_381::verify_aggregated(aggregated, message, public_keys);
        
        ASSERT_TRUE(is_valid)
            << "Single signature aggregation verification failed";
        
        // Verify size is still 96 bytes
        ASSERT_EQ(aggregated.size(), 96)
            << "Single aggregated signature size incorrect";
    }
}

/**
 * Property: Aggregation order independence
 * 
 * The order in which signatures are aggregated should not affect
 * the final aggregated signature (commutativity).
 */
TEST_F(BLS12_381_AggregationPropertyTest, AggregationOrderIndependence) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random number of signers
        std::uniform_int_distribution<size_t> signer_dist(2, 20);
        size_t num_signers = signer_dist(rng_);
        
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(32, 512);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypairs and sign
        std::vector<BLS12_381_PublicKey> public_keys;
        std::vector<BLS12_381_Signature> signatures;
        
        for (size_t i = 0; i < num_signers; ++i) {
            auto [public_key, private_key] = BLS12_381::generate_keypair();
            public_keys.push_back(public_key);
            
            BLS12_381_Signature signature = BLS12_381::sign(message, private_key);
            signatures.push_back(signature);
        }
        
        // Aggregate in original order
        BLS12_381_Signature aggregated1 = BLS12_381::aggregate(signatures);
        
        // Shuffle signatures
        std::vector<BLS12_381_Signature> shuffled_signatures = signatures;
        std::shuffle(shuffled_signatures.begin(), shuffled_signatures.end(), rng_);
        
        // Aggregate in shuffled order
        BLS12_381_Signature aggregated2 = BLS12_381::aggregate(shuffled_signatures);
        
        // Both aggregated signatures should be equal
        ASSERT_EQ(aggregated1, aggregated2)
            << "Aggregation order affected the result";
        
        // Both should verify correctly
        ASSERT_TRUE(BLS12_381::verify_aggregated(aggregated1, message, public_keys));
        ASSERT_TRUE(BLS12_381::verify_aggregated(aggregated2, message, public_keys));
    }
}

/**
 * Property: Modified aggregated signature fails verification
 * 
 * If we modify any byte of an aggregated signature, verification should fail.
 */
TEST_F(BLS12_381_AggregationPropertyTest, ModifiedAggregatedSignatureFails) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random number of signers
        std::uniform_int_distribution<size_t> signer_dist(2, 20);
        size_t num_signers = signer_dist(rng_);
        
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(32, 512);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypairs and sign
        std::vector<BLS12_381_PublicKey> public_keys;
        std::vector<BLS12_381_Signature> signatures;
        
        for (size_t i = 0; i < num_signers; ++i) {
            auto [public_key, private_key] = BLS12_381::generate_keypair();
            public_keys.push_back(public_key);
            
            BLS12_381_Signature signature = BLS12_381::sign(message, private_key);
            signatures.push_back(signature);
        }
        
        // Aggregate signatures
        BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
        
        // Verify original (should succeed)
        ASSERT_TRUE(BLS12_381::verify_aggregated(aggregated, message, public_keys));
        
        // Modify the aggregated signature by flipping a random bit
        std::uniform_int_distribution<size_t> byte_dist(0, 95);
        std::uniform_int_distribution<int> bit_dist(0, 7);
        size_t byte_index = byte_dist(rng_);
        int bit_index = bit_dist(rng_);
        
        auto sig_data = aggregated.serialize();
        sig_data[byte_index] ^= (1 << bit_index);
        BLS12_381_Signature modified_signature(sig_data);
        
        // Verify modified signature (should fail)
        bool is_valid_modified = BLS12_381::verify_aggregated(modified_signature, message, public_keys);
        
        ASSERT_FALSE(is_valid_modified)
            << "Modified aggregated signature incorrectly verified";
    }
}

/**
 * Property: Aggregation works for maximum validator set size
 * 
 * The system should handle aggregation for up to 500 validators
 * (the maximum validator set size specified in requirements).
 */
TEST_F(BLS12_381_AggregationPropertyTest, MaximumValidatorSetSize) {
    const int NUM_TRIALS = 10; // Fewer trials due to computational cost

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        const size_t MAX_VALIDATORS = 500;
        
        // Generate random message
        std::vector<uint8_t> message = generate_random_data(256);
        
        // Generate keypairs and sign
        std::vector<BLS12_381_PublicKey> public_keys;
        std::vector<BLS12_381_Signature> signatures;
        
        for (size_t i = 0; i < MAX_VALIDATORS; ++i) {
            auto [public_key, private_key] = BLS12_381::generate_keypair();
            public_keys.push_back(public_key);
            
            BLS12_381_Signature signature = BLS12_381::sign(message, private_key);
            signatures.push_back(signature);
        }
        
        // Aggregate all 500 signatures
        BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
        
        // Verify size is still 96 bytes
        ASSERT_EQ(aggregated.size(), 96)
            << "Aggregated signature size incorrect for 500 validators";
        
        // Verify aggregated signature
        bool is_valid = BLS12_381::verify_aggregated(aggregated, message, public_keys);
        
        ASSERT_TRUE(is_valid)
            << "Aggregated signature verification failed for 500 validators";
    }
}

/**
 * Property: Aggregation with 2/3 supermajority
 * 
 * Test aggregation with exactly 2/3 of validators (the quorum threshold).
 * This simulates the minimum required for a Quorum Certificate.
 */
TEST_F(BLS12_381_AggregationPropertyTest, SupermajorityQuorum) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random total validator count
        std::uniform_int_distribution<size_t> total_dist(3, 150);
        size_t total_validators = total_dist(rng_);
        
        // Calculate 2/3 supermajority (rounded up)
        size_t supermajority = (total_validators * 2 + 2) / 3;
        
        // Generate random message
        std::vector<uint8_t> message = generate_random_data(256);
        
        // Generate keypairs for all validators
        std::vector<BLS12_381_PublicKey> all_public_keys;
        std::vector<std::pair<BLS12_381_PublicKey, BLS12_381_PrivateKey>> all_keypairs;
        
        for (size_t i = 0; i < total_validators; ++i) {
            auto keypair = BLS12_381::generate_keypair();
            all_public_keys.push_back(keypair.first);
            all_keypairs.push_back(keypair);
        }
        
        // Only supermajority signs
        std::vector<BLS12_381_PublicKey> signing_public_keys;
        std::vector<BLS12_381_Signature> signatures;
        
        for (size_t i = 0; i < supermajority; ++i) {
            signing_public_keys.push_back(all_keypairs[i].first);
            BLS12_381_Signature signature = BLS12_381::sign(message, all_keypairs[i].second);
            signatures.push_back(signature);
        }
        
        // Aggregate signatures from supermajority
        BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
        
        // Verify with signing public keys (should succeed)
        bool is_valid = BLS12_381::verify_aggregated(aggregated, message, signing_public_keys);
        
        ASSERT_TRUE(is_valid)
            << "Supermajority aggregated signature verification failed ("
            << supermajority << " out of " << total_validators << " validators)";
        
        // Verify size is 96 bytes
        ASSERT_EQ(aggregated.size(), 96);
    }
}

/**
 * Property: Aggregation serialization preserves validity
 * 
 * Serializing and deserializing an aggregated signature should
 * preserve its ability to verify correctly.
 */
TEST_F(BLS12_381_AggregationPropertyTest, AggregationSerializationPreservesValidity) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random number of signers
        std::uniform_int_distribution<size_t> signer_dist(2, 50);
        size_t num_signers = signer_dist(rng_);
        
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(32, 512);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypairs and sign
        std::vector<BLS12_381_PublicKey> public_keys;
        std::vector<BLS12_381_Signature> signatures;
        
        for (size_t i = 0; i < num_signers; ++i) {
            auto [public_key, private_key] = BLS12_381::generate_keypair();
            public_keys.push_back(public_key);
            
            BLS12_381_Signature signature = BLS12_381::sign(message, private_key);
            signatures.push_back(signature);
        }
        
        // Aggregate signatures
        BLS12_381_Signature original_aggregated = BLS12_381::aggregate(signatures);
        
        // Verify original
        ASSERT_TRUE(BLS12_381::verify_aggregated(original_aggregated, message, public_keys));
        
        // Serialize to bytes
        std::vector<uint8_t> serialized = original_aggregated.serialize();
        
        // Deserialize
        BLS12_381_Signature deserialized_aggregated(serialized);
        
        // Verify deserialized aggregated signature
        bool is_valid = BLS12_381::verify_aggregated(deserialized_aggregated, message, public_keys);
        
        ASSERT_TRUE(is_valid)
            << "Deserialized aggregated signature failed to verify";
        
        // Signatures should be equal
        ASSERT_EQ(original_aggregated, deserialized_aggregated);
    }
}
