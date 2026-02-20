#include "sarafu/state/monetary_policy_engine.h"
#include <gtest/gtest.h>
#include <random>
#include <cmath>

using namespace sarafu::state;

/**
 * Property-Based Test for Proportional Reward Distribution
 * 
 * **Validates: Requirements 11.3**
 * 
 * Property 33: Proportional Reward Distribution
 * For any epoch, validator rewards are distributed proportional to each validator's stake
 * in the active set.
 * 
 * This test validates that:
 * 1. Rewards are proportional to validator stake
 * 2. Total distributed rewards equal total issued tokens
 * 3. Jailed validators receive no rewards
 * 4. Rewards are credited to withdrawal addresses
 * 5. Distribution is fair and deterministic
 */
class ProportionalRewardDistributionPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random supply (1M to 1B tokens)
    uint64_t generate_random_supply() {
        std::uniform_int_distribution<uint64_t> dist(1000000, 1000000000);
        return dist(rng_);
    }

    // Generate random stake (1000 to max)
    uint64_t generate_random_stake(uint64_t max_stake) {
        std::uniform_int_distribution<uint64_t> dist(1000, max_stake);
        return dist(rng_);
    }

    // Generate random issuance coefficient (0.05 to 0.2)
    double generate_random_k() {
        std::uniform_real_distribution<double> dist(0.05, 0.2);
        return dist(rng_);
    }

    std::mt19937 rng_;
};

/**
 * Property: Rewards are proportional to validator stake
 * 
 * For any validator with stake S in total stake Stotal,
 * the validator receives (S / Stotal) * total_rewards.
 */
TEST_F(ProportionalRewardDistributionPropertyTest, RewardsProportionalToStake) {
    const int NUM_TRIALS = 500;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        double k = generate_random_k();

        // Create multiple validators
        std::uniform_int_distribution<int> num_validators_dist(3, 10);
        int num_validators = num_validators_dist(rng_);

        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators; // Empty - no jailed validators

        uint64_t total_stake = 0;
        for (int i = 0; i < num_validators; ++i) {
            std::string validator_id = "validator_" + std::to_string(i);
            std::string withdrawal_addr = "withdrawal_" + std::to_string(i);
            
            uint64_t stake = generate_random_stake(supply / num_validators);
            validator_stakes[validator_id] = stake;
            withdrawal_addresses[validator_id] = withdrawal_addr;
            total_stake += stake;
        }

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Simulate one epoch of issuance
        for (uint64_t block = 0; block < 10000; ++block) {
            engine.update_supply(0, BLOCKS_PER_YEAR);
        }

        uint64_t total_rewards = engine.state().tokens_issued_this_epoch;

        // Distribute rewards
        auto rewards = engine.distribute_rewards(
            validator_stakes,
            jailed_validators,
            withdrawal_addresses
        );

        // Verify proportional distribution
        for (const auto& [validator_id, stake] : validator_stakes) {
            std::string withdrawal_addr = withdrawal_addresses[validator_id];
            uint64_t reward = rewards[withdrawal_addr];

            double expected_proportion = static_cast<double>(stake) / static_cast<double>(total_stake);
            uint64_t expected_reward = static_cast<uint64_t>(std::round(expected_proportion * static_cast<double>(total_rewards)));

            ASSERT_EQ(reward, expected_reward)
                << "Reward not proportional to stake for " << validator_id << " on trial " << trial
                << " (stake=" << stake << ", total_stake=" << total_stake
                << ", expected=" << expected_reward << ", actual=" << reward << ")";
        }
    }
}

/**
 * Property: Total distributed rewards equal total issued tokens
 * 
 * The sum of all validator rewards equals the total tokens issued in the epoch.
 */
TEST_F(ProportionalRewardDistributionPropertyTest, TotalRewardsEqualTotalIssued) {
    const int NUM_TRIALS = 500;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        double k = generate_random_k();

        // Create multiple validators
        std::uniform_int_distribution<int> num_validators_dist(3, 10);
        int num_validators = num_validators_dist(rng_);

        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators;

        uint64_t total_stake = 0;
        for (int i = 0; i < num_validators; ++i) {
            std::string validator_id = "validator_" + std::to_string(i);
            std::string withdrawal_addr = "withdrawal_" + std::to_string(i);
            
            uint64_t stake = generate_random_stake(supply / num_validators);
            validator_stakes[validator_id] = stake;
            withdrawal_addresses[validator_id] = withdrawal_addr;
            total_stake += stake;
        }

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Simulate one epoch
        for (uint64_t block = 0; block < 10000; ++block) {
            engine.update_supply(0, BLOCKS_PER_YEAR);
        }

        uint64_t total_issued = engine.state().tokens_issued_this_epoch;

        // Distribute rewards
        auto rewards = engine.distribute_rewards(
            validator_stakes,
            jailed_validators,
            withdrawal_addresses
        );

        // Sum all rewards
        uint64_t total_distributed = 0;
        for (const auto& [addr, reward] : rewards) {
            total_distributed += reward;
        }

        // Allow small rounding error (within 1% or 1 token)
        uint64_t tolerance = std::max(static_cast<uint64_t>(1), total_issued / 100);
        ASSERT_NEAR(total_distributed, total_issued, tolerance)
            << "Total distributed rewards do not equal total issued on trial " << trial
            << " (issued=" << total_issued << ", distributed=" << total_distributed << ")";
    }
}

/**
 * Property: Jailed validators receive no rewards
 * 
 * For any validator that is jailed or slashed, they receive zero rewards.
 */
TEST_F(ProportionalRewardDistributionPropertyTest, JailedValidatorsReceiveNoRewards) {
    const int NUM_TRIALS = 500;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        double k = generate_random_k();

        // Create validators
        std::uniform_int_distribution<int> num_validators_dist(5, 10);
        int num_validators = num_validators_dist(rng_);

        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators;

        uint64_t total_stake = 0;
        for (int i = 0; i < num_validators; ++i) {
            std::string validator_id = "validator_" + std::to_string(i);
            std::string withdrawal_addr = "withdrawal_" + std::to_string(i);
            
            uint64_t stake = generate_random_stake(supply / num_validators);
            validator_stakes[validator_id] = stake;
            withdrawal_addresses[validator_id] = withdrawal_addr;
            total_stake += stake;

            // Jail some validators (30% chance)
            std::uniform_int_distribution<int> jail_dist(0, 9);
            if (jail_dist(rng_) < 3) {
                jailed_validators.insert(validator_id);
            }
        }

        // Skip if all validators are jailed
        if (jailed_validators.size() == validator_stakes.size()) {
            continue;
        }

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Simulate one epoch
        for (uint64_t block = 0; block < 10000; ++block) {
            engine.update_supply(0, BLOCKS_PER_YEAR);
        }

        // Distribute rewards
        auto rewards = engine.distribute_rewards(
            validator_stakes,
            jailed_validators,
            withdrawal_addresses
        );

        // Verify jailed validators receive no rewards
        for (const auto& validator_id : jailed_validators) {
            std::string withdrawal_addr = withdrawal_addresses[validator_id];
            uint64_t reward = rewards[withdrawal_addr];

            ASSERT_EQ(reward, 0)
                << "Jailed validator " << validator_id << " received rewards on trial " << trial;
        }
    }
}

/**
 * Property: Rewards are credited to withdrawal addresses
 * 
 * For any validator, rewards are credited to their withdrawal address,
 * not their validator ID.
 */
TEST_F(ProportionalRewardDistributionPropertyTest, RewardsCreditedToWithdrawalAddresses) {
    const int NUM_TRIALS = 500;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        double k = generate_random_k();

        // Create validators
        std::uniform_int_distribution<int> num_validators_dist(3, 10);
        int num_validators = num_validators_dist(rng_);

        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators;

        uint64_t total_stake = 0;
        for (int i = 0; i < num_validators; ++i) {
            std::string validator_id = "validator_" + std::to_string(i);
            std::string withdrawal_addr = "withdrawal_" + std::to_string(i);
            
            uint64_t stake = generate_random_stake(supply / num_validators);
            validator_stakes[validator_id] = stake;
            withdrawal_addresses[validator_id] = withdrawal_addr;
            total_stake += stake;
        }

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Simulate one epoch
        for (uint64_t block = 0; block < 10000; ++block) {
            engine.update_supply(0, BLOCKS_PER_YEAR);
        }

        // Distribute rewards
        auto rewards = engine.distribute_rewards(
            validator_stakes,
            jailed_validators,
            withdrawal_addresses
        );

        // Verify rewards are keyed by withdrawal address
        for (const auto& [validator_id, stake] : validator_stakes) {
            std::string withdrawal_addr = withdrawal_addresses[validator_id];

            // Reward should be in the map with withdrawal address as key
            ASSERT_TRUE(rewards.find(withdrawal_addr) != rewards.end())
                << "Reward not found for withdrawal address " << withdrawal_addr
                << " on trial " << trial;

            // Reward should not be in the map with validator ID as key
            ASSERT_TRUE(rewards.find(validator_id) == rewards.end())
                << "Reward incorrectly keyed by validator ID on trial " << trial;
        }
    }
}

/**
 * Property: Distribution is deterministic
 * 
 * For any set of validators and stakes, distribute_rewards produces
 * the same result every time.
 */
TEST_F(ProportionalRewardDistributionPropertyTest, DistributionIsDeterministic) {
    const int NUM_TRIALS = 200;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        double k = generate_random_k();

        // Create validators
        std::uniform_int_distribution<int> num_validators_dist(3, 10);
        int num_validators = num_validators_dist(rng_);

        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators;

        uint64_t total_stake = 0;
        for (int i = 0; i < num_validators; ++i) {
            std::string validator_id = "validator_" + std::to_string(i);
            std::string withdrawal_addr = "withdrawal_" + std::to_string(i);
            
            uint64_t stake = generate_random_stake(supply / num_validators);
            validator_stakes[validator_id] = stake;
            withdrawal_addresses[validator_id] = withdrawal_addr;
            total_stake += stake;
        }

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Simulate one epoch
        for (uint64_t block = 0; block < 10000; ++block) {
            engine.update_supply(0, BLOCKS_PER_YEAR);
        }

        // Distribute rewards three times
        auto rewards1 = engine.distribute_rewards(validator_stakes, jailed_validators, withdrawal_addresses);
        auto rewards2 = engine.distribute_rewards(validator_stakes, jailed_validators, withdrawal_addresses);
        auto rewards3 = engine.distribute_rewards(validator_stakes, jailed_validators, withdrawal_addresses);

        // All three should be identical
        ASSERT_EQ(rewards1, rewards2)
            << "Distribution not deterministic (run 1 != run 2) on trial " << trial;
        ASSERT_EQ(rewards2, rewards3)
            << "Distribution not deterministic (run 2 != run 3) on trial " << trial;
    }
}

/**
 * Property: Equal stakes receive equal rewards
 * 
 * For any two validators with equal stake, they receive equal rewards.
 */
TEST_F(ProportionalRewardDistributionPropertyTest, EqualStakesReceiveEqualRewards) {
    const int NUM_TRIALS = 500;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        double k = generate_random_k();

        // Create validators with equal stakes
        uint64_t equal_stake = generate_random_stake(supply / 10);

        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators;

        std::uniform_int_distribution<int> num_validators_dist(3, 10);
        int num_validators = num_validators_dist(rng_);

        uint64_t total_stake = 0;
        for (int i = 0; i < num_validators; ++i) {
            std::string validator_id = "validator_" + std::to_string(i);
            std::string withdrawal_addr = "withdrawal_" + std::to_string(i);
            
            validator_stakes[validator_id] = equal_stake;
            withdrawal_addresses[validator_id] = withdrawal_addr;
            total_stake += equal_stake;
        }

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Simulate one epoch
        for (uint64_t block = 0; block < 10000; ++block) {
            engine.update_supply(0, BLOCKS_PER_YEAR);
        }

        // Distribute rewards
        auto rewards = engine.distribute_rewards(
            validator_stakes,
            jailed_validators,
            withdrawal_addresses
        );

        // All validators should receive equal rewards
        std::vector<uint64_t> reward_amounts;
        for (const auto& [validator_id, stake] : validator_stakes) {
            std::string withdrawal_addr = withdrawal_addresses[validator_id];
            reward_amounts.push_back(rewards[withdrawal_addr]);
        }

        for (size_t i = 1; i < reward_amounts.size(); ++i) {
            ASSERT_EQ(reward_amounts[i], reward_amounts[0])
                << "Equal stakes did not receive equal rewards on trial " << trial
                << " (reward[0]=" << reward_amounts[0] << ", reward[" << i << "]=" << reward_amounts[i] << ")";
        }
    }
}

/**
 * Property: Validator with 100% stake receives 100% rewards
 * 
 * For a single validator with all the stake, they receive all rewards.
 */
TEST_F(ProportionalRewardDistributionPropertyTest, SingleValidatorReceivesAllRewards) {
    const int NUM_TRIALS = 200;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = generate_random_stake(supply);
        double k = generate_random_k();

        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators;

        std::string validator_id = "validator_0";
        std::string withdrawal_addr = "withdrawal_0";
        
        validator_stakes[validator_id] = stake;
        withdrawal_addresses[validator_id] = withdrawal_addr;

        MonetaryPolicyEngine engine(supply, stake);
        engine.set_issuance_coefficient(k);

        // Simulate one epoch
        for (uint64_t block = 0; block < 10000; ++block) {
            engine.update_supply(0, BLOCKS_PER_YEAR);
        }

        uint64_t total_issued = engine.state().tokens_issued_this_epoch;

        // Distribute rewards
        auto rewards = engine.distribute_rewards(
            validator_stakes,
            jailed_validators,
            withdrawal_addresses
        );

        uint64_t validator_reward = rewards[withdrawal_addr];

        // Single validator should receive all rewards (within rounding tolerance)
        uint64_t tolerance = std::max(static_cast<uint64_t>(1), total_issued / 100);
        ASSERT_NEAR(validator_reward, total_issued, tolerance)
            << "Single validator did not receive all rewards on trial " << trial
            << " (issued=" << total_issued << ", reward=" << validator_reward << ")";
    }
}

/**
 * Property: Multiple withdrawal addresses can share rewards
 * 
 * Multiple validators can have the same withdrawal address,
 * and rewards accumulate correctly.
 */
TEST_F(ProportionalRewardDistributionPropertyTest, SharedWithdrawalAddressesAccumulate) {
    const int NUM_TRIALS = 200;
    const uint64_t BLOCKS_PER_YEAR = 15768000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        double k = generate_random_k();

        // Create validators with shared withdrawal address
        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators;

        std::string shared_withdrawal = "shared_withdrawal";
        uint64_t total_stake = 0;

        // Create 3 validators sharing the same withdrawal address
        for (int i = 0; i < 3; ++i) {
            std::string validator_id = "validator_" + std::to_string(i);
            uint64_t stake = generate_random_stake(supply / 10);
            
            validator_stakes[validator_id] = stake;
            withdrawal_addresses[validator_id] = shared_withdrawal;
            total_stake += stake;
        }

        MonetaryPolicyEngine engine(supply, total_stake);
        engine.set_issuance_coefficient(k);

        // Simulate one epoch
        for (uint64_t block = 0; block < 10000; ++block) {
            engine.update_supply(0, BLOCKS_PER_YEAR);
        }

        uint64_t total_issued = engine.state().tokens_issued_this_epoch;

        // Distribute rewards
        auto rewards = engine.distribute_rewards(
            validator_stakes,
            jailed_validators,
            withdrawal_addresses
        );

        // Shared withdrawal address should receive sum of all validator rewards
        uint64_t shared_reward = rewards[shared_withdrawal];

        // Should equal total issued (all validators share same address)
        uint64_t tolerance = std::max(static_cast<uint64_t>(1), total_issued / 100);
        ASSERT_NEAR(shared_reward, total_issued, tolerance)
            << "Shared withdrawal address did not accumulate correctly on trial " << trial
            << " (issued=" << total_issued << ", shared_reward=" << shared_reward << ")";
    }
}

/**
 * Property: Zero total issued results in zero rewards
 * 
 * If no tokens were issued in the epoch, all validators receive zero rewards.
 */
TEST_F(ProportionalRewardDistributionPropertyTest, ZeroIssuedResultsInZeroRewards) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        uint64_t supply = generate_random_supply();
        uint64_t stake = 0; // Zero stake means zero issuance
        double k = generate_random_k();

        std::map<std::string, uint64_t> validator_stakes;
        std::map<std::string, std::string> withdrawal_addresses;
        std::set<std::string> jailed_validators;

        std::uniform_int_distribution<int> num_validators_dist(3, 10);
        int num_validators = num_validators_dist(rng_);

        for (int i = 0; i < num_validators; ++i) {
            std::string validator_id = "validator_" + std::to_string(i);
            std::string withdrawal_addr = "withdrawal_" + std::to_string(i);
            
            uint64_t val_stake = generate_random_stake(supply / num_validators);
            validator_stakes[validator_id] = val_stake;
            withdrawal_addresses[validator_id] = withdrawal_addr;
        }

        MonetaryPolicyEngine engine(supply, stake);
        engine.set_issuance_coefficient(k);

        // Don't simulate any blocks - tokens_issued_this_epoch = 0

        // Distribute rewards
        auto rewards = engine.distribute_rewards(
            validator_stakes,
            jailed_validators,
            withdrawal_addresses
        );

        // All validators should receive zero rewards
        for (const auto& [validator_id, val_stake] : validator_stakes) {
            std::string withdrawal_addr = withdrawal_addresses[validator_id];
            uint64_t reward = rewards[withdrawal_addr];

            ASSERT_EQ(reward, 0)
                << "Validator received non-zero reward with zero issuance on trial " << trial;
        }
    }
}
