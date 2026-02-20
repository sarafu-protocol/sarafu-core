#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>

namespace sarafu {
namespace config {

/**
 * Network configuration
 */
struct NetworkConfig {
    std::string listen_address = "0.0.0.0:30303";
    std::vector<std::string> bootstrap_peers;
    size_t min_peers = 8;
    size_t max_peers = 50;
    size_t gossip_fanout = 8;
};

/**
 * RPC configuration
 */
struct RpcConfig {
    std::string grpc_address = "0.0.0.0:50051";
    std::string rest_address = "0.0.0.0:8080";
    std::string websocket_address = "0.0.0.0:8081";
    bool enable_grpc = true;
    bool enable_rest = true;
    bool enable_websocket = true;
    bool enable_rate_limiting = true;
    size_t max_requests_per_minute = 100;
    bool enable_authentication = false;
};

/**
 * Storage configuration
 */
struct StorageConfig {
    std::string data_directory = "./data";
    bool enable_pruning = false;
    uint64_t pruning_keep_blocks = 1000000; // Keep 1M blocks (weak subjectivity period)
};

/**
 * Validator configuration
 */
struct ValidatorConfig {
    bool is_validator = false;
    std::string consensus_key_path;
    std::string withdrawal_key_path;
};

/**
 * Genesis configuration
 */
struct GenesisConfig {
    std::string genesis_file_path = "./genesis.json";
};

/**
 * Logging configuration
 */
struct LogConfig {
    std::string log_level = "INFO"; // DEBUG, INFO, WARN, ERROR
    std::string log_file;           // Empty means stdout only
    bool enable_structured_logging = true;
};

/**
 * Consensus configuration
 */
struct ConsensusConfig {
    uint64_t block_time_ms = 2000;  // 2 seconds
    uint64_t view_change_timeout_ms = 10000; // 10 seconds
    uint64_t epoch_length = 10000;  // blocks per epoch
};

/**
 * Main configuration class
 * Loads configuration from TOML/YAML file with command-line and environment variable overrides
 */
class Configuration {
public:
    /**
     * Default constructor with default values
     */
    Configuration();

    /**
     * Load configuration from file
     * @param config_file Path to TOML or YAML configuration file
     * @return true if successful, false otherwise
     */
    bool LoadFromFile(const std::string& config_file);

    /**
     * Apply command-line argument overrides
     * @param argc Argument count
     * @param argv Argument values
     */
    void ApplyCommandLineOverrides(int argc, char** argv);

    /**
     * Apply environment variable overrides
     * Environment variables are prefixed with SARAFU_
     * Example: SARAFU_NETWORK_LISTEN_ADDRESS=0.0.0.0:30303
     */
    void ApplyEnvironmentOverrides();

    /**
     * Validate configuration
     * Checks that all required parameters are present and valid
     * @return true if valid, false otherwise
     */
    bool Validate(std::string& error_message) const;

    /**
     * Get network configuration
     */
    const NetworkConfig& GetNetworkConfig() const { return network_; }
    NetworkConfig& GetNetworkConfig() { return network_; }

    /**
     * Get RPC configuration
     */
    const RpcConfig& GetRpcConfig() const { return rpc_; }
    RpcConfig& GetRpcConfig() { return rpc_; }

    /**
     * Get storage configuration
     */
    const StorageConfig& GetStorageConfig() const { return storage_; }
    StorageConfig& GetStorageConfig() { return storage_; }

    /**
     * Get validator configuration
     */
    const ValidatorConfig& GetValidatorConfig() const { return validator_; }
    ValidatorConfig& GetValidatorConfig() { return validator_; }

    /**
     * Get genesis configuration
     */
    const GenesisConfig& GetGenesisConfig() const { return genesis_; }
    GenesisConfig& GetGenesisConfig() { return genesis_; }

    /**
     * Get logging configuration
     */
    const LogConfig& GetLogConfig() const { return log_; }
    LogConfig& GetLogConfig() { return log_; }

    /**
     * Get consensus configuration
     */
    const ConsensusConfig& GetConsensusConfig() const { return consensus_; }
    ConsensusConfig& GetConsensusConfig() { return consensus_; }

    /**
     * Get chain ID
     */
    uint32_t GetChainId() const { return chain_id_; }
    void SetChainId(uint32_t chain_id) { chain_id_ = chain_id; }

private:
    NetworkConfig network_;
    RpcConfig rpc_;
    StorageConfig storage_;
    ValidatorConfig validator_;
    GenesisConfig genesis_;
    LogConfig log_;
    ConsensusConfig consensus_;
    uint32_t chain_id_ = 0;

    /**
     * Parse TOML configuration file
     */
    bool ParseToml(const std::string& config_file);

    /**
     * Parse YAML configuration file (future support)
     */
    bool ParseYaml(const std::string& config_file);

    /**
     * Helper to get environment variable with prefix
     */
    std::optional<std::string> GetEnvVar(const std::string& name) const;

    /**
     * Helper to parse command-line arguments
     */
    std::map<std::string, std::string> ParseCommandLineArgs(int argc, char** argv) const;
};

} // namespace config
} // namespace sarafu
