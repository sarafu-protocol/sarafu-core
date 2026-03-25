#include "sarafu/consensus/slashing_detector.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <cmath>

using namespace sarafu::consensus;

/**
 * Property-Based Test for Quadratic Slashing Calculation
 * 
 * **Validates: Requirements 3.3, 3.7**
 * 
 * Property 11: Quadratic Slashing Calculation
 * For any safety violation involving validators with total stake Sviolating,
 * each validator i's penalty equals min(1.0, α·si/Stotal + β·(Sviolating/Stotal)^2)·si
 * where α=0.05 and β=0.5.
 * 
 * This test validates that:
 * 1. The penalty formula is correctly implemented
 * 2. Penalties scale with individual stake
 * 3. Penalties scale with total violating stake (correlation)
 * 4. Penalties never exceed validator's stake
 * 5. Penalties are proportional to the severity of collusion
 */
class QuadraticSlashingCalculationPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        detector_ = std::make_unique<SlashingDetector>();
        
        // Constants from the spec
        alpha_ = 0.05;
        beta_ = 0.5;
    }

    // Calculate expected penalty using the formula
    uint64_t calculate_expected_penalty(
        uint64_t validator_stake,
        uint64_t total_stake,
        uint64_t total_violating_stake
    ) const {
        if (total_stake == 0 || validator_stake == 0) {
            return 0;
        }

        double si = static_cast<double>(validator_stake);
        double S_total = static_cast<double>(total_stake);
        double S_violating = static_cast<double>(total_violating_stake);

        double individual_term = alpha_ * (si / S_total);
        double correlation_term = 0.0;
        if (S_total > 0.0) {
            double ratio = S_violating / S_total;
            correlation_term = beta_ * ratio * ratio;
        }
        double penalty_fraction = std::min(1.0, individual_term + correlation_term);

        uint64_t penalty = static_cast<uint64_t>(penalty_fraction * si);
        return std::min(penalty, validator_stake);
    }

    std::mt19937 rng_;
    std::unique_ptr<SlashingDetector> detector_;
    double alpha_;
    double beta_;
};

/**
 * Property 11: Quadratic Slashing Calculation
 * 
 * For any safety violation, the penalty is calculated using the quadratic
 * correlated slashing formula with α=0.05 and β=0.5.
 */
TEST_F(QuadraticSlashingCalculationPropertyTest, QuadraticSlashingFormula) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random total stake (1M to 1B tokens)
        std::uniform_int_distribution<uint64_t> total_stake_dist(1000000, 1000000000);
        uint64_t total_stake = total_stake_dist(rng_);

        // Generate random validator stake (1% to 10% of total)
        std::uniform_int_distribution<uint64_t> validator_stake_dist(
            total_stake / 100,
            total_stake / 10
        );
        uint64_t validator_stake = validator_stake_dist(rng_);

        // Generate random number of co-violators (1 to 10)
        std::uniform_int_distribution<int> num_violators_dist(1, 10);
        int num_violators = num_violators_dist(rng_);

        // Generate co-violator stakes
        std::vector<uint64_t> co_violator_stakes;
        uint64_t total_violating_stake = validator_stake;  // Include this validator
        
        for (int i = 0; i < num_violators - 1; ++i) {
            std::uniform_int_distribution<uint64_t> co_stake_dist(
                total_stake / 100,
                total_stake / 10
            );
            uint64_t co_stake = co_stake_dist(rng_);
            co_violator_stakes.push_back(co_stake);
            total_violating_stake += co_stake;
        }
        co_violator_stakes.push_back(validator_stake);  // Include this validator

        // Calculate penalty using detector
        uint64_t actual_penalty = detector_->calculate_penalty(
            validator_stake,
            total_stake,
            co_violator_stakes
        );

        // Calculate expected penalty
        uint64_t expected_penalty = calculate_expected_penalty(
            validator_stake,
            total_stake,
            total_violating_stake
        );

        // Allow small rounding differences (within 1 token)
        EXPECT_NEAR(actual_penalty, expected_penalty, 1)
            << "Penalty calculation incorrect for trial " << trial
            << "\n  Validator stake: " << validator_stake
            << "\n  Total stake: " << total_stake
            << "\n  Total violating stake: " << total_violating_stake
            << "\n  Actual penalty: " << actual_penalty
            << "\n  Expected penalty: " << expected_penalty;
    }
}

/**
 * Property: Penalty never exceeds validator's stake
 * 
 * No matter how large the violation, the penalty should never
 * exceed the validator's bonded stake.
 */
TEST_F(QuadraticSlashingCalculationPropertyTest, PenaltyNeverExceedsStake) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random total stake
        std::uniform_int_distribution<uint64_t> total_stake_dist(1000000, 1000000000);
        uint64_t total_stake = total_stake_dist(rng_);

        // Generate random validator stake
        std::uniform_int_distribution<uint64_t> validator_stake_dist(
            total_stake / 100,
            total_stake / 10
        );
        uint64_t validator_stake = validator_stake_dist(rng_);

        // Generate large violating stake (up to 100% of total)
        std::uniform_int_distribution<uint64_t> violating_stake_dist(
            validator_stake,
            total_stake
        );
        uint64_t total_violating_stake = violating_stake_dist(rng_);

        std::vector<uint64_t> co_violator_stakes = {total_violating_stake};

        // Calculate penalty
        uint64_t penalty = detector_->calculate_penalty(
            validator_stake,
            total_stake,
            co_violator_stakes
        );

        // Penalty should never exceed validator's stake
        EXPECT_LE(penalty, validator_stake)
            << "Penalty exceeds validator stake for trial " << trial
            << "\n  Validator stake: " << validator_stake
            << "\n  Penalty: " << penalty;
    }
}

/**
 * Property: Penalty increases with individual stake
 * 
 * For the same total stake and violating stake, a validator with
 * more stake should receive a higher absolute penalty.
 */
TEST_F(QuadraticSlashingCalculationPropertyTest, PenaltyIncreasesWithStake) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random total stake
        std::uniform_int_distribution<uint64_t> total_stake_dist(10000000, 1000000000);
        uint64_t total_stake = total_stake_dist(rng_);

        // Generate two different validator stakes
        std::uniform_int_distribution<uint64_t> stake1_dist(
            total_stake / 100,
            total_stake / 20
        );
        uint64_t validator_stake1 = stake1_dist(rng_);
        
        std::uniform_int_distribution<uint64_t> stake2_dist(
            total_stake / 10,
            total_stake / 5
        );
        uint64_t validator_stake2 = stake2_dist(rng_);

        // Ensure stake2 > stake1
        if (validator_stake2 <= validator_stake1) {
            continue;
        }

        // Generate same violating stake for both
        std::uniform_int_distribution<uint64_t> violating_stake_dist(
            total_stake / 10,
            total_stake / 3
        );
        uint64_t total_violating_stake = violating_stake_dist(rng_);

        std::vector<uint64_t> co_violator_stakes = {total_violating_stake};

        // Calculate penalties
        uint64_t penalty1 = detector_->calculate_penalty(
            validator_stake1,
            total_stake,
            co_violator_stakes
        );

        uint64_t penalty2 = detector_->calculate_penalty(
            validator_stake2,
            total_stake,
            co_violator_stakes
        );

        // Higher stake should result in higher absolute penalty
        EXPECT_GT(penalty2, penalty1)
            << "Penalty did not increase with stake for trial " << trial
            << "\n  Stake 1: " << validator_stake1 << " -> Penalty: " << penalty1
            << "\n  Stake 2: " << validator_stake2 << " -> Penalty: " << penalty2;
    }
}

/**
 * Property: Penalty increases with total violating stake (correlation)
 * 
 * For the same validator stake and total stake, a larger group of
 * co-violators should result in a higher penalty (quadratic correlation).
 */
TEST_F(QuadraticSlashingCalculationPropertyTest, PenaltyIncreasesWithCollusion) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random total stake
        std::uniform_int_distribution<uint64_t> total_stake_dist(10000000, 1000000000);
        uint64_t total_stake = total_stake_dist(rng_);

        // Generate validator stake
        std::uniform_int_distribution<uint64_t> validator_stake_dist(
            total_stake / 100,
            total_stake / 20
        );
        uint64_t validator_stake = validator_stake_dist(rng_);

        // Generate two different violating stakes (small and large collusion)
        std::uniform_int_distribution<uint64_t> small_violating_dist(
            validator_stake,
            total_stake / 10
        );
        uint64_t small_violating_stake = small_violating_dist(rng_);

        std::uniform_int_distribution<uint64_t> large_violating_dist(
            total_stake / 5,
            total_stake / 2
        );
        uint64_t large_violating_stake = large_violating_dist(rng_);

        // Ensure large > small
        if (large_violating_stake <= small_violating_stake) {
            continue;
        }

        // Calculate penalties
        uint64_t penalty_small = detector_->calculate_penalty(
            validator_stake,
            total_stake,
            {small_violating_stake}
        );

        uint64_t penalty_large = detector_->calculate_penalty(
            validator_stake,
            total_stake,
            {large_violating_stake}
        );

        // Larger collusion should result in higher penalty
        EXPECT_GT(penalty_large, penalty_small)
            << "Penalty did not increase with collusion for trial " << trial
            << "\n  Small violating stake: " << small_violating_stake << " -> Penalty: " << penalty_small
            << "\n  Large violating stake: " << large_violating_stake << " -> Penalty: " << penalty_large;
    }
}

/**
 * Property: Zero stake results in zero penalty
 * 
 * If validator has zero stake, penalty should be zero.
 */
TEST_F(QuadraticSlashingCalculationPropertyTest, ZeroStakeZeroPenalty) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random total stake
        std::uniform_int_distribution<uint64_t> total_stake_dist(1000000, 1000000000);
        uint64_t total_stake = total_stake_dist(rng_);

        // Generate random violating stake
        std::uniform_int_distribution<uint64_t> violating_stake_dist(
            total_stake / 10,
            total_stake / 3
        );
        uint64_t total_violating_stake = violating_stake_dist(rng_);

        // Calculate penalty for zero stake
        uint64_t penalty = detector_->calculate_penalty(
            0,  // Zero validator stake
            total_stake,
            {total_violating_stake}
        );

        EXPECT_EQ(penalty, 0)
            << "Zero stake should result in zero penalty for trial " << trial;
    }
}

/**
 * Property: Zero total stake results in zero penalty
 * 
 * If total stake is zero (edge case), penalty should be zero.
 */
TEST_F(QuadraticSlashingCalculationPropertyTest, ZeroTotalStakeZeroPenalty) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random validator stake
        std::uniform_int_distribution<uint64_t> validator_stake_dist(1000, 1000000);
        uint64_t validator_stake = validator_stake_dist(rng_);

        // Calculate penalty with zero total stake
        uint64_t penalty = detector_->calculate_penalty(
            validator_stake,
            0,  // Zero total stake
            {validator_stake}
        );

        EXPECT_EQ(penalty, 0)
            << "Zero total stake should result in zero penalty for trial " << trial;
    }
}

/**
 * Property: Individual penalty component (α term)
 * 
 * For a single violator (no collusion), the penalty should be
 * approximately α·si (5% of stake).
 */
TEST_F(QuadraticSlashingCalculationPropertyTest, IndividualPenaltyComponent) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random total stake
        std::uniform_int_distribution<uint64_t> total_stake_dist(10000000, 1000000000);
        uint64_t total_stake = total_stake_dist(rng_);

        // Generate small validator stake (< 1% of total to keep penalty < 100%)
        std::uniform_int_distribution<uint64_t> validator_stake_dist(
            total_stake / 1000,
            total_stake / 100
        );
        uint64_t validator_stake = validator_stake_dist(rng_);

        // Single violator (only this validator)
        std::vector<uint64_t> co_violator_stakes = {validator_stake};

        // Calculate penalty
        uint64_t penalty = detector_->calculate_penalty(
            validator_stake,
            total_stake,
            co_violator_stakes
        );

        // Expected penalty: α·si/Stotal·si = α·si (since si/Stotal is small)
        // For small stakes, the correlation term is negligible
        double expected_fraction = alpha_ * (static_cast<double>(validator_stake) / static_cast<double>(total_stake));
        expected_fraction += beta_ * (static_cast<double>(validator_stake) / static_cast<double>(total_stake)) * 
                            (static_cast<double>(validator_stake) / static_cast<double>(total_stake));
        uint64_t expected_penalty = static_cast<uint64_t>(expected_fraction * validator_stake);

        // Allow 1% tolerance for rounding
        double tolerance = validator_stake * 0.01;
        EXPECT_NEAR(penalty, expected_penalty, tolerance)
            << "Individual penalty component incorrect for trial " << trial
            << "\n  Validator stake: " << validator_stake
            << "\n  Total stake: " << total_stake
            << "\n  Penalty: " << penalty
            << "\n  Expected: " << expected_penalty;
    }
}

/**
 * Property: Correlation penalty component (β term)
 * 
 * For large collusions (e.g., 1/3 of stake), the correlation term
 * should dominate and result in significant penalties.
 */
TEST_F(QuadraticSlashingCalculationPropertyTest, CorrelationPenaltyComponent) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random total stake
        std::uniform_int_distribution<uint64_t> total_stake_dist(10000000, 1000000000);
        uint64_t total_stake = total_stake_dist(rng_);

        // Generate validator stake (part of large collusion)
        std::uniform_int_distribution<uint64_t> validator_stake_dist(
            total_stake / 100,
            total_stake / 50
        );
        uint64_t validator_stake = validator_stake_dist(rng_);

        // Large collusion: 1/3 of total stake
        uint64_t total_violating_stake = total_stake / 3;

        // Calculate penalty
        uint64_t penalty = detector_->calculate_penalty(
            validator_stake,
            total_stake,
            {total_violating_stake}
        );

        // For 1/3 collusion, penalty should be significant
        // Expected: min(1.0, 0.05·si/S + 0.5·(1/3)^2)·si
        double si_over_S = static_cast<double>(validator_stake) / static_cast<double>(total_stake);
        double expected_fraction = alpha_ * si_over_S + beta_ * (1.0 / 3.0) * (1.0 / 3.0);
        expected_fraction = std::min(1.0, expected_fraction);
        uint64_t expected_penalty = static_cast<uint64_t>(expected_fraction * validator_stake);

        // Allow 1% tolerance
        double tolerance = validator_stake * 0.01;
        EXPECT_NEAR(penalty, expected_penalty, tolerance)
            << "Correlation penalty component incorrect for trial " << trial
            << "\n  Validator stake: " << validator_stake
            << "\n  Total stake: " << total_stake
            << "\n  Total violating stake: " << total_violating_stake
            << "\n  Penalty: " << penalty
            << "\n  Expected: " << expected_penalty;
    }
}

/**
 * Property: Penalty is deterministic
 * 
 * Calculating the penalty multiple times with the same inputs
 * should always produce the same result.
 */
TEST_F(QuadraticSlashingCalculationPropertyTest, PenaltyIsDeterministic) {
    const int NUM_TRIALS = 500;
    const int NUM_REPETITIONS = 10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random parameters
        std::uniform_int_distribution<uint64_t> total_stake_dist(1000000, 1000000000);
        uint64_t total_stake = total_stake_dist(rng_);

        std::uniform_int_distribution<uint64_t> validator_stake_dist(
            total_stake / 100,
            total_stake / 10
        );
        uint64_t validator_stake = validator_stake_dist(rng_);

        std::uniform_int_distribution<uint64_t> violating_stake_dist(
            validator_stake,
            total_stake / 2
        );
        uint64_t total_violating_stake = violating_stake_dist(rng_);

        std::vector<uint64_t> co_violator_stakes = {total_violating_stake};

        // Calculate penalty multiple times
        uint64_t first_penalty = detector_->calculate_penalty(
            validator_stake,
            total_stake,
            co_violator_stakes
        );

        for (int rep = 1; rep < NUM_REPETITIONS; ++rep) {
            uint64_t subsequent_penalty = detector_->calculate_penalty(
                validator_stake,
                total_stake,
                co_violator_stakes
            );

            EXPECT_EQ(first_penalty, subsequent_penalty)
                << "Penalty calculation not deterministic for trial " << trial
                << ", repetition " << rep;
        }
    }
}

/**
 * Property: Penalty with multiple co-violators
 * 
 * The penalty should correctly sum up stakes from multiple co-violators.
 */
TEST_F(QuadraticSlashingCalculationPropertyTest, MultipleCoViolators) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random total stake
        std::uniform_int_distribution<uint64_t> total_stake_dist(10000000, 1000000000);
        uint64_t total_stake = total_stake_dist(rng_);

        // Generate validator stake
        std::uniform_int_distribution<uint64_t> validator_stake_dist(
            total_stake / 100,
            total_stake / 20
        );
        uint64_t validator_stake = validator_stake_dist(rng_);

        // Generate multiple co-violator stakes
        std::uniform_int_distribution<int> num_violators_dist(2, 10);
        int num_violators = num_violators_dist(rng_);

        std::vector<uint64_t> co_violator_stakes;
        uint64_t total_violating_stake = 0;

        for (int i = 0; i < num_violators; ++i) {
            std::uniform_int_distribution<uint64_t> co_stake_dist(
                total_stake / 200,
                total_stake / 50
            );
            uint64_t co_stake = co_stake_dist(rng_);
            co_violator_stakes.push_back(co_stake);
            total_violating_stake += co_stake;
        }

        // Calculate penalty with multiple co-violators
        uint64_t penalty_multiple = detector_->calculate_penalty(
            validator_stake,
            total_stake,
            co_violator_stakes
        );

        // Calculate penalty with single aggregated stake
        uint64_t penalty_single = detector_->calculate_penalty(
            validator_stake,
            total_stake,
            {total_violating_stake}
        );

        // Should be the same (penalty depends on total violating stake)
        EXPECT_EQ(penalty_multiple, penalty_single)
            << "Penalty with multiple co-violators incorrect for trial " << trial
            << "\n  Penalty (multiple): " << penalty_multiple
            << "\n  Penalty (single): " << penalty_single;
    }
}
