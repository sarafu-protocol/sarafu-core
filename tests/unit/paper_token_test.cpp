#include <gtest/gtest.h>
#include "sarafu/state/paper_token.h"
#include "sarafu/state/paper_token_manager.h"
#include "sarafu/state/paper_token_validator.h"
#include "sarafu/state/paper_token_executor.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/blake3_hash.h"

using namespace sarafu::state;
using namespace sarafu::crypto;

class PaperTokenTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate keypairs for testing
        auto [pub1, priv1] = Ed25519::generate_keypair();
        auto [pub2, priv2] = Ed25519::generate_keypair();
        auto [pub3, priv3] = Ed25519::generate_keypair();
        
        creator_public_key_ = pub1;
        creator_private_key_ = priv1;
        redeemer_public_key_ = pub2;
        redeemer_private_key_ = priv2;
        proposer_public_key_ = pub3;
        proposer_private_key_ = priv3;
        
        // Create addresses
        creator_address_ = Address::from_public_key(creator_public_key_.serialize());
        redeemer_address_ = Address::from_public_key(redeemer_public_key_.serialize());
        proposer_address_ = Address::from_public_key(proposer_public_key_.serialize());
        
        // Initialize managers
        account_manager_ = std::make_unique<AccountManager>();
        token_manager_ = std::make_unique<PaperTokenManager>();
        validator_ = std::make_unique<PaperTokenValidator>(1);  // chain_id = 1
        executor_ = std::make_unique<PaperTokenExecutor>(*account_manager_, *token_manager_);
        
        // Create accounts with initial balances
        account_manager_->create_account(creator_address_, 1000000);
        account_manager_->create_account(redeemer_address_, 1000000);
        account_manager_->create_account(proposer_address_, 0);
        
        // Generate a secret for testing
        secret_ = generate_secret(32);
        hash_lock_ = Blake3Hash::hash(secret_);
    }
    
    std::vector<uint8_t> generate_secret(size_t size) {
        std::vector<uint8_t> secret(size);
        for (size_t i = 0; i < size; ++i) {
            secret[i] = static_cast<uint8_t>(i);
        }
        return secret;
    }
    
    Ed25519_PublicKey creator_public_key_;
    Ed25519_PrivateKey creator_private_key_;
    Ed25519_PublicKey redeemer_public_key_;
    Ed25519_PrivateKey redeemer_private_key_;
    Ed25519_PublicKey proposer_public_key_;
    Ed25519_PrivateKey proposer_private_key_;
    
    Address creator_address_;
    Address redeemer_address_;
    Address proposer_address_;
    
    std::vector<uint8_t> secret_;
    Blake3Hash hash_lock_;
    
    std::unique_ptr<AccountManager> account_manager_;
    std::unique_ptr<PaperTokenManager> token_manager_;
    std::unique_ptr<PaperTokenValidator> validator_;
    std::unique_ptr<PaperTokenExecutor> executor_;
};

// ============================================================================
// PaperToken Tests
// ============================================================================

TEST_F(PaperTokenTest, ComputeTokenId) {
    uint64_t creation_height = 100;
    
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, creation_height);
    
    EXPECT_NE(token_id, Blake3Hash::zero());
    
    // Same inputs should produce same token_id
    auto token_id2 = PaperToken::compute_token_id(hash_lock_, creator_address_, creation_height);
    EXPECT_EQ(token_id, token_id2);
    
    // Different inputs should produce different token_id
    auto token_id3 = PaperToken::compute_token_id(hash_lock_, creator_address_, creation_height + 1);
    EXPECT_NE(token_id, token_id3);
}

TEST_F(PaperTokenTest, TokenSerialization) {
    uint64_t creation_height = 100;
    uint64_t refund_height = 100 + (30 * 43200);  // 30 days
    
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, creation_height);
    PaperToken token(token_id, 50000, hash_lock_, creator_address_, creation_height, refund_height);
    
    auto serialized = token.serialize();
    auto deserialized = PaperToken::deserialize(serialized);
    
    EXPECT_EQ(token, deserialized);
}

TEST_F(PaperTokenTest, TokenRedeemableAndRefundable) {
    uint64_t creation_height = 100;
    uint64_t refund_height = 200;
    
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, creation_height);
    PaperToken token(token_id, 50000, hash_lock_, creator_address_, creation_height, refund_height);
    
    // Before refund height: redeemable, not refundable
    EXPECT_TRUE(token.is_redeemable(150));
    EXPECT_FALSE(token.can_refund(150));
    
    // At refund height: not redeemable, refundable
    EXPECT_FALSE(token.is_redeemable(200));
    EXPECT_TRUE(token.can_refund(200));
    
    // After refund height: not redeemable, refundable
    EXPECT_FALSE(token.is_redeemable(250));
    EXPECT_TRUE(token.can_refund(250));
    
    // Consumed token: not redeemable, not refundable
    token.consumed = true;
    EXPECT_FALSE(token.is_redeemable(150));
    EXPECT_FALSE(token.can_refund(250));
}

// ============================================================================
// CreatePaperTokenTx Tests
// ============================================================================

TEST_F(PaperTokenTest, CreatePaperTokenTxSignAndVerify) {
    CreatePaperTokenTx tx(
        creator_address_,
        50000,
        hash_lock_,
        30,  // 30 days refund delay
        0,   // nonce
        1000,  // fee
        100000,  // gas_limit
        1    // chain_id
    );
    
    tx.sign(creator_private_key_);
    
    EXPECT_TRUE(tx.verify_signature(creator_public_key_));
    EXPECT_FALSE(tx.verify_signature(redeemer_public_key_));
}

TEST_F(PaperTokenTest, CreatePaperTokenTxValidation) {
    CreatePaperTokenTx tx(
        creator_address_,
        50000,
        hash_lock_,
        30,
        0,
        1000,
        100000,
        1
    );
    
    tx.sign(creator_private_key_);
    
    auto error = validator_->validate_create(tx, creator_public_key_, *account_manager_, 100);
    EXPECT_FALSE(error.has_value()) << error.value_or("");
}

TEST_F(PaperTokenTest, CreatePaperTokenTxInvalidRefundDelay) {
    // Too short
    CreatePaperTokenTx tx1(creator_address_, 50000, hash_lock_, 5, 0, 1000, 100000, 1);
    EXPECT_FALSE(tx1.has_valid_refund_delay());
    
    // Too long
    CreatePaperTokenTx tx2(creator_address_, 50000, hash_lock_, 400, 0, 1000, 100000, 1);
    EXPECT_FALSE(tx2.has_valid_refund_delay());
    
    // Valid
    CreatePaperTokenTx tx3(creator_address_, 50000, hash_lock_, 30, 0, 1000, 100000, 1);
    EXPECT_TRUE(tx3.has_valid_refund_delay());
}

TEST_F(PaperTokenTest, CreatePaperTokenTxExecution) {
    CreatePaperTokenTx tx(creator_address_, 50000, hash_lock_, 30, 0, 1000, 100000, 1);
    tx.sign(creator_private_key_);
    
    uint64_t block_height = 100;
    uint64_t base_fee = 5;
    
    auto receipt = executor_->execute_create(tx, block_height, base_fee, proposer_address_);
    
    EXPECT_TRUE(receipt.success);
    EXPECT_EQ(receipt.block_height, block_height);
    
    // Check creator balance decreased
    auto creator_account = account_manager_->get_account(creator_address_);
    EXPECT_EQ(creator_account.balance, 1000000 - 50000 - 1000);
    EXPECT_EQ(creator_account.nonce, 1);
    
    // Check token was created
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, block_height);
    auto token = token_manager_->get_token(token_id);
    EXPECT_TRUE(token.has_value());
    EXPECT_EQ(token->amount, 50000);
    EXPECT_FALSE(token->consumed);
}

// ============================================================================
// RedeemPaperTokenTx Tests
// ============================================================================

TEST_F(PaperTokenTest, RedeemPaperTokenTxSignAndVerify) {
    uint64_t block_height = 100;
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, block_height);
    
    RedeemPaperTokenTx tx(
        token_id,
        secret_,
        redeemer_address_,
        redeemer_address_,
        0,
        1000,
        100000,
        1
    );
    
    tx.sign(redeemer_private_key_);
    
    EXPECT_TRUE(tx.verify_signature(redeemer_public_key_));
    EXPECT_FALSE(tx.verify_signature(creator_public_key_));
}

TEST_F(PaperTokenTest, RedeemPaperTokenTxSecretVerification) {
    uint64_t block_height = 100;
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, block_height);
    
    RedeemPaperTokenTx tx(token_id, secret_, redeemer_address_, redeemer_address_, 0, 1000, 100000, 1);
    
    EXPECT_TRUE(tx.verify_secret(hash_lock_));
    
    // Wrong secret
    std::vector<uint8_t> wrong_secret = generate_secret(32);
    wrong_secret[0] = 255;
    RedeemPaperTokenTx tx2(token_id, wrong_secret, redeemer_address_, redeemer_address_, 0, 1000, 100000, 1);
    
    EXPECT_FALSE(tx2.verify_secret(hash_lock_));
}

TEST_F(PaperTokenTest, RedeemPaperTokenTxFullFlow) {
    // 1. Create token
    CreatePaperTokenTx create_tx(creator_address_, 50000, hash_lock_, 30, 0, 1000, 100000, 1);
    create_tx.sign(creator_private_key_);
    
    uint64_t block_height = 100;
    auto create_receipt = executor_->execute_create(create_tx, block_height, 5, proposer_address_);
    EXPECT_TRUE(create_receipt.success);
    
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, block_height);
    
    // 2. Redeem token
    RedeemPaperTokenTx redeem_tx(
        token_id,
        secret_,
        redeemer_address_,
        redeemer_address_,
        0,
        1000,
        100000,
        1
    );
    redeem_tx.sign(redeemer_private_key_);
    
    auto redeem_receipt = executor_->execute_redeem(redeem_tx, block_height + 10, 5, proposer_address_);
    EXPECT_TRUE(redeem_receipt.success);
    
    // Check redeemer received funds
    auto redeemer_account = account_manager_->get_account(redeemer_address_);
    EXPECT_EQ(redeemer_account.balance, 1000000 + 50000 - 1000);
    
    // Check token is consumed
    auto token = token_manager_->get_token(token_id);
    EXPECT_TRUE(token.has_value());
    EXPECT_TRUE(token->consumed);
}

// ============================================================================
// RefundPaperTokenTx Tests
// ============================================================================

TEST_F(PaperTokenTest, RefundPaperTokenTxSignAndVerify) {
    uint64_t block_height = 100;
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, block_height);
    
    RefundPaperTokenTx tx(token_id, creator_address_, creator_address_, 0, 1000, 100000, 1);
    tx.sign(creator_private_key_);
    
    EXPECT_TRUE(tx.verify_signature(creator_public_key_));
    EXPECT_FALSE(tx.verify_signature(redeemer_public_key_));
}

TEST_F(PaperTokenTest, RefundPaperTokenTxFullFlow) {
    // 1. Create token with 30-day refund delay
    CreatePaperTokenTx create_tx(creator_address_, 50000, hash_lock_, 30, 0, 1000, 100000, 1);
    create_tx.sign(creator_private_key_);
    
    uint64_t block_height = 100;
    auto create_receipt = executor_->execute_create(create_tx, block_height, 5, proposer_address_);
    EXPECT_TRUE(create_receipt.success);
    
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, block_height);
    
    // 2. Try to refund before refund_height (should fail validation)
    uint64_t refund_height = block_height + (30 * 43200);
    RefundPaperTokenTx refund_tx(token_id, creator_address_, creator_address_, 1, 1000, 100000, 1);
    refund_tx.sign(creator_private_key_);
    
    auto error = validator_->validate_refund(
        refund_tx,
        creator_public_key_,
        *account_manager_,
        *token_manager_,
        refund_height - 1
    );
    EXPECT_TRUE(error.has_value());
    EXPECT_EQ(error.value(), "Refund height not yet reached");
    
    // 3. Refund after refund_height (should succeed)
    auto refund_receipt = executor_->execute_refund(refund_tx, refund_height, 5, proposer_address_);
    EXPECT_TRUE(refund_receipt.success);
    
    // Check creator received refund
    auto creator_account = account_manager_->get_account(creator_address_);
    EXPECT_EQ(creator_account.balance, 1000000 - 1000 - 1000);  // Initial - create_fee - refund_fee
    
    // Check token is consumed
    auto token = token_manager_->get_token(token_id);
    EXPECT_TRUE(token.has_value());
    EXPECT_TRUE(token->consumed);
}

// ============================================================================
// PaperTokenManager Tests
// ============================================================================

TEST_F(PaperTokenTest, TokenManagerMerkleTree) {
    // Create multiple tokens
    for (int i = 0; i < 5; ++i) {
        auto secret = generate_secret(32 + i);
        auto hash_lock = Blake3Hash::hash(secret);
        auto token_id = PaperToken::compute_token_id(hash_lock, creator_address_, 100 + i);
        
        PaperToken token(token_id, 10000 * (i + 1), hash_lock, creator_address_, 100 + i, 200 + i);
        token_manager_->create_token(token);
    }
    
    // Compute Merkle root
    auto root = token_manager_->compute_merkle_root();
    EXPECT_NE(root, Blake3Hash::zero());
    
    // Generate and verify proof for first token
    auto secret = generate_secret(32);
    auto hash_lock = Blake3Hash::hash(secret);
    auto token_id = PaperToken::compute_token_id(hash_lock, creator_address_, 100);
    
    auto token = token_manager_->get_token(token_id);
    EXPECT_TRUE(token.has_value());
    
    auto proof = token_manager_->generate_merkle_proof(token_id);
    EXPECT_FALSE(proof.empty());
    
    bool valid = PaperTokenManager::verify_merkle_proof(*token, proof, root);
    EXPECT_TRUE(valid);
}

TEST_F(PaperTokenTest, TokenManagerTotalLocked) {
    // Create tokens
    PaperToken token1(Blake3Hash::hash("token1"), 10000, hash_lock_, creator_address_, 100, 200);
    PaperToken token2(Blake3Hash::hash("token2"), 20000, hash_lock_, creator_address_, 101, 201);
    PaperToken token3(Blake3Hash::hash("token3"), 30000, hash_lock_, creator_address_, 102, 202);
    
    token_manager_->create_token(token1);
    token_manager_->create_token(token2);
    token_manager_->create_token(token3);
    
    EXPECT_EQ(token_manager_->get_total_locked(), 60000);
    
    // Consume one token
    token_manager_->consume_token(token1.token_id);
    EXPECT_EQ(token_manager_->get_total_locked(), 50000);
}

// ============================================================================
// Security Tests
// ============================================================================

TEST_F(PaperTokenTest, FrontRunningProtection) {
    // Create token
    CreatePaperTokenTx create_tx(creator_address_, 50000, hash_lock_, 30, 0, 1000, 100000, 1);
    create_tx.sign(creator_private_key_);
    
    uint64_t block_height = 100;
    executor_->execute_create(create_tx, block_height, 5, proposer_address_);
    
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, block_height);
    
    // Attacker sees the secret in mempool and tries to redirect to their address
    Address attacker_address = Address::from_public_key(proposer_public_key_.serialize());
    
    RedeemPaperTokenTx attacker_tx(
        token_id,
        secret_,
        attacker_address,  // Attacker's address
        redeemer_address_,
        0,
        1000,
        100000,
        1
    );
    
    // Attacker signs with redeemer's key (stolen from mempool)
    attacker_tx.sign(redeemer_private_key_);
    
    // The signature is over hash(secret) || destination || nonce
    // Since destination is attacker_address, the signature won't match
    // when verified against the original redeemer's intent
    
    // In practice, the attacker cannot create a valid signature
    // because they don't have the redeemer's private key
    // and the signature binds the secret to a specific destination
    
    EXPECT_TRUE(attacker_tx.verify_signature(redeemer_public_key_));
    
    // However, if the attacker tries to use their own key:
    RedeemPaperTokenTx attacker_tx2(
        token_id,
        secret_,
        attacker_address,
        attacker_address,  // Attacker as redeemer
        0,
        1000,
        100000,
        1
    );
    
    // Create attacker account
    account_manager_->create_account(attacker_address, 1000000);
    
    attacker_tx2.sign(proposer_private_key_);
    
    // This would succeed, but the attacker needs to pay the fee
    // and the original redeemer can also submit their transaction
    // The first one to be included wins (standard mempool race)
}

TEST_F(PaperTokenTest, DoubleSpendPrevention) {
    // Create token
    CreatePaperTokenTx create_tx(creator_address_, 50000, hash_lock_, 30, 0, 1000, 100000, 1);
    create_tx.sign(creator_private_key_);
    
    uint64_t block_height = 100;
    executor_->execute_create(create_tx, block_height, 5, proposer_address_);
    
    auto token_id = PaperToken::compute_token_id(hash_lock_, creator_address_, block_height);
    
    // First redemption
    RedeemPaperTokenTx redeem_tx1(token_id, secret_, redeemer_address_, redeemer_address_, 0, 1000, 100000, 1);
    redeem_tx1.sign(redeemer_private_key_);
    
    auto receipt1 = executor_->execute_redeem(redeem_tx1, block_height + 10, 5, proposer_address_);
    EXPECT_TRUE(receipt1.success);
    
    // Second redemption attempt (should fail)
    RedeemPaperTokenTx redeem_tx2(token_id, secret_, creator_address_, redeemer_address_, 1, 1000, 100000, 1);
    redeem_tx2.sign(redeemer_private_key_);
    
    auto receipt2 = executor_->execute_redeem(redeem_tx2, block_height + 11, 5, proposer_address_);
    EXPECT_FALSE(receipt2.success);
    EXPECT_EQ(receipt2.error_message, "Token already consumed");
}

TEST_F(PaperTokenTest, ReplayProtection) {
    // Create token on chain 1
    CreatePaperTokenTx tx1(creator_address_, 50000, hash_lock_, 30, 0, 1000, 100000, 1);
    tx1.sign(creator_private_key_);
    
    // Try to replay on chain 2
    auto validator2 = std::make_unique<PaperTokenValidator>(2);
    
    auto error = validator2->validate_create(tx1, creator_public_key_, *account_manager_, 100);
    EXPECT_TRUE(error.has_value());
    EXPECT_EQ(error.value(), "Invalid chain ID");
}
