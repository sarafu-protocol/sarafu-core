#include "sarafu/state/paper_token_executor.h"
#include <stdexcept>

namespace sarafu {
namespace state {

PaperTokenExecutor::PaperTokenExecutor(
    AccountManager& account_manager,
    PaperTokenManager& token_manager
) : account_manager_(account_manager),
    token_manager_(token_manager),
    total_burned_(0),
    total_priority_fees_(0) {}

PaperTokenExecutor::~PaperTokenExecutor() {}

TransactionReceipt PaperTokenExecutor::execute_create(
    const CreatePaperTokenTx& tx,
    uint64_t block_height,
    uint64_t base_fee,
    const Address& proposer_address
) {
    try {
        // 1. Deduct amount + fee from creator's balance
        auto creator_account = account_manager_.get_account(tx.from);
        
        uint64_t required = tx.amount + tx.fee;
        
        if (creator_account.balance < required) {
            return TransactionReceipt(
                tx.hash(),
                block_height,
                false,
                0,
                "Insufficient balance"
            );
        }
        
        creator_account.balance -= required;
        
        // 2. Increment creator's nonce
        creator_account.nonce++;
        
        account_manager_.update_account(creator_account);
        
        // 3. Calculate refund_height
        uint64_t refund_height = block_height + (tx.refund_delay_days * BLOCKS_PER_DAY);
        
        // 4. Compute token_id
        crypto::Blake3Hash token_id = PaperToken::compute_token_id(
            tx.hash_lock,
            tx.from,
            block_height
        );
        
        // 5. Create PaperToken
        PaperToken token(
            token_id,
            tx.amount,
            tx.hash_lock,
            tx.from,
            block_height,
            refund_height
        );
        
        if (!token_manager_.create_token(token)) {
            return TransactionReceipt(
                tx.hash(),
                block_height,
                false,
                0,
                "Token ID already exists"
            );
        }
        
        // 6. Burn base fee portion
        uint64_t base_fee_amount = calculate_base_fee_amount(tx.fee, tx.gas_limit, base_fee);
        total_burned_ += base_fee_amount;
        
        // 7. Pay priority fee to block proposer
        uint64_t priority_fee_amount = calculate_priority_fee_amount(tx.fee, base_fee_amount);
        
        if (priority_fee_amount > 0) {
            auto proposer_account = account_manager_.get_account(proposer_address);
            proposer_account.balance += priority_fee_amount;
            account_manager_.update_account(proposer_account);
            total_priority_fees_ += priority_fee_amount;
        }
        
        // 8. Return success receipt
        return TransactionReceipt(
            tx.hash(),
            block_height,
            true,
            tx.gas_limit,
            ""
        );
        
    } catch (const std::exception& e) {
        return TransactionReceipt(
            tx.hash(),
            block_height,
            false,
            0,
            std::string("Execution error: ") + e.what()
        );
    }
}

TransactionReceipt PaperTokenExecutor::execute_redeem(
    const RedeemPaperTokenTx& tx,
    uint64_t block_height,
    uint64_t base_fee,
    const Address& proposer_address
) {
    try {
        // 1. Get token
        auto token_opt = token_manager_.get_token(tx.token_id);
        if (!token_opt) {
            return TransactionReceipt(
                tx.hash(),
                block_height,
                false,
                0,
                "Token does not exist"
            );
        }
        
        const auto& token = *token_opt;
        
        if (token.consumed) {
            return TransactionReceipt(
                tx.hash(),
                block_height,
                false,
                0,
                "Token already consumed"
            );
        }
        
        // 2. Deduct fee from redeemer's balance
        auto redeemer_account = account_manager_.get_account(tx.redeemer);
        
        if (redeemer_account.balance < tx.fee) {
            return TransactionReceipt(
                tx.hash(),
                block_height,
                false,
                0,
                "Insufficient balance for fee"
            );
        }
        
        redeemer_account.balance -= tx.fee;
        
        // 3. Increment redeemer's nonce
        redeemer_account.nonce++;
        
        account_manager_.update_account(redeemer_account);
        
        // 4. Transfer token.amount to destination
        auto dest_account = account_manager_.get_account(tx.destination);
        dest_account.balance += token.amount;
        account_manager_.update_account(dest_account);
        
        // 5. Mark token as consumed
        if (!token_manager_.consume_token(tx.token_id)) {
            return TransactionReceipt(
                tx.hash(),
                block_height,
                false,
                0,
                "Failed to consume token"
            );
        }
        
        // 6. Burn base fee portion
        uint64_t base_fee_amount = calculate_base_fee_amount(tx.fee, tx.gas_limit, base_fee);
        total_burned_ += base_fee_amount;
        
        // 7. Pay priority fee to block proposer
        uint64_t priority_fee_amount = calculate_priority_fee_amount(tx.fee, base_fee_amount);
        
        if (priority_fee_amount > 0) {
            auto proposer_account = account_manager_.get_account(proposer_address);
            proposer_account.balance += priority_fee_amount;
            account_manager_.update_account(proposer_account);
            total_priority_fees_ += priority_fee_amount;
        }
        
        // 8. Return success receipt
        return TransactionReceipt(
            tx.hash(),
            block_height,
            true,
            tx.gas_limit,
            ""
        );
        
    } catch (const std::exception& e) {
        return TransactionReceipt(
            tx.hash(),
            block_height,
            false,
            0,
            std::string("Execution error: ") + e.what()
        );
    }
}

TransactionReceipt PaperTokenExecutor::execute_refund(
    const RefundPaperTokenTx& tx,
    uint64_t block_height,
    uint64_t base_fee,
    const Address& proposer_address
) {
    try {
        // 1. Get token
        auto token_opt = token_manager_.get_token(tx.token_id);
        if (!token_opt) {
            return TransactionReceipt(
                tx.hash(),
                block_height,
                false,
                0,
                "Token does not exist"
            );
        }
        
        const auto& token = *token_opt;
        
        if (token.consumed) {
            return TransactionReceipt(
                tx.hash(),
                block_height,
                false,
                0,
                "Token already consumed"
            );
        }
        
        // 2. Deduct fee from creator's balance
        auto creator_account = account_manager_.get_account(tx.from);
        
        if (creator_account.balance < tx.fee) {
            return TransactionReceipt(
                tx.hash(),
                block_height,
                false,
                0,
                "Insufficient balance for fee"
            );
        }
        
        creator_account.balance -= tx.fee;
        
        // 3. Increment creator's nonce
        creator_account.nonce++;
        
        account_manager_.update_account(creator_account);
        
        // 4. Transfer token.amount to destination
        auto dest_account = account_manager_.get_account(tx.destination);
        dest_account.balance += token.amount;
        account_manager_.update_account(dest_account);
        
        // 5. Mark token as consumed
        if (!token_manager_.consume_token(tx.token_id)) {
            return TransactionReceipt(
                tx.hash(),
                block_height,
                false,
                0,
                "Failed to consume token"
            );
        }
        
        // 6. Burn base fee portion
        uint64_t base_fee_amount = calculate_base_fee_amount(tx.fee, tx.gas_limit, base_fee);
        total_burned_ += base_fee_amount;
        
        // 7. Pay priority fee to block proposer
        uint64_t priority_fee_amount = calculate_priority_fee_amount(tx.fee, base_fee_amount);
        
        if (priority_fee_amount > 0) {
            auto proposer_account = account_manager_.get_account(proposer_address);
            proposer_account.balance += priority_fee_amount;
            account_manager_.update_account(proposer_account);
            total_priority_fees_ += priority_fee_amount;
        }
        
        // 8. Return success receipt
        return TransactionReceipt(
            tx.hash(),
            block_height,
            true,
            tx.gas_limit,
            ""
        );
        
    } catch (const std::exception& e) {
        return TransactionReceipt(
            tx.hash(),
            block_height,
            false,
            0,
            std::string("Execution error: ") + e.what()
        );
    }
}

void PaperTokenExecutor::reset_counters() {
    total_burned_ = 0;
    total_priority_fees_ = 0;
}

uint64_t PaperTokenExecutor::calculate_base_fee_amount(
    uint64_t fee,
    uint64_t gas_limit,
    uint64_t base_fee
) const {
    uint64_t base_fee_amount = base_fee * gas_limit;
    return std::min(base_fee_amount, fee);
}

uint64_t PaperTokenExecutor::calculate_priority_fee_amount(
    uint64_t fee,
    uint64_t base_fee_amount
) const {
    if (fee > base_fee_amount) {
        return fee - base_fee_amount;
    }
    return 0;
}

} // namespace state
} // namespace sarafu
