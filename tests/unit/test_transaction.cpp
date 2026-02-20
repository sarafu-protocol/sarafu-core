#include <gtest/gtest.h>
#include "sarafu/state/transaction.h"
#include "sarafu/crypto/ed25519.h"

using namespace sarafu::state;
using namespace sarafu::crypto;

// ============================================================================
// Transaction Tests
// ============================================================================

TEST(TransactionTest, DefaultConstructor) {
    Transaction tx;
    EXPECT_TRUE(tx.from.is_zero());
    EXPECT_TRUE(tx.to.is_zero());
    EXPECT_EQ(tx.amount, 0);
    EXPECT_EQ(tx.nonce, 0);
    EXPECT_EQ(tx.fee, 0);
    EXPECT_EQ(tx.gas_limit, 0);
    EXPECT_EQ(tx.chain_id, 0);
}

TEST(TransactionTest, ConstructorWithParameters) {
    Address::AddressArray from_data, to_data;
    from_data.fill(0x01);
    to_data.fill(0x02);
    Address from(from_data);
    Address to(to_data);
    
    Transaction tx(from, to, 1000, 5, 10, 21000, 1);
    
    EXPECT_EQ(tx.from, from);
    EXPECT_EQ(tx.to, to);
    EXPECT_EQ(tx.amount, 1000);
    EXPECT_EQ(tx.nonce, 5);
    EXPECT_EQ(tx.fee, 10);
    EXPECT_EQ(tx.gas_limit, 21000);
    EXPECT_EQ(tx.chain_id, 1);
}

TEST(TransactionTest, HashDeterminism) {
    Address::AddressArray from_data, to_data;
    from_data.fill(0x01);
    to_data.fill(0x02);
    Address from(from_data);
    Address to(to_data);
    
    Transaction tx1(from, to, 1000, 5, 10, 21000, 1);
    Transaction tx2(from, to, 1000, 5, 10, 21000, 1);
    
    // Same transaction data should produce same hash
    EXPECT_EQ(tx1.hash(), tx2.hash());
}

TEST(TransactionTest, HashIncludesChainId) {
    Address::AddressArray from_data, to_data;
    from_data.fill(0x01);
    to_data.fill(0x02);
    Address from(from_data);
    Address to(to_data);
    
    Transaction tx1(from, to, 1000, 5, 10, 21000, 1);
    Transaction tx2(from, to, 1000, 5, 10, 21000, 2); // Different chain_id
    
    // Different chain_id should produce different hash
    EXPECT_NE(tx1.hash(), tx2.hash());
}

TEST(TransactionTest, HashExcludesSignature) {
    Address::AddressArray from_data, to_data;
    from_data.fill(0x01);
    to_data.fill(0x02);
    Address from(from_data);
    Address to(to_data);
    
    Transaction tx1(from, to, 1000, 5, 10, 21000, 1);
    Transaction tx2(from, to, 1000, 5, 10, 21000, 1);
    
    // Generate a keypair and sign tx2
    auto [pub_key, priv_key] = Ed25519::generate_keypair();
    tx2.sign(priv_key);
    
    // Hash should be the same even though tx2 has a signature
    EXPECT_EQ(tx1.hash(), tx2.hash());
}

TEST(TransactionTest, SignAndVerify) {
    // Generate keypair
    auto [pub_key, priv_key] = Ed25519::generate_keypair();
    
    // Create transaction with from address derived from public key
    Address from = Address::from_public_key(pub_key.serialize());
    Address to = Address::zero();
    
    Transaction tx(from, to, 1000, 5, 10, 21000, 1);
    
    // Sign the transaction
    tx.sign(priv_key);
    
    // Verify the signature
    EXPECT_TRUE(tx.verify_signature(pub_key));
}

TEST(TransactionTest, VerifyInvalidSignature) {
    // Generate two keypairs
    auto [pub_key1, priv_key1] = Ed25519::generate_keypair();
    auto [pub_key2, priv_key2] = Ed25519::generate_keypair();
    
    // Create transaction
    Address from = Address::from_public_key(pub_key1.serialize());
    Address to = Address::zero();
    
    Transaction tx(from, to, 1000, 5, 10, 21000, 1);
    
    // Sign with priv_key1
    tx.sign(priv_key1);
    
    // Verify with pub_key2 should fail
    EXPECT_FALSE(tx.verify_signature(pub_key2));
}

TEST(TransactionTest, SerializationForSigning) {
    Address::AddressArray from_data, to_data;
    from_data.fill(0x01);
    to_data.fill(0x02);
    Address from(from_data);
    Address to(to_data);
    
    Transaction tx(from, to, 1000, 5, 10, 21000, 1);
    
    auto serialized = tx.serialize_for_signing();
    
    // Expected size: 32 + 32 + 8 + 8 + 8 + 8 + 4 = 100 bytes
    EXPECT_EQ(serialized.size(), 100);
}

TEST(TransactionTest, SerializationComplete) {
    Address::AddressArray from_data, to_data;
    from_data.fill(0x01);
    to_data.fill(0x02);
    Address from(from_data);
    Address to(to_data);
    
    Transaction tx(from, to, 1000, 5, 10, 21000, 1);
    
    // Sign the transaction
    auto [pub_key, priv_key] = Ed25519::generate_keypair();
    tx.sign(priv_key);
    
    auto serialized = tx.serialize();
    
    // Expected size: 32 + 32 + 8 + 8 + 8 + 8 + 4 + 64 = 164 bytes
    EXPECT_EQ(serialized.size(), 164);
}

TEST(TransactionTest, SerializationRoundTrip) {
    // Generate keypair
    auto [pub_key, priv_key] = Ed25519::generate_keypair();
    
    // Create and sign transaction
    Address from = Address::from_public_key(pub_key.serialize());
    Address::AddressArray to_data;
    to_data.fill(0x02);
    Address to(to_data);
    
    Transaction original(from, to, 1000, 5, 10, 21000, 1);
    original.sign(priv_key);
    
    // Serialize and deserialize
    auto serialized = original.serialize();
    Transaction deserialized = Transaction::deserialize(serialized);
    
    // Verify all fields match
    EXPECT_EQ(original.from, deserialized.from);
    EXPECT_EQ(original.to, deserialized.to);
    EXPECT_EQ(original.amount, deserialized.amount);
    EXPECT_EQ(original.nonce, deserialized.nonce);
    EXPECT_EQ(original.fee, deserialized.fee);
    EXPECT_EQ(original.gas_limit, deserialized.gas_limit);
    EXPECT_EQ(original.chain_id, deserialized.chain_id);
    EXPECT_EQ(original.signature, deserialized.signature);
    
    // Verify signature is still valid
    EXPECT_TRUE(deserialized.verify_signature(pub_key));
}

TEST(TransactionTest, DeserializationInvalidSize) {
    std::vector<uint8_t> invalid_data(100); // Wrong size
    EXPECT_THROW(Transaction::deserialize(invalid_data), std::invalid_argument);
}

TEST(TransactionTest, Equality) {
    Address::AddressArray from_data, to_data;
    from_data.fill(0x01);
    to_data.fill(0x02);
    Address from(from_data);
    Address to(to_data);
    
    Transaction tx1(from, to, 1000, 5, 10, 21000, 1);
    Transaction tx2(from, to, 1000, 5, 10, 21000, 1);
    Transaction tx3(from, to, 2000, 5, 10, 21000, 1); // Different amount
    
    EXPECT_EQ(tx1, tx2);
    EXPECT_NE(tx1, tx3);
}

TEST(TransactionTest, LargeValues) {
    Address from = Address::zero();
    Address to = Address::zero();
    
    Transaction tx(from, to, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT32_MAX);
    
    auto serialized = tx.serialize();
    Transaction deserialized = Transaction::deserialize(serialized);
    
    EXPECT_EQ(tx.amount, deserialized.amount);
    EXPECT_EQ(tx.nonce, deserialized.nonce);
    EXPECT_EQ(tx.fee, deserialized.fee);
    EXPECT_EQ(tx.gas_limit, deserialized.gas_limit);
    EXPECT_EQ(tx.chain_id, deserialized.chain_id);
}

TEST(TransactionTest, DifferentFieldsProduceDifferentHashes) {
    Address::AddressArray from_data, to_data;
    from_data.fill(0x01);
    to_data.fill(0x02);
    Address from(from_data);
    Address to(to_data);
    
    Transaction base(from, to, 1000, 5, 10, 21000, 1);
    
    // Different amount
    Transaction tx_amount(from, to, 2000, 5, 10, 21000, 1);
    EXPECT_NE(base.hash(), tx_amount.hash());
    
    // Different nonce
    Transaction tx_nonce(from, to, 1000, 6, 10, 21000, 1);
    EXPECT_NE(base.hash(), tx_nonce.hash());
    
    // Different fee
    Transaction tx_fee(from, to, 1000, 5, 20, 21000, 1);
    EXPECT_NE(base.hash(), tx_fee.hash());
    
    // Different gas_limit
    Transaction tx_gas(from, to, 1000, 5, 10, 42000, 1);
    EXPECT_NE(base.hash(), tx_gas.hash());
    
    // Different chain_id
    Transaction tx_chain(from, to, 1000, 5, 10, 21000, 2);
    EXPECT_NE(base.hash(), tx_chain.hash());
}

TEST(TransactionTest, SignatureChangesAfterModification) {
    // Generate keypair
    auto [pub_key, priv_key] = Ed25519::generate_keypair();
    
    // Create and sign transaction
    Address from = Address::from_public_key(pub_key.serialize());
    Address to = Address::zero();
    
    Transaction tx(from, to, 1000, 5, 10, 21000, 1);
    tx.sign(priv_key);
    
    // Verify signature is valid
    EXPECT_TRUE(tx.verify_signature(pub_key));
    
    // Modify the transaction
    tx.amount = 2000;
    
    // Signature should now be invalid
    EXPECT_FALSE(tx.verify_signature(pub_key));
}

TEST(TransactionTest, ChainIdPreventsReplay) {
    // Generate keypair
    auto [pub_key, priv_key] = Ed25519::generate_keypair();
    
    // Create transaction for chain 1
    Address from = Address::from_public_key(pub_key.serialize());
    Address to = Address::zero();
    
    Transaction tx1(from, to, 1000, 5, 10, 21000, 1);
    tx1.sign(priv_key);
    
    // Create identical transaction for chain 2
    Transaction tx2(from, to, 1000, 5, 10, 21000, 2);
    tx2.signature = tx1.signature; // Copy signature from tx1
    
    // tx1 signature should be valid for tx1
    EXPECT_TRUE(tx1.verify_signature(pub_key));
    
    // tx1 signature should NOT be valid for tx2 (different chain_id)
    EXPECT_FALSE(tx2.verify_signature(pub_key));
}
