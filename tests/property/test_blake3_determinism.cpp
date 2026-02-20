#include "sarafu/crypto/blake3_hash.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

using namespace sarafu::crypto;

/**
 * Property-Based Test for Blake3 Determinism
 * 
 * **Validates: Requirements 10.1, 10.2, 10.3, 10.4**
 * 
 * Property 64: Blake3 Determinism
 * For any data D, Blake3(D) produces the same hash across all nodes and all executions.
 * 
 * This test validates that:
 * 1. The same input always produces the same output (determinism)
 * 2. Hash computation is independent of execution context
 * 3. Hash computation is independent of timing
 * 4. Hash computation works correctly for all input sizes
 */
class Blake3DeterminismPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random data of specified size
    std::vector<uint8_t> generate_random_data(size_t size) {
        std::vector<uint8_t> data(size);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return data;
    }

    // Generate random string of specified length
    std::string generate_random_string(size_t length) {
        std::string str;
        str.reserve(length);
        std::uniform_int_distribution<int> dist(32, 126); // Printable ASCII
        for (size_t i = 0; i < length; ++i) {
            str += static_cast<char>(dist(rng_));
        }
        return str;
    }

    std::mt19937 rng_;
};

/**
 * Property: Blake3 hash is deterministic for the same input
 * 
 * For any input data D, computing Blake3(D) multiple times
 * must always produce the same hash value.
 */
TEST_F(Blake3DeterminismPropertyTest, SameInputProducesSameHash) {
    const int NUM_TRIALS = 1000;
    const int NUM_REPETITIONS = 10;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random input size between 0 and 10KB
        std::uniform_int_distribution<size_t> size_dist(0, 10240);
        size_t input_size = size_dist(rng_);
        
        // Generate random data
        std::vector<uint8_t> data = generate_random_data(input_size);
        
        // Compute hash multiple times
        Blake3Hash first_hash = Blake3Hash::hash(data);
        
        for (int rep = 1; rep < NUM_REPETITIONS; ++rep) {
            Blake3Hash subsequent_hash = Blake3Hash::hash(data);
            ASSERT_EQ(first_hash, subsequent_hash)
                << "Hash mismatch for input size " << input_size
                << " on repetition " << rep;
        }
    }
}

/**
 * Property: Blake3 hash is deterministic across different data types
 * 
 * For the same underlying bytes, Blake3 should produce the same hash
 * regardless of whether the input is provided as vector<uint8_t>,
 * string, or raw pointer.
 */
TEST_F(Blake3DeterminismPropertyTest, ConsistentAcrossDataTypes) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random string
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t length = size_dist(rng_);
        std::string str_data = generate_random_string(length);
        
        // Convert to vector
        std::vector<uint8_t> vec_data(str_data.begin(), str_data.end());
        
        // Compute hashes using different input types
        Blake3Hash hash_from_string = Blake3Hash::hash(str_data);
        Blake3Hash hash_from_vector = Blake3Hash::hash(vec_data);
        Blake3Hash hash_from_pointer = Blake3Hash::hash(
            reinterpret_cast<const uint8_t*>(str_data.data()),
            str_data.size()
        );
        
        ASSERT_EQ(hash_from_string, hash_from_vector)
            << "Hash mismatch between string and vector for length " << length;
        ASSERT_EQ(hash_from_string, hash_from_pointer)
            << "Hash mismatch between string and pointer for length " << length;
        ASSERT_EQ(hash_from_vector, hash_from_pointer)
            << "Hash mismatch between vector and pointer for length " << length;
    }
}

/**
 * Property: Blake3 hash is independent of execution timing
 * 
 * Computing the hash at different times should produce the same result.
 * This validates that the hash function has no time-dependent behavior.
 */
TEST_F(Blake3DeterminismPropertyTest, IndependentOfTiming) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random data
        std::uniform_int_distribution<size_t> size_dist(0, 4096);
        size_t input_size = size_dist(rng_);
        std::vector<uint8_t> data = generate_random_data(input_size);
        
        // Compute hash immediately
        Blake3Hash hash1 = Blake3Hash::hash(data);
        
        // Small delay
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        
        // Compute hash again
        Blake3Hash hash2 = Blake3Hash::hash(data);
        
        // Another delay
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        
        // Compute hash a third time
        Blake3Hash hash3 = Blake3Hash::hash(data);
        
        ASSERT_EQ(hash1, hash2)
            << "Hash changed after timing delay (first comparison)";
        ASSERT_EQ(hash2, hash3)
            << "Hash changed after timing delay (second comparison)";
        ASSERT_EQ(hash1, hash3)
            << "Hash changed after timing delay (third comparison)";
    }
}

/**
 * Property: Blake3 hash works correctly for all input sizes
 * 
 * The hash function should produce valid, deterministic results
 * for inputs of any size from 0 bytes to very large sizes.
 */
TEST_F(Blake3DeterminismPropertyTest, WorksForAllInputSizes) {
    // Test specific boundary sizes
    std::vector<size_t> test_sizes = {
        0,           // Empty input
        1,           // Single byte
        31,          // Just under hash size
        32,          // Exactly hash size
        33,          // Just over hash size
        63,          // Just under 64 bytes
        64,          // Common block size
        65,          // Just over 64 bytes
        127,         // Just under 128 bytes
        128,         // Another common block size
        255,         // Just under 256 bytes
        256,         // Power of 2
        1023,        // Just under 1KB
        1024,        // 1KB
        4095,        // Just under 4KB
        4096,        // 4KB (page size)
        8192,        // 8KB
        16384,       // 16KB
        65535,       // Just under 64KB
        65536,       // 64KB
        1048576      // 1MB
    };

    for (size_t size : test_sizes) {
        std::vector<uint8_t> data = generate_random_data(size);
        
        // Compute hash multiple times
        Blake3Hash hash1 = Blake3Hash::hash(data);
        Blake3Hash hash2 = Blake3Hash::hash(data);
        Blake3Hash hash3 = Blake3Hash::hash(data);
        
        ASSERT_EQ(hash1, hash2)
            << "Hash not deterministic for input size " << size;
        ASSERT_EQ(hash2, hash3)
            << "Hash not deterministic for input size " << size;
        
        // Verify hash is 32 bytes
        ASSERT_EQ(hash1.size(), 32)
            << "Hash size incorrect for input size " << size;
    }
}

/**
 * Property: Blake3 hash is collision-resistant for different inputs
 * 
 * Different inputs should produce different hashes (with overwhelming probability).
 * While this doesn't prove collision resistance, it validates basic functionality.
 */
TEST_F(Blake3DeterminismPropertyTest, DifferentInputsProduceDifferentHashes) {
    const int NUM_TRIALS = 1000;
    std::set<std::string> seen_hashes;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random data
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t input_size = size_dist(rng_);
        std::vector<uint8_t> data = generate_random_data(input_size);
        
        Blake3Hash hash = Blake3Hash::hash(data);
        std::string hash_hex = hash.to_hex();
        
        // Check for collision (should be extremely rare)
        ASSERT_EQ(seen_hashes.count(hash_hex), 0)
            << "Unexpected hash collision detected for random inputs";
        
        seen_hashes.insert(hash_hex);
    }
}

/**
 * Property: Blake3 hash changes completely with single bit flip
 * 
 * Changing a single bit in the input should produce a completely
 * different hash (avalanche effect).
 */
TEST_F(Blake3DeterminismPropertyTest, AvalancheEffect) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random data (at least 1 byte)
        std::uniform_int_distribution<size_t> size_dist(1, 1024);
        size_t input_size = size_dist(rng_);
        std::vector<uint8_t> data = generate_random_data(input_size);
        
        // Compute original hash
        Blake3Hash original_hash = Blake3Hash::hash(data);
        
        // Flip a random bit
        std::uniform_int_distribution<size_t> byte_dist(0, input_size - 1);
        std::uniform_int_distribution<int> bit_dist(0, 7);
        size_t byte_index = byte_dist(rng_);
        int bit_index = bit_dist(rng_);
        
        data[byte_index] ^= (1 << bit_index);
        
        // Compute modified hash
        Blake3Hash modified_hash = Blake3Hash::hash(data);
        
        // Hashes should be different
        ASSERT_NE(original_hash, modified_hash)
            << "Hash did not change after flipping bit " << bit_index
            << " in byte " << byte_index;
        
        // Count differing bits (should be approximately 50%)
        int differing_bits = 0;
        for (size_t i = 0; i < 32; ++i) {
            uint8_t xor_byte = original_hash.bytes()[i] ^ modified_hash.bytes()[i];
            for (int bit = 0; bit < 8; ++bit) {
                if (xor_byte & (1 << bit)) {
                    differing_bits++;
                }
            }
        }
        
        // Avalanche effect: expect roughly 50% of bits to differ (128 out of 256)
        // Allow range of 25% to 75% (64 to 192 bits) for statistical variation
        ASSERT_GE(differing_bits, 64)
            << "Insufficient avalanche effect: only " << differing_bits << " bits differ";
        ASSERT_LE(differing_bits, 192)
            << "Excessive avalanche effect: " << differing_bits << " bits differ";
    }
}

/**
 * Property: Blake3 hash is deterministic for empty input
 * 
 * The hash of empty data should always be the same known value.
 */
TEST_F(Blake3DeterminismPropertyTest, EmptyInputDeterminism) {
    const int NUM_REPETITIONS = 100;
    
    // Known Blake3 hash of empty input
    const std::string expected_hex = "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262";
    
    for (int i = 0; i < NUM_REPETITIONS; ++i) {
        std::vector<uint8_t> empty;
        Blake3Hash hash = Blake3Hash::hash(empty);
        
        ASSERT_EQ(hash.to_hex(), expected_hex)
            << "Empty input hash mismatch on iteration " << i;
    }
}

/**
 * Property: Blake3 hash serialization is deterministic
 * 
 * Serializing and deserializing a hash should preserve its value.
 */
TEST_F(Blake3DeterminismPropertyTest, SerializationDeterminism) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random data and hash it
        std::uniform_int_distribution<size_t> size_dist(0, 1024);
        size_t input_size = size_dist(rng_);
        std::vector<uint8_t> data = generate_random_data(input_size);
        
        Blake3Hash original_hash = Blake3Hash::hash(data);
        
        // Serialize to hex
        std::string hex = original_hash.to_hex();
        
        // Deserialize from hex
        Blake3Hash reconstructed_hash = Blake3Hash::from_hex(hex);
        
        // Should be identical
        ASSERT_EQ(original_hash, reconstructed_hash)
            << "Hash changed after hex serialization round-trip";
        
        // Serialize to bytes
        std::vector<uint8_t> bytes = original_hash.serialize();
        
        // Reconstruct from bytes
        Blake3Hash reconstructed_from_bytes(bytes);
        
        // Should be identical
        ASSERT_EQ(original_hash, reconstructed_from_bytes)
            << "Hash changed after byte serialization round-trip";
    }
}

/**
 * Property: Blake3 hash comparison operators are consistent
 * 
 * The comparison operators should be transitive and consistent.
 */
TEST_F(Blake3DeterminismPropertyTest, ComparisonConsistency) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate three different inputs
        std::vector<uint8_t> data1 = generate_random_data(100);
        std::vector<uint8_t> data2 = generate_random_data(100);
        std::vector<uint8_t> data3 = generate_random_data(100);
        
        Blake3Hash hash1 = Blake3Hash::hash(data1);
        Blake3Hash hash2 = Blake3Hash::hash(data2);
        Blake3Hash hash3 = Blake3Hash::hash(data3);
        
        // Test reflexivity: a == a
        ASSERT_TRUE(hash1 == hash1);
        ASSERT_FALSE(hash1 != hash1);
        
        // Test symmetry: if a == b, then b == a
        if (hash1 == hash2) {
            ASSERT_TRUE(hash2 == hash1);
        }
        
        // Test transitivity: if a == b and b == c, then a == c
        if (hash1 == hash2 && hash2 == hash3) {
            ASSERT_TRUE(hash1 == hash3);
        }
        
        // Test less-than consistency
        if (hash1 < hash2) {
            ASSERT_FALSE(hash2 < hash1);
            ASSERT_TRUE(hash1 != hash2);
        }
    }
}

