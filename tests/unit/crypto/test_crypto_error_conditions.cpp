#include <gtest/gtest.h>
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/bls12_381.h"
#include <vector>
#include <limits>

using namespace sarafu::crypto;

// ============================================================================
// Blake3 Error Conditions and Edge Cases
// ============================================================================

class Blake3ErrorTest : public ::testing::Test {};

TEST_F(Blake3ErrorTest, InvalidHexStringTooShort) {
    EXPECT_THROW(Blake3Hash::from_hex("abc"), std::invalid_argument);
    EXPECT_THROW(Blake3Hash::from_hex(""), std::invalid_argument);
}

TEST_F(Blake3ErrorTest, InvalidHexStringTooLong) {
    std::string too_long(65, 'a'); // 65 hex chars (should be 64)
    EXPECT_THROW(Blake3Hash::from_hex(too_long), std::invalid_argument);
}

TEST_F(Blake3ErrorTest, InvalidHexStringNonHexCharacters) {
    std::string invalid_hex = "g0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    EXPECT_THROW(Blake3Hash::from_hex(invalid_hex), std::invalid_argument);
}

TEST_F(Blake3ErrorTest, ConstructorFromVectorWrongSize) {
    std::vector<uint8_t> too_short(16, 0x42);
    EXPECT_THROW(Blake3Hash hash(too_short), std::invalid_argument);
    
    std::vector<uint8_t> too_long(64, 0x42);
    EXPECT_THROW(Blake3Hash hash(too_long), std::invalid_argument);
}

TEST_F(Blake3ErrorTest, EmptyDataHashing) {
    // Empty data should not throw, but produce valid hash
    std::vector<uint8_t> empty;
    EXPECT_NO_THROW(Blake3Hash::hash(empty));
    
    Blake3Hash hash = Blake3Hash::hash(empty);
    EXPECT_EQ(hash.size(), 32);
    EXPECT_NE(hash, Blake3Hash::zero());
}

TEST_F(Blake3ErrorTest, MaxSizeDataHashing) {
    // Test with very large data (10 MB)
    std::vector<uint8_t> large_data(10 * 1024 * 1024, 0xAB);
    EXPECT_NO_THROW(Blake3Hash::hash(large_data));
    
    Blake3Hash hash = Blake3Hash::hash(large_data);
    EXPECT_EQ(hash.size(), 32);
}

TEST_F(Blake3ErrorTest, BoundaryValueHashing) {
    // Test with boundary sizes
    std::vector<uint8_t> one_byte = {0xFF};
    Blake3Hash hash1 = Blake3Hash::hash(one_byte);
    EXPECT_EQ(hash1.size(), 32);
    
    std::vector<uint8_t> max_byte(1, 0xFF);
    Blake3Hash hash2 = Blake3Hash::hash(max_byte);
    EXPECT_EQ(hash2.size(), 32);
}

// ============================================================================
// Ed25519 Error Conditions and Edge Cases
// ============================================================================

class Ed25519ErrorTest : public ::testing::Test {};

TEST_F(Ed25519ErrorTest, InvalidPublicKeySize) {
    std::vector<uint8_t> too_short(16, 0x42);
    EXPECT_THROW(Ed25519_PublicKey pk(too_short), std::invalid_argument);
    
    std::vector<uint8_t> too_long(64, 0x42);
    EXPECT_THROW(Ed25519_PublicKey pk(too_long), std::invalid_argument);
}

TEST_F(Ed25519ErrorTest, InvalidSignatureSize) {
    std::vector<uint8_t> too_short(32, 0x42);
    EXPECT_THROW(Ed25519_Signature sig(too_short), std::invalid_argument);
    
    std::vector<uint8_t> too_long(128, 0x42);
    EXPECT_THROW(Ed25519_Signature sig(too_long), std::invalid_argument);
}

TEST_F(Ed25519ErrorTest, InvalidHexPublicKey) {
    // Too short
    EXPECT_THROW(Ed25519_PublicKey::from_hex("abc"), std::invalid_argument);
    
    // Too long
    std::string too_long(Ed25519_PublicKey::KEY_SIZE * 2 + 2, 'a');
    EXPECT_THROW(Ed25519_PublicKey::from_hex(too_long), std::invalid_argument);
    
    // Non-hex characters
    std::string invalid_hex(Ed25519_PublicKey::KEY_SIZE * 2, 'g');
    EXPECT_THROW(Ed25519_PublicKey::from_hex(invalid_hex), std::invalid_argument);
}

TEST_F(Ed25519ErrorTest, InvalidHexSignature) {
    // Too short
    EXPECT_THROW(Ed25519_Signature::from_hex("abc"), std::invalid_argument);
    
    // Too long
    std::string too_long(Ed25519_Signature::SIGNATURE_SIZE * 2 + 2, 'a');
    EXPECT_THROW(Ed25519_Signature::from_hex(too_long), std::invalid_argument);
}

TEST_F(Ed25519ErrorTest, VerifyWithInvalidSignature) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    std::vector<uint8_t> message = {0x01, 0x02, 0x03};
    
    // Create an invalid signature (all zeros)
    Ed25519_Signature::SignatureArray invalid_sig_data;
    invalid_sig_data.fill(0);
    Ed25519_Signature invalid_sig(invalid_sig_data);
    
    // Verification should fail, not throw
    EXPECT_FALSE(Ed25519::verify(invalid_sig, message, public_key));
}

TEST_F(Ed25519ErrorTest, VerifyWithCorruptedSignature) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    std::vector<uint8_t> message = {0x01, 0x02, 0x03};
    
    // Sign the message
    auto signature = Ed25519::sign(message, private_key);
    
    // Corrupt the signature
    auto sig_data = signature.data();
    Ed25519_Signature::SignatureArray corrupted_data = sig_data;
    corrupted_data[0] ^= 0xFF; // Flip bits
    Ed25519_Signature corrupted_sig(corrupted_data);
    
    // Verification should fail
    EXPECT_FALSE(Ed25519::verify(corrupted_sig, message, public_key));
}

TEST_F(Ed25519ErrorTest, EmptyMessageSigning) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    std::vector<uint8_t> empty_message;
    
    // Should not throw
    EXPECT_NO_THROW(Ed25519::sign(empty_message, private_key));
    
    auto signature = Ed25519::sign(empty_message, private_key);
    EXPECT_TRUE(Ed25519::verify(signature, empty_message, public_key));
}

TEST_F(Ed25519ErrorTest, MaxSizeMessageSigning) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Very large message (10 MB)
    std::vector<uint8_t> large_message(10 * 1024 * 1024);
    for (size_t i = 0; i < large_message.size(); ++i) {
        large_message[i] = static_cast<uint8_t>(i % 256);
    }
    
    // Should not throw
    EXPECT_NO_THROW(Ed25519::sign(large_message, private_key));
    
    auto signature = Ed25519::sign(large_message, private_key);
    EXPECT_TRUE(Ed25519::verify(signature, large_message, public_key));
}

TEST_F(Ed25519ErrorTest, BoundaryValueMessage) {
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Single byte with max value
    std::vector<uint8_t> max_byte = {0xFF};
    auto sig1 = Ed25519::sign(max_byte, private_key);
    EXPECT_TRUE(Ed25519::verify(sig1, max_byte, public_key));
    
    // Single byte with min value
    std::vector<uint8_t> min_byte = {0x00};
    auto sig2 = Ed25519::sign(min_byte, private_key);
    EXPECT_TRUE(Ed25519::verify(sig2, min_byte, public_key));
}

// ============================================================================
// BLS12-381 Error Conditions and Edge Cases
// ============================================================================

class BLS12_381ErrorTest : public ::testing::Test {};

TEST_F(BLS12_381ErrorTest, InvalidPublicKeySize) {
    std::vector<uint8_t> too_short(24, 0x42);
    EXPECT_THROW(BLS12_381_PublicKey pk(too_short), std::invalid_argument);
    
    std::vector<uint8_t> too_long(96, 0x42);
    EXPECT_THROW(BLS12_381_PublicKey pk(too_long), std::invalid_argument);
}

TEST_F(BLS12_381ErrorTest, InvalidSignatureSize) {
    std::vector<uint8_t> too_short(48, 0x42);
    EXPECT_THROW(BLS12_381_Signature sig(too_short), std::invalid_argument);
    
    std::vector<uint8_t> too_long(192, 0x42);
    EXPECT_THROW(BLS12_381_Signature sig(too_long), std::invalid_argument);
}

TEST_F(BLS12_381ErrorTest, InvalidHexPublicKey) {
    // Too short
    EXPECT_THROW(BLS12_381_PublicKey::from_hex("abc"), std::invalid_argument);
    
    // Too long
    std::string too_long(BLS12_381_PublicKey::KEY_SIZE * 2 + 2, 'a');
    EXPECT_THROW(BLS12_381_PublicKey::from_hex(too_long), std::invalid_argument);
}

TEST_F(BLS12_381ErrorTest, AggregateEmptySignatures) {
    std::vector<BLS12_381_Signature> empty_signatures;
    
    // Should throw exception
    EXPECT_THROW(BLS12_381::aggregate(empty_signatures), std::invalid_argument);
}

TEST_F(BLS12_381ErrorTest, VerifyAggregatedEmptyKeys) {
    auto [pk, sk] = BLS12_381::generate_keypair();
    std::vector<uint8_t> message = {0x01, 0x02, 0x03};
    BLS12_381_Signature sig = BLS12_381::sign(message, sk);
    
    std::vector<BLS12_381_PublicKey> empty_keys;
    
    // Should throw exception
    EXPECT_THROW(
        BLS12_381::verify_aggregated(sig, message, empty_keys),
        std::invalid_argument
    );
}

TEST_F(BLS12_381ErrorTest, VerifyWithInvalidSignature) {
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    std::vector<uint8_t> message = {0x01, 0x02, 0x03};
    
    // Create an invalid signature (all zeros)
    BLS12_381_Signature::SignatureArray invalid_sig_data;
    invalid_sig_data.fill(0);
    BLS12_381_Signature invalid_sig(invalid_sig_data);
    
    // Verification should fail, not throw
    EXPECT_FALSE(BLS12_381::verify(invalid_sig, message, public_key));
}

TEST_F(BLS12_381ErrorTest, VerifyWithCorruptedSignature) {
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    std::vector<uint8_t> message = {0x01, 0x02, 0x03};
    
    // Sign the message
    auto signature = BLS12_381::sign(message, private_key);
    
    // Corrupt the signature
    auto sig_data = signature.data();
    BLS12_381_Signature::SignatureArray corrupted_data = sig_data;
    corrupted_data[0] ^= 0xFF; // Flip bits
    BLS12_381_Signature corrupted_sig(corrupted_data);
    
    // Verification should fail
    EXPECT_FALSE(BLS12_381::verify(corrupted_sig, message, public_key));
}

TEST_F(BLS12_381ErrorTest, EmptyMessageSigning) {
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    std::vector<uint8_t> empty_message;
    
    // Should not throw
    EXPECT_NO_THROW(BLS12_381::sign(empty_message, private_key));
    
    auto signature = BLS12_381::sign(empty_message, private_key);
    EXPECT_TRUE(BLS12_381::verify(signature, empty_message, public_key));
}

TEST_F(BLS12_381ErrorTest, MaxSizeMessageSigning) {
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    
    // Very large message (10 MB)
    std::vector<uint8_t> large_message(10 * 1024 * 1024);
    for (size_t i = 0; i < large_message.size(); ++i) {
        large_message[i] = static_cast<uint8_t>(i % 256);
    }
    
    // Should not throw
    EXPECT_NO_THROW(BLS12_381::sign(large_message, private_key));
    
    auto signature = BLS12_381::sign(large_message, private_key);
    EXPECT_TRUE(BLS12_381::verify(signature, large_message, public_key));
}

TEST_F(BLS12_381ErrorTest, AggregateWithMismatchedPublicKeys) {
    // Create 3 signatures on the same message
    std::vector<uint8_t> message = {0x01, 0x02, 0x03};
    
    auto [pk1, sk1] = BLS12_381::generate_keypair();
    auto [pk2, sk2] = BLS12_381::generate_keypair();
    auto [pk3, sk3] = BLS12_381::generate_keypair();
    
    BLS12_381_Signature sig1 = BLS12_381::sign(message, sk1);
    BLS12_381_Signature sig2 = BLS12_381::sign(message, sk2);
    
    // Aggregate 2 signatures
    std::vector<BLS12_381_Signature> sigs = {sig1, sig2};
    BLS12_381_Signature aggregated = BLS12_381::aggregate(sigs);
    
    // Try to verify with wrong set of public keys (3 keys instead of 2)
    std::vector<BLS12_381_PublicKey> wrong_keys = {pk1, pk2, pk3};
    EXPECT_FALSE(BLS12_381::verify_aggregated(aggregated, message, wrong_keys));
}

TEST_F(BLS12_381ErrorTest, BoundaryValueMessage) {
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    
    // Single byte with max value
    std::vector<uint8_t> max_byte = {0xFF};
    auto sig1 = BLS12_381::sign(max_byte, private_key);
    EXPECT_TRUE(BLS12_381::verify(sig1, max_byte, public_key));
    
    // Single byte with min value
    std::vector<uint8_t> min_byte = {0x00};
    auto sig2 = BLS12_381::sign(min_byte, private_key);
    EXPECT_TRUE(BLS12_381::verify(sig2, min_byte, public_key));
}

TEST_F(BLS12_381ErrorTest, AggregateSingleSignature) {
    auto [pk, sk] = BLS12_381::generate_keypair();
    std::vector<uint8_t> message = {0x01, 0x02, 0x03};
    
    BLS12_381_Signature sig = BLS12_381::sign(message, sk);
    
    // Aggregate single signature (edge case)
    std::vector<BLS12_381_Signature> sigs = {sig};
    BLS12_381_Signature aggregated = BLS12_381::aggregate(sigs);
    
    // Should verify with single public key
    std::vector<BLS12_381_PublicKey> pks = {pk};
    EXPECT_TRUE(BLS12_381::verify_aggregated(aggregated, message, pks));
}
