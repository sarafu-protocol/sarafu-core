#include "sarafu/consensus/light_client.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/crypto/merkle_tree.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/blake3_hash.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace sarafu::consensus;
using namespace sarafu::crypto;

/**
 * Property-Based Tests for Light Client Verification
 * 
 * **Validates: Requirements 5.3, 5.4, 5.7, 5.8**
 * 
 * Property 27: Light Client Proof Size (≤5 KB for N≤500 validators)
 * Property 28: Merkle Proof Verification (already tested in test_merkle_proof_verification.cpp)
 * Property 29: Supermajority Verification (≥2/3 stake)
 * Property 30: Epoch Consistency
 */
class LightClientVerificationPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random Blake3 hash
    Blake3Hash generate_random_hash() {
        std::vector<uint8_t> data(32);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 32; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return Blake3Hash(data);
    }

    // Generate random validator
    Validator generate_random_validator(uint64_t stake) {
        auto [bls_pk, bls_sk] = BLS12_381::generate_keypair();
        auto [ed_pk, ed_sk] = Ed25519::generate_keypair();
        
        // Generate random validator ID (address)
        std::vector<uint8_t> id_data(32);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 32; ++i) {
            id_data[i] = static_cast<uint8_t>(dist(rng_));
        }
        ValidatorID id(id_data);
        
        return Validator(id, bls_pk, ed_pk, stake);
    }

    // Generate validator set with specified number of validators
    // Returns the validator set and the corresponding private keys
    std::pair<ValidatorSet, std::vector<std::pair<Validator, BLS12_381_PrivateKey>>> 
    generate_validator_set_with_keys(size_t num_validators, uint64_t epoch) {
        std::vector<Validator> validators;
        std::vector<std::pair<Validator, BLS12_381_PrivateKey>> validator_key_pairs;
        validators.reserve(num_validators);
        validator_key_pairs.reserve(num_validators);
        
        uint64_t total_stake = 0;
        const uint64_t stake_value = 1000000;
        
        for (size_t i = 0; i < num_validators; ++i) {
            uint64_t stake = stake_value;
            auto [bls_pk, bls_sk] = BLS12_381::generate_keypair();
            auto [ed_pk, ed_sk] = Ed25519::generate_keypair();
            
            // Generate random validator ID (address)
            std::vector<uint8_t> id_data(32);
            std::uniform_int_distribution<uint16_t> dist(0, 255);
            for (size_t j = 0; j < 32; ++j) {
                id_data[j] = static_cast<uint8_t>(dist(rng_));
            }
            ValidatorID id(id_data);
            
            Validator validator(id, bls_pk, ed_pk, stake);
            validator.status = ValidatorStatus::Active;
            validators.push_back(validator);
            validator_key_pairs.emplace_back(validator, bls_sk);
            total_stake += stake;
        }
        
        // Build Merkle tree for validator set
        std::vector<Blake3Hash> validator_hashes;
        validator_hashes.reserve(num_validators);
        for (const auto& validator : validators) {
            validator_hashes.push_back(validator.hash());
        }
        
        MerkleTree tree;
        tree.build_tree(validator_hashes);
        Blake3Hash merkle_root = tree.get_root();
        
        ValidatorSet validator_set(epoch, validators, total_stake, merkle_root);
        return {validator_set, validator_key_pairs};
    }

    // Generate validator set with specified number of validators (without keys)
    ValidatorSet generate_validator_set(size_t num_validators, uint64_t epoch) {
        return generate_validator_set_with_keys(num_validators, epoch).first;
    }

    // Create a valid header proof
    HeaderProof create_valid_header_proof(
        const ValidatorSet& validator_set,
        uint64_t block_height,
        const Blake3Hash& previous_hash,
        const std::vector<std::pair<Validator, BLS12_381_PrivateKey>>& validator_keys_pairs,
        double signing_fraction = 0.75  // 75% of validators sign
    ) {
        // Create block header
        BlockHeader header;
        header.height = block_height;
        header.timestamp = 1000000 + block_height;
        header.previous_hash = previous_hash;
        header.state_root = generate_random_hash();
        header.transactions_root = generate_random_hash();
        header.validator_set_root = validator_set.merkle_root;
        header.proposer = validator_set.validators[0].id;
        header.epoch = validator_set.epoch;
        
        // Select signing validators (signing_fraction of total)
        size_t num_signers = static_cast<size_t>(validator_set.validators.size() * signing_fraction);
        if (num_signers == 0) num_signers = 1;
        
        std::vector<Validator> signing_validators;
        std::vector<BLS12_381_Signature> signatures;
        uint64_t total_stake_signed = 0;
        
        Blake3Hash block_hash = header.hash();
        std::vector<uint8_t> message = block_hash.serialize();
        
        for (size_t i = 0; i < num_signers && i < validator_keys_pairs.size(); ++i) {
            const auto& [validator, private_key] = validator_keys_pairs[i];
            signing_validators.push_back(validator);
            total_stake_signed += validator.bonded_stake;
            
            // Sign the block hash
            BLS12_381_Signature sig = BLS12_381::sign(message, private_key);
            signatures.push_back(sig);
        }
        
        // Aggregate signatures
        BLS12_381_Signature aggregated_sig;
        if (!signatures.empty()) {
            aggregated_sig = BLS12_381::aggregate(signatures);
        }
        
        // Create QC
        std::vector<ValidatorID> signer_ids;
        for (const auto& v : signing_validators) {
            signer_ids.push_back(v.id);
        }
        
        QuorumCertificate qc(
            block_height,
            block_hash,
            0,  // view_number
            aggregated_sig,
            signer_ids,
            total_stake_signed
        );
        
        // Create Merkle proofs for signing validators
        std::vector<Blake3Hash> validator_hashes;
        for (const auto& v : validator_set.validators) {
            validator_hashes.push_back(v.hash());
        }
        
        MerkleTree tree;
        tree.build_tree(validator_hashes);
        
        // For simplicity, use the proof for the first validator
        // In a real implementation, each validator would have its own proof
        auto proof_opt = tree.generate_proof(0);
        std::vector<Blake3Hash> merkle_proof;
        if (proof_opt.has_value()) {
            merkle_proof = proof_opt.value().sibling_hashes;
        }
        
        return HeaderProof(header, qc, merkle_proof, signing_validators, validator_set.total_stake);
    }

    std::mt19937 rng_;
};

/**
 * Property 27: Light Client Proof Size
 * 
 * For any validator set with N ≤ 500 validators, the header verification proof
 * size is ≤ 5 KB.
 * 
 * **Validates: Requirements 5.8**
 */
TEST_F(LightClientVerificationPropertyTest, ProofSizeUnder5KB) {
    const size_t MAX_PROOF_SIZE = 5 * 1024; // 5 KB
    
    // Test various validator set sizes (up to 500 validators)
    std::vector<size_t> test_sizes = {10, 50, 100, 200, 300, 400, 500};
    
    for (size_t num_validators : test_sizes) {
        // Generate validator set with keys
        auto [validator_set, validator_key_pairs] = generate_validator_set_with_keys(num_validators, 1);
        
        // Create header proof
        Blake3Hash genesis_hash = generate_random_hash();
        HeaderProof proof = create_valid_header_proof(
            validator_set,
            1,
            genesis_hash,
            validator_key_pairs,
            0.75  // 75% of validators sign
        );
        
        // Calculate proof size
        size_t proof_size = proof.size_bytes();
        
        ASSERT_LE(proof_size, MAX_PROOF_SIZE)
            << "Proof size " << proof_size << " bytes exceeds 5 KB limit "
            << "for validator set with " << num_validators << " validators";
        
        // Log proof size for analysis
        std::cout << "Validator set size: " << num_validators
                  << ", Proof size: " << proof_size << " bytes"
                  << " (" << (proof_size * 100 / MAX_PROOF_SIZE) << "% of limit)"
                  << std::endl;
    }
}

/**
 * Property 29: Supermajority Verification
 * 
 * For any header proof, the light client accepts it only if the signing
 * validators represent ≥2/3 of total stake.
 * 
 * **Validates: Requirements 5.3**
 */
TEST_F(LightClientVerificationPropertyTest, SupermajorityVerification) {
    const int NUM_TRIALS = 100;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set with random size
        std::uniform_int_distribution<size_t> size_dist(10, 50);
        size_t num_validators = size_dist(rng_);
        
        auto [validator_set, validator_key_pairs] = generate_validator_set_with_keys(num_validators, 1);
        
        // Test with different signing fractions
        std::vector<double> signing_fractions = {
            0.50,  // 50% - should fail
            0.60,  // 60% - should fail
            0.66,  // 66% - should fail (just under 2/3)
            0.67,  // 67% - should pass (just over 2/3)
            0.75,  // 75% - should pass
            0.90,  // 90% - should pass
            1.00   // 100% - should pass
        };
        
        for (double fraction : signing_fractions) {
            // Initialize light client per fraction to avoid state carry-over
            LightClient light_client;
            Blake3Hash genesis_hash = generate_random_hash();
            LightClientState checkpoint(1, validator_set.merkle_root, 0, genesis_hash);
            ASSERT_TRUE(light_client.initialize(checkpoint));

            // Create header proof with specified signing fraction
            HeaderProof proof = create_valid_header_proof(
                validator_set,
                1,
                genesis_hash,
                validator_key_pairs,
                fraction
            );
            
            // Calculate actual stake fraction
            uint64_t total_stake_signed = 0;
            for (const auto& v : proof.signing_validators) {
                total_stake_signed += v.bonded_stake;
            }
            double actual_fraction = static_cast<double>(total_stake_signed) / validator_set.total_stake;
            
            // Verify header
            bool verified = light_client.verify_header(proof);
            
            // Check if result matches expectation (≥2/3 threshold)
            bool should_pass = (3 * total_stake_signed) >= (2 * validator_set.total_stake);
            
            if (should_pass) {
                ASSERT_TRUE(verified)
                    << "Header proof with " << (actual_fraction * 100) << "% stake should verify";
            } else {
                ASSERT_FALSE(verified)
                    << "Header proof with " << (actual_fraction * 100) << "% stake should not verify";
            }
        }
    }
}

/**
 * Property 30: Epoch Consistency
 * 
 * For any validator set proof, the proof's epoch number must match the
 * light client's current epoch.
 * 
 * **Validates: Requirements 5.7**
 */
TEST_F(LightClientVerificationPropertyTest, EpochConsistency) {
    const int NUM_TRIALS = 100;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> size_dist(10, 50);
        size_t num_validators = size_dist(rng_);
        
        std::uniform_int_distribution<uint64_t> epoch_dist(1, 100);
        uint64_t current_epoch = epoch_dist(rng_);
        
        auto [validator_set, validator_key_pairs] = generate_validator_set_with_keys(num_validators, current_epoch);
        
        // Initialize light client with current epoch
        LightClient light_client;
        Blake3Hash genesis_hash = generate_random_hash();
        LightClientState checkpoint(current_epoch, validator_set.merkle_root, 0, genesis_hash);
        ASSERT_TRUE(light_client.initialize(checkpoint));
        
        // Test 1: Proof with matching epoch should verify
        {
            HeaderProof proof = create_valid_header_proof(
                validator_set,
                1,
                genesis_hash,
                validator_key_pairs,
                0.75
            );
            
            ASSERT_EQ(proof.header.epoch, current_epoch)
                << "Proof epoch should match current epoch";
            
            bool verified = light_client.verify_header(proof);
            ASSERT_TRUE(verified)
                << "Proof with matching epoch should verify";
        }
        
        // Test 2: Proof with wrong epoch should fail
        {
            // Create validator set with different epoch
            uint64_t wrong_epoch = current_epoch + 1;
            auto [wrong_validator_set, wrong_validator_key_pairs] = generate_validator_set_with_keys(num_validators, wrong_epoch);
            
            HeaderProof proof = create_valid_header_proof(
                wrong_validator_set,
                1,
                genesis_hash,
                wrong_validator_key_pairs,
                0.75
            );
            
            ASSERT_NE(proof.header.epoch, current_epoch)
                << "Proof epoch should differ from current epoch";
            
            bool verified = light_client.verify_header(proof);
            ASSERT_FALSE(verified)
                << "Proof with wrong epoch should not verify";
        }
    }
}

/**
 * Property: Height Progression
 * 
 * The light client should only accept headers with height greater than
 * the current finalized height.
 */
TEST_F(LightClientVerificationPropertyTest, HeightProgression) {
    const int NUM_TRIALS = 50;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> size_dist(10, 30);
        size_t num_validators = size_dist(rng_);
        
        auto [validator_set, validator_key_pairs] = generate_validator_set_with_keys(num_validators, 1);
        
        // Initialize light client at height 10
        LightClient light_client;
        Blake3Hash block_hash_10 = generate_random_hash();
        LightClientState checkpoint(1, validator_set.merkle_root, 10, block_hash_10);
        ASSERT_TRUE(light_client.initialize(checkpoint));
        
        // Test 1: Header at height 11 should verify
        {
            HeaderProof proof = create_valid_header_proof(
                validator_set,
                11,
                block_hash_10,
                validator_key_pairs,
                0.75
            );
            
            bool verified = light_client.verify_header(proof);
            ASSERT_TRUE(verified)
                << "Header at height 11 should verify when finalized height is 10";
        }
        
        // Test 2: Header at height 10 (same as finalized) should fail
        {
            HeaderProof proof = create_valid_header_proof(
                validator_set,
                10,
                generate_random_hash(),
                validator_key_pairs,
                0.75
            );
            
            bool verified = light_client.verify_header(proof);
            ASSERT_FALSE(verified)
                << "Header at height 10 should not verify when finalized height is 10";
        }
        
        // Test 3: Header at height 9 (below finalized) should fail
        {
            HeaderProof proof = create_valid_header_proof(
                validator_set,
                9,
                generate_random_hash(),
                validator_key_pairs,
                0.75
            );
            
            bool verified = light_client.verify_header(proof);
            ASSERT_FALSE(verified)
                << "Header at height 9 should not verify when finalized height is 10";
        }
    }
}

/**
 * Property: Chain Extension
 * 
 * For the immediate next block, the previous_hash must match the
 * finalized_block_hash.
 */
TEST_F(LightClientVerificationPropertyTest, ChainExtension) {
    const int NUM_TRIALS = 50;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> size_dist(10, 30);
        size_t num_validators = size_dist(rng_);
        
        auto [validator_set, validator_key_pairs] = generate_validator_set_with_keys(num_validators, 1);
        
        // Initialize light client
        LightClient light_client;
        Blake3Hash finalized_hash = generate_random_hash();
        LightClientState checkpoint(1, validator_set.merkle_root, 10, finalized_hash);
        ASSERT_TRUE(light_client.initialize(checkpoint));
        
        // Test 1: Header with correct previous_hash should verify
        {
            HeaderProof proof = create_valid_header_proof(
                validator_set,
                11,
                finalized_hash,  // Correct previous hash
                validator_key_pairs,
                0.75
            );
            
            bool verified = light_client.verify_header(proof);
            ASSERT_TRUE(verified)
                << "Header with correct previous_hash should verify";
        }
        
        // Test 2: Header with wrong previous_hash should fail
        {
            Blake3Hash wrong_hash = generate_random_hash();
            HeaderProof proof = create_valid_header_proof(
                validator_set,
                11,
                wrong_hash,  // Wrong previous hash
                validator_key_pairs,
                0.75
            );
            
            bool verified = light_client.verify_header(proof);
            ASSERT_FALSE(verified)
                << "Header with wrong previous_hash should not verify";
        }
    }
}

/**
 * Property: State Update After Verification
 * 
 * After successfully verifying a header, the light client state should
 * be updated to the new finalized height and block hash.
 */
TEST_F(LightClientVerificationPropertyTest, StateUpdateAfterVerification) {
    const int NUM_TRIALS = 50;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> size_dist(10, 30);
        size_t num_validators = size_dist(rng_);
        
        auto [validator_set, validator_key_pairs] = generate_validator_set_with_keys(num_validators, 1);
        
        // Initialize light client
        LightClient light_client;
        Blake3Hash genesis_hash = generate_random_hash();
        LightClientState checkpoint(1, validator_set.merkle_root, 0, genesis_hash);
        ASSERT_TRUE(light_client.initialize(checkpoint));
        
        // Verify initial state
        ASSERT_EQ(light_client.state().finalized_height, 0);
        ASSERT_EQ(light_client.state().finalized_block_hash, genesis_hash);
        
        // Create and verify header at height 1
        HeaderProof proof = create_valid_header_proof(
            validator_set,
            1,
            genesis_hash,
            validator_key_pairs,
            0.75
        );
        
        Blake3Hash expected_hash = proof.header.hash();
        bool verified = light_client.verify_header(proof);
        ASSERT_TRUE(verified);
        
        // Check state was updated
        ASSERT_EQ(light_client.state().finalized_height, 1)
            << "Finalized height should be updated to 1";
        ASSERT_EQ(light_client.state().finalized_block_hash, expected_hash)
            << "Finalized block hash should be updated";
    }
}

/**
 * Property: Uninitialized Light Client Rejects All Proofs
 * 
 * A light client that has not been initialized should reject all header proofs.
 */
TEST_F(LightClientVerificationPropertyTest, UninitializedClientRejectsProofs) {
    // Generate validator set
    auto [validator_set, validator_key_pairs] = generate_validator_set_with_keys(10, 1);
    
    // Create uninitialized light client
    LightClient light_client;
    ASSERT_FALSE(light_client.is_initialized());
    
    // Create valid header proof
    Blake3Hash genesis_hash = generate_random_hash();
    HeaderProof proof = create_valid_header_proof(
        validator_set,
        1,
        genesis_hash,
        validator_key_pairs,
        0.75
    );
    
    // Verification should fail because client is not initialized
    bool verified = light_client.verify_header(proof);
    ASSERT_FALSE(verified)
        << "Uninitialized light client should reject all proofs";
}

/**
 * Property: Proof Serialization Round-Trip
 * 
 * Serializing and deserializing a HeaderProof should preserve its value.
 */
TEST_F(LightClientVerificationPropertyTest, ProofSerializationRoundTrip) {
    const int NUM_TRIALS = 50;
    
    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> size_dist(5, 20);
        size_t num_validators = size_dist(rng_);
        
        auto [validator_set, validator_key_pairs] = generate_validator_set_with_keys(num_validators, 1);
        
        // Create header proof
        Blake3Hash genesis_hash = generate_random_hash();
        HeaderProof original_proof = create_valid_header_proof(
            validator_set,
            1,
            genesis_hash,
            validator_key_pairs,
            0.75
        );
        
        // Serialize
        std::vector<uint8_t> serialized = original_proof.serialize();
        
        // Deserialize
        HeaderProof deserialized_proof = HeaderProof::deserialize(serialized);
        
        // Check equality
        ASSERT_EQ(deserialized_proof.header, original_proof.header)
            << "Header changed after serialization round-trip";
        ASSERT_EQ(deserialized_proof.qc, original_proof.qc)
            << "QC changed after serialization round-trip";
        ASSERT_EQ(deserialized_proof.validator_set_merkle_proof.size(),
                  original_proof.validator_set_merkle_proof.size())
            << "Merkle proof size changed after serialization round-trip";
        ASSERT_EQ(deserialized_proof.signing_validators.size(),
                  original_proof.signing_validators.size())
            << "Number of signing validators changed after serialization round-trip";
    }
}
