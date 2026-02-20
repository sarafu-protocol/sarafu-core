#include "sarafu/consensus/validator_registry.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <algorithm>

using namespace sarafu::consensus;
using namespace sarafu::crypto;
using namespace sarafu::state;

/**
 * Property-Based Test for Top-N Validator Selection
 * 
 * **Validates: Requirements 2.2, 2.3**
 * 
 * Property 5: Top-N Selection
 * For any epoch transition, the active validator set contains exactly the top N validators
 * by bonded stake, with lexicographic address ordering for ties.
 * 
 * This test validates that:
 * 1. Exactly N validators are selected as active
 * 2. Active validators have the highest stakes
 * 3. Ties are broken by lexicographic validator ID ordering
 * 4. Validators below minimum stake are excluded
 * 5. Jailed and tombstoned validators are excluded
 * 6. Selection is deterministic for the same input
 */
class TopNSelectionPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        
        // Configure registry with N=10 active validators
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        config.blocks_per_epoch = 10000;
        
        registry_ = std::make_unique<ValidatorRegistry>(config);
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
    std::unique_ptr<ValidatorRegistry> registry_;
};

/**
 * Property: Exactly N validators are selected as active
 * 
 * For any validator set with M >= N validators meeting minimum stake,
 * exactly N validators are marked as Active after epoch transition.
 */
TEST_F(TopNSelectionPropertyTest, ExactlyNValidatorsSelected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Create fresh registry for each trial
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        config.blocks_per_epoch = 10000;
        ValidatorRegistry registry(config);

        // Generate random number of validators (15-50)
        std::uniform_int_distribution<int> validator_count_dist(15, 50);
        int num_validators = validator_count_dist(rng_);

        // Add validators with random stakes
        for (int i = 0; i < num_validators; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(1000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Count active validators
        int active_count = 0;
        for (const auto& validator : new_set.validators) {
            if (validator.status == ValidatorStatus::Active) {
                active_count++;
            }
        }

        ASSERT_EQ(active_count, 10)
            << "Expected exactly 10 active validators, got " << active_count
            << " on trial " << trial;
    }
}

/**
 * Property: Active validators have the highest stakes
 * 
 * For any epoch transition, all active validators have stake >= all standby validators.
 */
TEST_F(TopNSelectionPropertyTest, ActiveValidatorsHaveHighestStakes) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Generate validators with distinct stakes
        std::uniform_int_distribution<int> validator_count_dist(15, 30);
        int num_validators = validator_count_dist(rng_);

        for (int i = 0; i < num_validators; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(1000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Get active and standby validators
        std::vector<Validator> active_validators = new_set.get_active_validators();
        std::vector<Validator> standby_validators = new_set.get_standby_validators();

        // Find minimum stake among active validators
        uint64_t min_active_stake = UINT64_MAX;
        for (const auto& v : active_validators) {
            if (v.bonded_stake < min_active_stake) {
                min_active_stake = v.bonded_stake;
            }
        }

        // Verify all standby validators have stake <= min_active_stake
        for (const auto& v : standby_validators) {
            ASSERT_LE(v.bonded_stake, min_active_stake)
                << "Standby validator has stake " << v.bonded_stake
                << " which is greater than minimum active stake " << min_active_stake
                << " on trial " << trial;
        }
    }
}

/**
 * Property: Ties are broken by lexicographic validator ID ordering
 * 
 * For any two validators with equal stake, the one with lexicographically
 * smaller ID is ranked higher.
 */
TEST_F(TopNSelectionPropertyTest, TiesBrokenByLexicographicOrdering) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 5;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Create validators with identical stakes
        uint64_t common_stake = 100000;
        std::vector<ValidatorID> validator_ids;

        // Generate 10 validators with same stake
        for (int i = 0; i < 10; ++i) {
            ValidatorID id = generate_random_validator_id();
            validator_ids.push_back(id);
            
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();

            registry.add_validator(id, consensus_key, withdrawal_key, common_stake);
        }

        // Sort IDs lexicographically
        std::sort(validator_ids.begin(), validator_ids.end());

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Get active validators
        std::vector<Validator> active_validators = new_set.get_active_validators();
        ASSERT_EQ(active_validators.size(), 5);

        // Verify active validators are the first 5 in lexicographic order
        for (size_t i = 0; i < 5; ++i) {
            bool found = false;
            for (const auto& v : active_validators) {
                if (v.id == validator_ids[i]) {
                    found = true;
                    break;
                }
            }
            ASSERT_TRUE(found)
                << "Expected validator " << i << " to be active (lexicographic tie-breaking)"
                << " on trial " << trial;
        }
    }
}

/**
 * Property: Validators below minimum stake are excluded
 * 
 * For any validator with stake < minimum_self_bond, the validator
 * is not selected as active.
 */
TEST_F(TopNSelectionPropertyTest, ValidatorsBelowMinimumStakeExcluded) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 10000;
        ValidatorRegistry registry(config);

        // Add validators with stakes both above and below minimum
        std::uniform_int_distribution<int> validator_count_dist(15, 25);
        int num_validators = validator_count_dist(rng_);

        for (int i = 0; i < num_validators; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            
            // 30% chance of stake below minimum
            uint64_t stake;
            if (i < num_validators / 3) {
                stake = generate_random_stake(100, 9999);  // Below minimum
            } else {
                stake = generate_random_stake(10000, 1000000);  // Above minimum
            }

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Verify no active validator has stake below minimum
        std::vector<Validator> active_validators = new_set.get_active_validators();
        for (const auto& v : active_validators) {
            ASSERT_GE(v.bonded_stake, 10000)
                << "Active validator has stake " << v.bonded_stake
                << " which is below minimum 10000 on trial " << trial;
        }
    }
}

/**
 * Property: Jailed validators are excluded from active set
 * 
 * For any validator with status Jailed, the validator is not selected
 * as active even if they have sufficient stake.
 */
TEST_F(TopNSelectionPropertyTest, JailedValidatorsExcluded) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add 15 validators
        std::vector<ValidatorID> validator_ids;
        for (int i = 0; i < 15; ++i) {
            ValidatorID id = generate_random_validator_id();
            validator_ids.push_back(id);
            
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Jail 3 random validators
        std::uniform_int_distribution<int> jail_dist(0, 14);
        for (int i = 0; i < 3; ++i) {
            int jail_index = jail_dist(rng_);
            registry.update_validator(
                validator_ids[jail_index],
                std::nullopt,
                std::nullopt,
                ValidatorStatus::Jailed
            );
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Verify no active validator is jailed
        std::vector<Validator> active_validators = new_set.get_active_validators();
        for (const auto& v : active_validators) {
            ASSERT_NE(v.status, ValidatorStatus::Jailed)
                << "Active validator is jailed on trial " << trial;
        }
    }
}

/**
 * Property: Tombstoned validators are excluded from active set
 * 
 * For any validator with status Tombstoned, the validator is never selected
 * as active.
 */
TEST_F(TopNSelectionPropertyTest, TombstonedValidatorsExcluded) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add 15 validators
        std::vector<ValidatorID> validator_ids;
        for (int i = 0; i < 15; ++i) {
            ValidatorID id = generate_random_validator_id();
            validator_ids.push_back(id);
            
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Tombstone 2 random validators
        std::uniform_int_distribution<int> tombstone_dist(0, 14);
        for (int i = 0; i < 2; ++i) {
            int tombstone_index = tombstone_dist(rng_);
            registry.update_validator(
                validator_ids[tombstone_index],
                std::nullopt,
                std::nullopt,
                ValidatorStatus::Tombstoned
            );
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Verify no active validator is tombstoned
        std::vector<Validator> active_validators = new_set.get_active_validators();
        for (const auto& v : active_validators) {
            ASSERT_NE(v.status, ValidatorStatus::Tombstoned)
                << "Active validator is tombstoned on trial " << trial;
        }
    }
}

/**
 * Property: Selection is deterministic for the same input
 * 
 * For any validator set, running epoch transition twice with the same
 * parameters produces identical active validator sets.
 */
TEST_F(TopNSelectionPropertyTest, SelectionIsDeterministic) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;

        // Create two identical registries
        ValidatorRegistry registry1(config);
        ValidatorRegistry registry2(config);

        // Add same validators to both
        std::uniform_int_distribution<int> validator_count_dist(15, 30);
        int num_validators = validator_count_dist(rng_);

        std::vector<ValidatorID> validator_ids;
        std::vector<BLS12_381_PublicKey> consensus_keys;
        std::vector<Ed25519_PublicKey> withdrawal_keys;
        std::vector<uint64_t> stakes;

        for (int i = 0; i < num_validators; ++i) {
            validator_ids.push_back(generate_random_validator_id());
            consensus_keys.push_back(generate_random_bls_key());
            withdrawal_keys.push_back(generate_random_ed25519_key());
            stakes.push_back(generate_random_stake(1000, 1000000));
        }

        // Add to both registries
        for (int i = 0; i < num_validators; ++i) {
            registry1.add_validator(validator_ids[i], consensus_keys[i], 
                                   withdrawal_keys[i], stakes[i]);
            registry2.add_validator(validator_ids[i], consensus_keys[i], 
                                   withdrawal_keys[i], stakes[i]);
        }

        // Transition both to epoch 1
        ValidatorSet set1 = registry1.transition_epoch(1, 10000);
        ValidatorSet set2 = registry2.transition_epoch(1, 10000);

        // Get active validators from both
        std::vector<Validator> active1 = set1.get_active_validators();
        std::vector<Validator> active2 = set2.get_active_validators();

        ASSERT_EQ(active1.size(), active2.size())
            << "Different number of active validators on trial " << trial;

        // Sort both by ID for comparison
        std::sort(active1.begin(), active1.end(), 
                 [](const Validator& a, const Validator& b) { return a.id < b.id; });
        std::sort(active2.begin(), active2.end(), 
                 [](const Validator& a, const Validator& b) { return a.id < b.id; });

        // Verify same validators are active
        for (size_t i = 0; i < active1.size(); ++i) {
            ASSERT_EQ(active1[i].id, active2[i].id)
                << "Different active validator at position " << i
                << " on trial " << trial;
            ASSERT_EQ(active1[i].bonded_stake, active2[i].bonded_stake)
                << "Different stake for validator at position " << i
                << " on trial " << trial;
        }
    }
}

/**
 * Property: Active set size is min(N, qualified_validators)
 * 
 * If fewer than N validators meet the minimum stake requirement,
 * all qualified validators are selected as active.
 */
TEST_F(TopNSelectionPropertyTest, ActiveSetSizeIsMinimum) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 10000;
        ValidatorRegistry registry(config);

        // Add fewer than N validators meeting minimum stake
        std::uniform_int_distribution<int> validator_count_dist(3, 8);
        int num_qualified = validator_count_dist(rng_);

        for (int i = 0; i < num_qualified; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Add some validators below minimum stake
        for (int i = 0; i < 5; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(100, 9999);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Count active validators
        int active_count = 0;
        for (const auto& validator : new_set.validators) {
            if (validator.status == ValidatorStatus::Active) {
                active_count++;
            }
        }

        ASSERT_EQ(active_count, num_qualified)
            << "Expected " << num_qualified << " active validators, got " << active_count
            << " on trial " << trial;
    }
}

/**
 * Property: Ranking is stable across multiple transitions
 * 
 * If validator stakes don't change, the same validators remain active
 * across multiple epoch transitions.
 */
TEST_F(TopNSelectionPropertyTest, RankingIsStableAcrossTransitions) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        std::uniform_int_distribution<int> validator_count_dist(15, 25);
        int num_validators = validator_count_dist(rng_);

        for (int i = 0; i < num_validators; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(1000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet set1 = registry.transition_epoch(1, 10000);
        std::vector<Validator> active1 = set1.get_active_validators();

        // Transition to epoch 2 (no stake changes)
        ValidatorSet set2 = registry.transition_epoch(2, 10000);
        std::vector<Validator> active2 = set2.get_active_validators();

        // Sort both by ID
        std::sort(active1.begin(), active1.end(), 
                 [](const Validator& a, const Validator& b) { return a.id < b.id; });
        std::sort(active2.begin(), active2.end(), 
                 [](const Validator& a, const Validator& b) { return a.id < b.id; });

        ASSERT_EQ(active1.size(), active2.size())
            << "Active set size changed without stake changes on trial " << trial;

        // Verify same validators are active
        for (size_t i = 0; i < active1.size(); ++i) {
            ASSERT_EQ(active1[i].id, active2[i].id)
                << "Active validator changed at position " << i
                << " without stake changes on trial " << trial;
        }
    }
}
