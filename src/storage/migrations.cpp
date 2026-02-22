#include "migrations.h"
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <iostream>

namespace sarafu {
namespace storage {

// Constants
const std::string MigrationRunner::MIGRATION_CF_NAME = "migrations";
const std::string MigrationRunner::VERSION_KEY = "schema_version";
const std::string MigrationRunner::MIGRATION_RECORD_PREFIX = "migration_";

// ============================================================================
// MigrationRecord Implementation
// ============================================================================

std::vector<uint8_t> MigrationRecord::serialize() const {
    std::vector<uint8_t> result;
    
    // Serialize version (4 bytes)
    result.push_back((version >> 24) & 0xFF);
    result.push_back((version >> 16) & 0xFF);
    result.push_back((version >> 8) & 0xFF);
    result.push_back(version & 0xFF);
    
    // Serialize name length and name
    uint32_t name_len = static_cast<uint32_t>(name.size());
    result.push_back((name_len >> 24) & 0xFF);
    result.push_back((name_len >> 16) & 0xFF);
    result.push_back((name_len >> 8) & 0xFF);
    result.push_back(name_len & 0xFF);
    result.insert(result.end(), name.begin(), name.end());
    
    // Serialize description length and description
    uint32_t desc_len = static_cast<uint32_t>(description.size());
    result.push_back((desc_len >> 24) & 0xFF);
    result.push_back((desc_len >> 16) & 0xFF);
    result.push_back((desc_len >> 8) & 0xFF);
    result.push_back(desc_len & 0xFF);
    result.insert(result.end(), description.begin(), description.end());
    
    // Serialize applied_at (8 bytes)
    for (int i = 7; i >= 0; --i) {
        result.push_back((applied_at >> (i * 8)) & 0xFF);
    }
    
    // Serialize checksum length and checksum
    uint32_t checksum_len = static_cast<uint32_t>(checksum.size());
    result.push_back((checksum_len >> 24) & 0xFF);
    result.push_back((checksum_len >> 16) & 0xFF);
    result.push_back((checksum_len >> 8) & 0xFF);
    result.push_back(checksum_len & 0xFF);
    result.insert(result.end(), checksum.begin(), checksum.end());
    
    // Serialize success (1 byte)
    result.push_back(success ? 1 : 0);
    
    // Serialize error_message length and error_message
    uint32_t error_len = static_cast<uint32_t>(error_message.size());
    result.push_back((error_len >> 24) & 0xFF);
    result.push_back((error_len >> 16) & 0xFF);
    result.push_back((error_len >> 8) & 0xFF);
    result.push_back(error_len & 0xFF);
    result.insert(result.end(), error_message.begin(), error_message.end());
    
    return result;
}

MigrationRecord MigrationRecord::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 4 + 4 + 4 + 8 + 4 + 1 + 4) {
        throw std::invalid_argument("MigrationRecord::deserialize: data too short");
    }
    
    size_t offset = 0;
    
    // Deserialize version
    uint32_t v = (static_cast<uint32_t>(data[offset]) << 24) |
                 (static_cast<uint32_t>(data[offset + 1]) << 16) |
                 (static_cast<uint32_t>(data[offset + 2]) << 8) |
                 static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Deserialize name
    uint32_t name_len = (static_cast<uint32_t>(data[offset]) << 24) |
                        (static_cast<uint32_t>(data[offset + 1]) << 16) |
                        (static_cast<uint32_t>(data[offset + 2]) << 8) |
                        static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    std::string n(data.begin() + offset, data.begin() + offset + name_len);
    offset += name_len;
    
    // Deserialize description
    uint32_t desc_len = (static_cast<uint32_t>(data[offset]) << 24) |
                        (static_cast<uint32_t>(data[offset + 1]) << 16) |
                        (static_cast<uint32_t>(data[offset + 2]) << 8) |
                        static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    std::string desc(data.begin() + offset, data.begin() + offset + desc_len);
    offset += desc_len;
    
    // Deserialize applied_at
    uint64_t applied = 0;
    for (int i = 0; i < 8; ++i) {
        applied = (applied << 8) | data[offset++];
    }
    
    // Deserialize checksum
    uint32_t checksum_len = (static_cast<uint32_t>(data[offset]) << 24) |
                            (static_cast<uint32_t>(data[offset + 1]) << 16) |
                            (static_cast<uint32_t>(data[offset + 2]) << 8) |
                            static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    std::string cs(data.begin() + offset, data.begin() + offset + checksum_len);
    offset += checksum_len;
    
    // Deserialize success
    bool succ = (data[offset++] != 0);
    
    // Deserialize error_message
    uint32_t error_len = (static_cast<uint32_t>(data[offset]) << 24) |
                         (static_cast<uint32_t>(data[offset + 1]) << 16) |
                         (static_cast<uint32_t>(data[offset + 2]) << 8) |
                         static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    std::string err(data.begin() + offset, data.begin() + offset + error_len);
    
    return MigrationRecord(v, n, desc, applied, cs, succ, err);
}

// ============================================================================
// MigrationRunner Implementation
// ============================================================================

MigrationRunner::MigrationRunner(rocksdb::DB* db)
    : db_(db), migrations_() {
    if (!db_) {
        throw std::invalid_argument("MigrationRunner: db cannot be null");
    }
}

void MigrationRunner::register_migration(std::unique_ptr<Migration> migration) {
    if (!migration) {
        throw std::invalid_argument("MigrationRunner::register_migration: migration cannot be null");
    }
    
    // Verify version is sequential
    uint32_t expected_version = static_cast<uint32_t>(migrations_.size()) + 1;
    if (migration->version() != expected_version) {
        throw std::invalid_argument(
            "MigrationRunner::register_migration: migrations must be registered in order. "
            "Expected version " + std::to_string(expected_version) +
            ", got " + std::to_string(migration->version())
        );
    }
    
    migrations_.push_back(std::move(migration));
}

uint32_t MigrationRunner::get_current_version() const {
    rocksdb::ReadOptions read_options;
    std::string value;
    
    rocksdb::Status status = db_->Get(read_options, VERSION_KEY, &value);
    
    if (status.IsNotFound()) {
        return 0;  // No migrations applied yet
    }
    
    if (!status.ok()) {
        std::cerr << "Error reading schema version: " << status.ToString() << std::endl;
        return 0;
    }
    
    if (value.size() != 4) {
        std::cerr << "Invalid schema version format" << std::endl;
        return 0;
    }
    
    uint32_t version = (static_cast<uint32_t>(static_cast<uint8_t>(value[0])) << 24) |
                       (static_cast<uint32_t>(static_cast<uint8_t>(value[1])) << 16) |
                       (static_cast<uint32_t>(static_cast<uint8_t>(value[2])) << 8) |
                       static_cast<uint32_t>(static_cast<uint8_t>(value[3]));
    
    return version;
}

uint32_t MigrationRunner::get_target_version() const {
    return static_cast<uint32_t>(migrations_.size());
}

bool MigrationRunner::migrate_to_latest(std::string& error_message) {
    return migrate_to_version(get_target_version(), error_message);
}

bool MigrationRunner::migrate_to_version(uint32_t target_version, std::string& error_message) {
    uint32_t current_version = get_current_version();
    
    if (target_version > migrations_.size()) {
        error_message = "Target version " + std::to_string(target_version) +
                       " exceeds maximum registered version " + std::to_string(migrations_.size());
        return false;
    }
    
    if (current_version == target_version) {
        // Already at target version
        return true;
    }
    
    std::cout << "Migrating database from version " << current_version
              << " to version " << target_version << std::endl;
    
    if (target_version > current_version) {
        // Upgrade: apply migrations sequentially
        for (uint32_t v = current_version + 1; v <= target_version; ++v) {
            Migration& migration = *migrations_[v - 1];
            
            std::cout << "Applying migration " << v << ": " << migration.name() << std::endl;
            
            if (!apply_migration(migration, error_message)) {
                std::cerr << "Migration " << v << " failed: " << error_message << std::endl;
                return false;
            }
            
            std::cout << "Migration " << v << " completed successfully" << std::endl;
        }
    } else {
        // Downgrade: revert migrations in reverse order
        for (uint32_t v = current_version; v > target_version; --v) {
            Migration& migration = *migrations_[v - 1];
            
            std::cout << "Reverting migration " << v << ": " << migration.name() << std::endl;
            
            if (!revert_migration(migration, error_message)) {
                std::cerr << "Migration " << v << " revert failed: " << error_message << std::endl;
                return false;
            }
            
            std::cout << "Migration " << v << " reverted successfully" << std::endl;
        }
    }
    
    std::cout << "Database migration completed successfully" << std::endl;
    return true;
}

std::vector<MigrationRecord> MigrationRunner::get_migration_history() const {
    std::vector<MigrationRecord> history;
    
    rocksdb::ReadOptions read_options;
    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(read_options));
    
    for (it->Seek(MIGRATION_RECORD_PREFIX); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        
        if (key.find(MIGRATION_RECORD_PREFIX) != 0) {
            break;  // No more migration records
        }
        
        try {
            std::vector<uint8_t> data(it->value().data(),
                                     it->value().data() + it->value().size());
            MigrationRecord record = MigrationRecord::deserialize(data);
            history.push_back(record);
        } catch (const std::exception& e) {
            std::cerr << "Error deserializing migration record: " << e.what() << std::endl;
        }
    }
    
    return history;
}

bool MigrationRunner::verify_migration_integrity(std::string& error_message) const {
    auto history = get_migration_history();
    
    for (const auto& record : history) {
        if (record.version > migrations_.size()) {
            error_message = "Migration record version " + std::to_string(record.version) +
                           " exceeds registered migrations";
            return false;
        }
        
        const Migration& migration = *migrations_[record.version - 1];
        
        if (migration.checksum() != record.checksum) {
            error_message = "Migration " + std::to_string(record.version) +
                           " checksum mismatch. Migration code may have changed.";
            return false;
        }
    }
    
    return true;
}

void MigrationRunner::set_schema_version(uint32_t version) {
    std::vector<uint8_t> value = {
        static_cast<uint8_t>((version >> 24) & 0xFF),
        static_cast<uint8_t>((version >> 16) & 0xFF),
        static_cast<uint8_t>((version >> 8) & 0xFF),
        static_cast<uint8_t>(version & 0xFF)
    };
    
    rocksdb::WriteOptions write_options;
    rocksdb::Slice value_slice(reinterpret_cast<const char*>(value.data()), value.size());
    
    rocksdb::Status status = db_->Put(write_options, VERSION_KEY, value_slice);
    
    if (!status.ok()) {
        throw std::runtime_error("Failed to set schema version: " + status.ToString());
    }
}

void MigrationRunner::record_migration(const Migration& migration, bool success,
                                      const std::string& error) {
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    MigrationRecord record(
        migration.version(),
        migration.name(),
        migration.description(),
        now,
        migration.checksum(),
        success,
        error
    );
    
    std::string key = MIGRATION_RECORD_PREFIX + std::to_string(migration.version());
    std::vector<uint8_t> value = record.serialize();
    
    rocksdb::WriteOptions write_options;
    rocksdb::Slice value_slice(reinterpret_cast<const char*>(value.data()), value.size());
    
    rocksdb::Status status = db_->Put(write_options, key, value_slice);
    
    if (!status.ok()) {
        std::cerr << "Failed to record migration: " << status.ToString() << std::endl;
    }
}

bool MigrationRunner::apply_migration(Migration& migration, std::string& error_message) {
    try {
        // Apply the migration
        migration.up(db_);
        
        // Update schema version
        set_schema_version(migration.version());
        
        // Record successful migration
        record_migration(migration, true, "");
        
        return true;
    } catch (const std::exception& e) {
        error_message = e.what();
        
        // Record failed migration
        record_migration(migration, false, error_message);
        
        return false;
    }
}

bool MigrationRunner::revert_migration(Migration& migration, std::string& error_message) {
    try {
        // Revert the migration
        migration.down(db_);
        
        // Update schema version
        set_schema_version(migration.version() - 1);
        
        // Record successful revert
        record_migration(migration, true, "reverted");
        
        return true;
    } catch (const std::exception& e) {
        error_message = e.what();
        
        // Record failed revert
        record_migration(migration, false, "revert failed: " + error_message);
        
        return false;
    }
}

bool ensure_migration_column_family(rocksdb::DB* db) {
    // In this implementation, we're using the default column family
    // for migration metadata. In a production system, you might want
    // to create a separate column family for migrations.
    return true;
}

} // namespace storage
} // namespace sarafu
