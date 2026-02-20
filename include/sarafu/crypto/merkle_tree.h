#pragma once

#include "sarafu/crypto/blake3_hash.h"
#include <vector>
#include <optional>

namespace sarafu {
namespace crypto {

/**
 * MerkleTree implements a binary Merkle tree for state commitments.
 * 
 * The tree uses Blake3 for all internal node hashing and provides
 * efficient proof generation and verification for leaf inclusion.
 * 
 * Tree structure:
 *                    Root
 *                   /    \
 *                  /      \
 *                 /        \
 *              H(A,B)     H(C,D)
 *              /  \        /  \
 *             A    B      C    D
 * 
 * Where A, B, C, D are leaf hashes.
 * 
 * Proof for leaf A: [B, H(C,D)]
 * Verification: Root == H(H(A, B), H(C,D))
 * 
 * Requirements: 5.4, 5.8, 10.5
 */
class MerkleTree {
public:
    /**
     * MerkleProof represents a proof of inclusion for a leaf in the tree.
     * Contains the sibling hashes needed to reconstruct the path to the root.
     */
    struct MerkleProof {
        size_t leaf_index;                    // Index of the leaf in the tree
        Blake3Hash leaf_hash;                 // Hash of the leaf
        std::vector<Blake3Hash> sibling_hashes; // Sibling hashes along the path to root
        
        // Serialization
        std::vector<uint8_t> serialize() const;
        static MerkleProof deserialize(const std::vector<uint8_t>& data);
    };

    // Constructors
    MerkleTree();
    
    /**
     * Build a Merkle tree from a list of leaf hashes.
     * 
     * @param leaf_hashes Vector of Blake3 hashes representing the leaves
     * 
     * Edge cases:
     * - Empty tree: root is zero hash
     * - Single leaf: root is the leaf hash itself
     * - Odd number of leaves: last leaf is duplicated to make pairs
     */
    void build_tree(const std::vector<Blake3Hash>& leaf_hashes);
    
    /**
     * Get the Merkle root of the tree.
     * 
     * @return The root hash of the tree, or zero hash if tree is empty
     */
    Blake3Hash get_root() const;
    
    /**
     * Generate a Merkle proof for a leaf at the given index.
     * 
     * @param leaf_index Index of the leaf (0-based)
     * @return MerkleProof containing the sibling hashes, or std::nullopt if index is invalid
     */
    std::optional<MerkleProof> generate_proof(size_t leaf_index) const;
    
    /**
     * Verify a Merkle proof against a given root.
     * 
     * @param proof The Merkle proof to verify
     * @param root The expected root hash
     * @return true if the proof is valid, false otherwise
     */
    static bool verify_proof(const MerkleProof& proof, const Blake3Hash& root);
    
    /**
     * Get the number of leaves in the tree.
     */
    size_t leaf_count() const { return leaves_.size(); }
    
    /**
     * Check if the tree is empty.
     */
    bool is_empty() const { return leaves_.empty(); }

private:
    /**
     * Compute the hash of two child nodes.
     * Uses Blake3 to hash the concatenation of left and right hashes.
     */
    static Blake3Hash hash_pair(const Blake3Hash& left, const Blake3Hash& right);
    
    /**
     * Build the internal tree structure from leaves.
     * Constructs a complete binary tree by repeatedly hashing pairs.
     */
    void build_internal_tree();
    
    /**
     * Get the sibling hash for a node at a given index in a level.
     * 
     * @param level_nodes The nodes at the current level
     * @param index The index of the node
     * @return The sibling hash
     */
    static Blake3Hash get_sibling(const std::vector<Blake3Hash>& level_nodes, size_t index);

    std::vector<Blake3Hash> leaves_;  // Leaf hashes
    Blake3Hash root_;                 // Root hash of the tree
    
    // Internal tree structure: vector of levels, where each level is a vector of hashes
    // Level 0 is the leaves, and the last level contains only the root
    std::vector<std::vector<Blake3Hash>> tree_levels_;
};

} // namespace crypto
} // namespace sarafu
