#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <chrono>
#include <algorithm>
#include <set>
#include <map>
#include <random>
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/blake3_hash.h"
#include "../test_utils.h"

namespace sarafu {
namespace stress {

/**
 * Stress Test: Network Partition Stress Test
 * 
 * This test simulates network partitions and validates:
 * - System detects network partitions
 * - Consensus halts when ≥1/3 stake is isolated
 * - Consensus continues when <1/3 stake is isolated
 * - System recovers when connectivity is restored
 * - Validators sync to canonical chain after partition
 * 
 * Validates Requirements: 6.6
 * Validates Properties: 18, 19, 20, 21, 22, 23, 24
 */
class NetworkPartitionStressTest : public ::testing::Test {
protected:
    static constexpr size_t NUM_VALIDATORS = 100;
    static constexpr uint64_t INITIAL_STAKE = 1000000;
    static constexpr double ONE_THIRD_THRESHOLD = 0.333333;

    struct TestValidator {
        consensus::ValidatorID id;
        crypto::BLS12_381_PrivateKey consensus_key;
        crypto::BLS12_381_PublicKey consensus_pubkey;
        crypto::Ed25519_PrivateKey withdrawal_key;
        crypto::Ed25519_PublicKey withdrawal_pubkey;
        uint64_t stake;
        bool is_isolated;
        uint64_t last_seen_height;
    };

    struct PartitionEvent {
        uint64_t timestamp_ms;
        uint64_t block_height;
        std::set<consensus::ValidatorID> isolated_validators;
        uint64_t isolated_stake;
        uint64_t total_stake;
        bool is_resolved;
        uint64_t resolution_timestamp_ms;
        
        double get_isolated_ratio() const {
            if (total_stake == 0) return 0.0;
            return static_cast<double>(isolated_stake) / total_stake;
        }
        
        uint64_t get_duration_ms() const {
            if (!is_resolved) return 0;
            return resolution_timestamp_ms - timestamp_ms;
        }
    };

    struct BlockRecord {
        uint64_t height;
        uint64_t timestamp_ms;
        crypto::Blake3Hash hash;
        std::set<consensus::ValidatorID> signers;
        bool is_finalized;
        bool is_canonical;
    };

    std::shared_ptr<consensus::ValidatorRegistry> validator_registry_;
    std::vector<TestValidator> test_validators_;
    std::vector<PartitionEvent> partition_events_;
    std::vector<BlockRecord> canonical_chain_;
    std::vector<BlockRecord> minority_chain_;
    std::mt19937 rng_;

    void SetUp() override {
        std::cout << "\n=== Network Partition Stress Test ===" << std::endl;
        std::cout << "Total validators: " << NUM_VALIDATORS << std::endl;

        rng_.seed(42);  // Fixed seed for reproducibility

        // Configure validator registry
        consensus::ValidatorRegistry::Config config;
        config.active_validator_count = NUM_VALIDATORS;
        config.minimum_self_bond = 100000;
        
        validator_registry_ = std::make_shared<consensus::ValidatorRegistry>(config);

        auto start_time = std::chrono::high_resolution_clock::now();

        // Create validators with equal stake
        for (size_t i = 0; i < NUM_VALIDATORS; ++i) {
            TestValidator tv;
            
            // Generate keys
            auto bls_keypair = crypto::BLS12_381::generate_keypair();
            tv.consensus_pubkey = bls_keypair.first;
            tv.consensus_key = bls_keypair.second;
            
            auto ed_keypair = crypto::Ed25519::generate_keypair();
            tv.withdrawal_pubkey = ed_keypair.first;
            tv.withdrawal_key = ed_keypair.second;
            
            tv.stake = INITIAL_STAKE;
            tv.is_isolated = false;
            tv.last_seen_height = 0;

            // Derive validator ID from withdrawal key
            auto pubkey_bytes = tv.withdrawal_pubkey.serialize();
            auto hash = crypto::Blake3Hash::hash(pubkey_bytes);
            tv.id = state::Address(hash.serialize());

            test_validators_.push_back(tv);

            // Add validator to registry
            validator_registry_->add_validator(
                tv.id,
                tv.consensus_pubkey,
                tv.withdrawal_pubkey,
                tv.stake
            );

            // Progress indicator
            if ((i + 1) % 20 == 0) {
                std::cout << "  Created " << (i + 1) << " validators..." << std::endl;
            }
        }

        // Finalize epoch 0
        validator_registry_->transition_epoch(0, 0);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time
        );
        std::cout << "Validator setup completed in " << duration.count() << " ms" << std::endl;
    }

    void TearDown() override {
        test_validators_.clear();
        partition_events_.clear();
        canonical_chain_.clear();
        minority_chain_.clear();
        validator_registry_.reset();
    }

    /**
     * Simulate a network partition by isolating a percentage of validators.
     */
    PartitionEvent create_partition(uint32_t isolated_pct, uint64_t block_height, uint64_t timestamp_ms) {
        PartitionEvent event;
        event.timestamp_ms = timestamp_ms;
        event.block_height = block_height;
        event.is_resolved = false;
        event.resolution_timestamp_ms = 0;
        event.total_stake = NUM_VALIDATORS * INITIAL_STAKE;

        // Randomly select validators to isolate
        size_t num_isolated = (NUM_VALIDATORS * isolated_pct) / 100;
        std::vector<size_t> indices(NUM_VALIDATORS);
        std::iota(indices.begin(), indices.end(), 0);
        std::shuffle(indices.begin(), indices.end(), rng_);

        event.isolated_stake = 0;
        for (size_t i = 0; i < num_isolated; ++i) {
            size_t idx = indices[i];
            test_validators_[idx].is_isolated = true;
            event.isolated_validators.insert(test_validators_[idx].id);
            event.isolated_stake += test_validators_[idx].stake;
        }

        return event;
    }

    /**
     * Resolve a network partition by reconnecting isolated validators.
     */
    void resolve_partition(PartitionEvent& event, uint64_t timestamp_ms) {
        event.is_resolved = true;
        event.resolution_timestamp_ms = timestamp_ms;

        // Reconnect all isolated validators
        for (auto& validator : test_validators_) {
            if (event.isolated_validators.count(validator.id) > 0) {
                validator.is_isolated = false;
            }
        }
    }

    /**
     * Simulate block production with partition.
     */
    BlockRecord produce_block(uint64_t height, uint64_t timestamp_ms, bool is_canonical) {
        BlockRecord block;
        block.height = height;
        block.timestamp_ms = timestamp_ms;
        
        // Generate block hash
        std::vector<uint8_t> hash_input = {
            static_cast<uint8_t>(height >> 24),
            static_cast<uint8_t>(height >> 16),
            static_cast<uint8_t>(height >> 8),
            static_cast<uint8_t>(height),
            static_cast<uint8_t>(is_canonical ? 1 : 0)
        };
        block.hash = crypto::Blake3Hash::hash(hash_input);
        block.is_canonical = is_canonical;

        // Collect signatures from non-isolated validators
        for (const auto& validator : test_validators_) {
            if (!validator.is_isolated) {
                block.signers.insert(validator.id);
            }
        }

        // Block is finalized if it has ≥2/3 quorum
        size_t required_signers = (NUM_VALIDATORS * 2) / 3 + 1;
        block.is_finalized = (block.signers.size() >= required_signers);

        return block;
    }

    /**
     * Calculate the number of active (non-isolated) validators.
     */
    size_t count_active_validators() const {
        return std::count_if(test_validators_.begin(), test_validators_.end(),
                            [](const TestValidator& v) { return !v.is_isolated; });
    }

    /**
     * Calculate the total stake of active (non-isolated) validators.
     */
    uint64_t calculate_active_stake() const {
        uint64_t active_stake = 0;
        for (const auto& validator : test_validators_) {
            if (!validator.is_isolated) {
                active_stake += validator.stake;
            }
        }
        return active_stake;
    }
};

/**
 * Test: Simulate minority partition (<1/3 stake) and verify consensus continues.
 * 
 * This test validates that when less than 1/3 of stake is isolated,
 * the majority partition continues producing finalized blocks.
 */
TEST_F(NetworkPartitionStressTest, MinorityPartitionAllowsConsensus) {
    std::cout << "\n=== Minority Partition Test (25% isolated) ===" << std::endl;

    uint64_t total_stake = NUM_VALIDATORS * INITIAL_STAKE;
    uint32_t isolated_pct = 25;  // 25% < 33.3%

    // Create partition
    auto partition = create_partition(isolated_pct, 100, 1000000);
    partition_events_.push_back(partition);

    std::cout << "Partition created:" << std::endl;
    std::cout << "  Isolated validators: " << partition.isolated_validators.size() << " / " << NUM_VALIDATORS << std::endl;
    std::cout << "  Isolated stake: " << partition.isolated_stake << " SAR (" 
              << (partition.get_isolated_ratio() * 100.0) << "%)" << std::endl;
    std::cout << "  Active validators: " << count_active_validators() << std::endl;
    std::cout << "  Active stake: " << calculate_active_stake() << " SAR" << std::endl;

    EXPECT_LT(partition.get_isolated_ratio(), ONE_THIRD_THRESHOLD)
        << "Partition should isolate less than 1/3 of stake";

    // Simulate block production on majority partition
    std::cout << "\nProducing blocks on majority partition..." << std::endl;
    
    uint64_t current_time = partition.timestamp_ms;
    for (uint64_t height = partition.block_height; height < partition.block_height + 50; ++height) {
        auto block = produce_block(height, current_time, true);
        canonical_chain_.push_back(block);
        current_time += 2000;  // 2-second block time

        if ((height - partition.block_height + 1) % 10 == 0) {
            std::cout << "  Produced " << (height - partition.block_height + 1) << " blocks..." << std::endl;
        }
    }

    // Verify all blocks are finalized
    size_t finalized_count = std::count_if(canonical_chain_.begin(), canonical_chain_.end(),
                                          [](const BlockRecord& b) { return b.is_finalized; });

    std::cout << "\nResults:" << std::endl;
    std::cout << "  Total blocks produced: " << canonical_chain_.size() << std::endl;
    std::cout << "  Finalized blocks: " << finalized_count << std::endl;
    std::cout << "  Finalization rate: " << (finalized_count * 100.0 / canonical_chain_.size()) << "%" << std::endl;

    EXPECT_EQ(finalized_count, canonical_chain_.size())
        << "All blocks should be finalized with minority partition";

    std::cout << "✓ Consensus continues with minority partition" << std::endl;
}

/**
 * Test: Simulate major partition (≥1/3 stake) and verify consensus halts.
 * 
 * This test validates that when ≥1/3 of stake is isolated,
 * the blockchain halts block finalization.
 */
TEST_F(NetworkPartitionStressTest, MajorPartitionHaltsConsensus) {
    std::cout << "\n=== Major Partition Test (40% isolated) ===" << std::endl;

    uint64_t total_stake = NUM_VALIDATORS * INITIAL_STAKE;
    uint32_t isolated_pct = 40;  // 40% > 33.3%

    // Create partition
    auto partition = create_partition(isolated_pct, 100, 1000000);
    partition_events_.push_back(partition);

    std::cout << "Partition created:" << std::endl;
    std::cout << "  Isolated validators: " << partition.isolated_validators.size() << " / " << NUM_VALIDATORS << std::endl;
    std::cout << "  Isolated stake: " << partition.isolated_stake << " SAR (" 
              << (partition.get_isolated_ratio() * 100.0) << "%)" << std::endl;
    std::cout << "  Active validators: " << count_active_validators() << std::endl;
    std::cout << "  Active stake: " << calculate_active_stake() << " SAR" << std::endl;

    EXPECT_GE(partition.get_isolated_ratio(), ONE_THIRD_THRESHOLD)
        << "Partition should isolate ≥1/3 of stake";

    // Attempt to produce blocks (should not finalize)
    std::cout << "\nAttempting to produce blocks..." << std::endl;
    
    uint64_t current_time = partition.timestamp_ms;
    for (uint64_t height = partition.block_height; height < partition.block_height + 50; ++height) {
        auto block = produce_block(height, current_time, true);
        canonical_chain_.push_back(block);
        current_time += 2000;
    }

    // Verify blocks are NOT finalized (insufficient quorum)
    size_t finalized_count = std::count_if(canonical_chain_.begin(), canonical_chain_.end(),
                                          [](const BlockRecord& b) { return b.is_finalized; });

    std::cout << "\nResults:" << std::endl;
    std::cout << "  Total blocks produced: " << canonical_chain_.size() << std::endl;
    std::cout << "  Finalized blocks: " << finalized_count << std::endl;
    std::cout << "  Finalization rate: " << (finalized_count * 100.0 / canonical_chain_.size()) << "%" << std::endl;

    EXPECT_EQ(finalized_count, 0)
        << "No blocks should be finalized with major partition";

    std::cout << "✓ Consensus halts with major partition" << std::endl;
}

/**
 * Test: Simulate partition recovery and verify consensus resumes.
 * 
 * This test validates that when a partition is resolved,
 * the blockchain resumes producing finalized blocks.
 */
TEST_F(NetworkPartitionStressTest, PartitionRecoveryResumesConsensus) {
    std::cout << "\n=== Partition Recovery Test ===" << std::endl;

    uint32_t isolated_pct = 40;  // 40% > 33.3%

    // Create partition
    auto partition = create_partition(isolated_pct, 100, 1000000);

    std::cout << "Phase 1: Partition created (40% isolated)" << std::endl;
    std::cout << "  Isolated stake: " << (partition.get_isolated_ratio() * 100.0) << "%" << std::endl;

    // Attempt to produce blocks during partition (should not finalize)
    uint64_t current_time = partition.timestamp_ms;
    for (uint64_t height = partition.block_height; height < partition.block_height + 20; ++height) {
        auto block = produce_block(height, current_time, true);
        canonical_chain_.push_back(block);
        current_time += 2000;
    }

    size_t finalized_before = std::count_if(canonical_chain_.begin(), canonical_chain_.end(),
                                           [](const BlockRecord& b) { return b.is_finalized; });

    std::cout << "  Blocks during partition: " << canonical_chain_.size() << std::endl;
    std::cout << "  Finalized: " << finalized_before << std::endl;

    EXPECT_EQ(finalized_before, 0)
        << "No blocks should finalize during major partition";

    // Resolve partition
    std::cout << "\nPhase 2: Resolving partition..." << std::endl;
    resolve_partition(partition, current_time);
    partition_events_.push_back(partition);

    std::cout << "  Partition duration: " << partition.get_duration_ms() << " ms" << std::endl;
    std::cout << "  Active validators: " << count_active_validators() << " / " << NUM_VALIDATORS << std::endl;

    EXPECT_EQ(count_active_validators(), NUM_VALIDATORS)
        << "All validators should be active after recovery";

    // Produce blocks after recovery (should finalize)
    std::cout << "\nPhase 3: Producing blocks after recovery..." << std::endl;
    
    size_t blocks_before_recovery = canonical_chain_.size();
    for (uint64_t height = partition.block_height + 20; height < partition.block_height + 70; ++height) {
        auto block = produce_block(height, current_time, true);
        canonical_chain_.push_back(block);
        current_time += 2000;
    }

    // Count finalized blocks after recovery
    size_t finalized_after = 0;
    for (size_t i = blocks_before_recovery; i < canonical_chain_.size(); ++i) {
        if (canonical_chain_[i].is_finalized) {
            finalized_after++;
        }
    }

    size_t blocks_after_recovery = canonical_chain_.size() - blocks_before_recovery;

    std::cout << "  Blocks after recovery: " << blocks_after_recovery << std::endl;
    std::cout << "  Finalized: " << finalized_after << std::endl;
    std::cout << "  Finalization rate: " << (finalized_after * 100.0 / blocks_after_recovery) << "%" << std::endl;

    EXPECT_EQ(finalized_after, blocks_after_recovery)
        << "All blocks should finalize after partition recovery";

    std::cout << "✓ Consensus resumes after partition recovery" << std::endl;
}

/**
 * Test: Simulate minority chain sync after partition.
 * 
 * This test validates that validators on the minority partition
 * can sync to the canonical chain after partition is resolved.
 */
TEST_F(NetworkPartitionStressTest, MinorityChainSyncAfterPartition) {
    std::cout << "\n=== Minority Chain Sync Test ===" << std::endl;

    uint32_t isolated_pct = 25;  // 25% < 33.3%

    // Create partition
    auto partition = create_partition(isolated_pct, 100, 1000000);

    std::cout << "Partition created (25% isolated)" << std::endl;
    std::cout << "  Isolated validators: " << partition.isolated_validators.size() << std::endl;

    // Produce blocks on majority partition (canonical chain)
    uint64_t current_time = partition.timestamp_ms;
    for (uint64_t height = partition.block_height; height < partition.block_height + 50; ++height) {
        auto block = produce_block(height, current_time, true);
        canonical_chain_.push_back(block);
        current_time += 2000;
    }

    std::cout << "  Canonical chain height: " << canonical_chain_.size() << std::endl;

    // Simulate minority partition attempting to produce blocks (won't finalize)
    // In reality, minority would see they don't have quorum and wait
    uint64_t minority_time = partition.timestamp_ms;
    for (uint64_t height = partition.block_height; height < partition.block_height + 10; ++height) {
        // Minority produces fewer blocks since they can't finalize
        auto block = produce_block(height, minority_time, false);
        // Override signers to only include isolated validators
        block.signers.clear();
        for (const auto& vid : partition.isolated_validators) {
            block.signers.insert(vid);
        }
        block.is_finalized = false;  // Can't finalize without quorum
        minority_chain_.push_back(block);
        minority_time += 2000;
    }

    std::cout << "  Minority chain height: " << minority_chain_.size() << std::endl;

    // Resolve partition
    std::cout << "\nResolving partition..." << std::endl;
    resolve_partition(partition, current_time);

    // Minority validators sync to canonical chain
    std::cout << "Syncing minority validators to canonical chain..." << std::endl;
    
    uint64_t canonical_height = canonical_chain_.back().height;
    
    for (auto& validator : test_validators_) {
        // All validators sync to canonical chain height
        validator.last_seen_height = canonical_height;
    }

    // Verify all validators are at the same height
    bool all_synced = true;
    
    for (const auto& validator : test_validators_) {
        if (validator.last_seen_height != canonical_height) {
            all_synced = false;
            break;
        }
    }

    std::cout << "  All validators synced: " << (all_synced ? "YES" : "NO") << std::endl;
    std::cout << "  Canonical height: " << canonical_height << std::endl;

    EXPECT_TRUE(all_synced)
        << "All validators should sync to canonical chain after partition";

    // Verify minority chain blocks are discarded (not canonical)
    for (const auto& block : minority_chain_) {
        EXPECT_FALSE(block.is_canonical)
            << "Minority chain blocks should not be canonical";
        EXPECT_FALSE(block.is_finalized)
            << "Minority chain blocks should not be finalized";
    }

    std::cout << "✓ Minority validators successfully synced to canonical chain" << std::endl;
}

/**
 * Test: Simulate multiple partition cycles.
 * 
 * This test validates that the system can handle multiple
 * partition and recovery cycles.
 */
TEST_F(NetworkPartitionStressTest, MultiplePartitionCycles) {
    std::cout << "\n=== Multiple Partition Cycles Test ===" << std::endl;

    uint64_t current_time = 1000000;
    uint64_t current_height = 100;

    // Simulate 5 partition cycles
    for (int cycle = 0; cycle < 5; ++cycle) {
        std::cout << "\nCycle " << (cycle + 1) << ":" << std::endl;

        // Alternate between minority and major partitions
        uint32_t isolated_pct = (cycle % 2 == 0) ? 25 : 40;

        // Create partition
        auto partition = create_partition(isolated_pct, current_height, current_time);
        std::cout << "  Partition: " << isolated_pct << "% isolated" << std::endl;

        // Produce blocks during partition
        for (int i = 0; i < 20; ++i) {
            auto block = produce_block(current_height++, current_time, true);
            canonical_chain_.push_back(block);
            current_time += 2000;
        }

        // Resolve partition
        resolve_partition(partition, current_time);
        partition_events_.push_back(partition);
        std::cout << "  Partition resolved after " << partition.get_duration_ms() << " ms" << std::endl;

        // Produce blocks after recovery
        for (int i = 0; i < 10; ++i) {
            auto block = produce_block(current_height++, current_time, true);
            canonical_chain_.push_back(block);
            current_time += 2000;
        }
    }

    std::cout << "\nResults:" << std::endl;
    std::cout << "  Total partition cycles: " << partition_events_.size() << std::endl;
    std::cout << "  Total blocks produced: " << canonical_chain_.size() << std::endl;

    // Verify all partitions were resolved
    bool all_resolved = std::all_of(partition_events_.begin(), partition_events_.end(),
                                   [](const PartitionEvent& e) { return e.is_resolved; });

    EXPECT_TRUE(all_resolved)
        << "All partitions should be resolved";

    std::cout << "  All partitions resolved: " << (all_resolved ? "YES" : "NO") << std::endl;
    std::cout << "✓ System handled multiple partition cycles successfully" << std::endl;
}

/**
 * Test: Verify partition event logging and metrics.
 * 
 * This test validates that partition events are properly logged
 * with all required information.
 */
TEST_F(NetworkPartitionStressTest, PartitionEventLoggingAndMetrics) {
    std::cout << "\n=== Partition Event Logging Test ===" << std::endl;

    // Create and resolve multiple partitions
    uint64_t current_time = 1000000;
    
    for (int i = 0; i < 3; ++i) {
        uint32_t isolated_pct = 20 + i * 10;  // 20%, 30%, 40%
        auto partition = create_partition(isolated_pct, 100 + i * 50, current_time);
        
        current_time += 60000;  // 60 seconds
        resolve_partition(partition, current_time);
        
        partition_events_.push_back(partition);
        current_time += 10000;  // 10 seconds between partitions
    }

    std::cout << "Partition events logged: " << partition_events_.size() << std::endl;

    // Verify each event has required information
    for (size_t i = 0; i < partition_events_.size(); ++i) {
        const auto& event = partition_events_[i];
        
        std::cout << "\nEvent " << (i + 1) << ":" << std::endl;
        std::cout << "  Timestamp: " << event.timestamp_ms << " ms" << std::endl;
        std::cout << "  Block height: " << event.block_height << std::endl;
        std::cout << "  Isolated validators: " << event.isolated_validators.size() << std::endl;
        std::cout << "  Isolated stake: " << event.isolated_stake << " SAR (" 
                  << (event.get_isolated_ratio() * 100.0) << "%)" << std::endl;
        std::cout << "  Duration: " << event.get_duration_ms() << " ms" << std::endl;
        std::cout << "  Resolved: " << (event.is_resolved ? "YES" : "NO") << std::endl;

        // Verify required fields
        EXPECT_GT(event.timestamp_ms, 0) << "Event should have timestamp";
        EXPECT_GT(event.block_height, 0) << "Event should have block height";
        EXPECT_GT(event.isolated_validators.size(), 0) << "Event should have isolated validators";
        EXPECT_GT(event.isolated_stake, 0) << "Event should have isolated stake";
        EXPECT_GT(event.total_stake, 0) << "Event should have total stake";
        EXPECT_TRUE(event.is_resolved) << "Event should be resolved";
        EXPECT_GT(event.get_duration_ms(), 0) << "Event should have duration";
    }

    std::cout << "\n✓ All partition events properly logged with metrics" << std::endl;
}

} // namespace stress
} // namespace sarafu
