#pragma once

#include "sarafu/state/paper_token.h"
#include "sarafu/state/paper_token_manager.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/state/transaction.h"

namespace sarafu {
namespace state {

/**
 * PaperTokenExecutor executes paper token transactions and applies state changes.
 * 
 * This class is responsible for:
 * - Creating paper tokens and locking funds
 * - Redeeming paper tokens and transferring funds
 * - Refunding paper tokens to creators
 * - Deducting fees and burning base fees
 * - Generating transaction receipts
 * 
 * Note: All transactions must be validated before execution.
 * The executor assumes transactions are valid and does not perform validation.
 */
class PaperTokenExecutor {
public:
    /**
     * Construct a PaperTokenExecutor.
     * 
     * @param account_manager The account manager for balance updates
     * @param token_manager The paper token manager for token state
     */
    PaperTokenExecutor(
        AccountManager& account_manager,
        PaperTokenManager& token_manager
    );
    
    ~PaperTokenExecutor();
    
    /**
     * Execute a CreatePaperTokenTx.
     * 
     * Steps:
     * 1. Deduct amount + fee from creator's balance
     * 2. Increment creator's nonce
     * 3. Calculate refund_height = current_height + (refund_delay_days * blocks_per_day)
     * 4. Compute token_id = Blake3(hash_lock || creator || creation_height)
     * 5. Create PaperToken in token manager
     * 6. Burn base fee portion
     * 7. Pay priority fee to block proposer
     * 8. Return receipt
     * 
     * @param tx The transaction to execute
     * @param block_height The height of the block containing the transaction
     * @param base_fee The current base fee per gas
     * @param proposer_address The address of the block proposer
     * @return TransactionReceipt with execution result
     */
    TransactionReceipt execute_create(
        const CreatePaperTokenTx& tx,
        uint64_t block_height,
        uint64_t base_fee,
        const Address& proposer_address
    );
    
    /**
     * Execute a RedeemPaperTokenTx.
     * 
     * Steps:
     * 1. Get token from token manager
     * 2. Deduct fee from redeemer's balance
     * 3. Increment redeemer's nonce
     * 4. Transfer token.amount to destination
     * 5. Mark token as consumed
     * 6. Burn base fee portion
     * 7. Pay priority fee to block proposer
     * 8. Return receipt
     * 
     * @param tx The transaction to execute
     * @param block_height The height of the block containing the transaction
     * @param base_fee The current base fee per gas
     * @param proposer_address The address of the block proposer
     * @return TransactionReceipt with execution result
     */
    TransactionReceipt execute_redeem(
        const RedeemPaperTokenTx& tx,
        uint64_t block_height,
        uint64_t base_fee,
        const Address& proposer_address
    );
    
    /**
     * Execute a RefundPaperTokenTx.
     * 
     * Steps:
     * 1. Get token from token manager
     * 2. Deduct fee from creator's balance
     * 3. Increment creator's nonce
     * 4. Transfer token.amount to destination
     * 5. Mark token as consumed
     * 6. Burn base fee portion
     * 7. Pay priority fee to block proposer
     * 8. Return receipt
     * 
     * @param tx The transaction to execute
     * @param block_height The height of the block containing the transaction
     * @param base_fee The current base fee per gas
     * @param proposer_address The address of the block proposer
     * @return TransactionReceipt with execution result
     */
    TransactionReceipt execute_refund(
        const RefundPaperTokenTx& tx,
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
    AccountManager& account_manager_;
    PaperTokenManager& token_manager_;
    uint64_t total_burned_;
    uint64_t total_priority_fees_;
    
    // Blocks per day (assuming 2-second block time: 43,200 blocks/day)
    static constexpr uint64_t BLOCKS_PER_DAY = 43200;
    
    /**
     * Calculate the base fee portion of the transaction fee.
     */
    uint64_t calculate_base_fee_amount(uint64_t fee, uint64_t gas_limit, uint64_t base_fee) const;
    
    /**
     * Calculate the priority fee portion of the transaction fee.
     */
    uint64_t calculate_priority_fee_amount(uint64_t fee, uint64_t base_fee_amount) const;
};

} // namespace state
} // namespace sarafu
