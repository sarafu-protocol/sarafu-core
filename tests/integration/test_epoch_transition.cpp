#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <algorithm>
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/consensus/block.h"
#include "sarafu/storage/state_storage.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/blake3_hash.h"
#include "../test_utils.h"

namespace sarafu {
namespace integration {

/**
 * Integration Test 29.2: Epoch Transition
 * 
 * This test validates epoch transitions with validator set changes:
 * - Simulate epoch boundary (every 10,000 blocks)
 * - Verify new validator set is activated
 * - Verify epoch transition QC is valid
 * - Verify validator rankings are correct
 * 
 * Validates Requirements: 2.1, 2.2, 2.3, 2.6
 * Validates Properties: 4 (Epoch Determinism), 5 (Top-N Selection), 8 (Epoch Transition Authorization)
 */
class EpochTransitionTest : public ::testing::Test {
protected:
    static constexpr size_t MAX_ACTIVE_VALIDATORS = 10;
    static constexpr uint64_t MIN_SELF_BOND = 100000;
    static constexpr uint64_t BLOCKS_PER_EPOCH = 10000;

    std::shared_ptr<storage::StateStorage> storage_;
    std::shared_ptr<consensus::ValidatorRegistry> validator_registry_;

    struct TestValidator {
        consensus::ValidatorID id;
        crypto::BLS12_381_PrivateKey consensus_key;
        crypto::Ed25519_PrivateKey withdrawal_key;
        uint64_t stake;
    };

    std::vector<TestValidator> test_validators_;

    void SetUp() override {
        storage_ = std::make_shared<storage::StateStorage>();
        validator_registry_ = std::make_shared<consensus::ValidatorRegistry>(
            storage_,
            MAX_ACTIVE_VALIDATORS,
            MIN_SELF_BOND
        );

        // Create 15 validators with varying stakes
        // Top 10 should be active, bottom 5 should be standby
        std::vector<uint64_t> stakes = {
            1000000,  // Rank 1
            900000,   // Rank 2
            800000,   // Rank 3
            700000,   // Rank 4
            600000,   // Rank 5
            500000,   // Rank 6
            400000,   // Rank 7
            300000,   // Rank 8
            200000,   // Rank 9
            150000,   // Rank 10 (last active)
            140000,   // Rank 11 (first standby)
            130000,   // Rank 12
            120000,   // Rank 13
            110000,   // Rank 14
            100000    // Rank 15
        };

        for (size_t i = 0; i < stakes.size(); ++i) {
            TestValidator tv;
            tv.consensus_key = crypto::BLS12_381_PrivateKey::generate();
            tv.withdrawal_key = crypto::Ed25519_PrivateKey::generate();
            tv.stake = stakes[i];

            // Derive validator ID
            auto withdrawal_pubkey = tv.withdrawal_key.public_key();
            auto pubkey_bytes = withdrawal_pubkey.serialize();
            auto hash = crypto::Blake3Hash::hash(pubkey_bytes);
            tv.id = state::Address(hash.serialize());

            test_validators_.push_back(tv);

            // Add validator to registry
            consensus::Validator val(
                tv.id,
                tv.consensus_key.public_key(),
                tv.withdrawal_key.public_key(),
                tv.stake
            );

            validator_registry_->add_validator(val);
        }

        // Finalize epoch 0
        validator_registry_->finalize_epoch(0);
    }

    void TearDown() override {
        test_validators_.clear();
        validator_registry_.reset();
        storage_.reset();
    }

    /**
     * Calculate epoch number from block height.
     */
    uint64_t calculate_epoch(uint64_t block_height) const {
        return block_height / BLOCKS_PER_EPOCH;
    }

    /**
     * Simulate epoch transition by changing validator stakes and finalizing new epoch.
     */
    void simulate_epoch_transition(uint64_t new_epoch) {
        // Modify some validator stakes to trigger set changes
        if (new_epoch == 1) {
            // Promote validator at rank 11 to rank 5 by increasing stake
            auto& promoted = test_validators_[10];  // Originally rank 11
            promoted.stake = 650000;  // Now rank 5

            // Demote validator at rank 10 to rank 11 by decreasing stake
            auto& demoted = test_validators_[9];  // Originally rank 10
            demoted.stake = 135000;  // Now rank 11

            // Update stakes in registry
            validator_registry_->update_validator_stake(promoted.id, promoted.stake);
            validator_registry_->update_validator_stake(demoted.id, demoted.stake);
        }

        // Finalize the new epoch
        validator_registry_->finalize_epoch(new_epoch);
    }

    /**
     * Create a mock QC for epoch transition.
     */
    consensus::QuorumCertificate create_epoch_transition_qc(
        uint64_t epoch,
        const consensus::ValidatorSet& old_set
    ) {
        consensus::QuorumCertificate qc;
        qc.block_height = epoch * BLOCKS_PER_EPOCH;
        qc.block_hash = crypto::Blake3Hash();  // Mock hash
        qc.view_number = 0;

        // Collect signatures from ≥2/3 of old validator set
        auto active_validators = old_set.get_active_validators();
        uint64_t required_stake = (old_set.total_stake * 2) / 3 + 1;
        uint64_t accumulated_stake = 0;

        std::vector<crypto::BLS12_381_Signature> signatures;
        for (const auto& validator : active_validators) {
            // Find the test validator
            auto it = std::find_if(test_validators_.begin(), test_validators_.end(),
                [&](const TestValidator& tv) { return tv.id == validator.id; });

            if (it != test_validators_.end()) {
                // Sign the epoch transition
                auto message = qc.block_hash.serialize();
                auto signature = it->consensus_key.sign(message);
                signatures.push_back(signature);

                qc.signers.push_back(validator.id);
                accumulated_stake += validator.bonded_stake;

                if (accumulated_stake >= required_stake) {
                    break;
                }
            }
        }

        // Aggregate signatures
        if (!signatures.empty()) {
            qc.aggregated_signature = crypto::BLS12_381_Signature::aggregate(signatures);
        }
        qc.total_stake_signed = accumulated_stake;

        return qc;
    }
};

/**
 * Test: Epoch determinism - epoch number is calculated correctly from block height.
 * 
 * Validates Property 4: Epoch Determinism
 */
TEST_F(EpochTransitionTest, EpochDeterminism) {
    // Test epoch calculation
    EXPECT_EQ(calculate_epoch(0), 0);
    EXPECT_EQ(calculate_epoch(1), 0);
    EXPECT_EQ(calculate_epoch(9999), 0);
    EXPECT_EQ(calculate_epoch(10000), 1);
    EXPECT_EQ(calculate_epoch(10001), 1);
    EXPECT_EQ(calculate_epoch(19999), 1);
    EXPECT_EQ(calculate_epoch(20000), 2);
    EXPECT_EQ(calculate_epoch(100000), 10);
}

/**
 * Test: Top-N validator selection at epoch boundary.
 * 
 * Validates Property 5: Top-N Selection
 * - Active set contains exactly top N validators by stake
 * - Standby set contains validators ranked N+1 and below
 */
TEST_F(EpochTransitionTest, TopNSelection) {
    const auto& validator_set = validator_registry_->current_set();
    EXPECT_EQ(validator_set.epoch, 0);

    // Get active and standby validators
    auto active_validators = validator_set.get_active_validators();
    auto standby_validators = validator_set.get_standby_validators();

    // Verify exactly 10 active validators
    EXPECT_EQ(active_validators.size(), MAX_ACTIVE_VALIDATORS);

    // Verify 5 standby validators
    EXPECT_EQ(standby_validators.size(), 5);

    // Verify active validators are top 10 by stake
    std::vector<uint64_t> active_stakes;
    for (const auto& val : active_validators) {
        active_stakes.push_back(val.bonded_stake);
    }

    // Active stakes should be sorted descending
    EXPECT_TRUE(std::is_sorted(active_stakes.rbegin(), active_stakes.rend()));

    // Verify standby validators have lower stakes than all active validators
    uint64_t min_active_stake = *std::min_element(active_stakes.begin(), active_stakes.end());
    for (const auto& val : standby_validators) {
        EXPECT_LT(val.bonded_stake, min_active_stake)
            << "Standby validator has higher stake than active validator";
    }
}

/**
 * Test: Minimum stake enforcement.
 * 
 * Validates Property 6: Minimum Stake Enforcement
 * - All active validators must have stake ≥ minimum self-bond
 */
TEST_F(EpochTransitionTest, MinimumStakeEnforcement) {
    const auto& validator_set = validator_registry_->current_set();
    auto active_validators = validator_set.get_active_validators();

    for (const auto& validator : active_validators) {
        EXPECT_GE(validator.bonded_stake, MIN_SELF_BOND)
            << "Active validator has stake below minimum self-bond";
    }
}

/**
 * Test: Standby queue completeness.
 * 
 * Validates Property 7: Standby Queue Completeness
 * - Standby queue contains all validators ranked N+1 and below
 */
TEST_F(EpochTransitionTest, StandbyQueueCompleteness) {
    const auto& validator_set = validator_registry_->current_set();
    auto standby_validators = validator_set.get_standby_validators();

    // We created 15 validators, top 10 are active, bottom 5 are standby
    EXPECT_EQ(standby_validators.size(), 5);

    // Verify all standby validators meet minimum stake requirement
    for (const auto& validator : standby_validators) {
        EXPECT_GE(validator.bonded_stake, MIN_SELF_BOND)
            << "Standby validator does not meet minimum stake requirement";
    }
}

/**
 * Test: Epoch transition with validator set changes.
 * 
 * Validates:
 * - New validator set is activated at epoch boundary
 * - Validator rankings are updated correctly
 * - Promoted validators become active
 * - Demoted validators become standby
 */
TEST_F(EpochTransitionTest, EpochTransitionWithSetChanges) {
    // Get epoch 0 validator set
    const auto& epoch0_set = validator_registry_->current_set();
    EXPECT_EQ(epoch0_set.epoch, 0);

    auto epoch0_active = epoch0_set.get_active_validators();
    EXPECT_EQ(epoch0_active.size(), MAX_ACTIVE_VALIDATORS);

    // Record the validator at rank 10 (last active) and rank 11 (first standby)
    auto rank10_id = test_validators_[9].id;   // Originally rank 10
    auto rank11_id = test_validators_[10].id;  // Originally rank 11

    // Verify rank 10 is active and rank 11 is standby in epoch 0
    EXPECT_TRUE(epoch0_set.is_active(rank10_id));
    EXPECT_FALSE(epoch0_set.is_active(rank11_id));

    // Simulate epoch transition to epoch 1 with stake changes
    simulate_epoch_transition(1);

    // Get epoch 1 validator set
    const auto& epoch1_set = validator_registry_->current_set();
    EXPECT_EQ(epoch1_set.epoch, 1);

    auto epoch1_active = epoch1_set.get_active_validators();
    EXPECT_EQ(epoch1_active.size(), MAX_ACTIVE_VALIDATORS);

    // Verify rank 11 is now active (promoted) and rank 10 is now standby (demoted)
    EXPECT_TRUE(epoch1_set.is_active(rank11_id))
        << "Validator with increased stake was not promoted to active set";
    EXPECT_FALSE(epoch1_set.is_active(rank10_id))
        << "Validator with decreased stake was not demoted to standby";
}

/**
 * Test: Epoch transition QC validity.
 * 
 * Validates Property 8: Epoch Transition Authorization
 * - New validator set must be signed by ≥2/3 of previous epoch's stake
 */
TEST_F(EpochTransitionTest, EpochTransitionQCValidity) {
    // Get epoch 0 validator set
    const auto& epoch0_set = validator_registry_->current_set();

    // Create epoch transition QC from epoch 0 validators
    auto transition_qc = create_epoch_transition_qc(1, epoch0_set);

    // Verify QC has ≥2/3 of epoch 0 stake
    uint64_t required_stake = (epoch0_set.total_stake * 2) / 3 + 1;
    EXPECT_GE(transition_qc.total_stake_signed, required_stake)
        << "Epoch transition QC does not have ≥2/3 of previous epoch's stake";

    // Verify QC has correct block height (epoch boundary)
    EXPECT_EQ(transition_qc.block_height, BLOCKS_PER_EPOCH);

    // Verify QC has enough signers
    size_t required_signers = (MAX_ACTIVE_VALIDATORS * 2) / 3 + 1;
    EXPECT_GE(transition_qc.signers.size(), required_signers)
        << "Epoch transition QC does not have enough signers";

    // Verify all signers were in the previous epoch's active set
    for (const auto& signer_id : transition_qc.signers) {
        EXPECT_TRUE(epoch0_set.is_active(signer_id))
            << "QC signer was not in previous epoch's active set";
    }
}

/**
 * Test: Validator set root changes at epoch boundary.
 * 
 * Validates that the validator set Merkle root is updated when the set changes.
 */
TEST_F(EpochTransitionTest, ValidatorSetRootChanges) {
    // Get epoch 0 validator set root
    const auto& epoch0_set = validator_registry_->current_set();
    auto epoch0_root = epoch0_set.merkle_root;

    // Simulate epoch transition with set changes
    simulate_epoch_transition(1);

    // Get epoch 1 validator set root
    const auto& epoch1_set = validator_registry_->current_set();
    auto epoch1_root = epoch1_set.merkle_root;

    // Validator set root should change because the active set changed
    EXPECT_NE(epoch0_root, epoch1_root)
        << "Validator set root did not change despite set changes";
}

/**
 * Test: Total stake calculation at epoch boundary.
 * 
 * Validates that total stake is correctly calculated from active validators only.
 */
TEST_F(EpochTransitionTest, TotalStakeCalculation) {
    const auto& validator_set = validator_registry_->current_set();
    auto active_validators = validator_set.get_active_validators();

    // Calculate expected total stake
    uint64_t expected_total_stake = 0;
    for (const auto& validator : active_validators) {
        expected_total_stake += validator.bonded_stake;
    }

    // Verify total stake matches
    EXPECT_EQ(validator_set.total_stake, expected_total_stake)
        << "Total stake does not match sum of active validator stakes";
}

} // namespace integration
} // namespace sarafu
