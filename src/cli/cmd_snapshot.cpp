// Snapshot management command implementation

#include "sarafu/storage/snapshot.h"
#include "sarafu/crypto/ed25519.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <iomanip>

namespace fs = std::filesystem;

void print_snapshot_usage() {
    std::cout << "Usage: sar snapshot <subcommand> [options]\n\n";
    std::cout << "Manage blockchain state snapshots\n\n";
    std::cout << "Subcommands:\n";
    std::cout << "  create       Create a new snapshot\n";
    std::cout << "  restore      Restore from a snapshot\n";
    std::cout << "  verify       Verify snapshot integrity\n";
    std::cout << "  list         List available snapshots\n";
    std::cout << "  delete       Delete a snapshot\n";
    std::cout << "  info         Show snapshot information\n\n";
    std::cout << "Run 'sar snapshot <subcommand> --help' for more information on a subcommand.\n";
}

void print_create_usage() {
    std::cout << "Usage: sar snapshot create [options]\n\n";
    std::cout << "Create a new blockchain state snapshot\n\n";
    std::cout << "Options:\n";
    std::cout << "  --db-path <path>        Path to RocksDB database (default: ~/.sarafu/data)\n";
    std::cout << "  --snapshot-dir <path>   Directory to store snapshots (default: ~/.sarafu/snapshots)\n";
    std::cout << "  --height <n>            Block height for snapshot (required)\n";
    std::cout << "  --block-hash <hash>     Block hash at height (required)\n";
    std::cout << "  --state-root <hash>     State root hash (required)\n";
    std::cout << "  --signing-key <path>    Path to Ed25519 signing key (required)\n";
    std::cout << "  --output <path>         Custom output path (optional)\n";
    std::cout << "  --help                  Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sar snapshot create --height 12345 --block-hash 0xabc... --state-root 0xdef... --signing-key ./key.bin\n";
}

void print_restore_usage() {
    std::cout << "Usage: sar snapshot restore [options]\n\n";
    std::cout << "Restore blockchain state from a snapshot\n\n";
    std::cout << "Options:\n";
    std::cout << "  --db-path <path>        Path to RocksDB database (default: ~/.sarafu/data)\n";
    std::cout << "  --snapshot <path>       Path to snapshot file (required)\n";
    std::cout << "  --no-verify-signature   Skip signature verification\n";
    std::cout << "  --help                  Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sar snapshot restore --snapshot ./snapshots/snapshot-12345-abc.tar.gz\n";
}

void print_verify_usage() {
    std::cout << "Usage: sar snapshot verify [options]\n\n";
    std::cout << "Verify snapshot integrity without restoring\n\n";
    std::cout << "Options:\n";
    std::cout << "  --snapshot <path>       Path to snapshot file (required)\n";
    std::cout << "  --no-verify-signature   Skip signature verification\n";
    std::cout << "  --help                  Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sar snapshot verify --snapshot ./snapshots/snapshot-12345-abc.tar.gz\n";
}

void print_list_usage() {
    std::cout << "Usage: sar snapshot list [options]\n\n";
    std::cout << "List available snapshots\n\n";
    std::cout << "Options:\n";
    std::cout << "  --snapshot-dir <path>   Directory containing snapshots (default: ~/.sarafu/snapshots)\n";
    std::cout << "  --help                  Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sar snapshot list\n";
}

void print_delete_usage() {
    std::cout << "Usage: sar snapshot delete [options]\n\n";
    std::cout << "Delete a snapshot file\n\n";
    std::cout << "Options:\n";
    std::cout << "  --snapshot <path>       Path to snapshot file (required)\n";
    std::cout << "  --help                  Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sar snapshot delete --snapshot ./snapshots/snapshot-12345-abc.tar.gz\n";
}

void print_info_usage() {
    std::cout << "Usage: sar snapshot info [options]\n\n";
    std::cout << "Show snapshot information\n\n";
    std::cout << "Options:\n";
    std::cout << "  --snapshot <path>       Path to snapshot file (required)\n";
    std::cout << "  --help                  Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sar snapshot info --snapshot ./snapshots/snapshot-12345-abc.tar.gz\n";
}

std::string get_home_dir() {
    const char* home = std::getenv("HOME");
    if (home) return std::string(home);
    return ".";
}

std::vector<uint8_t> read_signing_key(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open signing key file: " << path << "\n";
        return {};
    }

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> key(size);
    file.read(reinterpret_cast<char*>(key.data()), size);
    file.close();

    return key;
}

int cmd_snapshot_create(int argc, char* argv[]) {
    std::string db_path = get_home_dir() + "/.sarafu/data";
    std::string snapshot_dir = get_home_dir() + "/.sarafu/snapshots";
    uint64_t height = 0;
    std::string block_hash;
    std::string state_root;
    std::string signing_key_path;
    std::string output_path;

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_create_usage();
            return 0;
        } else if (strcmp(argv[i], "--db-path") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (strcmp(argv[i], "--snapshot-dir") == 0 && i + 1 < argc) {
            snapshot_dir = argv[++i];
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            height = std::stoull(argv[++i]);
        } else if (strcmp(argv[i], "--block-hash") == 0 && i + 1 < argc) {
            block_hash = argv[++i];
        } else if (strcmp(argv[i], "--state-root") == 0 && i + 1 < argc) {
            state_root = argv[++i];
        } else if (strcmp(argv[i], "--signing-key") == 0 && i + 1 < argc) {
            signing_key_path = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_create_usage();
            return 1;
        }
    }

    // Validate required arguments
    if (height == 0) {
        std::cerr << "Error: --height is required\n";
        print_create_usage();
        return 1;
    }
    if (block_hash.empty()) {
        std::cerr << "Error: --block-hash is required\n";
        print_create_usage();
        return 1;
    }
    if (state_root.empty()) {
        std::cerr << "Error: --state-root is required\n";
        print_create_usage();
        return 1;
    }
    if (signing_key_path.empty()) {
        std::cerr << "Error: --signing-key is required\n";
        print_create_usage();
        return 1;
    }

    // Read signing key
    auto signing_key = read_signing_key(signing_key_path);
    if (signing_key.empty()) {
        return 1;
    }

    std::cout << "========================================\n";
    std::cout << "Creating Snapshot\n";
    std::cout << "========================================\n";
    std::cout << "Block height: " << height << "\n";
    std::cout << "Block hash:   " << block_hash << "\n";
    std::cout << "State root:   " << state_root << "\n";
    std::cout << "Database:     " << db_path << "\n";
    std::cout << "Output dir:   " << snapshot_dir << "\n";
    std::cout << "\n";

    // Create snapshot
    sarafu::storage::SnapshotManager manager(db_path, snapshot_dir);
    std::string snapshot_path = manager.create_snapshot(
        height, block_hash, state_root, signing_key, output_path
    );

    if (snapshot_path.empty()) {
        std::cerr << "\n✗ Snapshot creation failed\n";
        return 1;
    }

    std::cout << "========================================\n";
    std::cout << "✓ Snapshot created successfully!\n";
    std::cout << "========================================\n";
    std::cout << "Location: " << snapshot_path << "\n";
    std::cout << "\n⚠️  IMPORTANT:\n";
    std::cout << "  • Verify the snapshot before using it for recovery\n";
    std::cout << "  • Store snapshots in multiple secure locations\n";
    std::cout << "  • Test restoration procedure regularly\n\n";

    return 0;
}

int cmd_snapshot_restore(int argc, char* argv[]) {
    std::string db_path = get_home_dir() + "/.sarafu/data";
    std::string snapshot_path;
    bool verify_signature = true;

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_restore_usage();
            return 0;
        } else if (strcmp(argv[i], "--db-path") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (strcmp(argv[i], "--snapshot") == 0 && i + 1 < argc) {
            snapshot_path = argv[++i];
        } else if (strcmp(argv[i], "--no-verify-signature") == 0) {
            verify_signature = false;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_restore_usage();
            return 1;
        }
    }

    // Validate required arguments
    if (snapshot_path.empty()) {
        std::cerr << "Error: --snapshot is required\n";
        print_restore_usage();
        return 1;
    }

    std::cout << "========================================\n";
    std::cout << "Restoring from Snapshot\n";
    std::cout << "========================================\n";
    std::cout << "Snapshot:     " << snapshot_path << "\n";
    std::cout << "Database:     " << db_path << "\n";
    std::cout << "Verify sig:   " << (verify_signature ? "yes" : "no") << "\n";
    std::cout << "\n";

    // Get snapshot directory from snapshot path
    std::string snapshot_dir = fs::path(snapshot_path).parent_path().string();
    if (snapshot_dir.empty()) snapshot_dir = ".";

    // Restore snapshot
    sarafu::storage::SnapshotManager manager(db_path, snapshot_dir);
    bool success = manager.restore_snapshot(snapshot_path, verify_signature);

    if (!success) {
        std::cerr << "\n✗ Snapshot restoration failed\n";
        return 1;
    }

    std::cout << "========================================\n";
    std::cout << "✓ Snapshot restored successfully!\n";
    std::cout << "========================================\n";
    std::cout << "\n⚠️  NEXT STEPS:\n";
    std::cout << "  • Verify state integrity before resuming operations\n";
    std::cout << "  • Check that block height matches expected value\n";
    std::cout << "  • Restart the node to resume consensus\n\n";

    return 0;
}

int cmd_snapshot_verify(int argc, char* argv[]) {
    std::string snapshot_path;
    bool verify_signature = true;

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_verify_usage();
            return 0;
        } else if (strcmp(argv[i], "--snapshot") == 0 && i + 1 < argc) {
            snapshot_path = argv[++i];
        } else if (strcmp(argv[i], "--no-verify-signature") == 0) {
            verify_signature = false;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_verify_usage();
            return 1;
        }
    }

    // Validate required arguments
    if (snapshot_path.empty()) {
        std::cerr << "Error: --snapshot is required\n";
        print_verify_usage();
        return 1;
    }

    std::cout << "Verifying snapshot: " << snapshot_path << "\n";

    // Get snapshot directory from snapshot path
    std::string snapshot_dir = fs::path(snapshot_path).parent_path().string();
    if (snapshot_dir.empty()) snapshot_dir = ".";

    // Verify snapshot
    sarafu::storage::SnapshotManager manager("", snapshot_dir);
    bool valid = manager.verify_snapshot(snapshot_path, verify_signature);

    if (!valid) {
        std::cerr << "✗ Snapshot verification failed\n";
        return 1;
    }

    std::cout << "✓ Snapshot verification passed\n";
    return 0;
}

int cmd_snapshot_list(int argc, char* argv[]) {
    std::string snapshot_dir = get_home_dir() + "/.sarafu/snapshots";

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_list_usage();
            return 0;
        } else if (strcmp(argv[i], "--snapshot-dir") == 0 && i + 1 < argc) {
            snapshot_dir = argv[++i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_list_usage();
            return 1;
        }
    }

    std::cout << "Listing snapshots in: " << snapshot_dir << "\n\n";

    // List snapshots
    sarafu::storage::SnapshotManager manager("", snapshot_dir);
    auto snapshots = manager.list_snapshots();

    if (snapshots.empty()) {
        std::cout << "No snapshots found.\n";
        return 0;
    }

    std::cout << "Found " << snapshots.size() << " snapshot(s):\n\n";
    for (const auto& snapshot : snapshots) {
        std::cout << "  " << fs::path(snapshot).filename().string() << "\n";
        
        // Try to get manifest info
        auto manifest_opt = manager.get_manifest(snapshot);
        if (manifest_opt) {
            auto& manifest = *manifest_opt;
            std::cout << "    Height:  " << manifest.block_height << "\n";
            std::cout << "    Hash:    " << manifest.block_hash.substr(0, 16) << "...\n";
            std::cout << "    Size:    " << manifest.file_size << " bytes\n";
            std::cout << "    Version: " << manifest.node_version << "\n";
        }
        std::cout << "\n";
    }

    return 0;
}

int cmd_snapshot_delete(int argc, char* argv[]) {
    std::string snapshot_path;

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_delete_usage();
            return 0;
        } else if (strcmp(argv[i], "--snapshot") == 0 && i + 1 < argc) {
            snapshot_path = argv[++i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_delete_usage();
            return 1;
        }
    }

    // Validate required arguments
    if (snapshot_path.empty()) {
        std::cerr << "Error: --snapshot is required\n";
        print_delete_usage();
        return 1;
    }

    std::cout << "Deleting snapshot: " << snapshot_path << "\n";

    // Get snapshot directory from snapshot path
    std::string snapshot_dir = fs::path(snapshot_path).parent_path().string();
    if (snapshot_dir.empty()) snapshot_dir = ".";

    // Delete snapshot
    sarafu::storage::SnapshotManager manager("", snapshot_dir);
    bool success = manager.delete_snapshot(snapshot_path);

    if (!success) {
        std::cerr << "✗ Failed to delete snapshot\n";
        return 1;
    }

    std::cout << "✓ Snapshot deleted successfully\n";
    return 0;
}

int cmd_snapshot_info(int argc, char* argv[]) {
    std::string snapshot_path;

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_info_usage();
            return 0;
        } else if (strcmp(argv[i], "--snapshot") == 0 && i + 1 < argc) {
            snapshot_path = argv[++i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_info_usage();
            return 1;
        }
    }

    // Validate required arguments
    if (snapshot_path.empty()) {
        std::cerr << "Error: --snapshot is required\n";
        print_info_usage();
        return 1;
    }

    // Get snapshot directory from snapshot path
    std::string snapshot_dir = fs::path(snapshot_path).parent_path().string();
    if (snapshot_dir.empty()) snapshot_dir = ".";

    // Get manifest
    sarafu::storage::SnapshotManager manager("", snapshot_dir);
    auto manifest_opt = manager.get_manifest(snapshot_path);

    if (!manifest_opt) {
        std::cerr << "✗ Failed to read snapshot manifest\n";
        return 1;
    }

    auto& manifest = *manifest_opt;

    std::cout << "========================================\n";
    std::cout << "Snapshot Information\n";
    std::cout << "========================================\n";
    std::cout << "File:          " << fs::path(snapshot_path).filename().string() << "\n";
    std::cout << "Block height:  " << manifest.block_height << "\n";
    std::cout << "Block hash:    " << manifest.block_hash << "\n";
    std::cout << "State root:    " << manifest.state_root << "\n";
    std::cout << "Timestamp:     " << manifest.timestamp << "\n";
    std::cout << "File size:     " << manifest.file_size << " bytes\n";
    std::cout << "Checksum:      " << manifest.checksum << "\n";
    std::cout << "Schema ver:    " << manifest.schema_version << "\n";
    std::cout << "Node version:  " << manifest.node_version << "\n";
    std::cout << "Creator key:   " << manifest.creator_pubkey.substr(0, 16) << "...\n";
    std::cout << "Signature:     " << manifest.signature.substr(0, 16) << "...\n";
    std::cout << "========================================\n";

    return 0;
}

int cmd_snapshot(int argc, char* argv[]) {
    if (argc < 1) {
        print_snapshot_usage();
        return 1;
    }

    std::string subcommand = argv[0];

    if (subcommand == "create") {
        return cmd_snapshot_create(argc - 1, argv + 1);
    } else if (subcommand == "restore") {
        return cmd_snapshot_restore(argc - 1, argv + 1);
    } else if (subcommand == "verify") {
        return cmd_snapshot_verify(argc - 1, argv + 1);
    } else if (subcommand == "list") {
        return cmd_snapshot_list(argc - 1, argv + 1);
    } else if (subcommand == "delete") {
        return cmd_snapshot_delete(argc - 1, argv + 1);
    } else if (subcommand == "info") {
        return cmd_snapshot_info(argc - 1, argv + 1);
    } else if (subcommand == "--help" || subcommand == "-h") {
        print_snapshot_usage();
        return 0;
    } else {
        std::cerr << "Unknown subcommand: " << subcommand << "\n";
        print_snapshot_usage();
        return 1;
    }
}
