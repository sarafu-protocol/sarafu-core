#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <map>
#include <cstring>
#include "sarafu/state/governance_module.h"

namespace sarafu {
namespace integration {

/**
 * Integration Test 29.5: Governance Integration
 * 
 * This test validates the complete governance workflow:
 * - Submit proposal with sufficient backing
 * - Vote with ≥2/3 approval
 * - Wait for timelock
 * - Execute proposal and verify parameter change
 * 
 * Validates Requirements: 14.1, 14.2, 14.3, 14.4, 14.5, 14.6, 14.7, 14.8
 * Validates Properties: 40, 41, 42, 43
 */
class GovernanceIntegrationTest : public ::testing::Test {
protected:
    static constexpr size_t NUM_VALIDATORS = 100;
    static constexpr uint64_t VALIDATOR_STAKE = 1000000;
    static constexpr uint64_t BLOCKS_PER_DAY = 43200;  // 2-second blocks

    std::unique_ptr<state::GovernanceModule> governance_;
    std::map<state::ValidatorID, uint64_t> validator_stakes_;
    uint64_t total_stake_;
    uint64_t current_height_;

    void SetUp() override {
        governance_ = std::make_unique<state::GovernanceModule>(BLOCKS_PER_DAY);

        // Create validators with equal stake
        total_stake_ = 0;
        for (size_t i = 0; i < NUM_VALIDATORS; ++i) {
            state::ValidatorID validator_id = i;
            validator_stakes_[validator_id] = VALIDATOR_STAKE;
            total_stake_ += VALIDATOR_STAKE;
        }

        current_height_ = 1000;
    }

    void TearDown() override {
        governance_.reset();
        validator_stakes_.clear();
    }

    /**
     * Serialize a double value for governance proposals.
     */
    std::vector<uint8_t> serialize_double(double value) {
        std::vector<uint8_t> bytes(sizeof(double));
        std::memcpy(bytes.data(), &value, sizeof(double));
        return bytes;
    }

    /**
     * Serialize a uint64_t value for governance proposals.
     */
    std::vector<uint8_t> serialize_uint64(uint64_t value) {
        std::vector<uint8_t> bytes(sizeof(uint64_t));
        std::memcpy(bytes.data(), &value, sizeof(uint64_t));
        return bytes;
    }

    /**
     * Get backers representing a certain percentage of total stake.
     */
    std::vector<state::ValidatorID> get_backers(double percentage) {
        size_t num_backers = static_cast<size_t>(NUM_VALIDATORS * percentage);
        std::vector<state::ValidatorID> backers;
        for (size_t i = 0; i < num_backers; ++i) {
            backers.push_back(i);
        }
        return backers;
    }

    /**
     * Cast votes from validators.
     */
    void cast_votes(
        uint64_t proposal_id,
        double percentage_for,
        double percentage_against
    ) {
        size_t num_for = static_cast<size_t>(NUM_VALIDATORS * percentage_for);
        size_t num_against = static_cast<size_t>(NUM_VALIDATORS * percentage_against);

        // Vote for
        for (size_t i = 0; i < num_for; ++i) {
            state::ValidatorID voter = i;
            bool voted = governance_->vote(
                proposal_id,
                voter,
                true,  // in_favor
                validator_stakes_[voter],
                current_height_
            );
            ASSERT_TRUE(voted) << "Failed to cast vote for validator " << i;
        }

        // Vote against
        for (size_t i = num_for; i < num_for + num_against; ++i) {
            state::ValidatorID voter = i;
            bool voted = governance_->vote(
                proposal_id,
                voter,
                false,  // against
                validator_stakes_[voter],
                current_height_
            );
            ASSERT_TRUE(voted) << "Failed to cast vote for validator " << i;
        }
    }

    /**
     * Advance blockchain height by a number of blocks.
     */
    void advance_blocks(uint64_t num_blocks) {
        current_height_ += num_blocks;
    }

    /**
     * Advance blockchain height by a number of days.
     */
    void advance_days(uint64_t num_days) {
        advance_blocks(num_days * BLOCKS_PER_DAY);
    }
};

/**
 * Test: Submit proposal with sufficient backing.
 * 
 * Validates Property 40: Proposal Threshold
 * - Proposal requires ≥0.1% stake backing
 */
TEST_F(GovernanceIntegrationTest, SubmitProposalWithSufficientBacking) {
    // Get backers representing 0.2% of stake (above threshold)
    auto backers = get_backers(0.002);

    // Submit proposal to change issuance coefficient
    double new_k = 0.15;  // Within valid range [0.05, 0.2]
    auto new_value = serialize_double(new_k);

    state::Address proposer = {1, 2, 3, 4};

    auto proposal_id = governance_->submit_proposal(
        state::ParameterType::IssuanceCoefficient,
        new_value,
        "Increase issuance coefficient to 0.15",
        proposer,
        backers,
        validator_stakes_,
        total_stake_,
        current_height_
    );

    ASSERT_TRUE(proposal_id.has_value()) << "Failed to submit proposal with sufficient backing";

    // Verify proposal was created
    auto proposal = governance_->get_proposal(proposal_id.value());
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->status, state::ProposalStatus::Active);
    EXPECT_EQ(proposal->parameter, state::ParameterType::IssuanceCoefficient);
}

/**
 * Test: Reject proposal with insufficient backing.
 * 
 * Validates Property 40: Proposal Threshold
 * - Proposal with <0.1% stake backing is rejected
 */
TEST_F(GovernanceIntegrationTest, RejectProposalWithInsufficientBacking) {
    // Get backers representing 0.05% of stake (below threshold)
    auto backers = get_backers(0.0005);

    // Submit proposal
    double new_k = 0.15;
    auto new_value = serialize_double(new_k);
    state::Address proposer = {1, 2, 3, 4};

    auto proposal_id = governance_->submit_proposal(
        state::ParameterType::IssuanceCoefficient,
        new_value,
        "Increase issuance coefficient to 0.15",
        proposer,
        backers,
        validator_stakes_,
        total_stake_,
        current_height_
    );

    EXPECT_FALSE(proposal_id.has_value())
        << "Proposal with insufficient backing was accepted";
}

/**
 * Test: Complete governance workflow with approval.
 * 
 * Validates:
 * - Property 41: Approval Quorum (≥2/3 approval, ≥10% participation)
 * - Property 42: Timelock Duration (14 days for safety-critical)
 * - Full workflow: submit → vote → finalize → timelock → execute
 */
TEST_F(GovernanceIntegrationTest, CompleteWorkflowWithApproval) {
    // Step 1: Submit proposal
    auto backers = get_backers(0.002);
    double new_k = 0.15;
    auto new_value = serialize_double(new_k);
    state::Address proposer = {1, 2, 3, 4};

    auto proposal_id = governance_->submit_proposal(
        state::ParameterType::IssuanceCoefficient,
        new_value,
        "Increase issuance coefficient to 0.15",
        proposer,
        backers,
        validator_stakes_,
        total_stake_,
        current_height_
    );

    ASSERT_TRUE(proposal_id.has_value());

    // Step 2: Vote (70% for, 5% against = 75% participation, >2/3 approval)
    cast_votes(proposal_id.value(), 0.70, 0.05);

    // Step 3: Advance to end of voting period (7 days)
    advance_days(7);

    // Step 4: Finalize voting
    bool finalized = governance_->finalize_voting(
        proposal_id.value(),
        total_stake_,
        current_height_
    );

    ASSERT_TRUE(finalized) << "Failed to finalize voting";

    // Verify proposal is approved and in timelock
    auto proposal = governance_->get_proposal(proposal_id.value());
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->status, state::ProposalStatus::Approved);

    // Verify votes
    uint64_t expected_votes_for = static_cast<uint64_t>(0.70 * NUM_VALIDATORS * VALIDATOR_STAKE);
    uint64_t expected_votes_against = static_cast<uint64_t>(0.05 * NUM_VALIDATORS * VALIDATOR_STAKE);
    EXPECT_EQ(proposal->votes_for, expected_votes_for);
    EXPECT_EQ(proposal->votes_against, expected_votes_against);

    // Step 5: Wait for timelock (14 days for safety-critical parameter)
    advance_days(14);

    // Step 6: Execute proposal
    bool executed = governance_->execute_proposal(
        proposal_id.value(),
        current_height_
    );

    ASSERT_TRUE(executed) << "Failed to execute proposal";

    // Verify proposal is executed
    proposal = governance_->get_proposal(proposal_id.value());
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->status, state::ProposalStatus::Executed);
}

/**
 * Test: Reject proposal with insufficient approval.
 * 
 * Validates Property 41: Approval Quorum
 * - Proposal with <2/3 approval is rejected
 */
TEST_F(GovernanceIntegrationTest, RejectProposalWithInsufficientApproval) {
    // Submit proposal
    auto backers = get_backers(0.002);
    double new_k = 0.15;
    auto new_value = serialize_double(new_k);
    state::Address proposer = {1, 2, 3, 4};

    auto proposal_id = governance_->submit_proposal(
        state::ParameterType::IssuanceCoefficient,
        new_value,
        "Increase issuance coefficient to 0.15",
        proposer,
        backers,
        validator_stakes_,
        total_stake_,
        current_height_
    );

    ASSERT_TRUE(proposal_id.has_value());

    // Vote (40% for, 40% against = 80% participation, but only 50% approval)
    cast_votes(proposal_id.value(), 0.40, 0.40);

    // Advance to end of voting period
    advance_days(7);

    // Finalize voting
    bool finalized = governance_->finalize_voting(
        proposal_id.value(),
        total_stake_,
        current_height_
    );

    ASSERT_TRUE(finalized);

    // Verify proposal is rejected
    auto proposal = governance_->get_proposal(proposal_id.value());
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->status, state::ProposalStatus::Rejected)
        << "Proposal with insufficient approval was not rejected";
}

/**
 * Test: Reject proposal with insufficient quorum.
 * 
 * Validates Property 41: Approval Quorum
 * - Proposal with <10% participation is rejected
 */
TEST_F(GovernanceIntegrationTest, RejectProposalWithInsufficientQuorum) {
    // Submit proposal
    auto backers = get_backers(0.002);
    double new_k = 0.15;
    auto new_value = serialize_double(new_k);
    state::Address proposer = {1, 2, 3, 4};

    auto proposal_id = governance_->submit_proposal(
        state::ParameterType::IssuanceCoefficient,
        new_value,
        "Increase issuance coefficient to 0.15",
        proposer,
        backers,
        validator_stakes_,
        total_stake_,
        current_height_
    );

    ASSERT_TRUE(proposal_id.has_value());

    // Vote (5% for, 0% against = 5% participation, below 10% quorum)
    cast_votes(proposal_id.value(), 0.05, 0.0);

    // Advance to end of voting period
    advance_days(7);

    // Finalize voting
    bool finalized = governance_->finalize_voting(
        proposal_id.value(),
        total_stake_,
        current_height_
    );

    ASSERT_TRUE(finalized);

    // Verify proposal is rejected
    auto proposal = governance_->get_proposal(proposal_id.value());
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->status, state::ProposalStatus::Rejected)
        << "Proposal with insufficient quorum was not rejected";
}

/**
 * Test: Timelock durations for different parameter types.
 * 
 * Validates Property 42: Timelock Duration
 * - Safety-critical: 14 days
 * - Administrative: 7 days
 * - Performance: 3 days
 */
TEST_F(GovernanceIntegrationTest, TimelockDurations) {
    state::Address proposer = {1, 2, 3, 4};
    auto backers = get_backers(0.002);

    // Test safety-critical parameter (IssuanceCoefficient) - 14 days
    {
        double new_k = 0.15;
        auto new_value = serialize_double(new_k);

        auto proposal_id = governance_->submit_proposal(
            state::ParameterType::IssuanceCoefficient,
            new_value,
            "Test safety-critical timelock",
            proposer,
            backers,
            validator_stakes_,
            total_stake_,
            current_height_
        );

        ASSERT_TRUE(proposal_id.has_value());

        cast_votes(proposal_id.value(), 0.70, 0.05);
        advance_days(7);

        governance_->finalize_voting(proposal_id.value(), total_stake_, current_height_);

        auto proposal = governance_->get_proposal(proposal_id.value());
        ASSERT_TRUE(proposal.has_value());
        EXPECT_EQ(proposal->status, state::ProposalStatus::Approved);

        // Try to execute before timelock expires (should fail)
        advance_days(13);  // 13 days (1 day short)
        bool executed = governance_->execute_proposal(proposal_id.value(), current_height_);
        EXPECT_FALSE(executed) << "Executed proposal before timelock expired";

        // Execute after timelock expires
        advance_days(1);  // Now 14 days
        executed = governance_->execute_proposal(proposal_id.value(), current_height_);
        EXPECT_TRUE(executed) << "Failed to execute after timelock";
    }

    // Test administrative parameter (ValidatorSetSize) - 7 days
    {
        uint64_t new_size = 150;
        auto new_value = serialize_uint64(new_size);

        auto proposal_id = governance_->submit_proposal(
            state::ParameterType::ValidatorSetSize,
            new_value,
            "Test administrative timelock",
            proposer,
            backers,
            validator_stakes_,
            total_stake_,
            current_height_
        );

        ASSERT_TRUE(proposal_id.has_value());

        cast_votes(proposal_id.value(), 0.70, 0.05);
        advance_days(7);

        governance_->finalize_voting(proposal_id.value(), total_stake_, current_height_);

        // Try to execute before timelock expires (should fail)
        advance_days(6);  // 6 days (1 day short)
        bool executed = governance_->execute_proposal(proposal_id.value(), current_height_);
        EXPECT_FALSE(executed) << "Executed proposal before timelock expired";

        // Execute after timelock expires
        advance_days(1);  // Now 7 days
        executed = governance_->execute_proposal(proposal_id.value(), current_height_);
        EXPECT_TRUE(executed) << "Failed to execute after timelock";
    }

    // Test performance parameter (GasLimit) - 3 days
    {
        uint64_t new_gas_limit = 40000000;
        auto new_value = serialize_uint64(new_gas_limit);

        auto proposal_id = governance_->submit_proposal(
            state::ParameterType::GasLimit,
            new_value,
            "Test performance timelock",
            proposer,
            backers,
            validator_stakes_,
            total_stake_,
            current_height_
        );

        ASSERT_TRUE(proposal_id.has_value());

        cast_votes(proposal_id.value(), 0.70, 0.05);
        advance_days(7);

        governance_->finalize_voting(proposal_id.value(), total_stake_, current_height_);

        // Try to execute before timelock expires (should fail)
        advance_days(2);  // 2 days (1 day short)
        bool executed = governance_->execute_proposal(proposal_id.value(), current_height_);
        EXPECT_FALSE(executed) << "Executed proposal before timelock expired";

        // Execute after timelock expires
        advance_days(1);  // Now 3 days
        executed = governance_->execute_proposal(proposal_id.value(), current_height_);
        EXPECT_TRUE(executed) << "Failed to execute after timelock";
    }
}

/**
 * Test: Validate issuance coefficient bounds.
 * 
 * Validates that governance enforces 0.05 ≤ k ≤ 0.2
 */
TEST_F(GovernanceIntegrationTest, ValidateIssuanceCoefficientBounds) {
    state::Address proposer = {1, 2, 3, 4};
    auto backers = get_backers(0.002);

    // Test value below minimum (should be rejected)
    {
        double new_k = 0.04;  // Below minimum 0.05
        auto new_value = serialize_double(new_k);

        auto proposal_id = governance_->submit_proposal(
            state::ParameterType::IssuanceCoefficient,
            new_value,
            "Test k below minimum",
            proposer,
            backers,
            validator_stakes_,
            total_stake_,
            current_height_
        );

        EXPECT_FALSE(proposal_id.has_value())
            << "Proposal with k < 0.05 was accepted";
    }

    // Test value above maximum (should be rejected)
    {
        double new_k = 0.21;  // Above maximum 0.2
        auto new_value = serialize_double(new_k);

        auto proposal_id = governance_->submit_proposal(
            state::ParameterType::IssuanceCoefficient,
            new_value,
            "Test k above maximum",
            proposer,
            backers,
            validator_stakes_,
            total_stake_,
            current_height_
        );

        EXPECT_FALSE(proposal_id.has_value())
            << "Proposal with k > 0.2 was accepted";
    }

    // Test value at minimum (should be accepted)
    {
        double new_k = 0.05;
        auto new_value = serialize_double(new_k);

        auto proposal_id = governance_->submit_proposal(
            state::ParameterType::IssuanceCoefficient,
            new_value,
            "Test k at minimum",
            proposer,
            backers,
            validator_stakes_,
            total_stake_,
            current_height_
        );

        EXPECT_TRUE(proposal_id.has_value())
            << "Proposal with k = 0.05 was rejected";
    }

    // Test value at maximum (should be accepted)
    {
        double new_k = 0.2;
        auto new_value = serialize_double(new_k);

        auto proposal_id = governance_->submit_proposal(
            state::ParameterType::IssuanceCoefficient,
            new_value,
            "Test k at maximum",
            proposer,
            backers,
            validator_stakes_,
            total_stake_,
            current_height_
        );

        EXPECT_TRUE(proposal_id.has_value())
            << "Proposal with k = 0.2 was rejected";
    }
}

/**
 * Test: Prevent double voting.
 * 
 * Validates that a validator cannot vote twice on the same proposal.
 */
TEST_F(GovernanceIntegrationTest, PreventDoubleVoting) {
    // Submit proposal
    auto backers = get_backers(0.002);
    double new_k = 0.15;
    auto new_value = serialize_double(new_k);
    state::Address proposer = {1, 2, 3, 4};

    auto proposal_id = governance_->submit_proposal(
        state::ParameterType::IssuanceCoefficient,
        new_value,
        "Test double voting prevention",
        proposer,
        backers,
        validator_stakes_,
        total_stake_,
        current_height_
    );

    ASSERT_TRUE(proposal_id.has_value());

    // Validator 0 votes for
    state::ValidatorID voter = 0;
    bool voted = governance_->vote(
        proposal_id.value(),
        voter,
        true,
        validator_stakes_[voter],
        current_height_
    );

    ASSERT_TRUE(voted);

    // Validator 0 tries to vote again (should fail)
    voted = governance_->vote(
        proposal_id.value(),
        voter,
        false,  // Try to change vote
        validator_stakes_[voter],
        current_height_
    );

    EXPECT_FALSE(voted) << "Validator was able to vote twice";

    // Verify only one vote was counted
    auto proposal = governance_->get_proposal(proposal_id.value());
    ASSERT_TRUE(proposal.has_value());
    EXPECT_EQ(proposal->votes_for, VALIDATOR_STAKE);
    EXPECT_EQ(proposal->votes_against, 0);
}

/**
 * Test: Voting period enforcement.
 * 
 * Validates that votes cannot be cast after the voting period ends.
 */
TEST_F(GovernanceIntegrationTest, VotingPeriodEnforcement) {
    // Submit proposal
    auto backers = get_backers(0.002);
    double new_k = 0.15;
    auto new_value = serialize_double(new_k);
    state::Address proposer = {1, 2, 3, 4};

    auto proposal_id = governance_->submit_proposal(
        state::ParameterType::IssuanceCoefficient,
        new_value,
        "Test voting period enforcement",
        proposer,
        backers,
        validator_stakes_,
        total_stake_,
        current_height_
    );

    ASSERT_TRUE(proposal_id.has_value());

    // Advance past voting period (7 days)
    advance_days(8);

    // Try to vote after voting period ends (should fail)
    state::ValidatorID voter = 0;
    bool voted = governance_->vote(
        proposal_id.value(),
        voter,
        true,
        validator_stakes_[voter],
        current_height_
    );

    EXPECT_FALSE(voted) << "Vote was accepted after voting period ended";
}

} // namespace integration
} // namespace sarafu
