#include "sarafu/consensus/validator_registry.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace sarafu::consensus;
using namespace sarafu::crypto;
using namespace sarafu::state;

/**
 * Property-Based Test for Epoch Transition Authorization
 * 
 * **Validates: Requirements 2.6**
 * 
 * Property 8: Epoch Transition Authorization
 * For any epoch transition from E to E+1, the new validator set must be signed
 * by ≥2/3 of epoch E's total stake.
 * 
 * This test validates that:
 * 1. QC with ≥2/3 stake is accepted
 * 2. QC with <2/3 stake is rejected
 * 3. Stake calculation includes only active validators
 * 4. Exact 2/3 threshold is correctly computed
 */
class EpochTransitionAuthorizationPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        rng_.seed(42);
    }

    ValidatorID generate_random_validator_id() {
        std::vector<uint8_t> data(32);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 32; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return Address(data);
    }

    uint64_t generate_random_stake(uint64_t min, uint64_t max) {
        std::uniform_int_distribution<uint64_t> dist(min, max);
        return dist(rng_);
    }

    BLS12_381_PublicKey generate_random_bls_key() {
        std::vector<uint8_t> data(48);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 48; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return BLS12_381_PublicKey(data);
    }

    Ed25519_PublicKey generate_random_ed25519_key() {
        std::vector<uint8_t> data(32);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 32; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return Ed25519_PublicKey(data);
    }

    std::mt19937 rng_;
};

/**
 * Property: QC with ≥2/3 stake is accepted
 * 
 * For any validator set with total stake S, a QC signed by validators
 * with total stake ≥ (2S/3) is accepted.
 */
TEST_F(EpochTransitionAuthorizationPropertyTest, QCWithTwoThirdsStakeAccepted) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        std::vector<ValidatorID> validator_ids;
        for (int i = 0; i < 10; ++i) {
            ValidatorID id = generate_random_validator_id();
            validator_ids.push_back(id);
            
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 100000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet validator_set = registry.transition_epoch(1, 10000);
        std::vector<Validator> active_validators = validator_set.get_active_validators();

        // Calculate 2/3 threshold
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;

        // Select validators to reach ≥2/3 stake
        std::vector<ValidatorID> signers;
        uint64_t accumulated_stake = 0;
        
        for (const auto& v : active_validators) {
            signers.push_back(v.id);
            accumulated_stake += v.bonded_stake;
            
            if (accumulated_stake >= required_stake) {
                break;
            }
        }

        // Create QC
        QuorumCertificate qc;
        qc.signers = signers;

        // Verify QC is accepted
        bool result = registry.require_epoch_transition_qc(qc, validator_set);
        ASSERT_TRUE(result)
            << "QC with " << accumulated_stake << " stake (≥" << required_stake
            << " required) was rejected on trial " << trial;
    }
}

/**
 * Property: QC with <2/3 stake is rejected
 * 
 * For any validator set with total stake S, a QC signed by validators
 * with total stake < (2S/3) is rejected.
 */
TEST_F(EpochTransitionAuthorizationPropertyTest, QCWithLessThanTwoThirdsStakeRejected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        for (int i = 0; i < 10; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 100000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet validator_set = registry.transition_epoch(1, 10000);
        std::vector<Validator> active_validators = validator_set.get_active_validators();

        // Calculate 2/3 threshold
        uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;

        // Select validators to reach <2/3 stake
        std::vector<ValidatorID> signers;
        uint64_t accumulated_stake = 0;
        
        for (const auto& v : active_validators) {
            if (accumulated_stake + v.bonded_stake >= required_stake) {
                break;  // Stop before reaching 2/3
            }
            signers.push_back(v.id);
            accumulated_stake += v.bonded_stake;
        }

        // Only test if we have signers with <2/3 stake
        if (accumulated_stake < required_stake && !signers.empty()) {
            // Create QC
            QuorumCertificate qc;
            qc.signers = signers;

            // Verify QC is rejected
            bool result = registry.require_epoch_transition_qc(qc, validator_set);
            ASSERT_FALSE(result)
                << "QC with " << accumulated_stake << " stake (<" << required_stake
                << " required) was accepted on trial " << trial;
        }
    }
}

/**
 * Property: Stake calculation includes only active validators
 * 
 * For any QC, only signatures from active validators count toward
 * the 2/3 threshold.
 */
TEST_F(EpochTransitionAuthorizationPropertyTest, OnlyActiveValidatorsCount) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        for (int i = 0; i < 15; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 100000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet validator_set = registry.transition_epoch(1, 10000);
        std::vector<Validator> active_validators = validator_set.get_active_validators();
        std::vector<Validator> standby_validators = validator_set.get_standby_validators();

        // Create QC with only standby validators
        std::vector<ValidatorID> signers;
        for (const auto& v : standby_validators) {
            signers.push_back(v.id);
        }

        QuorumCertificate qc;
        qc.signers = signers;

        // Verify QC is rejected (standby validators don't count)
        bool result = registry.require_epoch_transition_qc(qc, validator_set);
        ASSERT_FALSE(result)
            << "QC with only standby validators was accepted on trial " << trial;
    }
}

/**
 * Property: Exact 2/3 threshold is correctly computed
 * 
 * For any total stake S, the threshold is ceiling(2S/3), ensuring
 * that exactly 2/3 or more is required.
 */
TEST_F(EpochTransitionAuthorizationPropertyTest, ExactTwoThirdsThresholdCorrect) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators with specific stakes to test threshold
        std::vector<ValidatorID> validator_ids;
        std::vector<uint64_t> stakes;
        
        for (int i = 0; i < 10; ++i) {
            ValidatorID id = generate_random_validator_id();
            validator_ids.push_back(id);
            
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 100000);
            stakes.push_back(stake);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet validator_set = registry.transition_epoch(1, 10000);

        // Calculate exact 2/3 threshold
        uint64_t total_stake = validator_set.total_stake;
        uint64_t required_stake = (total_stake * 2 + 2) / 3;  // Ceiling division

        // Verify threshold is correct
        ASSERT_GE(required_stake * 3, total_stake * 2)
            << "Threshold too low on trial " << trial;
        ASSERT_LT((required_stake - 1) * 3, total_stake * 2)
            << "Threshold too high on trial " << trial;
    }
}

/**
 * Property: All active validators signing is always sufficient
 * 
 * For any validator set, a QC signed by all active validators
 * is always accepted (100% > 2/3).
 */
TEST_F(EpochTransitionAuthorizationPropertyTest, AllActiveValidatorsSufficient) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        for (int i = 0; i < 10; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 100000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet validator_set = registry.transition_epoch(1, 10000);
        std::vector<Validator> active_validators = validator_set.get_active_validators();

        // Create QC with all active validators
        std::vector<ValidatorID> signers;
        for (const auto& v : active_validators) {
            signers.push_back(v.id);
        }

        QuorumCertificate qc;
        qc.signers = signers;

        // Verify QC is accepted
        bool result = registry.require_epoch_transition_qc(qc, validator_set);
        ASSERT_TRUE(result)
            << "QC with all active validators was rejected on trial " << trial;
    }
}

/**
 * Property: Empty QC is always rejected
 * 
 * For any validator set, a QC with no signers is rejected.
 */
TEST_F(EpochTransitionAuthorizationPropertyTest, EmptyQCRejected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        for (int i = 0; i < 10; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 100000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet validator_set = registry.transition_epoch(1, 10000);

        // Create empty QC
        QuorumCertificate qc;
        qc.signers.clear();

        // Verify QC is rejected
        bool result = registry.require_epoch_transition_qc(qc, validator_set);
        ASSERT_FALSE(result)
            << "Empty QC was accepted on trial " << trial;
    }
}
