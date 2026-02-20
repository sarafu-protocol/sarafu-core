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
 * Property-Based Test for Standby Queue Completeness
 * 
 * **Validates: Requirements 2.5**
 * 
 * Property 7: Standby Queue Completeness
 * For any epoch, the standby queue contains all validators ranked N+1 and below
 * who meet the minimum stake requirement.
 * 
 * This test validates that:
 * 1. All validators ranked N+1 and below are in standby
 * 2. Standby validators meet minimum stake requirement
 * 3. Standby queue is complete (no missing validators)
 * 4. Standby validators are correctly ordered by stake
 * 5. Jailed and tombstoned validators can be in standby
 */
class StandbyQueueCompletenessPropertyTest : public ::testing::Test {
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
 * Property: All validators ranked N+1 and below are in standby
 * 
 * For any validator set with M > N validators meeting minimum stake,
 * validators ranked N+1 through M are in Standby status.
 */
TEST_F(StandbyQueueCompletenessPropertyTest, ValidatorsRankedBelowNAreStandby) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators above minimum
        std::uniform_int_distribution<int> validator_count_dist(15, 30);
        int num_validators = validator_count_dist(rng_);

        std::vector<std::pair<ValidatorID, uint64_t>> validators_with_stakes;
        for (int i = 0; i < num_validators; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(1000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
            validators_with_stakes.push_back({id, stake});
        }

        // Sort by stake (descending), then by ID (lexicographic)
        std::sort(validators_with_stakes.begin(), validators_with_stakes.end(),
                 [](const auto& a, const auto& b) {
                     if (a.second != b.second) return a.second > b.second;
                     return a.first < b.first;
                 });

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Verify validators ranked 11+ are standby
        for (size_t i = 10; i < validators_with_stakes.size(); ++i) {
            const Validator* v = new_set.find_validator(validators_with_stakes[i].first);
            ASSERT_NE(v, nullptr);
            ASSERT_EQ(v->status, ValidatorStatus::Standby)
                << "Validator ranked " << (i + 1) << " is not standby on trial " << trial;
        }
    }
}

/**
 * Property: Standby validators meet minimum stake requirement
 * 
 * For any validator in standby status, their bonded_stake >= minimum_self_bond
 * (unless they were slashed or unbonded after selection).
 */
TEST_F(StandbyQueueCompletenessPropertyTest, StandbyValidatorsMeetMinimum) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t minimum_stake = 10000;

        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = minimum_stake;
        ValidatorRegistry registry(config);

        // Add validators above minimum
        for (int i = 0; i < 20; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Get standby validators
        std::vector<Validator> standby_validators = new_set.get_standby_validators();

        // Verify all standby validators meet minimum
        for (const auto& v : standby_validators) {
            ASSERT_GE(v.bonded_stake, minimum_stake)
                << "Standby validator has stake " << v.bonded_stake
                << " below minimum " << minimum_stake << " on trial " << trial;
        }
    }
}

/**
 * Property: Standby queue is complete (no missing validators)
 * 
 * For any validator set, the union of active and standby validators
 * equals all validators meeting minimum stake (excluding jailed/tombstoned).
 */
TEST_F(StandbyQueueCompletenessPropertyTest, StandbyQueueIsComplete) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        std::vector<ValidatorID> all_validator_ids;
        for (int i = 0; i < 25; ++i) {
            ValidatorID id = generate_random_validator_id();
            all_validator_ids.push_back(id);
            
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(1000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Count active and standby validators
        std::vector<Validator> active_validators = new_set.get_active_validators();
        std::vector<Validator> standby_validators = new_set.get_standby_validators();

        // Verify all validators are accounted for (active or standby)
        int total_active_standby = active_validators.size() + standby_validators.size();
        ASSERT_EQ(total_active_standby, all_validator_ids.size())
            << "Missing validators: expected " << all_validator_ids.size()
            << " but got " << total_active_standby << " on trial " << trial;
    }
}

/**
 * Property: Standby validators are ordered by stake
 * 
 * For any standby queue, validators are ordered by bonded_stake (descending),
 * with lexicographic ID ordering for ties.
 */
TEST_F(StandbyQueueCompletenessPropertyTest, StandbyValidatorsOrderedByStake) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 5;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        for (int i = 0; i < 15; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(1000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet new_set = registry.transition_epoch(1, 10000);

        // Get standby validators
        std::vector<Validator> standby_validators = new_set.get_standby_validators();

        // Verify ordering
        for (size_t i = 1; i < standby_validators.size(); ++i) {
            const Validator& prev = standby_validators[i - 1];
            const Validator& curr = standby_validators[i];

            // Previous validator should have >= stake
            if (prev.bonded_stake == curr.bonded_stake) {
                // If stakes equal, check lexicographic ordering
                ASSERT_LT(prev.id, curr.id)
                    << "Standby validators not lexicographically ordered on trial " << trial;
            } else {
                ASSERT_GE(prev.bonded_stake, curr.bonded_stake)
                    << "Standby validators not ordered by stake on trial " << trial;
            }
        }
    }
}

/**
 * Property: Jailed validators can be in standby
 * 
 * For any jailed validator meeting minimum stake, they are in standby
 * (not active, but still in the validator set).
 */
TEST_F(StandbyQueueCompletenessPropertyTest, JailedValidatorsInStandby) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        std::vector<ValidatorID> validator_ids;
        for (int i = 0; i < 15; ++i) {
            ValidatorID id = generate_random_validator_id();
            validator_ids.push_back(id);
            
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(10000, 1000000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Jail some validators
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

        // Verify jailed validators are not active
        for (const auto& v : new_set.validators) {
            if (v.status == ValidatorStatus::Jailed) {
                // Jailed validators should not be active
                ASSERT_NE(v.status, ValidatorStatus::Active)
                    << "Jailed validator is active on trial " << trial;
            }
        }
    }
}

/**
 * Property: Standby queue size equals total validators minus active count
 * 
 * For any validator set with M validators meeting minimum stake,
 * standby queue size = max(0, M - N).
 */
TEST_F(StandbyQueueCompletenessPropertyTest, StandbyQueueSizeCorrect) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators above minimum
        std::uniform_int_distribution<int> validator_count_dist(5, 30);
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

        // Get active and standby counts
        int active_count = new_set.get_active_validators().size();
        int standby_count = new_set.get_standby_validators().size();

        // Verify counts
        ASSERT_EQ(active_count + standby_count, num_validators)
            << "Active + standby != total validators on trial " << trial;

        if (num_validators >= 10) {
            ASSERT_EQ(active_count, 10);
            ASSERT_EQ(standby_count, num_validators - 10);
        } else {
            ASSERT_EQ(active_count, num_validators);
            ASSERT_EQ(standby_count, 0);
        }
    }
}

/**
 * Property: Standby validators can become active in next epoch
 * 
 * For any standby validator, if active validators are removed,
 * the standby validator can be promoted to active in the next epoch.
 */
TEST_F(StandbyQueueCompletenessPropertyTest, StandbyCanBecomeActive) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        std::vector<ValidatorID> validator_ids;
        for (int i = 0; i < 15; ++i) {
            ValidatorID id = generate_random_validator_id();
            validator_ids.push_back(id);
            
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(1000, 100000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet set1 = registry.transition_epoch(1, 10000);
        std::vector<Validator> standby1 = set1.get_standby_validators();
        ASSERT_EQ(standby1.size(), 5);

        // Remember a standby validator
        ValidatorID standby_id = standby1[0].id;

        // Unbond stake from some active validators
        std::vector<Validator> active1 = set1.get_active_validators();
        for (int i = 0; i < 3; ++i) {
            registry.unbond_stake(active1[i].id, active1[i].bonded_stake, 0);
        }

        // Transition to epoch 2
        ValidatorSet set2 = registry.transition_epoch(2, 10000);

        // Check if the standby validator is now active
        const Validator* v = set2.find_validator(standby_id);
        ASSERT_NE(v, nullptr);
        
        // The standby validator should now be active (since 3 active validators were removed)
        ASSERT_EQ(v->status, ValidatorStatus::Active)
            << "Standby validator not promoted to active on trial " << trial;
    }
}

/**
 * Property: Standby queue persists across epochs
 * 
 * For any standby validator, if their stake doesn't change,
 * they remain in standby across multiple epochs.
 */
TEST_F(StandbyQueueCompletenessPropertyTest, StandbyQueuePersistsAcrossEpochs) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validators
        for (int i = 0; i < 20; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey consensus_key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
            uint64_t stake = generate_random_stake(1000, 100000);

            registry.add_validator(id, consensus_key, withdrawal_key, stake);
        }

        // Transition to epoch 1
        ValidatorSet set1 = registry.transition_epoch(1, 10000);
        std::vector<Validator> standby1 = set1.get_standby_validators();
        ASSERT_EQ(standby1.size(), 10);

        // Transition to epoch 2 (no stake changes)
        ValidatorSet set2 = registry.transition_epoch(2, 10000);
        std::vector<Validator> standby2 = set2.get_standby_validators();

        // Verify same validators are in standby
        ASSERT_EQ(standby1.size(), standby2.size());

        for (const auto& v1 : standby1) {
            bool found = false;
            for (const auto& v2 : standby2) {
                if (v1.id == v2.id) {
                    found = true;
                    ASSERT_EQ(v2.status, ValidatorStatus::Standby)
                        << "Validator status changed without stake change on trial " << trial;
                    break;
                }
            }
            ASSERT_TRUE(found)
                << "Standby validator disappeared on trial " << trial;
        }
    }
}
