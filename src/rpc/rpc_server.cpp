#include "sarafu/rpc/rpc_server.h"
#include <iostream>
#include <thread>

namespace sarafu {
namespace rpc {

RpcServer::RpcServer(
    const RpcServerConfig& config,
    std::shared_ptr<StateMachine> state_machine,
    std::shared_ptr<Mempool> mempool,
    std::shared_ptr<ConsensusEngine> consensus,
    std::shared_ptr<ValidatorRegistry> validators,
    std::shared_ptr<FeeMarket> fee_market
)
    : config_(config), running_(false) {
    
    // Initialize rate limiter
    if (config_.enable_rate_limiting) {
        RateLimitConfig rate_config(
            config_.max_requests_per_minute,
            std::chrono::seconds(60)
        );
        rate_limiter_ = std::make_shared<RateLimiter>(rate_config);
        std::cout << "Rate limiting enabled: " << config_.max_requests_per_minute 
                  << " requests per minute" << std::endl;
    } else {
        // Create a permissive rate limiter
        RateLimitConfig rate_config(1000000, std::chrono::seconds(1));
        rate_limiter_ = std::make_shared<RateLimiter>(rate_config);
        std::cout << "Rate limiting disabled" << std::endl;
    }
    
    // Initialize auth manager
    auth_manager_ = std::make_shared<AuthManager>();
    if (config_.enable_authentication) {
        auth_manager_->Enable();
        std::cout << "Authentication enabled" << std::endl;
    } else {
        auth_manager_->Disable();
        std::cout << "Authentication disabled" << std::endl;
    }
    
    // Initialize middleware
    middleware_ = std::make_shared<RpcMiddleware>(rate_limiter_, auth_manager_);
    
    // Initialize gRPC server
    if (config_.enable_grpc) {
        grpc_service_ = std::make_shared<GrpcServer>(
            state_machine, mempool, consensus, validators, fee_market
        );
        std::cout << "gRPC server initialized on " << config_.grpc_address << std::endl;
    }
    
    // Initialize REST gateway
    if (config_.enable_rest && grpc_service_) {
        rest_gateway_ = std::make_shared<RestGateway>(grpc_service_);
        std::cout << "REST gateway initialized on " << config_.rest_address << std::endl;
    }
    
    // Initialize WebSocket server
    if (config_.enable_websocket) {
        websocket_server_ = std::make_shared<WebSocketServer>();
        event_publisher_ = std::make_shared<EventPublisher>(websocket_server_);
        std::cout << "WebSocket server initialized on " << config_.websocket_address << std::endl;
    }
}

RpcServer::~RpcServer() {
    Stop();
}

void RpcServer::Start() {
    if (running_) {
        std::cout << "RPC server already running" << std::endl;
        return;
    }
    
    running_ = true;
    std::cout << "Starting RPC server..." << std::endl;
    
    // Start WebSocket server
    if (config_.enable_websocket && websocket_server_) {
        websocket_server_->Start(config_.websocket_address);
    }
    
    // Start REST gateway
    if (config_.enable_rest && rest_gateway_) {
        rest_gateway_->Start(config_.rest_address);
    }
    
    // Start gRPC server (this will block)
    if (config_.enable_grpc && grpc_service_) {
        grpc_runner_ = std::make_unique<GrpcServerRunner>(
            config_.grpc_address, grpc_service_.get()
        );
        
        // Run gRPC server in a separate thread so it doesn't block
        std::thread grpc_thread([this]() {
            grpc_runner_->Run();
        });
        grpc_thread.detach();
    }
    
    std::cout << "RPC server started successfully" << std::endl;
    std::cout << "  gRPC: " << (config_.enable_grpc ? config_.grpc_address : "disabled") << std::endl;
    std::cout << "  REST: " << (config_.enable_rest ? config_.rest_address : "disabled") << std::endl;
    std::cout << "  WebSocket: " << (config_.enable_websocket ? config_.websocket_address : "disabled") << std::endl;
}

void RpcServer::Stop() {
    if (!running_) {
        return;
    }
    
    std::cout << "Stopping RPC server..." << std::endl;
    
    // Stop gRPC server
    if (grpc_runner_) {
        grpc_runner_->Shutdown();
        grpc_runner_.reset();
    }
    
    // Stop REST gateway
    if (rest_gateway_) {
        rest_gateway_->Stop();
    }
    
    // Stop WebSocket server
    if (websocket_server_) {
        websocket_server_->Stop();
    }
    
    running_ = false;
    std::cout << "RPC server stopped" << std::endl;
}

} // namespace rpc
} // namespace sarafu
