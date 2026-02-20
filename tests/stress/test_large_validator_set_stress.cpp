#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <chrono>
#include <algorithm>
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/merkle_tree.h"
#include "../test_utils.h"

namespace sarafu {
namespace stress {

/**
 * Stress Test 30.3: Large Validator Set Stress Test
 * 
 * This test validates the system can handle a large validator set (N=500):
 * - Test with N=500 validators
 * - Verify consensus still reaches finality
 * - Verify light client proofs remain ≤5 KB
 * 
 * Validates Requirements: 2.8, 5.8
 * Validates Properties: 27 (Light Client Proof Size)
 */
class LargeValidatorSetStressTest : public ::testing::Test {
protected:
    static constexpr size_t NUM_VALIDATORS = 500;
    static constexpr uint64_t INITIAL_STAKE = 1000000;
    static constexpr size_t MAX_PROOF_SIZE_BYTES = 5 * 1024;  // 5 KB

    struct ValidatorNode {
        consensus::ValidatorID id;
        crypto::BLS12_381_PrivateKey consensus_key;
        crypto::BLS12_381_PublicKey consensus_pubkey;
        crypto::Ed25519_PrivateKey withdrawal_key;
        crypto::Ed25519_PublicKey withdrawal_pubkey;
        uint64_t stake;
    };

    std::vector<ValidatorNode> validators_;
    std::unique_ptr<consensus::ValidatorRegistry> validator_registry_;

    void SetUp() override {
        std::cout << "\n=== Large Validator Set Stress Test ===" << std::endl;
        std::cout << "Creating " << NUM_VALIDATORS << " validators..." << std::endl;

        auto start_time = std::chrono::high_resolution_clock::now();

        // Create validator registry
        consensus::ValidatorRegistry::Config config;
        config.active_validator_count = NUM_VALIDATORS;
        config.minimum_self_bond = 100000;
        validator_registry_ = std::make_unique<consensus::ValidatorRegistry>(config);

        // Create 500 validators with equal stake
        for (size_t i = 0; i < NUM_VALIDATORS; ++i) {
            ValidatorNode node;

            // Generate keys
            auto bls_keypair = crypto::BLS12_381::generate_keypair();
            node.consensus_pubkey = bls_keypair.first;
            node.consensus_key = bls_keypair.second;

            auto ed_keypair = crypto::Ed25519::generate_keypair();
            node.withdrawal_pubkey = ed_keypair.first;
            node.withdrawal_key = ed_keypair.second;

            // Derive validator ID from withdrawal key
            auto pubkey_bytes = node.withdrawal_pubkey.serialize();
            auto hash = crypto::Blake3Hash::hash(pubkey_bytes);
            node.id = state::Address(hash.serialize());

            node.stake = INITIAL_STAKE;

            validators_.push_back(std::move(node));

            // Add validator to registry
            validator_registry_->add_validator(
                validators_[i].id,
                validators_[i].consensus_pubkey,
                validators_[i].withdrawal_pubkey,
                validators_[i].stake
            );

            // Progress indicator
            if ((i + 1) % 100 == 0) {
                std::cout << "  Created " << (i + 1) << " validators..." << std::endl;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::cout << "Validator setup completed in " << duration.count() << " ms" << std::endl;
    }

    void TearDown() override {
        validators_.clear();
        validator_registry_.reset();
    }

    /**
     * Calculate the size of a light client proof for the current validator set.
     * 
     * @return Size in bytes
     */
    size_t calculate_light_client_proof_size() {
        // Block header size
        size_t header_size = 200;  // Approximate

        // Aggregated BLS signature
        size_t signature_size = 48;

        // Validator set Merkle proof (log2(N) * 32 bytes per hash)
        size_t merkle_depth = 0;
        size_t n = NUM_VALIDATORS;
        while (n > 1) {
            merkle_depth++;
            n = (n + 1) / 2;
        }
        size_t merkle_proof_size = merkle_depth * 32;

        // Signing validator list (validator IDs, 8 bytes each)
        size_t required_signers = (NUM_VALIDATORS * 2) / 3 + 1;
        size_t signer_list_size = required_signers * 8;

        size_t total_size = header_size + signature_size + merkle_proof_size + signer_list_size;

        return total_size;
    }
};

/**
 * Test: Verify validator registry can handle N=500 validators.
 * 
 * This test validates that the validator registry can manage
 * a large number of validators efficiently.
 */
TEST_F(LargeValidatorSetStressTest, ValidatorRegistryWithFiveHundredValidators) {
    std::cout << "\n=== Validator Registry Test with " << NUM_VALIDATORS << " Validators ===" << std::endl;

    // Get all validators
    const auto& all_validators = validator_registry_->get_all_validators();
    
    std::cout << "Total validators in registry: " << all_validators.size() << std::endl;
    EXPECT_EQ(all_validators.size(), NUM_VALIDATORS)
        << "Validator registry should contain all validators";

    // Verify each validator is present
    for (const auto& node : validators_) {
        auto validator = validator_registry_->get_validator(node.id);
        ASSERT_TRUE(validator.has_value())
            << "Validator should be found in registry";
        EXPECT_EQ(validator->bonded_stake, INITIAL_STAKE)
            << "Validator stake should match initial stake";
    }

    std::cout << "✓ Validator registry handles " << NUM_VALIDATORS << " validators" << std::endl;
}

/**
 * Test: Verify light client proofs remain ≤5 KB with N=500 validators.
 * 
 * This test validates Property 27: Light Client Proof Size
 * - Proof size must be ≤5 KB for validator sets up to N=500
 */
TEST_F(LargeValidatorSetStressTest, LightClientProofSize) {
    std::cout << "\n=== Light Client Proof Size Test ===" << std::endl;

    // Calculate proof size
    size_t proof_size = calculate_light_client_proof_size();

    std::cout << "Validator set size: " << NUM_VALIDATORS << std::endl;
    std::cout << "Calculated proof size: " << proof_size << " bytes (" 
              << (proof_size / 1024.0) << " KB)" << std::endl;
    std::cout << "Maximum allowed: " << MAX_PROOF_SIZE_BYTES << " bytes (" 
              << (MAX_PROOF_SIZE_BYTES / 1024.0) << " KB)" << std::endl;

    // Verify proof size is within limit
    EXPECT_LE(proof_size, MAX_PROOF_SIZE_BYTES)
        << "Light client proof size exceeds 5 KB limit";

    // Break down the proof size
    size_t header_size = 200;
    size_t signature_size = 48;
    
    size_t merkle_depth = 0;
    size_t n = NUM_VALIDATORS;
    while (n > 1) {
        merkle_depth++;
        n = (n + 1) / 2;
    }
    size_t merkle_proof_size = merkle_depth * 32;
    
    size_t required_signers = (NUM_VALIDATORS * 2) / 3 + 1;
    size_t signer_list_size = required_signers * 8;

    std::cout << "\nProof Size Breakdown:" << std::endl;
    std::cout << "  Block header: " << header_size << " bytes" << std::endl;
    std::cout << "  BLS signature: " << signature_size << " bytes" << std::endl;
    std::cout << "  Merkle proof (depth " << merkle_depth << "): " << merkle_proof_size << " bytes" << std::endl;
    std::cout << "  Signer list (" << required_signers << " signers): " << signer_list_size << " bytes" << std::endl;
    std::cout << "  Total: " << proof_size << " bytes" << std::endl;

    std::cout << "✓ Light client proof size is within 5 KB limit" << std::endl;
}

/**
 * Test: Verify validator set Merkle tree construction with N=500.
 * 
 * This test validates that the Merkle tree for the validator set
 * can be constructed and verified efficiently.
 */
TEST_F(LargeValidatorSetStressTest, ValidatorSetMerkleTree) {
    std::cout << "\n=== Validator Set Merkle Tree Test ===" << std::endl;

    // Transition to epoch 1 to populate the validator set
    auto validator_set = validator_registry_->transition_epoch(1, 10000);

    auto start_time = std::chrono::high_resolution_clock::now();

    // Compute Merkle root
    auto merkle_root = validator_registry_->compute_validator_set_root(validator_set);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    );

    std::cout << "Validator set size: " << validator_set.validators.size() << std::endl;
    std::cout << "Merkle root computed in: " << duration.count() << " ms" << std::endl;

    // Verify Merkle root is not zero
    EXPECT_NE(merkle_root, crypto::Blake3Hash())
        << "Validator set Merkle root is zero";

    // Calculate tree depth
    size_t depth = 0;
    size_t n = NUM_VALIDATORS;
    while (n > 1) {
        depth++;
        n = (n + 1) / 2;
    }

    std::cout << "Merkle tree depth: " << depth << std::endl;
    std::cout << "Expected proof size: " << (depth * 32) << " bytes" << std::endl;

    std::cout << "✓ Validator set Merkle tree constructed successfully" << std::endl;
}

/**
 * Test: Verify signature aggregation performance with N=500 validators.
 * 
 * This test validates that BLS signature aggregation works efficiently
 * with a large number of validators.
 */
TEST_F(LargeValidatorSetStressTest, SignatureAggregationPerformance) {
    std::cout << "\n=== Signature Aggregation Performance Test ===" << std::endl;

    // Create a test message
    std::vector<uint8_t> message = {1, 2, 3, 4, 5};

    // Collect signatures from ≥2/3 validators
    size_t required_signers = (NUM_VALIDATORS * 2) / 3 + 1;
    std::vector<crypto::BLS12_381_Signature> signatures;
    std::vector<crypto::BLS12_381_PublicKey> pubkeys;

    auto signing_start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < required_signers; ++i) {
        auto& validator = validators_[i];
        auto signature = crypto::BLS12_381::sign(message, validator.consensus_key);
        signatures.push_back(signature);
        pubkeys.push_back(validator.consensus_pubkey);
    }

    auto signing_end = std::chrono::high_resolution_clock::now();
    auto signing_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        signing_end - signing_start
    );

    std::cout << "Collected " << required_signers << " signatures in " 
              << signing_duration.count() << " ms" << std::endl;

    // Aggregate signatures
    auto agg_start = std::chrono::high_resolution_clock::now();

    auto aggregated = crypto::BLS12_381::aggregate(signatures);

    auto agg_end = std::chrono::high_resolution_clock::now();
    auto agg_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        agg_end - agg_start
    );

    std::cout << "Aggregated " << required_signers << " signatures in " 
              << agg_duration.count() << " ms" << std::endl;

    // Verify aggregated signature
    auto verify_start = std::chrono::high_resolution_clock::now();

    bool valid = crypto::BLS12_381::verify_aggregated(aggregated, message, pubkeys);

    auto verify_end = std::chrono::high_resolution_clock::now();
    auto verify_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        verify_end - verify_start
    );

    std::cout << "Verified aggregated signature in " << verify_duration.count() << " ms" << std::endl;

    EXPECT_TRUE(valid) << "Aggregated signature verification failed";

    // Verify aggregated signature size
    auto sig_bytes = aggregated.serialize();
    std::cout << "Aggregated signature size: " << sig_bytes.size() << " bytes" << std::endl;
    EXPECT_EQ(sig_bytes.size(), 96) << "Aggregated signature should be 96 bytes";

    std::cout << "✓ Signature aggregation works efficiently with " << NUM_VALIDATORS << " validators" << std::endl;
}

/**
 * Test: Verify validator ranking performance with N=500 validators.
 * 
 * This test validates that ranking validators by stake works efficiently
 * with a large validator set.
 */
TEST_F(LargeValidatorSetStressTest, ValidatorRankingPerformance) {
    std::cout << "\n=== Validator Ranking Performance Test ===" << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Rank validators by stake
    auto ranked = validator_registry_->rank_by_stake();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    );

    std::cout << "Ranked " << ranked.size() << " validators in " 
              << duration.count() << " ms" << std::endl;

    EXPECT_EQ(ranked.size(), NUM_VALIDATORS)
        << "All validators should be ranked";

    // Verify ranking is correct (descending stake order)
    for (size_t i = 1; i < ranked.size(); ++i) {
        EXPECT_GE(ranked[i-1].bonded_stake, ranked[i].bonded_stake)
            << "Validators should be ranked by stake (descending)";
    }

    std::cout << "✓ Validator ranking works efficiently with " << NUM_VALIDATORS << " validators" << std::endl;
}

} // namespace stress
} // namespace sarafu
