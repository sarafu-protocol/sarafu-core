#include "sarafu/consensus/block_validator.h"
#include "sarafu/crypto/merkle_tree.h"
#include "sarafu/state/transaction_validator.h"
#include <sstream>

namespace sarafu {
namespace consensus {

BlockValidator::BlockValidator(
    std::shared_ptr<ValidatorRegistry> validator_registry,
    std::shared_ptr<state::StateMachine> state_machine
)
    : validator_registry_(validator_registry),
      state_machine_(state_machine) {}

std::string BlockValidator::validate_block(
    const Block& block,
    const Block& parent_block,
    bool verify_state_root
) {
    // Check block height increment
    std::string error = verify_block_height(block, parent_block);
    if (!error.empty()) {
        return error;
    }

    // Check block hash chain
    error = verify_block_hash_chain(block, parent_block);
    if (!error.empty()) {
        return error;
    }

    // Check timestamp monotonicity
    error = verify_timestamp(block, parent_block);
    if (!error.empty()) {
        return error;
    }

    // Check proposer membership
    error = verify_proposer(block);
    if (!error.empty()) {
        return error;
    }

    // Check transactions root
    error = verify_transactions_root(block);
    if (!error.empty()) {
        return error;
    }

    // Check all transactions are valid
    error = verify_transactions(block);
    if (!error.empty()) {
        return error;
    }

    // Optionally verify state root (expensive)
    if (verify_state_root) {
        error = this->verify_state_root(block);
        if (!error.empty()) {
            return error;
        }
    }

    return "";  // Valid
}

std::string BlockValidator::verify_block_height(
    const Block& block,
    const Block& parent_block
) const {
    if (block.header.height != parent_block.header.height + 1) {
        std::ostringstream oss;
        oss << "Invalid block height: expected " << (parent_block.header.height + 1)
            << ", got " << block.header.height;
        return oss.str();
    }
    return "";
}

std::string BlockValidator::verify_block_hash_chain(
    const Block& block,
    const Block& parent_block
) const {
    crypto::Blake3Hash parent_hash = parent_block.hash();
    if (block.header.previous_hash != parent_hash) {
        std::ostringstream oss;
        oss << "Invalid previous_hash: expected " << parent_hash.to_hex()
            << ", got " << block.header.previous_hash.to_hex();
        return oss.str();
    }
    return "";
}

std::string BlockValidator::verify_timestamp(
    const Block& block,
    const Block& parent_block
) const {
    if (block.header.timestamp <= parent_block.header.timestamp) {
        std::ostringstream oss;
        oss << "Invalid timestamp: block timestamp " << block.header.timestamp
            << " must be greater than parent timestamp " << parent_block.header.timestamp;
        return oss.str();
    }
    return "";
}

std::string BlockValidator::verify_transactions(const Block& block) {
    state::TransactionValidator tx_validator(1);  // chain_id = 1

    for (size_t i = 0; i < block.transactions.size(); ++i) {
        const auto& tx = block.transactions[i];
        
        // For block validation, we need the public key to verify signature
        // In a full implementation, we would extract it from the transaction
        // For now, we'll do basic structural validation
        
        // Check zero addresses
        if (state::TransactionValidator::is_zero_address(tx.from) ||
            state::TransactionValidator::is_zero_address(tx.to)) {
            std::ostringstream oss;
            oss << "Transaction " << i << " invalid: zero address";
            return oss.str();
        }
        
        // Check non-negative amounts (always true for uint64_t, but check anyway)
        if (!state::TransactionValidator::has_non_negative_amounts(tx)) {
            std::ostringstream oss;
            oss << "Transaction " << i << " invalid: negative amount";
            return oss.str();
        }
        
        // Check chain ID
        if (!state::TransactionValidator::has_valid_chain_id(tx, 1)) {
            std::ostringstream oss;
            oss << "Transaction " << i << " invalid: wrong chain ID";
            return oss.str();
        }
    }

    return "";
}

std::string BlockValidator::verify_proposer(const Block& block) const {
    // Get the current validator set
    const ValidatorSet& validator_set = validator_registry_->current_set();

    // Check if proposer is in the active set
    bool found = false;
    for (const auto& validator : validator_set.validators) {
        if (validator.id == block.header.proposer) {
            // Check that validator is active
            if (validator.status != ValidatorStatus::Active) {
                std::ostringstream oss;
                oss << "Proposer " << block.header.proposer.to_hex()
                    << " is not active (status: " << static_cast<int>(validator.status) << ")";
                return oss.str();
            }
            found = true;
            break;
        }
    }

    if (!found) {
        std::ostringstream oss;
        oss << "Proposer " << block.header.proposer.to_hex()
            << " is not in the active validator set";
        return oss.str();
    }

    return "";
}

std::string BlockValidator::verify_state_root(const Block& block) {
    // This is expensive: we need to execute all transactions and compute the state root
    // In practice, this would be done on a copy of the state
    
    // For now, we'll just compute the state root and compare
    // A full implementation would execute transactions on a temporary state
    crypto::Blake3Hash computed_root = state_machine_->compute_state_root();
    
    if (block.header.state_root != computed_root) {
        std::ostringstream oss;
        oss << "State root mismatch: expected " << computed_root.to_hex()
            << ", got " << block.header.state_root.to_hex();
        return oss.str();
    }

    return "";
}

std::string BlockValidator::verify_transactions_root(const Block& block) const {
    // Compute Merkle root of all transaction hashes
    std::vector<crypto::Blake3Hash> tx_hashes;
    tx_hashes.reserve(block.transactions.size());
    
    for (const auto& tx : block.transactions) {
        tx_hashes.push_back(tx.hash());
    }

    // Handle empty transaction list
    if (tx_hashes.empty()) {
        // Empty Merkle tree has a zero hash root
        crypto::Blake3Hash empty_root = crypto::Blake3Hash::zero();
        if (block.header.transactions_root != empty_root) {
            std::ostringstream oss;
            oss << "Transactions root mismatch for empty block: expected "
                << empty_root.to_hex() << ", got "
                << block.header.transactions_root.to_hex();
            return oss.str();
        }
        return "";
    }

    // Build Merkle tree
    crypto::MerkleTree merkle_tree;
    merkle_tree.build_tree(tx_hashes);
    crypto::Blake3Hash computed_root = merkle_tree.get_root();

    if (block.header.transactions_root != computed_root) {
        std::ostringstream oss;
        oss << "Transactions root mismatch: expected " << computed_root.to_hex()
            << ", got " << block.header.transactions_root.to_hex();
        return oss.str();
    }

    return "";
}

} // namespace consensus
} // namespace sarafu
