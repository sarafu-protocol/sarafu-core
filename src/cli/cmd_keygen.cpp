// Key generation command implementation

#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/bls12_381.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <iomanip>

namespace fs = std::filesystem;

void print_keygen_usage() {
    std::cout << "Usage: sar keygen [options]\n\n";
    std::cout << "Generate validator keys (BLS12-381 consensus + Ed25519 withdrawal)\n\n";
    std::cout << "Options:\n";
    std::cout << "  --output-dir <path>    Output directory for keys (default: ./keys)\n";
    std::cout << "  --validator-id <id>    Validator ID number (default: 1)\n";
    std::cout << "  --count <n>            Generate keys for N validators (default: 1)\n";
    std::cout << "  --help                 Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sar keygen --validator-id 1 --output-dir ./docker/keys\n";
    std::cout << "  sar keygen --count 10 --output-dir ./docker/keys\n";
}

bool save_binary_key(const std::vector<uint8_t>& key_data, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open file for writing: " << path << "\n";
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(key_data.data()), key_data.size());
    file.close();
    
    // Set restrictive permissions (owner read/write only)
    fs::permissions(path, 
        fs::perms::owner_read | fs::perms::owner_write,
        fs::perm_options::replace);
    
    return true;
}

bool save_json_metadata(const std::string& type, 
                        const std::string& public_key_hex,
                        int validator_id,
                        const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open file for writing: " << path << "\n";
        return false;
    }
    
    file << "{\n";
    file << "  \"type\": \"" << type << "\",\n";
    file << "  \"public_key\": \"0x" << public_key_hex << "\",\n";
    file << "  \"validator_id\": " << validator_id << "\n";
    file << "}\n";
    
    file.close();
    
    fs::permissions(path, 
        fs::perms::owner_read | fs::perms::owner_write,
        fs::perm_options::replace);
    
    return true;
}

bool generate_validator_keys(int validator_id, const std::string& output_dir) {
    std::cout << "\nGenerating keys for validator-" << validator_id << "...\n";
    
    // Create output directory
    fs::path validator_dir = fs::path(output_dir) / ("validator-" + std::to_string(validator_id));
    try {
        fs::create_directories(validator_dir);
    } catch (const std::exception& e) {
        std::cerr << "Error: Failed to create directory: " << e.what() << "\n";
        return false;
    }
    
    // Generate Ed25519 withdrawal key
    std::cout << "  Generating Ed25519 withdrawal key...\n";
    auto ed25519_keypair = sarafu::crypto::Ed25519::generate_keypair();
    auto withdrawal_private = ed25519_keypair.second.serialize();
    auto withdrawal_public = ed25519_keypair.first.serialize();
    
    std::string withdrawal_bin_path = (validator_dir / "withdrawal_key.bin").string();
    std::string withdrawal_json_path = (validator_dir / "withdrawal_key.json").string();
    
    if (!save_binary_key(withdrawal_private, withdrawal_bin_path)) {
        return false;
    }
    
    // Convert public key to hex
    std::ostringstream hex_stream;
    for (uint8_t byte : withdrawal_public) {
        hex_stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::string withdrawal_pubkey_hex = hex_stream.str();
    
    if (!save_json_metadata("Ed25519", withdrawal_pubkey_hex, validator_id, withdrawal_json_path)) {
        return false;
    }
    
    std::cout << "    ✓ Saved to " << withdrawal_bin_path << "\n";
    std::cout << "    Public key: 0x" << withdrawal_pubkey_hex << "\n";
    
    // Generate BLS12-381 consensus key
    std::cout << "  Generating BLS12-381 consensus key...\n";
    auto bls_keypair = sarafu::crypto::BLS12_381::generate_keypair();
    auto consensus_private = bls_keypair.second.serialize();
    auto consensus_public = bls_keypair.first.serialize();
    
    std::string consensus_bin_path = (validator_dir / "consensus_key.bin").string();
    std::string consensus_json_path = (validator_dir / "consensus_key.json").string();
    
    if (!save_binary_key(consensus_private, consensus_bin_path)) {
        return false;
    }
    
    // Convert public key to hex
    hex_stream.str("");
    hex_stream.clear();
    for (uint8_t byte : consensus_public) {
        hex_stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::string consensus_pubkey_hex = hex_stream.str();
    
    if (!save_json_metadata("BLS12-381", consensus_pubkey_hex, validator_id, consensus_json_path)) {
        return false;
    }
    
    std::cout << "    ✓ Saved to " << consensus_bin_path << "\n";
    std::cout << "    Public key: 0x" << consensus_pubkey_hex << "\n";
    
    return true;
}

int cmd_keygen(int argc, char* argv[]) {
    std::string output_dir = "./keys";
    int validator_id = 1;
    int count = 1;
    
    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_keygen_usage();
            return 0;
        } else if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
        } else if (strcmp(argv[i], "--validator-id") == 0 && i + 1 < argc) {
            validator_id = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            count = std::stoi(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_keygen_usage();
            return 1;
        }
    }
    
    std::cout << "========================================\n";
    std::cout << "Sarafu Validator Key Generator\n";
    std::cout << "========================================\n";
    std::cout << "\nGenerating keys for " << count << " validator(s)\n";
    std::cout << "Output directory: " << output_dir << "\n";
    
    // Generate keys
    bool success = true;
    for (int i = 0; i < count; i++) {
        int vid = validator_id + i;
        if (!generate_validator_keys(vid, output_dir)) {
            success = false;
            break;
        }
    }
    
    if (success) {
        std::cout << "\n========================================\n";
        std::cout << "✓ All keys generated successfully!\n";
        std::cout << "========================================\n";
        std::cout << "\n⚠️  SECURITY WARNINGS:\n";
        std::cout << "  • Backup these keys securely in multiple locations\n";
        std::cout << "  • Never share private keys or commit to version control\n";
        std::cout << "  • Store keys encrypted in production environments\n\n";
        return 0;
    } else {
        std::cerr << "\n✗ Key generation failed\n";
        return 1;
    }
}
