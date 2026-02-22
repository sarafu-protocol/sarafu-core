#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>
#include "sarafu/state/paper_token.h"
#include "sarafu/crypto/blake3_hash.h"

namespace sarafu {
namespace state {

/**
 * PaperTokenManager manages the state tree of paper tokens.
 * 
 * This class provides:
 * - CRUD operations for paper tokens
 * - Merkle tree computation for light client proofs
 * - Token lookup by ID
 * - Validation of token operations
 * 
 * The paper token state tree is separate from the account state tree
 * to enable efficient light client verification of token existence.
 */
class PaperTokenManager {
public:
    PaperTokenManager();
    ~PaperTokenManager();
    
    /**
     * Create a new paper token.
     * 
     * @param token The paper token to create
     * @return true if created successfully, false if token_id already exists
     */
    bool create_token(const PaperToken& token);
    
    /**
     * Get a paper token by ID.
     * 
     * @param token_id The token ID to look up
     * @return The token if found, std::nullopt otherwise
     */
    std::optional<PaperToken> get_token(const crypto::Blake3Hash& token_id) const;
    
    /**
     * Mark a token as consumed (after redemption or refund).
     * 
     * @param token_id The token ID to consume
     * @return true if consumed successfully, false if token not found
     */
    bool consume_token(const crypto::Blake3Hash& token_id);
    
    /**
     * Check if a token exists.
     * 
     * @param token_id The token ID to check
     * @return true if token exists, false otherwise
     */
    bool token_exists(const crypto::Blake3Hash& token_id) const;
    
    /**
     * Check if a token is consumed.
     * 
     * @param token_id The token ID to check
     * @return true if token is consumed, false if not consumed or not found
     */
    bool is_token_consumed(const crypto::Blake3Hash& token_id) const;
    
    /**
     * Get all tokens (for testing and debugging).
     * 
     * @return Vector of all paper tokens
     */
    std::vector<PaperToken> get_all_tokens() const;
    
    /**
     * Get all tokens created by a specific address.
     * 
     * @param creator The creator address
     * @return Vector of tokens created by this address
     */
    std::vector<PaperToken> get_tokens_by_creator(const Address& creator) const;
    
    /**
     * Compute the Merkle root of all paper tokens.
     * 
     * This root is included in the block header to enable light client
     * verification of token existence and spending.
     * 
     * @return Merkle root of all paper tokens
     */
    crypto::Blake3Hash compute_merkle_root() const;
    
    /**
     * Generate a Merkle proof for a specific token.
     * 
     * Light clients can use this proof to verify that a token exists
     * in the state tree without downloading all tokens.
     * 
     * @param token_id The token ID to generate proof for
     * @return Vector of sibling hashes for the Merkle proof, or empty if token not found
     */
    std::vector<crypto::Blake3Hash> generate_merkle_proof(
        const crypto::Blake3Hash& token_id
    ) const;
    
    /**
     * Verify a Merkle proof for a token.
     * 
     * @param token The token to verify
     * @param proof The Merkle proof (sibling hashes)
     * @param root The expected Merkle root
     * @return true if proof is valid, false otherwise
     */
    static bool verify_merkle_proof(
        const PaperToken& token,
        const std::vector<crypto::Blake3Hash>& proof,
        const crypto::Blake3Hash& root
    );
    
    /**
     * Get the total amount locked in all paper tokens.
     * 
     * @return Sum of amounts in all unconsumed tokens
     */
    uint64_t get_total_locked() const;
    
    /**
     * Get the number of paper tokens.
     * 
     * @return Total number of tokens (including consumed)
     */
    size_t get_token_count() const;
    
    /**
     * Get the number of unconsumed paper tokens.
     * 
     * @return Number of tokens that have not been redeemed or refunded
     */
    size_t get_active_token_count() const;
    
    /**
     * Clear all tokens (for testing).
     */
    void clear();
    
    /**
     * Create a snapshot of the current state.
     * 
     * @param height The block height for this snapshot
     */
    void save_snapshot(uint64_t height);
    
    /**
     * Restore state from a snapshot.
     * 
     * @param height The block height to restore to
     * @return true if snapshot exists and was restored, false otherwise
     */
    bool restore_snapshot(uint64_t height);

private:
    // Token storage: token_id -> PaperToken
    std::map<crypto::Blake3Hash, PaperToken> tokens_;
    
    // Snapshots for state reversion: height -> token map
    std::map<uint64_t, std::map<crypto::Blake3Hash, PaperToken>> snapshots_;
    
    /**
     * Build a binary Merkle tree from token hashes.
     * 
     * @param leaves Vector of leaf hashes (sorted)
     * @return Root hash of the Merkle tree
     */
    static crypto::Blake3Hash build_merkle_tree(
        const std::vector<crypto::Blake3Hash>& leaves
    );
    
    /**
     * Compute the parent hash in a Merkle tree.
     * 
     * @param left Left child hash
     * @param right Right child hash
     * @return Blake3(left || right)
     */
    static crypto::Blake3Hash merkle_parent(
        const crypto::Blake3Hash& left,
        const crypto::Blake3Hash& right
    );
};

} // namespace state
} // namespace sarafu
