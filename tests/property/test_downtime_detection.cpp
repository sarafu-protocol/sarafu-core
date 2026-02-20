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
 * Property-Based Test for Downtime Detection
 * 
 * **Validates: Requirements 4.1, 4.6**
 * 
 * Property 14: Downtime Detection
 * For any validator V in epoch E, if V's signing rate < 95%, the system detects
 * a downtime violation.
 * 
 * Property 15: Downtime Counter Reset
 * For any validator V, if V successfully signs ≥95% of blocks in an epoch after
 * being jailed, their consecutive downtime counter resets to 0.
 * 
 * This test validates that:
 * 1. Validators with <95% signing rate are detected
 * 2. First violation results in jailing for 1 epoch
 * 3. Second consecutive violation slashes 0.5%
 * 4. Third+ consecutive violation slashes 1%
 * 5. Good performance resets consecutive downtime counter
 */
class DowntimeDetectionPropertyTest : public ::testing::Test {
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
 * Property: Validators with <95% signing rate are detected
 * 
 * For any validator with signing_rate < 0.95, check_downtime() returns
 * that validator's ID.
 */
TEST_F(DowntimeDetectionPropertyTest, LowSigningRateDetected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        config.downtime_threshold = 0.95;
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
        registry.transition_epoch(1, 10000);

        // Simulate signing with <95% rate for one validator
        ValidatorID low_performer = validator_ids[0];
        uint64_t total_blocks = 10000;
        uint64_t blocks_to_sign = static_cast<uint64_t>(total_blocks * 0.90);  // 90% < 95%

        for (uint64_t i = 0; i < blocks_to_sign; ++i) {
            registry.record_signature(low_performer, i);
        }

        // Check downtime
        std::vector<ValidatorID> violators = registry.check_downtime(1, total_blocks);

        // Verify low performer is detected
        bool found = false;
        for (const auto& id : violators) {
            if (id == low_performer) {
                found = true;
                break;
            }
        }

        ASSERT_TRUE(found)
            << "Validator with 90% signing rate not detected on trial " << trial;
    }
}

/**
 * Property: First violation results in jailing for 1 epoch
 * 
 * For any validator's first downtime violation, they are jailed
 * until epoch E+1 without slashing.
 */
TEST_F(DowntimeDetectionPropertyTest, FirstViolationResultsInJailing) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validator
        ValidatorID id = generate_random_validator_id();
        BLS12_381_PublicKey consensus_key = generate_random_bls_key();
        Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
        uint64_t initial_stake = 100000;

        registry.add_validator(id, consensus_key, withdrawal_key, initial_stake);

        // Add more validators to fill active set
        for (int i = 0; i < 9; ++i) {
            ValidatorID other_id = generate_random_validator_id();
            BLS12_381_PublicKey other_key = generate_random_bls_key();
            Ed25519_PublicKey other_withdrawal = generate_random_ed25519_key();
            uint64_t other_stake = generate_random_stake(10000, 100000);

            registry.add_validator(other_id, other_key, other_withdrawal, other_stake);
        }

        // Transition to epoch 1
        registry.transition_epoch(1, 10000);

        // Simulate low signing rate (90%)
        uint64_t total_blocks = 10000;
        uint64_t blocks_to_sign = 9000;

        for (uint64_t i = 0; i < blocks_to_sign; ++i) {
            registry.record_signature(id, i);
        }

        // Check downtime
        registry.check_downtime(1, total_blocks);

        // Verify validator is jailed
        auto validator = registry.get_validator(id);
        ASSERT_TRUE(validator.has_value());
        ASSERT_EQ(validator->status, ValidatorStatus::Jailed)
            << "First violation did not result in jailing on trial " << trial;
        ASSERT_EQ(validator->jailed_until_epoch, 2)
            << "Jail period incorrect on trial " << trial;
        ASSERT_EQ(validator->consecutive_downtime_epochs, 1)
            << "Consecutive downtime counter incorrect on trial " << trial;
        ASSERT_EQ(validator->bonded_stake, initial_stake)
            << "Stake was slashed on first violation on trial " << trial;
    }
}

/**
 * Property: Second consecutive violation slashes 0.5%
 * 
 * For any validator's second consecutive downtime violation,
 * 0.5% of their stake is slashed.
 */
TEST_F(DowntimeDetectionPropertyTest, SecondViolationSlashesHalfPercent) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validator
        ValidatorID id = generate_random_validator_id();
        BLS12_381_PublicKey consensus_key = generate_random_bls_key();
        Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
        uint64_t initial_stake = 100000;

        registry.add_validator(id, consensus_key, withdrawal_key, initial_stake);

        // Add more validators
        for (int i = 0; i < 9; ++i) {
            ValidatorID other_id = generate_random_validator_id();
            BLS12_381_PublicKey other_key = generate_random_bls_key();
            Ed25519_PublicKey other_withdrawal = generate_random_ed25519_key();
            uint64_t other_stake = generate_random_stake(10000, 100000);

            registry.add_validator(other_id, other_key, other_withdrawal, other_stake);
        }

        // Transition to epoch 1
        registry.transition_epoch(1, 10000);

        // First violation (90% signing rate)
        for (uint64_t i = 0; i < 9000; ++i) {
            registry.record_signature(id, i);
        }
        registry.check_downtime(1, 10000);

        // Verify first violation
        auto validator1 = registry.get_validator(id);
        ASSERT_TRUE(validator1.has_value());
        ASSERT_EQ(validator1->consecutive_downtime_epochs, 1);
        uint64_t stake_after_first = validator1->bonded_stake;
        ASSERT_EQ(stake_after_first, initial_stake);  // No slashing on first

        // Transition to epoch 2 (validator released from jail)
        registry.transition_epoch(2, 10000);

        // Second violation (90% signing rate again)
        for (uint64_t i = 0; i < 9000; ++i) {
            registry.record_signature(id, i);
        }
        registry.check_downtime(2, 10000);

        // Verify second violation slashed 0.5%
        auto validator2 = registry.get_validator(id);
        ASSERT_TRUE(validator2.has_value());
        ASSERT_EQ(validator2->consecutive_downtime_epochs, 2);
        
        uint64_t expected_slash = (stake_after_first * 5) / 1000;  // 0.5%
        uint64_t expected_stake = stake_after_first - expected_slash;
        
        ASSERT_EQ(validator2->bonded_stake, expected_stake)
            << "Second violation did not slash 0.5% on trial " << trial;
        ASSERT_EQ(validator2->status, ValidatorStatus::Standby)
            << "Validator not moved to Standby on trial " << trial;
    }
}

/**
 * Property: Third+ consecutive violation slashes 1%
 * 
 * For any validator's third or subsequent consecutive downtime violation,
 * 1% of their stake is slashed.
 */
TEST_F(DowntimeDetectionPropertyTest, ThirdViolationSlashesOnePercent) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validator
        ValidatorID id = generate_random_validator_id();
        BLS12_381_PublicKey consensus_key = generate_random_bls_key();
        Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
        uint64_t initial_stake = 100000;

        registry.add_validator(id, consensus_key, withdrawal_key, initial_stake);

        // Add more validators
        for (int i = 0; i < 9; ++i) {
            ValidatorID other_id = generate_random_validator_id();
            BLS12_381_PublicKey other_key = generate_random_bls_key();
            Ed25519_PublicKey other_withdrawal = generate_random_ed25519_key();
            uint64_t other_stake = generate_random_stake(10000, 100000);

            registry.add_validator(other_id, other_key, other_withdrawal, other_stake);
        }

        // First violation
        registry.transition_epoch(1, 10000);
        for (uint64_t i = 0; i < 9000; ++i) {
            registry.record_signature(id, i);
        }
        registry.check_downtime(1, 10000);

        // Second violation
        registry.transition_epoch(2, 10000);
        for (uint64_t i = 0; i < 9000; ++i) {
            registry.record_signature(id, i);
        }
        registry.check_downtime(2, 10000);

        auto validator2 = registry.get_validator(id);
        ASSERT_TRUE(validator2.has_value());
        uint64_t stake_after_second = validator2->bonded_stake;

        // Third violation
        registry.transition_epoch(3, 10000);
        for (uint64_t i = 0; i < 9000; ++i) {
            registry.record_signature(id, i);
        }
        registry.check_downtime(3, 10000);

        // Verify third violation slashed 1%
        auto validator3 = registry.get_validator(id);
        ASSERT_TRUE(validator3.has_value());
        ASSERT_EQ(validator3->consecutive_downtime_epochs, 3);
        
        uint64_t expected_slash = stake_after_second / 100;  // 1%
        uint64_t expected_stake = stake_after_second - expected_slash;
        
        ASSERT_EQ(validator3->bonded_stake, expected_stake)
            << "Third violation did not slash 1% on trial " << trial;
    }
}

/**
 * Property: Good performance resets consecutive downtime counter
 * 
 * For any validator with consecutive_downtime_epochs > 0, if they achieve
 * ≥95% signing rate in an epoch, their counter resets to 0.
 */
TEST_F(DowntimeDetectionPropertyTest, GoodPerformanceResetsCounter) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        ValidatorRegistry registry(config);

        // Add validator
        ValidatorID id = generate_random_validator_id();
        BLS12_381_PublicKey consensus_key = generate_random_bls_key();
        Ed25519_PublicKey withdrawal_key = generate_random_ed25519_key();
        uint64_t initial_stake = 100000;

        registry.add_validator(id, consensus_key, withdrawal_key, initial_stake);

        // Add more validators
        for (int i = 0; i < 9; ++i) {
            ValidatorID other_id = generate_random_validator_id();
            BLS12_381_PublicKey other_key = generate_random_bls_key();
            Ed25519_PublicKey other_withdrawal = generate_random_ed25519_key();
            uint64_t other_stake = generate_random_stake(10000, 100000);

            registry.add_validator(other_id, other_key, other_withdrawal, other_stake);
        }

        // First violation
        registry.transition_epoch(1, 10000);
        for (uint64_t i = 0; i < 9000; ++i) {
            registry.record_signature(id, i);
        }
        registry.check_downtime(1, 10000);

        // Verify counter is 1
        auto validator1 = registry.get_validator(id);
        ASSERT_TRUE(validator1.has_value());
        ASSERT_EQ(validator1->consecutive_downtime_epochs, 1);

        // Good performance in epoch 2 (98% signing rate)
        registry.transition_epoch(2, 10000);
        for (uint64_t i = 0; i < 9800; ++i) {
            registry.record_signature(id, i);
        }
        registry.check_downtime(2, 10000);

        // Verify counter reset to 0
        auto validator2 = registry.get_validator(id);
        ASSERT_TRUE(validator2.has_value());
        ASSERT_EQ(validator2->consecutive_downtime_epochs, 0)
            << "Consecutive downtime counter not reset after good performance on trial " << trial;
    }
}

/**
 * Property: Validators with ≥95% signing rate are not penalized
 * 
 * For any validator with signing_rate >= 0.95, check_downtime() does not
 * return that validator's ID.
 */
TEST_F(DowntimeDetectionPropertyTest, GoodPerformanceNotPenalized) {
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
        registry.transition_epoch(1, 10000);

        // Simulate good signing (98% rate) for all validators
        uint64_t total_blocks = 10000;
        uint64_t blocks_to_sign = 9800;

        for (const auto& id : validator_ids) {
            for (uint64_t i = 0; i < blocks_to_sign; ++i) {
                registry.record_signature(id, i);
            }
        }

        // Check downtime
        std::vector<ValidatorID> violators = registry.check_downtime(1, total_blocks);

        // Verify no validators are penalized
        ASSERT_EQ(violators.size(), 0)
            << "Validators with 98% signing rate were penalized on trial " << trial;
    }
}

/**
 * Property: Exact 95% threshold is correctly applied
 * 
 * For any validator with exactly 95% signing rate, they are not penalized.
 * For any validator with 94.9% signing rate, they are penalized.
 */
TEST_F(DowntimeDetectionPropertyTest, ExactThresholdCorrectlyApplied) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        ValidatorRegistry::Config config;
        config.active_validator_count = 10;
        config.minimum_self_bond = 1000;
        config.downtime_threshold = 0.95;
        ValidatorRegistry registry(config);

        // Add two validators
        ValidatorID id_at_threshold = generate_random_validator_id();
        ValidatorID id_below_threshold = generate_random_validator_id();

        BLS12_381_PublicKey key1 = generate_random_bls_key();
        BLS12_381_PublicKey key2 = generate_random_bls_key();
        Ed25519_PublicKey withdrawal1 = generate_random_ed25519_key();
        Ed25519_PublicKey withdrawal2 = generate_random_ed25519_key();

        registry.add_validator(id_at_threshold, key1, withdrawal1, 100000);
        registry.add_validator(id_below_threshold, key2, withdrawal2, 100000);

        // Add more validators
        for (int i = 0; i < 8; ++i) {
            ValidatorID id = generate_random_validator_id();
            BLS12_381_PublicKey key = generate_random_bls_key();
            Ed25519_PublicKey withdrawal = generate_random_ed25519_key();
            registry.add_validator(id, key, withdrawal, generate_random_stake(10000, 100000));
        }

        // Transition to epoch 1
        registry.transition_epoch(1, 10000);

        // Validator 1: exactly 95% (9500 / 10000)
        for (uint64_t i = 0; i < 9500; ++i) {
            registry.record_signature(id_at_threshold, i);
        }

        // Validator 2: below 95% (9499 / 10000 = 94.99%)
        for (uint64_t i = 0; i < 9499; ++i) {
            registry.record_signature(id_below_threshold, i);
        }

        // Check downtime
        std::vector<ValidatorID> violators = registry.check_downtime(1, 10000);

        // Verify validator at threshold is not penalized
        bool at_threshold_penalized = false;
        for (const auto& id : violators) {
            if (id == id_at_threshold) {
                at_threshold_penalized = true;
                break;
            }
        }
        ASSERT_FALSE(at_threshold_penalized)
            << "Validator at 95% threshold was penalized on trial " << trial;

        // Verify validator below threshold is penalized
        bool below_threshold_penalized = false;
        for (const auto& id : violators) {
            if (id == id_below_threshold) {
                below_threshold_penalized = true;
                break;
            }
        }
        ASSERT_TRUE(below_threshold_penalized)
            << "Validator below 95% threshold was not penalized on trial " << trial;
    }
}
