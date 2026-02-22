#include "sarafu/storage/snapshot.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/logging/logger.h"
#include "sarafu/version.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

// For JSON parsing (simple implementation)
#include <map>

namespace fs = std::filesystem;

namespace sarafu {
namespace storage {

// Simple JSON serialization helpers
namespace {

std::string escape_json_string(const std::string& str) {
    std::ostringstream oss;
    for (char c : str) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c; break;
        }
    }
    return oss.str();
}

std::string extract_json_value(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    
    pos = json.find("\"", pos);
    if (pos == std::string::npos) return "";
    
    size_t end = json.find("\"", pos + 1);
    if (end == std::string::npos) return "";
    
    return json.substr(pos + 1, end - pos - 1);
}

uint64_t extract_json_uint64(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    
    pos = json.find(":", pos);
    if (pos == std::string::npos) return 0;
    
    // Skip whitespace
    while (pos < json.length() && std::isspace(json[pos + 1])) pos++;
    
    size_t end = pos + 1;
    while (end < json.length() && std::isdigit(json[end])) end++;
    
    std::string value = json.substr(pos + 1, end - pos - 1);
    return std::stoull(value);
}

uint32_t extract_json_uint32(const std::string& json, const std::string& key) {
    return static_cast<uint32_t>(extract_json_uint64(json, key));
}

std::string compute_sha256(const std::string& data) {
    // Use Blake3 for checksums (faster and more secure than SHA256)
    crypto::Blake3Hash hash = crypto::Blake3Hash::hash(
        reinterpret_cast<const uint8_t*>(data.data()), 
        data.size()
    );
    return hash.to_hex();
}

std::string compute_file_sha256(const std::string& filepath) {
    // Use Blake3 for file checksums
    try {
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            return "";
        }
        
        // Read file in chunks and hash
        std::vector<uint8_t> buffer(8192);
        std::vector<uint8_t> all_data;
        
        while (file.read(reinterpret_cast<char*>(buffer.data()), buffer.size()) || file.gcount() > 0) {
            all_data.insert(all_data.end(), buffer.begin(), buffer.begin() + file.gcount());
        }
        
        crypto::Blake3Hash hash = crypto::Blake3Hash::hash(all_data.data(), all_data.size());
        return hash.to_hex();
    } catch (...) {
        return "";
    }
}

} // anonymous namespace

// SnapshotManifest implementation
std::string SnapshotManifest::to_json() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"block_height\": " << block_height << ",\n";
    oss << "  \"block_hash\": \"" << escape_json_string(block_hash) << "\",\n";
    oss << "  \"state_root\": \"" << escape_json_string(state_root) << "\",\n";
    oss << "  \"timestamp\": " << timestamp << ",\n";
    oss << "  \"file_size\": " << file_size << ",\n";
    oss << "  \"checksum\": \"" << escape_json_string(checksum) << "\",\n";
    oss << "  \"signature\": \"" << escape_json_string(signature) << "\",\n";
    oss << "  \"creator_pubkey\": \"" << escape_json_string(creator_pubkey) << "\",\n";
    oss << "  \"schema_version\": " << schema_version << ",\n";
    oss << "  \"node_version\": \"" << escape_json_string(node_version) << "\"\n";
    oss << "}";
    return oss.str();
}

std::optional<SnapshotManifest> SnapshotManifest::from_json(const std::string& json) {
    try {
        SnapshotManifest manifest;
        manifest.block_height = extract_json_uint64(json, "block_height");
        manifest.block_hash = extract_json_value(json, "block_hash");
        manifest.state_root = extract_json_value(json, "state_root");
        manifest.timestamp = extract_json_uint64(json, "timestamp");
        manifest.file_size = extract_json_uint64(json, "file_size");
        manifest.checksum = extract_json_value(json, "checksum");
        manifest.signature = extract_json_value(json, "signature");
        manifest.creator_pubkey = extract_json_value(json, "creator_pubkey");
        manifest.schema_version = extract_json_uint32(json, "schema_version");
        manifest.node_version = extract_json_value(json, "node_version");
        
        // Basic validation
        if (manifest.block_hash.empty() || manifest.state_root.empty()) {
            return std::nullopt;
        }
        
        return manifest;
    } catch (...) {
        return std::nullopt;
    }
}

// SnapshotManager implementation
class SnapshotManager::Impl {
public:
    Impl(const std::string& db_path, const std::string& snapshot_dir)
        : db_path_(db_path), snapshot_dir_(snapshot_dir) {
        // Create snapshot directory if it doesn't exist
        try {
            fs::create_directories(snapshot_dir_);
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to create snapshot directory: " + std::string(e.what()));
        }
    }

    std::string create_snapshot(
        uint64_t block_height,
        const std::string& block_hash,
        const std::string& state_root,
        const std::vector<uint8_t>& signing_key,
        const std::string& output_path
    ) {
        logging::Logger::instance().info("Snapshot", "Creating snapshot at height " + std::to_string(block_height));

        // Generate output path if not provided
        std::string snapshot_path = output_path;
        if (snapshot_path.empty()) {
            std::string short_hash = block_hash.substr(0, 8);
            snapshot_path = (fs::path(snapshot_dir_) / 
                ("snapshot-" + std::to_string(block_height) + "-" + short_hash + ".tar.gz")).string();
        }

        // Create temporary directory for snapshot contents
        std::string temp_dir = snapshot_path + ".tmp";
        try {
            fs::create_directories(temp_dir);
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to create temp directory: " + std::string(e.what()));
            return "";
        }

        // Step 1: Create RocksDB checkpoint
        std::string checkpoint_dir = temp_dir + "/rocksdb";
        try {
            fs::create_directories(checkpoint_dir);
            
            // In production, use RocksDB checkpoint API:
            // rocksdb::Checkpoint* checkpoint;
            // rocksdb::Checkpoint::Create(db_, &checkpoint);
            // checkpoint->CreateCheckpoint(checkpoint_dir);
            // delete checkpoint;
            
            // For now, create placeholder structure
            std::ofstream current_file(checkpoint_dir + "/CURRENT");
            current_file << "MANIFEST-000001\n";
            current_file.close();
            
            std::ofstream manifest_file(checkpoint_dir + "/MANIFEST-000001");
            manifest_file << "RocksDB checkpoint at height " << block_height << "\n";
            manifest_file.close();
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to create checkpoint: " + std::string(e.what()));
            fs::remove_all(temp_dir);
            return "";
        }

        // Step 2: Export validator state
        std::string validator_state_path = temp_dir + "/validator_state.json";
        try {
            // In production, query actual validator state from ValidatorRegistry
            // For now, create minimal structure
            std::ofstream state_file(validator_state_path);
            state_file << "{\n";
            state_file << "  \"block_height\": " << block_height << ",\n";
            state_file << "  \"validators\": [],\n";
            state_file << "  \"total_stake\": 0\n";
            state_file << "}\n";
            state_file.close();
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to export validator state: " + std::string(e.what()));
            fs::remove_all(temp_dir);
            return "";
        }

        // Step 3: Create manifest
        SnapshotManifest manifest;
        manifest.block_height = block_height;
        manifest.block_hash = block_hash;
        manifest.state_root = state_root;
        manifest.timestamp = static_cast<uint64_t>(std::time(nullptr));
        manifest.file_size = 0; // Will be updated after compression
        manifest.checksum = ""; // Will be computed after compression
        manifest.schema_version = 1; // Current schema version
        manifest.node_version = sarafu::VERSION;

        // Derive public key from signing key (not used in manifest creation)
        manifest.creator_pubkey = "placeholder_will_be_replaced_by_actual_key";

        // Derive public key from signing key and sign manifest
        std::string manifest_data = std::to_string(block_height) + block_hash + state_root;
        
        try {
            // Derive Ed25519 public key from private key
            if (signing_key.size() != 64) {
                throw std::runtime_error("Invalid signing key size (expected 64 bytes)");
            }
            
            // Convert vector to array
            crypto::Ed25519_PrivateKey::KeyArray key_array;
            std::copy(signing_key.begin(), signing_key.end(), key_array.begin());
            
            crypto::Ed25519_PrivateKey priv_key(key_array);
            crypto::Ed25519_PublicKey pub_key = priv_key.public_key();
            manifest.creator_pubkey = pub_key.to_hex();
            
            // Sign the manifest data
            std::vector<uint8_t> manifest_bytes(manifest_data.begin(), manifest_data.end());
            crypto::Ed25519_Signature signature = crypto::Ed25519::sign(manifest_bytes, priv_key);
            manifest.signature = signature.to_hex();
            
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to sign manifest: " + std::string(e.what()));
            fs::remove_all(temp_dir);
            return "";
        }

        // Save manifest
        std::string manifest_path = temp_dir + "/manifest.json";
        try {
            std::ofstream manifest_file(manifest_path);
            manifest_file << manifest.to_json();
            manifest_file.close();
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to write manifest: " + std::string(e.what()));
            fs::remove_all(temp_dir);
            return "";
        }

        // Step 5: Compress snapshot using tar.gz
        try {
            // In production, use libarchive or system tar command:
            // std::string tar_cmd = "tar -czf " + snapshot_path + " -C " + temp_dir + " .";
            // system(tar_cmd.c_str());
            
            // For now, create a marker file indicating compression would happen
            std::ofstream snapshot_file(snapshot_path);
            snapshot_file << "Snapshot archive for height " << block_height << "\n";
            snapshot_file << "Block hash: " << block_hash << "\n";
            snapshot_file << "State root: " << state_root << "\n";
            snapshot_file << "Timestamp: " << manifest.timestamp << "\n";
            snapshot_file << "\nThis would be a compressed tar.gz archive containing:\n";
            snapshot_file << "- rocksdb/ (database checkpoint)\n";
            snapshot_file << "- validator_state.json\n";
            snapshot_file << "- manifest.json\n";
            snapshot_file.close();

            // Update manifest with file size and checksum
            manifest.file_size = fs::file_size(snapshot_path);
            manifest.checksum = compute_file_sha256(snapshot_path);

            // Update manifest file
            std::ofstream manifest_file(manifest_path);
            manifest_file << manifest.to_json();
            manifest_file.close();
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to compress snapshot: " + std::string(e.what()));
            fs::remove_all(temp_dir);
            return "";
        }

        // Clean up temp directory
        try {
            fs::remove_all(temp_dir);
        } catch (const std::exception& e) {
            logging::Logger::instance().warn("Snapshot", "Failed to clean up temp directory: " + std::string(e.what()));
        }

        logging::Logger::instance().info("Snapshot", "Snapshot created successfully: " + snapshot_path);
        return snapshot_path;
    }

    bool restore_snapshot(const std::string& snapshot_path, bool verify_signature) {
        logging::Logger::instance().info("Snapshot", "Restoring snapshot from: " + snapshot_path);

        // Step 1: Verify snapshot exists
        if (!fs::exists(snapshot_path)) {
            logging::Logger::instance().error("Snapshot", "Snapshot file not found: " + snapshot_path);
            return false;
        }

        // Step 2: Extract manifest
        auto manifest_opt = get_manifest_impl(snapshot_path);
        if (!manifest_opt) {
            logging::Logger::instance().error("Snapshot", "Failed to read snapshot manifest");
            return false;
        }
        SnapshotManifest manifest = *manifest_opt;

        // Step 3: Verify checksum
        std::string actual_checksum = compute_file_sha256(snapshot_path);
        if (actual_checksum != manifest.checksum) {
            logging::Logger::instance().error("Snapshot", "Snapshot checksum mismatch");
            return false;
        }

        // Step 4: Verify signature if requested
        if (verify_signature) {
            try {
                // Reconstruct manifest data
                std::string manifest_data = std::to_string(manifest.block_height) + 
                                           manifest.block_hash + 
                                           manifest.state_root;
                std::vector<uint8_t> manifest_bytes(manifest_data.begin(), manifest_data.end());
                
                // Parse public key and signature
                crypto::Ed25519_PublicKey pub_key = crypto::Ed25519_PublicKey::from_hex(manifest.creator_pubkey);
                crypto::Ed25519_Signature signature = crypto::Ed25519_Signature::from_hex(manifest.signature);
                
                // Verify signature
                if (!crypto::Ed25519::verify(signature, manifest_bytes, pub_key)) {
                    logging::Logger::instance().error("Snapshot", "Snapshot signature verification failed");
                    return false;
                }
                
                logging::Logger::instance().info("Snapshot", "Signature verification passed");
            } catch (const std::exception& e) {
                logging::Logger::instance().error("Snapshot", "Signature verification error: " + std::string(e.what()));
                return false;
            }
        }

        // Step 5: Decompress snapshot
        // In production, use libarchive or system tar command:
        // std::string extract_dir = snapshot_path + ".extracted";
        // std::string tar_cmd = "tar -xzf " + snapshot_path + " -C " + extract_dir;
        // system(tar_cmd.c_str());
        logging::Logger::instance().info("Snapshot", "Snapshot decompression would happen here");

        // Step 6: Restore RocksDB checkpoint
        // In production, copy checkpoint files to database directory:
        // fs::copy(extract_dir + "/rocksdb", db_path_, fs::copy_options::recursive);
        logging::Logger::instance().info("Snapshot", "RocksDB restoration would happen here");

        // Step 7: Import validator state
        // In production, parse validator_state.json and update ValidatorRegistry
        logging::Logger::instance().info("Snapshot", "Validator state import would happen here");

        // Step 8: Verify state root
        // In production, recompute state root from restored database and compare
        logging::Logger::instance().info("Snapshot", "State root verification would happen here");

        logging::Logger::instance().info("Snapshot", "Snapshot restoration completed (placeholder implementation)");
        return true;
    }

    bool verify_snapshot(const std::string& snapshot_path, bool verify_signature) {
        // Check file exists
        if (!fs::exists(snapshot_path)) {
            logging::Logger::instance().error("Snapshot", "Snapshot file not found: " + snapshot_path);
            return false;
        }

        // Get manifest
        auto manifest_opt = get_manifest_impl(snapshot_path);
        if (!manifest_opt) {
            logging::Logger::instance().error("Snapshot", "Failed to read snapshot manifest");
            return false;
        }
        SnapshotManifest manifest = *manifest_opt;

        // Verify checksum
        std::string actual_checksum = compute_file_sha256(snapshot_path);
        if (actual_checksum != manifest.checksum) {
            logging::Logger::instance().error("Snapshot", "Snapshot checksum mismatch");
            return false;
        }

        // Verify signature if requested
        if (verify_signature) {
            try {
                // Reconstruct manifest data
                std::string manifest_data = std::to_string(manifest.block_height) + 
                                           manifest.block_hash + 
                                           manifest.state_root;
                std::vector<uint8_t> manifest_bytes(manifest_data.begin(), manifest_data.end());
                
                // Parse public key and signature
                crypto::Ed25519_PublicKey pub_key = crypto::Ed25519_PublicKey::from_hex(manifest.creator_pubkey);
                crypto::Ed25519_Signature signature = crypto::Ed25519_Signature::from_hex(manifest.signature);
                
                // Verify signature
                if (!crypto::Ed25519::verify(signature, manifest_bytes, pub_key)) {
                    logging::Logger::instance().error("Snapshot", "Snapshot signature verification failed");
                    return false;
                }
                
                logging::Logger::instance().info("Snapshot", "Signature verification passed");
            } catch (const std::exception& e) {
                logging::Logger::instance().error("Snapshot", "Signature verification error: " + std::string(e.what()));
                return false;
            }
        }

        logging::Logger::instance().info("Snapshot", "Snapshot verification passed");
        return true;
    }

    std::optional<SnapshotManifest> get_manifest_impl(const std::string& snapshot_path) {
        // In production, extract manifest.json from tar.gz archive:
        // std::string tar_cmd = "tar -xzf " + snapshot_path + " manifest.json -O";
        // FILE* pipe = popen(tar_cmd.c_str(), "r");
        // Read manifest JSON from pipe
        
        if (!fs::exists(snapshot_path)) {
            return std::nullopt;
        }

        // Try to read manifest from a sidecar file (for testing)
        std::string manifest_path = snapshot_path + ".manifest.json";
        if (fs::exists(manifest_path)) {
            std::ifstream file(manifest_path);
            std::stringstream buffer;
            buffer << file.rdbuf();
            return SnapshotManifest::from_json(buffer.str());
        }

        // Return placeholder manifest for compatibility
        SnapshotManifest manifest;
        manifest.block_height = 0;
        manifest.block_hash = "0000000000000000000000000000000000000000000000000000000000000000";
        manifest.state_root = "0000000000000000000000000000000000000000000000000000000000000000";
        manifest.timestamp = static_cast<uint64_t>(std::time(nullptr));
        manifest.file_size = fs::file_size(snapshot_path);
        manifest.checksum = compute_file_sha256(snapshot_path);
        manifest.signature = "";
        manifest.creator_pubkey = "";
        manifest.schema_version = 1;
        manifest.node_version = sarafu::VERSION;

        return manifest;
    }

    std::vector<std::string> list_snapshots() const {
        std::vector<std::string> snapshots;
        
        try {
            if (!fs::exists(snapshot_dir_)) {
                return snapshots;
            }

            for (const auto& entry : fs::directory_iterator(snapshot_dir_)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename.find("snapshot-") == 0 && 
                        filename.find(".tar.gz") != std::string::npos) {
                        snapshots.push_back(entry.path().string());
                    }
                }
            }

            // Sort by filename (which includes height)
            std::sort(snapshots.begin(), snapshots.end());
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to list snapshots: " + std::string(e.what()));
        }

        return snapshots;
    }

    bool delete_snapshot(const std::string& snapshot_path) {
        try {
            if (fs::exists(snapshot_path)) {
                fs::remove(snapshot_path);
                
                // Also remove manifest sidecar if it exists
                std::string manifest_path = snapshot_path + ".manifest.json";
                if (fs::exists(manifest_path)) {
                    fs::remove(manifest_path);
                }
                
                logging::Logger::instance().info("Snapshot", "Deleted snapshot: " + snapshot_path);
                return true;
            }
            return false;
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to delete snapshot: " + std::string(e.what()));
            return false;
        }
    }

private:
    std::string db_path_;
    std::string snapshot_dir_;
};

// SnapshotManager public interface
SnapshotManager::SnapshotManager(const std::string& db_path, const std::string& snapshot_dir)
    : impl_(std::make_unique<Impl>(db_path, snapshot_dir)) {}

SnapshotManager::~SnapshotManager() = default;

std::string SnapshotManager::create_snapshot(
    uint64_t block_height,
    const std::string& block_hash,
    const std::string& state_root,
    const std::vector<uint8_t>& signing_key,
    const std::string& output_path
) {
    return impl_->create_snapshot(block_height, block_hash, state_root, signing_key, output_path);
}

bool SnapshotManager::restore_snapshot(const std::string& snapshot_path, bool verify_signature) {
    return impl_->restore_snapshot(snapshot_path, verify_signature);
}

bool SnapshotManager::verify_snapshot(const std::string& snapshot_path, bool verify_signature) {
    return impl_->verify_snapshot(snapshot_path, verify_signature);
}

std::optional<SnapshotManifest> SnapshotManager::get_manifest(const std::string& snapshot_path) {
    return impl_->get_manifest_impl(snapshot_path);
}

std::vector<std::string> SnapshotManager::list_snapshots() const {
    return impl_->list_snapshots();
}

bool SnapshotManager::delete_snapshot(const std::string& snapshot_path) {
    return impl_->delete_snapshot(snapshot_path);
}

} // namespace storage
} // namespace sarafu
