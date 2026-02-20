#include "sarafu/state/transaction_validator.h"
#include <sstream>

namespace sarafu {
namespace state {

TransactionValidator::TransactionValidator(uint32_t chain_id)
    : chain_id_(chain_id) {
}

TransactionValidator::~TransactionValidator() = default;

bool TransactionValidator::verify_signature(
    const Transaction& tx,
    const crypto::Ed25519_PublicKey& public_key
) const {
    return tx.verify_signature(public_key);
}

std::optional<std::string> TransactionValidator::validate_transaction(
    const Transaction& tx,
    const crypto::Ed25519_PublicKey& public_key,
    const AccountManager& account_manager
) const {
    // Check for zero addresses (Requirement 30.1, 30.2)
    if (is_zero_address(tx.from)) {
        return "Transaction from address cannot be zero address";
    }
    
    if (is_zero_address(tx.to)) {
        return "Transaction to address cannot be zero address";
    }
    
    // Check non-negative amounts (Requirement 30.3, 30.4)
    // Note: This is always true for uint64_t, but we check for completeness
    if (!has_non_negative_amounts(tx)) {
        return "Transaction amounts must be non-negative";
    }
    
    // Verify chain ID (Requirement 8.4, 30.7)
    if (!has_valid_chain_id(tx, chain_id_)) {
        std::ostringstream oss;
        oss << "Invalid chain ID: expected " << chain_id_ 
            << ", got " << tx.chain_id;
        return oss.str();
    }
    
    // Verify signature (Requirement 8.1, 8.2, 8.3)
    if (!verify_signature(tx, public_key)) {
        return "Invalid transaction signature";
    }
    
    // Verify nonce (Requirement 7.2, 7.4, 30.5)
    if (!has_valid_nonce(tx, account_manager)) {
        uint64_t expected_nonce = account_manager.get_nonce(tx.from);
        std::ostringstream oss;
        oss << "Invalid nonce: expected " << expected_nonce 
            << ", got " << tx.nonce;
        return oss.str();
    }
    
    // Check balance sufficiency (Requirement 7.8, 30.8)
    if (!has_sufficient_balance(tx, account_manager)) {
        uint64_t balance = account_manager.get_balance(tx.from);
        uint64_t required = tx.amount + tx.fee;
        std::ostringstream oss;
        oss << "Insufficient balance: have " << balance 
            << ", need " << required;
        return oss.str();
    }
    
    // All checks passed
    return std::nullopt;
}

bool TransactionValidator::is_zero_address(const Address& address) {
    return address.is_zero();
}

bool TransactionValidator::has_non_negative_amounts(const Transaction& tx) {
    // For uint64_t, amounts are always non-negative
    // This function exists for API completeness and future extensibility
    (void)tx; // Suppress unused parameter warning
    return true;
}

bool TransactionValidator::has_sufficient_balance(
    const Transaction& tx,
    const AccountManager& account_manager
) {
    uint64_t required = tx.amount + tx.fee;
    return account_manager.has_sufficient_balance(tx.from, required);
}

bool TransactionValidator::has_valid_nonce(
    const Transaction& tx,
    const AccountManager& account_manager
) {
    return account_manager.validate_nonce(tx.from, tx.nonce);
}

bool TransactionValidator::has_valid_chain_id(const Transaction& tx, uint32_t chain_id) {
    return tx.chain_id == chain_id;
}

} // namespace state
} // namespace sarafu
