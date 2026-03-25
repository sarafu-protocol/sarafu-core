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
#include <random>
#include <unordered_set>
#include <system_error>

// For JSON parsing (simple implementation)
#include <map>

#include <rocksdb/db.h>
#include <rocksdb/utilities/checkpoint.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

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

bool is_path_within(const fs::path& base, const fs::path& target) {
    auto base_canonical = fs::weakly_canonical(base);
    auto target_canonical = fs::weakly_canonical(target);
    auto base_str = base_canonical.string();
    auto target_str = target_canonical.string();
    if (base_str.back() != fs::path::preferred_separator) {
        base_str.push_back(fs::path::preferred_separator);
    }
    return target_str.rfind(base_str, 0) == 0;
}

std::string random_suffix() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    std::ostringstream oss;
    oss << std::hex << dist(gen);
    return oss.str();
}

std::optional<fs::path> create_temp_dir(const fs::path& base_dir, const std::string& prefix) {
    for (int i = 0; i < 5; ++i) {
        auto candidate = base_dir / (prefix + "-" + random_suffix());
        std::error_code ec;
        if (fs::create_directories(candidate, ec)) {
            return candidate;
        }
    }
    return std::nullopt;
}

#ifndef _WIN32
bool run_command(const std::vector<std::string>& args) {
    if (args.empty()) {
        return false;
    }
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv.data());
        _exit(127);
    }
    if (pid < 0) {
        return false;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

std::optional<std::string> run_command_capture(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::nullopt;
    }
    int pipe_fd[2];
    if (pipe(pipe_fd) != 0) {
        return std::nullopt;
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);
        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        _exit(127);
    }

    if (pid < 0) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return std::nullopt;
    }

    close(pipe_fd[1]);
    std::string output;
    char buffer[4096];
    ssize_t read_bytes = 0;
    while ((read_bytes = read(pipe_fd[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, buffer + read_bytes);
    }
    close(pipe_fd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return std::nullopt;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return std::nullopt;
    }

    return output;
}
#endif

bool is_safe_archive_entry(const std::string& entry) {
    if (entry.empty()) {
        return false;
    }
    if (entry[0] == '/') {
        return false;
    }
    if (entry.find("..") != std::string::npos) {
        return false;
    }
    return true;
}

bool is_allowed_archive_entry(const std::string& entry) {
    if (!is_safe_archive_entry(entry)) {
        return false;
    }
    if (entry == "manifest.json" || entry == "validator_state.json") {
        return true;
    }
    if (entry == "rocksdb" || entry == "rocksdb/") {
        return true;
    }
    if (entry.rfind("rocksdb/", 0) == 0) {
        return true;
    }
    return false;
}

std::optional<std::vector<std::string>> list_archive_entries(const std::string& snapshot_path) {
#ifdef _WIN32
    (void)snapshot_path;
    return std::nullopt;
#else
    auto output = run_command_capture({"tar", "-tzf", snapshot_path});
    if (!output) {
        return std::nullopt;
    }
    std::vector<std::string> entries;
    std::istringstream iss(*output);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            entries.push_back(line);
        }
    }
    return entries;
#endif
}

bool extract_archive(const std::string& snapshot_path, const fs::path& extract_dir) {
#ifdef _WIN32
    (void)snapshot_path;
    (void)extract_dir;
    return false;
#else
    return run_command({
        "tar",
        "-xzf",
        snapshot_path,
        "-C",
        extract_dir.string(),
        "--no-same-owner",
        "--no-same-permissions"
    });
#endif
}

bool create_archive(const fs::path& source_dir, const fs::path& output_path) {
#ifdef _WIN32
    (void)source_dir;
    (void)output_path;
    return false;
#else
    return run_command({
        "tar",
        "-czf",
        output_path.string(),
        "-C",
        source_dir.string(),
        "."
    });
#endif
}

std::optional<fs::path> extract_snapshot_to_temp(const std::string& snapshot_path,
                                                 const fs::path& base_dir) {
    auto entries_opt = list_archive_entries(snapshot_path);
    if (!entries_opt) {
        return std::nullopt;
    }
    for (const auto& entry : *entries_opt) {
        if (!is_allowed_archive_entry(entry)) {
            return std::nullopt;
        }
    }

    auto extract_dir_opt = create_temp_dir(base_dir, "snapshot-extract");
    if (!extract_dir_opt) {
        return std::nullopt;
    }

    if (!extract_archive(snapshot_path, *extract_dir_opt)) {
        fs::remove_all(*extract_dir_opt);
        return std::nullopt;
    }

    return extract_dir_opt;
}

bool create_rocksdb_checkpoint(const std::string& db_path, const std::string& checkpoint_dir,
                               std::string& error) {
    rocksdb::Options options;
    options.create_if_missing = false;
    rocksdb::DB* db = nullptr;
    auto status = rocksdb::DB::Open(options, db_path, &db);
    if (!status.ok()) {
        error = status.ToString();
        return false;
    }
    std::unique_ptr<rocksdb::DB> db_ptr(db);
    rocksdb::Checkpoint* checkpoint = nullptr;
    status = rocksdb::Checkpoint::Create(db_ptr.get(), &checkpoint);
    if (!status.ok()) {
        error = status.ToString();
        return false;
    }
    std::unique_ptr<rocksdb::Checkpoint> checkpoint_ptr(checkpoint);
    status = checkpoint_ptr->CreateCheckpoint(checkpoint_dir);
    if (!status.ok()) {
        error = status.ToString();
        return false;
    }
    return true;
}

std::string compute_payload_checksum(const fs::path& payload_dir) {
    std::vector<std::pair<std::string, std::string>> entries;
    for (const auto& entry : fs::recursive_directory_iterator(payload_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        auto rel_path = fs::relative(entry.path(), payload_dir).string();
        if (rel_path == "manifest.json") {
            continue;
        }
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file) {
            continue;
        }
        std::vector<uint8_t> data(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        auto hash = crypto::Blake3Hash::hash(data).to_hex();
        entries.emplace_back(rel_path, hash);
    }
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    std::string combined;
    for (const auto& entry : entries) {
        combined.append(entry.first);
        combined.push_back('\n');
        combined.append(entry.second);
        combined.push_back('\n');
    }
    return crypto::Blake3Hash::hash(combined).to_hex();
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
        fs::path snapshot_dir_path = fs::path(snapshot_dir_);
        std::error_code dir_ec;
        fs::create_directories(snapshot_dir_path, dir_ec);

        if (snapshot_path.empty()) {
            std::string short_hash = block_hash.substr(0, 8);
            snapshot_path = (snapshot_dir_path /
                ("snapshot-" + std::to_string(block_height) + "-" + short_hash + ".tar.gz")).string();
        }

        fs::path snapshot_path_fs(snapshot_path);
        auto snapshot_str = snapshot_path_fs.string();
        if (snapshot_str.size() < 7 || snapshot_str.substr(snapshot_str.size() - 7) != ".tar.gz") {
            logging::Logger::instance().error("Snapshot", "Snapshot path must end with .tar.gz");
            return "";
        }

        if (!is_path_within(snapshot_dir_path, snapshot_path_fs)) {
            logging::Logger::instance().error("Snapshot", "Snapshot path must be within snapshot directory");
            return "";
        }

        if (fs::exists(snapshot_path_fs)) {
            logging::Logger::instance().error("Snapshot", "Snapshot file already exists: " + snapshot_path);
            return "";
        }

        // Create temporary directory for snapshot contents
        auto temp_dir_opt = create_temp_dir(snapshot_dir_path, "snapshot-build");
        if (!temp_dir_opt) {
            logging::Logger::instance().error("Snapshot", "Failed to create temp directory");
            return "";
        }
        fs::path temp_dir = *temp_dir_opt;

        // Step 1: Create RocksDB checkpoint
        std::string checkpoint_dir = (temp_dir / "rocksdb").string();
        try {
            fs::create_directories(checkpoint_dir);
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to create checkpoint dir: " + std::string(e.what()));
            fs::remove_all(temp_dir);
            return "";
        }

        std::string checkpoint_error;
        if (!create_rocksdb_checkpoint(db_path_, checkpoint_dir, checkpoint_error)) {
            logging::Logger::instance().error("Snapshot", "Failed to create RocksDB checkpoint: " + checkpoint_error);
            fs::remove_all(temp_dir);
            return "";
        }

        // Step 2: Export validator state
        std::string validator_state_path = (temp_dir / "validator_state.json").string();
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
        manifest.checksum = compute_payload_checksum(temp_dir);
        manifest.schema_version = 1; // Current schema version
        manifest.node_version = sarafu::VERSION;

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
        std::string manifest_path = (temp_dir / "manifest.json").string();
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
            fs::path snapshot_path_fs(snapshot_path);
            for (int attempt = 0; attempt < 3; ++attempt) {
                std::ofstream manifest_file(manifest_path);
                manifest_file << manifest.to_json();
                manifest_file.close();

                if (!create_archive(temp_dir, snapshot_path_fs)) {
                    throw std::runtime_error("tar failed");
                }

                uint64_t new_size = fs::file_size(snapshot_path_fs);
                if (manifest.file_size == new_size) {
                    break;
                }
                manifest.file_size = new_size;
            }
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

        // Step 2: Extract snapshot safely
        auto extract_dir_opt = extract_snapshot_to_temp(snapshot_path, fs::path(snapshot_dir_));
        if (!extract_dir_opt) {
            logging::Logger::instance().error("Snapshot", "Failed to extract snapshot");
            return false;
        }
        fs::path extract_dir = *extract_dir_opt;

        // Step 3: Read manifest
        fs::path manifest_path = extract_dir / "manifest.json";
        if (!fs::exists(manifest_path)) {
            logging::Logger::instance().error("Snapshot", "Manifest not found in snapshot");
            fs::remove_all(extract_dir);
            return false;
        }
        std::ifstream manifest_file(manifest_path);
        std::stringstream manifest_buffer;
        manifest_buffer << manifest_file.rdbuf();
        auto manifest_opt = SnapshotManifest::from_json(manifest_buffer.str());
        if (!manifest_opt) {
            logging::Logger::instance().error("Snapshot", "Failed to parse snapshot manifest");
            fs::remove_all(extract_dir);
            return false;
        }
        SnapshotManifest manifest = *manifest_opt;

        // Step 4: Verify checksum
        std::string actual_checksum = compute_payload_checksum(extract_dir);
        if (actual_checksum != manifest.checksum) {
            logging::Logger::instance().error("Snapshot", "Snapshot payload checksum mismatch");
            fs::remove_all(extract_dir);
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

        // Step 5: Restore RocksDB checkpoint
        fs::path checkpoint_dir = extract_dir / "rocksdb";
        if (!fs::exists(checkpoint_dir)) {
            logging::Logger::instance().error("Snapshot", "RocksDB checkpoint missing in snapshot");
            fs::remove_all(extract_dir);
            return false;
        }

        try {
            if (fs::exists(db_path_)) {
                auto backup_path = fs::path(db_path_).string() + ".bak." + std::to_string(std::time(nullptr));
                fs::rename(db_path_, backup_path);
                logging::Logger::instance().info("Snapshot", "Existing database backed up to " + backup_path);
            }
            fs::create_directories(db_path_);
            fs::copy(checkpoint_dir, db_path_, fs::copy_options::recursive);
        } catch (const std::exception& e) {
            logging::Logger::instance().error("Snapshot", "Failed to restore RocksDB: " + std::string(e.what()));
            fs::remove_all(extract_dir);
            return false;
        }

        // Step 6: Import validator state
        fs::path validator_state = extract_dir / "validator_state.json";
        if (!fs::exists(validator_state)) {
            logging::Logger::instance().warn("Snapshot", "Validator state not found; skipping import");
        }

        // Step 7: Verify state root (caller responsibility)
        logging::Logger::instance().info("Snapshot", "State root verification should be performed by caller");

        fs::remove_all(extract_dir);
        logging::Logger::instance().info("Snapshot", "Snapshot restoration completed");
        return true;
    }

    bool verify_snapshot(const std::string& snapshot_path, bool verify_signature) {
        // Check file exists
        if (!fs::exists(snapshot_path)) {
            logging::Logger::instance().error("Snapshot", "Snapshot file not found: " + snapshot_path);
            return false;
        }

        // Extract snapshot safely
        auto extract_dir_opt = extract_snapshot_to_temp(snapshot_path, fs::path(snapshot_dir_));
        if (!extract_dir_opt) {
            logging::Logger::instance().error("Snapshot", "Failed to extract snapshot");
            return false;
        }
        fs::path extract_dir = *extract_dir_opt;

        // Get manifest
        fs::path manifest_path = extract_dir / "manifest.json";
        if (!fs::exists(manifest_path)) {
            logging::Logger::instance().error("Snapshot", "Manifest not found in snapshot");
            fs::remove_all(extract_dir);
            return false;
        }
        std::ifstream manifest_file(manifest_path);
        std::stringstream manifest_buffer;
        manifest_buffer << manifest_file.rdbuf();
        auto manifest_opt = SnapshotManifest::from_json(manifest_buffer.str());
        if (!manifest_opt) {
            logging::Logger::instance().error("Snapshot", "Failed to parse snapshot manifest");
            fs::remove_all(extract_dir);
            return false;
        }
        SnapshotManifest manifest = *manifest_opt;

        // Verify checksum
        std::string actual_checksum = compute_payload_checksum(extract_dir);
        if (actual_checksum != manifest.checksum) {
            logging::Logger::instance().error("Snapshot", "Snapshot payload checksum mismatch");
            fs::remove_all(extract_dir);
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

        fs::remove_all(extract_dir);
        logging::Logger::instance().info("Snapshot", "Snapshot verification passed");
        return true;
    }

    std::optional<SnapshotManifest> get_manifest_impl(const std::string& snapshot_path) {
        if (!fs::exists(snapshot_path)) {
            return std::nullopt;
        }
        auto extract_dir_opt = extract_snapshot_to_temp(snapshot_path, fs::path(snapshot_dir_));
        if (!extract_dir_opt) {
            return std::nullopt;
        }

        fs::path manifest_path = *extract_dir_opt / "manifest.json";
        if (!fs::exists(manifest_path)) {
            fs::remove_all(*extract_dir_opt);
            return std::nullopt;
        }

        std::ifstream file(manifest_path);
        std::stringstream buffer;
        buffer << file.rdbuf();
        auto manifest = SnapshotManifest::from_json(buffer.str());
        fs::remove_all(*extract_dir_opt);
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
