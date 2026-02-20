#include <gtest/gtest.h>
#include "sarafu/consensus/block.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/state/account.h"
#include "sarafu/state/transaction.h"

using namespace sarafu::consensus;
using namespace sarafu::crypto;
using namespace sarafu::state;

// Test QuorumCertificate construction and serialization
TEST(QuorumCertificateTest, ConstructionAndSerialization) {
    // Create a QC
    uint64_t height = 100;
    Blake3Hash block_hash = Blake3Hash::hash("test_block");
    uint64_t view = 5;
    BLS12_381_Signature sig;  // Default signature
    std::vector<ValidatorID> signers = {
        Address::from_hex("0000000000000000000000000000000000000000000000000000000000000001"),
        Address::from_hex("0000000000000000000000000000000000000000000000000000000000000002")
    };
    uint64_t stake = 1000000;

    QuorumCertificate qc(height, block_hash, view, sig, signers, stake);

    // Verify fields
    EXPECT_EQ(qc.block_height, height);
    EXPECT_EQ(qc.block_hash, block_hash);
    EXPECT_EQ(qc.view_number, view);
    EXPECT_EQ(qc.signers.size(), 2);
    EXPECT_EQ(qc.total_stake_signed, stake);

    // Test serialization round-trip
    auto serialized = qc.serialize();
    EXPECT_GT(serialized.size(), 0);

    auto deserialized = QuorumCertificate::deserialize(serialized);
    EXPECT_EQ(qc, deserialized);
}

// Test QuorumCertificate equality
TEST(QuorumCertificateTest, Equality) {
    Blake3Hash hash1 = Blake3Hash::hash("block1");
    Blake3Hash hash2 = Blake3Hash::hash("block2");
    BLS12_381_Signature sig;
    std::vector<ValidatorID> signers = {Address::zero()};

    QuorumCertificate qc1(100, hash1, 5, sig, signers, 1000);
    QuorumCertificate qc2(100, hash1, 5, sig, signers, 1000);
    QuorumCertificate qc3(100, hash2, 5, sig, signers, 1000);

    EXPECT_EQ(qc1, qc2);
    EXPECT_NE(qc1, qc3);
}

// Test BlockHeader construction and hashing
TEST(BlockHeaderTest, ConstructionAndHashing) {
    uint64_t height = 42;
    uint64_t timestamp = 1234567890;
    Blake3Hash prev_hash = Blake3Hash::hash("previous_block");
    Blake3Hash state_root = Blake3Hash::hash("state");
    Blake3Hash tx_root = Blake3Hash::hash("transactions");
    Blake3Hash val_root = Blake3Hash::hash("validators");
    ValidatorID proposer = Address::from_hex("0000000000000000000000000000000000000000000000000000000000000001");
    uint64_t epoch = 4;

    BlockHeader header(height, timestamp, prev_hash, state_root, tx_root, val_root, proposer, epoch);

    // Verify fields
    EXPECT_EQ(header.height, height);
    EXPECT_EQ(header.timestamp, timestamp);
    EXPECT_EQ(header.previous_hash, prev_hash);
    EXPECT_EQ(header.state_root, state_root);
    EXPECT_EQ(header.transactions_root, tx_root);
    EXPECT_EQ(header.validator_set_root, val_root);
    EXPECT_EQ(header.proposer, proposer);
    EXPECT_EQ(header.epoch, epoch);

    // Test hashing
    Blake3Hash hash1 = header.hash();
    Blake3Hash hash2 = header.hash();
    EXPECT_EQ(hash1, hash2);  // Deterministic hashing

    // Different header should have different hash
    BlockHeader header2(height + 1, timestamp, prev_hash, state_root, tx_root, val_root, proposer, epoch);
    Blake3Hash hash3 = header2.hash();
    EXPECT_NE(hash1, hash3);
}

// Test BlockHeader serialization
TEST(BlockHeaderTest, Serialization) {
    BlockHeader header(
        100,
        1234567890,
        Blake3Hash::hash("prev"),
        Blake3Hash::hash("state"),
        Blake3Hash::hash("tx"),
        Blake3Hash::hash("val"),
        Address::zero(),
        10
    );

    // Serialize and deserialize
    auto serialized = header.serialize();
    EXPECT_EQ(serialized.size(), 8 + 8 + 32 + 32 + 32 + 32 + 32 + 8);  // Fixed size

    auto deserialized = BlockHeader::deserialize(serialized);
    EXPECT_EQ(header, deserialized);
}

// Test BlockHeader equality
TEST(BlockHeaderTest, Equality) {
    Blake3Hash hash1 = Blake3Hash::hash("test1");
    Blake3Hash hash2 = Blake3Hash::hash("test2");

    BlockHeader h1(100, 1000, hash1, hash1, hash1, hash1, Address::zero(), 10);
    BlockHeader h2(100, 1000, hash1, hash1, hash1, hash1, Address::zero(), 10);
    BlockHeader h3(101, 1000, hash1, hash1, hash1, hash1, Address::zero(), 10);

    EXPECT_EQ(h1, h2);
    EXPECT_NE(h1, h3);
}

// Test Block construction
TEST(BlockTest, Construction) {
    BlockHeader header(
        100,
        1234567890,
        Blake3Hash::hash("prev"),
        Blake3Hash::hash("state"),
        Blake3Hash::hash("tx"),
        Blake3Hash::hash("val"),
        Address::zero(),
        10
    );

    std::vector<Transaction> transactions;
    // Add a simple transaction
    Transaction tx(
        Address::from_hex("0000000000000000000000000000000000000000000000000000000000000001"),
        Address::from_hex("0000000000000000000000000000000000000000000000000000000000000002"),
        1000,  // amount
        0,     // nonce
        10,    // fee
        21000, // gas_limit
        1      // chain_id
    );
    transactions.push_back(tx);

    QuorumCertificate qc;
    Block block(header, transactions, qc);

    // Verify fields
    EXPECT_EQ(block.header, header);
    EXPECT_EQ(block.transactions.size(), 1);
    EXPECT_EQ(block.justify, qc);

    // Block hash should equal header hash
    EXPECT_EQ(block.hash(), header.hash());
}

// Test Block serialization
TEST(BlockTest, Serialization) {
    BlockHeader header(
        100,
        1234567890,
        Blake3Hash::hash("prev"),
        Blake3Hash::hash("state"),
        Blake3Hash::hash("tx"),
        Blake3Hash::hash("val"),
        Address::zero(),
        10
    );

    std::vector<Transaction> transactions;
    Transaction tx(
        Address::from_hex("0000000000000000000000000000000000000000000000000000000000000001"),
        Address::from_hex("0000000000000000000000000000000000000000000000000000000000000002"),
        1000, 0, 10, 21000, 1
    );
    transactions.push_back(tx);

    QuorumCertificate qc;
    Block block(header, transactions, qc);

    // Serialize and deserialize
    auto serialized = block.serialize();
    EXPECT_GT(serialized.size(), 0);

    auto deserialized = Block::deserialize(serialized);
    EXPECT_EQ(block.header, deserialized.header);
    EXPECT_EQ(block.transactions.size(), deserialized.transactions.size());
    EXPECT_EQ(block.justify, deserialized.justify);
}

// Test Block with empty transactions
TEST(BlockTest, EmptyTransactions) {
    BlockHeader header(
        100,
        1234567890,
        Blake3Hash::hash("prev"),
        Blake3Hash::hash("state"),
        Blake3Hash::hash("tx"),
        Blake3Hash::hash("val"),
        Address::zero(),
        10
    );

    std::vector<Transaction> transactions;  // Empty
    QuorumCertificate qc;
    Block block(header, transactions, qc);

    EXPECT_EQ(block.transactions.size(), 0);

    // Serialize and deserialize
    auto serialized = block.serialize();
    auto deserialized = Block::deserialize(serialized);
    EXPECT_EQ(block.header, deserialized.header);
    EXPECT_EQ(deserialized.transactions.size(), 0);
}

// Test Block equality
TEST(BlockTest, Equality) {
    BlockHeader h1(100, 1000, Blake3Hash::hash("a"), Blake3Hash::hash("b"), 
                   Blake3Hash::hash("c"), Blake3Hash::hash("d"), Address::zero(), 10);
    BlockHeader h2(101, 1000, Blake3Hash::hash("a"), Blake3Hash::hash("b"), 
                   Blake3Hash::hash("c"), Blake3Hash::hash("d"), Address::zero(), 10);

    std::vector<Transaction> txs;
    QuorumCertificate qc;

    Block b1(h1, txs, qc);
    Block b2(h1, txs, qc);
    Block b3(h2, txs, qc);

    EXPECT_EQ(b1, b2);
    EXPECT_NE(b1, b3);
}

// Test default constructors
TEST(BlockTest, DefaultConstructors) {
    QuorumCertificate qc;
    EXPECT_EQ(qc.block_height, 0);
    EXPECT_EQ(qc.view_number, 0);
    EXPECT_EQ(qc.total_stake_signed, 0);

    BlockHeader header;
    EXPECT_EQ(header.height, 0);
    EXPECT_EQ(header.timestamp, 0);
    EXPECT_EQ(header.epoch, 0);

    Block block;
    EXPECT_EQ(block.header.height, 0);
    EXPECT_EQ(block.transactions.size(), 0);
}
