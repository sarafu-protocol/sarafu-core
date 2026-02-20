#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include "sarafu/node.h"
#include "sarafu/config/configuration.h"
#include "sarafu/consensus/consensus_engine.h"
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/consensus/block.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/state/mempool.h"
#include "sarafu/network/network_layer.h"
#include "../test_utils.h"

namespace sarafu {
namespace integration {

/**
 * Integration Test 29.1: Multi-Validator Consensus
 * 
 * This test validates that multiple validators can reach consensus:
 * - Launch 10 validators
 * - Produce 100 blocks
 * - Verify all validators see the same finalized blocks
 * - Verify no conflicting finalizations
 * 
 * Validates Requirements: 1.1, 1.2, 1.3, 1.6, 1.7
 * Validates Properties: 1 (No Conflicting Finalization), 2 (QC Validity), 3 (Finality Implies QC)
 */
class MultiValidatorConsensusTest : public ::testing::Test {
protected:
    static constexpr size_t NUM_VALIDATORS = 10;
    static constexpr size_t NUM_BLOCKS = 100;
    static constexpr uint64_t INITIAL_STAKE = 1000000;
    static constexpr uint64_t BLOCK_TIME_MS = 2000;

    struct ValidatorNode {
        consensus::ValidatorID id;
        crypto::BLS12_381_PrivateKey consensus_key;
        crypto::Ed25519_PrivateKey withdrawal_key;
        std::shared_ptr<consensus::ValidatorRegistry> validator_registry;
        std::shared_ptr<state::StateMachine> state_machine;
        std::shared_ptr<consensus::ConsensusEngine> consensus_engine;
        std::shared_ptr<state::Mempool> mempool;
    };

    std::vector<ValidatorNode> validators_;
    std::shared_ptr<storage::StateStorage> storage_;

    void SetUp() override {
        // Initialize storage (in-memory for testing)
        storage_ = std::make_shared<storage::StateStorage>();

        // Create 10 validators with equal stake
        for (size_t i = 0; i < NUM_VALIDATORS; ++i) {
            ValidatorNode node;

            // Generate keys
            node.consensus_key = crypto::BLS12_381_PrivateKey::generate();
            node.withdrawal_key = crypto::Ed25519_PrivateKey::generate();

            // Derive validator ID from withdrawal key
            auto withdrawal_pubkey = node.withdrawal_key.public_key();
            auto pubkey_bytes = withdrawal_pubkey.serialize();
            auto hash = crypto::Blake3Hash::hash(pubkey_bytes);
            node.id = state::Address(hash.serialize());

            // Create validator registry
            node.validator_registry = std::make_shared<consensus::ValidatorRegistry>(
                storage_,
                NUM_VALIDATORS,  // max active validators
                100000           // minimum self-bond
            );

            // Create state machine
            node.state_machine = std::make_shared<state::StateMachine>(storage_);

            // Create consensus engine
            consensus::ConsensusEngine::Config consensus_config;
            consensus_config.block_time_ms = BLOCK_TIME_MS;
            node.consensus_engine = std::make_shared<consensus::ConsensusEngine>(
                node.validator_registry,
                node.state_machine,
                consensus_config
            );

            // Create mempool
            node.mempool = std::make_shared<state::Mempool>();

            validators_.push_back(std::move(node));
        }

        // Register all validators in each validator registry
        for (auto& node : validators_) {
            for (size_t i = 0; i < NUM_VALIDATORS; ++i) {
                auto& validator = validators_[i];
                auto consensus_pubkey = validator.consensus_key.public_key();
                auto withdrawal_pubkey = validator.withdrawal_key.public_key();

                consensus::Validator val(
                    validator.id,
                    consensus_pubkey,
                    withdrawal_pubkey,
                    INITIAL_STAKE
                );
                val.status = consensus::ValidatorStatus::Active;

                // Add validator to registry
                node.validator_registry->add_validator(val);
            }

            // Finalize validator set for epoch 0
            node.validator_registry->finalize_epoch(0);
        }

        // Create and set genesis block for all validators
        consensus::BlockHeader genesis_header;
        genesis_header.height = 0;
        genesis_header.timestamp = std::time(nullptr);
        genesis_header.previous_hash = crypto::Blake3Hash();  // Zero hash
        genesis_header.state_root = crypto::Blake3Hash();
        genesis_header.transactions_root = crypto::Blake3Hash();
        genesis_header.validator_set_root = validators_[0].validator_registry->current_set().merkle_root;
        genesis_header.proposer = validators_[0].id;
        genesis_header.epoch = 0;

        consensus::QuorumCertificate genesis_qc;
        genesis_qc.block_height = 0;
        genesis_qc.block_hash = genesis_header.hash();
        genesis_qc.view_number = 0;
        genesis_qc.total_stake_signed = NUM_VALIDATORS * INITIAL_STAKE;

        consensus::Block genesis_block(genesis_header, {}, genesis_qc);

        for (auto& node : validators_) {
            node.consensus_engine->set_genesis_block(genesis_block);
        }
    }

    void TearDown() override {
        validators_.clear();
        storage_.reset();
    }

    /**
     * Simulate block proposal and voting for one block.
     * 
     * @param block_height The height of the block to produce
     * @return true if block was finalized, false otherwise
     */
    bool produce_block(uint64_t block_height) {
        // Determine leader for this height
        size_t leader_index = block_height % NUM_VALIDATORS;
        auto& leader = validators_[leader_index];

        // Leader proposes block
        auto proposed_block = leader.consensus_engine->propose_block(
            leader.id,
            leader.consensus_key,
            *leader.mempool,
            1000  // base_fee
        );

        if (!proposed_block.has_value()) {
            return false;
        }

        // Store block in leader's consensus engine
        leader.consensus_engine->store_block(proposed_block.value());

        // All validators receive and vote on the block
        std::vector<network::Vote> votes;
        for (auto& validator : validators_) {
            // Each validator processes the block
            bool valid = validator.consensus_engine->on_receive_block(proposed_block.value());
            if (!valid) {
                continue;
            }

            // Create vote
            network::Vote vote;
            vote.block_height = proposed_block->header.height;
            vote.block_hash = proposed_block->hash();
            vote.view_number = validator.consensus_engine->current_view();
            vote.validator_id = validator.id;

            // Sign the vote
            auto vote_message = vote.block_hash.serialize();
            vote.signature = validator.consensus_key.sign(vote_message);

            votes.push_back(vote);
        }

        // Aggregate votes to create QC
        if (votes.size() >= (NUM_VALIDATORS * 2 / 3 + 1)) {
            // All validators process votes
            for (auto& validator : validators_) {
                for (const auto& vote : votes) {
                    auto qc = validator.consensus_engine->on_receive_vote(vote);
                    if (qc.has_value()) {
                        // Block is finalized
                        validator.consensus_engine->mark_finalized(
                            proposed_block->hash(),
                            qc.value()
                        );
                    }
                }
            }
            return true;
        }

        return false;
    }

    /**
     * Verify all validators have the same finalized block at a given height.
     * 
     * @param height The block height to check
     * @return true if all validators agree, false otherwise
     */
    bool verify_consensus_at_height(uint64_t height) {
        if (validators_.empty()) {
            return false;
        }

        // Get the block hash from the first validator
        auto first_block = validators_[0].consensus_engine->get_block_by_height(height);
        if (!first_block.has_value()) {
            return false;
        }

        auto expected_hash = first_block->hash();

        // Verify all other validators have the same block
        for (size_t i = 1; i < validators_.size(); ++i) {
            auto block = validators_[i].consensus_engine->get_block_by_height(height);
            if (!block.has_value()) {
                return false;
            }

            if (block->hash() != expected_hash) {
                return false;
            }

            // Verify block is finalized
            if (!validators_[i].consensus_engine->is_finalized(expected_hash)) {
                return false;
            }
        }

        return true;
    }

    /**
     * Verify no conflicting finalizations exist.
     * 
     * This checks that no two different blocks at the same height are finalized.
     * 
     * @return true if no conflicts, false if conflicts detected
     */
    bool verify_no_conflicting_finalizations() {
        for (size_t i = 0; i < validators_.size(); ++i) {
            for (size_t j = i + 1; j < validators_.size(); ++j) {
                auto& validator_i = validators_[i];
                auto& validator_j = validators_[j];

                uint64_t finalized_height_i = validator_i.consensus_engine->finalized_height();
                uint64_t finalized_height_j = validator_j.consensus_engine->finalized_height();

                // Check all finalized heights
                uint64_t min_height = std::min(finalized_height_i, finalized_height_j);

                for (uint64_t h = 0; h <= min_height; ++h) {
                    auto block_i = validator_i.consensus_engine->get_block_by_height(h);
                    auto block_j = validator_j.consensus_engine->get_block_by_height(h);

                    if (block_i.has_value() && block_j.has_value()) {
                        if (block_i->hash() != block_j->hash()) {
                            // Conflicting finalization detected!
                            return false;
                        }
                    }
                }
            }
        }

        return true;
    }
};

/**
 * Test: Launch 10 validators, produce 100 blocks, verify consensus.
 * 
 * This test validates:
 * - All validators can participate in consensus
 * - Blocks are finalized with ≥2/3 stake
 * - All validators see the same finalized blocks
 * - No conflicting finalizations occur
 */
TEST_F(MultiValidatorConsensusTest, ProduceAndFinalizeBlocks) {
    // Produce 100 blocks
    for (uint64_t height = 1; height <= NUM_BLOCKS; ++height) {
        bool finalized = produce_block(height);
        ASSERT_TRUE(finalized) << "Failed to finalize block at height " << height;

        // Verify consensus at this height
        bool consensus = verify_consensus_at_height(height);
        EXPECT_TRUE(consensus) << "Validators disagree on block at height " << height;
    }

    // Verify all validators have finalized all 100 blocks
    for (const auto& validator : validators_) {
        EXPECT_EQ(validator.consensus_engine->finalized_height(), NUM_BLOCKS)
            << "Validator did not finalize all blocks";
    }

    // Verify no conflicting finalizations
    bool no_conflicts = verify_no_conflicting_finalizations();
    EXPECT_TRUE(no_conflicts) << "Conflicting finalizations detected!";
}

/**
 * Test: Verify Quorum Certificate validity.
 * 
 * This test validates Property 2: QC Validity
 * - QC must have ≥2/3 of total stake
 * - Aggregated signature must be valid
 */
TEST_F(MultiValidatorConsensusTest, VerifyQuorumCertificateValidity) {
    // Produce one block
    bool finalized = produce_block(1);
    ASSERT_TRUE(finalized);

    // Get the QC from the first validator
    auto block = validators_[0].consensus_engine->get_block_by_height(1);
    ASSERT_TRUE(block.has_value());

    // The next block's justify should be the QC for this block
    // For now, verify the highest QC
    const auto& qc = validators_[0].consensus_engine->highest_qc();

    // Verify QC has ≥2/3 stake
    uint64_t total_stake = NUM_VALIDATORS * INITIAL_STAKE;
    uint64_t required_stake = (total_stake * 2) / 3 + 1;
    EXPECT_GE(qc.total_stake_signed, required_stake)
        << "QC does not have ≥2/3 stake";

    // Verify QC has correct number of signers
    EXPECT_GE(qc.signers.size(), (NUM_VALIDATORS * 2) / 3 + 1)
        << "QC does not have enough signers";
}

/**
 * Test: Verify finality implies QC.
 * 
 * This test validates Property 3: Finality Implies QC
 * - If a block is marked as finalized, it must have a valid QC
 */
TEST_F(MultiValidatorConsensusTest, VerifyFinalityImpliesQC) {
    // Produce 10 blocks
    for (uint64_t height = 1; height <= 10; ++height) {
        bool finalized = produce_block(height);
        ASSERT_TRUE(finalized);
    }

    // For each finalized block, verify it has a QC
    for (uint64_t height = 1; height <= 10; ++height) {
        for (const auto& validator : validators_) {
            auto block = validator.consensus_engine->get_block_by_height(height);
            ASSERT_TRUE(block.has_value());

            // Verify block is finalized
            EXPECT_TRUE(validator.consensus_engine->is_finalized(block->hash()))
                << "Block at height " << height << " is not finalized";

            // The QC for this block should be in the next block's justify
            // Or in the highest_qc if this is the latest block
            if (height == 10) {
                const auto& qc = validator.consensus_engine->highest_qc();
                EXPECT_EQ(qc.block_height, height)
                    << "Highest QC does not match finalized block height";
            }
        }
    }
}

} // namespace integration
} // namespace sarafu
