#include "sarafu/state/monetary_policy_engine.h"
#include <gtest/gtest.h>
#include <random>
#include <cmath>

using namespace sarafu::state;

/**
 * Property-Based Test for Issuance Rate Calculation
 * 
 * **Validates: Requirements 11.1**
 * 
 * Property 31: Issuance Rate Calculation
 * For any epoch with staking ratio σ and issuance coefficient k,
 * the annual issuance rate equals k·σ.
 * 
 * This test validates that:
 * 1. Issuance rate is calculated as rt = k·σ
 * 2. Staking ratio is calculated as σ = Stotal / Mt
 * 3. The formula holds for various supply and stake values
 * 4. Edge cases (zero supply, zero stake) are handled correctly
 */
class IssuanceRateCalculationPropertyTest : public ::testing::Test {
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
        std::uniform_int_distribution<uint64_t> dist(0, max_supply);
        return dist(rng_);
    }

    // Generate random issuance coefficient (0.05 to 0.2)
    double generate_random_k() {
        std::uniform_real_distribution<double> dist(0.05, 0.2);
        return dist(rng_);
    }

    std::mt19937 rng_;
};

/**
 * Property: Issuance rate equals k·σ
 * 
 * For any supply Mt, bonded stake Stotal, and coefficient k,
 * the issuance rate rt = k·(Stotal / Mt).
 */
TEST_F(IssuanceRateCalculationPropertyTest, IssuanceRateEqualsKTimesSigma) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);
        double k = generate_random_k();

        MonetaryPolicyEngine engine(supply, stake);
        
        // Set custom k value
        auto error = engine.set_issuance_coefficient(k);
        ASSERT_FALSE(error.has_value()) << "Failed to set k: " << error.value();

        // Calculate expected values
        double expected_sigma = static_cast<double>(stake) / static_cast<double>(supply);
        double expected_rate = k * expected_sigma;

        // Get actual values
        double actual_sigma = engine.calculate_staking_ratio();
        double actual_rate = engine.calculate_issuance_rate();

        // Verify staking ratio
        ASSERT_NEAR(actual_sigma, expected_sigma, 1e-9)
            << "Staking ratio mismatch on trial " << trial
            << " (supply=" << supply << ", stake=" << stake << ")";

        // Verify issuance rate
        ASSERT_NEAR(actual_rate, expected_rate, 1e-9)
            << "Issuance rate mismatch on trial " << trial
            << " (k=" << k << ", sigma=" << expected_sigma << ")";
    }
}

/**
 * Property: Staking ratio is bounded [0, 1]
 * 
 * For any supply Mt and bonded stake Stotal where Stotal ≤ Mt,
 * the staking ratio σ ∈ [0, 1].
 */
TEST_F(IssuanceRateCalculationPropertyTest, StakingRatioIsBounded) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);

        double sigma = engine.calculate_staking_ratio();

        ASSERT_GE(sigma, 0.0)
            << "Staking ratio is negative on trial " << trial;
        ASSERT_LE(sigma, 1.0)
            << "Staking ratio exceeds 1.0 on trial " << trial
            << " (supply=" << supply << ", stake=" << stake << ")";
    }
}

/**
 * Property: Issuance rate scales linearly with k
 * 
 * For any fixed staking ratio σ, doubling k doubles the issuance rate.
 */
TEST_F(IssuanceRateCalculationPropertyTest, IssuanceRateScalesLinearlyWithK) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);

        // Test with k1
        double k1 = 0.1;
        engine.set_issuance_coefficient(k1);
        double rate1 = engine.calculate_issuance_rate();

        // Test with k2 = 2 * k1
        double k2 = 0.2;
        engine.set_issuance_coefficient(k2);
        double rate2 = engine.calculate_issuance_rate();

        // Verify rate2 ≈ 2 * rate1
        ASSERT_NEAR(rate2, 2.0 * rate1, 1e-9)
            << "Issuance rate does not scale linearly with k on trial " << trial
            << " (rate1=" << rate1 << ", rate2=" << rate2 << ")";
    }
}

/**
 * Property: Issuance rate scales linearly with staking ratio
 * 
 * For any fixed k, doubling the staking ratio doubles the issuance rate.
 */
TEST_F(IssuanceRateCalculationPropertyTest, IssuanceRateScalesLinearlyWithSigma) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        
        // Ensure stake1 is at most half of supply so we can double it
        uint64_t stake1 = generate_random_stake(supply / 2);
        uint64_t stake2 = stake1 * 2;

        double k = generate_random_k();

        // Test with stake1
        MonetaryPolicyEngine engine1(supply, stake1);
        engine1.set_issuance_coefficient(k);
        double rate1 = engine1.calculate_issuance_rate();

        // Test with stake2 = 2 * stake1
        MonetaryPolicyEngine engine2(supply, stake2);
        engine2.set_issuance_coefficient(k);
        double rate2 = engine2.calculate_issuance_rate();

        // Verify rate2 ≈ 2 * rate1
        ASSERT_NEAR(rate2, 2.0 * rate1, 1e-9)
            << "Issuance rate does not scale linearly with staking ratio on trial " << trial
            << " (rate1=" << rate1 << ", rate2=" << rate2 << ")";
    }
}

/**
 * Property: Zero stake yields zero issuance rate
 * 
 * For any supply Mt > 0 and stake Stotal = 0,
 * the issuance rate rt = 0.
 */
TEST_F(IssuanceRateCalculationPropertyTest, ZeroStakeYieldsZeroIssuance) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = 0;
        double k = generate_random_k();

        MonetaryPolicyEngine engine(supply, stake);
        engine.set_issuance_coefficient(k);

        double sigma = engine.calculate_staking_ratio();
        double rate = engine.calculate_issuance_rate();

        ASSERT_EQ(sigma, 0.0)
            << "Staking ratio should be 0 when stake is 0 on trial " << trial;
        ASSERT_EQ(rate, 0.0)
            << "Issuance rate should be 0 when stake is 0 on trial " << trial;
    }
}

/**
 * Property: Full stake yields issuance rate equal to k
 * 
 * For any supply Mt > 0 and stake Stotal = Mt (100% staked),
 * the issuance rate rt = k.
 */
TEST_F(IssuanceRateCalculationPropertyTest, FullStakeYieldsIssuanceRateEqualToK) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = supply; // 100% staked
        double k = generate_random_k();

        MonetaryPolicyEngine engine(supply, stake);
        engine.set_issuance_coefficient(k);

        double sigma = engine.calculate_staking_ratio();
        double rate = engine.calculate_issuance_rate();

        ASSERT_NEAR(sigma, 1.0, 1e-9)
            << "Staking ratio should be 1.0 when fully staked on trial " << trial;
        ASSERT_NEAR(rate, k, 1e-9)
            << "Issuance rate should equal k when fully staked on trial " << trial;
    }
}

/**
 * Property: Updating bonded stake updates staking ratio and issuance rate
 * 
 * For any engine state, updating the bonded stake recalculates
 * the staking ratio and issuance rate correctly.
 */
TEST_F(IssuanceRateCalculationPropertyTest, UpdatingStakeUpdatesRates) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t initial_stake = generate_random_stake(supply);
        uint64_t new_stake = generate_random_stake(supply);
        double k = generate_random_k();

        MonetaryPolicyEngine engine(supply, initial_stake);
        engine.set_issuance_coefficient(k);

        // Update stake
        engine.update_bonded_stake(new_stake);

        // Calculate expected values
        double expected_sigma = static_cast<double>(new_stake) / static_cast<double>(supply);
        double expected_rate = k * expected_sigma;

        // Get actual values
        double actual_sigma = engine.calculate_staking_ratio();
        double actual_rate = engine.calculate_issuance_rate();

        ASSERT_NEAR(actual_sigma, expected_sigma, 1e-9)
            << "Staking ratio not updated correctly on trial " << trial;
        ASSERT_NEAR(actual_rate, expected_rate, 1e-9)
            << "Issuance rate not updated correctly on trial " << trial;
    }
}

/**
 * Property: Issuance rate is deterministic
 * 
 * For any supply Mt, stake Stotal, and coefficient k,
 * calculating the issuance rate multiple times yields the same result.
 */
TEST_F(IssuanceRateCalculationPropertyTest, IssuanceRateIsDeterministic) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);
        double k = generate_random_k();

        MonetaryPolicyEngine engine(supply, stake);
        engine.set_issuance_coefficient(k);

        // Calculate rate multiple times
        double rate1 = engine.calculate_issuance_rate();
        double rate2 = engine.calculate_issuance_rate();
        double rate3 = engine.calculate_issuance_rate();

        ASSERT_EQ(rate1, rate2)
            << "Issuance rate not deterministic (rate1 != rate2) on trial " << trial;
        ASSERT_EQ(rate2, rate3)
            << "Issuance rate not deterministic (rate2 != rate3) on trial " << trial;
    }
}

/**
 * Property: State reflects calculated values
 * 
 * For any engine state, the state.staking_ratio and state.issuance_rate
 * match the values returned by calculate_staking_ratio() and calculate_issuance_rate().
 */
TEST_F(IssuanceRateCalculationPropertyTest, StateReflectsCalculatedValues) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);
        double k = generate_random_k();

        MonetaryPolicyEngine engine(supply, stake);
        engine.set_issuance_coefficient(k);

        const MonetaryState& state = engine.state();
        double calculated_sigma = engine.calculate_staking_ratio();
        double calculated_rate = engine.calculate_issuance_rate();

        ASSERT_EQ(state.staking_ratio, calculated_sigma)
            << "State staking ratio does not match calculated value on trial " << trial;
        ASSERT_EQ(state.issuance_rate, calculated_rate)
            << "State issuance rate does not match calculated value on trial " << trial;
    }
}

/**
 * Property: Issuance rate is non-negative
 * 
 * For any valid supply, stake, and k values,
 * the issuance rate is always ≥ 0.
 */
TEST_F(IssuanceRateCalculationPropertyTest, IssuanceRateIsNonNegative) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);
        double k = generate_random_k();

        MonetaryPolicyEngine engine(supply, stake);
        engine.set_issuance_coefficient(k);

        double rate = engine.calculate_issuance_rate();

        ASSERT_GE(rate, 0.0)
            << "Issuance rate is negative on trial " << trial;
    }
}
