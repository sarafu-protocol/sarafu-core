#include "sarafu/state/fee_market.h"
#include <gtest/gtest.h>
#include <random>

using namespace sarafu::state;

/**
 * Property-Based Test for Minimum Fee Enforcement
 * 
 * **Validates: Requirements 12.8**
 * 
 * Property 39: Minimum Fee Enforcement
 * For any transaction T, if T.fee < current_base_fee * gas_limit,
 * the transaction is rejected.
 * 
 * This test validates that:
 * 1. Transactions with fee >= base_fee * gas_limit are accepted
 * 2. Transactions with fee < base_fee * gas_limit are rejected
 * 3. Fee sufficiency check is accurate for all fee and gas values
 * 4. Edge cases (zero fee, zero gas) are handled correctly
 */
class MinimumFeeEnforcementPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random base fee (100 to 10,000)
    uint64_t generate_random_base_fee() {
        std::uniform_int_distribution<uint64_t> dist(100, 10000);
        return dist(rng_);
    }

    // Generate random gas limit (1,000 to 1,000,000)
    uint64_t generate_random_gas_limit() {
        std::uniform_int_distribution<uint64_t> dist(1000, 1000000);
        return dist(rng_);
    }

    // Generate random max gas (10M to 100M)
    uint64_t generate_random_max_gas() {
        std::uniform_int_distribution<uint64_t> dist(10000000, 100000000);
        return dist(rng_);
    }

    std::mt19937 rng_;
};

/**
 * Property: Transactions with sufficient fee are accepted
 * 
 * For any transaction with fee >= base_fee * gas_limit,
 * is_fee_sufficient() returns true.
 */
TEST_F(MinimumFeeEnforcementPropertyTest, SufficientFeeIsAccepted) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_limit = generate_random_gas_limit();

        FeeMarket market(base_fee, max_gas);

        // Calculate minimum required fee
        uint64_t minimum_fee = base_fee * gas_limit;

        // Test with exactly minimum fee
        ASSERT_TRUE(market.is_fee_sufficient(minimum_fee, gas_limit))
            << "Transaction with exactly minimum fee should be accepted on trial " << trial
            << " (base_fee=" << base_fee << ", gas_limit=" << gas_limit
            << ", minimum_fee=" << minimum_fee << ")";

        // Test with fee above minimum
        uint64_t extra_fee = 1 + (trial % 10000);
        ASSERT_TRUE(market.is_fee_sufficient(minimum_fee + extra_fee, gas_limit))
            << "Transaction with fee above minimum should be accepted on trial " << trial
            << " (base_fee=" << base_fee << ", gas_limit=" << gas_limit
            << ", fee=" << (minimum_fee + extra_fee) << ")";
    }
}

/**
 * Property: Transactions with insufficient fee are rejected
 * 
 * For any transaction with fee < base_fee * gas_limit,
 * is_fee_sufficient() returns false.
 */
TEST_F(MinimumFeeEnforcementPropertyTest, InsufficientFeeIsRejected) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_limit = generate_random_gas_limit();

        FeeMarket market(base_fee, max_gas);

        // Calculate minimum required fee
        uint64_t minimum_fee = base_fee * gas_limit;

        // Test with fee below minimum (if minimum > 0)
        if (minimum_fee > 0) {
            uint64_t insufficient_fee = minimum_fee - 1;
            ASSERT_FALSE(market.is_fee_sufficient(insufficient_fee, gas_limit))
                << "Transaction with insufficient fee should be rejected on trial " << trial
                << " (base_fee=" << base_fee << ", gas_limit=" << gas_limit
                << ", fee=" << insufficient_fee << ", minimum=" << minimum_fee << ")";
        }

        // Test with zero fee
        ASSERT_FALSE(market.is_fee_sufficient(0, gas_limit))
            << "Transaction with zero fee should be rejected on trial " << trial
            << " (base_fee=" << base_fee << ", gas_limit=" << gas_limit << ")";
    }
}

/**
 * Property: Fee estimation matches minimum requirement
 * 
 * For any gas_limit and priority_fee,
 * estimate_fee() returns base_fee * gas_limit + priority_fee.
 */
TEST_F(MinimumFeeEnforcementPropertyTest, FeeEstimationIsAccurate) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_limit = generate_random_gas_limit();
        uint64_t priority_fee = trial % 10000;

        FeeMarket market(base_fee, max_gas);

        uint64_t estimated_fee = market.estimate_fee(gas_limit, priority_fee);
        uint64_t expected_fee = (base_fee * gas_limit) + priority_fee;

        ASSERT_EQ(estimated_fee, expected_fee)
            << "Fee estimation mismatch on trial " << trial
            << " (base_fee=" << base_fee << ", gas_limit=" << gas_limit
            << ", priority_fee=" << priority_fee << ")";
    }
}

/**
 * Property: Estimated fee is always sufficient
 * 
 * For any gas_limit and priority_fee,
 * the fee returned by estimate_fee() is sufficient according to is_fee_sufficient().
 */
TEST_F(MinimumFeeEnforcementPropertyTest, EstimatedFeeIsSufficient) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_limit = generate_random_gas_limit();
        uint64_t priority_fee = trial % 10000;

        FeeMarket market(base_fee, max_gas);

        uint64_t estimated_fee = market.estimate_fee(gas_limit, priority_fee);

        ASSERT_TRUE(market.is_fee_sufficient(estimated_fee, gas_limit))
            << "Estimated fee should always be sufficient on trial " << trial
            << " (base_fee=" << base_fee << ", gas_limit=" << gas_limit
            << ", estimated_fee=" << estimated_fee << ")";
    }
}

/**
 * Property: Fee sufficiency is deterministic
 * 
 * For any fee and gas_limit, checking fee sufficiency multiple times
 * yields the same result.
 */
TEST_F(MinimumFeeEnforcementPropertyTest, FeeSufficiencyIsDeterministic) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_limit = generate_random_gas_limit();
        uint64_t fee = trial * 1000;

        FeeMarket market(base_fee, max_gas);

        bool result1 = market.is_fee_sufficient(fee, gas_limit);
        bool result2 = market.is_fee_sufficient(fee, gas_limit);
        bool result3 = market.is_fee_sufficient(fee, gas_limit);

        ASSERT_EQ(result1, result2)
            << "Fee sufficiency check not deterministic (result1 != result2) on trial " << trial;
        ASSERT_EQ(result2, result3)
            << "Fee sufficiency check not deterministic (result2 != result3) on trial " << trial;
    }
}

/**
 * Property: Zero gas limit requires zero fee
 * 
 * For any base_fee, a transaction with gas_limit = 0 requires fee = 0.
 */
TEST_F(MinimumFeeEnforcementPropertyTest, ZeroGasRequiresZeroFee) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();

        FeeMarket market(base_fee, max_gas);

        // Zero gas limit should require zero fee
        ASSERT_TRUE(market.is_fee_sufficient(0, 0))
            << "Zero gas limit with zero fee should be sufficient on trial " << trial;

        // Any positive fee with zero gas should also be sufficient
        ASSERT_TRUE(market.is_fee_sufficient(1000, 0))
            << "Zero gas limit with positive fee should be sufficient on trial " << trial;
    }
}

/**
 * Property: Higher base fee requires higher minimum fee
 * 
 * For any gas_limit, if base_fee2 > base_fee1,
 * then minimum_fee2 > minimum_fee1.
 */
TEST_F(MinimumFeeEnforcementPropertyTest, HigherBaseFeeRequiresHigherMinimum) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee1 = generate_random_base_fee();
        uint64_t base_fee2 = base_fee1 + 100 + (trial % 1000);
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_limit = generate_random_gas_limit();

        FeeMarket market1(base_fee1, max_gas);
        FeeMarket market2(base_fee2, max_gas);

        uint64_t min_fee1 = market1.estimate_fee(gas_limit, 0);
        uint64_t min_fee2 = market2.estimate_fee(gas_limit, 0);

        ASSERT_GT(min_fee2, min_fee1)
            << "Higher base fee should require higher minimum fee on trial " << trial
            << " (base_fee1=" << base_fee1 << ", base_fee2=" << base_fee2
            << ", min_fee1=" << min_fee1 << ", min_fee2=" << min_fee2 << ")";
    }
}

/**
 * Property: Higher gas limit requires higher minimum fee
 * 
 * For any base_fee, if gas_limit2 > gas_limit1,
 * then minimum_fee2 > minimum_fee1.
 */
TEST_F(MinimumFeeEnforcementPropertyTest, HigherGasLimitRequiresHigherMinimum) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_limit1 = generate_random_gas_limit();
        uint64_t gas_limit2 = gas_limit1 + 1000 + (trial % 10000);

        FeeMarket market(base_fee, max_gas);

        uint64_t min_fee1 = market.estimate_fee(gas_limit1, 0);
        uint64_t min_fee2 = market.estimate_fee(gas_limit2, 0);

        ASSERT_GT(min_fee2, min_fee1)
            << "Higher gas limit should require higher minimum fee on trial " << trial
            << " (gas_limit1=" << gas_limit1 << ", gas_limit2=" << gas_limit2
            << ", min_fee1=" << min_fee1 << ", min_fee2=" << min_fee2 << ")";
    }
}

/**
 * Property: Minimum fee scales linearly with gas limit
 * 
 * For any base_fee, doubling the gas_limit doubles the minimum fee.
 */
TEST_F(MinimumFeeEnforcementPropertyTest, MinimumFeeScalesLinearlyWithGas) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_limit1 = generate_random_gas_limit();
        uint64_t gas_limit2 = gas_limit1 * 2;

        FeeMarket market(base_fee, max_gas);

        uint64_t min_fee1 = market.estimate_fee(gas_limit1, 0);
        uint64_t min_fee2 = market.estimate_fee(gas_limit2, 0);

        ASSERT_EQ(min_fee2, min_fee1 * 2)
            << "Minimum fee should scale linearly with gas limit on trial " << trial
            << " (gas_limit1=" << gas_limit1 << ", gas_limit2=" << gas_limit2
            << ", min_fee1=" << min_fee1 << ", min_fee2=" << min_fee2 << ")";
    }
}

/**
 * Property: Minimum fee scales linearly with base fee
 * 
 * For any gas_limit, doubling the base_fee doubles the minimum fee.
 */
TEST_F(MinimumFeeEnforcementPropertyTest, MinimumFeeScalesLinearlyWithBaseFee) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee1 = generate_random_base_fee();
        uint64_t base_fee2 = base_fee1 * 2;
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_limit = generate_random_gas_limit();

        FeeMarket market1(base_fee1, max_gas);
        FeeMarket market2(base_fee2, max_gas);

        uint64_t min_fee1 = market1.estimate_fee(gas_limit, 0);
        uint64_t min_fee2 = market2.estimate_fee(gas_limit, 0);

        ASSERT_EQ(min_fee2, min_fee1 * 2)
            << "Minimum fee should scale linearly with base fee on trial " << trial
            << " (base_fee1=" << base_fee1 << ", base_fee2=" << base_fee2
            << ", min_fee1=" << min_fee1 << ", min_fee2=" << min_fee2 << ")";
    }
}

/**
 * Property: Priority fee does not affect fee sufficiency
 * 
 * For any transaction, fee sufficiency depends only on base_fee * gas_limit,
 * not on the priority fee component.
 */
TEST_F(MinimumFeeEnforcementPropertyTest, PriorityFeeDoesNotAffectSufficiency) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t base_fee = generate_random_base_fee();
        uint64_t max_gas = generate_random_max_gas();
        uint64_t gas_limit = generate_random_gas_limit();

        FeeMarket market(base_fee, max_gas);

        uint64_t minimum_fee = base_fee * gas_limit;

        // Test with various priority fees
        for (uint64_t priority_fee = 0; priority_fee <= 10000; priority_fee += 1000) {
            uint64_t total_fee = minimum_fee + priority_fee;

            ASSERT_TRUE(market.is_fee_sufficient(total_fee, gas_limit))
                << "Fee with priority should be sufficient on trial " << trial
                << " (minimum_fee=" << minimum_fee << ", priority_fee=" << priority_fee << ")";
        }
    }
}
