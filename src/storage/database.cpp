#include "sarafu/storage/database.h"
#include <filesystem>
#include <map>
#include <mutex>
#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/write_batch.h>

namespace sarafu {
namespace storage {

namespace {

struct ColumnFamilyEntry {
    ColumnFamily cf;
    const char* name;
};

const std::vector<ColumnFamilyEntry>& column_family_entries() {
    static const std::vector<ColumnFamilyEntry> entries = {
        {ColumnFamily::Accounts, "accounts"},
        {ColumnFamily::Blocks, "blocks"},
        {ColumnFamily::BlockHashes, "block_hashes"},
        {ColumnFamily::Transactions, "transactions"},
        {ColumnFamily::Validators, "validators"},
        {ColumnFamily::ValidatorSets, "validator_sets"},
        {ColumnFamily::Governance, "governance"},
        {ColumnFamily::Metadata, "metadata"}
    };
    return entries;
}

const char* column_family_name(ColumnFamily cf) {
    for (const auto& entry : column_family_entries()) {
        if (entry.cf == cf) {
            return entry.name;
        }
    }
    return "default";
}

rocksdb::ColumnFamilyOptions default_cf_options() {
    rocksdb::ColumnFamilyOptions options;
    return options;
}

} // namespace

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

class Database::Impl {
public:
    Impl(std::unique_ptr<rocksdb::DB> db,
         std::map<ColumnFamily, rocksdb::ColumnFamilyHandle*> handles,
         std::vector<rocksdb::ColumnFamilyHandle*> all_handles)
        : db_(std::move(db)), handles_(std::move(handles)), all_handles_(std::move(all_handles)) {}

    ~Impl() {
        for (auto* handle : all_handles_) {
            delete handle;
        }
    }

    Result<void> put(ColumnFamily cf, const std::vector<uint8_t>& key, const std::vector<uint8_t>& value) {
        auto* handle = handle_for(cf);
        if (!handle) {
            return Result<void>::error("Unknown column family");
        }
        rocksdb::WriteOptions write_options;
        rocksdb::Slice key_slice(reinterpret_cast<const char*>(key.data()), key.size());
        rocksdb::Slice value_slice(reinterpret_cast<const char*>(value.data()), value.size());
        auto status = db_->Put(write_options, handle, key_slice, value_slice);
        if (!status.ok()) {
            return Result<void>::error(status.ToString());
        }
        return Result<void>::ok();
    }

    Result<std::vector<uint8_t>> get(ColumnFamily cf, const std::vector<uint8_t>& key) const {
        auto* handle = handle_for(cf);
        if (!handle) {
            return Result<std::vector<uint8_t>>::error("Unknown column family");
        }
        rocksdb::ReadOptions read_options;
        std::string value;
        rocksdb::Slice key_slice(reinterpret_cast<const char*>(key.data()), key.size());
        auto status = db_->Get(read_options, handle, key_slice, &value);
        if (status.IsNotFound()) {
            return Result<std::vector<uint8_t>>::error("Key not found");
        }
        if (!status.ok()) {
            return Result<std::vector<uint8_t>>::error(status.ToString());
        }
        return Result<std::vector<uint8_t>>::ok(std::vector<uint8_t>(value.begin(), value.end()));
    }

    Result<void> del(ColumnFamily cf, const std::vector<uint8_t>& key) {
        auto* handle = handle_for(cf);
        if (!handle) {
            return Result<void>::error("Unknown column family");
        }
        rocksdb::WriteOptions write_options;
        rocksdb::Slice key_slice(reinterpret_cast<const char*>(key.data()), key.size());
        auto status = db_->Delete(write_options, handle, key_slice);
        if (!status.ok() && !status.IsNotFound()) {
            return Result<void>::error(status.ToString());
        }
        return Result<void>::ok();
    }

    Result<void> write_batch(const BatchWrite& batch) {
        rocksdb::WriteBatch write_batch;
        for (const auto& op : batch.operations_) {
            auto* handle = handle_for(op.cf);
            if (!handle) {
                return Result<void>::error("Unknown column family");
            }
            rocksdb::Slice key_slice(reinterpret_cast<const char*>(op.key.data()), op.key.size());
            if (op.type == BatchWrite::Operation::Type::Put) {
                rocksdb::Slice value_slice(reinterpret_cast<const char*>(op.value.data()), op.value.size());
                write_batch.Put(handle, key_slice, value_slice);
            } else {
                write_batch.Delete(handle, key_slice);
            }
        }
        rocksdb::WriteOptions write_options;
        auto status = db_->Write(write_options, &write_batch);
        if (!status.ok()) {
            return Result<void>::error(status.ToString());
        }
        return Result<void>::ok();
    }

    bool exists(ColumnFamily cf, const std::vector<uint8_t>& key) const {
        auto* handle = handle_for(cf);
        if (!handle) {
            return false;
        }
        rocksdb::ReadOptions read_options;
        std::string value;
        rocksdb::Slice key_slice(reinterpret_cast<const char*>(key.data()), key.size());
        auto status = db_->Get(read_options, handle, key_slice, &value);
        return status.ok();
    }

private:
    rocksdb::ColumnFamilyHandle* handle_for(ColumnFamily cf) const {
        auto it = handles_.find(cf);
        if (it == handles_.end()) {
            return nullptr;
        }
        return it->second;
    }

    std::unique_ptr<rocksdb::DB> db_;
    std::map<ColumnFamily, rocksdb::ColumnFamilyHandle*> handles_;
    std::vector<rocksdb::ColumnFamilyHandle*> all_handles_;
};

Database::Database() : impl_(nullptr) {}

Database::~Database() = default;

Result<std::unique_ptr<Database>> Database::open(const std::string& path) {
    try {
        std::filesystem::create_directories(path);
    } catch (const std::exception& e) {
        return Result<std::unique_ptr<Database>>::error(
            std::string("Failed to create database directory: ") + e.what()
        );
    }

    rocksdb::Options options;
    options.create_if_missing = true;
    options.create_missing_column_families = true;
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();

    std::vector<rocksdb::ColumnFamilyDescriptor> descriptors;
    descriptors.emplace_back(rocksdb::kDefaultColumnFamilyName, default_cf_options());
    for (const auto& entry : column_family_entries()) {
        descriptors.emplace_back(entry.name, default_cf_options());
    }

    rocksdb::DB* db_ptr = nullptr;
    std::vector<rocksdb::ColumnFamilyHandle*> handles;
    auto status = rocksdb::DB::Open(options, path, descriptors, &handles, &db_ptr);
    if (!status.ok()) {
        return Result<std::unique_ptr<Database>>::error("Failed to open RocksDB: " + status.ToString());
    }

    std::unique_ptr<rocksdb::DB> db(db_ptr);
    std::map<ColumnFamily, rocksdb::ColumnFamilyHandle*> handle_map;
    std::vector<rocksdb::ColumnFamilyHandle*> all_handles;
    all_handles.reserve(handles.size());

    if (!handles.empty()) {
        all_handles.push_back(handles[0]);
        for (size_t i = 1; i < handles.size(); ++i) {
            all_handles.push_back(handles[i]);
            handle_map.emplace(column_family_entries()[i - 1].cf, handles[i]);
        }
    }

    auto database = std::unique_ptr<Database>(new Database());
    database->impl_ = std::make_unique<Impl>(std::move(db), std::move(handle_map), std::move(all_handles));
    return Result<std::unique_ptr<Database>>::ok(std::move(database));
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
