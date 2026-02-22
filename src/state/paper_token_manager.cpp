#include "sarafu/state/paper_token_manager.h"
#include <algorithm>
#include <stdexcept>

namespace sarafu {
namespace state {

PaperTokenManager::PaperTokenManager() {}

PaperTokenManager::~PaperTokenManager() {}

bool PaperTokenManager::create_token(const PaperToken& token) {
    if (token_exists(token.token_id)) {
        return false;
    }
    
    tokens_[token.token_id] = token;
    return true;
}

std::optional<PaperToken> PaperTokenManager::get_token(const crypto::Blake3Hash& token_id) const {
    auto it = tokens_.find(token_id);
    if (it == tokens_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool PaperTokenManager::consume_token(const crypto::Blake3Hash& token_id) {
    auto it = tokens_.find(token_id);
    if (it == tokens_.end()) {
        return false;
    }
    
    it->second.consumed = true;
    return true;
}

bool PaperTokenManager::token_exists(const crypto::Blake3Hash& token_id) const {
    return tokens_.find(token_id) != tokens_.end();
}

bool PaperTokenManager::is_token_consumed(const crypto::Blake3Hash& token_id) const {
    auto it = tokens_.find(token_id);
    if (it == tokens_.end()) {
        return false;
    }
    return it->second.consumed;
}

std::vector<PaperToken> PaperTokenManager::get_all_tokens() const {
    std::vector<PaperToken> result;
    result.reserve(tokens_.size());
    
    for (const auto& [id, token] : tokens_) {
        result.push_back(token);
    }
    
    return result;
}

std::vector<PaperToken> PaperTokenManager::get_tokens_by_creator(const Address& creator) const {
    std::vector<PaperToken> result;
    
    for (const auto& [id, token] : tokens_) {
        if (token.creator_address == creator) {
            result.push_back(token);
        }
    }
    
    return result;
}

crypto::Blake3Hash PaperTokenManager::compute_merkle_root() const {
    if (tokens_.empty()) {
        return crypto::Blake3Hash::zero();
    }
    
    // Collect all token hashes and sort them
    std::vector<crypto::Blake3Hash> leaves;
    leaves.reserve(tokens_.size());
    
    for (const auto& [id, token] : tokens_) {
        leaves.push_back(token.hash());
    }
    
    std::sort(leaves.begin(), leaves.end());
    
    return build_merkle_tree(leaves);
}

std::vector<crypto::Blake3Hash> PaperTokenManager::generate_merkle_proof(
    const crypto::Blake3Hash& token_id
) const {
    auto token_opt = get_token(token_id);
    if (!token_opt) {
        return {};
    }
    
    // Collect all token hashes and sort them
    std::vector<crypto::Blake3Hash> leaves;
    leaves.reserve(tokens_.size());
    
    for (const auto& [id, token] : tokens_) {
        leaves.push_back(token.hash());
    }
    
    std::sort(leaves.begin(), leaves.end());
    
    // Find the index of our token
    crypto::Blake3Hash target_hash = token_opt->hash();
    auto it = std::find(leaves.begin(), leaves.end(), target_hash);
    if (it == leaves.end()) {
        return {};
    }
    
    size_t index = std::distance(leaves.begin(), it);
    
    // Generate proof by collecting sibling hashes
    std::vector<crypto::Blake3Hash> proof;
    std::vector<crypto::Blake3Hash> current_level = leaves;
    
    while (current_level.size() > 1) {
        std::vector<crypto::Blake3Hash> next_level;
        
        for (size_t i = 0; i < current_level.size(); i += 2) {
            // Check if our target node is in this pair
            if (i <= index && index < i + 2) {
                // Add the sibling to the proof
                if (index % 2 == 0) {
                    // Target is left child, add right sibling
                    if (i + 1 < current_level.size()) {
                        proof.push_back(current_level[i + 1]);
                    } else {
                        // Odd number of nodes, duplicate the last one
                        proof.push_back(current_level[i]);
                    }
                } else {
                    // Target is right child, add left sibling
                    proof.push_back(current_level[i]);
                }
            }
            
            // Build next level using sorted order
            if (i + 1 < current_level.size()) {
                const auto& left = current_level[i];
                const auto& right = current_level[i + 1];
                if (left < right) {
                    next_level.push_back(merkle_parent(left, right));
                } else {
                    next_level.push_back(merkle_parent(right, left));
                }
            } else {
                // Odd number of nodes, duplicate the last one
                next_level.push_back(merkle_parent(current_level[i], current_level[i]));
            }
        }
        
        // Update index for next level
        index = index / 2;
        current_level = next_level;
    }
    
    return proof;
}

bool PaperTokenManager::verify_merkle_proof(
    const PaperToken& token,
    const std::vector<crypto::Blake3Hash>& proof,
    const crypto::Blake3Hash& root
) {
    if (proof.empty()) {
        // Single token case or empty tree
        return root == token.hash() || root == crypto::Blake3Hash::zero();
    }
    
    crypto::Blake3Hash current_hash = token.hash();
    
    for (const auto& sibling : proof) {
        // Always use sorted order (smaller hash first) for consistency
        if (current_hash < sibling) {
            current_hash = merkle_parent(current_hash, sibling);
        } else {
            current_hash = merkle_parent(sibling, current_hash);
        }
    }
    
    return current_hash == root;
}

uint64_t PaperTokenManager::get_total_locked() const {
    uint64_t total = 0;
    
    for (const auto& [id, token] : tokens_) {
        if (!token.consumed) {
            total += token.amount;
        }
    }
    
    return total;
}

size_t PaperTokenManager::get_token_count() const {
    return tokens_.size();
}

size_t PaperTokenManager::get_active_token_count() const {
    size_t count = 0;
    
    for (const auto& [id, token] : tokens_) {
        if (!token.consumed) {
            ++count;
        }
    }
    
    return count;
}

void PaperTokenManager::clear() {
    tokens_.clear();
    snapshots_.clear();
}

void PaperTokenManager::save_snapshot(uint64_t height) {
    snapshots_[height] = tokens_;
}

bool PaperTokenManager::restore_snapshot(uint64_t height) {
    auto it = snapshots_.find(height);
    if (it == snapshots_.end()) {
        return false;
    }
    
    tokens_ = it->second;
    
    // Remove snapshots newer than this height
    auto upper = snapshots_.upper_bound(height);
    snapshots_.erase(upper, snapshots_.end());
    
    return true;
}

crypto::Blake3Hash PaperTokenManager::build_merkle_tree(
    const std::vector<crypto::Blake3Hash>& leaves
) {
    if (leaves.empty()) {
        return crypto::Blake3Hash::zero();
    }
    
    if (leaves.size() == 1) {
        return leaves[0];
    }
    
    std::vector<crypto::Blake3Hash> current_level = leaves;
    
    while (current_level.size() > 1) {
        std::vector<crypto::Blake3Hash> next_level;
        
        for (size_t i = 0; i < current_level.size(); i += 2) {
            if (i + 1 < current_level.size()) {
                const auto& left = current_level[i];
                const auto& right = current_level[i + 1];
                // Use sorted order for consistency
                if (left < right) {
                    next_level.push_back(merkle_parent(left, right));
                } else {
                    next_level.push_back(merkle_parent(right, left));
                }
            } else {
                // Odd number of nodes, duplicate the last one
                next_level.push_back(merkle_parent(current_level[i], current_level[i]));
            }
        }
        
        current_level = next_level;
    }
    
    return current_level[0];
}

crypto::Blake3Hash PaperTokenManager::merkle_parent(
    const crypto::Blake3Hash& left,
    const crypto::Blake3Hash& right
) {
    std::vector<uint8_t> data;
    data.reserve(64);
    
    auto left_bytes = left.serialize();
    auto right_bytes = right.serialize();
    
    data.insert(data.end(), left_bytes.begin(), left_bytes.end());
    data.insert(data.end(), right_bytes.begin(), right_bytes.end());
    
    return crypto::Blake3Hash::hash(data);
}

} // namespace state
} // namespace sarafu
