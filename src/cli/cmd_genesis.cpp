// Genesis file generation command implementation

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <vector>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

void print_genesis_usage() {
    std::cout << "Usage: sar genesis [options]\n\n";
    std::cout << "Generate genesis file for Sarafu blockchain\n\n";
    std::cout << "Options:\n";
    std::cout << "  --keys-dir <path>      Directory containing validator keys (default: ./keys)\n";
    std::cout << "  --output <path>        Output genesis file path (default: ./genesis.json)\n";
    std::cout << "  --chain-id <id>        Chain ID (default: sarafu-testnet-1)\n";
    std::cout << "  --validator-count <n>  Number of validators (default: auto-detect)\n";
    std::cout << "  --help                 Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  sar genesis --keys-dir ./docker/keys --output ./docker/genesis.json\n";
    std::cout << "  sar genesis --chain-id sarafu-mainnet-1 --validator-count 100\n";
}

struct ValidatorInfo {
    int id;
    std::string consensus_pubkey;
    std::string withdrawal_address;
};

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string extract_json_value(const std::string& json, const std::string& key) {
    std::string search_key = "\"" + key + "\":";
    size_t pos = json.find(search_key);
    if (pos == std::string::npos) {
        return "";
    }
    
    pos += search_key.length();
    
    // Skip whitespace
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) {
        pos++;
    }
    
    // Extract value (assuming it's a quoted string)
    if (json[pos] == '"') {
        pos++; // Skip opening quote
        size_t end_pos = json.find('"', pos);
        if (end_pos != std::string::npos) {
            return json.substr(pos, end_pos - pos);
        }
    }
    
    return "";
}

bool load_validator_info(const std::string& keys_dir, int validator_id, ValidatorInfo& info) {
    fs::path validator_dir = fs::path(keys_dir) / ("validator-" + std::to_string(validator_id));
    
    std::string consensus_json_path = (validator_dir / "consensus_key.json").string();
    std::string withdrawal_json_path = (validator_dir / "withdrawal_key.json").string();
    
    if (!fs::exists(consensus_json_path) || !fs::exists(withdrawal_json_path)) {
        return false;
    }
    
    std::string consensus_json = read_file(consensus_json_path);
    std::string withdrawal_json = read_file(withdrawal_json_path);
    
    info.id = validator_id;
    info.consensus_pubkey = extract_json_value(consensus_json, "public_key");
    info.withdrawal_address = extract_json_value(withdrawal_json, "public_key");
    
    return !info.consensus_pubkey.empty() && !info.withdrawal_address.empty();
}

bool generate_genesis_file(const std::string& keys_dir, 
                           const std::string& output_path,
                           const std::string& chain_id,
                           int validator_count) {
    
    std::cout << "Loading validator keys from: " << keys_dir << "\n";
    
    // Load validator information
    std::vector<ValidatorInfo> validators;
    
    if (validator_count > 0) {
        // Load specific number of validators
        for (int i = 1; i <= validator_count; i++) {
            ValidatorInfo info;
            if (load_validator_info(keys_dir, i, info)) {
                validators.push_back(info);
                std::cout << "  ✓ Loaded validator-" << i << "\n";
            } else {
                std::cerr << "  ✗ Failed to load validator-" << i << "\n";
            }
        }
    } else {
        // Auto-detect validators
        for (int i = 1; i <= 100; i++) { // Max 100 validators
            ValidatorInfo info;
            if (load_validator_info(keys_dir, i, info)) {
                validators.push_back(info);
                std::cout << "  ✓ Loaded validator-" << i << "\n";
            } else {
                break; // No more validators
            }
        }
    }
    
    if (validators.empty()) {
        std::cerr << "Error: No validator keys found in " << keys_dir << "\n";
        return false;
    }
    
    std::cout << "\nGenerating genesis file with " << validators.size() << " validators...\n";
    
    // Create genesis JSON
    std::ofstream file(output_path);
    if (!file.is_open()) {
        std::cerr << "Error: Failed to open output file: " << output_path << "\n";
        return false;
    }
    
    file << "{\n";
    file << "  \"chain_id\": \"" << chain_id << "\",\n";
    file << "  \"genesis_time\": \"2026-02-20T00:00:00Z\",\n";
    file << "  \"config\": {\n";
    file << "    \"slot_duration_ms\": 2000,\n";
    file << "    \"slots_per_epoch\": 32,\n";
    file << "    \"min_validator_stake\": \"32000000000000000000\",\n";
    file << "    \"max_validators\": 1024,\n";
    file << "    \"base_fee_per_gas\": \"1000000000\",\n";
    file << "    \"block_gas_limit\": \"30000000\"\n";
    file << "  },\n";
    file << "  \"initial_validators\": [\n";
    
    for (size_t i = 0; i < validators.size(); i++) {
        const auto& v = validators[i];
        file << "    {\n";
        file << "      \"validator_id\": " << v.id << ",\n";
        file << "      \"consensus_pubkey\": \"" << v.consensus_pubkey << "\",\n";
        file << "      \"withdrawal_address\": \"" << v.withdrawal_address << "\",\n";
        file << "      \"stake\": \"32000000000000000000\",\n";
        file << "      \"commission_rate\": \"0.10\"\n";
        file << "    }";
        if (i < validators.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    
    file << "  ],\n";
    file << "  \"initial_accounts\": [\n";
    file << "    {\n";
    file << "      \"address\": \"0x1111111111111111111111111111111111111111111111111111111111111111\",\n";
    file << "      \"balance\": \"1000000000000000000000000\",\n";
    file << "      \"nonce\": 0\n";
    file << "    },\n";
    file << "    {\n";
    file << "      \"address\": \"0x2222222222222222222222222222222222222222222222222222222222222222\",\n";
    file << "      \"balance\": \"1000000000000000000000000\",\n";
    file << "      \"nonce\": 0\n";
    file << "    },\n";
    file << "    {\n";
    file << "      \"address\": \"0x3333333333333333333333333333333333333333333333333333333333333333\",\n";
    file << "      \"balance\": \"500000000000000000000000\",\n";
    file << "      \"nonce\": 0\n";
    file << "    }\n";
    file << "  ],\n";
    file << "  \"initial_supply\": \"2500000000000000000000000\",\n";
    file << "  \"monetary_policy\": {\n";
    file << "    \"target_annual_inflation\": \"0.05\",\n";
    file << "    \"security_budget_ratio\": \"0.02\",\n";
    file << "    \"max_inflation\": \"0.10\",\n";
    file << "    \"min_inflation\": \"0.01\"\n";
    file << "  }\n";
    file << "}\n";
    
    file.close();
    
    std::cout << "✓ Genesis file created: " << output_path << "\n";
    std::cout << "\nGenesis Summary:\n";
    std::cout << "  Chain ID: " << chain_id << "\n";
    std::cout << "  Validators: " << validators.size() << "\n";
    std::cout << "  Initial Supply: 2,500,000 tokens\n";
    
    return true;
}

int cmd_genesis(int argc, char* argv[]) {
    std::string keys_dir = "./keys";
    std::string output_path = "./genesis.json";
    std::string chain_id = "sarafu-testnet-1";
    int validator_count = 0; // 0 means auto-detect
    
    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_genesis_usage();
            return 0;
        } else if (strcmp(argv[i], "--keys-dir") == 0 && i + 1 < argc) {
            keys_dir = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        } else if (strcmp(argv[i], "--chain-id") == 0 && i + 1 < argc) {
            chain_id = argv[++i];
        } else if (strcmp(argv[i], "--validator-count") == 0 && i + 1 < argc) {
            validator_count = std::stoi(argv[++i]);
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            print_genesis_usage();
            return 1;
        }
    }
    
    std::cout << "========================================\n";
    std::cout << "Sarafu Genesis File Generator\n";
    std::cout << "========================================\n\n";
    
    if (generate_genesis_file(keys_dir, output_path, chain_id, validator_count)) {
        std::cout << "\n========================================\n";
        std::cout << "✓ Genesis file created successfully!\n";
        std::cout << "========================================\n\n";
        return 0;
    } else {
        std::cerr << "\n✗ Genesis generation failed\n";
        return 1;
    }
}
