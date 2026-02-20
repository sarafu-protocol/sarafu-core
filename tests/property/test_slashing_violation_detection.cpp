#include "sarafu/consensus/slashing_detector.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/state/account.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace sarafu::consensus;
using namespace sarafu::crypto;
using namespace sarafu::state;

/**
 * Property-Based Tests for Slashing Violation Detection
 * 
 * **Validates: Requirements 3.1, 3.2**
 * 
 * Property 9: Double-Sign Detection
 * For any validator V, if V signs two different blocks B1 and B2 at the same height H,
 * the system detects this as a double-signing violation.
 * 
 * Property 10: Surround Vote Detection
 * For any validator V, if V votes for block B2 that conflicts with a previously signed block B1,
 * the system detects this as a surround vote violation.
 */
class SlashingViolationDetectionPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
        detector_ = std::make_unique<SlashingDetector>();
    }

    // Generate random validator ID
    ValidatorID generate_random_validator_id() {
        std::vector<uint8_t> data(32);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 32; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return Address(data);
    }

    // Generate random block hash
    Blake3Hash generate_random_hash() {
        std::vector<uint8_t> data(32);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 32; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return Blake3Hash(data);
    }

    // Generate random BLS signature (for testing purposes)
    BLS12_381_Signature generate_random_signature() {
        std::vector<uint8_t> data(96);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 96; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return BLS12_381_Signature(data);
    }

    std::mt19937 rng_;
    std::unique_ptr<SlashingDetector> detector_;
};

/**
 * Property 9: Double-Sign Detection
 * 
 * For any validator V, if V signs two different blocks B1 and B2 at the same height H,
 * the system detects this as a double-signing violation.
 */
TEST_F(SlashingViolationDetectionPropertyTest, DoubleSignDetection) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random validator
        ValidatorID validator_id = generate_random_validator_id();

        // Generate random height
        std::uniform_int_distribution<uint64_t> height_dist(1, 1000000);
        uint64_t height = height_dist(rng_);

        // Generate two different block hashes at the same height
        Blake3Hash hash1 = generate_random_hash();
        Blake3Hash hash2 = generate_random_hash();

        // Ensure hashes are different
        while (hash1 == hash2) {
            hash2 = generate_random_hash();
        }

        // Generate signatures
        BLS12_381_Signature sig1 = generate_random_signature();
        BLS12_381_Signature sig2 = generate_random_signature();

        // Generate timestamps
        std::uniform_int_distribution<uint64_t> time_dist(1000000, 2000000);
        uint64_t timestamp1 = time_dist(rng_);
        uint64_t timestamp2 = time_dist(rng_);

        // Create signature records
        SignatureRecord record1(validator_id, height, hash1, sig1, timestamp1);
        SignatureRecord record2(validator_id, height, hash2, sig2, timestamp2);

        // Detect double-signing
        auto violation = detector_->detect_double_sign(record1, record2);

        // Violation should be detected
        ASSERT_TRUE(violation.has_value())
            << "Failed to detect double-signing at height " << height
            << " for trial " << trial;

        // Verify violation details
        EXPECT_EQ(violation->validator_id, validator_id);
        EXPECT_EQ(violation->block_height, height);
        EXPECT_EQ(violation->reason, SlashReason::DoubleSign);
    }
}

/**
 * Property: Same block at same height is NOT a double-sign violation
 * 
 * If a validator signs the same block twice (same hash), this should
 * not be detected as a violation.
 */
TEST_F(SlashingViolationDetectionPropertyTest, SameBlockNotViolation) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random validator
        ValidatorID validator_id = generate_random_validator_id();

        // Generate random height
        std::uniform_int_distribution<uint64_t> height_dist(1, 1000000);
        uint64_t height = height_dist(rng_);

        // Generate same block hash
        Blake3Hash hash = generate_random_hash();

        // Generate different signatures (validator might sign same block multiple times)
        BLS12_381_Signature sig1 = generate_random_signature();
        BLS12_381_Signature sig2 = generate_random_signature();

        // Generate timestamps
        std::uniform_int_distribution<uint64_t> time_dist(1000000, 2000000);
        uint64_t timestamp1 = time_dist(rng_);
        uint64_t timestamp2 = time_dist(rng_);

        // Create signature records with SAME hash
        SignatureRecord record1(validator_id, height, hash, sig1, timestamp1);
        SignatureRecord record2(validator_id, height, hash, sig2, timestamp2);

        // Detect double-signing
        auto violation = detector_->detect_double_sign(record1, record2);

        // No violation should be detected (same block)
        EXPECT_FALSE(violation.has_value())
            << "Incorrectly detected double-signing for same block at height " << height
            << " for trial " << trial;
    }
}

/**
 * Property: Different heights are NOT a double-sign violation
 * 
 * If a validator signs different blocks at different heights,
 * this is normal behavior and should not be detected as a violation.
 */
TEST_F(SlashingViolationDetectionPropertyTest, DifferentHeightsNotViolation) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random validator
        ValidatorID validator_id = generate_random_validator_id();

        // Generate two different heights
        std::uniform_int_distribution<uint64_t> height_dist(1, 1000000);
        uint64_t height1 = height_dist(rng_);
        uint64_t height2 = height_dist(rng_);

        // Ensure heights are different
        while (height1 == height2) {
            height2 = height_dist(rng_);
        }

        // Generate different block hashes
        Blake3Hash hash1 = generate_random_hash();
        Blake3Hash hash2 = generate_random_hash();

        // Generate signatures
        BLS12_381_Signature sig1 = generate_random_signature();
        BLS12_381_Signature sig2 = generate_random_signature();

        // Generate timestamps
        std::uniform_int_distribution<uint64_t> time_dist(1000000, 2000000);
        uint64_t timestamp1 = time_dist(rng_);
        uint64_t timestamp2 = time_dist(rng_);

        // Create signature records at DIFFERENT heights
        SignatureRecord record1(validator_id, height1, hash1, sig1, timestamp1);
        SignatureRecord record2(validator_id, height2, hash2, sig2, timestamp2);

        // Detect double-signing
        auto violation = detector_->detect_double_sign(record1, record2);

        // No violation should be detected (different heights)
        EXPECT_FALSE(violation.has_value())
            << "Incorrectly detected double-signing for different heights "
            << height1 << " and " << height2 << " for trial " << trial;
    }
}

/**
 * Property: Different validators are NOT a double-sign violation
 * 
 * If two different validators sign different blocks at the same height,
 * this is normal behavior and should not be detected as a violation.
 */
TEST_F(SlashingViolationDetectionPropertyTest, DifferentValidatorsNotViolation) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate two different validators
        ValidatorID validator_id1 = generate_random_validator_id();
        ValidatorID validator_id2 = generate_random_validator_id();

        // Ensure validators are different
        while (validator_id1 == validator_id2) {
            validator_id2 = generate_random_validator_id();
        }

        // Generate same height
        std::uniform_int_distribution<uint64_t> height_dist(1, 1000000);
        uint64_t height = height_dist(rng_);

        // Generate different block hashes
        Blake3Hash hash1 = generate_random_hash();
        Blake3Hash hash2 = generate_random_hash();

        // Generate signatures
        BLS12_381_Signature sig1 = generate_random_signature();
        BLS12_381_Signature sig2 = generate_random_signature();

        // Generate timestamps
        std::uniform_int_distribution<uint64_t> time_dist(1000000, 2000000);
        uint64_t timestamp1 = time_dist(rng_);
        uint64_t timestamp2 = time_dist(rng_);

        // Create signature records from DIFFERENT validators
        SignatureRecord record1(validator_id1, height, hash1, sig1, timestamp1);
        SignatureRecord record2(validator_id2, height, hash2, sig2, timestamp2);

        // Detect double-signing
        auto violation = detector_->detect_double_sign(record1, record2);

        // No violation should be detected (different validators)
        EXPECT_FALSE(violation.has_value())
            << "Incorrectly detected double-signing for different validators at height "
            << height << " for trial " << trial;
    }
}

/**
 * Property 10: Surround Vote Detection (Same Height)
 * 
 * For any validator V, if V votes for two different blocks at the same height,
 * the system detects this as a surround vote violation.
 */
TEST_F(SlashingViolationDetectionPropertyTest, SurroundVoteDetectionSameHeight) {
    const int NUM_TRIALS = 1000;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random validator
        ValidatorID validator_id = generate_random_validator_id();

        // Generate random height
        std::uniform_int_distribution<uint64_t> height_dist(1, 1000000);
        uint64_t height = height_dist(rng_);

        // Generate random view number
        std::uniform_int_distribution<uint64_t> view_dist(1, 10000);
        uint64_t view1 = view_dist(rng_);
        uint64_t view2 = view_dist(rng_);

        // Generate two different block hashes
        Blake3Hash hash1 = generate_random_hash();
        Blake3Hash hash2 = generate_random_hash();

        // Ensure hashes are different
        while (hash1 == hash2) {
            hash2 = generate_random_hash();
        }

        // Generate signatures
        BLS12_381_Signature sig1 = generate_random_signature();
        BLS12_381_Signature sig2 = generate_random_signature();

        // Create votes at same height but different blocks
        Vote vote1(validator_id, height, hash1, view1, sig1);
        Vote vote2(validator_id, height, hash2, view2, sig2);

        // Detect surround vote
        auto violation = detector_->detect_surround_vote(vote1, vote2);

        // Violation should be detected
        ASSERT_TRUE(violation.has_value())
            << "Failed to detect surround vote at height " << height
            << " for trial " << trial;

        // Verify violation details
        EXPECT_EQ(violation->validator_id, validator_id);
        EXPECT_EQ(violation->block_height, height);
        EXPECT_EQ(violation->reason, SlashReason::SurroundVote);
    }
}

/**
 * Property: Surround Vote Detection (Same View, Different Heights)
 * 
 * If a validator votes for different blocks at different heights in the same view,
 * this should be detected as a surround vote violation.
 */
TEST_F(SlashingViolationDetectionPropertyTest, SurroundVoteDetectionSameView) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random validator
        ValidatorID validator_id = generate_random_validator_id();

        // Generate two different heights
        std::uniform_int_distribution<uint64_t> height_dist(1, 1000000);
        uint64_t height1 = height_dist(rng_);
        uint64_t height2 = height_dist(rng_);

        // Ensure heights are different
        while (height1 == height2) {
            height2 = height_dist(rng_);
        }

        // Generate same view number
        std::uniform_int_distribution<uint64_t> view_dist(1, 10000);
        uint64_t view = view_dist(rng_);

        // Generate two different block hashes
        Blake3Hash hash1 = generate_random_hash();
        Blake3Hash hash2 = generate_random_hash();

        // Ensure hashes are different
        while (hash1 == hash2) {
            hash2 = generate_random_hash();
        }

        // Generate signatures
        BLS12_381_Signature sig1 = generate_random_signature();
        BLS12_381_Signature sig2 = generate_random_signature();

        // Create votes at different heights but same view
        Vote vote1(validator_id, height1, hash1, view, sig1);
        Vote vote2(validator_id, height2, hash2, view, sig2);

        // Detect surround vote
        auto violation = detector_->detect_surround_vote(vote1, vote2);

        // Violation should be detected
        ASSERT_TRUE(violation.has_value())
            << "Failed to detect surround vote in view " << view
            << " at heights " << height1 << " and " << height2
            << " for trial " << trial;

        // Verify violation details
        EXPECT_EQ(violation->validator_id, validator_id);
        EXPECT_EQ(violation->reason, SlashReason::SurroundVote);
    }
}

/**
 * Property: Same vote is NOT a surround vote violation
 * 
 * If a validator votes for the same block twice, this should not
 * be detected as a violation.
 */
TEST_F(SlashingViolationDetectionPropertyTest, SameVoteNotViolation) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random validator
        ValidatorID validator_id = generate_random_validator_id();

        // Generate random height
        std::uniform_int_distribution<uint64_t> height_dist(1, 1000000);
        uint64_t height = height_dist(rng_);

        // Generate random view number
        std::uniform_int_distribution<uint64_t> view_dist(1, 10000);
        uint64_t view = view_dist(rng_);

        // Generate same block hash
        Blake3Hash hash = generate_random_hash();

        // Generate different signatures (might vote multiple times)
        BLS12_381_Signature sig1 = generate_random_signature();
        BLS12_381_Signature sig2 = generate_random_signature();

        // Create votes for SAME block
        Vote vote1(validator_id, height, hash, view, sig1);
        Vote vote2(validator_id, height, hash, view, sig2);

        // Detect surround vote
        auto violation = detector_->detect_surround_vote(vote1, vote2);

        // No violation should be detected (same block)
        EXPECT_FALSE(violation.has_value())
            << "Incorrectly detected surround vote for same block at height " << height
            << " for trial " << trial;
    }
}

/**
 * Property: Different validators are NOT a surround vote violation
 * 
 * If two different validators vote for different blocks, this is
 * normal behavior and should not be detected as a violation.
 */
TEST_F(SlashingViolationDetectionPropertyTest, DifferentValidatorsNotSurroundVote) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate two different validators
        ValidatorID validator_id1 = generate_random_validator_id();
        ValidatorID validator_id2 = generate_random_validator_id();

        // Ensure validators are different
        while (validator_id1 == validator_id2) {
            validator_id2 = generate_random_validator_id();
        }

        // Generate random height
        std::uniform_int_distribution<uint64_t> height_dist(1, 1000000);
        uint64_t height = height_dist(rng_);

        // Generate random view number
        std::uniform_int_distribution<uint64_t> view_dist(1, 10000);
        uint64_t view = view_dist(rng_);

        // Generate different block hashes
        Blake3Hash hash1 = generate_random_hash();
        Blake3Hash hash2 = generate_random_hash();

        // Generate signatures
        BLS12_381_Signature sig1 = generate_random_signature();
        BLS12_381_Signature sig2 = generate_random_signature();

        // Create votes from DIFFERENT validators
        Vote vote1(validator_id1, height, hash1, view, sig1);
        Vote vote2(validator_id2, height, hash2, view, sig2);

        // Detect surround vote
        auto violation = detector_->detect_surround_vote(vote1, vote2);

        // No violation should be detected (different validators)
        EXPECT_FALSE(violation.has_value())
            << "Incorrectly detected surround vote for different validators at height "
            << height << " for trial " << trial;
    }
}

/**
 * Property: Signature history tracking
 * 
 * The detector should correctly track signature history for validators.
 */
TEST_F(SlashingViolationDetectionPropertyTest, SignatureHistoryTracking) {
    const int NUM_VALIDATORS = 10;
    const int SIGNATURES_PER_VALIDATOR = 100;

    std::vector<ValidatorID> validators;
    for (int i = 0; i < NUM_VALIDATORS; ++i) {
        validators.push_back(generate_random_validator_id());
    }

    // Record signatures for each validator
    for (const auto& validator_id : validators) {
        for (int j = 0; j < SIGNATURES_PER_VALIDATOR; ++j) {
            std::uniform_int_distribution<uint64_t> height_dist(1, 1000000);
            uint64_t height = height_dist(rng_);

            Blake3Hash hash = generate_random_hash();
            BLS12_381_Signature sig = generate_random_signature();

            std::uniform_int_distribution<uint64_t> time_dist(1000000, 2000000);
            uint64_t timestamp = time_dist(rng_);

            SignatureRecord record(validator_id, height, hash, sig, timestamp);
            detector_->record_signature(record);
        }
    }

    // Verify history is tracked correctly
    for (const auto& validator_id : validators) {
        auto history = detector_->get_signature_history(validator_id);
        
        // Should have recorded signatures (up to max_signature_history limit)
        EXPECT_GT(history.size(), 0)
            << "No signature history found for validator";
        EXPECT_LE(history.size(), SIGNATURES_PER_VALIDATOR)
            << "Signature history exceeds expected size";
    }
}

/**
 * Property: Vote history tracking
 * 
 * The detector should correctly track vote history for validators.
 */
TEST_F(SlashingViolationDetectionPropertyTest, VoteHistoryTracking) {
    const int NUM_VALIDATORS = 10;
    const int VOTES_PER_VALIDATOR = 100;

    std::vector<ValidatorID> validators;
    for (int i = 0; i < NUM_VALIDATORS; ++i) {
        validators.push_back(generate_random_validator_id());
    }

    // Record votes for each validator
    for (const auto& validator_id : validators) {
        for (int j = 0; j < VOTES_PER_VALIDATOR; ++j) {
            std::uniform_int_distribution<uint64_t> height_dist(1, 1000000);
            uint64_t height = height_dist(rng_);

            std::uniform_int_distribution<uint64_t> view_dist(1, 10000);
            uint64_t view = view_dist(rng_);

            Blake3Hash hash = generate_random_hash();
            BLS12_381_Signature sig = generate_random_signature();

            Vote vote(validator_id, height, hash, view, sig);
            detector_->record_vote(vote);
        }
    }

    // Verify history is tracked correctly
    for (const auto& validator_id : validators) {
        auto history = detector_->get_vote_history(validator_id);
        
        // Should have recorded votes (up to max_signature_history limit)
        EXPECT_GT(history.size(), 0)
            << "No vote history found for validator";
        EXPECT_LE(history.size(), VOTES_PER_VALIDATOR)
            << "Vote history exceeds expected size";
    }
}

/**
 * Property: History pruning works correctly
 * 
 * Old signatures and votes should be pruned to prevent unbounded growth.
 */
TEST_F(SlashingViolationDetectionPropertyTest, HistoryPruning) {
    ValidatorID validator_id = generate_random_validator_id();

    // Record signatures at various heights
    for (uint64_t height = 1; height <= 1000; ++height) {
        Blake3Hash hash = generate_random_hash();
        BLS12_381_Signature sig = generate_random_signature();
        uint64_t timestamp = 1000000 + height;

        SignatureRecord record(validator_id, height, hash, sig, timestamp);
        detector_->record_signature(record);
    }

    // Verify all signatures are recorded
    auto history_before = detector_->get_signature_history(validator_id);
    EXPECT_EQ(history_before.size(), 1000);

    // Prune old signatures (keep only last 100 blocks)
    detector_->prune_old_signatures(1000, 100);

    // Verify old signatures are pruned
    auto history_after = detector_->get_signature_history(validator_id);
    EXPECT_LE(history_after.size(), 101)
        << "History not pruned correctly";

    // Verify remaining signatures are recent (within retention period)
    // Heights 900-1000 should be kept (101 blocks)
    for (const auto& record : history_after) {
        EXPECT_GE(record.block_height, 900)
            << "Old signature not pruned";
    }
}
