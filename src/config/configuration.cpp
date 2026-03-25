#include "sarafu/config/configuration.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>

namespace sarafu {
namespace config {

Configuration::Configuration() {
    // Default values are set in the struct definitions
}

// Helper function for safe integer parsing
static uint64_t safe_parse_uint(const std::string& value_str, const std::string& key, int line_num) {
    std::string trimmed = value_str;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\r\n"));
    trimmed.erase(trimmed.find_last_not_of(" \t\r\n") + 1);
    
    if (trimmed.empty()) {
        std::cerr << "Warning: Empty value for '" << key << "' at line " << line_num << std::endl;
        return 0;
    }
    
    for (char c : trimmed) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            std::cerr << "Error: Invalid integer '" << trimmed << "' for '" << key << "' at line " << line_num << std::endl;
            return 0;
        }
    }
    
    try {
        return std::stoull(trimmed);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing '" << key << "': " << e.what() << std::endl;
        return 0;
    }
}

bool Configuration::LoadFromFile(const std::string& config_file) {
    auto ends_with = [](const std::string& str, const std::string& suffix) {
        return str.size() >= suffix.size() &&
               str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
    };

    auto resolve_config_path = [](const std::string& path_str) -> std::optional<std::string> {
        namespace fs = std::filesystem;
        fs::path input(path_str);
        if (fs::exists(input)) {
            return input.string();
        }
        if (input.is_absolute()) {
            return std::nullopt;
        }

        fs::path probe = fs::current_path();
        for (int depth = 0; depth < 4; ++depth) {
            fs::path candidate = (probe / input).lexically_normal();
            if (fs::exists(candidate)) {
                return candidate.string();
            }
            if (!probe.has_parent_path()) {
                break;
            }
            probe = probe.parent_path();
        }
        return std::nullopt;
    };

    auto resolved = resolve_config_path(config_file);
    if (!resolved.has_value()) {
        std::cerr << "Failed to locate configuration file: " << config_file << std::endl;
        return false;
    }

    const std::string& resolved_path = *resolved;
    if (ends_with(resolved_path, ".toml")) {
        return ParseToml(resolved_path);
    } else if (ends_with(resolved_path, ".yaml") || ends_with(resolved_path, ".yml")) {
        return ParseYaml(resolved_path);
    } else {
        std::cerr << "Unsupported configuration file format: " << resolved_path << std::endl;
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
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (line.empty() || line[0] == '#') continue;
        
        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }
        
        size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, equals_pos);
        std::string value = line.substr(equals_pos + 1);
        
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        if (value.length() >= 2 && 
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.length() - 2);
        }
        
        if (current_section == "network") {
            if (key == "listen_address") {
                network_.listen_address = value;
            } else if (key == "bootstrap_peers") {
                if (!value.empty() && value.front() == '[' && value.back() == ']') {
                    value = value.substr(1, value.length() - 2);
                    std::stringstream ss(value);
                    std::string peer;
                    while (std::getline(ss, peer, ',')) {
                        peer.erase(0, peer.find_first_not_of(" \t\"'"));
                        peer.erase(peer.find_last_not_of(" \t\"'") + 1);
                        if (!peer.empty()) network_.bootstrap_peers.push_back(peer);
                    }
                }
            } else if (key == "min_peers") {
                network_.min_peers = safe_parse_uint(value, key, line_number);
            } else if (key == "max_peers") {
                network_.max_peers = safe_parse_uint(value, key, line_number);
            } else if (key == "gossip_fanout") {
                network_.gossip_fanout = safe_parse_uint(value, key, line_number);
            }
        } else if (current_section == "rpc") {
            if (key == "grpc_address") {
                rpc_.grpc_address = value;
            } else if (key == "grpc_port") {
                uint64_t port = safe_parse_uint(value, key, line_number);
                if (port > 0) rpc_.grpc_address = "0.0.0.0:" + std::to_string(port);
            } else if (key == "rest_address") {
                rpc_.rest_address = value;
            } else if (key == "rest_port") {
                uint64_t port = safe_parse_uint(value, key, line_number);
                if (port > 0) rpc_.rest_address = "0.0.0.0:" + std::to_string(port);
            } else if (key == "websocket_address") {
                rpc_.websocket_address = value;
            } else if (key == "websocket_port") {
                uint64_t port = safe_parse_uint(value, key, line_number);
                if (port > 0) rpc_.websocket_address = "0.0.0.0:" + std::to_string(port);
            } else if (key == "enable_grpc") {
                rpc_.enable_grpc = (value == "true" || value == "1");
            } else if (key == "enable_rest") {
                rpc_.enable_rest = (value == "true" || value == "1");
            } else if (key == "enable_websocket") {
                rpc_.enable_websocket = (value == "true" || value == "1");
            } else if (key == "enable_rest_tls") {
                rpc_.enable_rest_tls = (value == "true" || value == "1");
            } else if (key == "enable_websocket_tls") {
                rpc_.enable_websocket_tls = (value == "true" || value == "1");
            } else if (key == "tls_cert_path") {
                rpc_.tls_cert_path = value;
            } else if (key == "tls_key_path") {
                rpc_.tls_key_path = value;
            } else if (key == "tls_ca_path") {
                rpc_.tls_ca_path = value;
            } else if (key == "tls_require_client_auth") {
                rpc_.tls_require_client_auth = (value == "true" || value == "1");
            } else if (key == "enable_rate_limiting") {
                rpc_.enable_rate_limiting = (value == "true" || value == "1");
            } else if (key == "max_requests_per_minute") {
                rpc_.max_requests_per_minute = safe_parse_uint(value, key, line_number);
            } else if (key == "enable_authentication") {
                rpc_.enable_authentication = (value == "true" || value == "1");
            }
        } else if (current_section == "storage") {
            if (key == "data_directory" || key == "data_dir") {
                storage_.data_directory = value;
            } else if (key == "enable_pruning") {
                storage_.enable_pruning = (value == "true" || value == "1");
            } else if (key == "pruning_keep_blocks") {
                storage_.pruning_keep_blocks = safe_parse_uint(value, key, line_number);
            }
        } else if (current_section == "validator") {
            if (key == "is_validator" || key == "enabled") {
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
        } else if (current_section == "log" || current_section == "logging") {
            if (key == "log_level" || key == "level") {
                log_.log_level = value;
            } else if (key == "log_file" || key == "file") {
                log_.log_file = value;
            } else if (key == "enable_structured_logging" || key == "format") {
                if (key == "format") {
                    log_.enable_structured_logging = (value == "json");
                } else {
                    log_.enable_structured_logging = (value == "true" || value == "1");
                }
            }
        } else if (current_section == "consensus") {
            if (key == "block_time_ms") {
                consensus_.block_time_ms = safe_parse_uint(value, key, line_number);
            } else if (key == "view_change_timeout_ms") {
                consensus_.view_change_timeout_ms = safe_parse_uint(value, key, line_number);
            } else if (key == "epoch_length") {
                consensus_.epoch_length = safe_parse_uint(value, key, line_number);
            }
        } else if (current_section == "chain") {
            if (key == "chain_id") {
                uint64_t id = safe_parse_uint(value, key, line_number);
                if (id > 0) chain_id_ = static_cast<uint32_t>(id);
            }
        }
    }

    return true;
}

bool Configuration::ParseYaml(const std::string& config_file) {
    std::cerr << "YAML configuration parsing not yet implemented" << std::endl;
    return false;
}

void Configuration::ApplyCommandLineOverrides(int argc, char** argv) {
    auto args = ParseCommandLineArgs(argc, argv);
    
    for (const auto& [key, value] : args) {
        if (key == "--network-listen-address" || key == "--listen-address") {
            network_.listen_address = value;
        } else if (key == "--bootstrap-peer") {
            network_.bootstrap_peers.push_back(value);
        } else if (key == "--grpc-address") {
            rpc_.grpc_address = value;
        } else if (key == "--rest-address") {
            rpc_.rest_address = value;
        } else if (key == "--websocket-address") {
            rpc_.websocket_address = value;
        } else if (key == "--rest-tls") {
            rpc_.enable_rest_tls = (value == "true" || value == "1");
        } else if (key == "--websocket-tls") {
            rpc_.enable_websocket_tls = (value == "true" || value == "1");
        } else if (key == "--tls-cert") {
            rpc_.tls_cert_path = value;
        } else if (key == "--tls-key") {
            rpc_.tls_key_path = value;
        } else if (key == "--tls-ca") {
            rpc_.tls_ca_path = value;
        } else if (key == "--tls-require-client-auth") {
            rpc_.tls_require_client_auth = (value == "true" || value == "1");
        } else if (key == "--data-dir" || key == "--data-directory") {
            storage_.data_directory = value;
        } else if (key == "--validator") {
            validator_.is_validator = (value == "true" || value == "1" || value.empty());
        } else if (key == "--consensus-key") {
            validator_.consensus_key_path = value;
        } else if (key == "--withdrawal-key") {
            validator_.withdrawal_key_path = value;
        } else if (key == "--genesis") {
            genesis_.genesis_file_path = value;
        } else if (key == "--log-level") {
            log_.log_level = value;
        } else if (key == "--log-file") {
            log_.log_file = value;
        } else if (key == "--chain-id") {
            try {
                chain_id_ = std::stoul(value);
            } catch (...) {
                std::cerr << "Invalid chain_id: " << value << std::endl;
            }
        }
    }
}

void Configuration::ApplyEnvironmentOverrides() {
    if (auto val = GetEnvVar("SARAFU_NETWORK_LISTEN_ADDRESS")) {
        network_.listen_address = *val;
    }
    if (auto val = GetEnvVar("SARAFU_RPC_GRPC_ADDRESS")) {
        rpc_.grpc_address = *val;
    }
    if (auto val = GetEnvVar("SARAFU_RPC_REST_ADDRESS")) {
        rpc_.rest_address = *val;
    }
    if (auto val = GetEnvVar("SARAFU_RPC_WEBSOCKET_ADDRESS")) {
        rpc_.websocket_address = *val;
    }
    if (auto val = GetEnvVar("SARAFU_RPC_REST_TLS_ENABLED")) {
        rpc_.enable_rest_tls = (*val == "true" || *val == "1");
    }
    if (auto val = GetEnvVar("SARAFU_RPC_WEBSOCKET_TLS_ENABLED")) {
        rpc_.enable_websocket_tls = (*val == "true" || *val == "1");
    }
    if (auto val = GetEnvVar("SARAFU_RPC_TLS_CERT_PATH")) {
        rpc_.tls_cert_path = *val;
    }
    if (auto val = GetEnvVar("SARAFU_RPC_TLS_KEY_PATH")) {
        rpc_.tls_key_path = *val;
    }
    if (auto val = GetEnvVar("SARAFU_RPC_TLS_CA_PATH")) {
        rpc_.tls_ca_path = *val;
    }
    if (auto val = GetEnvVar("SARAFU_RPC_TLS_REQUIRE_CLIENT_AUTH")) {
        rpc_.tls_require_client_auth = (*val == "true" || *val == "1");
    }
    if (auto val = GetEnvVar("SARAFU_STORAGE_DATA_DIRECTORY")) {
        storage_.data_directory = *val;
    }
    if (auto val = GetEnvVar("SARAFU_VALIDATOR_IS_VALIDATOR")) {
        validator_.is_validator = (*val == "true" || *val == "1");
    }
    if (auto val = GetEnvVar("SARAFU_LOG_LEVEL")) {
        log_.log_level = *val;
    }
    if (auto val = GetEnvVar("SARAFU_CHAIN_ID")) {
        try {
            chain_id_ = std::stoul(*val);
        } catch (...) {
            std::cerr << "Invalid chain_id: " << *val << std::endl;
        }
    }
}

bool Configuration::Validate(std::string& error_message) const {
    auto is_valid_log_level = [](const std::string& level) {
        std::string upper = level;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return upper == "DEBUG" || upper == "INFO" || upper == "WARN" || upper == "ERROR";
    };

    if (network_.listen_address.empty()) {
        error_message = "Network listen address is required";
        return false;
    }
    if (network_.min_peers > network_.max_peers) {
        error_message = "min_peers cannot be greater than max_peers";
        return false;
    }
    if (storage_.data_directory.empty()) {
        error_message = "Data directory is required";
        return false;
    }
    if (!is_valid_log_level(log_.log_level)) {
        error_message = "Invalid log level: " + log_.log_level;
        return false;
    }
    if (consensus_.block_time_ms == 0) {
        error_message = "Block time must be greater than 0";
        return false;
    }
    if (rpc_.enable_rest_tls || rpc_.enable_websocket_tls) {
        if (rpc_.tls_cert_path.empty() || rpc_.tls_key_path.empty()) {
            error_message = "TLS cert/key path is required when TLS is enabled for REST/WebSocket";
            return false;
        }
        if (rpc_.tls_require_client_auth && rpc_.tls_ca_path.empty()) {
            error_message = "TLS CA path is required when client auth is enabled";
            return false;
        }
    }
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
        
        if (starts_with(arg, "--")) {
            size_t equals_pos = arg.find('=');
            if (equals_pos != std::string::npos) {
                std::string key = arg.substr(0, equals_pos);
                std::string value = arg.substr(equals_pos + 1);
                args[key] = value;
            } else if (i + 1 < argc && !starts_with(std::string(argv[i + 1]), "--")) {
                args[arg] = argv[i + 1];
                ++i;
            } else {
                args[arg] = "true";
            }
        }
    }
    
    return args;
}

} // namespace config
} // namespace sarafu
