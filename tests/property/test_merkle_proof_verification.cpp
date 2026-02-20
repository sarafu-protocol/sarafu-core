#include "sarafu/crypto/merkle_tree.h"
#include "sarafu/crypto/blake3_hash.h"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <set>

using namespace sarafu::crypto;

/**
 * Property-Based Test for Merkle Proof Verification
 * 
 * **Validates: Requirements 5.4**
 * 
 * Property 28: Merkle Proof Verification
 * For any valid Merkle tree with N leaves, for any leaf index i where 0 ≤ i < N,
 * the proof generated for leaf i must verify successfully against the tree root,
 * and any modification to the leaf, proof, or root must cause verification to fail.
 * 
 * This test validates that:
 * 1. Valid proofs always verify successfully
 * 2. Modified leaf hashes cause verification to fail
 * 3. Modified proof elements cause verification to fail
 * 4. Modified roots cause verification to fail
 * 5. Proofs work correctly for all tree sizes
 * 6. Proofs work correctly for all leaf positions
 */
class MerkleProofVerificationPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Seed random generator with a fixed seed for reproducibility
        rng_.seed(42);
    }

    // Generate random Blake3 hash
    Blake3Hash generate_random_hash() {
        std::vector<uint8_t> data(32);
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        for (size_t i = 0; i < 32; ++i) {
            data[i] = static_cast<uint8_t>(dist(rng_));
        }
        return Blake3Hash(data);
    }

    // Generate vector of random hashes
    std::vector<Blake3Hash> generate_random_hashes(size_t count) {
        std::vector<Blake3Hash> hashes;
        hashes.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            hashes.push_back(generate_random_hash());
        }
        return hashes;
    }

    // Modify a hash by flipping a random bit
    Blake3Hash modify_hash(const Blake3Hash& hash) {
        std::vector<uint8_t> data = hash.serialize();
        
        // Flip a random bit
        std::uniform_int_distribution<size_t> byte_dist(0, data.size() - 1);
        std::uniform_int_distribution<int> bit_dist(0, 7);
        size_t byte_index = byte_dist(rng_);
        int bit_index = bit_dist(rng_);
        
        data[byte_index] ^= (1 << bit_index);
        
        return Blake3Hash(data);
    }

    std::mt19937 rng_;
};

/**
 * Property: Valid proofs always verify successfully
 * 
 * For any Merkle tree with N leaves, for any leaf index i where 0 ≤ i < N,
 * the proof generated for leaf i must verify successfully against the tree root.
 */
TEST_F(MerkleProofVerificationPropertyTest, ValidProofsAlwaysVerify) {
    const int NUM_TRIALS = 500;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random tree size (1 to 100 leaves)
        std::uniform_int_distribution<size_t> size_dist(1, 100);
        size_t num_leaves = size_dist(rng_);
        
        // Generate random leaf hashes
        std::vector<Blake3Hash> leaves = generate_random_hashes(num_leaves);
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        Blake3Hash root = tree.get_root();
        
        // Test proof for each leaf
        for (size_t i = 0; i < num_leaves; ++i) {
            auto proof_opt = tree.generate_proof(i);
            ASSERT_TRUE(proof_opt.has_value())
                << "Failed to generate proof for leaf " << i
                << " in tree with " << num_leaves << " leaves";
            
            auto proof = proof_opt.value();
            
            // Verify the proof
            bool verified = MerkleTree::verify_proof(proof, root);
            ASSERT_TRUE(verified)
                << "Valid proof failed to verify for leaf " << i
                << " in tree with " << num_leaves << " leaves";
            
            // Verify proof structure
            ASSERT_EQ(proof.leaf_index, i)
                << "Proof has incorrect leaf index";
            ASSERT_EQ(proof.leaf_hash, leaves[i])
                << "Proof has incorrect leaf hash";
        }
    }
}

/**
 * Property: Modified leaf hashes cause verification to fail
 * 
 * For any valid proof, if the leaf hash is modified, verification must fail.
 */
TEST_F(MerkleProofVerificationPropertyTest, ModifiedLeafHashFailsVerification) {
    const int NUM_TRIALS = 300;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random tree size (2 to 50 leaves)
        std::uniform_int_distribution<size_t> size_dist(2, 50);
        size_t num_leaves = size_dist(rng_);
        
        // Generate random leaf hashes
        std::vector<Blake3Hash> leaves = generate_random_hashes(num_leaves);
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        Blake3Hash root = tree.get_root();
        
        // Pick a random leaf to test
        std::uniform_int_distribution<size_t> leaf_dist(0, num_leaves - 1);
        size_t leaf_index = leaf_dist(rng_);
        
        // Generate proof
        auto proof_opt = tree.generate_proof(leaf_index);
        ASSERT_TRUE(proof_opt.has_value());
        auto proof = proof_opt.value();
        
        // Verify original proof works
        ASSERT_TRUE(MerkleTree::verify_proof(proof, root));
        
        // Modify the leaf hash
        proof.leaf_hash = modify_hash(proof.leaf_hash);
        
        // Verification should fail
        bool verified = MerkleTree::verify_proof(proof, root);
        ASSERT_FALSE(verified)
            << "Proof with modified leaf hash should not verify";
    }
}

/**
 * Property: Modified proof elements cause verification to fail
 * 
 * For any valid proof with sibling hashes, if any sibling hash is modified,
 * verification must fail.
 */
TEST_F(MerkleProofVerificationPropertyTest, ModifiedProofElementsFailVerification) {
    const int NUM_TRIALS = 300;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random tree size (2 to 50 leaves, ensure multiple levels)
        std::uniform_int_distribution<size_t> size_dist(2, 50);
        size_t num_leaves = size_dist(rng_);
        
        // Generate random leaf hashes
        std::vector<Blake3Hash> leaves = generate_random_hashes(num_leaves);
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        Blake3Hash root = tree.get_root();
        
        // Pick a random leaf to test
        std::uniform_int_distribution<size_t> leaf_dist(0, num_leaves - 1);
        size_t leaf_index = leaf_dist(rng_);
        
        // Generate proof
        auto proof_opt = tree.generate_proof(leaf_index);
        ASSERT_TRUE(proof_opt.has_value());
        auto proof = proof_opt.value();
        
        // Skip if proof has no sibling hashes (single leaf tree)
        if (proof.sibling_hashes.empty()) {
            continue;
        }
        
        // Verify original proof works
        ASSERT_TRUE(MerkleTree::verify_proof(proof, root));
        
        // Modify a random sibling hash
        std::uniform_int_distribution<size_t> sibling_dist(0, proof.sibling_hashes.size() - 1);
        size_t sibling_index = sibling_dist(rng_);
        proof.sibling_hashes[sibling_index] = modify_hash(proof.sibling_hashes[sibling_index]);
        
        // Verification should fail
        bool verified = MerkleTree::verify_proof(proof, root);
        ASSERT_FALSE(verified)
            << "Proof with modified sibling hash should not verify";
    }
}

/**
 * Property: Modified roots cause verification to fail
 * 
 * For any valid proof, if the root is modified, verification must fail.
 */
TEST_F(MerkleProofVerificationPropertyTest, ModifiedRootFailsVerification) {
    const int NUM_TRIALS = 300;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random tree size (1 to 50 leaves)
        std::uniform_int_distribution<size_t> size_dist(1, 50);
        size_t num_leaves = size_dist(rng_);
        
        // Generate random leaf hashes
        std::vector<Blake3Hash> leaves = generate_random_hashes(num_leaves);
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        Blake3Hash root = tree.get_root();
        
        // Pick a random leaf to test
        std::uniform_int_distribution<size_t> leaf_dist(0, num_leaves - 1);
        size_t leaf_index = leaf_dist(rng_);
        
        // Generate proof
        auto proof_opt = tree.generate_proof(leaf_index);
        ASSERT_TRUE(proof_opt.has_value());
        auto proof = proof_opt.value();
        
        // Verify original proof works
        ASSERT_TRUE(MerkleTree::verify_proof(proof, root));
        
        // Modify the root
        Blake3Hash modified_root = modify_hash(root);
        
        // Verification should fail
        bool verified = MerkleTree::verify_proof(proof, modified_root);
        ASSERT_FALSE(verified)
            << "Proof should not verify against modified root";
    }
}

/**
 * Property: Proofs work correctly for all tree sizes
 * 
 * Test edge cases and various tree sizes to ensure correctness.
 */
TEST_F(MerkleProofVerificationPropertyTest, ProofsWorkForAllTreeSizes) {
    // Test specific tree sizes including edge cases
    std::vector<size_t> test_sizes = {
        1,      // Single leaf
        2,      // Two leaves (one level)
        3,      // Odd number (requires duplication)
        4,      // Power of 2
        5,      // Odd number
        7,      // Odd number
        8,      // Power of 2
        15,     // 2^4 - 1
        16,     // Power of 2
        31,     // 2^5 - 1
        32,     // Power of 2
        63,     // 2^6 - 1
        64,     // Power of 2
        100,    // Large tree
        127,    // 2^7 - 1
        128,    // Power of 2
        255,    // 2^8 - 1
        256,    // Power of 2
        500     // Maximum validator set size (Requirement 5.8)
    };

    for (size_t num_leaves : test_sizes) {
        // Generate random leaf hashes
        std::vector<Blake3Hash> leaves = generate_random_hashes(num_leaves);
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        Blake3Hash root = tree.get_root();
        
        // Test proof for first, middle, and last leaf
        std::vector<size_t> test_indices = {0, num_leaves / 2, num_leaves - 1};
        
        for (size_t leaf_index : test_indices) {
            if (leaf_index >= num_leaves) continue;
            
            auto proof_opt = tree.generate_proof(leaf_index);
            ASSERT_TRUE(proof_opt.has_value())
                << "Failed to generate proof for leaf " << leaf_index
                << " in tree with " << num_leaves << " leaves";
            
            auto proof = proof_opt.value();
            
            // Verify the proof
            bool verified = MerkleTree::verify_proof(proof, root);
            ASSERT_TRUE(verified)
                << "Valid proof failed to verify for leaf " << leaf_index
                << " in tree with " << num_leaves << " leaves";
        }
    }
}

/**
 * Property: Proofs work correctly for all leaf positions
 * 
 * For a given tree, proofs should work for every single leaf position.
 */
TEST_F(MerkleProofVerificationPropertyTest, ProofsWorkForAllLeafPositions) {
    const int NUM_TRIALS = 50;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random tree size (1 to 30 leaves)
        std::uniform_int_distribution<size_t> size_dist(1, 30);
        size_t num_leaves = size_dist(rng_);
        
        // Generate random leaf hashes
        std::vector<Blake3Hash> leaves = generate_random_hashes(num_leaves);
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        Blake3Hash root = tree.get_root();
        
        // Test proof for EVERY leaf
        for (size_t i = 0; i < num_leaves; ++i) {
            auto proof_opt = tree.generate_proof(i);
            ASSERT_TRUE(proof_opt.has_value())
                << "Failed to generate proof for leaf " << i;
            
            auto proof = proof_opt.value();
            
            // Verify the proof
            bool verified = MerkleTree::verify_proof(proof, root);
            ASSERT_TRUE(verified)
                << "Valid proof failed to verify for leaf " << i
                << " in tree with " << num_leaves << " leaves";
        }
    }
}

/**
 * Property: Empty tree has zero root
 * 
 * An empty Merkle tree should have a zero hash as its root.
 */
TEST_F(MerkleProofVerificationPropertyTest, EmptyTreeHasZeroRoot) {
    MerkleTree tree;
    std::vector<Blake3Hash> empty_leaves;
    tree.build_tree(empty_leaves);
    
    Blake3Hash root = tree.get_root();
    Blake3Hash zero = Blake3Hash::zero();
    
    ASSERT_EQ(root, zero)
        << "Empty tree should have zero hash as root";
    
    ASSERT_TRUE(tree.is_empty())
        << "Empty tree should report is_empty() as true";
    
    ASSERT_EQ(tree.leaf_count(), 0)
        << "Empty tree should have leaf_count() of 0";
}

/**
 * Property: Single leaf tree has leaf as root
 * 
 * A Merkle tree with a single leaf should have that leaf hash as its root.
 */
TEST_F(MerkleProofVerificationPropertyTest, SingleLeafTreeHasLeafAsRoot) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate a single random leaf
        Blake3Hash leaf = generate_random_hash();
        std::vector<Blake3Hash> leaves = {leaf};
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        Blake3Hash root = tree.get_root();
        
        // Root should equal the leaf
        ASSERT_EQ(root, leaf)
            << "Single leaf tree should have leaf hash as root";
        
        // Generate proof for the single leaf
        auto proof_opt = tree.generate_proof(0);
        ASSERT_TRUE(proof_opt.has_value());
        auto proof = proof_opt.value();
        
        // Proof should have no sibling hashes
        ASSERT_TRUE(proof.sibling_hashes.empty())
            << "Single leaf proof should have no sibling hashes";
        
        // Proof should verify
        ASSERT_TRUE(MerkleTree::verify_proof(proof, root))
            << "Single leaf proof should verify";
    }
}

/**
 * Property: Invalid leaf index returns nullopt
 * 
 * Attempting to generate a proof for an invalid leaf index should return nullopt.
 */
TEST_F(MerkleProofVerificationPropertyTest, InvalidLeafIndexReturnsNullopt) {
    const int NUM_TRIALS = 100;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random tree size (1 to 50 leaves)
        std::uniform_int_distribution<size_t> size_dist(1, 50);
        size_t num_leaves = size_dist(rng_);
        
        // Generate random leaf hashes
        std::vector<Blake3Hash> leaves = generate_random_hashes(num_leaves);
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        
        // Try to generate proof for invalid indices
        std::vector<size_t> invalid_indices = {
            num_leaves,      // Just beyond valid range
            num_leaves + 1,  // Further beyond
            num_leaves + 100,
            SIZE_MAX         // Maximum size_t value
        };
        
        for (size_t invalid_index : invalid_indices) {
            auto proof_opt = tree.generate_proof(invalid_index);
            ASSERT_FALSE(proof_opt.has_value())
                << "Should return nullopt for invalid index " << invalid_index
                << " in tree with " << num_leaves << " leaves";
        }
    }
}

/**
 * Property: Proof serialization round-trip preserves proof
 * 
 * Serializing and deserializing a proof should preserve its value.
 */
TEST_F(MerkleProofVerificationPropertyTest, ProofSerializationRoundTrip) {
    const int NUM_TRIALS = 200;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random tree size (1 to 50 leaves)
        std::uniform_int_distribution<size_t> size_dist(1, 50);
        size_t num_leaves = size_dist(rng_);
        
        // Generate random leaf hashes
        std::vector<Blake3Hash> leaves = generate_random_hashes(num_leaves);
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        Blake3Hash root = tree.get_root();
        
        // Pick a random leaf
        std::uniform_int_distribution<size_t> leaf_dist(0, num_leaves - 1);
        size_t leaf_index = leaf_dist(rng_);
        
        // Generate proof
        auto proof_opt = tree.generate_proof(leaf_index);
        ASSERT_TRUE(proof_opt.has_value());
        auto original_proof = proof_opt.value();
        
        // Serialize
        std::vector<uint8_t> serialized = original_proof.serialize();
        
        // Deserialize
        auto deserialized_proof = MerkleTree::MerkleProof::deserialize(serialized);
        
        // Check equality
        ASSERT_EQ(deserialized_proof.leaf_index, original_proof.leaf_index)
            << "Leaf index changed after serialization round-trip";
        ASSERT_EQ(deserialized_proof.leaf_hash, original_proof.leaf_hash)
            << "Leaf hash changed after serialization round-trip";
        ASSERT_EQ(deserialized_proof.sibling_hashes.size(), original_proof.sibling_hashes.size())
            << "Number of sibling hashes changed after serialization round-trip";
        
        for (size_t i = 0; i < original_proof.sibling_hashes.size(); ++i) {
            ASSERT_EQ(deserialized_proof.sibling_hashes[i], original_proof.sibling_hashes[i])
                << "Sibling hash " << i << " changed after serialization round-trip";
        }
        
        // Deserialized proof should still verify
        ASSERT_TRUE(MerkleTree::verify_proof(deserialized_proof, root))
            << "Deserialized proof should verify";
    }
}

/**
 * Property: Different trees produce different roots
 * 
 * Trees with different leaves should produce different roots
 * (with overwhelming probability).
 */
TEST_F(MerkleProofVerificationPropertyTest, DifferentTreesProduceDifferentRoots) {
    const int NUM_TRIALS = 200;
    std::set<std::string> seen_roots;

    for (int trial = 0; trial < NUM_TRIALS; ++trial) {
        // Generate random tree size (1 to 50 leaves)
        std::uniform_int_distribution<size_t> size_dist(1, 50);
        size_t num_leaves = size_dist(rng_);
        
        // Generate random leaf hashes
        std::vector<Blake3Hash> leaves = generate_random_hashes(num_leaves);
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        Blake3Hash root = tree.get_root();
        
        std::string root_hex = root.to_hex();
        
        // Check for collision (should be extremely rare)
        ASSERT_EQ(seen_roots.count(root_hex), 0)
            << "Unexpected root collision for different trees";
        
        seen_roots.insert(root_hex);
    }
}

/**
 * Property: Proof size is logarithmic in tree size
 * 
 * For a tree with N leaves, the proof should contain approximately log2(N) sibling hashes.
 */
TEST_F(MerkleProofVerificationPropertyTest, ProofSizeIsLogarithmic) {
    // Test specific tree sizes
    std::vector<std::pair<size_t, size_t>> size_and_expected_proof_length = {
        {1, 0},      // Single leaf: no siblings
        {2, 1},      // 2 leaves: 1 sibling
        {3, 2},      // 3 leaves: 2 siblings (rounded up)
        {4, 2},      // 4 leaves: 2 siblings
        {8, 3},      // 8 leaves: 3 siblings
        {16, 4},     // 16 leaves: 4 siblings
        {32, 5},     // 32 leaves: 5 siblings
        {64, 6},     // 64 leaves: 6 siblings
        {128, 7},    // 128 leaves: 7 siblings
        {256, 8},    // 256 leaves: 8 siblings
        {500, 9}     // 500 leaves: 9 siblings (ceil(log2(500)))
    };

    for (const auto& [num_leaves, expected_length] : size_and_expected_proof_length) {
        // Generate random leaf hashes
        std::vector<Blake3Hash> leaves = generate_random_hashes(num_leaves);
        
        // Build Merkle tree
        MerkleTree tree;
        tree.build_tree(leaves);
        
        // Generate proof for first leaf
        auto proof_opt = tree.generate_proof(0);
        ASSERT_TRUE(proof_opt.has_value());
        auto proof = proof_opt.value();
        
        // Check proof length
        ASSERT_EQ(proof.sibling_hashes.size(), expected_length)
            << "Proof length incorrect for tree with " << num_leaves << " leaves";
    }
}

/**
 * Property: Proof size meets light client requirement (≤5 KB for N=500)
 * 
 * For a validator set with 500 validators, the Merkle proof should be ≤5 KB.
 * This validates Requirement 5.8.
 */
TEST_F(MerkleProofVerificationPropertyTest, ProofSizeMeetsLightClientRequirement) {
    const size_t MAX_VALIDATORS = 500;
    
    // Generate 500 random validator hashes
    std::vector<Blake3Hash> validators = generate_random_hashes(MAX_VALIDATORS);
    
    // Build Merkle tree
    MerkleTree tree;
    tree.build_tree(validators);
    
    // Test proofs for several validators
    std::vector<size_t> test_indices = {0, MAX_VALIDATORS / 4, MAX_VALIDATORS / 2, 3 * MAX_VALIDATORS / 4, MAX_VALIDATORS - 1};
    for (size_t i : test_indices) {
        auto proof_opt = tree.generate_proof(i);
        ASSERT_TRUE(proof_opt.has_value());
        auto proof = proof_opt.value();
        
        // Calculate proof size
        std::vector<uint8_t> serialized = proof.serialize();
        size_t proof_size = serialized.size();
        
        // Proof structure:
        // - leaf_index: 8 bytes
        // - leaf_hash: 32 bytes
        // - num_siblings: 8 bytes
        // - sibling_hashes: 32 bytes each (9 siblings for 500 leaves)
        // Total: 8 + 32 + 8 + (9 * 32) = 336 bytes
        
        const size_t MAX_PROOF_SIZE = 5 * 1024; // 5 KB
        ASSERT_LE(proof_size, MAX_PROOF_SIZE)
            << "Proof size " << proof_size << " bytes exceeds 5 KB limit for validator " << i;
        
        // Verify the proof still works
        ASSERT_TRUE(MerkleTree::verify_proof(proof, tree.get_root()))
            << "Proof verification failed for validator " << i;
    }
}
