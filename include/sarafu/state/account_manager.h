#pragma once

#include <map>
#include <memory>
#include <optional>
#include "sarafu/state/account.h"
#include "sarafu/crypto/blake3_hash.h"

namespace sarafu {
namespace state {

/**
 * AccountManager manages account state and provides operations for
 * querying, creating, and updating accounts.
 * 
 * This class is responsible for:
 * - Retrieving account state
 * - Creating new accounts
 * - Updating account balances and nonces
 * - Validating nonce sequences
 * - Checking balance sufficiency
 * 
 * Requirements: 7.1, 7.2, 7.3, 7.6, 7.7, 7.8
 */
class AccountManager {
public:
    AccountManager();
    ~AccountManager();

    /**
     * Get an account by address.
     * 
     * If the account doesn't exist, returns a new account with zero balance
     * and nonce 0.
     * 
     * @param address The account address
     * @return The account state
     */
    Account get_account(const Address& address) const;

    /**
     * Create a new account with initial balance.
     * 
     * If the account already exists, this updates its balance.
     * 
     * @param address The account address
     * @param initial_balance The initial balance
     * @return The created account
     */
    Account create_account(const Address& address, uint64_t initial_balance);

    /**
     * Update an existing account's state.
     * 
     * This replaces the account state in storage.
     * 
     * @param account The updated account state
     */
    void update_account(const Account& account);

    /**
     * Validate that a transaction nonce matches the account's current nonce.
     * 
     * For replay protection, transactions must have a nonce equal to the
     * account's current nonce.
     * 
     * @param address The account address
     * @param transaction_nonce The transaction's nonce
     * @return true if nonce is valid (equals current nonce), false otherwise
     */
    bool validate_nonce(const Address& address, uint64_t transaction_nonce) const;

    /**
     * Increment an account's nonce by 1.
     * 
     * This is called after successfully executing a transaction.
     * 
     * @param address The account address
     */
    void increment_nonce(const Address& address);

    /**
     * Check if an account has sufficient balance for a transaction.
     * 
     * @param address The account address
     * @param required_amount The amount needed (typically amount + fee)
     * @return true if balance >= required_amount, false otherwise
     */
    bool has_sufficient_balance(const Address& address, uint64_t required_amount) const;

    /**
     * Deduct an amount from an account's balance.
     * 
     * This does not check if the balance is sufficient - caller must verify first.
     * 
     * @param address The account address
     * @param amount The amount to deduct
     */
    void deduct_balance(const Address& address, uint64_t amount);

    /**
     * Add an amount to an account's balance.
     * 
     * @param address The account address
     * @param amount The amount to add
     */
    void add_balance(const Address& address, uint64_t amount);

    /**
     * Get the current nonce for an account.
     * 
     * @param address The account address
     * @return The current nonce
     */
    uint64_t get_nonce(const Address& address) const;

    /**
     * Get the current balance for an account.
     * 
     * @param address The account address
     * @return The current balance
     */
    uint64_t get_balance(const Address& address) const;

    /**
     * Check if an account exists (has been created).
     * 
     * @param address The account address
     * @return true if the account exists, false otherwise
     */
    bool account_exists(const Address& address) const;

    /**
     * Get all accounts (for state root calculation).
     * 
     * @return Map of address to account
     */
    const std::map<Address, Account>& get_all_accounts() const;

    /**
     * Clear all accounts (for testing).
     */
    void clear();

private:
    // In-memory account storage
    // In production, this would be backed by a database
    std::map<Address, Account> accounts_;
};

} // namespace state
} // namespace sarafu
