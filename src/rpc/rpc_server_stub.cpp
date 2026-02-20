#include "sarafu/rpc/rpc_server.h"
#include "sarafu/logging/logger.h"

namespace sarafu {
namespace rpc {

RpcServer::RpcServer(
    const RpcServerConfig& config,
    std::shared_ptr<state::StateMachine> state_machine,
    std::shared_ptr<state::Mempool> mempool,
    std::shared_ptr<consensus::ConsensusEngine> consensus,
    std::shared_ptr<consensus::ValidatorRegistry> validators,
    std::shared_ptr<state::FeeMarket> fee_market
)
    : config_(config),
      state_machine_(state_machine),
      mempool_(mempool),
      consensus_(consensus),
      validators_(validators),
      fee_market_(fee_market),
      running_(false) {
}

RpcServer::~RpcServer() {
    if (running_) {
        Stop();
    }
}

void RpcServer::Start() {
    if (running_) {
        LOG_WARN("RpcServer", "RPC server is already running");
        return;
    }

    LOG_INFO("RpcServer", "Starting RPC server (stub implementation)...");

    // TODO: Start gRPC server when gRPC is integrated
    if (config_.enable_grpc) {
        LOG_INFO("RpcServer", "gRPC server would start at: " + config_.grpc_address);
    }

    // TODO: Start REST gateway when REST is integrated
    if (config_.enable_rest) {
        LOG_INFO("RpcServer", "REST gateway would start at: " + config_.rest_address);
    }

    // TODO: Start WebSocket server when WebSocket is integrated
    if (config_.enable_websocket) {
        LOG_INFO("RpcServer", "WebSocket server would start at: " + config_.websocket_address);
    }

    running_ = true;
    LOG_INFO("RpcServer", "RPC server started (stub)");
}

void RpcServer::Stop() {
    if (!running_) {
        return;
    }

    LOG_INFO("RpcServer", "Stopping RPC server...");

    // TODO: Stop all RPC servers when implemented

    running_ = false;
    LOG_INFO("RpcServer", "RPC server stopped");
}

} // namespace rpc
} // namespace sarafu
