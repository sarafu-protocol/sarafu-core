#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <cmath>
#include <filesystem>
#include <chrono>
#include "sarafu/consensus/slashing_detector.h"
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/storage/state_storage.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/blake3_hash.h"
#include "../test_utils.h"

namespace sarafu {
namespace integration {

/**
 * Integration Test 29.3: Slashing Integration
 * 
 * This test validates the complete slashing workflow:
 * - Trigger double-sign violation
 * - Verify penalty calculation (quadratic correlated slashing)
 * - Verify stake distribution (50% burn, 50% to validators)
 * - Verify tombstone marking
 * 
 * Validates Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7
 * Validates Properties: 9, 10, 11, 12, 13
 */
class SlashingIntegrationTest : public ::testing::Test {
protected:
    static constexpr size_t NUM_VALIDATORS = 10;
    static constexpr uint64_t INITIAL_STAKE = 1000000;
    static constexpr uint64_t INITIAL_SUPPLY = 100000000;
    static constexpr double ALPHA = 0.05;
    static constexpr double BETA = 0.5;

    struct TestValidator {
        consensus::ValidatorID id;
        crypto::BLS12_381_PrivateKey consensus_key;
        crypto::Ed25519_PrivateKey withdrawal_key;
        uint64_t stake;
    };

    std::shared_ptr<storage::StateStorage> storage_;
    std::shared_ptr<consensus::ValidatorRegistry> validator_registry_;
    std::unique_ptr<consensus::SlashingDetector> slashing_detector_;
    std::vector<TestValidator> test_validators_;
    uint64_t total_supply_;

    void SetUp() override {
        // Create temporary directory for test database
        auto unique_suffix = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        test_db_path_ = std::filesystem::temp_directory_path() /
                        ("sarafu_test_slashing_" + std::to_string(unique_suffix));
        std::filesystem::create_directories(test_db_path_);
        
        // Open database
        auto db_result = storage::Database::open(test_db_path_.string());
        ASSERT_TRUE(db_result.is_ok()) << "Failed to open database: " << db_result.error();
        auto db = std::move(db_result.value());
        
        storage_ = std::make_shared<storage::StateStorage>(std::move(db));
        
        // Configure validator registry
        consensus::ValidatorRegistry::Config config;
        config.active_validator_count = NUM_VALIDATORS;
        config.minimum_self_bond = 100000;
        
        validator_registry_ = std::make_shared<consensus::ValidatorRegistry>(config);

        consensus::SlashingDetector::Config slashing_config;
        slashing_config.alpha = ALPHA;
        slashing_config.beta = BETA;
        slashing_detector_ = std::make_unique<consensus::SlashingDetector>(slashing_config);

        total_supply_ = INITIAL_SUPPLY;

        // Create validators with equal stake
        for (size_t i = 0; i < NUM_VALIDATORS; ++i) {
            TestValidator tv;
            auto bls_keypair = crypto::BLS12_381::generate_keypair();
            tv.consensus_key = bls_keypair.second;
            auto ed_keypair = crypto::Ed25519::generate_keypair();
            tv.withdrawal_key = ed_keypair.second;
            tv.stake = INITIAL_STAKE;

            // Derive validator ID
            auto withdrawal_pubkey = tv.withdrawal_key.public_key();
            auto pubkey_bytes = withdrawal_pubkey.serialize();
            auto hash = crypto::Blake3Hash::hash(pubkey_bytes);
            tv.id = state::Address(hash.serialize());

            test_validators_.push_back(tv);

            // Add validator to registry
            validator_registry_->add_validator(
                tv.id,
                tv.consensus_key.public_key(),
                tv.withdrawal_key.public_key(),
                tv.stake
            );
        }

        // Finalize epoch 0
        validator_registry_->transition_epoch(0, 0);
    }

    void TearDown() override {
        test_validators_.clear();
        slashing_detector_.reset();
        validator_registry_.reset();
        storage_.reset();
        
        // Clean up test database
        std::filesystem::remove_all(test_db_path_);
    }

    std::filesystem::path test_db_path_;

    /**
     * Create a signature record for a validator signing a block.
     */
    consensus::SignatureRecord create_signature_record(
        const TestValidator& validator,
        uint64_t block_height,
        const crypto::Blake3Hash& block_hash
    ) {
        auto message = block_hash.serialize();
        auto signature = crypto::BLS12_381::sign(message, validator.consensus_key);

        return consensus::SignatureRecord(
            validator.id,
            block_height,
            block_hash,
            signature,
            std::time(nullptr)
        );
    }

    /**
     * Calculate expected penalty using quadratic correlated slashing formula.
     */
    uint64_t calculate_expected_penalty(
        uint64_t validator_stake,
        uint64_t total_stake,
        uint64_t violating_stake
    ) const {
        double penalty_fraction = ALPHA * static_cast<double>(validator_stake) / total_stake;
        if (total_stake > 0) {
            double ratio = static_cast<double>(violating_stake) / total_stake;
            penalty_fraction += BETA * ratio * ratio;
        }

        penalty_fraction = std::min(1.0, penalty_fraction);
        return static_cast<uint64_t>(penalty_fraction * validator_stake);
    }
};

/**
 * Test: Detect double-sign violation.
 * 
 * Validates Property 9: Double-Sign Detection
 * - Validator signs two different blocks at the same height
 * - System detects the violation
 */
TEST_F(SlashingIntegrationTest, DetectDoubleSign) {
    auto& violator = test_validators_[0];

    // Create two different blocks at the same height
    crypto::Blake3Hash block_hash_1 = crypto::Blake3Hash::hash(std::vector<uint8_t>{'A'});
    crypto::Blake3Hash block_hash_2 = crypto::Blake3Hash::hash(std::vector<uint8_t>{'B'});

    // Validator signs both blocks (double-sign violation)
    auto sig1 = create_signature_record(violator, 100, block_hash_1);
    auto sig2 = create_signature_record(violator, 100, block_hash_2);

    // Detect double-sign
    auto violation = slashing_detector_->detect_double_sign(sig1, sig2);

    ASSERT_TRUE(violation.has_value()) << "Double-sign violation not detected";
    EXPECT_EQ(violation->validator_id, violator.id);
    EXPECT_EQ(violation->block_height, 100);
    EXPECT_EQ(violation->reason, consensus::SlashReason::DoubleSign);
}

/**
 * Test: No false positive for same block signature.
 * 
 * Validates that signing the same block twice is not a violation.
 */
TEST_F(SlashingIntegrationTest, NoFalsePositiveForSameBlock) {
    auto& validator = test_validators_[0];

    // Create one block
    crypto::Blake3Hash block_hash = crypto::Blake3Hash::hash(std::vector<uint8_t>{'A'});

    // Validator signs the same block twice (not a violation)
    auto sig1 = create_signature_record(validator, 100, block_hash);
    auto sig2 = create_signature_record(validator, 100, block_hash);

    // Should not detect violation
    auto violation = slashing_detector_->detect_double_sign(sig1, sig2);

    EXPECT_FALSE(violation.has_value())
        << "False positive: same block signature detected as double-sign";
}

/**
 * Test: Calculate quadratic correlated slashing penalty.
 * 
 * Validates Property 11: Quadratic Slashing Calculation
 * - Penalty follows formula: min(1.0, α·si/Stotal + β·(Sviolating/Stotal)^2)·si
 */
TEST_F(SlashingIntegrationTest, CalculateQuadraticPenalty) {
    uint64_t total_stake = NUM_VALIDATORS * INITIAL_STAKE;

    // Test case 1: Single violator (no correlation)
    {
        uint64_t validator_stake = INITIAL_STAKE;
        std::vector<uint64_t> co_violator_stakes = {validator_stake};

        uint64_t penalty = slashing_detector_->calculate_penalty(
            validator_stake,
            total_stake,
            co_violator_stakes
        );

        uint64_t expected = calculate_expected_penalty(
            validator_stake,
            total_stake,
            validator_stake
        );

        EXPECT_EQ(penalty, expected)
            << "Single violator penalty calculation incorrect";
    }

    // Test case 2: Multiple violators (33% of stake)
    {
        uint64_t validator_stake = INITIAL_STAKE;
        size_t num_violators = 3;  // 30% of validators
        uint64_t violating_stake = num_violators * INITIAL_STAKE;

        std::vector<uint64_t> co_violator_stakes;
        for (size_t i = 0; i < num_violators; ++i) {
            co_violator_stakes.push_back(INITIAL_STAKE);
        }

        uint64_t penalty = slashing_detector_->calculate_penalty(
            validator_stake,
            total_stake,
            co_violator_stakes
        );

        uint64_t expected = calculate_expected_penalty(
            validator_stake,
            total_stake,
            violating_stake
        );

        uint64_t diff = (penalty > expected) ? (penalty - expected) : (expected - penalty);
        EXPECT_LE(diff, 1u)
            << "Correlated slashing penalty calculation incorrect";

        // Verify penalty is higher than single violator case
        uint64_t single_violator_penalty = calculate_expected_penalty(
            validator_stake,
            total_stake,
            validator_stake
        );

        EXPECT_GT(penalty, single_violator_penalty)
            << "Correlated slashing penalty should be higher than single violator";
    }

    // Test case 3: Large cartel (40% of stake)
    {
        uint64_t validator_stake = INITIAL_STAKE;
        size_t num_violators = 4;  // 40% of validators
        uint64_t violating_stake = num_violators * INITIAL_STAKE;

        std::vector<uint64_t> co_violator_stakes;
        for (size_t i = 0; i < num_violators; ++i) {
            co_violator_stakes.push_back(INITIAL_STAKE);
        }

        uint64_t penalty = slashing_detector_->calculate_penalty(
            validator_stake,
            total_stake,
            co_violator_stakes
        );

        uint64_t expected = calculate_expected_penalty(
            validator_stake,
            total_stake,
            violating_stake
        );

        EXPECT_EQ(penalty, expected)
            << "Large cartel penalty calculation incorrect";

        // Expected penalty fraction: 0.05 * (si/S) + 0.5 * (0.4)^2
        double expected_fraction = ALPHA * (static_cast<double>(validator_stake) / total_stake) +
                                   BETA * 0.4 * 0.4;
        EXPECT_NEAR(static_cast<double>(penalty) / validator_stake, expected_fraction, 0.01)
            << "Large cartel penalty fraction incorrect";
    }
}

/**
 * Test: Apply slashing and verify stake distribution.
 * 
 * Validates Property 12: Slashing Distribution
 * - 50% of slashed stake is burned
 * - 50% is distributed to active validators proportional to stake
 */
TEST_F(SlashingIntegrationTest, ApplySlashingAndVerifyDistribution) {
    auto& violator = test_validators_[0];
    uint64_t total_stake = NUM_VALIDATORS * INITIAL_STAKE;

    // Create double-sign violation
    crypto::Blake3Hash block_hash_1 = crypto::Blake3Hash::hash(std::vector<uint8_t>{'A'});
    crypto::Blake3Hash block_hash_2 = crypto::Blake3Hash::hash(std::vector<uint8_t>{'B'});

    auto sig1 = create_signature_record(violator, 100, block_hash_1);
    auto sig2 = create_signature_record(violator, 100, block_hash_2);

    auto violation = slashing_detector_->detect_double_sign(sig1, sig2);
    ASSERT_TRUE(violation.has_value());

    // Calculate penalty
    std::vector<uint64_t> co_violator_stakes = {violator.stake};
    uint64_t penalty = slashing_detector_->calculate_penalty(
        violator.stake,
        total_stake,
        co_violator_stakes
    );

    // Create slashing event
    consensus::SlashingEvent event(
        violator.id,
        100,
        consensus::SlashReason::DoubleSign,
        penalty,
        {violator.id}
    );

    // Record initial state
    uint64_t initial_supply = total_supply_;
    uint64_t initial_violator_stake = violator.stake;

    // Get initial stakes of other validators
    std::map<consensus::ValidatorID, uint64_t> initial_stakes;
    for (size_t i = 1; i < test_validators_.size(); ++i) {
        auto validator = validator_registry_->get_validator(test_validators_[i].id);
        ASSERT_TRUE(validator.has_value());
        initial_stakes[test_validators_[i].id] = validator->bonded_stake;
    }

    // Apply slashing
    bool applied = slashing_detector_->apply_slashing(
        *validator_registry_,
        event,
        total_supply_
    );

    ASSERT_TRUE(applied) << "Failed to apply slashing";

    // Verify violator's stake was reduced
    auto slashed_validator = validator_registry_->get_validator(violator.id);
    ASSERT_TRUE(slashed_validator.has_value());
    EXPECT_EQ(slashed_validator->bonded_stake, initial_violator_stake - penalty)
        << "Violator's stake not reduced correctly";

    // Verify 50% was burned (supply decreased)
    uint64_t burned_amount = penalty / 2;
    EXPECT_EQ(total_supply_, initial_supply - burned_amount)
        << "Burned amount incorrect";

    // Verify 50% was distributed to other validators
    uint64_t distributed_amount = penalty / 2;
    uint64_t total_distributed = 0;

    for (size_t i = 1; i < test_validators_.size(); ++i) {
        auto validator = validator_registry_->get_validator(test_validators_[i].id);
        ASSERT_TRUE(validator.has_value());

        uint64_t initial_stake = initial_stakes[test_validators_[i].id];
        uint64_t reward = validator->bonded_stake - initial_stake;
        total_distributed += reward;

        // Verify reward is proportional to stake
        // Each validator has equal stake, so should receive equal reward
        uint64_t expected_reward = distributed_amount / (NUM_VALIDATORS - 1);
        uint64_t remainder = distributed_amount % (NUM_VALIDATORS - 1);
        bool reward_ok = (reward == expected_reward) ||
                         (reward == expected_reward + remainder);
        EXPECT_TRUE(reward_ok)
            << "Validator reward not proportional to stake";
    }

    EXPECT_EQ(total_distributed, distributed_amount)
        << "Total distributed amount incorrect";
}

/**
 * Test: Verify tombstone marking for safety violations.
 * 
 * Validates Property 13: Tombstone Permanence
 * - Validator who commits safety violation is marked with tombstone status
 * - Tombstoned validator cannot rejoin validator set
 */
TEST_F(SlashingIntegrationTest, VerifyTombstoneMarking) {
    auto& violator = test_validators_[0];
    uint64_t total_stake = NUM_VALIDATORS * INITIAL_STAKE;

    // Create double-sign violation
    crypto::Blake3Hash block_hash_1 = crypto::Blake3Hash::hash(std::vector<uint8_t>{'A'});
    crypto::Blake3Hash block_hash_2 = crypto::Blake3Hash::hash(std::vector<uint8_t>{'B'});

    auto sig1 = create_signature_record(violator, 100, block_hash_1);
    auto sig2 = create_signature_record(violator, 100, block_hash_2);

    auto violation = slashing_detector_->detect_double_sign(sig1, sig2);
    ASSERT_TRUE(violation.has_value());

    // Calculate penalty
    std::vector<uint64_t> co_violator_stakes = {violator.stake};
    uint64_t penalty = slashing_detector_->calculate_penalty(
        violator.stake,
        total_stake,
        co_violator_stakes
    );

    // Create slashing event
    consensus::SlashingEvent event(
        violator.id,
        100,
        consensus::SlashReason::DoubleSign,
        penalty,
        {violator.id}
    );

    // Apply slashing
    bool applied = slashing_detector_->apply_slashing(
        *validator_registry_,
        event,
        total_supply_
    );

    ASSERT_TRUE(applied);

    // Verify validator is tombstoned
    auto slashed_validator = validator_registry_->get_validator(violator.id);
    ASSERT_TRUE(slashed_validator.has_value());
    EXPECT_EQ(slashed_validator->status, consensus::ValidatorStatus::Tombstoned)
        << "Validator not marked with tombstone status";

    // Verify tombstoned validator is not in active set
    const auto& validator_set = validator_registry_->current_set();
    EXPECT_FALSE(validator_set.is_active(violator.id))
        << "Tombstoned validator still in active set";
}

/**
 * Test: Multiple validators double-signing (correlated slashing).
 * 
 * Validates that when multiple validators commit the same violation,
 * the penalty increases due to correlation.
 */
TEST_F(SlashingIntegrationTest, CorrelatedSlashingMultipleViolators) {
    // Select 3 validators to double-sign (30% of stake)
    std::vector<size_t> violator_indices = {0, 1, 2};
    uint64_t total_stake = NUM_VALIDATORS * INITIAL_STAKE;
    uint64_t violating_stake = violator_indices.size() * INITIAL_STAKE;

    // Create two different blocks at the same height
    crypto::Blake3Hash block_hash_1 = crypto::Blake3Hash::hash(std::vector<uint8_t>{'A'});
    crypto::Blake3Hash block_hash_2 = crypto::Blake3Hash::hash(std::vector<uint8_t>{'B'});

    // All violators sign both blocks
    std::vector<consensus::SlashingEvent> events;
    for (size_t idx : violator_indices) {
        auto& violator = test_validators_[idx];

        auto sig1 = create_signature_record(violator, 100, block_hash_1);
        auto sig2 = create_signature_record(violator, 100, block_hash_2);

        auto violation = slashing_detector_->detect_double_sign(sig1, sig2);
        ASSERT_TRUE(violation.has_value());

        // Calculate penalty with correlation
        std::vector<uint64_t> co_violator_stakes;
        for (size_t co_idx : violator_indices) {
            co_violator_stakes.push_back(test_validators_[co_idx].stake);
        }

        uint64_t penalty = slashing_detector_->calculate_penalty(
            violator.stake,
            total_stake,
            co_violator_stakes
        );

        // Create slashing event
        std::vector<consensus::ValidatorID> co_violator_ids;
        for (size_t co_idx : violator_indices) {
            co_violator_ids.push_back(test_validators_[co_idx].id);
        }

        consensus::SlashingEvent event(
            violator.id,
            100,
            consensus::SlashReason::DoubleSign,
            penalty,
            co_violator_ids
        );

        events.push_back(event);
    }

    // Apply slashing to all violators
    for (const auto& event : events) {
        bool applied = slashing_detector_->apply_slashing(
            *validator_registry_,
            event,
            total_supply_
        );
        ASSERT_TRUE(applied);
    }

    // Verify all violators are tombstoned
    for (size_t idx : violator_indices) {
        auto validator = validator_registry_->get_validator(test_validators_[idx].id);
        ASSERT_TRUE(validator.has_value());
        EXPECT_EQ(validator->status, consensus::ValidatorStatus::Tombstoned)
            << "Violator " << idx << " not tombstoned";
    }

    // Verify penalties were higher due to correlation
    uint64_t expected_penalty = calculate_expected_penalty(
        INITIAL_STAKE,
        total_stake,
        violating_stake
    );

    for (const auto& event : events) {
        uint64_t diff = (event.penalty_amount > expected_penalty)
            ? (event.penalty_amount - expected_penalty)
            : (expected_penalty - event.penalty_amount);
        EXPECT_LE(diff, 1u)
            << "Correlated penalty calculation incorrect";
    }

    // Verify penalty is higher than single violator case
    uint64_t single_violator_penalty = calculate_expected_penalty(
        INITIAL_STAKE,
        total_stake,
        INITIAL_STAKE
    );

    EXPECT_GT(expected_penalty, single_violator_penalty)
        << "Correlated penalty not higher than single violator penalty";
}

/**
 * Test: Signature history tracking.
 * 
 * Validates that the slashing detector maintains signature history
 * for violation detection.
 */
TEST_F(SlashingIntegrationTest, SignatureHistoryTracking) {
    auto& validator = test_validators_[0];

    // Record multiple signatures
    for (uint64_t height = 1; height <= 10; ++height) {
        crypto::Blake3Hash block_hash = crypto::Blake3Hash::hash(std::vector<uint8_t>{static_cast<uint8_t>(height)});
        auto sig = create_signature_record(validator, height, block_hash);
        slashing_detector_->record_signature(sig);
    }

    // Get signature history
    auto history = slashing_detector_->get_signature_history(validator.id);

    EXPECT_EQ(history.size(), 10) << "Signature history size incorrect";

    // Verify signatures are in order
    for (size_t i = 0; i < history.size(); ++i) {
        EXPECT_EQ(history[i].block_height, i + 1)
            << "Signature history order incorrect";
    }
}

} // namespace integration
} // namespace sarafu
