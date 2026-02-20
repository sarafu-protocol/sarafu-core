#include "sarafu/rpc/grpc_server.h"
#include <iostream>

namespace sarafu {
namespace rpc {

GrpcServer::GrpcServer(
    std::shared_ptr<StateMachine> state_machine,
    std::shared_ptr<Mempool> mempool,
    std::shared_ptr<ConsensusEngine> consensus,
    std::shared_ptr<ValidatorRegistry> validators,
    std::shared_ptr<FeeMarket> fee_market
)
    : state_machine_(state_machine),
      mempool_(mempool),
      consensus_(consensus),
      validators_(validators),
      fee_market_(fee_market) {}

grpc::Status GrpcServer::SubmitTransaction(
    grpc::ServerContext* context,
    const sarafu::TransactionMessage* request,
    sarafu::SubmitTransactionResponse* response
) {
    try {
        // TODO: Convert protobuf message to internal Transaction type
        // TODO: Validate transaction signature
        // TODO: Add to mempool
        
        // Placeholder implementation
        response->set_accepted(false);
        response->set_error("Not implemented");
        
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "SubmitTransaction not yet implemented");
    } catch (const std::exception& e) {
        response->set_accepted(false);
        response->set_error(std::string("Internal error: ") + e.what());
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetAccount(
    grpc::ServerContext* context,
    const sarafu::GetAccountRequest* request,
    sarafu::GetAccountResponse* response
) {
    try {
        // TODO: Query account from state machine
        // TODO: Convert to protobuf message
        
        response->set_found(false);
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetAccount not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetAccountBalance(
    grpc::ServerContext* context,
    const sarafu::GetAccountBalanceRequest* request,
    sarafu::GetAccountBalanceResponse* response
) {
    try {
        // TODO: Query account balance from state machine
        
        response->set_balance(0);
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetAccountBalance not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetAccountNonce(
    grpc::ServerContext* context,
    const sarafu::GetAccountNonceRequest* request,
    sarafu::GetAccountNonceResponse* response
) {
    try {
        // TODO: Query account nonce from state machine
        
        response->set_nonce(0);
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetAccountNonce not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetTransactionStatus(
    grpc::ServerContext* context,
    const sarafu::GetTransactionStatusRequest* request,
    sarafu::GetTransactionStatusResponse* response
) {
    try {
        // TODO: Check mempool for pending transactions
        // TODO: Check storage for finalized transactions
        
        response->set_status(sarafu::GetTransactionStatusResponse::STATUS_NOT_FOUND);
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetTransactionStatus not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetTransactionReceipt(
    grpc::ServerContext* context,
    const sarafu::GetTransactionReceiptRequest* request,
    sarafu::GetTransactionReceiptResponse* response
) {
    try {
        // TODO: Query transaction receipt from storage
        
        response->set_found(false);
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetTransactionReceipt not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetBlockByHeight(
    grpc::ServerContext* context,
    const sarafu::GetBlockByHeightRequest* request,
    sarafu::BlockMessage* response
) {
    try {
        // TODO: Query block from consensus engine or storage
        
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetBlockByHeight not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetBlockByHash(
    grpc::ServerContext* context,
    const sarafu::GetBlockByHashRequest* request,
    sarafu::BlockMessage* response
) {
    try {
        // TODO: Query block from consensus engine or storage
        
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetBlockByHash not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetValidatorSet(
    grpc::ServerContext* context,
    const sarafu::GetValidatorSetRequest* request,
    sarafu::ValidatorSetUpdateMessage* response
) {
    try {
        // TODO: Query validator set from validator registry
        
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetValidatorSet not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetCurrentEpoch(
    grpc::ServerContext* context,
    const sarafu::Empty* request,
    sarafu::GetCurrentEpochResponse* response
) {
    try {
        // TODO: Query current epoch from consensus engine
        
        response->set_epoch(0);
        response->set_epoch_start_height(0);
        response->set_epoch_end_height(0);
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetCurrentEpoch not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetBaseFee(
    grpc::ServerContext* context,
    const sarafu::Empty* request,
    sarafu::GetBaseFeeResponse* response
) {
    try {
        // TODO: Query base fee from fee market
        
        response->set_base_fee(0);
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetBaseFee not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::EstimateFee(
    grpc::ServerContext* context,
    const sarafu::EstimateFeeRequest* request,
    sarafu::EstimateFeeResponse* response
) {
    try {
        // TODO: Estimate fee using fee market
        
        response->set_estimated_fee(0);
        response->set_base_fee(0);
        response->set_total_fee(0);
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "EstimateFee not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

grpc::Status GrpcServer::GetChainID(
    grpc::ServerContext* context,
    const sarafu::Empty* request,
    sarafu::GetChainIDResponse* response
) {
    try {
        // TODO: Get chain ID from configuration
        
        response->set_chain_id(0);
        return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "GetChainID not yet implemented");
    } catch (const std::exception& e) {
        return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
    }
}

// GrpcServerRunner implementation

GrpcServerRunner::GrpcServerRunner(const std::string& server_address, GrpcServer* service)
    : server_address_(server_address), service_(service) {}

GrpcServerRunner::~GrpcServerRunner() {
    Shutdown();
}

void GrpcServerRunner::Run() {
    grpc::ServerBuilder builder;
    
    // Listen on the given address without authentication
    builder.AddListeningPort(server_address_, grpc::InsecureServerCredentials());
    
    // Register the service
    builder.RegisterService(service_);
    
    // Build and start the server
    server_ = builder.BuildAndStart();
    
    if (server_) {
        std::cout << "gRPC server listening on " << server_address_ << std::endl;
        
        // Wait for the server to shutdown
        server_->Wait();
    } else {
        std::cerr << "Failed to start gRPC server on " << server_address_ << std::endl;
    }
}

void GrpcServerRunner::Shutdown() {
    if (server_) {
        server_->Shutdown();
        server_.reset();
    }
}

} // namespace rpc
} // namespace sarafu
