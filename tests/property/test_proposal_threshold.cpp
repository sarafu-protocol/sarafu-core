#include "sarafu/state/governance_module.h"
#include <gtest/gtest.h>
#include <random>

using namespace sarafu::state;

/**
 * Property-Based Test for Proposal Threshold
 * 
 * **Validates: Requirements 14.1**
 * 
 * Property 40: Proposal Threshold
 * For any proposal submission, the proposal is accepted only if the backing
 * stake is ≥0.1% of total stake.
 * 
 * This test validates that:
 * 1. Proposals with ≥0.1% backing are accepted
 * 2. Proposals with <0.1% backing are rejected
 * 3. Threshold calculation is accurate across different stake distributions
 * 4. Edge cases (exactly 0.1%, just below, just above) are handled correctly
 */
class ProposalThresholdPropertyTest : public ::testing::Test {
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

    // Generate random backing stake as fraction of total
    uint64_t generate_backing_stake(uint64_t total_stake, double fraction) {
        return static_cast<uint64_t>(total_stake * fraction);
    }

    // Generate random validator stakes that sum to total_stake
    std::map<ValidatorID, uint64_t> generate_validator_stakes(
        uint64_t total_stake,
        size_t num_validators
    ) {
        std::map<ValidatorID, uint64_t> stakes;
        uint64_t remaining = total_stake;

        for (size_t i = 0; i < num_validators - 1; ++i) {
            uint64_t max_stake = remaining / (num_validators - i);
            std::uniform_int_distribution<uint64_t> dist(1, max_stake);
            uint64_t stake = dist(rng_);
            stakes[i] = stake;
            remaining -= stake;
        }

        // Last validator gets remaining stake
        stakes[num_validators - 1] = remaining;

        return stakes;
    }

    // Select validators to back proposal
    std::vector<ValidatorID> select_backers(
        const std::map<ValidatorID, uint64_t>& validator_stakes,
        uint64_t target_backing
    ) {
        std::vector<ValidatorID> backers;
        uint64_t current_backing = 0;

        for (const auto& [validator_id, stake] : validator_stakes) {
            if (current_backing >= target_backing) {
                break;
            }
            backers.push_back(validator_id);
            current_backing += stake;
        }

        return backers;
    }

    // Create a valid parameter value
    std::vector<uint8_t> create_valid_parameter_value(ParameterType type) {
        std::vector<uint8_t> value;

        switch (type) {
            case ParameterType::IssuanceCoefficient: {
                double k = 0.1;  // Valid: 0.05 ≤ k ≤ 0.2
                value.resize(sizeof(double));
                std::memcpy(value.data(), &k, sizeof(double));
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
            case ParameterType::ValidatorSetSize: {
                uint64_t n = 150;  // Valid: 100 ≤ N ≤ 500
                value.resize(sizeof(uint64_t));
                std::memcpy(value.data(), &n, sizeof(uint64_t));
                break;
            }
            default: {
                double val = 0.5;
                value.resize(sizeof(double));
                std::memcpy(value.data(), &val, sizeof(double));
                break;
            }
        }

        return value;
    }

    std::mt19937 rng_;
};

/**
 * Property: Proposals with ≥0.1% backing are accepted
 * 
 * For any proposal with backing stake ≥0.1% of total stake,
 * the proposal is accepted and assigned an ID.
 */
TEST_F(ProposalThresholdPropertyTest, ProposalsAboveThresholdAccepted) {
    const int NUM_TRIALS = 1000;
    const double THRESHOLD = 0.001;  // 0.1%

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        uint64_t total_stake = generate_random_total_stake();
        size_t num_validators = 10 + (trial % 40);  // 10-50 validators

        auto validator_stakes = generate_validator_stakes(total_stake, num_validators);

        // Generate backing stake above threshold (0.1% to 10%)
        std::uniform_real_distribution<double> backing_dist(THRESHOLD, 0.1);
        double backing_fraction = backing_dist(rng_);
        uint64_t target_backing = generate_backing_stake(total_stake, backing_fraction);

        auto backers = select_backers(validator_stakes, target_backing);

        // Calculate actual backing
        uint64_t actual_backing = 0;
        for (auto backer : backers) {
            actual_backing += validator_stakes[backer];
        }

        // Ensure we're above threshold
        if (actual_backing < static_cast<uint64_t>(total_stake * THRESHOLD)) {
            continue;
        }

        Address proposer = {1, 2, 3, 4};
        auto param_value = create_valid_parameter_value(ParameterType::BlockSize);

        auto result = governance.submit_proposal(
            ParameterType::BlockSize,
            param_value,
            "Test proposal",
            proposer,
            backers,
            validator_stakes,
            total_stake,
            1000  // current_height
        );

        ASSERT_TRUE(result.has_value())
            << "Proposal with " << (backing_fraction * 100) << "% backing should be accepted on trial " << trial
            << " (actual_backing=" << actual_backing << ", total_stake=" << total_stake << ")";

        // Verify proposal was created
        auto proposal = governance.get_proposal(result.value());
        ASSERT_TRUE(proposal.has_value());
        EXPECT_EQ(proposal->status, ProposalStatus::Active);
    }
}

/**
 * Property: Proposals with <0.1% backing are rejected
 * 
 * For any proposal with backing stake <0.1% of total stake,
 * the proposal is rejected (returns std::nullopt).
 */
TEST_F(ProposalThresholdPropertyTest, ProposalsBelowThresholdRejected) {
    const int NUM_TRIALS = 1000;
    const double THRESHOLD = 0.001;  // 0.1%

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        uint64_t total_stake = generate_random_total_stake();
        size_t num_validators = 10 + (trial % 40);  // 10-50 validators

        auto validator_stakes = generate_validator_stakes(total_stake, num_validators);

        // Generate backing stake below threshold (0.001% to 0.099%)
        std::uniform_real_distribution<double> backing_dist(0.00001, THRESHOLD * 0.99);
        double backing_fraction = backing_dist(rng_);
        uint64_t target_backing = generate_backing_stake(total_stake, backing_fraction);

        auto backers = select_backers(validator_stakes, target_backing);

        // Calculate actual backing
        uint64_t actual_backing = 0;
        for (auto backer : backers) {
            actual_backing += validator_stakes[backer];
        }

        // Ensure we're below threshold
        if (actual_backing >= static_cast<uint64_t>(total_stake * THRESHOLD)) {
            continue;
        }

        Address proposer = {1, 2, 3, 4};
        auto param_value = create_valid_parameter_value(ParameterType::BlockSize);

        auto result = governance.submit_proposal(
            ParameterType::BlockSize,
            param_value,
            "Test proposal",
            proposer,
            backers,
            validator_stakes,
            total_stake,
            1000  // current_height
        );

        ASSERT_FALSE(result.has_value())
            << "Proposal with " << (backing_fraction * 100) << "% backing should be rejected on trial " << trial
            << " (actual_backing=" << actual_backing << ", total_stake=" << total_stake << ")";
    }
}

/**
 * Property: Exactly 0.1% backing is accepted
 * 
 * For any proposal with backing stake exactly equal to 0.1% of total stake,
 * the proposal is accepted (threshold is inclusive).
 * 
 * Note: We use integer arithmetic to avoid floating point precision issues.
 * The threshold check is: backing_stake * 1000 >= total_stake
 */
TEST_F(ProposalThresholdPropertyTest, ExactlyThresholdAccepted) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        // Use total stakes that are multiples of 1000 to ensure exact threshold
        uint64_t total_stake = (1000000 + (trial * 1000)) * 1000;

        // Calculate exact threshold backing: backing * 1000 = total_stake
        // So backing = total_stake / 1000
        uint64_t threshold_backing = total_stake / 1000;

        // Create validator stakes where first validator has exactly threshold backing
        std::map<ValidatorID, uint64_t> validator_stakes;
        validator_stakes[0] = threshold_backing;
        validator_stakes[1] = total_stake - threshold_backing;

        std::vector<ValidatorID> backers = {0};

        Address proposer = {1, 2, 3, 4};
        auto param_value = create_valid_parameter_value(ParameterType::BlockSize);

        auto result = governance.submit_proposal(
            ParameterType::BlockSize,
            param_value,
            "Test proposal",
            proposer,
            backers,
            validator_stakes,
            total_stake,
            1000  // current_height
        );

        ASSERT_TRUE(result.has_value())
            << "Proposal with exactly 0.1% backing should be accepted on trial " << trial
            << " (threshold_backing=" << threshold_backing << ", total_stake=" << total_stake << ")"
            << " (check: " << threshold_backing << " * 1000 = " << (threshold_backing * 1000) << ")";
    }
}

/**
 * Property: Just below 0.1% backing is rejected
 * 
 * For any proposal with backing stake just below 0.1% of total stake,
 * the proposal is rejected.
 * 
 * Note: We use integer arithmetic. The threshold check is: backing_stake * 1000 >= total_stake
 * So just below means: backing_stake * 1000 < total_stake
 */
TEST_F(ProposalThresholdPropertyTest, JustBelowThresholdRejected) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        // Use total stakes that are multiples of 1000 to ensure exact threshold
        uint64_t total_stake = (1000000 + (trial * 1000)) * 1000;

        // Calculate threshold backing: backing * 1000 = total_stake
        uint64_t threshold_backing = total_stake / 1000;
        
        // Just below threshold
        if (threshold_backing <= 1) {
            continue;  // Skip if threshold is too small
        }
        uint64_t below_threshold = threshold_backing - 1;

        // Create validator stakes
        std::map<ValidatorID, uint64_t> validator_stakes;
        validator_stakes[0] = below_threshold;
        validator_stakes[1] = total_stake - below_threshold;

        std::vector<ValidatorID> backers = {0};

        Address proposer = {1, 2, 3, 4};
        auto param_value = create_valid_parameter_value(ParameterType::BlockSize);

        auto result = governance.submit_proposal(
            ParameterType::BlockSize,
            param_value,
            "Test proposal",
            proposer,
            backers,
            validator_stakes,
            total_stake,
            1000  // current_height
        );

        ASSERT_FALSE(result.has_value())
            << "Proposal with backing just below 0.1% should be rejected on trial " << trial
            << " (below_threshold=" << below_threshold << ", threshold=" << threshold_backing
            << ", total_stake=" << total_stake << ")"
            << " (check: " << below_threshold << " * 1000 = " << (below_threshold * 1000) << ")";
    }
}

/**
 * Property: Multiple backers' stakes are summed correctly
 * 
 * For any proposal with multiple backers, the total backing stake
 * is the sum of all backers' stakes.
 */
TEST_F(ProposalThresholdPropertyTest, MultipleBackersSummedCorrectly) {
    const int NUM_TRIALS = 500;
    const double THRESHOLD = 0.001;  // 0.1%

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        uint64_t total_stake = generate_random_total_stake();
        size_t num_validators = 10 + (trial % 40);  // 10-50 validators

        auto validator_stakes = generate_validator_stakes(total_stake, num_validators);

        // Select multiple backers (2-5)
        size_t num_backers = 2 + (trial % 4);
        std::vector<ValidatorID> backers;
        uint64_t expected_backing = 0;

        size_t count = 0;
        for (const auto& [validator_id, stake] : validator_stakes) {
            if (count >= num_backers) {
                break;
            }
            backers.push_back(validator_id);
            expected_backing += stake;
            count++;
        }

        Address proposer = {1, 2, 3, 4};
        auto param_value = create_valid_parameter_value(ParameterType::BlockSize);

        auto result = governance.submit_proposal(
            ParameterType::BlockSize,
            param_value,
            "Test proposal",
            proposer,
            backers,
            validator_stakes,
            total_stake,
            1000  // current_height
        );

        bool should_accept = expected_backing >= static_cast<uint64_t>(total_stake * THRESHOLD);

        if (should_accept) {
            ASSERT_TRUE(result.has_value())
                << "Proposal with sufficient backing from multiple backers should be accepted on trial " << trial;

            auto proposal = governance.get_proposal(result.value());
            ASSERT_TRUE(proposal.has_value());
            EXPECT_EQ(proposal->stake_backing, expected_backing)
                << "Proposal backing should equal sum of backers' stakes";
        } else {
            ASSERT_FALSE(result.has_value())
                << "Proposal with insufficient backing from multiple backers should be rejected on trial " << trial;
        }
    }
}

/**
 * Property: Invalid parameter values are rejected regardless of backing
 * 
 * For any proposal with invalid parameter value, the proposal is rejected
 * even if backing stake is sufficient.
 */
TEST_F(ProposalThresholdPropertyTest, InvalidParameterValuesRejected) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        GovernanceModule governance;

        uint64_t total_stake = generate_random_total_stake();

        // Create validator with sufficient backing (10%)
        std::map<ValidatorID, uint64_t> validator_stakes;
        uint64_t backing = total_stake / 10;
        validator_stakes[0] = backing;
        validator_stakes[1] = total_stake - backing;

        std::vector<ValidatorID> backers = {0};

        Address proposer = {1, 2, 3, 4};

        // Create invalid parameter value (k = 0.3, outside bounds 0.05-0.2)
        std::vector<uint8_t> invalid_value;
        double invalid_k = 0.3;
        invalid_value.resize(sizeof(double));
        std::memcpy(invalid_value.data(), &invalid_k, sizeof(double));

        auto result = governance.submit_proposal(
            ParameterType::IssuanceCoefficient,
            invalid_value,
            "Test proposal",
            proposer,
            backers,
            validator_stakes,
            total_stake,
            1000  // current_height
        );

        ASSERT_FALSE(result.has_value())
            << "Proposal with invalid parameter value should be rejected on trial " << trial
            << " (backing=" << backing << ", total_stake=" << total_stake << ")";
    }
}

/**
 * Property: Threshold calculation is consistent across different total stakes
 * 
 * For any two proposals with the same backing fraction but different total stakes,
 * both should have the same acceptance outcome.
 */
TEST_F(ProposalThresholdPropertyTest, ThresholdConsistentAcrossStakes) {
    const int NUM_TRIALS = 500;
    const double THRESHOLD = 0.001;  // 0.1%

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate two different total stakes
        uint64_t total_stake1 = generate_random_total_stake();
        uint64_t total_stake2 = generate_random_total_stake();

        // Use same backing fraction for both
        std::uniform_real_distribution<double> backing_dist(0.0005, 0.002);
        double backing_fraction = backing_dist(rng_);

        uint64_t backing1 = generate_backing_stake(total_stake1, backing_fraction);
        uint64_t backing2 = generate_backing_stake(total_stake2, backing_fraction);

        // Create validator stakes for both
        std::map<ValidatorID, uint64_t> stakes1;
        stakes1[0] = backing1;
        stakes1[1] = total_stake1 - backing1;

        std::map<ValidatorID, uint64_t> stakes2;
        stakes2[0] = backing2;
        stakes2[1] = total_stake2 - backing2;

        std::vector<ValidatorID> backers = {0};
        Address proposer = {1, 2, 3, 4};
        auto param_value = create_valid_parameter_value(ParameterType::BlockSize);

        GovernanceModule gov1, gov2;

        auto result1 = gov1.submit_proposal(
            ParameterType::BlockSize, param_value, "Test", proposer,
            backers, stakes1, total_stake1, 1000
        );

        auto result2 = gov2.submit_proposal(
            ParameterType::BlockSize, param_value, "Test", proposer,
            backers, stakes2, total_stake2, 1000
        );

        bool expected_accept = backing_fraction >= THRESHOLD;

        ASSERT_EQ(result1.has_value(), expected_accept)
            << "Proposal 1 outcome mismatch on trial " << trial;
        ASSERT_EQ(result2.has_value(), expected_accept)
            << "Proposal 2 outcome mismatch on trial " << trial;
        ASSERT_EQ(result1.has_value(), result2.has_value())
            << "Proposals with same backing fraction should have same outcome on trial " << trial
            << " (fraction=" << backing_fraction << ")";
    }
}
