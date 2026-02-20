#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <map>

namespace sarafu {
namespace storage {

/**
 * ColumnFamily represents a logical partition in the database.
 * 
 * Column families allow organizing different types of data with
 * independent configuration and performance characteristics.
 */
enum class ColumnFamily {
    Accounts,           // address → AccountState
    Blocks,             // height → StoredBlock
    BlockHashes,        // hash → height
    Transactions,       // tx_hash → TransactionReceipt
    Validators,         // validator_id → ValidatorState
    ValidatorSets,      // epoch → ValidatorSet
    Governance,         // proposal_id → Proposal
    Metadata            // key → value (chain_id, genesis_hash, etc.)
};

/**
 * Result type for database operations.
 * 
 * Contains either a value (on success) or an error message (on failure).
 */
template<typename T>
class Result {
public:
    static Result<T> ok(T value) {
        Result<T> result;
        result.success_ = true;
        result.value_ = std::move(value);
        return result;
    }

    static Result<T> error(std::string message) {
        Result<T> result;
        result.success_ = false;
        result.error_message_ = std::move(message);
        return result;
    }

    bool is_ok() const { return success_; }
    bool is_error() const { return !success_; }

    const T& value() const { return value_; }
    T& value() { return value_; }
    const std::string& error() const { return error_message_; }

private:
    bool success_ = false;
    T value_;
    std::string error_message_;
};

// Specialization for void result
template<>
class Result<void> {
public:
    static Result<void> ok() {
        Result<void> result;
        result.success_ = true;
        return result;
    }

    static Result<void> error(std::string message) {
        Result<void> result;
        result.success_ = false;
        result.error_message_ = std::move(message);
        return result;
    }

    bool is_ok() const { return success_; }
    bool is_error() const { return !success_; }
    const std::string& error() const { return error_message_; }

private:
    bool success_ = false;
    std::string error_message_;
};

/**
 * BatchWrite represents a collection of database operations to be executed atomically.
 * 
 * All operations in a batch either succeed together or fail together,
 * ensuring consistency.
 */
class BatchWrite {
public:
    BatchWrite();
    ~BatchWrite();

    // Add operations to the batch
    void put(ColumnFamily cf, const std::vector<uint8_t>& key, const std::vector<uint8_t>& value);
    void del(ColumnFamily cf, const std::vector<uint8_t>& key);

    // Clear all operations
    void clear();

    // Get number of operations
    size_t size() const;

private:
    friend class Database;
    
    struct Operation {
        enum class Type { Put, Delete };
        Type type;
        ColumnFamily cf;
        std::vector<uint8_t> key;
        std::vector<uint8_t> value;
    };

    std::vector<Operation> operations_;
};

/**
 * Database provides a key-value storage interface backed by RocksDB.
 * 
 * Features:
 * - Column families for organizing different data types
 * - Atomic batch writes for consistency
 * - Efficient get/put/delete operations
 * - Thread-safe operations
 * 
 * The database is organized into column families:
 * - Accounts: Account state indexed by address
 * - Blocks: Block data indexed by height
 * - BlockHashes: Height lookup indexed by block hash
 * - Transactions: Transaction receipts indexed by tx hash
 * - Validators: Validator state indexed by validator ID
 * - ValidatorSets: Validator sets indexed by epoch
 * - Governance: Governance proposals indexed by proposal ID
 * - Metadata: Chain metadata (chain_id, genesis_hash, etc.)
 */
class Database {
public:
    /**
     * Open a database at the specified path.
     * 
     * Creates the database if it doesn't exist.
     * Initializes all column families.
     * 
     * @param path Directory path for the database
     * @return Result containing the database or an error
     */
    static Result<std::unique_ptr<Database>> open(const std::string& path);

    /**
     * Close the database.
     * 
     * Flushes all pending writes and releases resources.
     */
    ~Database();

    // Disable copy and move
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    /**
     * Store a key-value pair in the specified column family.
     * 
     * @param cf Column family to write to
     * @param key The key to store
     * @param value The value to store
     * @return Result indicating success or error
     */
    Result<void> put(
        ColumnFamily cf,
        const std::vector<uint8_t>& key,
        const std::vector<uint8_t>& value
    );

    /**
     * Retrieve a value by key from the specified column family.
     * 
     * @param cf Column family to read from
     * @param key The key to look up
     * @return Result containing the value if found, or an error
     */
    Result<std::vector<uint8_t>> get(
        ColumnFamily cf,
        const std::vector<uint8_t>& key
    ) const;

    /**
     * Delete a key-value pair from the specified column family.
     * 
     * @param cf Column family to delete from
     * @param key The key to delete
     * @return Result indicating success or error
     */
    Result<void> del(
        ColumnFamily cf,
        const std::vector<uint8_t>& key
    );

    /**
     * Execute a batch of operations atomically.
     * 
     * All operations in the batch either succeed together or fail together.
     * This ensures consistency when updating multiple keys.
     * 
     * @param batch The batch of operations to execute
     * @return Result indicating success or error
     */
    Result<void> write_batch(const BatchWrite& batch);

    /**
     * Check if a key exists in the specified column family.
     * 
     * @param cf Column family to check
     * @param key The key to check
     * @return true if the key exists, false otherwise
     */
    bool exists(ColumnFamily cf, const std::vector<uint8_t>& key) const;

private:
    Database();

    // Implementation details hidden from header
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace storage
} // namespace sarafu
