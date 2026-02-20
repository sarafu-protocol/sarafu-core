#include "sarafu/state/mempool.h"
#include "sarafu/state/account_manager.h"
#include <algorithm>
#include <ctime>

namespace sarafu {
namespace state {

Mempool::Mempool(const Config& config)
    : config_(config), account_manager_(nullptr)
{
}

void Mempool::set_account_manager(AccountManager* account_manager) {
    account_manager_ = account_manager;
}

bool Mempool::add_transaction(const Transaction& tx) {
    auto tx_hash = tx.hash();

    // Check if transaction already in mempool
    if (transactions_.find(tx_hash) != transactions_.end()) {
        return false;
    }

    // Validate with account manager if available
    if (account_manager_) {
        // Check balance sufficiency (amount + fee)
        uint64_t required = tx.amount + tx.fee;
        if (!account_manager_->has_sufficient_balance(tx.from, required)) {
            return false;
        }

        // Get current nonce
        uint64_t current_nonce = account_manager_->get_nonce(tx.from);

        // Check nonce is within acceptable range
        // Requirements 19.4, 19.8: nonce must be >= current_nonce and <= current_nonce + MAX_NONCE_GAP
        if (tx.nonce < current_nonce) {
            return false;  // Nonce too old
        }
        if (tx.nonce > current_nonce + config_.max_nonce_gap) {
            return false;  // Nonce gap too large
        }
    }

    // Get or create account queue
    auto& queue = account_queues_[tx.from];
    if (queue.account.is_zero()) {
        queue.account = tx.from;
        if (account_manager_) {
            queue.next_expected_nonce = account_manager_->get_nonce(tx.from);
        }
    }

    // Check queue size limit (Requirement 19.9)
    if (queue.transactions.size() >= config_.max_transactions_per_account) {
        return false;
    }

    // Create mempool transaction
    uint64_t timestamp = std::time(nullptr);
    uint64_t total_fee = tx.fee;
    MempoolTransaction mempool_tx(tx, timestamp, total_fee);

    // Insert into account queue in nonce order (Requirement 19.4)
    insert_in_nonce_order(queue, mempool_tx);

    // Add to transaction lookup
    transactions_[tx_hash] = mempool_tx;

    // Add to fee index (use negative fee for descending order)
    fee_index_.insert({total_fee, tx_hash});

    return true;
}

void Mempool::insert_in_nonce_order(AccountQueue& queue, const MempoolTransaction& mempool_tx) {
    // Find insertion point to maintain nonce ordering
    auto it = queue.transactions.begin();
    while (it != queue.transactions.end() && it->tx.nonce < mempool_tx.tx.nonce) {
        ++it;
    }
    queue.transactions.insert(it, mempool_tx);
}

std::vector<Transaction> Mempool::get_transactions_for_block(
    uint64_t max_gas,
    uint64_t min_base_fee
) {
    std::vector<Transaction> result;
    uint64_t gas_used = 0;

    // Track next expected nonce per account to maintain nonce ordering
    std::map<Address, uint64_t> account_next_nonce;
    if (account_manager_) {
        // Initialize with current nonces from account manager
        for (const auto& [addr, queue] : account_queues_) {
            account_next_nonce[addr] = account_manager_->get_nonce(addr);
        }
    }

    // Iterate fee index in descending order (Requirement 19.5)
    for (auto it = fee_index_.rbegin(); it != fee_index_.rend(); ++it) {
        auto tx_it = transactions_.find(it->second);
        if (tx_it == transactions_.end()) {
            continue;
        }

        const auto& mempool_tx = tx_it->second;
        const auto& tx = mempool_tx.tx;

        // Check fee requirement
        if (tx.fee < min_base_fee) {
            continue;
        }

        // Check gas limit
        if (gas_used + tx.gas_limit > max_gas) {
            continue;
        }

        // Check nonce ordering: only include if nonce matches expected
        // This ensures we maintain nonce order per account (Requirement 19.5)
        if (account_manager_) {
            auto nonce_it = account_next_nonce.find(tx.from);
            if (nonce_it != account_next_nonce.end()) {
                if (tx.nonce != nonce_it->second) {
                    continue;  // Skip - would break nonce ordering
                }
                // Update expected nonce for this account
                nonce_it->second++;
            }
        }

        // Add transaction
        result.push_back(tx);
        gas_used += tx.gas_limit;
    }

    return result;
}

void Mempool::remove_transactions(const std::vector<crypto::Blake3Hash>& tx_hashes) {
    for (const auto& tx_hash : tx_hashes) {
        auto it = transactions_.find(tx_hash);
        if (it == transactions_.end()) {
            continue;
        }

        const auto& mempool_tx = it->second;
        const auto& tx = mempool_tx.tx;

        // Remove from account queue
        auto queue_it = account_queues_.find(tx.from);
        if (queue_it != account_queues_.end()) {
            auto& queue = queue_it->second;
            queue.transactions.erase(
                std::remove_if(
                    queue.transactions.begin(),
                    queue.transactions.end(),
                    [&tx_hash](const MempoolTransaction& mt) {
                        return mt.tx.hash() == tx_hash;
                    }
                ),
                queue.transactions.end()
            );

            // Remove empty queue
            if (queue.transactions.empty()) {
                account_queues_.erase(queue_it);
            }
        }

        // Remove from fee index
        auto range = fee_index_.equal_range(mempool_tx.total_fee);
        for (auto fee_it = range.first; fee_it != range.second; ++fee_it) {
            if (fee_it->second == tx_hash) {
                fee_index_.erase(fee_it);
                break;
            }
        }

        // Remove from transaction lookup
        transactions_.erase(it);
    }
}

size_t Mempool::size() const {
    return transactions_.size();
}

std::vector<Transaction> Mempool::get_account_transactions(const Address& addr) const {
    std::vector<Transaction> result;

    auto it = account_queues_.find(addr);
    if (it != account_queues_.end()) {
        const auto& queue = it->second;
        for (const auto& mempool_tx : queue.transactions) {
            result.push_back(mempool_tx.tx);
        }
    }

    return result;
}

void Mempool::prune_old_transactions() {
    uint64_t current_time = std::time(nullptr);
    std::vector<crypto::Blake3Hash> to_remove;

    for (const auto& [tx_hash, mempool_tx] : transactions_) {
        if (current_time - mempool_tx.received_timestamp > config_.max_age_seconds) {
            to_remove.push_back(tx_hash);
        }
    }

    remove_transactions(to_remove);
}

void Mempool::clear() {
    account_queues_.clear();
    fee_index_.clear();
    transactions_.clear();
}

} // namespace state
} // namespace sarafu
