#include "sarafu/state/state_machine.h"
#include "sarafu/consensus/block.h"
#include "sarafu/consensus/genesis_builder.h"
#include <algorithm>

namespace sarafu {
namespace state {

StateMachine::StateMachine(uint32_t chain_id)
    : chain_id_(chain_id),
      account_manager_(),
      transaction_validator_(chain_id),
      transaction_executor_(account_manager_),
      receipts_(),
      current_height_(0),
      state_snapshots_() {}

StateMachine::~StateMachine() = default;

bool StateMachine::initialize_from_genesis(
    const consensus::Block& genesis_block,
    const std::vector<consensus::GenesisAllocation>& allocations
) {
    // Verify this is a genesis block (height 0, epoch 0)
    if (genesis_block.header.height != 0 || genesis_block.header.epoch != 0) {
        return false;
    }
    
    // Clear any existing state
    clear();
    
    // Create accounts for all genesis allocations
    for (const auto& allocation : allocations) {
        // Create account with initial balance and nonce 0
        account_manager_.create_account(allocation.address, allocation.amount);
        
        // Note: Vesting schedules would be stored separately in a production implementation
        // For now, we just create the accounts with their full allocation
    }
    
    // Compute state root and verify it matches the genesis block
    crypto::Blake3Hash computed_state_root = compute_state_root();
    if (computed_state_root != genesis_block.header.state_root) {
        // State root mismatch - genesis block is invalid
        clear();
        return false;
    }
    
    // Set initial height to 0
    current_height_ = 0;
    
    // Save genesis state snapshot
    save_state_snapshot(0);
    
    return true;
}

crypto::Blake3Hash StateMachine::execute_block(
    const consensus::Block& block,
    uint64_t base_fee,
    const Address& proposer_address
) {
    // Save state snapshot before executing block
    save_state_snapshot(current_height_);

    // Process each transaction in the block
    for (const auto& tx : block.transactions) {
        // Derive public key from sender address
        // In a real implementation, we would need to look up the public key
        // For now, we'll create a dummy public key
        // This is a simplification - in production, the public key would be
        // extracted from the transaction signature or stored separately
        crypto::Ed25519_PublicKey public_key;
        
        // Execute the transaction
        auto receipt = execute_transaction(
            tx,
            public_key,
            block.header.height,
            base_fee,
            proposer_address
        );
        
        // Store the receipt
        store_receipt(receipt);
    }

    // Update current height
    current_height_ = block.header.height;

    // Compute and return the new state root
    return compute_state_root();
}

TransactionReceipt StateMachine::execute_transaction(
    const Transaction& tx,
    const crypto::Ed25519_PublicKey& public_key,
    uint64_t block_height,
    uint64_t base_fee,
    const Address& proposer_address
) {
    // Validate the transaction
    auto validation_error = transaction_validator_.validate_transaction(
        tx,
        public_key,
        account_manager_
    );

    // If validation failed, return error receipt
    if (validation_error.has_value()) {
        return TransactionReceipt(
            tx.hash(),
            block_height,
            false,
            0,
            validation_error.value()
        );
    }

    // Execute the transaction
    return transaction_executor_.execute_transaction(
        tx,
        block_height,
        base_fee,
        proposer_address
    );
}

crypto::Blake3Hash StateMachine::compute_state_root() const {
    // Get all accounts
    const auto& accounts = account_manager_.get_all_accounts();

    // If no accounts, return zero hash
    if (accounts.empty()) {
        return crypto::Blake3Hash::zero();
    }

    // Create a vector of account hashes
    std::vector<crypto::Blake3Hash> account_hashes;
    account_hashes.reserve(accounts.size());

    for (const auto& [address, account] : accounts) {
        account_hashes.push_back(account.hash());
    }

    // Sort account hashes for deterministic ordering
    std::sort(account_hashes.begin(), account_hashes.end());

    // Build Merkle tree from account hashes
    crypto::MerkleTree merkle_tree;
    merkle_tree.build_tree(account_hashes);

    // Return the Merkle root
    return merkle_tree.get_root();
}

bool StateMachine::revert_to_height(uint64_t height) {
    // Check if we have a snapshot at this height
    auto it = state_snapshots_.find(height);
    if (it == state_snapshots_.end()) {
        return false;
    }

    // In a full implementation, we would restore the state from the snapshot
    // For now, we just clear all state and update the height
    // This is a simplified implementation
    
    // Clear all accounts (in production, restore from snapshot)
    account_manager_.clear();
    
    // Clear receipts after this height
    auto receipt_it = receipts_.begin();
    while (receipt_it != receipts_.end()) {
        if (receipt_it->second.block_height > height) {
            receipt_it = receipts_.erase(receipt_it);
        } else {
            ++receipt_it;
        }
    }

    // Update current height
    current_height_ = height;

    // Remove snapshots after this height
    auto snapshot_it = state_snapshots_.begin();
    while (snapshot_it != state_snapshots_.end()) {
        if (snapshot_it->first > height) {
            snapshot_it = state_snapshots_.erase(snapshot_it);
        } else {
            ++snapshot_it;
        }
    }

    return true;
}

Account StateMachine::get_account(const Address& address) const {
    return account_manager_.get_account(address);
}

std::optional<TransactionReceipt> StateMachine::get_receipt(
    const crypto::Blake3Hash& tx_hash
) const {
    auto it = receipts_.find(tx_hash);
    if (it != receipts_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void StateMachine::clear() {
    account_manager_.clear();
    receipts_.clear();
    current_height_ = 0;
    state_snapshots_.clear();
    transaction_executor_.reset_counters();
}

void StateMachine::store_receipt(const TransactionReceipt& receipt) {
    receipts_[receipt.tx_hash] = receipt;
}

void StateMachine::save_state_snapshot(uint64_t height) {
    // In a full implementation, this would save a complete state snapshot
    // For now, we just save the state root
    state_snapshots_[height] = compute_state_root();
}

} // namespace state
} // namespace sarafu
