#include "sarafu/crypto/merkle_tree.h"
#include <cstring>
#include <stdexcept>

namespace sarafu {
namespace crypto {

MerkleTree::MerkleTree() : root_(Blake3Hash::zero()) {}

void MerkleTree::build_tree(const std::vector<Blake3Hash>& leaf_hashes) {
    leaves_ = leaf_hashes;
    tree_levels_.clear();
    
    // Handle edge cases
    if (leaves_.empty()) {
        root_ = Blake3Hash::zero();
        return;
    }
    
    if (leaves_.size() == 1) {
        root_ = leaves_[0];
        tree_levels_.push_back(leaves_);
        return;
    }
    
    // Build the tree bottom-up
    build_internal_tree();
}

void MerkleTree::build_internal_tree() {
    // Start with the leaves as the first level
    tree_levels_.push_back(leaves_);
    
    // Build each level by hashing pairs from the previous level
    std::vector<Blake3Hash> current_level = leaves_;
    
    while (current_level.size() > 1) {
        std::vector<Blake3Hash> next_level;
        
        // Process pairs
        for (size_t i = 0; i < current_level.size(); i += 2) {
            Blake3Hash left = current_level[i];
            Blake3Hash right;
            
            // If odd number of nodes, duplicate the last one
            if (i + 1 < current_level.size()) {
                right = current_level[i + 1];
            } else {
                right = left;
            }
            
            Blake3Hash parent = hash_pair(left, right);
            next_level.push_back(parent);
        }
        
        tree_levels_.push_back(next_level);
        current_level = next_level;
    }
    
    // The last level contains only the root
    root_ = current_level[0];
}

Blake3Hash MerkleTree::get_root() const {
    return root_;
}

std::optional<MerkleTree::MerkleProof> MerkleTree::generate_proof(size_t leaf_index) const {
    // Validate index
    if (leaf_index >= leaves_.size()) {
        return std::nullopt;
    }
    
    // Handle single leaf case
    if (leaves_.size() == 1) {
        MerkleProof proof;
        proof.leaf_index = leaf_index;
        proof.leaf_hash = leaves_[0];
        proof.sibling_hashes = {}; // No siblings needed
        return proof;
    }
    
    MerkleProof proof;
    proof.leaf_index = leaf_index;
    proof.leaf_hash = leaves_[leaf_index];
    
    // Traverse from leaf to root, collecting sibling hashes
    size_t current_index = leaf_index;
    
    for (size_t level = 0; level < tree_levels_.size() - 1; ++level) {
        const auto& level_nodes = tree_levels_[level];
        
        // Get the sibling hash
        Blake3Hash sibling = get_sibling(level_nodes, current_index);
        proof.sibling_hashes.push_back(sibling);
        
        // Move to parent index in next level
        current_index = current_index / 2;
    }
    
    return proof;
}

bool MerkleTree::verify_proof(const MerkleProof& proof, const Blake3Hash& root) {
    // Start with the leaf hash
    Blake3Hash current_hash = proof.leaf_hash;
    size_t current_index = proof.leaf_index;
    
    // Handle empty proof (single leaf case)
    if (proof.sibling_hashes.empty()) {
        return current_hash == root;
    }
    
    // Traverse up the tree, hashing with siblings
    for (const auto& sibling : proof.sibling_hashes) {
        // Determine if current node is left or right child
        if (current_index % 2 == 0) {
            // Current node is left child
            current_hash = hash_pair(current_hash, sibling);
        } else {
            // Current node is right child
            current_hash = hash_pair(sibling, current_hash);
        }
        
        // Move to parent index
        current_index = current_index / 2;
    }
    
    // Final hash should match the root
    return current_hash == root;
}

Blake3Hash MerkleTree::hash_pair(const Blake3Hash& left, const Blake3Hash& right) {
    // Concatenate the two hashes and hash the result
    std::vector<uint8_t> combined;
    combined.reserve(Blake3Hash::HASH_SIZE * 2);
    
    // Append left hash
    combined.insert(combined.end(), left.bytes(), left.bytes() + Blake3Hash::HASH_SIZE);
    
    // Append right hash
    combined.insert(combined.end(), right.bytes(), right.bytes() + Blake3Hash::HASH_SIZE);
    
    // Hash the concatenation
    return Blake3Hash::hash(combined);
}

Blake3Hash MerkleTree::get_sibling(const std::vector<Blake3Hash>& level_nodes, size_t index) {
    // Determine sibling index
    size_t sibling_index;
    if (index % 2 == 0) {
        // Current node is left child, sibling is right
        sibling_index = index + 1;
    } else {
        // Current node is right child, sibling is left
        sibling_index = index - 1;
    }
    
    // If sibling exists, return it; otherwise duplicate current node
    if (sibling_index < level_nodes.size()) {
        return level_nodes[sibling_index];
    } else {
        return level_nodes[index];
    }
}

std::vector<uint8_t> MerkleTree::MerkleProof::serialize() const {
    std::vector<uint8_t> result;
    
    // Serialize leaf_index (8 bytes)
    result.resize(8);
    std::memcpy(result.data(), &leaf_index, 8);
    
    // Serialize leaf_hash (32 bytes)
    auto leaf_bytes = leaf_hash.serialize();
    result.insert(result.end(), leaf_bytes.begin(), leaf_bytes.end());
    
    // Serialize number of sibling hashes (8 bytes)
    size_t num_siblings = sibling_hashes.size();
    size_t offset = result.size();
    result.resize(offset + 8);
    std::memcpy(result.data() + offset, &num_siblings, 8);
    
    // Serialize each sibling hash (32 bytes each)
    for (const auto& sibling : sibling_hashes) {
        auto sibling_bytes = sibling.serialize();
        result.insert(result.end(), sibling_bytes.begin(), sibling_bytes.end());
    }
    
    return result;
}

MerkleTree::MerkleProof MerkleTree::MerkleProof::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8 + 32 + 8) {
        throw std::invalid_argument("Invalid proof data: too short");
    }
    
    MerkleProof proof;
    size_t offset = 0;
    
    // Deserialize leaf_index
    std::memcpy(&proof.leaf_index, data.data() + offset, 8);
    offset += 8;
    
    // Deserialize leaf_hash
    std::vector<uint8_t> leaf_bytes(data.begin() + offset, data.begin() + offset + 32);
    proof.leaf_hash = Blake3Hash(leaf_bytes);
    offset += 32;
    
    // Deserialize number of sibling hashes
    size_t num_siblings;
    std::memcpy(&num_siblings, data.data() + offset, 8);
    offset += 8;
    
    // Validate size
    if (data.size() != offset + num_siblings * 32) {
        throw std::invalid_argument("Invalid proof data: size mismatch");
    }
    
    // Deserialize sibling hashes
    for (size_t i = 0; i < num_siblings; ++i) {
        std::vector<uint8_t> sibling_bytes(data.begin() + offset, data.begin() + offset + 32);
        proof.sibling_hashes.push_back(Blake3Hash(sibling_bytes));
        offset += 32;
    }
    
    return proof;
}

} // namespace crypto
} // namespace sarafu
