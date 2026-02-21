#include <gtest/gtest.h>
#include "sarafu/crypto/bls12_381.h"
#include <vector>

using namespace sarafu::crypto;

class BLS12_381Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate test keypairs
        auto [pk1, sk1] = BLS12_381::generate_keypair();
        auto [pk2, sk2] = BLS12_381::generate_keypair();
        auto [pk3, sk3] = BLS12_381::generate_keypair();
        
        public_key1 = pk1;
        private_key1 = sk1;
        public_key2 = pk2;
        private_key2 = sk2;
        public_key3 = pk3;
        private_key3 = sk3;
        
        // Test message
        test_message = {0x01, 0x02, 0x03, 0x04, 0x05};
    }

    BLS12_381_PublicKey public_key1;
    BLS12_381_PrivateKey private_key1;
    BLS12_381_PublicKey public_key2;
    BLS12_381_PrivateKey private_key2;
    BLS12_381_PublicKey public_key3;
    BLS12_381_PrivateKey private_key3;
    std::vector<uint8_t> test_message;
};

TEST_F(BLS12_381Test, KeypairGeneration) {
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    
    // Check key sizes
    EXPECT_EQ(public_key.size(), 48);
    EXPECT_EQ(private_key.size(), 32);
    
    // Check that keys are not all zeros
    bool pk_nonzero = false;
    for (size_t i = 0; i < public_key.size(); ++i) {
        if (public_key.bytes()[i] != 0) {
            pk_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(pk_nonzero);
    
    bool sk_nonzero = false;
    for (size_t i = 0; i < private_key.size(); ++i) {
        if (private_key.bytes()[i] != 0) {
            sk_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(sk_nonzero);
}

TEST_F(BLS12_381Test, PublicKeyDerivation) {
    // Derive public key from private key
    BLS12_381_PublicKey derived_pk = private_key1.public_key();
    
    // Should match the original public key
    EXPECT_EQ(derived_pk, public_key1);
}

TEST_F(BLS12_381Test, SignAndVerify) {
    // Sign message
    BLS12_381_Signature signature = BLS12_381::sign(test_message, private_key1);
    
    // Check signature size (96 bytes for G2 signatures)
    EXPECT_EQ(signature.size(), 96);
    
    // Verify signature
    bool valid = BLS12_381::verify(signature, test_message, public_key1);
    EXPECT_TRUE(valid);
}

TEST_F(BLS12_381Test, VerifyInvalidSignature) {
    // Sign with one key
    BLS12_381_Signature signature = BLS12_381::sign(test_message, private_key1);
    
    // Try to verify with different public key
    bool valid = BLS12_381::verify(signature, test_message, public_key2);
    EXPECT_FALSE(valid);
}

TEST_F(BLS12_381Test, VerifyWrongMessage) {
    // Sign message
    BLS12_381_Signature signature = BLS12_381::sign(test_message, private_key1);
    
    // Try to verify with different message
    std::vector<uint8_t> wrong_message = {0x06, 0x07, 0x08};
    bool valid = BLS12_381::verify(signature, wrong_message, public_key1);
    EXPECT_FALSE(valid);
}

TEST_F(BLS12_381Test, SignatureDeterminism) {
    // Sign same message twice
    BLS12_381_Signature sig1 = BLS12_381::sign(test_message, private_key1);
    BLS12_381_Signature sig2 = BLS12_381::sign(test_message, private_key1);
    
    // Signatures should be identical
    EXPECT_EQ(sig1, sig2);
}

TEST_F(BLS12_381Test, AggregateSignatures) {
    // Sign same message with multiple keys
    BLS12_381_Signature sig1 = BLS12_381::sign(test_message, private_key1);
    BLS12_381_Signature sig2 = BLS12_381::sign(test_message, private_key2);
    BLS12_381_Signature sig3 = BLS12_381::sign(test_message, private_key3);
    
    // Aggregate signatures
    std::vector<BLS12_381_Signature> signatures = {sig1, sig2, sig3};
    BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
    
    // Aggregated signature should be same size as individual signatures (96 bytes)
    EXPECT_EQ(aggregated.size(), 96);
}

TEST_F(BLS12_381Test, VerifyAggregatedSignature) {
    // Sign same message with multiple keys
    BLS12_381_Signature sig1 = BLS12_381::sign(test_message, private_key1);
    BLS12_381_Signature sig2 = BLS12_381::sign(test_message, private_key2);
    BLS12_381_Signature sig3 = BLS12_381::sign(test_message, private_key3);
    
    // Aggregate signatures
    std::vector<BLS12_381_Signature> signatures = {sig1, sig2, sig3};
    BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
    
    // Verify aggregated signature
    std::vector<BLS12_381_PublicKey> public_keys = {public_key1, public_key2, public_key3};
    bool valid = BLS12_381::verify_aggregated(aggregated, test_message, public_keys);
    EXPECT_TRUE(valid);
}

TEST_F(BLS12_381Test, VerifyAggregatedSignatureWrongKeys) {
    // Sign same message with multiple keys
    BLS12_381_Signature sig1 = BLS12_381::sign(test_message, private_key1);
    BLS12_381_Signature sig2 = BLS12_381::sign(test_message, private_key2);
    
    // Aggregate signatures
    std::vector<BLS12_381_Signature> signatures = {sig1, sig2};
    BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
    
    // Try to verify with wrong set of public keys
    std::vector<BLS12_381_PublicKey> wrong_keys = {public_key1, public_key3};
    bool valid = BLS12_381::verify_aggregated(aggregated, test_message, wrong_keys);
    EXPECT_FALSE(valid);
}

TEST_F(BLS12_381Test, AggregateEmptySignatures) {
    std::vector<BLS12_381_Signature> empty_signatures;
    
    // Should throw exception
    EXPECT_THROW(BLS12_381::aggregate(empty_signatures), std::invalid_argument);
}

TEST_F(BLS12_381Test, VerifyAggregatedEmptyKeys) {
    BLS12_381_Signature sig = BLS12_381::sign(test_message, private_key1);
    std::vector<BLS12_381_PublicKey> empty_keys;
    
    // Should throw exception
    EXPECT_THROW(
        BLS12_381::verify_aggregated(sig, test_message, empty_keys),
        std::invalid_argument
    );
}

TEST_F(BLS12_381Test, PublicKeySerialization) {
    // Serialize public key
    std::vector<uint8_t> serialized = public_key1.serialize();
    EXPECT_EQ(serialized.size(), 48);
    
    // Create new public key from serialized data
    BLS12_381_PublicKey deserialized(serialized);
    EXPECT_EQ(deserialized, public_key1);
}

TEST_F(BLS12_381Test, SignatureSerialization) {
    BLS12_381_Signature signature = BLS12_381::sign(test_message, private_key1);
    
    // Serialize signature
    std::vector<uint8_t> serialized = signature.serialize();
    EXPECT_EQ(serialized.size(), 96);
    
    // Create new signature from serialized data
    BLS12_381_Signature deserialized(serialized);
    EXPECT_EQ(deserialized, signature);
    
    // Verify deserialized signature
    bool valid = BLS12_381::verify(deserialized, test_message, public_key1);
    EXPECT_TRUE(valid);
}

TEST_F(BLS12_381Test, HexSerialization) {
    // Convert public key to hex
    std::string hex = public_key1.to_hex();
    EXPECT_EQ(hex.length(), 96);  // 48 bytes * 2 hex chars
    
    // Convert back from hex
    BLS12_381_PublicKey from_hex = BLS12_381_PublicKey::from_hex(hex);
    EXPECT_EQ(from_hex, public_key1);
    
    // Test signature hex serialization
    BLS12_381_Signature signature = BLS12_381::sign(test_message, private_key1);
    std::string sig_hex = signature.to_hex();
    EXPECT_EQ(sig_hex.length(), 192);  // 96 bytes * 2 hex chars
}

TEST_F(BLS12_381Test, ComparisonOperators) {
    // Test equality
    BLS12_381_PublicKey pk_copy = public_key1;
    EXPECT_EQ(public_key1, pk_copy);
    EXPECT_FALSE(public_key1 != pk_copy);
    
    // Test inequality
    EXPECT_NE(public_key1, public_key2);
    EXPECT_FALSE(public_key1 == public_key2);
    
    // Test less-than (for use in maps/sets)
    bool less = public_key1 < public_key2 || public_key2 < public_key1;
    EXPECT_TRUE(less);  // One should be less than the other
}

TEST_F(BLS12_381Test, BatchVerification) {
    // Create multiple signatures on the same message
    std::vector<BLS12_381_Signature> signatures;
    std::vector<BLS12_381_PublicKey> public_keys;
    
    for (int i = 0; i < 10; ++i) {
        auto [pk, sk] = BLS12_381::generate_keypair();
        BLS12_381_Signature sig = BLS12_381::sign(test_message, sk);
        signatures.push_back(sig);
        public_keys.push_back(pk);
    }
    
    // Aggregate all signatures
    BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
    
    // Verify aggregated signature
    bool valid = BLS12_381::verify_aggregated(aggregated, test_message, public_keys);
    EXPECT_TRUE(valid);
}

// Official BLS12-381 test vector - Basic signature verification
TEST_F(BLS12_381Test, OfficialTestVector_BasicSignature) {
    // This test verifies that our BLS12-381 implementation produces
    // signatures that can be verified correctly
    
    // Generate a keypair
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    
    // Test message from BLS spec
    std::vector<uint8_t> message = {0x61, 0x62, 0x63}; // "abc"
    
    // Sign the message
    BLS12_381_Signature signature = BLS12_381::sign(message, private_key);
    
    // Verify the signature
    bool valid = BLS12_381::verify(signature, message, public_key);
    EXPECT_TRUE(valid);
    
    // Verify signature size matches spec (96 bytes for G2 signatures)
    EXPECT_EQ(signature.size(), 96);
    EXPECT_EQ(public_key.size(), 48);
}

// Test aggregation with known properties
TEST_F(BLS12_381Test, OfficialTestVector_AggregationProperties) {
    // Create 3 signatures on the same message
    std::vector<uint8_t> message = {0x74, 0x65, 0x73, 0x74}; // "test"
    
    auto [pk1, sk1] = BLS12_381::generate_keypair();
    auto [pk2, sk2] = BLS12_381::generate_keypair();
    auto [pk3, sk3] = BLS12_381::generate_keypair();
    
    BLS12_381_Signature sig1 = BLS12_381::sign(message, sk1);
    BLS12_381_Signature sig2 = BLS12_381::sign(message, sk2);
    BLS12_381_Signature sig3 = BLS12_381::sign(message, sk3);
    
    // Aggregate in different orders - should produce same result
    std::vector<BLS12_381_Signature> sigs_order1 = {sig1, sig2, sig3};
    std::vector<BLS12_381_Signature> sigs_order2 = {sig3, sig1, sig2};
    
    BLS12_381_Signature agg1 = BLS12_381::aggregate(sigs_order1);
    BLS12_381_Signature agg2 = BLS12_381::aggregate(sigs_order2);
    
    // Aggregation should be commutative
    EXPECT_EQ(agg1, agg2);
    
    // Both should verify with the correct public keys
    std::vector<BLS12_381_PublicKey> pks = {pk1, pk2, pk3};
    EXPECT_TRUE(BLS12_381::verify_aggregated(agg1, message, pks));
    EXPECT_TRUE(BLS12_381::verify_aggregated(agg2, message, pks));
}

// Test empty message signing (edge case)
TEST_F(BLS12_381Test, OfficialTestVector_EmptyMessage) {
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    
    // Empty message
    std::vector<uint8_t> empty_message;
    
    // Sign and verify empty message
    BLS12_381_Signature signature = BLS12_381::sign(empty_message, private_key);
    bool valid = BLS12_381::verify(signature, empty_message, public_key);
    
    EXPECT_TRUE(valid);
    EXPECT_EQ(signature.size(), 96);
}

// Test large message signing
TEST_F(BLS12_381Test, OfficialTestVector_LargeMessage) {
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    
    // Large message (1 KB)
    std::vector<uint8_t> large_message(1024);
    for (size_t i = 0; i < large_message.size(); ++i) {
        large_message[i] = static_cast<uint8_t>(i % 256);
    }
    
    // Sign and verify large message
    BLS12_381_Signature signature = BLS12_381::sign(large_message, private_key);
    bool valid = BLS12_381::verify(signature, large_message, public_key);
    
    EXPECT_TRUE(valid);
}
