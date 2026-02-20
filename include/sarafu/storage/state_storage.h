#pragma once

#include <memory>
#include <optional>
#include "sarafu/storage/database.h"
#include "sarafu/state/account.h"
#include "sarafu/state/transaction.h"
#include "sarafu/consensus/block.h"
#include "sarafu/crypto/blake3_hash.h"

namespace sarafu {
namespace storage {

/**
 * StateStorage provides high-level storage operations for blockchain state.
 * 
 * This class wraps the Database class and provides type-safe methods for
 * storing and retrieving blockchain data:
 * - Account states
 * - Blocks (by height and hash)
 * - Transaction receipts
 * - Index management (height↔hash mappings)
 * 
 * All operations use the appropriate column families and handle serialization.
 */
class StateStorage {
public:
    /**
     * Create a StateStorage instance using the provided database.
     * 
     * @param db The database to use for storage
     */
    explicit StateStorage(std::shared_ptr<Database> db);

    ~StateStorage();

    // Disable copy and move
    StateStorage(const StateStorage&) = delete;
    StateStorage& operator=(const StateStorage&) = delete;
    StateStorage(StateStorage&&) = delete;
    StateStorage& operator=(StateStorage&&) = delete;

    // ========== Account Operations ==========

    /**
     * Store an account state.
     * 
     * @param account The account to store
     * @return Result indicating success or error
     */
    Result<void> store_account(const state::Account& account);

    /**
     * Retrieve an account state by address.
     * 
     * @param address The account address
     * @return Result containing the account if found, or an error
     */
    Result<state::Account> get_account(const state::Address& address) const;

    /**
     * Check if an account exists.
     * 
     * @param address The account address
     * @return true if the account exists, false otherwise
     */
    bool account_exists(const state::Address& address) const;

    /**
     * Delete an account (used for testing or state pruning).
     * 
     * @param address The account address
     * @return Result indicating success or error
     */
    Result<void> delete_account(const state::Address& address);

    // ========== Block Operations ==========

    /**
     * Store a block.
     * 
     * This stores the block data and creates index entries:
     * - Blocks CF: height → block
     * - BlockHashes CF: hash → height
     * 
     * @param block The block to store
     * @return Result indicating success or error
     */
    Result<void> store_block(const consensus::Block& block);

    /**
     * Retrieve a block by height.
     * 
     * @param height The block height
     * @return Result containing the block if found, or an error
     */
    Result<consensus::Block> get_block_by_height(uint64_t height) const;

    /**
     * Retrieve a block by hash.
     * 
     * This first looks up the height from the hash, then retrieves the block.
     * 
     * @param hash The block hash
     * @return Result containing the block if found, or an error
     */
    Result<consensus::Block> get_block_by_hash(const crypto::Blake3Hash& hash) const;

    /**
     * Get the height of a block by its hash.
     * 
     * @param hash The block hash
     * @return Result containing the height if found, or an error
     */
    Result<uint64_t> get_block_height(const crypto::Blake3Hash& hash) const;

    /**
     * Check if a block exists at the given height.
     * 
     * @param height The block height
     * @return true if a block exists at this height, false otherwise
     */
    bool block_exists(uint64_t height) const;

    /**
     * Delete a block (used for testing or chain reorganization).
     * 
     * This removes both the block data and index entries.
     * 
     * @param height The block height
     * @return Result indicating success or error
     */
    Result<void> delete_block(uint64_t height);

    // ========== Transaction Receipt Operations ==========

    /**
     * Store a transaction receipt.
     * 
     * @param receipt The transaction receipt to store
     * @return Result indicating success or error
     */
    Result<void> store_transaction_receipt(const state::TransactionReceipt& receipt);

    /**
     * Retrieve a transaction receipt by transaction hash.
     * 
     * @param tx_hash The transaction hash
     * @return Result containing the receipt if found, or an error
     */
    Result<state::TransactionReceipt> get_transaction_receipt(
        const crypto::Blake3Hash& tx_hash
    ) const;

    /**
     * Check if a transaction receipt exists.
     * 
     * @param tx_hash The transaction hash
     * @return true if the receipt exists, false otherwise
     */
    bool receipt_exists(const crypto::Blake3Hash& tx_hash) const;

    /**
     * Delete a transaction receipt (used for testing).
     * 
     * @param tx_hash The transaction hash
     * @return Result indicating success or error
     */
    Result<void> delete_transaction_receipt(const crypto::Blake3Hash& tx_hash);

    // ========== Batch Operations ==========

    /**
     * Store multiple accounts atomically.
     * 
     * All accounts are stored in a single batch write.
     * 
     * @param accounts The accounts to store
     * @return Result indicating success or error
     */
    Result<void> store_accounts_batch(const std::vector<state::Account>& accounts);

    /**
     * Store a block and its transaction receipts atomically.
     * 
     * This ensures that the block and all its receipts are stored together.
     * 
     * @param block The block to store
     * @param receipts The transaction receipts for the block
     * @return Result indicating success or error
     */
    Result<void> store_block_with_receipts(
        const consensus::Block& block,
        const std::vector<state::TransactionReceipt>& receipts
    );

    // ========== Metadata Operations ==========

    /**
     * Store a metadata key-value pair.
     * 
     * Used for storing chain configuration like chain_id, genesis_hash, etc.
     * 
     * @param key The metadata key
     * @param value The metadata value
     * @return Result indicating success or error
     */
    Result<void> store_metadata(const std::string& key, const std::vector<uint8_t>& value);

    /**
     * Retrieve a metadata value by key.
     * 
     * @param key The metadata key
     * @return Result containing the value if found, or an error
     */
    Result<std::vector<uint8_t>> get_metadata(const std::string& key) const;

    /**
     * Get the underlying database.
     * 
     * @return Shared pointer to the database
     */
    std::shared_ptr<Database> database() const { return db_; }

private:
    std::shared_ptr<Database> db_;
};

} // namespace storage
} // namespace sarafu
