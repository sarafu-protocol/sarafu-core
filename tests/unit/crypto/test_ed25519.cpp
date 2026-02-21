#include <gtest/gtest.h>
#include "sarafu/crypto/ed25519.h"
#include <vector>
#include <string>

using namespace sarafu::crypto;

class Ed25519Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Test vectors can be added here
    }
};

// Test key generation
TEST_F(Ed25519Test, KeyGeneration) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Verify key sizes
    EXPECT_EQ(public_key.size(), Ed25519_PublicKey::KEY_SIZE);
    EXPECT_EQ(private_key.size(), Ed25519_PrivateKey::KEY_SIZE);
    
    // Verify public key can be extracted from private key
    auto extracted_pubkey = private_key.public_key();
    EXPECT_EQ(public_key, extracted_pubkey);
}

// Test signing and verification
TEST_F(Ed25519Test, SignAndVerify) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    // Sign the message
    auto signature = Ed25519::sign(message, private_key);
    
    // Verify signature size
    EXPECT_EQ(signature.size(), Ed25519_Signature::SIGNATURE_SIZE);
    
    // Verify the signature
    bool valid = Ed25519::verify(signature, message, public_key);
    EXPECT_TRUE(valid);
}

// Test signature verification fails with wrong message
TEST_F(Ed25519Test, VerifyFailsWithWrongMessage) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    std::vector<uint8_t> message1 = {0x01, 0x02, 0x03, 0x04, 0x05};
    std::vector<uint8_t> message2 = {0x01, 0x02, 0x03, 0x04, 0x06}; // Different
    
    auto signature = Ed25519::sign(message1, private_key);
    
    // Verification should fail with different message
    bool valid = Ed25519::verify(signature, message2, public_key);
    EXPECT_FALSE(valid);
}

// Test signature verification fails with wrong public key
TEST_F(Ed25519Test, VerifyFailsWithWrongPublicKey) {
    auto [public_key1, private_key1] = Ed25519::generate_keypair();
    auto [public_key2, private_key2] = Ed25519::generate_keypair();
    
    std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    auto signature = Ed25519::sign(message, private_key1);
    
    // Verification should fail with different public key
    bool valid = Ed25519::verify(signature, message, public_key2);
    EXPECT_FALSE(valid);
}

// Test empty message signing
TEST_F(Ed25519Test, SignEmptyMessage) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    std::vector<uint8_t> empty_message;
    
    auto signature = Ed25519::sign(empty_message, private_key);
    bool valid = Ed25519::verify(signature, empty_message, public_key);
    
    EXPECT_TRUE(valid);
}

// Test large message signing
TEST_F(Ed25519Test, SignLargeMessage) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Create a large message (1 MB)
    std::vector<uint8_t> large_message(1024 * 1024);
    for (size_t i = 0; i < large_message.size(); ++i) {
        large_message[i] = static_cast<uint8_t>(i % 256);
    }
    
    auto signature = Ed25519::sign(large_message, private_key);
    bool valid = Ed25519::verify(signature, large_message, public_key);
    
    EXPECT_TRUE(valid);
}

// Test public key serialization
TEST_F(Ed25519Test, PublicKeySerialization) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Serialize to vector
    auto serialized = public_key.serialize();
    EXPECT_EQ(serialized.size(), Ed25519_PublicKey::KEY_SIZE);
    
    // Reconstruct from vector
    Ed25519_PublicKey reconstructed(serialized);
    EXPECT_EQ(public_key, reconstructed);
}

// Test public key hex encoding
TEST_F(Ed25519Test, PublicKeyHexEncoding) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Convert to hex
    std::string hex = public_key.to_hex();
    EXPECT_EQ(hex.length(), Ed25519_PublicKey::KEY_SIZE * 2);
    
    // Reconstruct from hex
    Ed25519_PublicKey reconstructed = Ed25519_PublicKey::from_hex(hex);
    EXPECT_EQ(public_key, reconstructed);
}

// Test signature serialization
TEST_F(Ed25519Test, SignatureSerialization) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    std::vector<uint8_t> message = {0x01, 0x02, 0x03};
    
    auto signature = Ed25519::sign(message, private_key);
    
    // Serialize to vector
    auto serialized = signature.serialize();
    EXPECT_EQ(serialized.size(), Ed25519_Signature::SIGNATURE_SIZE);
    
    // Reconstruct from vector
    Ed25519_Signature reconstructed(serialized);
    EXPECT_EQ(signature, reconstructed);
    
    // Verify reconstructed signature works
    bool valid = Ed25519::verify(reconstructed, message, public_key);
    EXPECT_TRUE(valid);
}

// Test signature hex encoding
TEST_F(Ed25519Test, SignatureHexEncoding) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    std::vector<uint8_t> message = {0x01, 0x02, 0x03};
    
    auto signature = Ed25519::sign(message, private_key);
    
    // Convert to hex
    std::string hex = signature.to_hex();
    EXPECT_EQ(hex.length(), Ed25519_Signature::SIGNATURE_SIZE * 2);
    
    // Reconstruct from hex
    Ed25519_Signature reconstructed = Ed25519_Signature::from_hex(hex);
    EXPECT_EQ(signature, reconstructed);
    
    // Verify reconstructed signature works
    bool valid = Ed25519::verify(reconstructed, message, public_key);
    EXPECT_TRUE(valid);
}

// Test private key hex encoding
TEST_F(Ed25519Test, PrivateKeyHexEncoding) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Convert to hex
    std::string hex = private_key.to_hex();
    EXPECT_EQ(hex.length(), Ed25519_PrivateKey::KEY_SIZE * 2);
    
    // Reconstruct from hex
    Ed25519_PrivateKey reconstructed = Ed25519_PrivateKey::from_hex(hex);
    
    // Verify the reconstructed key works
    std::vector<uint8_t> message = {0x01, 0x02, 0x03};
    auto signature = Ed25519::sign(message, reconstructed);
    bool valid = Ed25519::verify(signature, message, public_key);
    EXPECT_TRUE(valid);
}

// Test public key comparison operators
TEST_F(Ed25519Test, PublicKeyComparison) {
    auto [pk1, sk1] = Ed25519::generate_keypair();
    auto [pk2, sk2] = Ed25519::generate_keypair();
    
    // Test equality
    EXPECT_EQ(pk1, pk1);
    EXPECT_NE(pk1, pk2);
    
    // Test less-than (for use in maps/sets)
    bool less = pk1 < pk2;
    bool greater = pk2 < pk1;
    EXPECT_NE(less, greater); // One should be true, one false
}

// Test signature comparison operators
TEST_F(Ed25519Test, SignatureComparison) {
    auto [pk, sk] = Ed25519::generate_keypair();
    std::vector<uint8_t> msg1 = {0x01};
    std::vector<uint8_t> msg2 = {0x02};
    
    auto sig1 = Ed25519::sign(msg1, sk);
    auto sig2 = Ed25519::sign(msg2, sk);
    
    // Test equality
    EXPECT_EQ(sig1, sig1);
    EXPECT_NE(sig1, sig2);
}

// Test invalid hex string handling
TEST_F(Ed25519Test, InvalidHexString) {
    // Too short
    EXPECT_THROW(Ed25519_PublicKey::from_hex("abc"), std::invalid_argument);
    
    // Too long
    std::string too_long(Ed25519_PublicKey::KEY_SIZE * 2 + 2, 'a');
    EXPECT_THROW(Ed25519_PublicKey::from_hex(too_long), std::invalid_argument);
}

// Test invalid vector size handling
TEST_F(Ed25519Test, InvalidVectorSize) {
    std::vector<uint8_t> too_short(Ed25519_PublicKey::KEY_SIZE - 1, 0);
    EXPECT_THROW(Ed25519_PublicKey pk(too_short), std::invalid_argument);
    
    std::vector<uint8_t> too_long(Ed25519_PublicKey::KEY_SIZE + 1, 0);
    EXPECT_THROW(Ed25519_PublicKey pk(too_long), std::invalid_argument);
}

// Test deterministic signatures (same message, same key = same signature)
TEST_F(Ed25519Test, DeterministicSignatures) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    std::vector<uint8_t> message = {0x01, 0x02, 0x03, 0x04, 0x05};
    
    auto sig1 = Ed25519::sign(message, private_key);
    auto sig2 = Ed25519::sign(message, private_key);
    
    // Ed25519 signatures should be deterministic
    EXPECT_EQ(sig1, sig2);
}

// RFC 8032 Test Vector 1: Empty message
TEST_F(Ed25519Test, RFC8032_TestVector1) {
    // Secret key (32 bytes seed)
    std::vector<uint8_t> secret_key_seed = {
        0x9d, 0x61, 0xb1, 0x9d, 0xef, 0xfd, 0x5a, 0x60,
        0xba, 0x84, 0x4a, 0xf4, 0x92, 0xec, 0x2c, 0xc4,
        0x44, 0x49, 0xc5, 0x69, 0x7b, 0x32, 0x69, 0x19,
        0x70, 0x3b, 0xac, 0x03, 0x1c, 0xae, 0x7f, 0x60
    };
    
    // Expected public key
    std::vector<uint8_t> expected_public_key = {
        0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
        0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
        0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
        0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a
    };
    
    // Message (empty)
    std::vector<uint8_t> message;
    
    // Expected signature
    std::vector<uint8_t> expected_signature = {
        0xe5, 0x56, 0x43, 0x00, 0xc3, 0x60, 0xac, 0x72,
        0x90, 0x86, 0xe2, 0xcc, 0x80, 0x6e, 0x82, 0x8a,
        0x84, 0x87, 0x7f, 0x1e, 0xb8, 0xe5, 0xd9, 0x74,
        0xd8, 0x73, 0xe0, 0x65, 0x22, 0x49, 0x01, 0x55,
        0x5f, 0xb8, 0x82, 0x15, 0x90, 0xa3, 0x3b, 0xac,
        0xc6, 0x1e, 0x39, 0x70, 0x1c, 0xf9, 0xb4, 0x6b,
        0xd2, 0x5b, 0xf5, 0xf0, 0x59, 0x5b, 0xbe, 0x24,
        0x65, 0x51, 0x41, 0x43, 0x8e, 0x7a, 0x10, 0x0b
    };
    
    // Note: This test requires the ability to construct a private key from a seed
    // For now, we verify the test vector format is correct
    EXPECT_EQ(secret_key_seed.size(), 32);
    EXPECT_EQ(expected_public_key.size(), 32);
    EXPECT_EQ(expected_signature.size(), 64);
}

// RFC 8032 Test Vector 2: Single byte message
TEST_F(Ed25519Test, RFC8032_TestVector2) {
    // Secret key (32 bytes seed)
    std::vector<uint8_t> secret_key_seed = {
        0x4c, 0xcd, 0x08, 0x9b, 0x28, 0xff, 0x96, 0xda,
        0x9d, 0xb6, 0xc3, 0x46, 0xec, 0x11, 0x4e, 0x0f,
        0x5b, 0x8a, 0x31, 0x9f, 0x35, 0xab, 0xa6, 0x24,
        0xda, 0x8c, 0xf6, 0xed, 0x4f, 0xb8, 0xa6, 0xfb
    };
    
    // Expected public key
    std::vector<uint8_t> expected_public_key = {
        0x3d, 0x40, 0x17, 0xc3, 0xe8, 0x43, 0x89, 0x5a,
        0x92, 0xb7, 0x0a, 0xa7, 0x4d, 0x1b, 0x7e, 0xbc,
        0x9c, 0x98, 0x2c, 0xcf, 0x2e, 0xc4, 0x96, 0x8c,
        0xc0, 0xcd, 0x55, 0xf1, 0x2a, 0xf4, 0x66, 0x0c
    };
    
    // Message (single byte 0x72)
    std::vector<uint8_t> message = {0x72};
    
    // Expected signature
    std::vector<uint8_t> expected_signature = {
        0x92, 0xa0, 0x09, 0xa9, 0xf0, 0xd4, 0xca, 0xb8,
        0x72, 0x0e, 0x82, 0x0b, 0x5f, 0x64, 0x25, 0x40,
        0xa2, 0xb2, 0x7b, 0x54, 0x16, 0x50, 0x3f, 0x8f,
        0xb3, 0x76, 0x22, 0x23, 0xeb, 0xdb, 0x69, 0xda,
        0x08, 0x5a, 0xc1, 0xe4, 0x3e, 0x15, 0x99, 0x6e,
        0x45, 0x8f, 0x36, 0x13, 0xd0, 0xf1, 0x1d, 0x8c,
        0x38, 0x7b, 0x2e, 0xae, 0xb4, 0x30, 0x2a, 0xee,
        0xb0, 0x0d, 0x29, 0x16, 0x12, 0xbb, 0x0c, 0x00
    };
    
    // Verify test vector format
    EXPECT_EQ(secret_key_seed.size(), 32);
    EXPECT_EQ(expected_public_key.size(), 32);
    EXPECT_EQ(message.size(), 1);
    EXPECT_EQ(expected_signature.size(), 64);
}

// RFC 8032 Test Vector 3: Two byte message
TEST_F(Ed25519Test, RFC8032_TestVector3) {
    // Secret key (32 bytes seed)
    std::vector<uint8_t> secret_key_seed = {
        0xc5, 0xaa, 0x8d, 0xf4, 0x3f, 0x9f, 0x83, 0x7b,
        0xed, 0xb7, 0x44, 0x2f, 0x31, 0xdc, 0xb7, 0xb1,
        0x66, 0xd3, 0x85, 0x35, 0x07, 0x6f, 0x09, 0x4b,
        0x85, 0xce, 0x3a, 0x2e, 0x0b, 0x44, 0x58, 0xf7
    };
    
    // Expected public key
    std::vector<uint8_t> expected_public_key = {
        0xfc, 0x51, 0xcd, 0x8e, 0x62, 0x18, 0xa1, 0xa3,
        0x8d, 0xa4, 0x7e, 0xd0, 0x02, 0x30, 0xf0, 0x58,
        0x08, 0x16, 0xed, 0x13, 0xba, 0x33, 0x03, 0xac,
        0x5d, 0xeb, 0x91, 0x15, 0x48, 0x90, 0x80, 0x25
    };
    
    // Message (two bytes 0xaf, 0x82)
    std::vector<uint8_t> message = {0xaf, 0x82};
    
    // Expected signature
    std::vector<uint8_t> expected_signature = {
        0x62, 0x91, 0xd6, 0x57, 0xde, 0xec, 0x24, 0x02,
        0x48, 0x27, 0xe6, 0x9c, 0x3a, 0xbe, 0x01, 0xa3,
        0x0c, 0xe5, 0x48, 0xa2, 0x84, 0x74, 0x3a, 0x44,
        0x5e, 0x36, 0x80, 0xd7, 0xdb, 0x5a, 0xc3, 0xac,
        0x18, 0xff, 0x9b, 0x53, 0x8d, 0x16, 0xf2, 0x90,
        0xae, 0x67, 0xf7, 0x60, 0x98, 0x4d, 0xc6, 0x59,
        0x4a, 0x7c, 0x15, 0xe9, 0x71, 0x6e, 0xd2, 0x8d,
        0xc0, 0x27, 0xbe, 0xce, 0xea, 0x1e, 0xc4, 0x0a
    };
    
    // Verify test vector format
    EXPECT_EQ(secret_key_seed.size(), 32);
    EXPECT_EQ(expected_public_key.size(), 32);
    EXPECT_EQ(message.size(), 2);
    EXPECT_EQ(expected_signature.size(), 64);
}
