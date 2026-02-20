#include "sarafu/storage/state_storage.h"
#include <cstring>

namespace sarafu {
namespace storage {

StateStorage::StateStorage(std::shared_ptr<Database> db) : db_(std::move(db)) {}

StateStorage::~StateStorage() = default;

// ========== Account Operations ==========

Result<void> StateStorage::store_account(const state::Account& account) {
    auto key = account.address.serialize();
    auto value = account.serialize();
    return db_->put(ColumnFamily::Accounts, key, value);
}

Result<state::Account> StateStorage::get_account(const state::Address& address) const {
    auto key = address.serialize();
    auto result = db_->get(ColumnFamily::Accounts, key);
    
    if (result.is_error()) {
        return Result<state::Account>::error(result.error());
    }

    try {
        auto account = state::Account::deserialize(result.value());
        return Result<state::Account>::ok(std::move(account));
    } catch (const std::exception& e) {
        return Result<state::Account>::error(
            std::string("Failed to deserialize account: ") + e.what()
        );
    }
}

bool StateStorage::account_exists(const state::Address& address) const {
    auto key = address.serialize();
    return db_->exists(ColumnFamily::Accounts, key);
}

Result<void> StateStorage::delete_account(const state::Address& address) {
    auto key = address.serialize();
    return db_->del(ColumnFamily::Accounts, key);
}

// ========== Block Operations ==========

Result<void> StateStorage::store_block(const consensus::Block& block) {
    // Use batch write to store block and index atomically
    BatchWrite batch;

    // Store block by height
    std::vector<uint8_t> height_key(sizeof(uint64_t));
    std::memcpy(height_key.data(), &block.header.height, sizeof(uint64_t));
    auto block_data = block.serialize();
    batch.put(ColumnFamily::Blocks, height_key, block_data);

    // Store hash → height index
    auto hash_key = block.hash().serialize();
    std::vector<uint8_t> height_value(sizeof(uint64_t));
    std::memcpy(height_value.data(), &block.header.height, sizeof(uint64_t));
    batch.put(ColumnFamily::BlockHashes, hash_key, height_value);

    return db_->write_batch(batch);
}

Result<consensus::Block> StateStorage::get_block_by_height(uint64_t height) const {
    std::vector<uint8_t> key(sizeof(uint64_t));
    std::memcpy(key.data(), &height, sizeof(uint64_t));
    
    auto result = db_->get(ColumnFamily::Blocks, key);
    if (result.is_error()) {
        return Result<consensus::Block>::error(result.error());
    }

    try {
        auto block = consensus::Block::deserialize(result.value());
        return Result<consensus::Block>::ok(std::move(block));
    } catch (const std::exception& e) {
        return Result<consensus::Block>::error(
            std::string("Failed to deserialize block: ") + e.what()
        );
    }
}

Result<consensus::Block> StateStorage::get_block_by_hash(const crypto::Blake3Hash& hash) const {
    // First get the height from the hash
    auto height_result = get_block_height(hash);
    if (height_result.is_error()) {
        return Result<consensus::Block>::error(height_result.error());
    }

    // Then get the block by height
    return get_block_by_height(height_result.value());
}

Result<uint64_t> StateStorage::get_block_height(const crypto::Blake3Hash& hash) const {
    auto key = hash.serialize();
    auto result = db_->get(ColumnFamily::BlockHashes, key);
    
    if (result.is_error()) {
        return Result<uint64_t>::error(result.error());
    }

    if (result.value().size() != sizeof(uint64_t)) {
        return Result<uint64_t>::error("Invalid height data");
    }

    uint64_t height;
    std::memcpy(&height, result.value().data(), sizeof(uint64_t));
    return Result<uint64_t>::ok(height);
}

bool StateStorage::block_exists(uint64_t height) const {
    std::vector<uint8_t> key(sizeof(uint64_t));
    std::memcpy(key.data(), &height, sizeof(uint64_t));
    return db_->exists(ColumnFamily::Blocks, key);
}

Result<void> StateStorage::delete_block(uint64_t height) {
    // First get the block to find its hash
    auto block_result = get_block_by_height(height);
    if (block_result.is_error()) {
        return Result<void>::error(block_result.error());
    }

    // Use batch write to delete block and index atomically
    BatchWrite batch;

    // Delete block by height
    std::vector<uint8_t> height_key(sizeof(uint64_t));
    std::memcpy(height_key.data(), &height, sizeof(uint64_t));
    batch.del(ColumnFamily::Blocks, height_key);

    // Delete hash → height index
    auto hash_key = block_result.value().hash().serialize();
    batch.del(ColumnFamily::BlockHashes, hash_key);

    return db_->write_batch(batch);
}

// ========== Transaction Receipt Operations ==========

Result<void> StateStorage::store_transaction_receipt(const state::TransactionReceipt& receipt) {
    auto key = receipt.tx_hash.serialize();
    auto value = receipt.serialize();
    return db_->put(ColumnFamily::Transactions, key, value);
}

Result<state::TransactionReceipt> StateStorage::get_transaction_receipt(
    const crypto::Blake3Hash& tx_hash
) const {
    auto key = tx_hash.serialize();
    auto result = db_->get(ColumnFamily::Transactions, key);
    
    if (result.is_error()) {
        return Result<state::TransactionReceipt>::error(result.error());
    }

    try {
        auto receipt = state::TransactionReceipt::deserialize(result.value());
        return Result<state::TransactionReceipt>::ok(std::move(receipt));
    } catch (const std::exception& e) {
        return Result<state::TransactionReceipt>::error(
            std::string("Failed to deserialize receipt: ") + e.what()
        );
    }
}

bool StateStorage::receipt_exists(const crypto::Blake3Hash& tx_hash) const {
    auto key = tx_hash.serialize();
    return db_->exists(ColumnFamily::Transactions, key);
}

Result<void> StateStorage::delete_transaction_receipt(const crypto::Blake3Hash& tx_hash) {
    auto key = tx_hash.serialize();
    return db_->del(ColumnFamily::Transactions, key);
}

// ========== Batch Operations ==========

Result<void> StateStorage::store_accounts_batch(const std::vector<state::Account>& accounts) {
    BatchWrite batch;

    for (const auto& account : accounts) {
        auto key = account.address.serialize();
        auto value = account.serialize();
        batch.put(ColumnFamily::Accounts, key, value);
    }

    return db_->write_batch(batch);
}

Result<void> StateStorage::store_block_with_receipts(
    const consensus::Block& block,
    const std::vector<state::TransactionReceipt>& receipts
) {
    BatchWrite batch;

    // Store block by height
    std::vector<uint8_t> height_key(sizeof(uint64_t));
    std::memcpy(height_key.data(), &block.header.height, sizeof(uint64_t));
    auto block_data = block.serialize();
    batch.put(ColumnFamily::Blocks, height_key, block_data);

    // Store hash → height index
    auto hash_key = block.hash().serialize();
    std::vector<uint8_t> height_value(sizeof(uint64_t));
    std::memcpy(height_value.data(), &block.header.height, sizeof(uint64_t));
    batch.put(ColumnFamily::BlockHashes, hash_key, height_value);

    // Store all transaction receipts
    for (const auto& receipt : receipts) {
        auto receipt_key = receipt.tx_hash.serialize();
        auto receipt_value = receipt.serialize();
        batch.put(ColumnFamily::Transactions, receipt_key, receipt_value);
    }

    return db_->write_batch(batch);
}

// ========== Metadata Operations ==========

Result<void> StateStorage::store_metadata(const std::string& key, const std::vector<uint8_t>& value) {
    std::vector<uint8_t> key_bytes(key.begin(), key.end());
    return db_->put(ColumnFamily::Metadata, key_bytes, value);
}

Result<std::vector<uint8_t>> StateStorage::get_metadata(const std::string& key) const {
    std::vector<uint8_t> key_bytes(key.begin(), key.end());
    return db_->get(ColumnFamily::Metadata, key_bytes);
}

} // namespace storage
} // namespace sarafu
