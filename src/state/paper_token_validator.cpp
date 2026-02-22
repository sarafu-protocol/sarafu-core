#include "sarafu/state/paper_token_validator.h"

namespace sarafu {
namespace state {

PaperTokenValidator::PaperTokenValidator(uint32_t chain_id)
    : chain_id_(chain_id) {}

PaperTokenValidator::~PaperTokenValidator() {}

std::optional<std::string> PaperTokenValidator::validate_create(
    const CreatePaperTokenTx& tx,
    const crypto::Ed25519_PublicKey& public_key,
    const AccountManager& account_manager,
    uint64_t current_height
) const {
    // Check chain ID
    if (!has_valid_chain_id(tx.chain_id)) {
        return "Invalid chain ID";
    }
    
    // Check signature
    if (!tx.verify_signature(public_key)) {
        return "Invalid signature";
    }
    
    // Check zero address
    if (is_zero_address(tx.from)) {
        return "From address cannot be zero";
    }
    
    // Check amount > 0
    if (tx.amount == 0) {
        return "Amount must be greater than zero";
    }
    
    // Check refund delay bounds
    if (!tx.has_valid_refund_delay()) {
        return "Refund delay must be between 7 and 365 days";
    }
    
    // Check nonce
    auto account = account_manager.get_account(tx.from);
    
    if (tx.nonce != account.nonce) {
        return "Invalid nonce";
    }
    
    // Check balance
    uint64_t required = tx.amount + tx.fee;
    if (account.balance < required) {
        return "Insufficient balance";
    }
    
    return std::nullopt;
}

std::optional<std::string> PaperTokenValidator::validate_redeem(
    const RedeemPaperTokenTx& tx,
    const crypto::Ed25519_PublicKey& public_key,
    const AccountManager& account_manager,
    const PaperTokenManager& token_manager,
    uint64_t current_height
) const {
    // Check chain ID
    if (!has_valid_chain_id(tx.chain_id)) {
        return "Invalid chain ID";
    }
    
    // Check signature (over hash(secret) || destination || nonce)
    if (!tx.verify_signature(public_key)) {
        return "Invalid signature";
    }
    
    // Check zero addresses
    if (is_zero_address(tx.destination)) {
        return "Destination address cannot be zero";
    }
    
    if (is_zero_address(tx.redeemer)) {
        return "Redeemer address cannot be zero";
    }
    
    // Check secret size
    if (!tx.has_valid_secret_size()) {
        return "Secret must be at least 32 bytes";
    }
    
    // Check token exists
    auto token_opt = token_manager.get_token(tx.token_id);
    if (!token_opt) {
        return "Token does not exist";
    }
    
    const auto& token = *token_opt;
    
    // Check token not consumed
    if (token.consumed) {
        return "Token has already been consumed";
    }
    
    // Check not yet refundable
    if (current_height >= token.refund_height) {
        return "Token is past refund height and can only be refunded by creator";
    }
    
    // Verify secret matches hash_lock
    if (!tx.verify_secret(token.hash_lock)) {
        return "Secret does not match hash lock";
    }
    
    // Check redeemer nonce
    auto redeemer_account = account_manager.get_account(tx.redeemer);
    
    if (tx.nonce != redeemer_account.nonce) {
        return "Invalid nonce";
    }
    
    // Check redeemer balance for fee
    if (redeemer_account.balance < tx.fee) {
        return "Insufficient balance for fee";
    }
    
    return std::nullopt;
}

std::optional<std::string> PaperTokenValidator::validate_refund(
    const RefundPaperTokenTx& tx,
    const crypto::Ed25519_PublicKey& public_key,
    const AccountManager& account_manager,
    const PaperTokenManager& token_manager,
    uint64_t current_height
) const {
    // Check chain ID
    if (!has_valid_chain_id(tx.chain_id)) {
        return "Invalid chain ID";
    }
    
    // Check signature
    if (!tx.verify_signature(public_key)) {
        return "Invalid signature";
    }
    
    // Check zero addresses
    if (is_zero_address(tx.from)) {
        return "From address cannot be zero";
    }
    
    if (is_zero_address(tx.destination)) {
        return "Destination address cannot be zero";
    }
    
    // Check token exists
    auto token_opt = token_manager.get_token(tx.token_id);
    if (!token_opt) {
        return "Token does not exist";
    }
    
    const auto& token = *token_opt;
    
    // Check token not consumed
    if (token.consumed) {
        return "Token has already been consumed";
    }
    
    // Check refund height reached
    if (current_height < token.refund_height) {
        return "Refund height not yet reached";
    }
    
    // Check tx.from matches token creator
    if (tx.from != token.creator_address) {
        return "Only token creator can refund";
    }
    
    // Check creator nonce
    auto creator_account = account_manager.get_account(tx.from);
    
    if (tx.nonce != creator_account.nonce) {
        return "Invalid nonce";
    }
    
    // Check creator balance for fee
    if (creator_account.balance < tx.fee) {
        return "Insufficient balance for fee";
    }
    
    return std::nullopt;
}

bool PaperTokenValidator::is_zero_address(const Address& address) {
    return address.is_zero();
}

bool PaperTokenValidator::has_valid_chain_id(uint32_t tx_chain_id) const {
    return tx_chain_id == chain_id_;
}

} // namespace state
} // namespace sarafu
