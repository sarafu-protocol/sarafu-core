#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <rocksdb/db.h>

namespace sarafu {
namespace storage {

/**
 * Record of an applied database migration.
 */
struct MigrationRecord {
    uint32_t version;           // Migration version number
    std::string name;           // Migration name
    std::string description;    // What the migration does
    uint64_t applied_at;        // Unix timestamp when applied
    std::string checksum;       // SHA256 of migration code
    bool success;               // Whether migration succeeded
    std::string error_message;  // Error if migration failed

    MigrationRecord()
        : version(0), name(""), description(""), applied_at(0),
          checksum(""), success(false), error_message("") {}

    MigrationRecord(uint32_t v, const std::string& n, const std::string& desc,
                   uint64_t applied, const std::string& cs, bool succ, const std::string& err)
        : version(v), name(n), description(desc), applied_at(applied),
          checksum(cs), success(succ), error_message(err) {}

    // Serialization
    std::vector<uint8_t> serialize() const;
    static MigrationRecord deserialize(const std::vector<uint8_t>& data);
};

/**
 * Abstract base class for database migrations.
 * 
 * Each migration must implement:
 * - version(): Returns the migration version number
 * - name(): Returns a descriptive name
 * - description(): Returns what the migration does
 * - up(): Applies the migration
 * - down(): Reverts the migration
 */
class Migration {
public:
    virtual ~Migration() = default;

    /**
     * Get the migration version number.
     * Versions must be sequential starting from 1.
     */
    virtual uint32_t version() const = 0;

    /**
     * Get the migration name (e.g., "add_validator_metadata").
     */
    virtual std::string name() const = 0;

    /**
     * Get a description of what this migration does.
     */
    virtual std::string description() const = 0;

    /**
     * Apply the migration (upgrade).
     * Throws std::runtime_error on failure.
     */
    virtual void up(rocksdb::DB* db) = 0;

    /**
     * Revert the migration (downgrade).
     * Throws std::runtime_error on failure.
     */
    virtual void down(rocksdb::DB* db) = 0;

    /**
     * Get a checksum of the migration code for verification.
     * This helps detect if migration code has changed.
     */
    virtual std::string checksum() const = 0;
};

/**
 * Migration runner that manages database schema versions.
 */
class MigrationRunner {
public:
    /**
     * Create a migration runner for the given database.
     */
    explicit MigrationRunner(rocksdb::DB* db);

    /**
     * Register a migration.
     * Migrations must be registered in order by version number.
     */
    void register_migration(std::unique_ptr<Migration> migration);

    /**
     * Get the current schema version from the database.
     * Returns 0 if no migrations have been applied.
     */
    uint32_t get_current_version() const;

    /**
     * Get the target schema version (highest registered migration).
     */
    uint32_t get_target_version() const;

    /**
     * Run all pending migrations to bring database to target version.
     * Returns true on success, false on failure.
     * On failure, error_message is populated with details.
     */
    bool migrate_to_latest(std::string& error_message);

    /**
     * Migrate to a specific version.
     * Can upgrade or downgrade depending on current version.
     * Returns true on success, false on failure.
     */
    bool migrate_to_version(uint32_t target_version, std::string& error_message);

    /**
     * Get all migration records from the database.
     */
    std::vector<MigrationRecord> get_migration_history() const;

    /**
     * Verify that all applied migrations match registered migrations.
     * Returns true if checksums match, false otherwise.
     */
    bool verify_migration_integrity(std::string& error_message) const;

private:
    rocksdb::DB* db_;
    std::vector<std::unique_ptr<Migration>> migrations_;

    // Internal helpers
    void set_schema_version(uint32_t version);
    void record_migration(const Migration& migration, bool success, const std::string& error);
    bool apply_migration(Migration& migration, std::string& error_message);
    bool revert_migration(Migration& migration, std::string& error_message);
    
    // Column family for migration metadata
    static const std::string MIGRATION_CF_NAME;
    static const std::string VERSION_KEY;
    static const std::string MIGRATION_RECORD_PREFIX;
};

/**
 * Helper function to create the migration column family if it doesn't exist.
 */
bool ensure_migration_column_family(rocksdb::DB* db);

} // namespace storage
} // namespace sarafu
