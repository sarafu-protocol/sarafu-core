#include "sarafu/state/governance_module.h"
#include <gtest/gtest.h>
#include <random>

using namespace sarafu::state;

/**
 * Property-Based Test for Timelock Duration
 * 
 * **Validates: Requirements 14.4, 14.5, 14.6**
 * 
 * Property 42: Timelock Duration
 * For any approved proposal, the timelock duration is:
 * - 14 days for safety-critical parameters (IssuanceCoefficient, SlashingAlpha, SlashingBeta)
 * - 7 days for administrative parameters (ValidatorSetSize, MinimumSelfBond)
 * - 3 days for performance parameters (BlockSize, GasLimit, BaseFeeAdjustment)
 * 
 * This test validates that:
 * 1. Safety-critical parameters have 14-day timelock
 * 2. Administrative parameters have 7-day timelock
 * 3. Performance parameters have 3-day timelock
 * 4. Timelock is calculated correctly in blocks
 */
class TimelockDurationPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        blocks_per_day_ = 43200;  // 2-second blocks
    }

    // Create a valid parameter value for the given type
    std::vector<uint8_t> create_valid_parameter_value(ParameterType type) {
        std::vector<uint8_t> value;

        switch (type) {
            case ParameterType::IssuanceCoefficient: {
                double k = 0.1;
                value.resize(sizeof(double));
                std::memcpy(value.data(), &k, sizeof(double));
                break;
            }
            case ParameterType::SlashingAlpha: {
                double alpha = 0.05;
                value.resize(sizeof(double));
                std::memcpy(value.data(), &alpha, sizeof(double));
                break;
            }
            case ParameterType::SlashingBeta: {
                double beta = 0.5;
                value.resize(sizeof(double));
                std::memcpy(value.data(), &beta, sizeof(double));
                break;
            }
            case ParameterType::ValidatorSetSize: {
                uint64_t n = 150;
                value.resize(sizeof(uint64_t));
                std::memcpy(value.data(), &n, sizeof(uint64_t));
                break;
            }
            case ParameterType::MinimumSelfBond: {
                uint64_t bond = 100000;
                value.resize(sizeof(uint64_t));
                std::memcpy(value.data(), &bond, sizeof(uint64_t));
                break;
            }
            case ParameterType::BlockSize: {
                uint64_t size = 1000000;
                value.resize(sizeof(uint64_t));
                std::memcpy(value.data(), &size, sizeof(uint64_t));
                break;
            }
            case ParameterType::GasLimit: {
                uint64_t limit = 10000000;
                value.resize(sizeof(uint64_t));
                std::memcpy(value.data(), &limit, sizeof(uint64_t));
                break;
            }
            case ParameterType::BaseFeeAdjustment: {
                double gamma = 0.125;
                value.resize(sizeof(double));
                std::memcpy(value.data(), &gamma, sizeof(double));
                break;
            }
        }

        return value;
    }

    // Submit and approve a proposal
    uint64_t submit_and_approve_proposal(
        GovernanceModule& governance,
        ParameterType parameter,
        uint64_t total_stake,
        uint64_t current_height,
        uint64_t block_rate
    ) {
        // Create validator with sufficient backing (10%)
        std::map<ValidatorID, uint64_t> validator_stakes;
        uint64_t backing = total_stake / 10;
        validator_stakes[0] = backing;
        validator_stakes[1] = total_stake - backing;

        std::vector<ValidatorID> backers = {0};
        Address proposer = {1, 2, 3, 4};
        auto param_value = create_valid_parameter_value(parameter);

        // Submit proposal
        auto result = governance.submit_proposal(
            parameter,
            param_value,
            "Test proposal",
            proposer,
            backers,
            validator_stakes,
            total_stake,
            current_height
        );

        uint64_t proposal_id = result.value();

        // Cast votes (70% for, 30% against, total 50% participation)
        uint64_t total_votes = total_stake / 2;
        uint64_t votes_for = (total_votes * 70) / 100;
        uint64_t votes_against = total_votes - votes_for;

        governance.vote(proposal_id, 100, true, votes_for, current_height);
        governance.vote(proposal_id, 101, false, votes_against, current_height);

        // Finalize voting
        uint64_t finalization_height = current_height + (7 * block_rate);
        governance.finalize_voting(proposal_id, total_stake, finalization_height);

        return proposal_id;
    }

    std::mt19937 rng_;
    uint64_t blocks_per_day_;
};

/**
 * Property: Safety-critical parameters have 14-day timelock
 * 
 * For any approved proposal for IssuanceCoefficient, SlashingAlpha, or SlashingBeta,
 * the timelock duration is exactly 14 days (in blocks).
 */
TEST_F(TimelockDurationPropertyTest, SafetyCriticalParametersHave14DayTimelock) {
    const int NUM_TRIALS = 300;

    std::vector<ParameterType> safety_critical_params = {
        ParameterType::IssuanceCoefficient,
        ParameterType::SlashingAlpha,
        ParameterType::SlashingBeta
    };

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance(blocks_per_day_);

        uint64_t total_stake = 10000000;
        uint64_t current_height = 1000;

        // Select a safety-critical parameter
        ParameterType param = safety_critical_params[trial % safety_critical_params.size()];

        // Submit and approve proposal
        uint64_t proposal_id = submit_and_approve_proposal(governance, param, total_stake, current_height, blocks_per_day_);

        // Check proposal
        auto proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());
        EXPECT_EQ(proposal->status, ProposalStatus::Approved);

        // Calculate expected timelock end
        uint64_t finalization_height = current_height + (7 * blocks_per_day_);
        uint64_t expected_timelock_end = finalization_height + (14 * blocks_per_day_);

        EXPECT_EQ(proposal->timelock_ends_at_height, expected_timelock_end)
            << "Safety-critical parameter should have 14-day timelock on trial " << trial
            << " (parameter=" << static_cast<int>(param) << ")";
    }
}

/**
 * Property: Administrative parameters have 7-day timelock
 * 
 * For any approved proposal for ValidatorSetSize or MinimumSelfBond,
 * the timelock duration is exactly 7 days (in blocks).
 */
TEST_F(TimelockDurationPropertyTest, AdministrativeParametersHave7DayTimelock) {
    const int NUM_TRIALS = 200;

    std::vector<ParameterType> admin_params = {
        ParameterType::ValidatorSetSize,
        ParameterType::MinimumSelfBond
    };

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance(blocks_per_day_);

        uint64_t total_stake = 10000000;
        uint64_t current_height = 1000;

        // Select an administrative parameter
        ParameterType param = admin_params[trial % admin_params.size()];

        // Submit and approve proposal
        uint64_t proposal_id = submit_and_approve_proposal(governance, param, total_stake, current_height, blocks_per_day_);

        // Check proposal
        auto proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());
        EXPECT_EQ(proposal->status, ProposalStatus::Approved);

        // Calculate expected timelock end
        uint64_t finalization_height = current_height + (7 * blocks_per_day_);
        uint64_t expected_timelock_end = finalization_height + (7 * blocks_per_day_);

        EXPECT_EQ(proposal->timelock_ends_at_height, expected_timelock_end)
            << "Administrative parameter should have 7-day timelock on trial " << trial
            << " (parameter=" << static_cast<int>(param) << ")";
    }
}

/**
 * Property: Performance parameters have 3-day timelock
 * 
 * For any approved proposal for BlockSize, GasLimit, or BaseFeeAdjustment,
 * the timelock duration is exactly 3 days (in blocks).
 */
TEST_F(TimelockDurationPropertyTest, PerformanceParametersHave3DayTimelock) {
    const int NUM_TRIALS = 300;

    std::vector<ParameterType> performance_params = {
        ParameterType::BlockSize,
        ParameterType::GasLimit,
        ParameterType::BaseFeeAdjustment
    };

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance(blocks_per_day_);

        uint64_t total_stake = 10000000;
        uint64_t current_height = 1000;

        // Select a performance parameter
        ParameterType param = performance_params[trial % performance_params.size()];

        // Submit and approve proposal
        uint64_t proposal_id = submit_and_approve_proposal(governance, param, total_stake, current_height, blocks_per_day_);

        // Check proposal
        auto proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());
        EXPECT_EQ(proposal->status, ProposalStatus::Approved);

        // Calculate expected timelock end
        uint64_t finalization_height = current_height + (7 * blocks_per_day_);
        uint64_t expected_timelock_end = finalization_height + (3 * blocks_per_day_);

        EXPECT_EQ(proposal->timelock_ends_at_height, expected_timelock_end)
            << "Performance parameter should have 3-day timelock on trial " << trial
            << " (parameter=" << static_cast<int>(param) << ")";
    }
}

/**
 * Property: Execution before timelock expires fails
 * 
 * For any approved proposal, attempting to execute before the timelock
 * expires should fail.
 */
TEST_F(TimelockDurationPropertyTest, ExecutionBeforeTimelockExpiresFails) {
    const int NUM_TRIALS = 500;

    std::vector<ParameterType> all_params = {
        ParameterType::IssuanceCoefficient,
        ParameterType::SlashingAlpha,
        ParameterType::SlashingBeta,
        ParameterType::ValidatorSetSize,
        ParameterType::MinimumSelfBond,
        ParameterType::BlockSize,
        ParameterType::GasLimit,
        ParameterType::BaseFeeAdjustment
    };

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance(blocks_per_day_);

        uint64_t total_stake = 10000000;
        uint64_t current_height = 1000;

        // Select a parameter
        ParameterType param = all_params[trial % all_params.size()];

        // Submit and approve proposal
        uint64_t proposal_id = submit_and_approve_proposal(governance, param, total_stake, current_height, blocks_per_day_);

        // Get proposal
        auto proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());

        // Try to execute before timelock expires
        std::uniform_int_distribution<uint64_t> height_dist(
            current_height + (7 * blocks_per_day_),
            proposal->timelock_ends_at_height - 1
        );
        uint64_t early_height = height_dist(rng_);

        bool executed = governance.execute_proposal(proposal_id, early_height);

        EXPECT_FALSE(executed)
            << "Execution should fail before timelock expires on trial " << trial
            << " (early_height=" << early_height << ", timelock_ends=" << proposal->timelock_ends_at_height << ")";
    }
}

/**
 * Property: Execution after timelock expires succeeds
 * 
 * For any approved proposal, attempting to execute after the timelock
 * expires should succeed.
 */
TEST_F(TimelockDurationPropertyTest, ExecutionAfterTimelockExpiresSucceeds) {
    const int NUM_TRIALS = 500;

    std::vector<ParameterType> all_params = {
        ParameterType::IssuanceCoefficient,
        ParameterType::SlashingAlpha,
        ParameterType::SlashingBeta,
        ParameterType::ValidatorSetSize,
        ParameterType::MinimumSelfBond,
        ParameterType::BlockSize,
        ParameterType::GasLimit,
        ParameterType::BaseFeeAdjustment
    };

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance(blocks_per_day_);

        uint64_t total_stake = 10000000;
        uint64_t current_height = 1000;

        // Select a parameter
        ParameterType param = all_params[trial % all_params.size()];

        // Submit and approve proposal
        uint64_t proposal_id = submit_and_approve_proposal(governance, param, total_stake, current_height, blocks_per_day_);

        // Get proposal
        auto proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());

        // Execute after timelock expires
        uint64_t execution_height = proposal->timelock_ends_at_height + (trial % 1000);

        bool executed = governance.execute_proposal(proposal_id, execution_height);

        EXPECT_TRUE(executed)
            << "Execution should succeed after timelock expires on trial " << trial
            << " (execution_height=" << execution_height << ", timelock_ends=" << proposal->timelock_ends_at_height << ")";

        // Verify status changed to Executed
        proposal = governance.get_proposal(proposal_id);
        ASSERT_TRUE(proposal.has_value());
        EXPECT_EQ(proposal->status, ProposalStatus::Executed);
    }
}

/**
 * Property: Timelock calculation is consistent across different block rates
 * 
 * For any parameter type, the timelock duration in days should be consistent
 * regardless of the blocks_per_day setting.
 */
TEST_F(TimelockDurationPropertyTest, TimelockConsistentAcrossBlockRates) {
    const int NUM_TRIALS = 300;

    std::vector<ParameterType> all_params = {
        ParameterType::IssuanceCoefficient,
        ParameterType::ValidatorSetSize,
        ParameterType::BlockSize
    };

    std::vector<uint64_t> expected_days = {14, 7, 3};

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Test with different block rates
        std::vector<uint64_t> block_rates = {43200, 28800, 86400};  // 2s, 3s, 1s blocks

        for (size_t i = 0; i < all_params.size(); ++i) {
            ParameterType param = all_params[i];
            uint64_t expected_day_count = expected_days[i];

            for (uint64_t block_rate : block_rates) {
                GovernanceModule governance(block_rate);

                uint64_t total_stake = 10000000;
                uint64_t current_height = 1000;

                // Submit and approve proposal
                uint64_t proposal_id = submit_and_approve_proposal(governance, param, total_stake, current_height, block_rate);

                // Get proposal
                auto proposal = governance.get_proposal(proposal_id);
                ASSERT_TRUE(proposal.has_value());

                // Calculate expected timelock duration in blocks
                uint64_t finalization_height = current_height + (7 * block_rate);
                uint64_t expected_timelock_end = finalization_height + (expected_day_count * block_rate);

                EXPECT_EQ(proposal->timelock_ends_at_height, expected_timelock_end)
                    << "Timelock should be consistent across block rates on trial " << trial
                    << " (parameter=" << static_cast<int>(param)
                    << ", block_rate=" << block_rate
                    << ", expected_days=" << expected_day_count << ")";
            }
        }
    }
}
