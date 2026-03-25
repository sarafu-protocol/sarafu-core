// Sarafu Blockchain Node - Main Entry Point
#include "sarafu/node.h"
#include "sarafu/config/configuration.h"
#include "sarafu/logging/logger.h"
#include "sarafu/version.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <memory>
#include <iomanip>
#include <thread>
#include <chrono>

// Global node pointer for signal handling
std::unique_ptr<sarafu::Node> g_node;
std::atomic<bool> g_shutdown_requested(false);

/**
 * Signal handler for graceful shutdown.
 * Handles SIGINT (Ctrl+C) and SIGTERM.
 */
void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nReceived shutdown signal, stopping node..." << std::endl;
        g_shutdown_requested.store(true);
        
        if (g_node) {
            g_node->Stop();
        }
    }
}

/**
 * Print usage information.
 */
void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --config <file>          Path to configuration file (default: config.toml)" << std::endl;
    std::cout << "  --data-dir <dir>         Data directory path" << std::endl;
    std::cout << "  --chain-id <id>          Chain ID (mainnet=1, testnet=2)" << std::endl;
    std::cout << "  --validator              Run in validator mode" << std::endl;
    std::cout << "  --consensus-key <file>   Path to BLS12-381 consensus key" << std::endl;
    std::cout << "  --withdrawal-key <file>  Path to Ed25519 withdrawal key" << std::endl;
    std::cout << "  --log-level <level>      Log level (DEBUG, INFO, WARN, ERROR)" << std::endl;
    std::cout << "  --help                   Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Environment Variables:" << std::endl;
    std::cout << "  SARAFU_NETWORK_LISTEN_ADDRESS    Network listen address" << std::endl;
    std::cout << "  SARAFU_RPC_GRPC_ADDRESS           gRPC server address" << std::endl;
    std::cout << "  SARAFU_RPC_REST_ADDRESS           REST server address" << std::endl;
    std::cout << "  SARAFU_RPC_REST_TLS_ENABLED       Enable REST TLS (true/false)" << std::endl;
    std::cout << "  SARAFU_RPC_WEBSOCKET_TLS_ENABLED  Enable WebSocket TLS (true/false)" << std::endl;
    std::cout << "  SARAFU_RPC_TLS_CERT_PATH          TLS certificate path" << std::endl;
    std::cout << "  SARAFU_RPC_TLS_KEY_PATH           TLS private key path" << std::endl;
    std::cout << "  SARAFU_RPC_TLS_CA_PATH            TLS CA certificate path (mTLS)" << std::endl;
    std::cout << "  SARAFU_RPC_TLS_REQUIRE_CLIENT_AUTH  Require client certs (true/false)" << std::endl;
    std::cout << "  SARAFU_STORAGE_DATA_DIRECTORY     Data directory path" << std::endl;
    std::cout << std::endl;
}

/**
 * Main entry point for the Sarafu blockchain node.
 * 
 * This function:
 * 1. Parses command-line arguments
 * 2. Loads configuration from file
 * 3. Applies command-line and environment variable overrides
 * 4. Validates configuration
 * 5. Initializes logging
 * 6. Creates and initializes the Node
 * 7. Registers signal handlers for graceful shutdown
 * 8. Starts the Node
 * 9. Waits for shutdown signal
 * 10. Performs graceful shutdown
 * 
 * Requirements: 25.1, 25.2, 25.3, 25.4
 */
int run_node(int argc, char* argv[]) {
    // Print banner
    std::cout << "========================================" << std::endl;
    std::cout << "  Sarafu Blockchain Node" << std::endl;
    std::cout << "  Version: " << sarafu::VERSION << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Check for help flag
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        }
    }

    // 1. Create configuration with defaults
    sarafu::config::Configuration config;

    // 2. Load configuration from file
    std::string config_file = "config.toml";
    
    // Check for --config argument
    for (int i = 1; i < argc - 1; i++) {
        std::string arg(argv[i]);
        if (arg == "--config") {
            config_file = argv[i + 1];
            break;
        }
    }

    std::cout << "Loading configuration from: " << config_file << std::endl;
    if (!config.LoadFromFile(config_file)) {
        std::cerr << "Warning: Failed to load configuration file, using defaults" << std::endl;
    }

    // 3. Apply command-line argument overrides
    config.ApplyCommandLineOverrides(argc, argv);

    // 4. Apply environment variable overrides
    config.ApplyEnvironmentOverrides();

    // 5. Validate configuration
    std::string validation_error;
    if (!config.Validate(validation_error)) {
        std::cerr << "Configuration validation failed: " << validation_error << std::endl;
        return 1;
    }

    // 6. Initialize logging
    const auto& log_config = config.GetLogConfig();
    sarafu::logging::LogLevel log_level = sarafu::logging::LogLevel::INFO;
    
    if (log_config.log_level == "DEBUG") {
        log_level = sarafu::logging::LogLevel::DEBUG;
    } else if (log_config.log_level == "INFO") {
        log_level = sarafu::logging::LogLevel::INFO;
    } else if (log_config.log_level == "WARN") {
        log_level = sarafu::logging::LogLevel::WARN;
    } else if (log_config.log_level == "ERROR") {
        log_level = sarafu::logging::LogLevel::ERROR;
    }

    sarafu::logging::Logger::instance().set_level(log_level);
    LOG_INFO("Main", "Logging initialized at level: " + log_config.log_level);

    // 7. Create Node
    LOG_INFO("Main", "Creating node...");
    g_node = std::make_unique<sarafu::Node>(config);

    // 8. Register signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    LOG_INFO("Main", "Signal handlers registered (SIGINT, SIGTERM)");

    // 9. Initialize Node
    LOG_INFO("Main", "Initializing node...");
    if (!g_node->Initialize()) {
        LOG_ERROR("Main", "Failed to initialize node");
        return 1;
    }

    // 10. Start Node
    LOG_INFO("Main", "Starting node...");
    if (!g_node->Start()) {
        LOG_ERROR("Main", "Failed to start node");
        return 1;
    }

    // 11. Print node information
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Node started successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Chain ID: " << config.GetChainId() << std::endl;
    std::cout << "Data Directory: " << config.GetStorageConfig().data_directory << std::endl;
    std::cout << "Network: " << config.GetNetworkConfig().listen_address << std::endl;
    
    if (config.GetRpcConfig().enable_grpc) {
        std::cout << "gRPC: " << config.GetRpcConfig().grpc_address << std::endl;
    }
    if (config.GetRpcConfig().enable_rest) {
        std::cout << "REST: " << config.GetRpcConfig().rest_address << std::endl;
    }
    if (config.GetRpcConfig().enable_websocket) {
        std::cout << "WebSocket: " << config.GetRpcConfig().websocket_address << std::endl;
    }
    
    if (g_node->IsValidator()) {
        std::cout << "Mode: Validator" << std::endl;
        std::cout << "Validator ID: " << g_node->GetValidatorId().to_hex() << std::endl;
    } else {
        std::cout << "Mode: Full Node" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "Press Ctrl+C to stop the node" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // 12. Wait for shutdown signal
    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 13. Graceful shutdown (already called in signal handler)
    LOG_INFO("Main", "Node shutdown complete");
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "  Node stopped successfully" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}

#ifndef CLI_MODE
// Main entry point when running as standalone node binary
int main(int argc, char* argv[]) {
    return run_node(argc, argv);
}
#endif
