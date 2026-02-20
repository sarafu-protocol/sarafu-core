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
 * Property-Based Test for Minimum Stake Enforcement
 * 
 * **Validates: Requirements 2.4, 2.7**
 * 
 * Property 6: Minimum Stake Enforcement
 * For any validator in the active set, their bonded stake must be ≥ minimum self-bond requirement.
 * 
 * This test validates that:
 * 1. All active validators meet minimum stake requirement
 * 2. Validators below minimum are never active
 * 3. Validators who fall below minimum are moved to standby
 * 4. Unbonding that would violate minimum is rejected
 * 5. Minimum stake is enforced consistently across epochs
 */
class MinimumStakeEnforcementPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random validator ID
    ValidatorID generate_random_validator_id() {
        std::vector<uint8_t> data(32);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 32; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return Address(data);
    }

    // Generate random stake amount
    uint64_t generate_random_stake(uint64_t min, uint64_t max) {
        std::uniform_int_distribution<uint64_t> dist(min, max);
        return dist(rng_);
    }

    // Generate random BLS key
    BLS12_381_PublicKey generate_random_bls_key() {
        std::vector<uint8_t> data(48);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 48; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return BLS12_381_PublicKey(data);
    }

    // Generate random Ed25519 key
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
 * Property: All active validators meet minimum stake requirement
 * 
 * For any validator set after epoch transition, all validators with
 * status Active have bonded_stake >= minimum_self_bond.
 */
TEST_F(MinimumStakeEnforcementPropertyTest, AllActiveValidatorsMeetMinimum) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random minimum stake
        uint64_t minimum_stake = generate_random_stake(1000, 50000);

        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = minimum_stake;
        ValidatorRegistry registry(config);

        // Add validators with various stakes
        std::uniform_int_distribution<int> validator_count_dist(15, 30);
        int num_validators = validator_count_dist(rng_);

        for (int i = 0; i < num_validators; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            
            // Mix of stakes above and below minimum
            uint64_t stake = generate_random_stake(100, 1000000);
            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Verify all active validators meet minimum
        std::vector<Validator> active_validators = new_set.get_active_validators();
        for (const auto& v : active_validators) {
            ASSERT_GE(v.bonded_stake, minimum_stake)
                << "Active validator has stake " << v.bonded_stake
                << " which is below minimum " << minimum_stake
                << " on trial " << trial;
        }
    }
}

/**
 * Property: Validators below minimum are never active
 * 
 * For any validator with bonded_stake < minimum_self_bond,
 * the validator's status is never Active.
 */
TEST_F(MinimumStakeEnforcementPropertyTest, ValidatorsBelowMinimumNeverActive) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t minimum_stake = 10000;

        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = minimum_stake;
        ValidatorRegistry registry(config);

        // Add validators below minimum
        std::vector<ValidatorID> below_minimum_ids;
        for (int i = 0; i < 10; ++i) {
            ValidatorID id = generate_random_validator_id();
            below_minimum_ids.push_back(id);
            
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(100, 9999);  // Below minimum

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Add validators above minimum
        for (int i = 0; i < 15; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 1000000);  // Above minimum

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Verify none of the below-minimum validators are active
        for (const auto& id : below_minimum_ids) {
            const Validator* v = new_set.find_validator(id);
            ASSERT_NE(v, nullptr) << "Validator not found on trial " << trial;
            ASSERT_NE(v->status, ValidatorStatus::Active)
                << "Validator with stake " << v->bonded_stake
                << " (below minimum " << minimum_stake << ") is active on trial " << trial;
        }
    }
}

/**
 * Property: Validators who fall below minimum are moved to standby
 * 
 * For any active validator whose stake falls below minimum_self_bond,
 * the validator is moved to Standby status at the next epoch transition.
 */
TEST_F(MinimumStakeEnforcementPropertyTest, ValidatorsFallingBelowMinimumMovedToStandby) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t minimum_stake = 10000;

        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = minimum_stake;
        ValidatorRegistry registry(config);

        // Add validators above minimum
        std::vector<ValidatorID> validator_ids;
        for (int i = 0; i < 15; ++i) {
            ValidatorID id = generate_random_validator_id();
            validator_ids.push_back(id);
            
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 100000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1 (all should be eligible)
        ValidatorSet set1 = registry.transition_epoch(1, 10000);
        std::vector<Validator> active1 = set1.get_active_validators();
        ASSERT_EQ(active1.size(), 10);

        // Reduce stake of some active validators below minimum
        std::uniform_int_distribution<int> reduce_count_dist(2, 5);
        int num_to_reduce = reduce_count_dist(rng_);
        
        std::vector<ValidatorID> reduced_ids;
        for (int i = 0; i < num_to_reduce && i < static_cast<int>(active1.size()); ++i) {
            ValidatorID id = active1[i].id;
            reduced_ids.push_back(id);
            
            // Unbond most of the stake (leaving below minimum)
            auto validator = registry.get_validator(id);
            ASSERT_TRUE(validator.has_value());
            
            uint64_t unbond_amount = validator->bonded_stake - generate_random_stake(100, 9999);
            if (unbond_amount > 0) {
                registry.unbond_stake(id, unbond_amount, 0);
            }
        }

        // Transition to epoch 2
        ValidatorSet set2 = registry.transition_epoch(2, 10000);

        // Verify reduced validators are no longer active
        for (const auto& id : reduced_ids) {
            const Validator* v = set2.find_validator(id);
            ASSERT_NE(v, nullptr);
            
            if (v->bonded_stake < minimum_stake) {
                ASSERT_NE(v->status, ValidatorStatus::Active)
                    << "Validator with stake " << v->bonded_stake
                    << " (below minimum) is still active on trial " << trial;
            }
        }
    }
}

/**
 * Property: Unbonding that would violate minimum is rejected
 * 
 * For any validator with bonded_stake S, unbonding amount A where
 * S - A < minimum_self_bond and S - A > 0 is rejected.
 */
TEST_F(MinimumStakeEnforcementPropertyTest, UnbondingViolatingMinimumRejected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t minimum_stake = 10000;

        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = minimum_stake;
        ValidatorRegistry registry(config);

        // Add validator with stake above minimum
        ValidatorID id = generate_random_validator_id();
        BLS12_381_PublicKey consensus_key = generate_random_bls_key();
        Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
        uint64_t initial_stake = generate_random_stake(10000, 100000);

        registry.add_validator(id, consensus_key, withdrawal_key, initial_stake);

        // Try to unbond amount that would leave stake below minimum (but not zero)
        uint64_t unbond_amount = initial_stake - generate_random_stake(1, 9999);
        
        bool result = registry.unbond_stake(id, unbond_amount, 0);

        // Unbonding should be rejected
        ASSERT_FALSE(result)
            << "Unbonding " << unbond_amount << " from " << initial_stake
            << " (leaving below minimum " << minimum_stake << ") was allowed on trial " << trial;

        // Verify stake unchanged
        auto validator = registry.get_validator(id);
        ASSERT_TRUE(validator.has_value());
        ASSERT_EQ(validator->bonded_stake, initial_stake)
            << "Stake changed after rejected unbonding on trial " << trial;
    }
}

/**
 * Property: Unbonding all stake is allowed regardless of minimum
 * 
 * For any validator, unbonding the entire stake (leaving 0) is allowed
 * even though 0 < minimum_self_bond.
 */
TEST_F(MinimumStakeEnforcementPropertyTest, UnbondingAllStakeAllowed) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t minimum_stake = 10000;

        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = minimum_stake;
        ValidatorRegistry registry(config);

        // Add validator
        ValidatorID id = generate_random_validator_id();
        BLS12_381_PublicKey consensus_key = generate_random_bls_key();
        Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
        uint64_t initial_stake = generate_random_stake(10000, 100000);

        registry.add_validator(id, consensus_key, withdrawal_key, initial_stake);

        // Unbond all stake
        bool result = registry.unbond_stake(id, initial_stake, 0);

        // Unbonding all should be allowed
        ASSERT_TRUE(result)
            << "Unbonding all stake was rejected on trial " << trial;

        // Verify stake is now 0
        auto validator = registry.get_validator(id);
        ASSERT_TRUE(validator.has_value());
        ASSERT_EQ(validator->bonded_stake, 0)
            << "Stake not zero after unbonding all on trial " << trial;

        // Verify validator moved to Standby
        ASSERT_EQ(validator->status, ValidatorStatus::Standby)
            << "Validator not moved to Standby after unbonding all on trial " << trial;
    }
}

/**
 * Property: Minimum stake is enforced consistently across epochs
 * 
 * For any sequence of epoch transitions, the minimum stake requirement
 * is enforced at every transition.
 */
TEST_F(MinimumStakeEnforcementPropertyTest, MinimumEnforcedAcrossEpochs) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t minimum_stake = 10000;

        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = minimum_stake;
        ValidatorRegistry registry(config);

        // Add validators
        for (int i = 0; i < 20; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(100, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition through multiple epochs
        std::uniform_int_distribution<int> epoch_dist(3, 10);
        int num_epochs = epoch_dist(rng_);

        for (int epoch = 1; epoch <= num_epochs; ++epoch) {
            ValidatorSet new_set = registry.transition_epoch(epoch, 10000);

            // Verify all active validators meet minimum
            std::vector<Validator> active_validators = new_set.get_active_validators();
            for (const auto& v : active_validators) {
                ASSERT_GE(v.bonded_stake, minimum_stake)
                    << "Active validator has stake " << v.bonded_stake
                    << " below minimum " << minimum_stake
                    << " at epoch " << epoch << " on trial " << trial;
            }
        }
    }
}

/**
 * Property: Bonding stake can bring validator above minimum
 * 
 * For any validator with stake < minimum_self_bond, bonding additional
 * stake to reach >= minimum_self_bond makes them eligible for active set.
 */
TEST_F(MinimumStakeEnforcementPropertyTest, BondingCanBringAboveMinimum) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t minimum_stake = 10000;

        ValidatorRegistry::Config config;
        config.active_validator_count = 5;
        config.minimum_self_bond = minimum_stake;
        ValidatorRegistry registry(config);

        // Add validator below minimum
        ValidatorID id = generate_random_validator_id();
        BLS12_381_PublicKey consensus_key = generate_random_bls_key();
        Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
        uint64_t initial_stake = generate_random_stake(1000, 9999);

        registry.add_validator(id, consensus_key, withdrawal_key, initial_stake);

        // Add some other validators above minimum
        for (int i = 0; i < 3; ++i) {
            ValidatorID other_id = generate_random_validator_id();
            BLS12_381_PublicKey other_consensus = generate_random_bls_key();
            Ed25519_PublicKey other_withdrawal = generate_random_ed25519_key();
            uint64_t other_stake = generate_random_stake(10000, 50000);

            registry.add_validator(other_id, other_consensus, other_withdrawal, other_stake);
        }

        // Transition to epoch 1 - validator should not be active
        ValidatorSet set1 = registry.transition_epoch(1, 10000);
        const Validator* v1 = set1.find_validator(id);
        ASSERT_NE(v1, nullptr);
        ASSERT_NE(v1->status, ValidatorStatus::Active)
            << "Validator below minimum is active on trial " << trial;

        // Bond additional stake to reach above minimum
        uint64_t bond_amount = minimum_stake - initial_stake + generate_random_stake(1, 10000);
        bool bond_result = registry.bond_stake(id, bond_amount);
        ASSERT_TRUE(bond_result);

        // Transition to epoch 2 - validator should now be eligible
        ValidatorSet set2 = registry.transition_epoch(2, 10000);
        const Validator* v2 = set2.find_validator(id);
        ASSERT_NE(v2, nullptr);
        ASSERT_GE(v2->bonded_stake, minimum_stake)
            << "Bonded stake not above minimum on trial " << trial;

        // Validator should be active (since we have < 5 validators above minimum)
        ASSERT_EQ(v2->status, ValidatorStatus::Active)
            << "Validator above minimum not active on trial " << trial;
    }
}

/**
 * Property: Minimum stake requirement is independent per validator
 * 
 * For any two validators A and B, A meeting the minimum stake requirement
 * does not affect whether B meets the requirement.
 */
TEST_F(MinimumStakeEnforcementPropertyTest, MinimumEnforcementIsIndependent) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t minimum_stake = 10000;

        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = minimum_stake;
        ValidatorRegistry registry(config);

        // Add validator A above minimum
        ValidatorID id_a = generate_random_validator_id();
        BLS12_381_PublicKey key_a = generate_random_bls_key();
        Ed25519_PublicKey withdrawal_a = generate_random_ed25519_key();
        uint64_t stake_a = generate_random_stake(10000, 100000);
        registry.add_validator(id_a, key_a, withdrawal_a, stake_a);

        // Add validator B below minimum
        ValidatorID id_b = generate_random_validator_id();
        BLS12_381_PublicKey key_b = generate_random_bls_key();
        Ed25519_PublicKey withdrawal_b = generate_random_ed25519_key();
        uint64_t stake_b = generate_random_stake(100, 9999);
        registry.add_validator(id_b, key_b, withdrawal_b, stake_b);

        // Add more validators to fill active set
        for (int i = 0; i < 10; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 100000);
            registry.add_validator(id, key, withdrawal, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Verify A can be active (meets minimum)
        const Validator* v_a = new_set.find_validator(id_a);
        ASSERT_NE(v_a, nullptr);
        ASSERT_GE(v_a->bonded_stake, minimum_stake);

        // Verify B cannot be active (below minimum)
        const Validator* v_b = new_set.find_validator(id_b);
        ASSERT_NE(v_b, nullptr);
        ASSERT_LT(v_b->bonded_stake, minimum_stake);
        ASSERT_NE(v_b->status, ValidatorStatus::Active)
            << "Validator B below minimum is active on trial " << trial;
    }
}

/**
 * Property: Zero stake validators are never active
 * 
 * For any validator with bonded_stake = 0, the validator is never active.
 */
TEST_F(MinimumStakeEnforcementPropertyTest, ZeroStakeValidatorsNeverActive) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validator with zero stake
        ValidatorID zero_id = generate_random_validator_id();
        BLS12_381_PublicKey zero_key = generate_random_bls_key();
        Ed25519_PublicKey zero_withdrawal = generate_random_ed25519_key();
        registry.add_validator(zero_id, zero_key, zero_withdrawal, 0);

        // Add validators with positive stake
        for (int i = 0; i < 15; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(1000, 100000);
            registry.add_validator(id, key, withdrawal, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Verify zero-stake validator is not active
        const Validator* v = new_set.find_validator(zero_id);
        ASSERT_NE(v, nullptr);
        ASSERT_EQ(v->bonded_stake, 0);
        ASSERT_NE(v->status, ValidatorStatus::Active)
            << "Zero-stake validator is active on trial " << trial;
    }
}
