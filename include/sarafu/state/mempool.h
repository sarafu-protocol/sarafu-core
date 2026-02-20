#pragma once

#include "sarafu/state/transaction.h"
#include "sarafu/crypto/blake3_hash.h"
#include <vector>
#include <map>
#include <deque>
#include <memory>

namespace sarafu {
namespace state {

// Forward declaration
class AccountManager;

/**
 * MempoolTransaction wraps a transaction with metadata.
 */
struct MempoolTransaction {
    Transaction tx;
    uint64_t received_timestamp;
    uint64_t total_fee;

    MempoolTransaction() : received_timestamp(0), total_fee(0) {}
    MempoolTransaction(const Transaction& transaction, uint64_t timestamp, uint64_t fee)
        : tx(transaction), received_timestamp(timestamp), total_fee(fee) {}
};

/**
 * AccountQueue maintains transactions for a single account in nonce order.
 */
struct AccountQueue {
    Address account;
    std::deque<MempoolTransaction> transactions;
    uint64_t next_expected_nonce;

    AccountQueue() : next_expected_nonce(0) {}
    explicit AccountQueue(const Address& addr) : account(addr), next_expected_nonce(0) {}
};

/**
 * Mempool manages pending transactions.
 * 
 * Responsibilities:
 * - Accept valid transactions
 * - Maintain nonce ordering per account
 * - Prioritize transactions by fee for block building
 * - Remove transactions after inclusion in blocks
 * 
 * Requirements: 19.1, 19.2, 19.3, 19.4, 19.5, 19.6, 19.7, 19.8, 19.9
 */
class Mempool {
public:
    /**
     * Configuration parameters for the mempool.
     */
    struct Config {
        size_t max_transactions_per_account;
        size_t max_nonce_gap;
        uint64_t max_age_seconds;

        Config()
            : max_transactions_per_account(100),
              max_nonce_gap(100),
              max_age_seconds(3600) {}
    };

    explicit Mempool(const Config& config = Config());

    /**
     * Set the account manager for balance and nonce validation.
     * 
     * @param account_manager Pointer to the account manager
     */
    void set_account_manager(AccountManager* account_manager);

    /**
     * Add a transaction to the mempool.
     * 
     * Validates:
     * - Transaction signature
     * - Sender has sufficient balance
     * - Nonce is within acceptable range (current_nonce to current_nonce + MAX_NONCE_GAP)
     * - Account queue is not full
     * - Transaction not already in mempool
     * 
     * @param tx The transaction to add
     * @return true if added successfully, false otherwise
     */
    bool add_transaction(const Transaction& tx);

    /**
     * Get transactions for block building, sorted by fee.
     * 
     * @param max_gas Maximum gas for the block
     * @param min_base_fee Minimum base fee required
     * @return Vector of transactions
     */
    std::vector<Transaction> get_transactions_for_block(
        uint64_t max_gas,
        uint64_t min_base_fee
    );

    /**
     * Remove transactions after block inclusion.
     * 
     * @param tx_hashes Vector of transaction hashes to remove
     */
    void remove_transactions(const std::vector<crypto::Blake3Hash>& tx_hashes);

    /**
     * Get the number of pending transactions.
     * 
     * @return Number of transactions in mempool
     */
    size_t size() const;

    /**
     * Get pending transactions for an account.
     * 
     * @param addr The account address
     * @return Vector of transactions
     */
    std::vector<Transaction> get_account_transactions(const Address& addr) const;

    /**
     * Prune old transactions (>1 hour).
     */
    void prune_old_transactions();

    /**
     * Clear all transactions (for testing).
     */
    void clear();

private:
    Config config_;

    // Account manager for validation (not owned)
    AccountManager* account_manager_;

    // Per-account transaction queues
    std::map<Address, AccountQueue> account_queues_;

    // Fee-sorted index for block building (fee -> tx_hash)
    std::multimap<uint64_t, crypto::Blake3Hash> fee_index_;

    // Transaction lookup (tx_hash -> MempoolTransaction)
    std::map<crypto::Blake3Hash, MempoolTransaction> transactions_;

    /**
     * Insert transaction into account queue in nonce order.
     * 
     * @param queue The account queue
     * @param mempool_tx The transaction to insert
     */
    void insert_in_nonce_order(AccountQueue& queue, const MempoolTransaction& mempool_tx);
};

} // namespace state
} // namespace sarafu
