#pragma once

#include "sarafu/state/transaction.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/crypto/blake3_hash.h"
#include <optional>
#include <string>

namespace sarafu {
namespace state {

/**
 * TransactionExecutor executes transactions and applies state changes.
 * 
 * This class is responsible for:
 * - Executing transactions and updating account state
 * - Deducting fees (base fee + priority fee)
 * - Transferring balances between accounts
 * - Burning base fees
 * - Paying priority fees to block proposer
 * - Generating transaction receipts
 * 
 * Requirements: 7.6, 7.7, 12.6, 12.7
 */
class TransactionExecutor {
public:
    /**
     * Construct a TransactionExecutor with an AccountManager.
     * 
     * @param account_manager The account manager for state updates
     */
    explicit TransactionExecutor(AccountManager& account_manager);

    ~TransactionExecutor();

    /**
     * Execute a transaction and apply state changes.
     * 
     * This performs the following steps:
     * 1. Deduct amount + fee from sender's balance
     * 2. Increment sender's nonce
     * 3. Add amount to recipient's balance
     * 4. Burn base fee portion
     * 5. Pay priority fee to block proposer
     * 6. Return receipt with success/failure status
     * 
     * Note: This assumes the transaction has already been validated.
     * The caller must ensure the transaction is valid before execution.
     * 
     * @param tx The transaction to execute
     * @param block_height The height of the block containing the transaction
     * @param base_fee The current base fee per gas
     * @param proposer_address The address of the block proposer (receives priority fee)
     * @return TransactionReceipt with execution result
     */
    TransactionReceipt execute_transaction(
        const Transaction& tx,
        uint64_t block_height,
        uint64_t base_fee,
        const Address& proposer_address
    );

    /**
     * Get the total amount burned from base fees.
     * 
     * @return Total base fees burned
     */
    uint64_t get_total_burned() const { return total_burned_; }

    /**
     * Get the total priority fees paid to proposers.
     * 
     * @return Total priority fees paid
     */
    uint64_t get_total_priority_fees() const { return total_priority_fees_; }

    /**
     * Reset the burned and priority fee counters (for testing).
     */
    void reset_counters();

private:
    /**
     * Calculate the base fee portion of the transaction fee.
     * 
     * @param tx The transaction
     * @param base_fee The current base fee per gas
     * @return The base fee amount to burn
     */
    uint64_t calculate_base_fee_amount(const Transaction& tx, uint64_t base_fee) const;

    /**
     * Calculate the priority fee portion of the transaction fee.
     * 
     * @param tx The transaction
     * @param base_fee_amount The base fee amount
     * @return The priority fee amount to pay to proposer
     */
    uint64_t calculate_priority_fee_amount(const Transaction& tx, uint64_t base_fee_amount) const;

    AccountManager& account_manager_;
    uint64_t total_burned_;
    uint64_t total_priority_fees_;
};

} // namespace state
} // namespace sarafu
