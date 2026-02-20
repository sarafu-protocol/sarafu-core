#pragma once

#include <string>
#include <optional>
#include "sarafu/state/transaction.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/crypto/ed25519.h"

namespace sarafu {
namespace state {

/**
 * TransactionValidator validates transactions against all protocol rules.
 * 
 * This class is responsible for:
 * - Verifying Ed25519 signatures
 * - Checking for zero addresses
 * - Validating non-negative amounts
 * - Verifying nonce correctness
 * - Checking balance sufficiency
 * - Validating chain ID
 * 
 * Requirements: 8.1, 8.2, 8.3, 30.1, 30.2, 30.3, 30.4, 30.5, 30.7, 30.8
 */
class TransactionValidator {
public:
    /**
     * Construct a TransactionValidator with a chain ID.
     * 
     * @param chain_id The network chain ID for replay protection
     */
    explicit TransactionValidator(uint32_t chain_id);

    ~TransactionValidator();

    /**
     * Verify the Ed25519 signature on a transaction.
     * 
     * This checks that the signature is valid for the transaction hash
     * and the sender's public key.
     * 
     * @param tx The transaction to verify
     * @param public_key The sender's Ed25519 public key
     * @return true if signature is valid, false otherwise
     */
    bool verify_signature(
        const Transaction& tx,
        const crypto::Ed25519_PublicKey& public_key
    ) const;

    /**
     * Validate a transaction against all protocol rules.
     * 
     * This performs comprehensive validation including:
     * - Signature verification
     * - Zero address checks
     * - Non-negative amount validation
     * - Nonce verification
     * - Balance sufficiency check
     * - Chain ID verification
     * 
     * @param tx The transaction to validate
     * @param public_key The sender's Ed25519 public key
     * @param account_manager The account manager for state queries
     * @return std::nullopt if valid, error message if invalid
     */
    std::optional<std::string> validate_transaction(
        const Transaction& tx,
        const crypto::Ed25519_PublicKey& public_key,
        const AccountManager& account_manager
    ) const;

    /**
     * Check if an address is the zero address.
     * 
     * @param address The address to check
     * @return true if address is zero, false otherwise
     */
    static bool is_zero_address(const Address& address);

    /**
     * Check if transaction amounts are non-negative.
     * 
     * This is always true for uint64_t, but included for completeness
     * and to match the design specification.
     * 
     * @param tx The transaction to check
     * @return true (amounts are always non-negative for uint64_t)
     */
    static bool has_non_negative_amounts(const Transaction& tx);

    /**
     * Check if sender has sufficient balance for transaction.
     * 
     * @param tx The transaction
     * @param account_manager The account manager for balance queries
     * @return true if balance >= amount + fee, false otherwise
     */
    static bool has_sufficient_balance(
        const Transaction& tx,
        const AccountManager& account_manager
    );

    /**
     * Check if transaction nonce matches account nonce.
     * 
     * @param tx The transaction
     * @param account_manager The account manager for nonce queries
     * @return true if nonces match, false otherwise
     */
    static bool has_valid_nonce(
        const Transaction& tx,
        const AccountManager& account_manager
    );

    /**
     * Check if transaction chain ID matches network chain ID.
     * 
     * @param tx The transaction
     * @param chain_id The expected chain ID
     * @return true if chain IDs match, false otherwise
     */
    static bool has_valid_chain_id(const Transaction& tx, uint32_t chain_id);

    /**
     * Get the chain ID for this validator.
     * 
     * @return The chain ID
     */
    uint32_t get_chain_id() const { return chain_id_; }

private:
    uint32_t chain_id_;
};

} // namespace state
} // namespace sarafu
