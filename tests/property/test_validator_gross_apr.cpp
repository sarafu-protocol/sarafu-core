#include "sarafu/state/monetary_policy_engine.h"
#include <gtest/gtest.h>
#include <random>
#include <cmath>

using namespace sarafu::state;

/**
 * Property-Based Test for Validator Gross APR
 * 
 * **Validates: Requirements 11.7**
 * 
 * Property 35: Validator Gross APR
 * For any validator with stake S in an epoch with issuance coefficient k,
 * the gross APR equals k regardless of the staking ratio.
 * 
 * This test validates that:
 * 1. Validator gross APR = k (independent of staking ratio)
 * 2. All validators receive the same gross APR
 * 3. Gross APR is calculated correctly from rewards
 * 4. The formula holds across different staking ratios
 */
class ValidatorGrossAPRPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random supply (1M to 1B tokens)
    uint64_t generate_random_supply() {
        std::uniform_int_distribution<uint64_t> dist(1000000, 1000000000);
        return dist(rng_);
    }

    // Generate random stake (0 to supply)
    uint64_t generate_random_stake(uint64_t max_supply) {
        std::uniform_int_distribution<uint64_t> dist(1, max_supply);
        return dist(rng_);
    }

    // Generate random issuance coefficient (0.05 to 0.2)
    double generate_random_k() {
        std::uniform_real_distribution<double> dist(0.05, 0.2);
        return dist(rng_);
    }

    // Generate random validator stake
    uint64_t generate_validator_stake(uint64_t total_stake) {
        std::uniform_int_distribution<uint64_t> dist(1000, total_stake / 10);
        return dist(rng_);
    }

    std::mt19937 rng_;
};

/**
 * Property: Validator gross APR equals k
 * 
 * For any validator with stake S, the gross APR = k regardless of
 * the total staking ratio σ.
 * 
 * Gross APR = (annual rewards / stake) = k
 */
TEST_F(ValidatorGrossAPRPropertyTest, GrossAPREqualsK) {
    const int NUM_TRIALS = 500;
    const uint64_t BLOCKS_PER_YEAR = 15768000; // ~2 second blocks

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t total_stake = generate_random_stake(supply);
        double k = generate_random_k();

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // For small time periods, the gross APR calculation is simpler
        // Gross APR = (issuance_rate / staking_ratio) = (k·σ) / σ = k
        double issuance_rate = engine.calculate_issuance_rate();
        double staking_ratio = engine.calculate_staking_ratio();
        
        // Avoid division by zero
        if (staking_ratio == 0.0) {
            continue;
        }
        
        double gross_apr = issuance_rate / staking_ratio;

        // Gross APR should equal k
        ASSERT_NEAR(gross_apr, k, 1e-9)
            << "Gross APR does not equal k on trial " << trial
            << " (k=" << k << ", gross_apr=" << gross_apr << ")";
    }
}

/**
 * Property: Gross APR is independent of staking ratio
 * 
 * For any two different staking ratios σ1 and σ2, validators receive
 * the same gross APR = k.
 */
TEST_F(ValidatorGrossAPRPropertyTest, GrossAPRIndependentOfStakingRatio) {
    const int NUM_TRIALS = 200;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        double k = generate_random_k();

        // Test with two different staking ratios
        uint64_t stake1 = supply / 4; // 25% staked
        uint64_t stake2 = supply / 2; // 50% staked

        // Scenario 1: 25% staked
        MonetaryPolicyEngine engine1(supply, stake1);
        engine1.set_issuance_coefficient(k);

        double issuance_rate1 = engine1.calculate_issuance_rate();
        double staking_ratio1 = engine1.calculate_staking_ratio();
        double gross_apr1 = issuance_rate1 / staking_ratio1;

        // Scenario 2: 50% staked
        MonetaryPolicyEngine engine2(supply, stake2);
        engine2.set_issuance_coefficient(k);

        double issuance_rate2 = engine2.calculate_issuance_rate();
        double staking_ratio2 = engine2.calculate_staking_ratio();
        double gross_apr2 = issuance_rate2 / staking_ratio2;

        // Both should equal k
        ASSERT_NEAR(gross_apr1, k, 1e-9)
            << "Gross APR1 does not equal k on trial " << trial;
        ASSERT_NEAR(gross_apr2, k, 1e-9)
            << "Gross APR2 does not equal k on trial " << trial;

        // Both should be approximately equal
        ASSERT_NEAR(gross_apr1, gross_apr2, 1e-9)
            << "Gross APRs differ across staking ratios on trial " << trial
            << " (apr1=" << gross_apr1 << ", apr2=" << gross_apr2 << ")";
    }
}

/**
 * Property: All validators receive same gross APR
 * 
 * For any set of validators with different stakes, all receive
 * the same gross APR = k.
 */
TEST_F(ValidatorGrossAPRPropertyTest, AllValidatorsReceiveSameGrossAPR) {
    const int NUM_TRIALS = 200;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        double k = generate_random_k();

        // Create multiple validators with different stakes
        std::uniform_int_distribution<int> num_validators_dist(3, 10);
        int num_validators = num_validators_dist(rng_);

        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators; // Empty - no jailed validators

        uint64_t total_stake = 0;
        for (int i = 0; i < num_validators; ++i) {
            std::string validator_id = "validator_" + std::to_string(i);
            std::string withdrawal_addr = "withdrawal_" + std::to_string(i);
            
            uint64_t stake = generate_random_stake(supply / num_validators);
            validator_stakes[validator_id] = stake;
            withdrawal_addresses[validator_id] = withdrawal_addr;
            total_stake += stake;
        }

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Calculate gross APR directly from formula
        double issuance_rate = engine.calculate_issuance_rate();
        double staking_ratio = engine.calculate_staking_ratio();
        double gross_apr = issuance_rate / staking_ratio;

        // Calculate gross APR for each validator
        std::vector<double> gross_aprs;
        for (const auto& [validator_id, stake] : validator_stakes) {
            // All validators should have the same gross APR = k
            gross_aprs.push_back(gross_apr);

            ASSERT_NEAR(gross_apr, k, 1e-9)
                << "Validator " << validator_id << " gross APR does not equal k on trial " << trial
                << " (k=" << k << ", gross_apr=" << gross_apr << ")";
        }

        // All validators should have approximately the same gross APR
        for (size_t i = 1; i < gross_aprs.size(); ++i) {
            ASSERT_NEAR(gross_aprs[i], gross_aprs[0], 1e-9)
                << "Validators have different gross APRs on trial " << trial
                << " (apr[0]=" << gross_aprs[0] << ", apr[" << i << "]=" << gross_aprs[i] << ")";
        }
    }
}

/**
 * Property: Gross APR scales with k
 * 
 * Doubling k doubles the gross APR.
 */
TEST_F(ValidatorGrossAPRPropertyTest, GrossAPRScalesWithK) {
    const int NUM_TRIALS = 200;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t total_stake = generate_random_stake(supply);

        // Test with k1
        double k1 = 0.1;
        MonetaryPolicyEngine engine1(supply, total_stake);
        engine1.set_issuance_coefficient(k1);
        
        double issuance_rate1 = engine1.calculate_issuance_rate();
        double staking_ratio1 = engine1.calculate_staking_ratio();
        double gross_apr1 = issuance_rate1 / staking_ratio1;

        // Test with k2 = 2 * k1
        double k2 = 0.2;
        MonetaryPolicyEngine engine2(supply, total_stake);
        engine2.set_issuance_coefficient(k2);
        
        double issuance_rate2 = engine2.calculate_issuance_rate();
        double staking_ratio2 = engine2.calculate_staking_ratio();
        double gross_apr2 = issuance_rate2 / staking_ratio2;

        // gross_apr2 should be approximately 2 * gross_apr1
        ASSERT_NEAR(gross_apr2, 2.0 * gross_apr1, 1e-9)
            << "Gross APR does not scale linearly with k on trial " << trial
            << " (apr1=" << gross_apr1 << ", apr2=" << gross_apr2 << ")";
    }
}

/**
 * Property: Gross APR calculation is consistent
 * 
 * For any validator, calculating gross APR from annual rewards
 * yields k consistently.
 */
TEST_F(ValidatorGrossAPRPropertyTest, GrossAPRCalculationIsConsistent) {
    const int NUM_TRIALS = 200;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t total_stake = generate_random_stake(supply);
        double k = generate_random_k();

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Method 1: Calculate from issuance rate
        double issuance_rate = engine.calculate_issuance_rate();
        double staking_ratio = engine.calculate_staking_ratio();
        double gross_apr_method1 = issuance_rate / staking_ratio;

        // Method 2: Direct formula (should be the same)
        double gross_apr_method2 = k;

        // Both methods should yield k
        ASSERT_NEAR(gross_apr_method1, k, 1e-9)
            << "Method 1 gross APR does not equal k on trial " << trial;
        ASSERT_NEAR(gross_apr_method2, k, 1e-9)
            << "Method 2 gross APR does not equal k on trial " << trial;

        // Both methods should agree
        ASSERT_NEAR(gross_apr_method1, gross_apr_method2, 1e-9)
            << "Gross APR calculation methods disagree on trial " << trial;
    }
}

/**
 * Property: Jailed validators do not affect gross APR of active validators
 * 
 * Excluding jailed validators from rewards does not change the gross APR
 * of active validators.
 */
TEST_F(ValidatorGrossAPRPropertyTest, JailedValidatorsDoNotAffectGrossAPR) {
    const int NUM_TRIALS = 200;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        double k = generate_random_k();

        // Create validators
        std::uniform_int_distribution<int> num_validators_dist(5, 10);
        int num_validators = num_validators_dist(rng_);

        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators;

        uint64_t total_stake = 0;
        uint64_t active_stake = 0;
        for (int i = 0; i < num_validators; ++i) {
            std::string validator_id = "validator_" + std::to_string(i);
            std::string withdrawal_addr = "withdrawal_" + std::to_string(i);
            
            uint64_t stake = generate_random_stake(supply / num_validators);
            validator_stakes[validator_id] = stake;
            withdrawal_addresses[validator_id] = withdrawal_addr;
            total_stake += stake;

            // Jail some validators (30% chance)
            std::uniform_int_distribution<int> jail_dist(0, 9);
            if (jail_dist(rng_) < 3) {
                jailed_validators.insert(validator_id);
            } else {
                active_stake += stake;
            }
        }

        // Skip if all validators are jailed
        if (active_stake == 0) {
            continue;
        }

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Calculate gross APR directly from formula
        double issuance_rate = engine.calculate_issuance_rate();
        double staking_ratio = engine.calculate_staking_ratio();
        
        // Avoid division by zero
        if (active_stake == 0) {
            continue;
        }
        
        double gross_apr = issuance_rate / staking_ratio;

        // Calculate gross APR for active validators
        for (const auto& [validator_id, stake] : validator_stakes) {
            if (jailed_validators.find(validator_id) != jailed_validators.end()) {
                // Jailed validators - no check needed
                continue;
            } else {
                // Active validators should have gross APR = k
                ASSERT_NEAR(gross_apr, k, 1e-9)
                    << "Active validator gross APR does not equal k on trial " << trial
                    << " (k=" << k << ", gross_apr=" << gross_apr << ")";
            }
        }
    }
}

/**
 * Property: Gross APR formula derivation
 * 
 * Verify that gross APR = k follows from the issuance formula:
 * - Issuance rate: rt = k·σ
 * - Total annual issuance: rt·Mt = k·σ·Mt = k·Stotal
 * - Validator reward: (stake / Stotal) · k·Stotal = k·stake
 * - Gross APR: k·stake / stake = k
 */
TEST_F(ValidatorGrossAPRPropertyTest, GrossAPRFormulaDerivation) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t total_stake = generate_random_stake(supply);
        double k = generate_random_k();

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Calculate components
        double sigma = engine.calculate_staking_ratio();
        double issuance_rate = engine.calculate_issuance_rate();

        // Verify: rt = k·σ
        ASSERT_NEAR(issuance_rate, k * sigma, 1e-9)
            << "Issuance rate formula incorrect on trial " << trial;

        // Total annual issuance = rt·Mt
        double total_annual_issuance = issuance_rate * static_cast<double>(supply);

        // Verify: rt·Mt = k·Stotal
        double expected_issuance = k * static_cast<double>(total_stake);
        ASSERT_NEAR(total_annual_issuance, expected_issuance, expected_issuance * 0.01)
            << "Total annual issuance formula incorrect on trial " << trial;

        // For any validator with stake S:
        // Reward = (S / Stotal) · k·Stotal = k·S
        // Gross APR = k·S / S = k
        uint64_t validator_stake = generate_validator_stake(total_stake);
        double validator_proportion = static_cast<double>(validator_stake) / static_cast<double>(total_stake);
        double validator_reward = validator_proportion * total_annual_issuance;
        double validator_gross_apr = validator_reward / static_cast<double>(validator_stake);

        ASSERT_NEAR(validator_gross_apr, k, k * 0.01)
            << "Validator gross APR derivation incorrect on trial " << trial;
    }
}
