#include <gtest/gtest.h>
#include "sarafu/config/configuration.h"
#include <fstream>
#include <cstdlib>

using namespace sarafu::config;

class ConfigurationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary config file for testing
        test_config_file_ = "test_config.toml";
        CreateTestConfigFile();
    }

    void TearDown() override {
        // Clean up test config file
        std::remove(test_config_file_.c_str());
    }

    void CreateTestConfigFile() {
        std::ofstream file(test_config_file_);
        file << "[chain]\n";
        file << "chain_id = 1337\n";
        file << "\n";
        file << "[network]\n";
        file << "listen_address = \"0.0.0.0:30303\"\n";
        file << "bootstrap_peers = [\"peer1\", \"peer2\"]\n";
        file << "min_peers = 8\n";
        file << "max_peers = 50\n";
        file << "\n";
        file << "[rpc]\n";
        file << "grpc_address = \"0.0.0.0:50051\"\n";
        file << "enable_grpc = true\n";
        file << "\n";
        file << "[storage]\n";
        file << "data_directory = \"./test_data\"\n";
        file << "\n";
        file << "[validator]\n";
        file << "is_validator = false\n";
        file << "\n";
        file << "[genesis]\n";
        file << "genesis_file_path = \"./genesis.json\"\n";
        file << "\n";
        file << "[log]\n";
        file << "log_level = \"DEBUG\"\n";
        file << "\n";
        file << "[consensus]\n";
        file << "block_time_ms = 2000\n";
        file << "epoch_length = 10000\n";
        file.close();
    }

    std::string test_config_file_;
};

TEST_F(ConfigurationTest, DefaultConfiguration) {
    Configuration config;
    
    // Check default values
    EXPECT_EQ(config.GetNetworkConfig().listen_address, "0.0.0.0:30303");
    EXPECT_EQ(config.GetRpcConfig().grpc_address, "0.0.0.0:50051");
    EXPECT_EQ(config.GetStorageConfig().data_directory, "./data");
    EXPECT_FALSE(config.GetValidatorConfig().is_validator);
    EXPECT_EQ(config.GetLogConfig().log_level, "INFO");
    EXPECT_EQ(config.GetConsensusConfig().block_time_ms, 2000);
}

TEST_F(ConfigurationTest, LoadFromFile) {
    Configuration config;
    
    ASSERT_TRUE(config.LoadFromFile(test_config_file_));
    
    // Verify loaded values
    EXPECT_EQ(config.GetChainId(), 1337);
    EXPECT_EQ(config.GetNetworkConfig().listen_address, "0.0.0.0:30303");
    EXPECT_EQ(config.GetNetworkConfig().bootstrap_peers.size(), 2);
    EXPECT_EQ(config.GetNetworkConfig().bootstrap_peers[0], "peer1");
    EXPECT_EQ(config.GetNetworkConfig().bootstrap_peers[1], "peer2");
    EXPECT_EQ(config.GetNetworkConfig().min_peers, 8);
    EXPECT_EQ(config.GetNetworkConfig().max_peers, 50);
    
    EXPECT_EQ(config.GetRpcConfig().grpc_address, "0.0.0.0:50051");
    EXPECT_TRUE(config.GetRpcConfig().enable_grpc);
    
    EXPECT_EQ(config.GetStorageConfig().data_directory, "./test_data");
    
    EXPECT_FALSE(config.GetValidatorConfig().is_validator);
    
    EXPECT_EQ(config.GetGenesisConfig().genesis_file_path, "./genesis.json");
    
    EXPECT_EQ(config.GetLogConfig().log_level, "DEBUG");
    
    EXPECT_EQ(config.GetConsensusConfig().block_time_ms, 2000);
    EXPECT_EQ(config.GetConsensusConfig().epoch_length, 10000);
}

TEST_F(ConfigurationTest, LoadNonExistentFile) {
    Configuration config;
    
    EXPECT_FALSE(config.LoadFromFile("nonexistent.toml"));
}

TEST_F(ConfigurationTest, CommandLineOverrides) {
    Configuration config;
    config.LoadFromFile(test_config_file_);
    
    // Simulate command-line arguments
    const char* argv[] = {
        "program",
        "--listen-address", "127.0.0.1:9999",
        "--data-dir", "/custom/data",
        "--log-level", "ERROR",
        "--chain-id", "42"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);
    
    config.ApplyCommandLineOverrides(argc, const_cast<char**>(argv));
    
    EXPECT_EQ(config.GetNetworkConfig().listen_address, "127.0.0.1:9999");
    EXPECT_EQ(config.GetStorageConfig().data_directory, "/custom/data");
    EXPECT_EQ(config.GetLogConfig().log_level, "ERROR");
    EXPECT_EQ(config.GetChainId(), 42);
}

TEST_F(ConfigurationTest, EnvironmentOverrides) {
    Configuration config;
    config.LoadFromFile(test_config_file_);
    
    // Set environment variables
    setenv("SARAFU_NETWORK_LISTEN_ADDRESS", "192.168.1.1:7777", 1);
    setenv("SARAFU_STORAGE_DATA_DIRECTORY", "/env/data", 1);
    setenv("SARAFU_LOG_LEVEL", "WARN", 1);
    setenv("SARAFU_CHAIN_ID", "99", 1);
    
    config.ApplyEnvironmentOverrides();
    
    EXPECT_EQ(config.GetNetworkConfig().listen_address, "192.168.1.1:7777");
    EXPECT_EQ(config.GetStorageConfig().data_directory, "/env/data");
    EXPECT_EQ(config.GetLogConfig().log_level, "WARN");
    EXPECT_EQ(config.GetChainId(), 99);
    
    // Clean up environment variables
    unsetenv("SARAFU_NETWORK_LISTEN_ADDRESS");
    unsetenv("SARAFU_STORAGE_DATA_DIRECTORY");
    unsetenv("SARAFU_LOG_LEVEL");
    unsetenv("SARAFU_CHAIN_ID");
}

TEST_F(ConfigurationTest, ValidationSuccess) {
    Configuration config;
    config.LoadFromFile(test_config_file_);
    
    std::string error_message;
    EXPECT_TRUE(config.Validate(error_message));
    EXPECT_TRUE(error_message.empty());
}

TEST_F(ConfigurationTest, ValidationFailureEmptyListenAddress) {
    Configuration config;
    config.GetNetworkConfig().listen_address = "";
    
    std::string error_message;
    EXPECT_FALSE(config.Validate(error_message));
    EXPECT_EQ(error_message, "Network listen address is required");
}

TEST_F(ConfigurationTest, ValidationFailureMinPeersGreaterThanMax) {
    Configuration config;
    config.LoadFromFile(test_config_file_);
    config.GetNetworkConfig().min_peers = 100;
    config.GetNetworkConfig().max_peers = 50;
    
    std::string error_message;
    EXPECT_FALSE(config.Validate(error_message));
    EXPECT_EQ(error_message, "min_peers cannot be greater than max_peers");
}

TEST_F(ConfigurationTest, ValidationFailureEmptyDataDirectory) {
    Configuration config;
    config.LoadFromFile(test_config_file_);
    config.GetStorageConfig().data_directory = "";
    
    std::string error_message;
    EXPECT_FALSE(config.Validate(error_message));
    EXPECT_EQ(error_message, "Data directory is required");
}

TEST_F(ConfigurationTest, ValidationFailureValidatorMissingKeys) {
    Configuration config;
    config.LoadFromFile(test_config_file_);
    config.GetValidatorConfig().is_validator = true;
    config.GetValidatorConfig().consensus_key_path = "";
    
    std::string error_message;
    EXPECT_FALSE(config.Validate(error_message));
    EXPECT_EQ(error_message, "Consensus key path is required for validator nodes");
}

TEST_F(ConfigurationTest, ValidationFailureInvalidLogLevel) {
    Configuration config;
    config.LoadFromFile(test_config_file_);
    config.GetLogConfig().log_level = "INVALID";
    
    std::string error_message;
    EXPECT_FALSE(config.Validate(error_message));
    EXPECT_TRUE(error_message.find("Invalid log level") != std::string::npos);
}

TEST_F(ConfigurationTest, ValidationFailureZeroBlockTime) {
    Configuration config;
    config.LoadFromFile(test_config_file_);
    config.GetConsensusConfig().block_time_ms = 0;
    
    std::string error_message;
    EXPECT_FALSE(config.Validate(error_message));
    EXPECT_EQ(error_message, "Block time must be greater than 0");
}

TEST_F(ConfigurationTest, LoadExampleConfigs) {
    // Test that example config files can be loaded
    Configuration mainnet_config;
    EXPECT_TRUE(mainnet_config.LoadFromFile("config.mainnet.toml"));
    
    std::string error_message;
    // Note: Validation may fail because genesis file doesn't exist, but that's okay
    // We're just testing that the config file parses correctly
    mainnet_config.Validate(error_message);
    
    Configuration testnet_config;
    EXPECT_TRUE(testnet_config.LoadFromFile("config.testnet.toml"));
    
    Configuration local_config;
    EXPECT_TRUE(local_config.LoadFromFile("config.local.toml"));
}
