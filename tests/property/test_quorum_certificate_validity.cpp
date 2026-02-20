#include "sarafu/consensus/block.h"
#include "sarafu/consensus/vote_aggregator.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/blake3_hash.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <algorithm>

using namespace sarafu::consensus;
using namespace sarafu::crypto;
using namespace sarafu::state;

/**
 * Property-Based Test for Quorum Certificate Validity
 * 
 * **Validates: Requirements 1.2, 9.3**
 * 
 * Property 2: Quorum Certificate Validity
 * For any Quorum Certificate QC, the total stake of signers must be ≥2/3 of total stake,
 * and the aggregated BLS signature must be valid for the block hash.
 * 
 * This test validates that:
 * 1. QCs can only be created when ≥2/3 stake has voted
 * 2. QC aggregated signatures verify correctly against the block hash
 * 3. QCs with <2/3 stake cannot be created
 * 4. QC signatures are bound to the specific block hash
 * 5. QC verification fails with wrong validator set
 * 6. QC total_stake_signed accurately reflects signing validators
 */
class QuorumCertificateValidityPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random data of specified size
    std::vector<uint8_t> generate_random_data(size_t size) {
        std::vector<uint8_t> data(size);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return data;
    }

    // Generate a random validator set with specified number of validators
    ValidatorSet generate_validator_set(size_t num_validators, uint64_t stake_per_validator = 1000000) {
        std::vector<Validator> validators;
        validators.reserve(num_validators);

        uint64_t total_stake = 0;

        for (size_t i = 0; i < num_validators; ++i) {
            // Generate keys
            auto [bls_public_key, bls_private_key] = BLS12_381::generate_keypair();
            auto [ed_public_key, ed_private_key] = Ed25519::generate_keypair();

            // Create validator ID (address)
            std::vector<uint8_t> id_bytes = generate_random_data(32);
            ValidatorID id(id_bytes);

            // Vary stake slightly for realism
            std::uniform_int_distribution<uint64_t> stake_dist(
                stake_per_validator * 8 / 10,
                stake_per_validator * 12 / 10
            );
            uint64_t stake = stake_dist(rng_);

            Validator validator(id, bls_public_key, ed_public_key, stake);
            validator.status = ValidatorStatus::Active;

            validators.push_back(validator);
            total_stake += stake;

            // Store private key for signing
            validator_private_keys_[id] = bls_private_key;
        }

        return ValidatorSet(0, validators, total_stake);
    }

    // Generate a random block hash
    Blake3Hash generate_random_block_hash() {
        return Blake3Hash(generate_random_data(32));
    }

    // Create a vote from a validator
    Vote create_vote(
        const ValidatorID& validator_id,
        uint64_t block_height,
        const Blake3Hash& block_hash,
        uint64_t view_number
    ) {
        // Get validator's private key
        auto it = validator_private_keys_.find(validator_id);
        if (it == validator_private_keys_.end()) {
            throw std::runtime_error("Validator private key not found");
        }

        // Sign the block hash
        std::vector<uint8_t> message = block_hash.serialize();
        BLS12_381_Signature signature = BLS12_381::sign(message, it->second);

        return Vote(validator_id, block_height, block_hash, view_number, signature);
    }

    // Verify a QC against a validator set
    bool verify_qc(const QuorumCertificate& qc, const ValidatorSet& validator_set) {
        // Check supermajority (≥2/3 stake)
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;
        if (qc.total_stake_signed < required_stake) {
            return false;
        }

        // Collect public keys of signers
        std::vector<BLS12_381_PublicKey> signer_public_keys;
        for (const auto& signer_id : qc.signers) {
            bool found = false;
            for (const auto& validator : validator_set.validators) {
                if (validator.id == signer_id) {
                    signer_public_keys.push_back(validator.consensus_key);
                    found = true;
                    break;
                }
            }
            if (!found) {
                return false;  // Signer not in validator set
            }
        }

        // Verify aggregated signature
        std::vector<uint8_t> message = qc.block_hash.serialize();
        return BLS12_381::verify_aggregated(qc.aggregated_signature, message, signer_public_keys);
    }

    std::mt19937 rng_;
    std::map<ValidatorID, BLS12_381_PrivateKey> validator_private_keys_;
};

/**
 * Property 2: Quorum Certificate Validity - Supermajority Requirement
 * 
 * For any QC created by VoteAggregator, the total stake of signers
 * must be ≥2/3 of total stake.
 */
TEST_F(QuorumCertificateValidityPropertyTest, QCRequiresSupermajority) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random validator set (10-100 validators)
        std::uniform_int_distribution<size_t> validator_dist(10, 100);
        size_t num_validators = validator_dist(rng_);

        ValidatorSet validator_set = generate_validator_set(num_validators);
        VoteAggregator aggregator(validator_set);

        // Generate random block
        uint64_t block_height = 100;
        Blake3Hash block_hash = generate_random_block_hash();
        uint64_t view_number = 1;

        // Calculate required stake for supermajority
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;

        // Add votes from random subset of validators
        std::uniform_real_distribution<double> fraction_dist(0.5, 1.0);
        double signing_fraction = fraction_dist(rng_);
        size_t num_signers = static_cast<size_t>(num_validators * signing_fraction);

        // Shuffle validators and select first num_signers
        std::vector<Validator> shuffled_validators = validator_set.validators;
        std::shuffle(shuffled_validators.begin(), shuffled_validators.end(), rng_);

        uint64_t accumulated_stake = 0;
        for (size_t i = 0; i < num_signers; ++i) {
            Vote vote = create_vote(
                shuffled_validators[i].id,
                block_height,
                block_hash,
                view_number
            );
            aggregator.add_vote(vote);
            accumulated_stake += shuffled_validators[i].bonded_stake;
        }

        // Try to aggregate votes
        auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);

        if (accumulated_stake >= required_stake) {
            // Should create QC
            ASSERT_TRUE(qc_opt.has_value())
                << "Failed to create QC with " << accumulated_stake << " stake (required: " << required_stake << ")";

            QuorumCertificate qc = qc_opt.value();

            // Verify QC has correct stake
            ASSERT_GE(qc.total_stake_signed, required_stake)
                << "QC total_stake_signed is below supermajority threshold";

            // Verify QC has correct number of signers
            ASSERT_EQ(qc.signers.size(), num_signers)
                << "QC signers count mismatch";
        } else {
            // Should not create QC
            ASSERT_FALSE(qc_opt.has_value())
                << "Created QC with insufficient stake: " << accumulated_stake << " (required: " << required_stake << ")";
        }
    }
}

/**
 * Property 2: Quorum Certificate Validity - Signature Verification
 * 
 * For any QC, the aggregated BLS signature must verify correctly
 * against the block hash and the signing validators' public keys.
 */
TEST_F(QuorumCertificateValidityPropertyTest, QCSignatureVerification) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random validator set (10-100 validators)
        std::uniform_int_distribution<size_t> validator_dist(10, 100);
        size_t num_validators = validator_dist(rng_);

        ValidatorSet validator_set = generate_validator_set(num_validators);
        VoteAggregator aggregator(validator_set);

        // Generate random block
        uint64_t block_height = 100;
        Blake3Hash block_hash = generate_random_block_hash();
        uint64_t view_number = 1;

        // Calculate required stake
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;

        // Add votes until we reach supermajority
        uint64_t accumulated_stake = 0;
        size_t num_signers = 0;
        for (const auto& validator : validator_set.validators) {
            Vote vote = create_vote(
                validator.id,
                block_height,
                block_hash,
                view_number
            );
            aggregator.add_vote(vote);
            accumulated_stake += validator.bonded_stake;
            num_signers++;

            if (accumulated_stake >= required_stake) {
                break;
            }
        }

        // Aggregate votes into QC
        auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
        ASSERT_TRUE(qc_opt.has_value()) << "Failed to create QC with " << accumulated_stake << " stake (required: " << required_stake << ")";

        QuorumCertificate qc = qc_opt.value();

        // Verify QC signature
        bool is_valid = verify_qc(qc, validator_set);
        ASSERT_TRUE(is_valid)
            << "QC signature verification failed for " << num_signers << " signers";
    }
}

/**
 * Property: QC signature is bound to block hash
 * 
 * A QC created for one block hash should not verify for a different block hash.
 */
TEST_F(QuorumCertificateValidityPropertyTest, QCBoundToBlockHash) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> validator_dist(10, 50);
        size_t num_validators = validator_dist(rng_);

        ValidatorSet validator_set = generate_validator_set(num_validators);
        VoteAggregator aggregator(validator_set);

        // Generate two different block hashes
        uint64_t block_height = 100;
        Blake3Hash block_hash1 = generate_random_block_hash();
        Blake3Hash block_hash2 = generate_random_block_hash();
        uint64_t view_number = 1;

        // Ensure hashes are different
        while (block_hash1 == block_hash2) {
            block_hash2 = generate_random_block_hash();
        }

        // Add votes for block_hash1 from all validators
        for (const auto& validator : validator_set.validators) {
            Vote vote = create_vote(
                validator.id,
                block_height,
                block_hash1,
                view_number
            );
            aggregator.add_vote(vote);
        }

        // Create QC for block_hash1
        auto qc_opt = aggregator.aggregate_votes(block_height, block_hash1, view_number);
        ASSERT_TRUE(qc_opt.has_value());

        QuorumCertificate qc = qc_opt.value();

        // Verify QC with correct block hash (should succeed)
        ASSERT_TRUE(verify_qc(qc, validator_set))
            << "QC verification failed with correct block hash";

        // Modify QC to use different block hash
        QuorumCertificate modified_qc = qc;
        modified_qc.block_hash = block_hash2;

        // Verify modified QC (should fail)
        bool is_valid_wrong_hash = verify_qc(modified_qc, validator_set);
        ASSERT_FALSE(is_valid_wrong_hash)
            << "QC incorrectly verified with wrong block hash";
    }
}

/**
 * Property: QC verification fails with wrong validator set
 * 
 * A QC should only verify against the validator set that created it.
 */
TEST_F(QuorumCertificateValidityPropertyTest, QCBoundToValidatorSet) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate two different validator sets
        std::uniform_int_distribution<size_t> validator_dist(10, 50);
        size_t num_validators = validator_dist(rng_);

        ValidatorSet validator_set1 = generate_validator_set(num_validators);
        ValidatorSet validator_set2 = generate_validator_set(num_validators);

        VoteAggregator aggregator(validator_set1);

        // Generate block
        uint64_t block_height = 100;
        Blake3Hash block_hash = generate_random_block_hash();
        uint64_t view_number = 1;

        // Add votes from all validators in set 1
        for (const auto& validator : validator_set1.validators) {
            Vote vote = create_vote(
                validator.id,
                block_height,
                block_hash,
                view_number
            );
            aggregator.add_vote(vote);
        }

        // Create QC
        auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
        ASSERT_TRUE(qc_opt.has_value());

        QuorumCertificate qc = qc_opt.value();

        // Verify with correct validator set (should succeed)
        ASSERT_TRUE(verify_qc(qc, validator_set1))
            << "QC verification failed with correct validator set";

        // Verify with wrong validator set (should fail)
        bool is_valid_wrong_set = verify_qc(qc, validator_set2);
        ASSERT_FALSE(is_valid_wrong_set)
            << "QC incorrectly verified with wrong validator set";
    }
}

/**
 * Property: QC total_stake_signed is accurate
 * 
 * The total_stake_signed field in a QC must accurately reflect
 * the sum of stakes from all signing validators.
 */
TEST_F(QuorumCertificateValidityPropertyTest, QCTotalStakeAccurate) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> validator_dist(10, 100);
        size_t num_validators = validator_dist(rng_);

        ValidatorSet validator_set = generate_validator_set(num_validators);
        VoteAggregator aggregator(validator_set);

        // Generate block
        uint64_t block_height = 100;
        Blake3Hash block_hash = generate_random_block_hash();
        uint64_t view_number = 1;

        // Calculate required stake
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;

        // Add votes until we reach supermajority
        uint64_t expected_total_stake = 0;
        size_t num_signers = 0;
        for (const auto& validator : validator_set.validators) {
            Vote vote = create_vote(
                validator.id,
                block_height,
                block_hash,
                view_number
            );
            aggregator.add_vote(vote);
            expected_total_stake += validator.bonded_stake;
            num_signers++;

            if (expected_total_stake >= required_stake) {
                break;
            }
        }

        // Create QC
        auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
        ASSERT_TRUE(qc_opt.has_value()) << "Failed to create QC with " << expected_total_stake << " stake (required: " << required_stake << ")";

        QuorumCertificate qc = qc_opt.value();

        // Verify total_stake_signed matches expected
        ASSERT_EQ(qc.total_stake_signed, expected_total_stake)
            << "QC total_stake_signed does not match sum of signing validators' stakes";
    }
}

/**
 * Property: QC with exactly 2/3 stake is valid
 * 
 * A QC with exactly the minimum required stake (2/3) should be valid.
 * This tests the boundary condition.
 */
TEST_F(QuorumCertificateValidityPropertyTest, QCWithExactlyTwoThirdsStake) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set with uniform stakes for easier calculation
        std::uniform_int_distribution<size_t> validator_dist(10, 50);
        size_t num_validators = validator_dist(rng_);

        // Use uniform stake to make 2/3 calculation precise
        uint64_t stake_per_validator = 1000000;
        ValidatorSet validator_set = generate_validator_set(num_validators, stake_per_validator);

        // Recalculate total stake (may vary slightly due to random variation)
        uint64_t actual_total_stake = 0;
        for (const auto& v : validator_set.validators) {
            actual_total_stake += v.bonded_stake;
        }
        validator_set.total_stake = actual_total_stake;

        VoteAggregator aggregator(validator_set);

        // Generate block
        uint64_t block_height = 100;
        Blake3Hash block_hash = generate_random_block_hash();
        uint64_t view_number = 1;

        // Calculate required stake (ceiling of 2/3)
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;

        // Add votes until we reach or just exceed required stake
        uint64_t accumulated_stake = 0;
        size_t num_signers = 0;
        for (const auto& validator : validator_set.validators) {
            if (accumulated_stake >= required_stake) {
                break;
            }
            Vote vote = create_vote(
                validator.id,
                block_height,
                block_hash,
                view_number
            );
            aggregator.add_vote(vote);
            accumulated_stake += validator.bonded_stake;
            num_signers++;
        }

        // Should be able to create QC
        auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
        ASSERT_TRUE(qc_opt.has_value())
            << "Failed to create QC with stake " << accumulated_stake << " (required: " << required_stake << ")";

        QuorumCertificate qc = qc_opt.value();

        // Verify QC is valid
        ASSERT_TRUE(verify_qc(qc, validator_set))
            << "QC with exactly 2/3 stake failed verification";
    }
}

/**
 * Property: QC with less than 2/3 stake cannot be created
 * 
 * VoteAggregator should not create a QC if the accumulated stake
 * is below the 2/3 threshold.
 */
TEST_F(QuorumCertificateValidityPropertyTest, QCRequiresAtLeastTwoThirdsStake) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> validator_dist(10, 50);
        size_t num_validators = validator_dist(rng_);

        ValidatorSet validator_set = generate_validator_set(num_validators);
        VoteAggregator aggregator(validator_set);

        // Generate block
        uint64_t block_height = 100;
        Blake3Hash block_hash = generate_random_block_hash();
        uint64_t view_number = 1;

        // Calculate required stake
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;

        // Add votes from subset that's definitely less than 2/3
        std::uniform_real_distribution<double> fraction_dist(0.3, 0.65);
        double signing_fraction = fraction_dist(rng_);
        size_t num_signers = static_cast<size_t>(num_validators * signing_fraction);

        uint64_t accumulated_stake = 0;
        for (size_t i = 0; i < num_signers; ++i) {
            Vote vote = create_vote(
                validator_set.validators[i].id,
                block_height,
                block_hash,
                view_number
            );
            aggregator.add_vote(vote);
            accumulated_stake += validator_set.validators[i].bonded_stake;
        }

        // Only test if we're actually below threshold
        if (accumulated_stake < required_stake) {
            // Should not create QC
            auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
            ASSERT_FALSE(qc_opt.has_value())
                << "Created QC with insufficient stake: " << accumulated_stake << " (required: " << required_stake << ")";
        }
    }
}

/**
 * Property: QC serialization preserves validity
 * 
 * Serializing and deserializing a QC should preserve its validity.
 */
TEST_F(QuorumCertificateValidityPropertyTest, QCSerializationPreservesValidity) {
    const int NUM_TRIALS = 150;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate validator set
        std::uniform_int_distribution<size_t> validator_dist(10, 50);
        size_t num_validators = validator_dist(rng_);

        ValidatorSet validator_set = generate_validator_set(num_validators);
        VoteAggregator aggregator(validator_set);

        // Generate block
        uint64_t block_height = 100;
        Blake3Hash block_hash = generate_random_block_hash();
        uint64_t view_number = 1;

        // Add votes from all validators
        for (const auto& validator : validator_set.validators) {
            Vote vote = create_vote(
                validator.id,
                block_height,
                block_hash,
                view_number
            );
            aggregator.add_vote(vote);
        }

        // Create QC
        auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
        ASSERT_TRUE(qc_opt.has_value());

        QuorumCertificate original_qc = qc_opt.value();

        // Verify original QC
        ASSERT_TRUE(verify_qc(original_qc, validator_set));

        // Serialize
        std::vector<uint8_t> serialized = original_qc.serialize();

        // Deserialize
        QuorumCertificate deserialized_qc = QuorumCertificate::deserialize(serialized);

        // Verify deserialized QC
        ASSERT_TRUE(verify_qc(deserialized_qc, validator_set))
            << "Deserialized QC failed verification";

        // QCs should be equal
        ASSERT_EQ(original_qc, deserialized_qc);
    }
}

/**
 * Property: QC works with maximum validator set size
 * 
 * The system should handle QC creation and verification for up to 500 validators.
 */
TEST_F(QuorumCertificateValidityPropertyTest, QCWithMaximumValidatorSetSize) {
    const int NUM_TRIALS = 5;  // Fewer trials due to computational cost

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        const size_t MAX_VALIDATORS = 500;

        ValidatorSet validator_set = generate_validator_set(MAX_VALIDATORS);
        VoteAggregator aggregator(validator_set);

        // Generate block
        uint64_t block_height = 100;
        Blake3Hash block_hash = generate_random_block_hash();
        uint64_t view_number = 1;

        // Add votes from all validators
        for (const auto& validator : validator_set.validators) {
            Vote vote = create_vote(
                validator.id,
                block_height,
                block_hash,
                view_number
            );
            aggregator.add_vote(vote);
        }

        // Create QC
        auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
        ASSERT_TRUE(qc_opt.has_value())
            << "Failed to create QC with 500 validators";

        QuorumCertificate qc = qc_opt.value();

        // Verify QC
        ASSERT_TRUE(verify_qc(qc, validator_set))
            << "QC verification failed for 500 validators";

        // Verify all validators are in signers list
        ASSERT_EQ(qc.signers.size(), MAX_VALIDATORS)
            << "QC signers count incorrect for 500 validators";
    }
}
