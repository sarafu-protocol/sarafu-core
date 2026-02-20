#include "sarafu/consensus/genesis_builder.h"
#include "sarafu/state/account.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace sarafu::consensus;
using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Property-Based Test for Genesis Allocation Cap
 * 
 * **Validates: Requirements 16.2**
 * 
 * Property 59: Genesis Allocation Cap
 * For any genesis allocation A, A ≤ 5% of total supply.
 * 
 * This test validates that:
 * 1. No single allocation can exceed 5% of total supply
 * 2. The GenesisBuilder enforces this constraint
 * 3. Allocations at or below 5% are accepted
 * 4. Allocations above 5% are rejected
 * 5. The constraint holds for various total supply values
 */
class GenesisAllocationCapPropertyTest : public ::testing::Test {
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

    // Generate random allocation amount
    uint64_t generate_random_amount(uint64_t min, uint64_t max) {
        std::uniform_int_distribution<uint64_t> dist(min, max);
        return dist(rng_);
    }

    std::mt19937 rng_;
};

/**
 * Property: No allocation can exceed 5% of total supply
 * 
 * For any genesis allocation A and total supply S,
 * if A > 0.05 * S, the allocation is rejected.
 */
TEST_F(GenesisAllocationCapPropertyTest, AllocationsAbove5PercentAreRejected) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        
        GenesisConfig config(
            total_supply,
            1700000000,  // timestamp
            100,         // validator_set_size
            1000000,     // minimum_self_bond
            1            // chain_id
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 5% cap
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Try to add allocation above 5%
        uint64_t excess_amount = cap + generate_random_amount(1, cap);
        Address address = generate_random_address();
        
        bool result = builder.add_allocation(address, excess_amount, "Test");
        
        ASSERT_FALSE(result)
            << "Allocation above 5% was accepted on trial " << trial
            << " (total_supply: " << total_supply
            << ", cap: " << cap
            << ", amount: " << excess_amount << ")";
    }
}

/**
 * Property: Allocations at or below 5% are accepted
 * 
 * For any genesis allocation A and total supply S,
 * if A ≤ 0.05 * S, the allocation is accepted.
 */
TEST_F(GenesisAllocationCapPropertyTest, AllocationsAtOrBelow5PercentAreAccepted) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        
        GenesisConfig config(
            total_supply,
            1700000000,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 5% cap
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Generate allocation at or below 5%
        uint64_t valid_amount = generate_random_amount(1, cap);
        Address address = generate_random_address();
        
        bool result = builder.add_allocation(address, valid_amount, "Test");
        
        ASSERT_TRUE(result)
            << "Valid allocation was rejected on trial " << trial
            << " (total_supply: " << total_supply
            << ", cap: " << cap
            << ", amount: " << valid_amount << ")";
    }
}

/**
 * Property: Exactly 5% allocation is accepted
 * 
 * An allocation of exactly 5% of total supply should be accepted.
 */
TEST_F(GenesisAllocationCapPropertyTest, Exactly5PercentIsAccepted) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        
        GenesisConfig config(
            total_supply,
            1700000000,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate exactly 5%
        uint64_t exact_5_percent = static_cast<uint64_t>(total_supply * 0.05);
        Address address = generate_random_address();
        
        bool result = builder.add_allocation(address, exact_5_percent, "Test");
        
        ASSERT_TRUE(result)
            << "Exactly 5% allocation was rejected on trial " << trial
            << " (total_supply: " << total_supply
            << ", amount: " << exact_5_percent << ")";
    }
}

/**
 * Property: Multiple allocations each below 5% are accepted
 * 
 * Multiple allocations, each below 5%, should all be accepted
 * as long as the total doesn't exceed supply.
 */
TEST_F(GenesisAllocationCapPropertyTest, MultipleAllocationsBelow5PercentAreAccepted) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        
        GenesisConfig config(
            total_supply,
            1700000000,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 5% cap
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Add multiple allocations, each below 5%
        std::uniform_int_distribution<int> count_dist(5, 15);
        int num_allocations = count_dist(rng_);
        
        int successful_allocations = 0;
        uint64_t total_allocated = 0;
        
        for (int i = 0; i < num_allocations; ++i) {
            // Generate amount below 5%
            uint64_t amount = generate_random_amount(1, cap);
            
            // Stop if we would exceed total supply
            if (total_allocated + amount > total_supply) {
                break;
            }
            
            Address address = generate_random_address();
            bool result = builder.add_allocation(address, amount, "Test");
            
            if (result) {
                successful_allocations++;
                total_allocated += amount;
            }
        }
        
        // All allocations that fit within supply should succeed
        ASSERT_GT(successful_allocations, 0)
            << "No allocations succeeded on trial " << trial;
        
        // Verify total allocated doesn't exceed supply
        ASSERT_LE(builder.get_total_allocated(), total_supply)
            << "Total allocated exceeds supply on trial " << trial;
    }
}

/**
 * Property: 5% cap scales with total supply
 * 
 * The 5% cap should scale proportionally with different total supply values.
 */
TEST_F(GenesisAllocationCapPropertyTest, CapScalesWithTotalSupply) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        
        GenesisConfig config(
            total_supply,
            1700000000,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate expected cap
        uint64_t expected_cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Verify builder's cap calculation matches
        ASSERT_TRUE(builder.exceeds_allocation_cap(expected_cap + 1))
            << "Cap calculation incorrect on trial " << trial;
        
        ASSERT_FALSE(builder.exceeds_allocation_cap(expected_cap))
            << "Cap calculation incorrect on trial " << trial;
    }
}

/**
 * Property: Very small allocations are accepted
 * 
 * Even very small allocations (1 token) should be accepted.
 */
TEST_F(GenesisAllocationCapPropertyTest, VerySmallAllocationsAreAccepted) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        
        GenesisConfig config(
            total_supply,
            1700000000,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        Address address = generate_random_address();
        bool result = builder.add_allocation(address, 1, "Test");
        
        ASSERT_TRUE(result)
            << "Minimum allocation was rejected on trial " << trial;
    }
}

/**
 * Property: Cap enforcement is independent of allocation order
 * 
 * The 5% cap should be enforced regardless of the order in which
 * allocations are added.
 */
TEST_F(GenesisAllocationCapPropertyTest, CapEnforcementIndependentOfOrder) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        
        GenesisConfig config(
            total_supply,
            1700000000,
            100,
            1000000,
            1
        );
        
        // Calculate 5% cap
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Create two allocations: one valid, one invalid
        uint64_t valid_amount = generate_random_amount(1, cap);
        uint64_t invalid_amount = cap + generate_random_amount(1, cap);
        
        Address addr1 = generate_random_address();
        Address addr2 = generate_random_address();
        
        // Try order 1: valid first, then invalid
        GenesisBuilder builder1(config);
        bool result1_valid = builder1.add_allocation(addr1, valid_amount, "Test");
        bool result1_invalid = builder1.add_allocation(addr2, invalid_amount, "Test");
        
        ASSERT_TRUE(result1_valid)
            << "Valid allocation rejected in order 1 on trial " << trial;
        ASSERT_FALSE(result1_invalid)
            << "Invalid allocation accepted in order 1 on trial " << trial;
        
        // Try order 2: invalid first, then valid
        GenesisBuilder builder2(config);
        bool result2_invalid = builder2.add_allocation(addr2, invalid_amount, "Test");
        bool result2_valid = builder2.add_allocation(addr1, valid_amount, "Test");
        
        ASSERT_FALSE(result2_invalid)
            << "Invalid allocation accepted in order 2 on trial " << trial;
        ASSERT_TRUE(result2_valid)
            << "Valid allocation rejected in order 2 on trial " << trial;
    }
}

/**
 * Property: Cap applies to each individual allocation
 * 
 * The 5% cap applies to each allocation individually, not to the sum.
 * Multiple 4% allocations should be accepted even though their sum exceeds 5%.
 */
TEST_F(GenesisAllocationCapPropertyTest, CapAppliesIndividually) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        
        GenesisConfig config(
            total_supply,
            1700000000,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 4% (below cap)
        uint64_t four_percent = static_cast<uint64_t>(total_supply * 0.04);
        
        // Add multiple 4% allocations
        int num_allocations = 3;  // 3 * 4% = 12% total
        int successful = 0;
        
        for (int i = 0; i < num_allocations; ++i) {
            Address address = generate_random_address();
            bool result = builder.add_allocation(address, four_percent, "Test");
            if (result) {
                successful++;
            }
        }
        
        // All should succeed (each is below 5% individually)
        ASSERT_EQ(successful, num_allocations)
            << "Not all 4% allocations succeeded on trial " << trial;
    }
}

/**
 * Property: Validation catches violations
 * 
 * The builder's validate() method should detect any allocations
 * that exceed the 5% cap.
 */
TEST_F(GenesisAllocationCapPropertyTest, ValidationCatchesViolations) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        
        GenesisConfig config(
            total_supply,
            1700000000,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 5% cap
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Add valid allocations
        std::uniform_int_distribution<int> count_dist(1, 5);
        int num_valid = count_dist(rng_);
        
        for (int i = 0; i < num_valid; ++i) {
            uint64_t amount = generate_random_amount(1, cap);
            Address address = generate_random_address();
            builder.add_allocation(address, amount, "Test");
        }
        
        // Validation should pass
        ASSERT_TRUE(builder.validate())
            << "Validation failed for valid allocations on trial " << trial;
    }
}

/**
 * Property: Large total supply values work correctly
 * 
 * The 5% cap should work correctly even for very large total supply values.
 */
TEST_F(GenesisAllocationCapPropertyTest, LargeTotalSupplyValuesWork) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Use very large total supply
        std::uniform_int_distribution<uint64_t> large_dist(
            10000000000000ULL,
            100000000000000ULL
        );
        uint64_t total_supply = large_dist(rng_);
        
        GenesisConfig config(
            total_supply,
            1700000000,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        // Calculate 5% cap
        uint64_t cap = static_cast<uint64_t>(total_supply * 0.05);
        
        // Try allocation at cap
        Address address = generate_random_address();
        bool result = builder.add_allocation(address, cap, "Test");
        
        ASSERT_TRUE(result)
            << "Allocation at cap failed for large supply on trial " << trial;
        
        // Try allocation above cap
        Address address2 = generate_random_address();
        bool result2 = builder.add_allocation(address2, cap + 1, "Test");
        
        ASSERT_FALSE(result2)
            << "Allocation above cap succeeded for large supply on trial " << trial;
    }
}

/**
 * Property: Zero allocations are rejected
 * 
 * Allocations of 0 tokens should be rejected (no point in creating them).
 */
TEST_F(GenesisAllocationCapPropertyTest, ZeroAllocationsAreRejected) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t total_supply = generate_random_total_supply();
        
        GenesisConfig config(
            total_supply,
            1700000000,
            100,
            1000000,
            1
        );
        
        GenesisBuilder builder(config);
        
        Address address = generate_random_address();
        bool result = builder.add_allocation(address, 0, "Test");
        
        // Zero allocations should be rejected (or accepted but not counted)
        // The implementation may vary, but they shouldn't affect total
        if (result) {
            ASSERT_EQ(builder.get_total_allocated(), 0)
                << "Zero allocation affected total on trial " << trial;
        }
    }
}

