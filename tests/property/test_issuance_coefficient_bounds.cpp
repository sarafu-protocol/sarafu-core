#include "sarafu/state/monetary_policy_engine.h"
#include <gtest/gtest.h>
#include <random>

using namespace sarafu::state;

/**
 * Property-Based Test for Issuance Coefficient Bounds
 * 
 * **Validates: Requirements 11.5**
 * 
 * Property 34: Issuance Coefficient Bounds
 * For any governance proposal to change k, the new value must satisfy 0.05 ≤ k ≤ 0.2.
 * 
 * This test validates that:
 * 1. Valid k values within bounds are accepted
 * 2. k values below 0.05 are rejected
 * 3. k values above 0.2 are rejected
 * 4. Boundary values (0.05 and 0.2) are accepted
 * 5. The engine enforces bounds consistently
 */
class IssuanceCoefficientBoundsPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random supply
    uint64_t generate_random_supply() {
        std::uniform_int_distribution<uint64_t> dist(1000000, 1000000000);
        return dist(rng_);
    }

    // Generate random stake
    uint64_t generate_random_stake(uint64_t max_supply) {
        std::uniform_int_distribution<uint64_t> dist(0, max_supply);
        return dist(rng_);
    }

    // Generate valid k (within bounds)
    double generate_valid_k() {
        std::uniform_real_distribution<double> dist(0.05, 0.2);
        return dist(rng_);
    }

    // Generate invalid k below minimum
    double generate_k_below_min() {
        std::uniform_real_distribution<double> dist(0.0, 0.049999);
        return dist(rng_);
    }

    // Generate invalid k above maximum
    double generate_k_above_max() {
        std::uniform_real_distribution<double> dist(0.200001, 1.0);
        return dist(rng_);
    }

    std::mt19937 rng_;
};

/**
 * Property: Valid k values are accepted
 * 
 * For any k where 0.05 ≤ k ≤ 0.2, set_issuance_coefficient(k) succeeds.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, ValidKValuesAreAccepted) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);
        double k = generate_valid_k();

        MonetaryPolicyEngine engine(supply, stake);

        auto error = engine.set_issuance_coefficient(k);

        ASSERT_FALSE(error.has_value())
            << "Valid k value " << k << " was rejected on trial " << trial
            << " with error: " << error.value();

        // Verify k was actually set
        ASSERT_EQ(engine.parameters().k, k)
            << "k value not set correctly on trial " << trial;
    }
}

/**
 * Property: k values below minimum are rejected
 * 
 * For any k < 0.05, set_issuance_coefficient(k) fails with an error.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, KBelowMinimumIsRejected) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);
        double k = generate_k_below_min();

        MonetaryPolicyEngine engine(supply, stake);
        double original_k = engine.parameters().k;

        auto error = engine.set_issuance_coefficient(k);

        ASSERT_TRUE(error.has_value())
            << "k value " << k << " below minimum was accepted on trial " << trial;

        // Verify k was not changed
        ASSERT_EQ(engine.parameters().k, original_k)
            << "k value changed despite rejection on trial " << trial;
    }
}

/**
 * Property: k values above maximum are rejected
 * 
 * For any k > 0.2, set_issuance_coefficient(k) fails with an error.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, KAboveMaximumIsRejected) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);
        double k = generate_k_above_max();

        MonetaryPolicyEngine engine(supply, stake);
        double original_k = engine.parameters().k;

        auto error = engine.set_issuance_coefficient(k);

        ASSERT_TRUE(error.has_value())
            << "k value " << k << " above maximum was accepted on trial " << trial;

        // Verify k was not changed
        ASSERT_EQ(engine.parameters().k, original_k)
            << "k value changed despite rejection on trial " << trial;
    }
}

/**
 * Property: Minimum boundary value is accepted
 * 
 * k = 0.05 (minimum) is accepted.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, MinimumBoundaryIsAccepted) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);

        auto error = engine.set_issuance_coefficient(0.05);

        ASSERT_FALSE(error.has_value())
            << "Minimum boundary k=0.05 was rejected on trial " << trial;

        ASSERT_EQ(engine.parameters().k, 0.05)
            << "Minimum k not set correctly on trial " << trial;
    }
}

/**
 * Property: Maximum boundary value is accepted
 * 
 * k = 0.2 (maximum) is accepted.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, MaximumBoundaryIsAccepted) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);

        auto error = engine.set_issuance_coefficient(0.2);

        ASSERT_FALSE(error.has_value())
            << "Maximum boundary k=0.2 was rejected on trial " << trial;

        ASSERT_EQ(engine.parameters().k, 0.2)
            << "Maximum k not set correctly on trial " << trial;
    }
}

/**
 * Property: Just below minimum is rejected
 * 
 * k = 0.05 - ε is rejected for small ε.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, JustBelowMinimumIsRejected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);
        double original_k = engine.parameters().k;

        // Test values just below minimum
        std::vector<double> test_values = {
            0.04999999,
            0.049,
            0.04,
            0.01
        };

        for (double k : test_values) {
            auto error = engine.set_issuance_coefficient(k);

            ASSERT_TRUE(error.has_value())
                << "k value " << k << " just below minimum was accepted on trial " << trial;

            ASSERT_EQ(engine.parameters().k, original_k)
                << "k value changed despite rejection on trial " << trial;
        }
    }
}

/**
 * Property: Just above maximum is rejected
 * 
 * k = 0.2 + ε is rejected for small ε.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, JustAboveMaximumIsRejected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);
        double original_k = engine.parameters().k;

        // Test values just above maximum
        std::vector<double> test_values = {
            0.20000001,
            0.201,
            0.21,
            0.25
        };

        for (double k : test_values) {
            auto error = engine.set_issuance_coefficient(k);

            ASSERT_TRUE(error.has_value())
                << "k value " << k << " just above maximum was accepted on trial " << trial;

            ASSERT_EQ(engine.parameters().k, original_k)
                << "k value changed despite rejection on trial " << trial;
        }
    }
}

/**
 * Property: Multiple valid updates work correctly
 * 
 * Setting k to multiple valid values in sequence works correctly.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, MultipleValidUpdatesWork) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);

        // Generate sequence of valid k values
        std::uniform_int_distribution<int> count_dist(2, 10);
        int num_updates = count_dist(rng_);

        for (int i = 0; i < num_updates; ++i) {
            double k = generate_valid_k();

            auto error = engine.set_issuance_coefficient(k);

            ASSERT_FALSE(error.has_value())
                << "Valid k value " << k << " was rejected on update " << i
                << " of trial " << trial;

            ASSERT_EQ(engine.parameters().k, k)
                << "k value not set correctly on update " << i
                << " of trial " << trial;
        }
    }
}

/**
 * Property: Invalid update does not corrupt state
 * 
 * Attempting to set an invalid k does not corrupt the engine state.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, InvalidUpdateDoesNotCorruptState) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);

        // Set a valid k
        double valid_k = generate_valid_k();
        engine.set_issuance_coefficient(valid_k);

        // Capture state before invalid update
        double k_before = engine.parameters().k;
        double rate_before = engine.calculate_issuance_rate();
        double sigma_before = engine.calculate_staking_ratio();

        // Attempt invalid update
        double invalid_k = (trial % 2 == 0) ? generate_k_below_min() : generate_k_above_max();
        auto error = engine.set_issuance_coefficient(invalid_k);

        ASSERT_TRUE(error.has_value())
            << "Invalid k was accepted on trial " << trial;

        // Verify state unchanged
        ASSERT_EQ(engine.parameters().k, k_before)
            << "k changed after invalid update on trial " << trial;
        ASSERT_EQ(engine.calculate_issuance_rate(), rate_before)
            << "Issuance rate changed after invalid update on trial " << trial;
        ASSERT_EQ(engine.calculate_staking_ratio(), sigma_before)
            << "Staking ratio changed after invalid update on trial " << trial;
    }
}

/**
 * Property: Default k is within bounds
 * 
 * The default k value (0.1) is within the valid bounds.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, DefaultKIsWithinBounds) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);

        double default_k = engine.parameters().k;

        ASSERT_GE(default_k, 0.05)
            << "Default k is below minimum on trial " << trial;
        ASSERT_LE(default_k, 0.2)
            << "Default k is above maximum on trial " << trial;
    }
}

/**
 * Property: Bounds are consistent with parameters
 * 
 * The min_k and max_k parameters match the enforced bounds.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, BoundsAreConsistentWithParameters) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);

        const IssuanceParameters& params = engine.parameters();

        ASSERT_EQ(params.min_k, 0.05)
            << "min_k parameter incorrect on trial " << trial;
        ASSERT_EQ(params.max_k, 0.2)
            << "max_k parameter incorrect on trial " << trial;

        // Verify bounds are enforced
        auto error_min = engine.set_issuance_coefficient(params.min_k - 0.001);
        ASSERT_TRUE(error_min.has_value())
            << "Value below min_k was accepted on trial " << trial;

        auto error_max = engine.set_issuance_coefficient(params.max_k + 0.001);
        ASSERT_TRUE(error_max.has_value())
            << "Value above max_k was accepted on trial " << trial;
    }
}

/**
 * Property: Error message is descriptive
 * 
 * When k is out of bounds, the error message includes the bounds.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, ErrorMessageIsDescriptive) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);

        // Test with invalid k
        double invalid_k = (trial % 2 == 0) ? 0.01 : 0.5;
        auto error = engine.set_issuance_coefficient(invalid_k);

        ASSERT_TRUE(error.has_value())
            << "Invalid k was accepted on trial " << trial;

        // Verify error message contains bounds information
        std::string error_msg = error.value();
        ASSERT_FALSE(error_msg.empty())
            << "Error message is empty on trial " << trial;

        // Error message should mention the bounds
        ASSERT_NE(error_msg.find("0.05"), std::string::npos)
            << "Error message does not mention min bound on trial " << trial;
        ASSERT_NE(error_msg.find("0.2"), std::string::npos)
            << "Error message does not mention max bound on trial " << trial;
    }
}

/**
 * Property: Negative k is rejected
 * 
 * Negative k values are rejected.
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, NegativeKIsRejected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);
        double original_k = engine.parameters().k;

        std::uniform_real_distribution<double> dist(-1.0, -0.001);
        double negative_k = dist(rng_);

        auto error = engine.set_issuance_coefficient(negative_k);

        ASSERT_TRUE(error.has_value())
            << "Negative k value " << negative_k << " was accepted on trial " << trial;

        ASSERT_EQ(engine.parameters().k, original_k)
            << "k value changed despite rejection on trial " << trial;
    }
}

/**
 * Property: Zero k is rejected
 * 
 * k = 0 is rejected (below minimum).
 */
TEST_F(IssuanceCoefficientBoundsPropertyTest, ZeroKIsRejected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);

        MonetaryPolicyEngine engine(supply, stake);
        double original_k = engine.parameters().k;

        auto error = engine.set_issuance_coefficient(0.0);

        ASSERT_TRUE(error.has_value())
            << "k=0 was accepted on trial " << trial;

        ASSERT_EQ(engine.parameters().k, original_k)
            << "k value changed despite rejection on trial " << trial;
    }
}
