#include "sarafu/storage/database.h"
#include <map>
#include <mutex>
#include <filesystem>

// TODO: Replace with actual RocksDB implementation
// For now, using in-memory map for testing

namespace sarafu {
namespace storage {

// BatchWrite implementation
BatchWrite::BatchWrite() = default;
BatchWrite::~BatchWrite() = default;

void BatchWrite::put(ColumnFamily cf, const std::vector<uint8_t>& key, const std::vector<uint8_t>& value) {
    Operation op;
    op.type = Operation::Type::Put;
    op.cf = cf;
    op.key = key;
    op.value = value;
    operations_.push_back(std::move(op));
}

void BatchWrite::del(ColumnFamily cf, const std::vector<uint8_t>& key) {
    Operation op;
    op.type = Operation::Type::Delete;
    op.cf = cf;
    op.key = key;
    operations_.push_back(std::move(op));
}

void BatchWrite::clear() {
    operations_.clear();
}

size_t BatchWrite::size() const {
    return operations_.size();
}

// Database implementation using in-memory storage
// TODO: Replace with RocksDB implementation
class Database::Impl {
public:
    Impl() = default;

    Result<void> put(ColumnFamily cf, const std::vector<uint8_t>& key, const std::vector<uint8_t>& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& cf_map = data_[cf];
        cf_map[key] = value;
        return Result<void>::ok();
    }

    Result<std::vector<uint8_t>> get(ColumnFamily cf, const std::vector<uint8_t>& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto cf_it = data_.find(cf);
        if (cf_it == data_.end()) {
            return Result<std::vector<uint8_t>>::error("Key not found");
        }

        auto& cf_map = cf_it->second;
        auto it = cf_map.find(key);
        if (it == cf_map.end()) {
            return Result<std::vector<uint8_t>>::error("Key not found");
        }

        return Result<std::vector<uint8_t>>::ok(it->second);
    }

    Result<void> del(ColumnFamily cf, const std::vector<uint8_t>& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto cf_it = data_.find(cf);
        if (cf_it == data_.end()) {
            return Result<void>::ok(); // Key doesn't exist, nothing to delete
        }

        auto& cf_map = cf_it->second;
        cf_map.erase(key);
        return Result<void>::ok();
    }

    Result<void> write_batch(const BatchWrite& batch) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Execute all operations atomically
        for (const auto& op : batch.operations_) {
            auto& cf_map = data_[op.cf];
            
            if (op.type == BatchWrite::Operation::Type::Put) {
                cf_map[op.key] = op.value;
            } else {
                cf_map.erase(op.key);
            }
        }

        return Result<void>::ok();
    }

    bool exists(ColumnFamily cf, const std::vector<uint8_t>& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto cf_it = data_.find(cf);
        if (cf_it == data_.end()) {
            return false;
        }

        auto& cf_map = cf_it->second;
        return cf_map.find(key) != cf_map.end();
    }

private:
    // In-memory storage: ColumnFamily -> (Key -> Value)
    mutable std::mutex mutex_;
    std::map<ColumnFamily, std::map<std::vector<uint8_t>, std::vector<uint8_t>>> data_;
};

// Database public interface
Database::Database() : impl_(std::make_unique<Impl>()) {}

Database::~Database() = default;

Result<std::unique_ptr<Database>> Database::open(const std::string& path) {
    // Create directory if it doesn't exist
    try {
        std::filesystem::create_directories(path);
    } catch (const std::exception& e) {
        return Result<std::unique_ptr<Database>>::error(
            std::string("Failed to create database directory: ") + e.what()
        );
    }

    // TODO: Open RocksDB with column families
    // For now, just create an in-memory database
    auto db = std::unique_ptr<Database>(new Database());
    return Result<std::unique_ptr<Database>>::ok(std::move(db));
}

Result<void> Database::put(
    ColumnFamily cf,
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& value
) {
    return impl_->put(cf, key, value);
}

Result<std::vector<uint8_t>> Database::get(
    ColumnFamily cf,
    const std::vector<uint8_t>& key
) const {
    return impl_->get(cf, key);
}

Result<void> Database::del(
    ColumnFamily cf,
    const std::vector<uint8_t>& key
) {
    return impl_->del(cf, key);
}

Result<void> Database::write_batch(const BatchWrite& batch) {
    return impl_->write_batch(batch);
}

bool Database::exists(ColumnFamily cf, const std::vector<uint8_t>& key) const {
    return impl_->exists(cf, key);
}

} // namespace storage
} // namespace sarafu
