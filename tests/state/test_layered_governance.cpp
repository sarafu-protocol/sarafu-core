#include <gtest/gtest.h>
#include "sarafu/state/layered_governance.h"
#include <cmath>

using namespace sarafu::state;
using namespace sarafu::consensus;

class LayeredGovernanceTest : public ::testing::Test {
protected:
    LayeredGovernance governance;
    
    LayeredGovernanceTest() : governance(43200) {}  // 43200 blocks per day
    
    ValidatorID create_validator_id(uint8_t id) {
        Address::AddressArray data = {};
        data[0] = id;
        return Address(data);
    }
};

TEST_F(LayeredGovernanceTest, SubmitSecurityProposal) {
    std::vector<uint8_t> value = {0x01, 0x02, 0x03};
    Address proposer = create_validator_id(1);
    
    auto proposal_id = governance.submit_proposal(
        ProposalType::SlashingParameters,
        value,
        "Update slashing parameters",
        proposer,
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    auto proposal = governance.get_proposal(*proposal_id);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->layer, GovernanceLayer::Security);
    EXPECT_EQ(proposal->status, LayeredProposalStatus::Active);
}

TEST_F(LayeredGovernanceTest, SubmitTreasuryProposal) {
    std::vector<uint8_t> value = {0x01, 0x02, 0x03};
    Address proposer = create_validator_id(1);
    
    auto proposal_id = governance.submit_proposal(
        ProposalType::TreasuryAllocation,
        value,
        "Treasury allocation",
        proposer,
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    auto proposal = governance.get_proposal(*proposal_id);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->layer, GovernanceLayer::Treasury);
}

TEST_F(LayeredGovernanceTest, SubmitConstitutionalProposal) {
    std::vector<uint8_t> value = {0x01, 0x02, 0x03};
    Address proposer = create_validator_id(1);
    
    auto proposal_id = governance.submit_proposal(
        ProposalType::InflationBand,
        value,
        "Update inflation band",
        proposer,
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    auto proposal = governance.get_proposal(*proposal_id);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->layer, GovernanceLayer::Constitutional);
    EXPECT_GT(proposal->timelock_ends_at_height, proposal->voting_ends_at_height);
}

TEST_F(LayeredGovernanceTest, Layer1LinearVoting) {
    // Submit security proposal
    auto proposal_id = governance.submit_proposal(
        ProposalType::ValidatorSetSize,
        {0x64},  // 100 validators
        "Increase validator set size",
        create_validator_id(1),
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    // Calculate voting power (linear for Layer 1)
    ValidatorID voter1 = create_validator_id(2);
    uint64_t stake1 = 100000;
    VotingPower power1 = governance.calculate_voting_power(voter1, stake1, 1000000, true);
    
    EXPECT_EQ(power1.security_power, stake1);  // Linear: 1 SAR = 1 vote
    
    // Cast vote
    bool voted = governance.vote(*proposal_id, voter1, true, power1, 1500);
    EXPECT_TRUE(voted);
    
    // Check vote recorded
    auto proposal = governance.get_proposal(*proposal_id);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->votes_for, stake1);
}

TEST_F(LayeredGovernanceTest, Layer2QuadraticVoting) {
    // Submit treasury proposal
    auto proposal_id = governance.submit_proposal(
        ProposalType::GrantProposal,
        {0x01},
        "Grant proposal",
        create_validator_id(1),
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    // Calculate voting power (quadratic for Layer 2)
    ValidatorID voter1 = create_validator_id(2);
    uint64_t stake1 = 10000;  // sqrt(10000) = 100
    uint64_t total_weight = 1000000;
    
    VotingPower power1 = governance.calculate_voting_power(voter1, stake1, total_weight, true);
    
    // Should be sqrt(stake) = 100
    EXPECT_EQ(power1.treasury_power, 100);
    
    // Cast vote
    bool voted = governance.vote(*proposal_id, voter1, true, power1, 1500);
    EXPECT_TRUE(voted);
    
    auto proposal = governance.get_proposal(*proposal_id);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->votes_for, 100);
}

TEST_F(LayeredGovernanceTest, Layer2VotingPowerCap) {
    // Test 2% cap on treasury voting power
    ValidatorID voter = create_validator_id(1);
    uint64_t large_stake = 1000000000;  // Very large stake
    uint64_t total_weight = 1000000;
    
    VotingPower power = governance.calculate_voting_power(voter, large_stake, total_weight, true);
    
    // Should be capped at 2% of total weight
    uint64_t expected_cap = static_cast<uint64_t>(total_weight * 0.02);
    EXPECT_EQ(power.treasury_power, expected_cap);
}

TEST_F(LayeredGovernanceTest, Layer3DualApproval) {
    // Submit constitutional proposal
    auto proposal_id = governance.submit_proposal(
        ProposalType::IssuanceRules,
        {0x01},
        "Update issuance rules",
        create_validator_id(1),
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    // Validator votes
    for (uint8_t i = 1; i <= 10; ++i) {
        ValidatorID voter = create_validator_id(i);
        VotingPower power = governance.calculate_voting_power(voter, 100000, 1000000, true);
        governance.vote(*proposal_id, voter, true, power, 1500);
    }
    
    // Finalize voting
    bool finalized = governance.finalize_voting(*proposal_id, 1000000, 1000000, 1000 + 7 * 43200);
    EXPECT_TRUE(finalized);
    
    auto proposal = governance.get_proposal(*proposal_id);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->status, LayeredProposalStatus::InTimelock);
}

TEST_F(LayeredGovernanceTest, Layer3TimelockEnforcement) {
    // Submit constitutional proposal
    auto proposal_id = governance.submit_proposal(
        ProposalType::TreasuryCaps,
        {0x01},
        "Update treasury caps",
        create_validator_id(1),
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    // Vote and finalize
    for (uint8_t i = 1; i <= 10; ++i) {
        ValidatorID voter = create_validator_id(i);
        VotingPower power = governance.calculate_voting_power(voter, 100000, 1000000, true);
        governance.vote(*proposal_id, voter, true, power, 1500);
    }
    
    uint64_t voting_end = 1000 + 7 * 43200;
    governance.finalize_voting(*proposal_id, 1000000, 1000000, voting_end);
    
    // Try to execute before timelock expires
    bool executed_early = governance.execute_proposal(*proposal_id, voting_end + 1);
    EXPECT_FALSE(executed_early);
    
    // Execute after timelock (21 days)
    uint64_t timelock_end = voting_end + 21 * 43200;
    bool executed = governance.execute_proposal(*proposal_id, timelock_end);
    EXPECT_TRUE(executed);
    
    auto proposal = governance.get_proposal(*proposal_id);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->status, LayeredProposalStatus::Executed);
}

TEST_F(LayeredGovernanceTest, StakeLockingDuringVoting) {
    auto proposal_id = governance.submit_proposal(
        ProposalType::SlashingParameters,
        {0x01},
        "Test proposal",
        create_validator_id(1),
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    ValidatorID voter = create_validator_id(2);
    VotingPower power = governance.calculate_voting_power(voter, 100000, 1000000, true);
    
    // Stake should not be locked initially
    EXPECT_FALSE(governance.is_stake_locked(voter));
    
    // Vote
    governance.vote(*proposal_id, voter, true, power, 1500);
    
    // Stake should be locked after voting
    EXPECT_TRUE(governance.is_stake_locked(voter));
    
    // Finalize to unlock
    governance.finalize_voting(*proposal_id, 1000000, 1000000, 1000 + 7 * 43200);
    
    // Stake should be unlocked after finalization
    EXPECT_FALSE(governance.is_stake_locked(voter));
}

TEST_F(LayeredGovernanceTest, PreventDoubleVoting) {
    auto proposal_id = governance.submit_proposal(
        ProposalType::BlockTime,
        {0x01},
        "Test proposal",
        create_validator_id(1),
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    ValidatorID voter = create_validator_id(2);
    VotingPower power = governance.calculate_voting_power(voter, 100000, 1000000, true);
    
    // First vote should succeed
    bool vote1 = governance.vote(*proposal_id, voter, true, power, 1500);
    EXPECT_TRUE(vote1);
    
    // Second vote should fail
    bool vote2 = governance.vote(*proposal_id, voter, false, power, 1600);
    EXPECT_FALSE(vote2);
}

TEST_F(LayeredGovernanceTest, VotingPeriodExpiration) {
    auto proposal_id = governance.submit_proposal(
        ProposalType::ValidatorSetSize,
        {0x01},
        "Test proposal",
        create_validator_id(1),
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    ValidatorID voter = create_validator_id(2);
    VotingPower power = governance.calculate_voting_power(voter, 100000, 1000000, true);
    
    // Vote after voting period ends
    uint64_t after_voting = 1000 + 8 * 43200;  // 8 days (voting period is 7 days)
    bool voted = governance.vote(*proposal_id, voter, true, power, after_voting);
    
    EXPECT_FALSE(voted);
}

TEST_F(LayeredGovernanceTest, Layer1MajorityThreshold) {
    auto proposal_id = governance.submit_proposal(
        ProposalType::SlashingParameters,
        {0x01},
        "Test proposal",
        create_validator_id(1),
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    // 6 votes for, 4 votes against (60% approval)
    for (uint8_t i = 1; i <= 6; ++i) {
        ValidatorID voter = create_validator_id(i);
        VotingPower power = governance.calculate_voting_power(voter, 100000, 1000000, true);
        governance.vote(*proposal_id, voter, true, power, 1500);
    }
    
    for (uint8_t i = 7; i <= 10; ++i) {
        ValidatorID voter = create_validator_id(i);
        VotingPower power = governance.calculate_voting_power(voter, 100000, 1000000, true);
        governance.vote(*proposal_id, voter, false, power, 1500);
    }
    
    // Finalize
    governance.finalize_voting(*proposal_id, 1000000, 1000000, 1000 + 7 * 43200);
    
    auto proposal = governance.get_proposal(*proposal_id);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->status, LayeredProposalStatus::Approved);  // >50% threshold met
}

TEST_F(LayeredGovernanceTest, Layer2SupermajorityThreshold) {
    auto proposal_id = governance.submit_proposal(
        ProposalType::TreasuryAllocation,
        {0x01},
        "Test proposal",
        create_validator_id(1),
        1000
    );
    
    ASSERT_TRUE(proposal_id.has_value());
    
    // 7 votes for, 3 votes against (70% approval)
    for (uint8_t i = 1; i <= 7; ++i) {
        ValidatorID voter = create_validator_id(i);
        VotingPower power = governance.calculate_voting_power(voter, 100000, 1000000, true);
        governance.vote(*proposal_id, voter, true, power, 1500);
    }
    
    for (uint8_t i = 8; i <= 10; ++i) {
        ValidatorID voter = create_validator_id(i);
        VotingPower power = governance.calculate_voting_power(voter, 100000, 1000000, true);
        governance.vote(*proposal_id, voter, false, power, 1500);
    }
    
    // Finalize
    governance.finalize_voting(*proposal_id, 1000000, 1000000, 1000 + 7 * 43200);
    
    auto proposal = governance.get_proposal(*proposal_id);
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->status, LayeredProposalStatus::Approved);  // ≥2/3 threshold met
}
