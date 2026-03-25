#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sarafu/rpc/tls_config.h"

// Forward declare httplib::Server
namespace httplib {
    class Server;
}

namespace sarafu {

// Forward declarations
namespace state {
    class StateMachine;
    class Mempool;
    class FeeMarket;
}

namespace consensus {
    class ConsensusEngine;
    class ValidatorRegistry;
}

namespace rpc {

// Forward declarations for RPC components
class GrpcServer;
class RestGateway;
class WebSocketServer;
class RateLimiter;
class AuthManager;
class EventPublisher;
class RpcMiddleware;
class GrpcServerRunner;

/**
 * RPC server configuration
 */
struct RpcServerConfig {
    // gRPC configuration
    std::string grpc_address = "0.0.0.0:50051";
    bool enable_grpc = true;
    
    // REST gateway configuration
    std::string rest_address = "0.0.0.0:8080";
    bool enable_rest = true;
    TlsConfig rest_tls;
    
    // WebSocket configuration
    std::string websocket_address = "0.0.0.0:8081";
    bool enable_websocket = true;
    TlsConfig websocket_tls;
    
    // Rate limiting configuration
    bool enable_rate_limiting = true;
    size_t max_requests_per_minute = 100;
    
    // Authentication configuration
    bool enable_authentication = false;
    
    // Chain ID
    uint32_t chain_id = 1;
};

/**
 * Unified RPC server that manages all RPC interfaces
 * Provides gRPC, REST, and WebSocket endpoints with rate limiting and authentication
 * 
 * Provides gRPC, REST, and WebSocket endpoints with rate limiting and authentication.
 */
class RpcServer {
public:
    /**
     * Constructor
     * @param config RPC server configuration
     * @param state_machine Pointer to the state machine
     * @param mempool Pointer to the mempool
     * @param consensus Pointer to the consensus engine
     * @param validators Pointer to the validator registry
     * @param fee_market Pointer to the fee market
     */
    RpcServer(
        const RpcServerConfig& config,
        std::shared_ptr<state::StateMachine> state_machine,
        std::shared_ptr<state::Mempool> mempool,
        std::shared_ptr<consensus::ConsensusEngine> consensus,
        std::shared_ptr<consensus::ValidatorRegistry> validators,
        std::shared_ptr<state::FeeMarket> fee_market
    );

    ~RpcServer();

    /**
     * Start all enabled RPC servers
     */
    void Start();

    /**
     * Stop all RPC servers
     */
    void Stop();

private:
    RpcServerConfig config_;
    
    // Core components
    std::shared_ptr<state::StateMachine> state_machine_;
    std::shared_ptr<state::Mempool> mempool_;
    std::shared_ptr<consensus::ConsensusEngine> consensus_;
    std::shared_ptr<consensus::ValidatorRegistry> validators_;
    std::shared_ptr<state::FeeMarket> fee_market_;
    
    bool running_;
    
    // RPC components
    std::shared_ptr<RateLimiter> rate_limiter_;
    std::shared_ptr<AuthManager> auth_manager_;
    std::shared_ptr<RpcMiddleware> middleware_;
    std::shared_ptr<GrpcServer> grpc_service_;
    std::unique_ptr<GrpcServerRunner> grpc_runner_;
    std::shared_ptr<RestGateway> rest_gateway_;
    std::shared_ptr<WebSocketServer> websocket_server_;
    std::shared_ptr<EventPublisher> event_publisher_;
};

} // namespace rpc
} // namespace sarafu
