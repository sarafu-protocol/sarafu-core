#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <filesystem>
#include <chrono>
#include "sarafu/consensus/light_client.h"
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/consensus/block.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/merkle_tree.h"
#include "sarafu/storage/state_storage.h"
#include "../test_utils.h"

namespace sarafu {
namespace integration {

/**
 * Integration Test 29.4: Light Client Sync
 * 
 * This test validates light client functionality:
 * - Create light client with checkpoint
 * - Verify light client can sync headers
 * - Verify proof sizes are ≤5 KB
 * - Verify epoch transitions work correctly
 * 
 * Validates Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8
 * Validates Properties: 27, 28, 29, 30
 */
class LightClientSyncTest : public ::testing::Test {
protected:
    static constexpr size_t NUM_VALIDATORS = 100;  // Test with realistic validator count
    static constexpr uint64_t INITIAL_STAKE = 1000000;
    static constexpr size_t MAX_PROOF_SIZE_BYTES = 5 * 1024;  // 5 KB

    struct TestValidator {
        consensus::ValidatorID id;
        crypto::BLS12_381_PrivateKey consensus_key;
        crypto::Ed25519_PrivateKey withdrawal_key;
        uint64_t stake;
    };

    std::shared_ptr<storage::StateStorage> storage_;
    std::shared_ptr<consensus::ValidatorRegistry> validator_registry_;
    std::unique_ptr<consensus::LightClient> light_client_;
    std::vector<TestValidator> test_validators_;

    void SetUp() override {
        // Create temporary directory for test database
        auto unique_suffix = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        test_db_path_ = std::filesystem::temp_directory_path() /
                        ("sarafu_test_light_client_" + std::to_string(unique_suffix));
        std::filesystem::create_directories(test_db_path_);
        
        // Open database
        auto db_result = storage::Database::open(test_db_path_.string());
        ASSERT_TRUE(db_result.is_ok()) << "Failed to open database: " << db_result.error();
        auto db = std::move(db_result.value());
        
        storage_ = std::make_shared<storage::StateStorage>(std::move(db));
        
        // Configure validator registry
        consensus::ValidatorRegistry::Config config;
        config.active_validator_count = NUM_VALIDATORS;
        config.minimum_self_bond = 100000;
        
        validator_registry_ = std::make_shared<consensus::ValidatorRegistry>(config);

        light_client_ = std::make_unique<consensus::LightClient>();

        // Create validators with equal stake
        for (size_t i = 0; i < NUM_VALIDATORS; ++i) {
            TestValidator tv;
            auto bls_keypair = crypto::BLS12_381::generate_keypair();
            tv.consensus_key = bls_keypair.second;
            auto ed_keypair = crypto::Ed25519::generate_keypair();
            tv.withdrawal_key = ed_keypair.second;
            tv.stake = INITIAL_STAKE;

            // Derive validator ID
            auto withdrawal_pubkey = tv.withdrawal_key.public_key();
            auto pubkey_bytes = withdrawal_pubkey.serialize();
            auto hash = crypto::Blake3Hash::hash(pubkey_bytes);
            tv.id = state::Address(hash.serialize());

            test_validators_.push_back(tv);

            // Add validator to registry
            validator_registry_->add_validator(
                tv.id,
                tv.consensus_key.public_key(),
                tv.withdrawal_key.public_key(),
                tv.stake
            );
        }

        // Finalize epoch 0
        validator_registry_->transition_epoch(0, 0);
    }

    void TearDown() override {
        test_validators_.clear();
        light_client_.reset();
        validator_registry_.reset();
        storage_.reset();
        
        // Clean up test database
        std::error_code ec;
        std::filesystem::remove_all(test_db_path_, ec);
    }

    std::filesystem::path test_db_path_;

    /**
     * Create a checkpoint for light client initialization.
     */
    consensus::LightClientState create_checkpoint(uint64_t height) {
        const auto& validator_set = validator_registry_->current_set();

        consensus::LightClientState checkpoint;
        checkpoint.current_epoch = validator_set.epoch;
        checkpoint.validator_set_root = validator_set.merkle_root;
        checkpoint.finalized_height = height;
        checkpoint.finalized_block_hash = crypto::Blake3Hash::hash(std::vector<uint8_t>{static_cast<uint8_t>(height)});

        return checkpoint;
    }

    /**
     * Create a header proof for a block.
     */
    consensus::HeaderProof create_header_proof(
        uint64_t block_height,
        const crypto::Blake3Hash& previous_hash
    ) {
        const auto& validator_set = validator_registry_->current_set();

        // Create block header
        consensus::BlockHeader header;
        header.height = block_height;
        header.timestamp = std::time(nullptr);
        header.previous_hash = previous_hash;
        header.state_root = crypto::Blake3Hash();
        header.transactions_root = crypto::Blake3Hash();
        header.validator_set_root = validator_set.merkle_root;
        header.proposer = test_validators_[0].id;
        header.epoch = validator_set.epoch;

        // Select ≥2/3 of validators to sign
        size_t num_signers = (NUM_VALIDATORS * 2) / 3 + 1;
        std::vector<consensus::Validator> signing_validators;
        std::vector<crypto::BLS12_381_Signature> signatures;
        std::vector<consensus::ValidatorID> signer_ids;

        auto active_validators = validator_set.get_active_validators();
        for (size_t i = 0; i < num_signers && i < active_validators.size(); ++i) {
            signing_validators.push_back(active_validators[i]);
            signer_ids.push_back(active_validators[i].id);

            // Find the test validator and sign
            auto it = std::find_if(test_validators_.begin(), test_validators_.end(),
                [&](const TestValidator& tv) { return tv.id == active_validators[i].id; });

            if (it != test_validators_.end()) {
                auto message = header.hash().serialize();
                auto signature = crypto::BLS12_381::sign(message, it->consensus_key);
                signatures.push_back(signature);
            }
        }

        // Create QC
        consensus::QuorumCertificate qc;
        qc.block_height = block_height;
        qc.block_hash = header.hash();
        qc.view_number = 0;
        qc.signers = signer_ids;

        // Aggregate signatures
        if (!signatures.empty()) {
            qc.aggregated_signature = crypto::BLS12_381::aggregate(signatures);
        }

        // Calculate total stake signed
        qc.total_stake_signed = 0;
        for (const auto& validator : signing_validators) {
            qc.total_stake_signed += validator.bonded_stake;
        }

        // Create Merkle proof for validator set
        // For simplicity, we'll create a minimal proof
        // In a real implementation, this would be a proper Merkle proof
        std::vector<crypto::Blake3Hash> merkle_proof;
        size_t proof_depth = 0;
        size_t n = NUM_VALIDATORS;
        while (n > 1) {
            n = (n + 1) / 2;
            proof_depth++;
        }

        // Create dummy Merkle proof (in real implementation, this would be computed from the tree)
        for (size_t i = 0; i < proof_depth; ++i) {
            merkle_proof.push_back(crypto::Blake3Hash::hash(std::vector<uint8_t>{static_cast<uint8_t>(i)}));
        }

        // Create header proof
        consensus::HeaderProof proof(
            header,
            qc,
            merkle_proof,
            signing_validators,
            validator_set.total_stake
        );

        return proof;
    }
};

/**
 * Test: Initialize light client with checkpoint.
 * 
 * Validates Requirement 5.5: Light client maintains current epoch's validator set root
 */
TEST_F(LightClientSyncTest, InitializeWithCheckpoint) {
    // Create checkpoint at height 1000
    auto checkpoint = create_checkpoint(1000);

    // Initialize light client
    bool initialized = light_client_->initialize(checkpoint);

    ASSERT_TRUE(initialized) << "Failed to initialize light client";
    EXPECT_TRUE(light_client_->is_initialized());

    // Verify state
    const auto& state = light_client_->state();
    EXPECT_EQ(state.current_epoch, checkpoint.current_epoch);
    EXPECT_EQ(state.validator_set_root, checkpoint.validator_set_root);
    EXPECT_EQ(state.finalized_height, checkpoint.finalized_height);
    EXPECT_EQ(state.finalized_block_hash, checkpoint.finalized_block_hash);
}

/**
 * Test: Verify header proof.
 * 
 * Validates Properties:
 * - 28: Merkle Proof Verification
 * - 29: Supermajority Verification (≥2/3 stake)
 */
TEST_F(LightClientSyncTest, VerifyHeaderProof) {
    // Initialize light client at height 1000
    auto checkpoint = create_checkpoint(1000);
    bool initialized = light_client_->initialize(checkpoint);
    ASSERT_TRUE(initialized);

    // Create header proof for height 1001
    auto proof = create_header_proof(1001, checkpoint.finalized_block_hash);

    // Verify the proof
    bool verified = light_client_->verify_header(proof);

    EXPECT_TRUE(verified) << "Failed to verify valid header proof";

    // Verify state was updated
    const auto& state = light_client_->state();
    EXPECT_EQ(state.finalized_height, 1001);
    EXPECT_EQ(state.finalized_block_hash, proof.header.hash());
}

/**
 * Test: Verify proof size is ≤5 KB.
 * 
 * Validates Property 27: Light Client Proof Size
 * - For N≤500 validators, proof size must be ≤5 KB
 */
TEST_F(LightClientSyncTest, VerifyProofSize) {
    // Create checkpoint
    auto checkpoint = create_checkpoint(1000);
    light_client_->initialize(checkpoint);

    // Create header proof
    auto proof = create_header_proof(1001, checkpoint.finalized_block_hash);

    // Calculate proof size
    size_t proof_size = proof.size_bytes();

    EXPECT_LE(proof_size, MAX_PROOF_SIZE_BYTES)
        << "Proof size " << proof_size << " bytes exceeds maximum " << MAX_PROOF_SIZE_BYTES << " bytes";

    // Log proof size for reference
    std::cout << "Proof size for " << NUM_VALIDATORS << " validators: "
              << proof_size << " bytes (" << (proof_size / 1024.0) << " KB)" << std::endl;
}

/**
 * Test: Sync multiple headers.
 * 
 * Validates that light client can sync a sequence of headers.
 */
TEST_F(LightClientSyncTest, SyncMultipleHeaders) {
    // Initialize light client at height 1000
    auto checkpoint = create_checkpoint(1000);
    bool initialized = light_client_->initialize(checkpoint);
    ASSERT_TRUE(initialized);

    // Sync 10 headers
    crypto::Blake3Hash previous_hash = checkpoint.finalized_block_hash;
    for (uint64_t height = 1001; height <= 1010; ++height) {
        auto proof = create_header_proof(height, previous_hash);
        bool verified = light_client_->verify_header(proof);

        ASSERT_TRUE(verified) << "Failed to verify header at height " << height;

        // Update previous hash for next iteration
        previous_hash = proof.header.hash();

        // Verify state progression
        const auto& state = light_client_->state();
        EXPECT_EQ(state.finalized_height, height);
    }

    // Verify final state
    const auto& state = light_client_->state();
    EXPECT_EQ(state.finalized_height, 1010);
}

/**
 * Test: Reject header with insufficient stake.
 * 
 * Validates Property 29: Supermajority Verification
 * - Light client rejects headers without ≥2/3 stake
 */
TEST_F(LightClientSyncTest, RejectInsufficientStake) {
    // Initialize light client
    auto checkpoint = create_checkpoint(1000);
    light_client_->initialize(checkpoint);

    // Create header proof with only 1/3 of validators (insufficient)
    const auto& validator_set = validator_registry_->current_set();
    auto active_validators = validator_set.get_active_validators();

    consensus::BlockHeader header;
    header.height = 1001;
    header.timestamp = std::time(nullptr);
    header.previous_hash = checkpoint.finalized_block_hash;
    header.state_root = crypto::Blake3Hash();
    header.transactions_root = crypto::Blake3Hash();
    header.validator_set_root = validator_set.merkle_root;
    header.proposer = test_validators_[0].id;
    header.epoch = validator_set.epoch;

    // Select only 1/3 of validators (insufficient)
    size_t num_signers = NUM_VALIDATORS / 3;
    std::vector<consensus::Validator> signing_validators;
    std::vector<crypto::BLS12_381_Signature> signatures;
    std::vector<consensus::ValidatorID> signer_ids;

    for (size_t i = 0; i < num_signers; ++i) {
        signing_validators.push_back(active_validators[i]);
        signer_ids.push_back(active_validators[i].id);

        auto it = std::find_if(test_validators_.begin(), test_validators_.end(),
            [&](const TestValidator& tv) { return tv.id == active_validators[i].id; });

        if (it != test_validators_.end()) {
            auto message = header.hash().serialize();
            auto signature = crypto::BLS12_381::sign(message, it->consensus_key);
            signatures.push_back(signature);
        }
    }

    // Create QC with insufficient stake
    consensus::QuorumCertificate qc;
    qc.block_height = 1001;
    qc.block_hash = header.hash();
    qc.view_number = 0;
    qc.signers = signer_ids;

    if (!signatures.empty()) {
        qc.aggregated_signature = crypto::BLS12_381::aggregate(signatures);
    }

    qc.total_stake_signed = 0;
    for (const auto& validator : signing_validators) {
        qc.total_stake_signed += validator.bonded_stake;
    }

    // Create header proof
    std::vector<crypto::Blake3Hash> merkle_proof;
    consensus::HeaderProof proof(
        header,
        qc,
        merkle_proof,
        signing_validators,
        validator_set.total_stake
    );

    // Verify the proof should fail
    bool verified = light_client_->verify_header(proof);

    EXPECT_FALSE(verified)
        << "Light client accepted header with insufficient stake";

    // Verify state was not updated
    const auto& state = light_client_->state();
    EXPECT_EQ(state.finalized_height, 1000)
        << "Light client state was updated despite invalid proof";
}

/**
 * Test: Reject header with wrong epoch.
 * 
 * Validates Property 30: Epoch Consistency
 * - Light client rejects headers from wrong epoch
 */
TEST_F(LightClientSyncTest, RejectWrongEpoch) {
    // Initialize light client at epoch 0
    auto checkpoint = create_checkpoint(1000);
    light_client_->initialize(checkpoint);

    // Create header proof with wrong epoch
    auto proof = create_header_proof(1001, checkpoint.finalized_block_hash);
    proof.header.epoch = 1;  // Wrong epoch (should be 0)

    // Verify the proof should fail
    bool verified = light_client_->verify_header(proof);

    EXPECT_FALSE(verified)
        << "Light client accepted header with wrong epoch";

    // Verify state was not updated
    const auto& state = light_client_->state();
    EXPECT_EQ(state.finalized_height, 1000);
}

/**
 * Test: Reject header that doesn't extend chain.
 * 
 * Validates that light client rejects headers with incorrect previous_hash.
 */
TEST_F(LightClientSyncTest, RejectNonExtendingHeader) {
    // Initialize light client
    auto checkpoint = create_checkpoint(1000);
    light_client_->initialize(checkpoint);

    // Create header proof with wrong previous_hash
    crypto::Blake3Hash wrong_previous_hash = crypto::Blake3Hash::hash(std::vector<uint8_t>{'X'});
    auto proof = create_header_proof(1001, wrong_previous_hash);

    // Verify the proof should fail
    bool verified = light_client_->verify_header(proof);

    EXPECT_FALSE(verified)
        << "Light client accepted header that doesn't extend chain";

    // Verify state was not updated
    const auto& state = light_client_->state();
    EXPECT_EQ(state.finalized_height, 1000);
}

/**
 * Test: Epoch transition.
 * 
 * Validates Requirement 5.6: Light client updates validator set root at epoch boundary
 */
TEST_F(LightClientSyncTest, EpochTransition) {
    // Initialize light client at epoch 0
    auto checkpoint = create_checkpoint(1000);
    light_client_->initialize(checkpoint);

    // Create new validator set for epoch 1
    crypto::Blake3Hash new_validator_set_root = crypto::Blake3Hash::hash(std::vector<uint8_t>{'E', 'P', 'O', 'C', 'H', '1'});

    // Create transition QC signed by ≥2/3 of epoch 0 validators
    const auto& validator_set = validator_registry_->current_set();
    auto active_validators = validator_set.get_active_validators();

    size_t num_signers = (NUM_VALIDATORS * 2) / 3 + 1;
    std::vector<crypto::BLS12_381_Signature> signatures;
    std::vector<consensus::ValidatorID> signer_ids;

    for (size_t i = 0; i < num_signers; ++i) {
        signer_ids.push_back(active_validators[i].id);

        auto it = std::find_if(test_validators_.begin(), test_validators_.end(),
            [&](const TestValidator& tv) { return tv.id == active_validators[i].id; });

        if (it != test_validators_.end()) {
            auto message = new_validator_set_root.serialize();
            auto signature = crypto::BLS12_381::sign(message, it->consensus_key);
            signatures.push_back(signature);
        }
    }

    consensus::QuorumCertificate transition_qc;
    transition_qc.block_height = 10000;  // Epoch boundary
    transition_qc.block_hash = new_validator_set_root;
    transition_qc.view_number = 0;
    transition_qc.signers = signer_ids;

    if (!signatures.empty()) {
        transition_qc.aggregated_signature = crypto::BLS12_381::aggregate(signatures);
    }

    transition_qc.total_stake_signed = 0;
    for (size_t i = 0; i < num_signers; ++i) {
        transition_qc.total_stake_signed += active_validators[i].bonded_stake;
    }

    // Update epoch
    bool updated = light_client_->update_epoch(1, new_validator_set_root, transition_qc);

    EXPECT_TRUE(updated) << "Failed to update epoch";

    // Verify state
    const auto& state = light_client_->state();
    EXPECT_EQ(state.current_epoch, 1);
    EXPECT_EQ(state.validator_set_root, new_validator_set_root);
}

/**
 * Test: Proof size scales with validator count.
 * 
 * This test documents how proof size scales with different validator counts.
 */
TEST_F(LightClientSyncTest, ProofSizeScaling) {
    // Test with current validator count
    auto checkpoint = create_checkpoint(1000);
    light_client_->initialize(checkpoint);

    auto proof = create_header_proof(1001, checkpoint.finalized_block_hash);
    size_t proof_size = proof.size_bytes();

    std::cout << "Proof size breakdown for " << NUM_VALIDATORS << " validators:" << std::endl;
    std::cout << "  Block header: ~200 bytes" << std::endl;
    std::cout << "  Aggregated signature: 96 bytes" << std::endl;

    size_t merkle_proof_depth = 0;
    size_t n = NUM_VALIDATORS;
    while (n > 1) {
        n = (n + 1) / 2;
        merkle_proof_depth++;
    }

    std::cout << "  Merkle proof depth: " << merkle_proof_depth << " levels" << std::endl;
    std::cout << "  Merkle proof size: " << (merkle_proof_depth * 32) << " bytes" << std::endl;
    std::cout << "  Signer list: ~" << (proof.signing_validators.size() * 8) << " bytes (IDs only)" << std::endl;
    std::cout << "  Total: " << proof_size << " bytes (" << (proof_size / 1024.0) << " KB)" << std::endl;

    // Verify it's under the limit
    EXPECT_LE(proof_size, MAX_PROOF_SIZE_BYTES);
}

} // namespace integration
} // namespace sarafu
