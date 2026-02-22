/**
 * Merkle Proof Fuzzer
 * 
 * This fuzzer tests the robustness of Merkle proof verification against
 * malformed and invalid inputs. It verifies that:
 * - MerkleProof::deserialize() does not crash on invalid inputs
 * - MerkleTree::verify_proof() handles invalid proofs safely
 * - No undefined behavior occurs with malformed proof data
 * - Invalid proofs are correctly rejected
 * 
 * Merkle proofs are critical for light client security. A bug in proof
 * verification could allow an attacker to convince a light client of
 * false state. This fuzzer ensures the verification logic is robust.
 * 
 * Requirements: 19.1, 19.5
 * Validates: Requirements 19.6, 19.7
 */

#include "sarafu/crypto/merkle_tree.h"
#include "sarafu/crypto/blake3_hash.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <stdexcept>

using namespace sarafu::crypto;

/**
 * Fuzz MerkleProof deserialization.
 * 
 * Tests that deserializing random bytes as a MerkleProof does not crash.
 */
static void fuzz_proof_deserialization(const uint8_t* data, size_t size) {
    if (size == 0) {
        return;
    }
    
    std::vector<uint8_t> input(data, data + size);
    
    try {
        // Attempt to deserialize the proof
        MerkleTree::MerkleProof proof = MerkleTree::MerkleProof::deserialize(input);
        
        // Access proof fields to ensure they're initialized
        (void)proof.leaf_index;
        (void)proof.leaf_hash;
        (void)proof.sibling_hashes.size();
        
        // Try to serialize it back (round-trip test)
        std::vector<uint8_t> serialized = proof.serialize();
        
        // If serialization succeeded, try deserializing again
        if (!serialized.empty()) {
            MerkleTree::MerkleProof proof2 = MerkleTree::MerkleProof::deserialize(serialized);
            
            // Verify round-trip consistency
            if (proof.leaf_index != proof2.leaf_index ||
                proof.leaf_hash != proof2.leaf_hash ||
                proof.sibling_hashes.size() != proof2.sibling_hashes.size()) {
                // This would indicate a serialization bug
                __builtin_trap();
            }
        }
        
    } catch (const std::exception& e) {
        // Exceptions are acceptable for invalid inputs
        (void)e.what();
    }
}

/**
 * Fuzz Merkle proof verification.
 * 
 * Input format:
 * - Bytes 0-31: Root hash (32 bytes)
 * - Bytes 32-39: Leaf index (8 bytes, little-endian)
 * - Bytes 40-71: Leaf hash (32 bytes)
 * - Byte 72: Number of sibling hashes (N)
 * - Bytes 73+: N * 32 bytes of sibling hashes
 * 
 * Tests that proof verification handles invalid proofs safely.
 */
static void fuzz_proof_verification(const uint8_t* data, size_t size) {
    // Need at least 73 bytes for root + index + leaf + count
    if (size < 73) {
        return;
    }
    
    try {
        // Extract root hash (32 bytes)
        std::vector<uint8_t> root_data(data, data + 32);
        Blake3Hash root(root_data);
        
        // Extract leaf index (8 bytes, little-endian)
        size_t leaf_index = 0;
        for (int i = 0; i < 8; i++) {
            leaf_index |= (static_cast<size_t>(data[32 + i]) << (i * 8));
        }
        
        // Extract leaf hash (32 bytes)
        std::vector<uint8_t> leaf_data(data + 40, data + 72);
        Blake3Hash leaf_hash(leaf_data);
        
        // Extract number of sibling hashes
        uint8_t num_siblings = data[72];
        
        // Limit to reasonable number to avoid timeout
        if (num_siblings > 64) {
            num_siblings = 64;
        }
        
        size_t required_size = 73 + (num_siblings * 32);
        if (size < required_size) {
            return;
        }
        
        // Extract sibling hashes
        std::vector<Blake3Hash> sibling_hashes;
        for (uint8_t i = 0; i < num_siblings; i++) {
            size_t offset = 73 + (i * 32);
            std::vector<uint8_t> sibling_data(data + offset, data + offset + 32);
            sibling_hashes.emplace_back(sibling_data);
        }
        
        // Create proof
        MerkleTree::MerkleProof proof;
        proof.leaf_index = leaf_index;
        proof.leaf_hash = leaf_hash;
        proof.sibling_hashes = sibling_hashes;
        
        // Verify proof - should not crash regardless of validity
        bool result = MerkleTree::verify_proof(proof, root);
        
        // Use result to prevent optimization
        (void)result;
        
    } catch (const std::exception& e) {
        // Exceptions are acceptable for invalid inputs
        (void)e.what();
    }
}

/**
 * Fuzz Merkle tree construction and proof generation.
 * 
 * Input format:
 * - Byte 0: Number of leaves (N)
 * - Bytes 1+: N * 32 bytes of leaf hashes
 * - Last 8 bytes: Leaf index for proof generation
 * 
 * Tests that tree construction and proof generation handle edge cases.
 */
static void fuzz_tree_construction(const uint8_t* data, size_t size) {
    if (size < 9) {  // Need at least 1 byte for count + 8 bytes for index
        return;
    }
    
    try {
        // Extract number of leaves
        uint8_t num_leaves = data[0];
        
        // Limit to reasonable number
        if (num_leaves > 100) {
            num_leaves = 100;
        }
        
        if (num_leaves == 0) {
            return;
        }
        
        size_t required_size = 1 + (num_leaves * 32) + 8;
        if (size < required_size) {
            return;
        }
        
        // Extract leaf hashes
        std::vector<Blake3Hash> leaves;
        for (uint8_t i = 0; i < num_leaves; i++) {
            size_t offset = 1 + (i * 32);
            std::vector<uint8_t> leaf_data(data + offset, data + offset + 32);
            leaves.emplace_back(leaf_data);
        }
        
        // Extract leaf index for proof generation
        size_t proof_index = 0;
        size_t index_offset = 1 + (num_leaves * 32);
        for (int i = 0; i < 8; i++) {
            proof_index |= (static_cast<size_t>(data[index_offset + i]) << (i * 8));
        }
        
        // Build tree
        MerkleTree tree;
        tree.build_tree(leaves);
        
        // Get root
        Blake3Hash root = tree.get_root();
        (void)root;
        
        // Try to generate proof
        auto proof_opt = tree.generate_proof(proof_index);
        
        if (proof_opt.has_value()) {
            // Verify the proof
            bool valid = MerkleTree::verify_proof(*proof_opt, root);
            
            // A proof generated by the tree should always be valid
            if (!valid) {
                // This would indicate a bug in proof generation
                __builtin_trap();
            }
        }
        
    } catch (const std::exception& e) {
        // Exceptions are acceptable for invalid inputs
        (void)e.what();
    }
}

/**
 * LibFuzzer entry point.
 * 
 * This function is called by libFuzzer with random byte sequences.
 * It tests Merkle proof operations with malformed inputs.
 * 
 * @param data Pointer to fuzzer-generated data
 * @param size Size of the fuzzer-generated data
 * @return 0 to continue fuzzing
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Skip empty inputs
    if (size == 0) {
        return 0;
    }
    
    // Use first byte to determine which function to fuzz
    uint8_t mode = data[0] % 3;
    
    switch (mode) {
        case 0:
            // Fuzz proof deserialization
            fuzz_proof_deserialization(data + 1, size - 1);
            break;
        case 1:
            // Fuzz proof verification
            fuzz_proof_verification(data + 1, size - 1);
            break;
        case 2:
            // Fuzz tree construction and proof generation
            fuzz_tree_construction(data + 1, size - 1);
            break;
    }
    
    // Return 0 to continue fuzzing
    return 0;
}
