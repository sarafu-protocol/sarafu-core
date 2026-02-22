#include <gtest/gtest.h>
#include "sarafu/state/staking_controls.h"

using namespace sarafu::state;
using namespace sarafu::consensus;

class StakingControlsTest : public ::testing::Test {
protected:
    StakingControls controls;
    
    ValidatorID create_validator_id(uint8_t id) {
        Address::AddressArray data = {};
        data[0] = id;
        return Address(data);
    }
};

TEST_F(StakingControlsTest, InitiateUnbonding) {
    ValidatorID validator = create_validator_id(1);
    uint64_t amount = 100000;
    uint64_t current_height = 1000;
    
    bool success = controls.initiate_unbonding(validator, amount, current_height);
    EXPECT_TRUE(success);
    
    // Check unbonding entry created
    auto entries = controls.get_unbonding_entries(validator);
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].amount, amount);
    EXPECT_EQ(entries[0].completion_height, current_height + 1209600);  // 28 days
}

TEST_F(StakingControlsTest, UnbondingPeriod28Days) {
    ValidatorID validator = create_validator_id(1);
    uint64_t amount = 100000;
    uint64_t current_height = 1000;
    
    controls.initiate_unbonding(validator, amount, current_height);
    
    // Check unbonding period is 28 days (1209600 blocks)
    EXPECT_EQ(controls.get_unbonding_period(), 1209600);
    
    auto entries = controls.get_unbonding_entries(validator);
    ASSERT_EQ(entries.size(), 1);
    EXPECT_EQ(entries[0].completion_height, current_height + 1209600);
}

TEST_F(StakingControlsTest, ProcessCompletedUnbonding) {
    ValidatorID validator = create_validator_id(1);
    uint64_t amount = 100000;
    uint64_t start_height = 1000;
    
    controls.initiate_unbonding(validator, amount, start_height);
    
    // Process before completion - should return empty
    auto completed1 = controls.process_completed_unbonding(start_height + 1000000);
    EXPECT_EQ(completed1.size(), 0);
    
    // Process after completion - should return entry
    auto completed2 = controls.process_completed_unbonding(start_height + 1209600);
    ASSERT_EQ(completed2.size(), 1);
    EXPECT_EQ(completed2[0].validator_id, validator);
    EXPECT_EQ(completed2[0].amount, amount);
    
    // Entry should be removed from queue
    auto remaining = controls.get_unbonding_entries(validator);
    EXPECT_EQ(remaining.size(), 0);
}

TEST_F(StakingControlsTest, MultipleUnbondingEntries) {
    ValidatorID validator = create_validator_id(1);
    uint64_t current_height = 1000;
    
    // Create multiple unbonding entries
    controls.initiate_unbonding(validator, 50000, current_height);
    controls.initiate_unbonding(validator, 30000, current_height + 100);
    controls.initiate_unbonding(validator, 20000, current_height + 200);
    
    auto entries = controls.get_unbonding_entries(validator);
    EXPECT_EQ(entries.size(), 3);
    
    uint64_t total = controls.get_unbonding_amount(validator);
    EXPECT_EQ(total, 100000);
}

TEST_F(StakingControlsTest, GetUnbondingAmount) {
    ValidatorID validator = create_validator_id(1);
    uint64_t current_height = 1000;
    
    controls.initiate_unbonding(validator, 50000, current_height);
    controls.initiate_unbonding(validator, 30000, current_height + 100);
    
    uint64_t total = controls.get_unbonding_amount(validator);
    EXPECT_EQ(total, 80000);
}

TEST_F(StakingControlsTest, VotingEligibleStake) {
    ValidatorID validator = create_validator_id(1);
    uint64_t bonded_stake = 200000;
    uint64_t current_height = 1000;
    
    // Initiate unbonding
    controls.initiate_unbonding(validator, 50000, current_height);
    
    // Only bonded stake is eligible for voting
    uint64_t eligible = controls.get_voting_eligible_stake(validator, bonded_stake);
    EXPECT_EQ(eligible, bonded_stake);  // Unbonding stake not included
}

TEST_F(StakingControlsTest, SlashUnbondingStake) {
    ValidatorID validator = create_validator_id(1);
    uint64_t current_height = 1000;
    
    // Create unbonding entries
    controls.initiate_unbonding(validator, 50000, current_height);
    controls.initiate_unbonding(validator, 30000, current_height + 100);
    
    // Slash 40000 from unbonding stake
    uint64_t slashed = controls.slash_unbonding_stake(validator, 40000);
    EXPECT_EQ(slashed, 40000);
    
    // Check remaining unbonding amount
    uint64_t remaining = controls.get_unbonding_amount(validator);
    EXPECT_EQ(remaining, 40000);  // 80000 - 40000
}

TEST_F(StakingControlsTest, SlashExceedsUnbondingAmount) {
    ValidatorID validator = create_validator_id(1);
    uint64_t current_height = 1000;
    
    controls.initiate_unbonding(validator, 50000, current_height);
    
    // Try to slash more than available
    uint64_t slashed = controls.slash_unbonding_stake(validator, 100000);
    EXPECT_EQ(slashed, 50000);  // Can only slash what's available
    
    uint64_t remaining = controls.get_unbonding_amount(validator);
    EXPECT_EQ(remaining, 0);
}

TEST_F(StakingControlsTest, SlashMultipleEntries) {
    ValidatorID validator = create_validator_id(1);
    uint64_t current_height = 1000;
    
    controls.initiate_unbonding(validator, 30000, current_height);
    controls.initiate_unbonding(validator, 20000, current_height + 100);
    controls.initiate_unbonding(validator, 10000, current_height + 200);
    
    // Slash 35000 (should slash first entry completely and 5000 from second)
    uint64_t slashed = controls.slash_unbonding_stake(validator, 35000);
    EXPECT_EQ(slashed, 35000);
    
    uint64_t remaining = controls.get_unbonding_amount(validator);
    EXPECT_EQ(remaining, 25000);  // 60000 - 35000
}

TEST_F(StakingControlsTest, UnbondingQueueOrdering) {
    ValidatorID validator1 = create_validator_id(1);
    ValidatorID validator2 = create_validator_id(2);
    uint64_t current_height = 1000;
    
    controls.initiate_unbonding(validator1, 50000, current_height);
    controls.initiate_unbonding(validator2, 30000, current_height + 100);
    controls.initiate_unbonding(validator1, 20000, current_height + 200);
    
    // Process at height where only first entry completes
    auto completed = controls.process_completed_unbonding(current_height + 1209600);
    ASSERT_EQ(completed.size(), 1);
    EXPECT_EQ(completed[0].validator_id, validator1);
    EXPECT_EQ(completed[0].amount, 50000);
    
    // Other entries should remain
    auto all_entries = controls.get_all_unbonding_entries();
    EXPECT_EQ(all_entries.size(), 2);
}

TEST_F(StakingControlsTest, ZeroAmountUnbonding) {
    ValidatorID validator = create_validator_id(1);
    uint64_t current_height = 1000;
    
    bool success = controls.initiate_unbonding(validator, 0, current_height);
    EXPECT_FALSE(success);
    
    auto entries = controls.get_unbonding_entries(validator);
    EXPECT_EQ(entries.size(), 0);
}

TEST_F(StakingControlsTest, UnbondingRemainsSlashable) {
    // Test that unbonding stake can be slashed during the 28-day period
    ValidatorID validator = create_validator_id(1);
    uint64_t current_height = 1000;
    
    controls.initiate_unbonding(validator, 100000, current_height);
    
    // Slash during unbonding period
    uint64_t mid_period = current_height + 600000;  // Halfway through
    uint64_t slashed = controls.slash_unbonding_stake(validator, 30000);
    EXPECT_EQ(slashed, 30000);
    
    // Process completion
    auto completed = controls.process_completed_unbonding(current_height + 1209600);
    ASSERT_EQ(completed.size(), 1);
    EXPECT_EQ(completed[0].amount, 70000);  // 100000 - 30000 slashed
}

TEST_F(StakingControlsTest, MultipleValidatorsIndependentUnbonding) {
    ValidatorID validator1 = create_validator_id(1);
    ValidatorID validator2 = create_validator_id(2);
    uint64_t current_height = 1000;
    
    controls.initiate_unbonding(validator1, 50000, current_height);
    controls.initiate_unbonding(validator2, 30000, current_height);
    
    // Check each validator's unbonding independently
    EXPECT_EQ(controls.get_unbonding_amount(validator1), 50000);
    EXPECT_EQ(controls.get_unbonding_amount(validator2), 30000);
    
    // Slash one validator
    controls.slash_unbonding_stake(validator1, 20000);
    
    // Other validator unaffected
    EXPECT_EQ(controls.get_unbonding_amount(validator1), 30000);
    EXPECT_EQ(controls.get_unbonding_amount(validator2), 30000);
}
