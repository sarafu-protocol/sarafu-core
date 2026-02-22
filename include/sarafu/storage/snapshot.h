#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <optional>

namespace sarafu {
namespace storage {

/**
 * SnapshotManifest contains metadata about a blockchain state snapshot.
 * 
 * The manifest is stored alongside the snapshot data and includes
 * cryptographic commitments to ensure integrity.
 */
struct SnapshotManifest {
    uint64_t block_height;       // Block height of snapshot
    std::string block_hash;      // Block hash at snapshot height (hex)
    std::string state_root;      // State root hash (hex)
    uint64_t timestamp;          // Unix timestamp of creation
    uint64_t file_size;          // Compressed snapshot size in bytes
    std::string checksum;        // SHA256 checksum of snapshot file (hex)
    std::string signature;       // Ed25519 signature by snapshot creator (hex)
    std::string creator_pubkey;  // Public key of snapshot creator (hex)
    uint32_t schema_version;     // Database schema version
    std::string node_version;    // Node software version

    /**
     * Serialize manifest to JSON string.
     */
    std::string to_json() const;

    /**
     * Deserialize manifest from JSON string.
     * Returns nullopt if parsing fails.
     */
    static std::optional<SnapshotManifest> from_json(const std::string& json);
};

/**
 * SnapshotManager handles creation and restoration of blockchain state snapshots.
 * 
 * Snapshots use RocksDB checkpoints for consistent state capture and include
 * cryptographic signatures for integrity verification.
 */
class SnapshotManager {
public:
    /**
     * Create a new SnapshotManager.
     * 
     * @param db_path Path to the RocksDB database
     * @param snapshot_dir Directory to store snapshots
     */
    SnapshotManager(const std::string& db_path, const std::string& snapshot_dir);
    ~SnapshotManager();

    // Disable copy and move
    SnapshotManager(const SnapshotManager&) = delete;
    SnapshotManager& operator=(const SnapshotManager&) = delete;
    SnapshotManager(SnapshotManager&&) = delete;
    SnapshotManager& operator=(SnapshotManager&&) = delete;

    /**
     * Create a snapshot of the current blockchain state.
     * 
     * Process:
     * 1. Pause state updates (caller responsibility)
     * 2. Flush pending writes to disk
     * 3. Create RocksDB checkpoint
     * 4. Export validator state
     * 5. Create manifest with block height and hash
     * 6. Compress and sign snapshot
     * 
     * @param block_height Current block height
     * @param block_hash Current block hash (hex)
     * @param state_root Current state root (hex)
     * @param signing_key Ed25519 private key for signing (32 bytes)
     * @param output_path Optional custom output path (default: snapshot_dir/snapshot-<height>-<hash>.tar.gz)
     * @return Path to created snapshot file, or empty string on error
     */
    std::string create_snapshot(
        uint64_t block_height,
        const std::string& block_hash,
        const std::string& state_root,
        const std::vector<uint8_t>& signing_key,
        const std::string& output_path = ""
    );

    /**
     * Restore blockchain state from a snapshot.
     * 
     * Process:
     * 1. Verify snapshot signature
     * 2. Decompress snapshot
     * 3. Verify manifest integrity
     * 4. Restore RocksDB checkpoint
     * 5. Import validator state
     * 6. Verify state root matches manifest
     * 
     * @param snapshot_path Path to snapshot file (.tar.gz)
     * @param verify_signature Whether to verify the snapshot signature (default: true)
     * @return true if restoration successful, false otherwise
     */
    bool restore_snapshot(
        const std::string& snapshot_path,
        bool verify_signature = true
    );

    /**
     * Verify the integrity of a snapshot without restoring it.
     * 
     * Checks:
     * - File exists and is readable
     * - Checksum matches manifest
     * - Signature is valid (if verify_signature is true)
     * - Manifest is well-formed
     * 
     * @param snapshot_path Path to snapshot file
     * @param verify_signature Whether to verify the signature
     * @return true if snapshot is valid, false otherwise
     */
    bool verify_snapshot(
        const std::string& snapshot_path,
        bool verify_signature = true
    );

    /**
     * Get the manifest from a snapshot file without extracting it.
     * 
     * @param snapshot_path Path to snapshot file
     * @return Manifest if successful, nullopt otherwise
     */
    std::optional<SnapshotManifest> get_manifest(const std::string& snapshot_path);

    /**
     * List all available snapshots in the snapshot directory.
     * 
     * @return Vector of snapshot file paths
     */
    std::vector<std::string> list_snapshots() const;

    /**
     * Delete a snapshot file.
     * 
     * @param snapshot_path Path to snapshot file
     * @return true if deletion successful, false otherwise
     */
    bool delete_snapshot(const std::string& snapshot_path);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace storage
} // namespace sarafu
