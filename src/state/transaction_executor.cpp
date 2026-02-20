#include "sarafu/state/transaction_executor.h"
#include <algorithm>

namespace sarafu {
namespace state {

TransactionExecutor::TransactionExecutor(AccountManager& account_manager)
    : account_manager_(account_manager),
      total_burned_(0),
      total_priority_fees_(0) {}

TransactionExecutor::~TransactionExecutor() = default;

TransactionReceipt TransactionExecutor::execute_transaction(
    const Transaction& tx,
    uint64_t block_height,
    uint64_t base_fee,
    const Address& proposer_address
) {
    // Calculate fee components
    uint64_t base_fee_amount = calculate_base_fee_amount(tx, base_fee);
    uint64_t priority_fee_amount = calculate_priority_fee_amount(tx, base_fee_amount);

    // Total amount to deduct from sender
    uint64_t total_deduction = tx.amount + tx.fee;

    // Get sender account
    Account sender = account_manager_.get_account(tx.from);

    // Verify sender has sufficient balance (should already be validated, but double-check)
    if (sender.balance < total_deduction) {
        return TransactionReceipt(
            tx.hash(),
            block_height,
            false,
            0,
            "Insufficient balance"
        );
    }

    // Step 1: Deduct amount + fee from sender
    account_manager_.deduct_balance(tx.from, total_deduction);

    // Step 2: Increment sender's nonce
    account_manager_.increment_nonce(tx.from);

    // Step 3: Add amount to recipient
    account_manager_.add_balance(tx.to, tx.amount);

    // Step 4: Burn base fee (remove from circulation)
    // In a real implementation, this would update the total supply
    // For now, we just track the burned amount
    total_burned_ += base_fee_amount;

    // Step 5: Pay priority fee to block proposer
    if (priority_fee_amount > 0) {
        account_manager_.add_balance(proposer_address, priority_fee_amount);
        total_priority_fees_ += priority_fee_amount;
    }

    // Step 6: Return success receipt
    return TransactionReceipt(
        tx.hash(),
        block_height,
        true,
        tx.gas_limit,  // For now, assume full gas is used
        ""
    );
}

uint64_t TransactionExecutor::calculate_base_fee_amount(
    const Transaction& tx,
    uint64_t base_fee
) const {
    // Base fee amount = base_fee * gas_limit
    // For simplicity, we'll use a portion of the total fee
    // In a full implementation, this would be: base_fee * gas_used
    
    // For now, calculate as: min(tx.fee, base_fee * gas_limit)
    uint64_t base_fee_total = base_fee * tx.gas_limit;
    return std::min(tx.fee, base_fee_total);
}

uint64_t TransactionExecutor::calculate_priority_fee_amount(
    const Transaction& tx,
    uint64_t base_fee_amount
) const {
    // Priority fee = total fee - base fee
    if (tx.fee > base_fee_amount) {
        return tx.fee - base_fee_amount;
    }
    return 0;
}

void TransactionExecutor::reset_counters() {
    total_burned_ = 0;
    total_priority_fees_ = 0;
}

} // namespace state
} // namespace sarafu
