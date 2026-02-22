#pragma once

#include <optional>
#include <string>
#include "sarafu/state/paper_token.h"
#include "sarafu/state/paper_token_manager.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/crypto/ed25519.h"

namespace sarafu {
namespace state {

/**
 * PaperTokenValidator validates paper token transactions.
 * 
 * This class enforces all protocol rules for paper tokens:
 * - Signature verification
 * - Balance sufficiency
 * - Refund delay bounds (7-365 days)
 * - Secret size requirements (≥256 bits)
 * - Hash lock verification
 * - Token existence and consumption checks
 * - Refund height enforcement
 * - Creator authorization for refunds
 */
class PaperTokenValidator {
public:
    /**
     * Construct a PaperTokenValidator with a chain ID.
     * 
     * @param chain_id The network chain ID for replay protection
     */
    explicit PaperTokenValidator(uint32_t chain_id);
    
    ~PaperTokenValidator();
    
    /**
     * Validate a CreatePaperTokenTx.
     * 
     * Checks:
     * - Signature is valid
     * - Nonce matches account nonce
     * - Balance >= amount + fee
     * - Refund delay is within bounds (7-365 days)
     * - Amount > 0
     * - Chain ID matches
     * 
     * @param tx The transaction to validate
     * @param public_key The creator's public key
     * @param account_manager The account manager for state queries
     * @param current_height The current block height
     * @return std::nullopt if valid, error message if invalid
     */
    std::optional<std::string> validate_create(
        const CreatePaperTokenTx& tx,
        const crypto::Ed25519_PublicKey& public_key,
        const AccountManager& account_manager,
        uint64_t current_height
    ) const;
    
    /**
     * Validate a RedeemPaperTokenTx.
     * 
     * Checks:
     * - Signature is valid (over hash(secret) || destination || nonce)
     * - Token exists
     * - Token is not consumed
     * - Secret size >= 32 bytes
     * - Blake3(secret) == token.hash_lock
     * - Current height < token.refund_height (not yet refundable)
     * - Redeemer has balance >= fee
     * - Nonce matches redeemer's account nonce
     * - Chain ID matches
     * 
     * @param tx The transaction to validate
     * @param public_key The redeemer's public key
     * @param account_manager The account manager for state queries
     * @param token_manager The paper token manager for token queries
     * @param current_height The current block height
     * @return std::nullopt if valid, error message if invalid
     */
    std::optional<std::string> validate_redeem(
        const RedeemPaperTokenTx& tx,
        const crypto::Ed25519_PublicKey& public_key,
        const AccountManager& account_manager,
        const PaperTokenManager& token_manager,
        uint64_t current_height
    ) const;
    
    /**
     * Validate a RefundPaperTokenTx.
     * 
     * Checks:
     * - Signature is valid
     * - Token exists
     * - Token is not consumed
     * - Current height >= token.refund_height
     * - tx.from == token.creator_address
     * - Creator has balance >= fee
     * - Nonce matches creator's account nonce
     * - Chain ID matches
     * 
     * @param tx The transaction to validate
     * @param public_key The creator's public key
     * @param account_manager The account manager for state queries
     * @param token_manager The paper token manager for token queries
     * @param current_height The current block height
     * @return std::nullopt if valid, error message if invalid
     */
    std::optional<std::string> validate_refund(
        const RefundPaperTokenTx& tx,
        const crypto::Ed25519_PublicKey& public_key,
        const AccountManager& account_manager,
        const PaperTokenManager& token_manager,
        uint64_t current_height
    ) const;
    
    /**
     * Get the chain ID for this validator.
     * 
     * @return The chain ID
     */
    uint32_t get_chain_id() const { return chain_id_; }

private:
    uint32_t chain_id_;
    
    /**
     * Check if an address is the zero address.
     */
    static bool is_zero_address(const Address& address);
    
    /**
     * Check if transaction chain ID matches network chain ID.
     */
    bool has_valid_chain_id(uint32_t tx_chain_id) const;
};

} // namespace state
} // namespace sarafu
