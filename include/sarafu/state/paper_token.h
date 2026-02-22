#pragma once

#include <cstdint>
#include <optional>
#include <vector>
#include "sarafu/state/account.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/ed25519.h"

namespace sarafu {
namespace state {

/**
 * PaperToken represents a hash-locked bearer instrument that can be transferred offline.
 * 
 * A paper token locks funds that can be redeemed by anyone who knows the secret,
 * or refunded to the creator after a time delay.
 * 
 * Security properties:
 * - Secret is never stored on-chain (only its Blake3 hash)
 * - Redemption requires signature binding secret to destination (prevents front-running)
 * - Token ID is unique and prevents replay attacks
 * - Refund delay enforced (7-365 days)
 * - Consumed flag prevents double-spending
 */
struct PaperToken {
    crypto::Blake3Hash token_id;        // Unique identifier: Blake3(hash_lock || creator || creation_height)
    uint64_t amount;                    // Amount locked in this token
    crypto::Blake3Hash hash_lock;       // Blake3(secret) - the hash of the secret
    Address creator_address;            // Original creator (can refund after delay)
    uint64_t creation_height;           // Block height when token was created
    uint64_t refund_height;             // Block height when refund becomes available
    bool consumed;                      // True if token has been redeemed or refunded
    
    // Constructors
    PaperToken();
    PaperToken(
        const crypto::Blake3Hash& token_id,
        uint64_t amt,
        const crypto::Blake3Hash& hash_lock,
        const Address& creator,
        uint64_t creation_height,
        uint64_t refund_height
    );
    
    /**
     * Compute the token ID from hash_lock, creator, and creation height.
     * 
     * token_id = Blake3(hash_lock || creator_address || creation_height)
     * 
     * This ensures each token has a unique identifier that cannot be replayed.
     */
    static crypto::Blake3Hash compute_token_id(
        const crypto::Blake3Hash& hash_lock,
        const Address& creator,
        uint64_t creation_height
    );
    
    /**
     * Check if the token can be refunded at the given block height.
     */
    bool can_refund(uint64_t current_height) const;
    
    /**
     * Check if the token is still redeemable (not consumed and not yet refundable).
     */
    bool is_redeemable(uint64_t current_height) const;
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static PaperToken deserialize(const std::vector<uint8_t>& data);
    
    // Hash for Merkle tree
    crypto::Blake3Hash hash() const;
    
    // Comparison
    bool operator==(const PaperToken& other) const;
    bool operator!=(const PaperToken& other) const;
};

/**
 * CreatePaperTokenTx creates a new paper token by locking funds.
 * 
 * The transaction:
 * - Deducts amount + fee from creator's balance
 * - Creates a new PaperToken in the state tree
 * - Stores only the hash of the secret (not the secret itself)
 * 
 * The creator must:
 * - Generate a cryptographically strong secret (≥256 bits / 32 bytes)
 * - Compute hash_lock = Blake3(secret)
 * - Optionally encrypt the secret with a password (off-chain)
 * - Print or store the secret securely for later redemption
 */
struct CreatePaperTokenTx {
    Address from;                       // Creator's address
    uint64_t amount;                    // Amount to lock in the token
    crypto::Blake3Hash hash_lock;       // Blake3(secret)
    uint32_t refund_delay_days;         // Days until refund (7-365)
    uint64_t nonce;                     // Creator's nonce
    uint64_t fee;                       // Transaction fee
    uint64_t gas_limit;                 // Gas limit
    uint32_t chain_id;                  // Chain ID for replay protection
    crypto::Ed25519_Signature signature; // Signature over transaction hash
    
    // Constructors
    CreatePaperTokenTx();
    CreatePaperTokenTx(
        const Address& from_addr,
        uint64_t amt,
        const crypto::Blake3Hash& hash_lock,
        uint32_t refund_delay,
        uint64_t n,
        uint64_t f,
        uint64_t gas,
        uint32_t chain
    );
    
    // Compute transaction hash for signing
    crypto::Blake3Hash hash() const;
    
    // Serialization
    std::vector<uint8_t> serialize_for_signing() const;
    std::vector<uint8_t> serialize() const;
    static CreatePaperTokenTx deserialize(const std::vector<uint8_t>& data);
    
    // Signing and verification
    void sign(const crypto::Ed25519_PrivateKey& private_key);
    bool verify_signature(const crypto::Ed25519_PublicKey& public_key) const;
    
    // Validation
    static constexpr uint32_t MIN_REFUND_DELAY_DAYS = 7;
    static constexpr uint32_t MAX_REFUND_DELAY_DAYS = 365;
    
    bool has_valid_refund_delay() const {
        return refund_delay_days >= MIN_REFUND_DELAY_DAYS && 
               refund_delay_days <= MAX_REFUND_DELAY_DAYS;
    }
    
    // Comparison
    bool operator==(const CreatePaperTokenTx& other) const;
    bool operator!=(const CreatePaperTokenTx& other) const;
};

/**
 * RedeemPaperTokenTx redeems a paper token by revealing the secret.
 * 
 * The transaction:
 * - Verifies Blake3(secret) == token.hash_lock
 * - Verifies redeemer's signature over (hash(secret) || destination || nonce)
 * - Transfers token.amount to destination address
 * - Marks token as consumed
 * 
 * Front-running protection:
 * - The redeemer must sign the redemption with their own key
 * - The signature binds the secret to a specific destination address
 * - An attacker who sees the secret cannot redirect funds to their address
 */
struct RedeemPaperTokenTx {
    crypto::Blake3Hash token_id;        // ID of token to redeem
    std::vector<uint8_t> secret;        // The raw secret (≥32 bytes)
    Address destination;                // Where to send the funds
    Address redeemer;                   // Redeemer's address (for signature verification)
    uint64_t nonce;                     // Redeemer's nonce
    uint64_t fee;                       // Transaction fee
    uint64_t gas_limit;                 // Gas limit
    uint32_t chain_id;                  // Chain ID for replay protection
    crypto::Ed25519_Signature signature; // Signature over (hash(secret) || destination || nonce)
    
    // Constructors
    RedeemPaperTokenTx();
    RedeemPaperTokenTx(
        const crypto::Blake3Hash& token_id,
        const std::vector<uint8_t>& secret,
        const Address& dest,
        const Address& redeemer_addr,
        uint64_t n,
        uint64_t f,
        uint64_t gas,
        uint32_t chain
    );
    
    /**
     * Compute the message that must be signed for redemption.
     * 
     * message = hash(secret) || destination || nonce
     * 
     * This binds the secret to a specific destination, preventing front-running.
     */
    crypto::Blake3Hash compute_signature_message() const;
    
    // Compute transaction hash
    crypto::Blake3Hash hash() const;
    
    // Serialization
    std::vector<uint8_t> serialize_for_signing() const;
    std::vector<uint8_t> serialize() const;
    static RedeemPaperTokenTx deserialize(const std::vector<uint8_t>& data);
    
    // Signing and verification
    void sign(const crypto::Ed25519_PrivateKey& private_key);
    bool verify_signature(const crypto::Ed25519_PublicKey& public_key) const;
    
    // Validation
    static constexpr size_t MIN_SECRET_SIZE = 32;  // 256 bits
    
    bool has_valid_secret_size() const {
        return secret.size() >= MIN_SECRET_SIZE;
    }
    
    /**
     * Verify that Blake3(secret) matches the expected hash_lock.
     */
    bool verify_secret(const crypto::Blake3Hash& hash_lock) const;
    
    // Comparison
    bool operator==(const RedeemPaperTokenTx& other) const;
    bool operator!=(const RedeemPaperTokenTx& other) const;
};

/**
 * RefundPaperTokenTx refunds a paper token to the creator after the delay.
 * 
 * The transaction:
 * - Verifies current_height >= token.refund_height
 * - Verifies signature is from token.creator_address
 * - Transfers token.amount to specified destination (must be creator-controlled)
 * - Marks token as consumed
 * 
 * The creator can specify any destination address they control.
 */
struct RefundPaperTokenTx {
    crypto::Blake3Hash token_id;        // ID of token to refund
    Address destination;                // Where to send the refund
    Address from;                       // Creator's address (must match token.creator_address)
    uint64_t nonce;                     // Creator's nonce
    uint64_t fee;                       // Transaction fee
    uint64_t gas_limit;                 // Gas limit
    uint32_t chain_id;                  // Chain ID for replay protection
    crypto::Ed25519_Signature signature; // Signature over transaction hash
    
    // Constructors
    RefundPaperTokenTx();
    RefundPaperTokenTx(
        const crypto::Blake3Hash& token_id,
        const Address& dest,
        const Address& from_addr,
        uint64_t n,
        uint64_t f,
        uint64_t gas,
        uint32_t chain
    );
    
    // Compute transaction hash for signing
    crypto::Blake3Hash hash() const;
    
    // Serialization
    std::vector<uint8_t> serialize_for_signing() const;
    std::vector<uint8_t> serialize() const;
    static RefundPaperTokenTx deserialize(const std::vector<uint8_t>& data);
    
    // Signing and verification
    void sign(const crypto::Ed25519_PrivateKey& private_key);
    bool verify_signature(const crypto::Ed25519_PublicKey& public_key) const;
    
    // Comparison
    bool operator==(const RefundPaperTokenTx& other) const;
    bool operator!=(const RefundPaperTokenTx& other) const;
};

} // namespace state
} // namespace sarafu
