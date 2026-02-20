#include "sarafu/state/fee_market.h"
#include <gtest/gtest.h>
#include <random>
#include <cmath>

using namespace sarafu::state;

/**
 * Property-Based Test for Base Fee Adjustment
 * 
 * **Validates: Requirements 12.2, 12.5**
 * 
 * Property 36: Base Fee Adjustment
 * For any block with gas_used G and target_gas T, the next base fee equals
 * current_base_fee · (1 + (G-T)/T · 0.125), clamped to ±12.5%.
 * 
 * This test validates that:
 * 1. Base fee increases when gas_used > target_gas
 * 2. Base fee decreases when gas_used < target_gas
 * 3. Base fee stays constant when gas_used = target_gas
 * 4. Adjustment is clamped to ±12.5% per block
 * 5. Formula is applied correctly for all gas usage values
 */
class BaseFeeAdjustmentPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random base fee (100 to 100,000)
    uint64_t generate_random_base_fee() {
        std::uniform_int_distribution<uint64_t> dist(100, 100000);
        return dist(rng_);
    }

    // Generate random max gas (1M to 100M)
    uint64_t generate_random_max_gas() {
        std::uniform_int_distribution<uint64_t> dist(1000000, 100000000);
        return dist(rng_);
    }

    // Generate random gas used (0 to max_gas)
    uint64_t generate_random_gas_used(uint64_t max_gas) {
        std::uniform_int_distribution<uint64_t> dist(0, max_gas);
        return dist(rng_);
    }

    std::mt19937 rng_;
};

/**
 * Property: Base fee adjustment follows the formula
 * 
 * For any block with gas_used G and target_gas T,
 * next_base_fee = current_base_fee * (1 + (G-T)/T * 0.125), clamped to ±12.5%.
 */
TEST_F(BaseFeeAdjustmentPropertyTest, BaseFeeFollowsFormula) {
    const int NUM_TRIALS = 1000;
    const double GAMMA = 0.125;
    const double MAX_ADJUSTMENT = 1.125;  // +12.5%
    const double MIN_ADJUSTMENT = 0.875;  // -12.5%

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_used = generate_random_gas_used(max_gas);

        FeeMarket market(base_fee, max_gas);
        uint64_t target_gas = market.target_gas();

        // Calculate expected next base fee
        int64_t gas_delta = static_cast<int64_t>(gas_used) - static_cast<int64_t>(target_gas);
        double adjustment_ratio = static_cast<double>(gas_delta) / static_cast<double>(target_gas);
        double adjustment_factor = adjustment_ratio * GAMMA;
        double multiplier = 1.0 + adjustment_factor;

        // Clamp to ±12.5%
        multiplier = std::max(MIN_ADJUSTMENT, std::min(MAX_ADJUSTMENT, multiplier));

        double expected_base_fee_double = static_cast<double>(base_fee) * multiplier;
        uint64_t expected_base_fee = static_cast<uint64_t>(std::round(expected_base_fee_double));
        expected_base_fee = std::max(uint64_t(1), expected_base_fee);

        // Calculate actual next base fee
        uint64_t actual_base_fee = market.calculate_next_base_fee(gas_used);

        ASSERT_EQ(actual_base_fee, expected_base_fee)
            << "Base fee adjustment formula mismatch on trial " << trial
            << " (base_fee=" << base_fee << ", gas_used=" << gas_used
            << ", target_gas=" << target_gas << ")";
    }
}

/**
 * Property: Base fee increases when gas_used > target_gas
 * 
 * For any block where gas_used significantly exceeds target_gas,
 * the next base fee must be greater than the current base fee.
 * (Note: Very small increases with very low base fees may round to zero)
 */
TEST_F(BaseFeeAdjustmentPropertyTest, BaseFeeIncreasesWhenOverTarget) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        
        FeeMarket market(base_fee, max_gas);
        uint64_t target_gas = market.target_gas();

        // Use gas significantly above target (at least 10% above)
        // This ensures the adjustment is large enough to not round to zero
        uint64_t min_increase = target_gas / 10;
        uint64_t gas_used = target_gas + min_increase;
        if (gas_used > max_gas) {
            gas_used = max_gas;
        }

        uint64_t next_base_fee = market.calculate_next_base_fee(gas_used);

        ASSERT_GT(next_base_fee, base_fee)
            << "Base fee should increase when gas_used > target_gas on trial " << trial
            << " (base_fee=" << base_fee << ", gas_used=" << gas_used
            << ", target_gas=" << target_gas << ")";
    }
}

/**
 * Property: Base fee decreases when gas_used < target_gas
 * 
 * For any block where gas_used < target_gas,
 * the next base fee must be less than the current base fee.
 */
TEST_F(BaseFeeAdjustmentPropertyTest, BaseFeeDecreasesWhenUnderTarget) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        
        FeeMarket market(base_fee, max_gas);
        uint64_t target_gas = market.target_gas();

        // Use gas below target (but not zero to avoid edge case)
        uint64_t gas_used = 1 + (trial % (target_gas - 1));

        uint64_t next_base_fee = market.calculate_next_base_fee(gas_used);

        ASSERT_LT(next_base_fee, base_fee)
            << "Base fee should decrease when gas_used < target_gas on trial " << trial
            << " (base_fee=" << base_fee << ", gas_used=" << gas_used
            << ", target_gas=" << target_gas << ")";
    }
}

/**
 * Property: Base fee stays constant when gas_used = target_gas
 * 
 * For any block where gas_used = target_gas,
 * the next base fee must equal the current base fee.
 */
TEST_F(BaseFeeAdjustmentPropertyTest, BaseFeeConstantAtTarget) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        
        FeeMarket market(base_fee, max_gas);
        uint64_t target_gas = market.target_gas();

        // Use exactly target gas
        uint64_t gas_used = target_gas;

        uint64_t next_base_fee = market.calculate_next_base_fee(gas_used);

        ASSERT_EQ(next_base_fee, base_fee)
            << "Base fee should stay constant when gas_used = target_gas on trial " << trial
            << " (base_fee=" << base_fee << ", target_gas=" << target_gas << ")";
    }
}

/**
 * Property: Maximum increase is clamped to 12.5%
 * 
 * For any block where gas_used = max_gas (100% full),
 * the next base fee increases by at most 12.5%.
 */
TEST_F(BaseFeeAdjustmentPropertyTest, MaximumIncreaseIsClamped) {
    const int NUM_TRIALS = 500;
    const double MAX_INCREASE_FACTOR = 1.125;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        
        FeeMarket market(base_fee, max_gas);

        // Use maximum gas (block is 100% full)
        uint64_t gas_used = max_gas;

        uint64_t next_base_fee = market.calculate_next_base_fee(gas_used);

        // Calculate maximum allowed increase
        uint64_t max_allowed = static_cast<uint64_t>(std::round(base_fee * MAX_INCREASE_FACTOR));

        ASSERT_LE(next_base_fee, max_allowed)
            << "Base fee increase exceeds 12.5% cap on trial " << trial
            << " (base_fee=" << base_fee << ", next_base_fee=" << next_base_fee
            << ", max_allowed=" << max_allowed << ")";

        // Verify it's close to the maximum (within rounding)
        ASSERT_GE(next_base_fee, max_allowed - 1)
            << "Base fee increase should be at maximum when block is full on trial " << trial;
    }
}

/**
 * Property: Maximum decrease is clamped to 12.5%
 * 
 * For any block where gas_used = 0 (empty block),
 * the next base fee decreases by at most 12.5%.
 */
TEST_F(BaseFeeAdjustmentPropertyTest, MaximumDecreaseIsClamped) {
    const int NUM_TRIALS = 500;
    const double MAX_DECREASE_FACTOR = 0.875;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        
        FeeMarket market(base_fee, max_gas);

        // Use zero gas (empty block)
        uint64_t gas_used = 0;

        uint64_t next_base_fee = market.calculate_next_base_fee(gas_used);

        // Calculate minimum allowed (maximum decrease)
        uint64_t min_allowed = static_cast<uint64_t>(std::round(base_fee * MAX_DECREASE_FACTOR));

        ASSERT_GE(next_base_fee, min_allowed)
            << "Base fee decrease exceeds 12.5% cap on trial " << trial
            << " (base_fee=" << base_fee << ", next_base_fee=" << next_base_fee
            << ", min_allowed=" << min_allowed << ")";

        // Verify it's close to the minimum (within rounding)
        ASSERT_LE(next_base_fee, min_allowed + 1)
            << "Base fee decrease should be at maximum when block is empty on trial " << trial;
    }
}

/**
 * Property: Base fee adjustment is deterministic
 * 
 * For any gas_used value, calculating the next base fee multiple times
 * yields the same result.
 */
TEST_F(BaseFeeAdjustmentPropertyTest, AdjustmentIsDeterministic) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_used = generate_random_gas_used(max_gas);
        
        FeeMarket market(base_fee, max_gas);

        // Calculate next base fee multiple times
        uint64_t result1 = market.calculate_next_base_fee(gas_used);
        uint64_t result2 = market.calculate_next_base_fee(gas_used);
        uint64_t result3 = market.calculate_next_base_fee(gas_used);

        ASSERT_EQ(result1, result2)
            << "Base fee calculation not deterministic (result1 != result2) on trial " << trial;
        ASSERT_EQ(result2, result3)
            << "Base fee calculation not deterministic (result2 != result3) on trial " << trial;
    }
}

/**
 * Property: Update base fee changes state
 * 
 * For any gas_used value, calling update_base_fee() updates the
 * internal state to the new base fee.
 */
TEST_F(BaseFeeAdjustmentPropertyTest, UpdateBaseFeeChangesState) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_used = generate_random_gas_used(max_gas);
        
        FeeMarket market(base_fee, max_gas);

        uint64_t expected_next = market.calculate_next_base_fee(gas_used);
        uint64_t actual_next = market.update_base_fee(gas_used);

        ASSERT_EQ(actual_next, expected_next)
            << "update_base_fee return value mismatch on trial " << trial;

        ASSERT_EQ(market.current_base_fee(), expected_next)
            << "Base fee state not updated correctly on trial " << trial;
    }
}

/**
 * Property: Base fee never goes below 1
 * 
 * For any base fee and gas usage, the next base fee is always at least 1.
 */
TEST_F(BaseFeeAdjustmentPropertyTest, BaseFeeNeverBelowOne) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Test with very low base fees
        uint64_t base_fee = 1 + (trial % 10);
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_used = generate_random_gas_used(max_gas);
        
        FeeMarket market(base_fee, max_gas);

        uint64_t next_base_fee = market.calculate_next_base_fee(gas_used);

        ASSERT_GE(next_base_fee, 1)
            << "Base fee went below 1 on trial " << trial
            << " (base_fee=" << base_fee << ", next_base_fee=" << next_base_fee << ")";
    }
}

/**
 * Property: Sequential adjustments compound correctly
 * 
 * For any sequence of gas usage values, applying adjustments sequentially
 * produces the correct final base fee.
 */
TEST_F(BaseFeeAdjustmentPropertyTest, SequentialAdjustmentsCompound) {
    const int NUM_TRIALS = 100;
    const int BLOCKS_PER_TRIAL = 10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        
        FeeMarket market(base_fee, max_gas);

        // Apply multiple adjustments
        for (int block = 0; block < BLOCKS_PER_TRIAL; ++block) {
            uint64_t gas_used = generate_random_gas_used(max_gas);
            uint64_t expected_next = market.calculate_next_base_fee(gas_used);
            market.update_base_fee(gas_used);

            ASSERT_EQ(market.current_base_fee(), expected_next)
                << "Sequential adjustment mismatch on trial " << trial
                << ", block " << block;
        }
    }
}

/**
 * Property: Target gas is always 50% of max gas
 * 
 * For any max_gas value, target_gas = max_gas / 2.
 */
TEST_F(BaseFeeAdjustmentPropertyTest, TargetGasIsHalfOfMax) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        
        FeeMarket market(base_fee, max_gas);

        uint64_t target_gas = market.target_gas();
        uint64_t expected_target = max_gas / 2;

        ASSERT_EQ(target_gas, expected_target)
            << "Target gas is not 50% of max gas on trial " << trial
            << " (max_gas=" << max_gas << ", target_gas=" << target_gas << ")";
    }
}
