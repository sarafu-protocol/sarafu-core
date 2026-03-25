#pragma once

#include "sarafu/state/account_manager.h"
#include "sarafu/state/transaction_validator.h"
#include "sarafu/state/transaction_executor.h"
#include "sarafu/state/transaction.h"
#include "sarafu/crypto/merkle_tree.h"
#include "sarafu/crypto/blake3_hash.h"
#include <vector>
#include <map>
#include <optional>

namespace sarafu {

// Forward declaration
namespace consensus {
    struct Block;
    struct GenesisAllocation;
}

namespace state {

/**
 * StateMachine coordinates transaction processing and state management.
 * 
 * This class integrates:
 * - AccountManager for account state
 * - TransactionValidator for transaction validation
 * - TransactionExecutor for transaction execution
 * - MerkleTree for state root computation
 * 
 * Responsibilities:
 * - Execute blocks of transactions
 * - Compute state roots using Merkle trees
 * - Support chain reorganization via state reversion
 * - Maintain transaction receipts
 * 
 * Requirements: 7.1, 17.6
 */
class StateMachine {
public:
    /**
     * Construct a StateMachine with a chain ID.
     * 
     * @param chain_id The network chain ID for replay protection
     */
    explicit StateMachine(uint32_t chain_id);

    ~StateMachine();

    /**
     * Execute a block of transactions.
     * 
     * This processes all transactions in the block sequentially:
     * 1. Validate each transaction
     * 2. Execute valid transactions
     * 3. Generate receipts
     * 4. Compute new state root
     * 
     * @param block The block to execute
     * @param base_fee The current base fee per gas
     * @param proposer_address The block proposer's address
     * @return State root after executing all transactions
     */
    crypto::Blake3Hash execute_block(
        const consensus::Block& block,
        uint64_t base_fee,
        const Address& proposer_address
    );

    /**
     * Execute a single transaction.
     * 
     * This validates and executes a transaction, returning a receipt.
     * 
     * @param tx The transaction to execute
     * @param public_key The sender's public key for signature verification
     * @param block_height The current block height
     * @param base_fee The current base fee per gas
     * @param proposer_address The block proposer's address
     * @return TransactionReceipt with execution result
     */
    TransactionReceipt execute_transaction(
        const Transaction& tx,
        const crypto::Ed25519_PublicKey& public_key,
        uint64_t block_height,
        uint64_t base_fee,
        const Address& proposer_address
    );

    /**
     * Initialize the state machine from a genesis block.
     * 
     * This method:
     * - Loads genesis allocations and creates accounts
     * - Initializes the validator set for epoch 0
     * - Sets the initial epoch to 0
     * - Computes and verifies the genesis state root
     * 
     * Requirements: 16.1, 16.6, 16.7
     * 
     * @param genesis_block The genesis block
     * @param allocations The genesis token allocations
     * @return true if initialization succeeded, false otherwise
     */
    bool initialize_from_genesis(
        const consensus::Block& genesis_block,
        const std::vector<consensus::GenesisAllocation>& allocations
    );

    /**
     * Compute the state root using a Merkle tree of all accounts.
     * 
     * This creates a Merkle tree from all account hashes and returns the root.
     * The state root is deterministic and uniquely identifies the current state.
     * 
     * @return Blake3 hash of the Merkle root
     */
    crypto::Blake3Hash compute_state_root() const;

    /**
     * Get the chain ID for this state machine.
     */
    uint32_t chain_id() const { return chain_id_; }

    /**
     * Revert state to a previous block height.
     * 
     * This is used for chain reorganization when a different fork becomes canonical.
     * Reverts all state changes made after the specified height.
     * 
     * Note: This is a simplified implementation that clears all state.
     * A production implementation would maintain state snapshots.
     * 
     * @param height The block height to revert to
     * @return true if reversion succeeded, false otherwise
     */
    bool revert_to_height(uint64_t height);

    /**
     * Get an account by address.
     * 
     * @param address The account address
     * @return The account state
     */
    Account get_account(const Address& address) const;

    /**
     * Get a transaction receipt by transaction hash.
     * 
     * @param tx_hash The transaction hash
     * @return The receipt if found, std::nullopt otherwise
     */
    std::optional<TransactionReceipt> get_receipt(const crypto::Blake3Hash& tx_hash) const;

    /**
     * Get the account manager (for testing).
     * 
     * @return Reference to the account manager
     */
    AccountManager& get_account_manager() { return account_manager_; }

    /**
     * Get the transaction executor (for testing).
     * 
     * @return Reference to the transaction executor
     */
    TransactionExecutor& get_transaction_executor() { return transaction_executor_; }

    /**
     * Get the current block height.
     * 
     * @return The current block height
     */
    uint64_t get_current_height() const { return current_height_; }

    /**
     * Clear all state (for testing).
     */
    void clear();

private:
    /**
     * Store a transaction receipt.
     * 
     * @param receipt The receipt to store
     */
    void store_receipt(const TransactionReceipt& receipt);

    /**
     * Save a state snapshot at a given height (for reversion).
     * 
     * Note: This is a placeholder for a full implementation.
     * 
     * @param height The block height
     */
    void save_state_snapshot(uint64_t height);

    uint32_t chain_id_;
    AccountManager account_manager_;
    TransactionValidator transaction_validator_;
    TransactionExecutor transaction_executor_;
    
    // Transaction receipts indexed by transaction hash
    std::map<crypto::Blake3Hash, TransactionReceipt> receipts_;
    
    // Current block height
    uint64_t current_height_;
    
    // State snapshots for reversion (height -> state root)
    // In production, this would store full state snapshots
    std::map<uint64_t, crypto::Blake3Hash> state_snapshots_;
};

} // namespace state
} // namespace sarafu
