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
