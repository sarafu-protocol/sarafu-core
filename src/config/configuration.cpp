#include "sarafu/config/configuration.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace sarafu {
namespace config {

Configuration::Configuration() {
    // Default values are set in the struct definitions
}

bool Configuration::LoadFromFile(const std::string& config_file) {
    // Determine file type by extension
    auto ends_with = [](const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() && 
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    
    if (ends_with(config_file, ".toml")) {
        return ParseToml(config_file);
    } else if (ends_with(config_file, ".yaml") || ends_with(config_file, ".yml")) {
        return ParseYaml(config_file);
    } else {
        std::cerr << "Unsupported configuration file format: " << config_file << std::endl;
        std::cerr << "Supported formats: .toml, .yaml, .yml" << std::endl;
        return false;
    }
}

bool Configuration::ParseToml(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open configuration file: " << config_file << std::endl;
        return false;
    }

    std::string line;
    std::string current_section;
    int line_number = 0;

    while (std::getline(file, line)) {
        line_number++;
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Check for section header [section]
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Parse key = value
        size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            std::cerr << "Invalid line " << line_number << " in " << config_file << ": " << line << std::endl;
            continue;
        }
        
        std::string key = line.substr(0, equals_pos);
        std::string value = line.substr(equals_pos + 1);
        
        // Trim key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Remove quotes from string values
        if (value.length() >= 2 && 
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.length() - 2);
        }
        
        // Parse based on section
        if (current_section == "network") {
            if (key == "listen_address") {
                network_.listen_address = value;
            } else if (key == "bootstrap_peers") {
                // Parse array: ["peer1", "peer2"]
                if (value.front() == '[' && value.back() == ']') {
                    value = value.substr(1, value.length() - 2);
                    std::stringstream ss(value);
                    std::string peer;
                    while (std::getline(ss, peer, ',')) {
                        peer.erase(0, peer.find_first_not_of(" \t\"'"));
                        peer.erase(peer.find_last_not_of(" \t\"'") + 1);
                        if (!peer.empty()) {
                            network_.bootstrap_peers.push_back(peer);
                        }
                    }
                }
            } else if (key == "min_peers") {
                network_.min_peers = std::stoull(value);
            } else if (key == "max_peers") {
                network_.max_peers = std::stoull(value);
            } else if (key == "gossip_fanout") {
                network_.gossip_fanout = std::stoull(value);
            }
        } else if (current_section == "rpc") {
            if (key == "grpc_address") {
                rpc_.grpc_address = value;
            } else if (key == "rest_address") {
                rpc_.rest_address = value;
            } else if (key == "websocket_address") {
                rpc_.websocket_address = value;
            } else if (key == "enable_grpc") {
                rpc_.enable_grpc = (value == "true" || value == "1");
            } else if (key == "enable_rest") {
                rpc_.enable_rest = (value == "true" || value == "1");
            } else if (key == "enable_websocket") {
                rpc_.enable_websocket = (value == "true" || value == "1");
            } else if (key == "enable_rate_limiting") {
                rpc_.enable_rate_limiting = (value == "true" || value == "1");
            } else if (key == "max_requests_per_minute") {
                rpc_.max_requests_per_minute = std::stoull(value);
            } else if (key == "enable_authentication") {
                rpc_.enable_authentication = (value == "true" || value == "1");
            }
        } else if (current_section == "storage") {
            if (key == "data_directory") {
                storage_.data_directory = value;
            } else if (key == "enable_pruning") {
                storage_.enable_pruning = (value == "true" || value == "1");
            } else if (key == "pruning_keep_blocks") {
                storage_.pruning_keep_blocks = std::stoull(value);
            }
        } else if (current_section == "validator") {
            if (key == "is_validator") {
                validator_.is_validator = (value == "true" || value == "1");
            } else if (key == "consensus_key_path") {
                validator_.consensus_key_path = value;
            } else if (key == "withdrawal_key_path") {
                validator_.withdrawal_key_path = value;
            }
        } else if (current_section == "genesis") {
            if (key == "genesis_file_path") {
                genesis_.genesis_file_path = value;
            }
        } else if (current_section == "log") {
            if (key == "log_level") {
                log_.log_level = value;
            } else if (key == "log_file") {
                log_.log_file = value;
            } else if (key == "enable_structured_logging") {
                log_.enable_structured_logging = (value == "true" || value == "1");
            }
        } else if (current_section == "consensus") {
            if (key == "block_time_ms") {
                consensus_.block_time_ms = std::stoull(value);
            } else if (key == "view_change_timeout_ms") {
                consensus_.view_change_timeout_ms = std::stoull(value);
            } else if (key == "epoch_length") {
                consensus_.epoch_length = std::stoull(value);
            }
        } else if (current_section == "chain") {
            if (key == "chain_id") {
                chain_id_ = std::stoul(value);
            }
        }
    }

    return true;
}

bool Configuration::ParseYaml(const std::string& config_file) {
    std::cerr << "YAML configuration parsing not yet implemented" << std::endl;
    std::cerr << "Please use TOML format (.toml) for now" << std::endl;
    return false;
}

void Configuration::ApplyCommandLineOverrides(int argc, char** argv) {
    auto args = ParseCommandLineArgs(argc, argv);
    
    for (const auto& [key, value] : args) {
        // Network overrides
        if (key == "--network-listen-address" || key == "--listen-address") {
            network_.listen_address = value;
        } else if (key == "--bootstrap-peer") {
            network_.bootstrap_peers.push_back(value);
        }
        // RPC overrides
        else if (key == "--grpc-address") {
            rpc_.grpc_address = value;
        } else if (key == "--rest-address") {
            rpc_.rest_address = value;
        } else if (key == "--websocket-address") {
            rpc_.websocket_address = value;
        }
        // Storage overrides
        else if (key == "--data-dir" || key == "--data-directory") {
            storage_.data_directory = value;
        }
        // Validator overrides
        else if (key == "--validator") {
            validator_.is_validator = (value == "true" || value == "1");
        } else if (key == "--consensus-key") {
            validator_.consensus_key_path = value;
        } else if (key == "--withdrawal-key") {
            validator_.withdrawal_key_path = value;
        }
        // Genesis overrides
        else if (key == "--genesis") {
            genesis_.genesis_file_path = value;
        }
        // Logging overrides
        else if (key == "--log-level") {
            log_.log_level = value;
        } else if (key == "--log-file") {
            log_.log_file = value;
        }
        // Chain overrides
        else if (key == "--chain-id") {
            chain_id_ = std::stoul(value);
        }
    }
}

void Configuration::ApplyEnvironmentOverrides() {
    // Network overrides
    if (auto val = GetEnvVar("SARAFU_NETWORK_LISTEN_ADDRESS")) {
        network_.listen_address = *val;
    }
    if (auto val = GetEnvVar("SARAFU_NETWORK_MIN_PEERS")) {
        network_.min_peers = std::stoull(*val);
    }
    if (auto val = GetEnvVar("SARAFU_NETWORK_MAX_PEERS")) {
        network_.max_peers = std::stoull(*val);
    }
    
    // RPC overrides
    if (auto val = GetEnvVar("SARAFU_RPC_GRPC_ADDRESS")) {
        rpc_.grpc_address = *val;
    }
    if (auto val = GetEnvVar("SARAFU_RPC_REST_ADDRESS")) {
        rpc_.rest_address = *val;
    }
    if (auto val = GetEnvVar("SARAFU_RPC_WEBSOCKET_ADDRESS")) {
        rpc_.websocket_address = *val;
    }
    if (auto val = GetEnvVar("SARAFU_RPC_ENABLE_GRPC")) {
        rpc_.enable_grpc = (*val == "true" || *val == "1");
    }
    if (auto val = GetEnvVar("SARAFU_RPC_ENABLE_REST")) {
        rpc_.enable_rest = (*val == "true" || *val == "1");
    }
    if (auto val = GetEnvVar("SARAFU_RPC_ENABLE_WEBSOCKET")) {
        rpc_.enable_websocket = (*val == "true" || *val == "1");
    }
    
    // Storage overrides
    if (auto val = GetEnvVar("SARAFU_STORAGE_DATA_DIRECTORY")) {
        storage_.data_directory = *val;
    }
    if (auto val = GetEnvVar("SARAFU_STORAGE_ENABLE_PRUNING")) {
        storage_.enable_pruning = (*val == "true" || *val == "1");
    }
    
    // Validator overrides
    if (auto val = GetEnvVar("SARAFU_VALIDATOR_IS_VALIDATOR")) {
        validator_.is_validator = (*val == "true" || *val == "1");
    }
    if (auto val = GetEnvVar("SARAFU_VALIDATOR_CONSENSUS_KEY_PATH")) {
        validator_.consensus_key_path = *val;
    }
    if (auto val = GetEnvVar("SARAFU_VALIDATOR_WITHDRAWAL_KEY_PATH")) {
        validator_.withdrawal_key_path = *val;
    }
    
    // Genesis overrides
    if (auto val = GetEnvVar("SARAFU_GENESIS_FILE_PATH")) {
        genesis_.genesis_file_path = *val;
    }
    
    // Logging overrides
    if (auto val = GetEnvVar("SARAFU_LOG_LEVEL")) {
        log_.log_level = *val;
    }
    if (auto val = GetEnvVar("SARAFU_LOG_FILE")) {
        log_.log_file = *val;
    }
    
    // Chain overrides
    if (auto val = GetEnvVar("SARAFU_CHAIN_ID")) {
        chain_id_ = std::stoul(*val);
    }
}

bool Configuration::Validate(std::string& error_message) const {
    // Validate network configuration
    if (network_.listen_address.empty()) {
        error_message = "Network listen address is required";
        return false;
    }
    if (network_.min_peers > network_.max_peers) {
        error_message = "min_peers cannot be greater than max_peers";
        return false;
    }
    
    // Validate RPC configuration
    if (rpc_.enable_grpc && rpc_.grpc_address.empty()) {
        error_message = "gRPC address is required when gRPC is enabled";
        return false;
    }
    if (rpc_.enable_rest && rpc_.rest_address.empty()) {
        error_message = "REST address is required when REST is enabled";
        return false;
    }
    if (rpc_.enable_websocket && rpc_.websocket_address.empty()) {
        error_message = "WebSocket address is required when WebSocket is enabled";
        return false;
    }
    
    // Validate storage configuration
    if (storage_.data_directory.empty()) {
        error_message = "Data directory is required";
        return false;
    }
    
    // Validate validator configuration
    if (validator_.is_validator) {
        if (validator_.consensus_key_path.empty()) {
            error_message = "Consensus key path is required for validator nodes";
            return false;
        }
        if (validator_.withdrawal_key_path.empty()) {
            error_message = "Withdrawal key path is required for validator nodes";
            return false;
        }
    }
    
    // Validate genesis configuration
    if (genesis_.genesis_file_path.empty()) {
        error_message = "Genesis file path is required";
        return false;
    }
    
    // Validate logging configuration
    std::string log_level_upper = log_.log_level;
    std::transform(log_level_upper.begin(), log_level_upper.end(), 
                   log_level_upper.begin(), ::toupper);
    if (log_level_upper != "DEBUG" && log_level_upper != "INFO" && 
        log_level_upper != "WARN" && log_level_upper != "ERROR") {
        error_message = "Invalid log level: " + log_.log_level + 
                       " (must be DEBUG, INFO, WARN, or ERROR)";
        return false;
    }
    
    // Validate consensus configuration
    if (consensus_.block_time_ms == 0) {
        error_message = "Block time must be greater than 0";
        return false;
    }
    if (consensus_.view_change_timeout_ms == 0) {
        error_message = "View change timeout must be greater than 0";
        return false;
    }
    if (consensus_.epoch_length == 0) {
        error_message = "Epoch length must be greater than 0";
        return false;
    }
    
    return true;
}

std::optional<std::string> Configuration::GetEnvVar(const std::string& name) const {
    const char* value = std::getenv(name.c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string(value);
}

std::map<std::string, std::string> Configuration::ParseCommandLineArgs(
    int argc, char** argv) const {
    std::map<std::string, std::string> args;
    
    auto starts_with = [](const std::string& str, const std::string& prefix) {
        return str.size() >= prefix.size() && 
               str.compare(0, prefix.size(), prefix) == 0;
    };
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        // Check if it's a flag (starts with --)
        if (starts_with(arg, "--")) {
            // Check if value is in same argument (--key=value)
            size_t equals_pos = arg.find('=');
            if (equals_pos != std::string::npos) {
                std::string key = arg.substr(0, equals_pos);
                std::string value = arg.substr(equals_pos + 1);
                args[key] = value;
            }
            // Check if next argument is the value
            else if (i + 1 < argc && !starts_with(std::string(argv[i + 1]), "--")) {
                args[arg] = argv[i + 1];
                ++i; // Skip next argument
            }
            // Boolean flag without value
            else {
                args[arg] = "true";
            }
        }
    }
    
    return args;
}

} // namespace config
} // namespace sarafu
