#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>

using namespace sarafu::crypto;

/**
 * Property-Based Test for Ed25519 Signature Validity
 * 
 * **Validates: Requirements 8.1, 8.2**
 * 
 * Property 21: Ed25519 Signature Validity
 * For any transaction T, T is accepted only if Ed25519_verify(T.signature, T.hash(), T.from.public_key) returns true.
 * 
 * This test validates that:
 * 1. Valid signatures always verify correctly
 * 2. Invalid signatures are always rejected
 * 3. Signatures are bound to specific messages (cannot be reused)
 * 4. Signatures are bound to specific keys (cannot be forged)
 * 5. Signature verification works for all message sizes
 */
class Ed25519SignatureValidityPropertyTest : public ::testing::Test {
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
 * Property: Valid signatures always verify correctly
 * 
 * For any message M and keypair (pk, sk), if signature S = sign(M, sk),
 * then verify(S, M, pk) must return true.
 */
TEST_F(Ed25519SignatureValidityPropertyTest, ValidSignaturesVerify) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random message size between 0 and 10KB
        std::uniform_int_distribution<size_t> size_dist(0, 10240);
        size_t message_size = size_dist(rng_);
        
        // Generate random message
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Sign the message
        Ed25519_Signature signature = Ed25519::sign(message, private_key);
        
        // Verify the signature
        bool is_valid = Ed25519::verify(signature, message, public_key);
        
        ASSERT_TRUE(is_valid)
            << "Valid signature failed to verify for message size " << message_size;
    }
}

/**
 * Property: Signatures with wrong public key are rejected
 * 
 * For any message M, keypair (pk1, sk1), and different public key pk2,
 * if signature S = sign(M, sk1), then verify(S, M, pk2) must return false.
 */
TEST_F(Ed25519SignatureValidityPropertyTest, WrongPublicKeyRejected) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate two different keypairs
        auto [public_key1, private_key1] = Ed25519::generate_keypair();
        auto [public_key2, private_key2] = Ed25519::generate_keypair();
        
        // Ensure keys are different
        ASSERT_NE(public_key1, public_key2);
        
        // Sign with first keypair
        Ed25519_Signature signature = Ed25519::sign(message, private_key1);
        
        // Verify with correct key (should succeed)
        ASSERT_TRUE(Ed25519::verify(signature, message, public_key1))
            << "Signature verification failed with correct key";
        
        // Verify with wrong key (should fail)
        bool is_valid_with_wrong_key = Ed25519::verify(signature, message, public_key2);
        
        ASSERT_FALSE(is_valid_with_wrong_key)
            << "Signature incorrectly verified with wrong public key";
    }
}

/**
 * Property: Signatures are bound to specific messages
 * 
 * For any two different messages M1 and M2, keypair (pk, sk),
 * if signature S = sign(M1, sk), then verify(S, M2, pk) must return false.
 */
TEST_F(Ed25519SignatureValidityPropertyTest, SignaturesBoundToMessage) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate two different messages
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t message_size = size_dist(rng_);
        
        std::vector<uint8_t> message1 = generate_random_data(message_size);
        std::vector<uint8_t> message2 = generate_random_data(message_size);
        
        // Ensure messages are different
        if (message1 == message2) {
            message2[0] ^= 0xFF; // Flip bits to make different
        }
        
        // Generate keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Sign first message
        Ed25519_Signature signature = Ed25519::sign(message1, private_key);
        
        // Verify with correct message (should succeed)
        ASSERT_TRUE(Ed25519::verify(signature, message1, public_key))
            << "Signature verification failed with correct message";
        
        // Verify with different message (should fail)
        bool is_valid_with_wrong_message = Ed25519::verify(signature, message2, public_key);
        
        ASSERT_FALSE(is_valid_with_wrong_message)
            << "Signature incorrectly verified with different message";
    }
}

/**
 * Property: Modified signatures are rejected
 * 
 * For any message M, keypair (pk, sk), and signature S = sign(M, sk),
 * if we modify S to S', then verify(S', M, pk) must return false.
 */
TEST_F(Ed25519SignatureValidityPropertyTest, ModifiedSignaturesRejected) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Sign the message
        Ed25519_Signature signature = Ed25519::sign(message, private_key);
        
        // Verify original signature (should succeed)
        ASSERT_TRUE(Ed25519::verify(signature, message, public_key))
            << "Original signature verification failed";
        
        // Modify the signature by flipping a random bit
        std::uniform_int_distribution<size_t> byte_dist(0, Ed25519_Signature::SIGNATURE_SIZE - 1);
        std::uniform_int_distribution<int> bit_dist(0, 7);
        size_t byte_index = byte_dist(rng_);
        int bit_index = bit_dist(rng_);
        
        // Create modified signature
        auto signature_data = signature.serialize();
        signature_data[byte_index] ^= (1 << bit_index);
        Ed25519_Signature modified_signature(signature_data);
        
        // Verify modified signature (should fail)
        bool is_valid_modified = Ed25519::verify(modified_signature, message, public_key);
        
        ASSERT_FALSE(is_valid_modified)
            << "Modified signature incorrectly verified (bit " << bit_index
            << " in byte " << byte_index << " was flipped)";
    }
}

/**
 * Property: Signature verification works for all message sizes
 * 
 * Ed25519 should correctly sign and verify messages of any size,
 * from empty messages to very large messages.
 */
TEST_F(Ed25519SignatureValidityPropertyTest, WorksForAllMessageSizes) {
    // Test specific boundary sizes
    std::vector<size_t> test_sizes = {
        0,           // Empty message
        1,           // Single byte
        31,          // Just under key size
        32,          // Exactly key size
        33,          // Just over key size
        63,          // Just under 64 bytes
        64,          // Common block size
        127,         // Just under 128 bytes
        128,         // Another common block size
        255,         // Just under 256 bytes
        256,         // Power of 2
        1023,        // Just under 1KB
        1024,        // 1KB
        4095,        // Just under 4KB
        4096,        // 4KB (page size)
        8192,        // 8KB
        16384,       // 16KB
        65536,       // 64KB
        1048576      // 1MB
    };

    for (size_t size : test_sizes) {
        std::vector<uint8_t> message = generate_random_data(size);
        
        // Generate keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Sign the message
        Ed25519_Signature signature = Ed25519::sign(message, private_key);
        
        // Verify the signature
        bool is_valid = Ed25519::verify(signature, message, public_key);
        
        ASSERT_TRUE(is_valid)
            << "Signature verification failed for message size " << size;
        
        // Verify signature is always 64 bytes
        ASSERT_EQ(signature.size(), 64)
            << "Signature size incorrect for message size " << size;
    }
}

/**
 * Property: Signature verification is deterministic
 * 
 * Verifying the same signature multiple times should always
 * produce the same result.
 */
TEST_F(Ed25519SignatureValidityPropertyTest, VerificationIsDeterministic) {
    const int NUM_TRIALS = 500;
    const int NUM_REPETITIONS = 10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(0, 1024);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Sign the message
        Ed25519_Signature signature = Ed25519::sign(message, private_key);
        
        // Verify multiple times
        bool first_result = Ed25519::verify(signature, message, public_key);
        
        for (int rep = 1; rep < NUM_REPETITIONS; ++rep) {
            bool subsequent_result = Ed25519::verify(signature, message, public_key);
            
            ASSERT_EQ(first_result, subsequent_result)
                << "Verification result changed on repetition " << rep
                << " for message size " << message_size;
        }
    }
}

/**
 * Property: Different messages produce different signatures
 * 
 * For any two different messages M1 and M2, and keypair (pk, sk),
 * sign(M1, sk) should produce a different signature than sign(M2, sk)
 * with overwhelming probability.
 */
TEST_F(Ed25519SignatureValidityPropertyTest, DifferentMessagesProduceDifferentSignatures) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate two different messages
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t message_size = size_dist(rng_);
        
        std::vector<uint8_t> message1 = generate_random_data(message_size);
        std::vector<uint8_t> message2 = generate_random_data(message_size);
        
        // Ensure messages are different
        if (message1 == message2) {
            message2[0] ^= 0xFF;
        }
        
        // Generate keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Sign both messages
        Ed25519_Signature signature1 = Ed25519::sign(message1, private_key);
        Ed25519_Signature signature2 = Ed25519::sign(message2, private_key);
        
        // Signatures should be different
        ASSERT_NE(signature1, signature2)
            << "Different messages produced identical signatures";
        
        // Each signature should verify with its corresponding message
        ASSERT_TRUE(Ed25519::verify(signature1, message1, public_key));
        ASSERT_TRUE(Ed25519::verify(signature2, message2, public_key));
        
        // Cross-verification should fail
        ASSERT_FALSE(Ed25519::verify(signature1, message2, public_key));
        ASSERT_FALSE(Ed25519::verify(signature2, message1, public_key));
    }
}

/**
 * Property: Signature is deterministic for the same message and key
 * 
 * Signing the same message multiple times with the same key
 * should produce the same signature (Ed25519 is deterministic).
 */
TEST_F(Ed25519SignatureValidityPropertyTest, SigningIsDeterministic) {
    const int NUM_TRIALS = 500;
    const int NUM_REPETITIONS = 10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(0, 1024);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Sign multiple times
        Ed25519_Signature first_signature = Ed25519::sign(message, private_key);
        
        for (int rep = 1; rep < NUM_REPETITIONS; ++rep) {
            Ed25519_Signature subsequent_signature = Ed25519::sign(message, private_key);
            
            ASSERT_EQ(first_signature, subsequent_signature)
                << "Signature changed on repetition " << rep
                << " for message size " << message_size;
        }
    }
}

/**
 * Property: Public key extraction from private key is consistent
 * 
 * The public key extracted from a private key should always be
 * the same and should correctly verify signatures.
 */
TEST_F(Ed25519SignatureValidityPropertyTest, PublicKeyExtractionConsistent) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Extract public key from private key multiple times
        Ed25519_PublicKey extracted_key1 = private_key.public_key();
        Ed25519_PublicKey extracted_key2 = private_key.public_key();
        Ed25519_PublicKey extracted_key3 = private_key.public_key();
        
        // All extractions should be identical
        ASSERT_EQ(extracted_key1, extracted_key2);
        ASSERT_EQ(extracted_key2, extracted_key3);
        
        // Extracted key should match original public key
        ASSERT_EQ(public_key, extracted_key1)
            << "Extracted public key does not match original";
        
        // Sign and verify with extracted key
        std::vector<uint8_t> message = generate_random_data(100);
        Ed25519_Signature signature = Ed25519::sign(message, private_key);
        
        ASSERT_TRUE(Ed25519::verify(signature, message, extracted_key1))
            << "Signature verification failed with extracted public key";
    }
}

/**
 * Property: Signature serialization preserves validity
 * 
 * Serializing and deserializing a signature should preserve
 * its ability to verify correctly.
 */
TEST_F(Ed25519SignatureValidityPropertyTest, SerializationPreservesValidity) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random message
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t message_size = size_dist(rng_);
        std::vector<uint8_t> message = generate_random_data(message_size);
        
        // Generate keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Sign the message
        Ed25519_Signature original_signature = Ed25519::sign(message, private_key);
        
        // Verify original signature
        ASSERT_TRUE(Ed25519::verify(original_signature, message, public_key));
        
        // Serialize to bytes
        std::vector<uint8_t> serialized = original_signature.serialize();
        
        // Deserialize
        Ed25519_Signature deserialized_signature(serialized);
        
        // Verify deserialized signature
        bool is_valid = Ed25519::verify(deserialized_signature, message, public_key);
        
        ASSERT_TRUE(is_valid)
            << "Deserialized signature failed to verify";
        
        // Signatures should be equal
        ASSERT_EQ(original_signature, deserialized_signature);
    }
}

/**
 * Property: Empty message can be signed and verified
 * 
 * Ed25519 should handle empty messages correctly.
 */
TEST_F(Ed25519SignatureValidityPropertyTest, EmptyMessageHandling) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        std::vector<uint8_t> empty_message;
        
        // Generate keypair
        auto [public_key, private_key] = Ed25519::generate_keypair();
        
        // Sign empty message
        Ed25519_Signature signature = Ed25519::sign(empty_message, private_key);
        
        // Verify signature
        bool is_valid = Ed25519::verify(signature, empty_message, public_key);
        
        ASSERT_TRUE(is_valid)
            << "Empty message signature verification failed on trial " << trial;
        
        // Verify with non-empty message should fail
        std::vector<uint8_t> non_empty = {0x00};
        ASSERT_FALSE(Ed25519::verify(signature, non_empty, public_key))
            << "Empty message signature incorrectly verified with non-empty message";
    }
}
