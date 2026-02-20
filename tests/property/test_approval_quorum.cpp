#include "sarafu/state/governance_module.h"
#include <gtest/gtest.h>
#include <random>

using namespace sarafu::state;

/**
 * Property-Based Test for Approval Quorum
 * 
 * **Validates: Requirements 14.3**
 * 
 * Property 41: Approval Quorum
 * For any proposal, it is approved only if votes_for / (votes_for + votes_against) ≥ 2/3
 * AND (votes_for + votes_against) ≥ 10% of total stake.
 * 
 * This test validates that:
 * 1. Proposals with ≥2/3 approval and ≥10% participation are approved
 * 2. Proposals with <2/3 approval are rejected (even with high participation)
 * 3. Proposals with <10% participation are rejected (even with high approval)
 * 4. Edge cases (exactly 2/3, exactly 10%) are handled correctly
 */
class ApprovalQuorumPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random total stake (1M to 100M)
    uint64_t generate_random_total_stake() {
        std::uniform_int_distribution<uint64_t> dist(1000000, 100000000);
        return dist(rng_);
    }

    // Create a valid parameter value
    std::vector<uint8_t> create_valid_parameter_value() {
        std::vector<uint8_t> value;
        uint64_t size = 1000000;
        value.resize(sizeof(uint64_t));
        std::memcpy(value.data(), &size, sizeof(uint64_t));
        return value;
    }

    // Submit a proposal and get its ID
    uint64_t submit_test_proposal(
        GovernanceModule& governance,
        uint64_t total_stake,
        uint64_t current_height
    ) {
        // Create validator with sufficient backing (10%)
        std::map<ValidatorID, uint64_t> validator_stakes;
        uint64_t backing = total_stake / 10;
        validator_stakes[0] = backing;
        validator_stakes[1] = total_stake - backing;

        std::vector<ValidatorID> backers = {0};
        Address proposer = {1, 2, 3, 4};
        auto param_value = create_valid_parameter_value();

        auto result = governance.submit_proposal(
            ParameterType::BlockSize,
            param_value,
            "Test proposal",
            proposer,
            backers,
            validator_stakes,
            total_stake,
            current_height
        );

        return result.value();
    }

    std::mt19937 rng_;
};

/**
 * Property: Proposals with ≥2/3 approval and ≥10% participation are approved
 * 
 * For any proposal with approval rate ≥2/3 and participation ≥10%,
 * the proposal is approved after finalization.
 */
TEST_F(ApprovalQuorumPropertyTest, ProposalsWithSufficientApprovalAndQuorumApproved) {
    const int NUM_TRIALS = 1000;
    const double APPROVAL_THRESHOLD = 2.0 / 3.0;
    const double QUORUM_THRESHOLD = 0.10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        uint64_t total_stake = generate_random_total_stake();
        uint64_t current_height = 1000;

        // Submit proposal
        uint64_t proposal_id = submit_test_proposal(governance, total_stake, current_height);

        // Generate participation rate (10% to 100%)
        std::uniform_real_distribution<double> participation_dist(QUORUM_THRESHOLD, 1.0);
        double participation_rate = participation_dist(rng_);
        uint64_t total_votes = static_cast<uint64_t>(total_stake * participation_rate);

        // Generate approval rate (2/3 to 100%)
        std::uniform_real_distribution<double> approval_dist(APPROVAL_THRESHOLD, 1.0);
        double approval_rate = approval_dist(rng_);
        uint64_t votes_for = static_cast<uint64_t>(total_votes * approval_rate);
        uint64_t votes_against = total_votes - votes_for;

        // Cast votes
        ValidatorID voter_id = 100;
        uint64_t remaining_for = votes_for;
        uint64_t remaining_against = votes_against;

        while (remaining_for > 0 || remaining_against > 0) {
            if (remaining_for > 0) {
                uint64_t vote_stake = std::min(remaining_for, total_stake / 100);
                governance.vote(proposal_id, voter_id++, true, vote_stake, current_height);
                remaining_for -= vote_stake;
            }
            if (remaining_against > 0) {
                uint64_t vote_stake = std::min(remaining_against, total_stake / 100);
                governance.vote(proposal_id, voter_id++, false, vote_stake, current_height);
                remaining_against -= vote_stake;
            }
        }

        // Advance to end of voting period
        uint64_t finalization_height = current_height + (7 * 43200);  // 7 days

        // Finalize voting
        bool finalized = governance.finalize_voting(proposal_id, total_stake, finalization_height);
        ASSERT_TRUE(finalized) << "Finalization should succeed on trial " << trial;

        // Check proposal status
        auto proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());

        EXPECT_EQ(proposal->status, ProposalStatus::Approved)
            << "Proposal with " << (approval_rate * 100) << "% approval and "
            << (participation_rate * 100) << "% participation should be approved on trial " << trial;
    }
}

/**
 * Property: Proposals with <2/3 approval are rejected
 * 
 * For any proposal with approval rate <2/3 (even with high participation),
 * the proposal is rejected after finalization.
 */
TEST_F(ApprovalQuorumPropertyTest, ProposalsWithInsufficientApprovalRejected) {
    const int NUM_TRIALS = 1000;
    const double APPROVAL_THRESHOLD = 2.0 / 3.0;
    const double QUORUM_THRESHOLD = 0.10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        uint64_t total_stake = generate_random_total_stake();
        uint64_t current_height = 1000;

        // Submit proposal
        uint64_t proposal_id = submit_test_proposal(governance, total_stake, current_height);

        // Generate participation rate (10% to 100%) - sufficient
        std::uniform_real_distribution<double> participation_dist(QUORUM_THRESHOLD, 1.0);
        double participation_rate = participation_dist(rng_);
        uint64_t total_votes = static_cast<uint64_t>(total_stake * participation_rate);

        // Generate approval rate (10% to 66%) - insufficient
        std::uniform_real_distribution<double> approval_dist(0.1, APPROVAL_THRESHOLD * 0.99);
        double approval_rate = approval_dist(rng_);
        uint64_t votes_for = static_cast<uint64_t>(total_votes * approval_rate);
        uint64_t votes_against = total_votes - votes_for;

        // Cast votes
        ValidatorID voter_id = 100;
        uint64_t remaining_for = votes_for;
        uint64_t remaining_against = votes_against;

        while (remaining_for > 0 || remaining_against > 0) {
            if (remaining_for > 0) {
                uint64_t vote_stake = std::min(remaining_for, total_stake / 100);
                governance.vote(proposal_id, voter_id++, true, vote_stake, current_height);
                remaining_for -= vote_stake;
            }
            if (remaining_against > 0) {
                uint64_t vote_stake = std::min(remaining_against, total_stake / 100);
                governance.vote(proposal_id, voter_id++, false, vote_stake, current_height);
                remaining_against -= vote_stake;
            }
        }

        // Advance to end of voting period
        uint64_t finalization_height = current_height + (7 * 43200);

        // Finalize voting
        bool finalized = governance.finalize_voting(proposal_id, total_stake, finalization_height);
        ASSERT_TRUE(finalized) << "Finalization should succeed on trial " << trial;

        // Check proposal status
        auto proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());

        EXPECT_EQ(proposal->status, ProposalStatus::Rejected)
            << "Proposal with " << (approval_rate * 100) << "% approval should be rejected on trial " << trial;
    }
}

/**
 * Property: Proposals with <10% participation are rejected
 * 
 * For any proposal with participation <10% (even with high approval),
 * the proposal is rejected after finalization.
 */
TEST_F(ApprovalQuorumPropertyTest, ProposalsWithInsufficientQuorumRejected) {
    const int NUM_TRIALS = 1000;
    const double APPROVAL_THRESHOLD = 2.0 / 3.0;
    const double QUORUM_THRESHOLD = 0.10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        uint64_t total_stake = generate_random_total_stake();
        uint64_t current_height = 1000;

        // Submit proposal
        uint64_t proposal_id = submit_test_proposal(governance, total_stake, current_height);

        // Generate participation rate (1% to 9.9%) - insufficient
        std::uniform_real_distribution<double> participation_dist(0.01, QUORUM_THRESHOLD * 0.99);
        double participation_rate = participation_dist(rng_);
        uint64_t total_votes = static_cast<uint64_t>(total_stake * participation_rate);

        // Generate approval rate (67% to 100%) - sufficient
        std::uniform_real_distribution<double> approval_dist(APPROVAL_THRESHOLD, 1.0);
        double approval_rate = approval_dist(rng_);
        uint64_t votes_for = static_cast<uint64_t>(total_votes * approval_rate);
        uint64_t votes_against = total_votes - votes_for;

        // Cast votes
        ValidatorID voter_id = 100;
        uint64_t remaining_for = votes_for;
        uint64_t remaining_against = votes_against;

        while (remaining_for > 0 || remaining_against > 0) {
            if (remaining_for > 0) {
                uint64_t vote_stake = std::min(remaining_for, total_stake / 100);
                governance.vote(proposal_id, voter_id++, true, vote_stake, current_height);
                remaining_for -= vote_stake;
            }
            if (remaining_against > 0) {
                uint64_t vote_stake = std::min(remaining_against, total_stake / 100);
                governance.vote(proposal_id, voter_id++, false, vote_stake, current_height);
                remaining_against -= vote_stake;
            }
        }

        // Advance to end of voting period
        uint64_t finalization_height = current_height + (7 * 43200);

        // Finalize voting
        bool finalized = governance.finalize_voting(proposal_id, total_stake, finalization_height);
        ASSERT_TRUE(finalized) << "Finalization should succeed on trial " << trial;

        // Check proposal status
        auto proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());

        EXPECT_EQ(proposal->status, ProposalStatus::Rejected)
            << "Proposal with " << (participation_rate * 100) << "% participation should be rejected on trial " << trial;
    }
}

/**
 * Property: Exactly 2/3 approval with ≥10% participation is approved
 * 
 * For any proposal with exactly 2/3 approval and sufficient participation,
 * the proposal is approved (threshold is inclusive).
 */
TEST_F(ApprovalQuorumPropertyTest, ExactlyTwoThirdsApprovalApproved) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        uint64_t total_stake = generate_random_total_stake();
        uint64_t current_height = 1000;

        // Submit proposal
        uint64_t proposal_id = submit_test_proposal(governance, total_stake, current_height);

        // Use 15% participation (above quorum)
        uint64_t total_votes = total_stake * 15 / 100;

        // Exactly 2/3 approval: votes_for * 3 = total_votes * 2
        uint64_t votes_for = (total_votes * 2) / 3;
        uint64_t votes_against = total_votes - votes_for;

        // Cast votes
        governance.vote(proposal_id, 100, true, votes_for, current_height);
        governance.vote(proposal_id, 101, false, votes_against, current_height);

        // Advance to end of voting period
        uint64_t finalization_height = current_height + (7 * 43200);

        // Finalize voting
        bool finalized = governance.finalize_voting(proposal_id, total_stake, finalization_height);
        ASSERT_TRUE(finalized);

        // Check proposal status
        auto proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());

        EXPECT_EQ(proposal->status, ProposalStatus::Approved)
            << "Proposal with exactly 2/3 approval should be approved on trial " << trial
            << " (votes_for=" << votes_for << ", total_votes=" << total_votes << ")";
    }
}

/**
 * Property: Exactly 10% participation with ≥2/3 approval is approved
 * 
 * For any proposal with exactly 10% participation and sufficient approval,
 * the proposal is approved (threshold is inclusive).
 */
TEST_F(ApprovalQuorumPropertyTest, ExactlyTenPercentParticipationApproved) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        uint64_t total_stake = generate_random_total_stake();
        uint64_t current_height = 1000;

        // Submit proposal
        uint64_t proposal_id = submit_test_proposal(governance, total_stake, current_height);

        // Exactly 10% participation
        uint64_t total_votes = total_stake / 10;

        // 70% approval (above 2/3)
        uint64_t votes_for = (total_votes * 70) / 100;
        uint64_t votes_against = total_votes - votes_for;

        // Cast votes
        governance.vote(proposal_id, 100, true, votes_for, current_height);
        governance.vote(proposal_id, 101, false, votes_against, current_height);

        // Advance to end of voting period
        uint64_t finalization_height = current_height + (7 * 43200);

        // Finalize voting
        bool finalized = governance.finalize_voting(proposal_id, total_stake, finalization_height);
        ASSERT_TRUE(finalized);

        // Check proposal status
        auto proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());

        EXPECT_EQ(proposal->status, ProposalStatus::Approved)
            << "Proposal with exactly 10% participation should be approved on trial " << trial
            << " (total_votes=" << total_votes << ", total_stake=" << total_stake << ")";
    }
}

/**
 * Property: Finalization before voting period ends fails
 * 
 * For any proposal, attempting to finalize before the voting period ends
 * should fail.
 */
TEST_F(ApprovalQuorumPropertyTest, FinalizationBeforeVotingPeriodEndsFails) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        uint64_t total_stake = generate_random_total_stake();
        uint64_t current_height = 1000;

        // Submit proposal
        uint64_t proposal_id = submit_test_proposal(governance, total_stake, current_height);

        // Cast some votes
        uint64_t votes = total_stake / 5;
        governance.vote(proposal_id, 100, true, votes, current_height);

        // Try to finalize before voting period ends
        std::uniform_int_distribution<uint64_t> height_dist(current_height, current_height + (7 * 43200) - 1);
        uint64_t early_height = height_dist(rng_);

        bool finalized = governance.finalize_voting(proposal_id, total_stake, early_height);

        EXPECT_FALSE(finalized)
            << "Finalization should fail before voting period ends on trial " << trial
            << " (early_height=" << early_height << ", voting_ends=" << (current_height + 7 * 43200) << ")";
    }
}
