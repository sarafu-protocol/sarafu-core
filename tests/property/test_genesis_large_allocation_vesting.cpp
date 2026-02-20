#include "sarafu/consensus/genesis_builder.h"
#include "sarafu/state/account.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace sarafu::consensus;
using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Test for Large Allocation Vesting
 * 
 * **Validates: Requirements 16.3**
 * 
 * Property 60: Large Allocation Vesting
 * For any genesis allocation A where A > 2% of total supply,
 * A has a 12-month linear vesting schedule.
 * 
 * This test validates that:
 * 1. Allocations >2% of supply automatically get vesting schedules
 * 2. Vesting schedules are 12 months (31,536,000 seconds)
 * 3. Vesting is linear over the 12-month period
 * 4. Allocations ≤2% do not require vesting
 * 5. Vesting calculations are correct at various timestamps
 */
class GenesisLargeAllocationVestingPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random address
    Address generate_random_address() {
        std::vector<uint8_t> data(32);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 32; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return Address(data);
    }

    // Generate random total supply
    uint64_t generate_random_total_supply(
        uint64_t min = 1000000000000ULL,
        uint64_t max = 10000000000000ULL
    ) {
        std::uniform_int_distribution<uint64_t> dist(min, max);
        return dist(rng_);
    }

    // Generate random amount
    uint64_t generate_random_amount(uint64_t min, uint64_t max) {
        std::uniform_int_distribution<uint64_t> dist(min, max);
        return dist(rng_);
    }

    // Generate random timestamp
    uint64_t generate_random_timestamp(
        uint64_t min = 1700000000,
        uint64_t max = 1800000000
    ) {
        std::uniform_int_distribution<uint64_t> dist(min, max);
        return dist(rng_);
    }

    std::mt19937 rng_;
    
    static constexpr uint64_t TWELVE_MONTHS_SECONDS = 31536000;  // 365 days
};

/**
 * Property: Allocations >2% automatically get vesting schedules
 * 
 * For any allocation A where A > 2% of total supply,
 * the allocation must have a vesting schedule.
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, AllocationsAbove2PercentHaveVesting) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 2% threshold and 5% cap
        uint64_t threshold = static_cast<uint64_t>(total_supply * 0.02);
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Generate allocation above 2% but below 5%
        uint64_t amount = generate_random_amount(threshold + 1, cap);
        Address address = generate_random_address();
        
        bool result = builder.add_allocation(address, amount, "Test");
        
        ASSERT_TRUE(result)
            << "Allocation above 2% was rejected on trial " << trial;
        
        // Verify allocation has vesting schedule
        const auto& allocations = builder.get_allocations();
        ASSERT_FALSE(allocations.empty())
            << "No allocations found on trial " << trial;
        
        bool found_with_vesting = false;
        for (const auto& alloc : allocations) {
            if (alloc.address == address) {
                ASSERT_TRUE(alloc.vesting_schedule.has_value())
                    << "Allocation above 2% missing vesting schedule on trial " << trial
                    << " (amount: " << amount << ", threshold: " << threshold << ")";
                found_with_vesting = true;
                break;
            }
        }
        
        ASSERT_TRUE(found_with_vesting)
            << "Allocation not found in builder on trial " << trial;
    }
}

/**
 * Property: Allocations ≤2% do not require vesting
 * 
 * For any allocation A where A ≤ 2% of total supply,
 * the allocation should not have a vesting schedule.
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, AllocationsAtOrBelow2PercentNoVesting) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 2% threshold
        uint64_t threshold = static_cast<uint64_t>(total_supply * 0.02);
        
        // Generate allocation at or below 2%
        uint64_t amount = generate_random_amount(1, threshold);
        Address address = generate_random_address();
        
        bool result = builder.add_allocation(address, amount, "Test");
        
        ASSERT_TRUE(result)
            << "Allocation at or below 2% was rejected on trial " << trial;
        
        // Verify allocation does not have vesting schedule
        const auto& allocations = builder.get_allocations();
        ASSERT_FALSE(allocations.empty())
            << "No allocations found on trial " << trial;
        
        for (const auto& alloc : allocations) {
            if (alloc.address == address) {
                ASSERT_FALSE(alloc.vesting_schedule.has_value())
                    << "Allocation at or below 2% has vesting schedule on trial " << trial
                    << " (amount: " << amount << ", threshold: " << threshold << ")";
                break;
            }
        }
    }
}

/**
 * Property: Vesting schedules are 12 months
 * 
 * For any allocation with vesting, the vesting duration
 * must be 12 months (31,536,000 seconds).
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, VestingSchedulesAre12Months) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 2% threshold and 5% cap
        uint64_t threshold = static_cast<uint64_t>(total_supply * 0.02);
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Generate allocation above 2%
        uint64_t amount = generate_random_amount(threshold + 1, cap);
        Address address = generate_random_address();
        
        builder.add_allocation(address, amount, "Test");
        
        // Verify vesting duration is 12 months
        const auto& allocations = builder.get_allocations();
        for (const auto& alloc : allocations) {
            if (alloc.address == address && alloc.vesting_schedule.has_value()) {
                ASSERT_EQ(alloc.vesting_schedule->duration_seconds, TWELVE_MONTHS_SECONDS)
                    << "Vesting duration is not 12 months on trial " << trial
                    << " (duration: " << alloc.vesting_schedule->duration_seconds << ")";
                break;
            }
        }
    }
}

/**
 * Property: Vesting is linear over time
 * 
 * For any vesting schedule, the vested amount should increase
 * linearly from 0 at start to total_amount at end.
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, VestingIsLinear) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 2% threshold and 5% cap
        uint64_t threshold = static_cast<uint64_t>(total_supply * 0.02);
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Generate allocation above 2%
        uint64_t amount = generate_random_amount(threshold + 1, cap);
        Address address = generate_random_address();
        
        builder.add_allocation(address, amount, "Test");
        
        // Get vesting schedule
        const auto& allocations = builder.get_allocations();
        for (const auto& alloc : allocations) {
            if (alloc.address == address && alloc.vesting_schedule.has_value()) {
                const auto& vesting = alloc.vesting_schedule.value();
                
                // Test at various points in time
                std::vector<double> time_fractions = {0.0, 0.25, 0.5, 0.75, 1.0};
                
                for (double fraction : time_fractions) {
                    uint64_t elapsed = static_cast<uint64_t>(TWELVE_MONTHS_SECONDS * fraction);
                    uint64_t timestamp = genesis_timestamp + elapsed;
                    
                    uint64_t vested = vesting.calculate_vested_amount(timestamp);
                    uint64_t expected = static_cast<uint64_t>(amount * fraction);
                    
                    // Allow small rounding error (within 0.1%)
                    uint64_t tolerance = amount / 1000;
                    uint64_t diff = (vested > expected) ? (vested - expected) : (expected - vested);
                    
                    ASSERT_LE(diff, tolerance)
                        << "Vesting is not linear on trial " << trial
                        << " at fraction " << fraction
                        << " (expected: " << expected << ", actual: " << vested << ")";
                }
                
                break;
            }
        }
    }
}

/**
 * Property: No tokens vested before start time
 * 
 * For any vesting schedule, no tokens should be vested
 * before the genesis timestamp.
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, NoTokensVestedBeforeStart) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 2% threshold and 5% cap
        uint64_t threshold = static_cast<uint64_t>(total_supply * 0.02);
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Generate allocation above 2%
        uint64_t amount = generate_random_amount(threshold + 1, cap);
        Address address = generate_random_address();
        
        builder.add_allocation(address, amount, "Test");
        
        // Get vesting schedule
        const auto& allocations = builder.get_allocations();
        for (const auto& alloc : allocations) {
            if (alloc.address == address && alloc.vesting_schedule.has_value()) {
                const auto& vesting = alloc.vesting_schedule.value();
                
                // Test before genesis
                uint64_t before_genesis = genesis_timestamp - generate_random_amount(1, 1000000);
                uint64_t vested = vesting.calculate_vested_amount(before_genesis);
                
                ASSERT_EQ(vested, 0)
                    << "Tokens vested before genesis on trial " << trial;
                
                break;
            }
        }
    }
}

/**
 * Property: All tokens vested after 12 months
 * 
 * For any vesting schedule, all tokens should be fully vested
 * after 12 months from genesis.
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, AllTokensVestedAfter12Months) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 2% threshold and 5% cap
        uint64_t threshold = static_cast<uint64_t>(total_supply * 0.02);
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Generate allocation above 2%
        uint64_t amount = generate_random_amount(threshold + 1, cap);
        Address address = generate_random_address();
        
        builder.add_allocation(address, amount, "Test");
        
        // Get vesting schedule
        const auto& allocations = builder.get_allocations();
        for (const auto& alloc : allocations) {
            if (alloc.address == address && alloc.vesting_schedule.has_value()) {
                const auto& vesting = alloc.vesting_schedule.value();
                
                // Test at and after 12 months
                uint64_t after_12_months = genesis_timestamp + TWELVE_MONTHS_SECONDS;
                uint64_t vested = vesting.calculate_vested_amount(after_12_months);
                
                ASSERT_EQ(vested, amount)
                    << "Not all tokens vested after 12 months on trial " << trial
                    << " (expected: " << amount << ", actual: " << vested << ")";
                
                // Test well after 12 months
                uint64_t much_later = after_12_months + generate_random_amount(1, 10000000);
                uint64_t vested_later = vesting.calculate_vested_amount(much_later);
                
                ASSERT_EQ(vested_later, amount)
                    << "Vested amount changed after full vesting on trial " << trial;
                
                break;
            }
        }
    }
}

/**
 * Property: Exactly 2% allocation does not require vesting
 * 
 * An allocation of exactly 2% should not have vesting.
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, Exactly2PercentNoVesting) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate exactly 2%
        uint64_t exact_2_percent = static_cast<uint64_t>(total_supply * 0.02);
        Address address = generate_random_address();
        
        builder.add_allocation(address, exact_2_percent, "Test");
        
        // Verify no vesting schedule
        const auto& allocations = builder.get_allocations();
        for (const auto& alloc : allocations) {
            if (alloc.address == address) {
                ASSERT_FALSE(alloc.vesting_schedule.has_value())
                    << "Exactly 2% allocation has vesting on trial " << trial;
                break;
            }
        }
    }
}

/**
 * Property: Vesting threshold scales with total supply
 * 
 * The 2% vesting threshold should scale proportionally
 * with different total supply values.
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, VestingThresholdScalesWithSupply) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate expected threshold
        uint64_t expected_threshold = static_cast<uint64_t>(total_supply * 0.02);
        
        // Verify builder's threshold calculation
        ASSERT_TRUE(builder.requires_vesting(expected_threshold + 1))
            << "Threshold calculation incorrect on trial " << trial;
        
        ASSERT_FALSE(builder.requires_vesting(expected_threshold))
            << "Threshold calculation incorrect on trial " << trial;
    }
}

/**
 * Property: Multiple large allocations all have vesting
 * 
 * If multiple allocations exceed 2%, each should have
 * its own vesting schedule.
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, MultipleLargeAllocationsHaveVesting) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 2% threshold and 5% cap
        uint64_t threshold = static_cast<uint64_t>(total_supply * 0.02);
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Add multiple large allocations
        std::uniform_int_distribution<int> count_dist(2, 5);
        int num_allocations = count_dist(rng_);
        
        std::vector<Address> addresses;
        for (int i = 0; i < num_allocations; ++i) {
            uint64_t amount = generate_random_amount(threshold + 1, cap);
            Address address = generate_random_address();
            addresses.push_back(address);
            
            bool result = builder.add_allocation(address, amount, "Test");
            if (!result) {
                break;  // Exceeded total supply
            }
        }
        
        // Verify all added allocations have vesting
        const auto& allocations = builder.get_allocations();
        for (const auto& addr : addresses) {
            bool found = false;
            for (const auto& alloc : allocations) {
                if (alloc.address == addr) {
                    ASSERT_TRUE(alloc.vesting_schedule.has_value())
                        << "Large allocation missing vesting on trial " << trial;
                    found = true;
                    break;
                }
            }
            if (!found) {
                break;  // Allocation wasn't added (exceeded supply)
            }
        }
    }
}

/**
 * Property: Vesting start time matches genesis timestamp
 * 
 * For any vesting schedule, the start time should be
 * the genesis timestamp.
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, VestingStartsAtGenesis) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 2% threshold and 5% cap
        uint64_t threshold = static_cast<uint64_t>(total_supply * 0.02);
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Generate allocation above 2%
        uint64_t amount = generate_random_amount(threshold + 1, cap);
        Address address = generate_random_address();
        
        builder.add_allocation(address, amount, "Test");
        
        // Verify vesting start time
        const auto& allocations = builder.get_allocations();
        for (const auto& alloc : allocations) {
            if (alloc.address == address && alloc.vesting_schedule.has_value()) {
                ASSERT_EQ(alloc.vesting_schedule->start_timestamp, genesis_timestamp)
                    << "Vesting start time doesn't match genesis on trial " << trial;
                break;
            }
        }
    }
}

/**
 * Property: is_fully_vested returns correct values
 * 
 * The is_fully_vested method should return false before 12 months
 * and true at or after 12 months.
 */
TEST_F(GenesisLargeAllocationVestingPropertyTest, IsFullyVestedCorrect) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        uint64_t genesis_timestamp = generate_random_timestamp();
        
        GenesisConfig config(
            total_supply,
            genesis_timestamp,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 2% threshold and 5% cap
        uint64_t threshold = static_cast<uint64_t>(total_supply * 0.02);
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Generate allocation above 2%
        uint64_t amount = generate_random_amount(threshold + 1, cap);
        Address address = generate_random_address();
        
        builder.add_allocation(address, amount, "Test");
        
        // Get vesting schedule
        const auto& allocations = builder.get_allocations();
        for (const auto& alloc : allocations) {
            if (alloc.address == address && alloc.vesting_schedule.has_value()) {
                const auto& vesting = alloc.vesting_schedule.value();
                
                // Test before 12 months
                uint64_t before = genesis_timestamp + generate_random_amount(0, TWELVE_MONTHS_SECONDS - 1);
                ASSERT_FALSE(vesting.is_fully_vested(before))
                    << "is_fully_vested returned true before 12 months on trial " << trial;
                
                // Test at 12 months
                uint64_t at_12_months = genesis_timestamp + TWELVE_MONTHS_SECONDS;
                ASSERT_TRUE(vesting.is_fully_vested(at_12_months))
                    << "is_fully_vested returned false at 12 months on trial " << trial;
                
                // Test after 12 months
                uint64_t after = at_12_months + generate_random_amount(1, 10000000);
                ASSERT_TRUE(vesting.is_fully_vested(after))
                    << "is_fully_vested returned false after 12 months on trial " << trial;
                
                break;
            }
        }
    }
}

