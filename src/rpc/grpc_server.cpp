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
        // Convert protobuf message to internal Transaction type
        state::Transaction tx;
        tx.from = state::Address::from_hex(request->from());
        tx.to = state::Address::from_hex(request->to());
        tx.amount = request->amount();
        tx.nonce = request->nonce();
        tx.fee = request->fee();
        tx.gas_limit = request->gas_limit();
        tx.chain_id = request->chain_id();
        
        // Deserialize signature
        std::vector<uint8_t> sig_data(request->signature().begin(), request->signature().end());
        tx.signature = crypto::Ed25519_Signature(sig_data);
        
        // Validate transaction signature
        if (!tx.verify_signature()) {
            response->set_accepted(false);
            response->set_error("Invalid transaction signature");
            return grpc::Status::OK;
        }
        
        // Add to mempool
        bool added = mempool_->add_transaction(tx);
        
        if (added) {
            response->set_accepted(true);
            response->set_transaction_hash(tx.hash().to_hex());
        } else {
            response->set_accepted(false);
            response->set_error("Transaction rejected by mempool");
        }
        
        return grpc::Status::OK;
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
        // Query account from state machine
        state::Address address = state::Address::from_hex(request->address());
        auto account_opt = state_machine_->get_account(address);
        
        if (account_opt.has_value()) {
            const auto& account = account_opt.value();
            response->set_found(true);
            response->set_address(address.to_hex());
            response->set_balance(account.balance);
            response->set_nonce(account.nonce);
        } else {
            response->set_found(false);
        }
        
        return grpc::Status::OK;
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
        // Query account balance from state machine
        state::Address address = state::Address::from_hex(request->address());
        auto account_opt = state_machine_->get_account(address);
        
        if (account_opt.has_value()) {
            response->set_balance(account_opt.value().balance);
        } else {
            response->set_balance(0);
        }
        
        return grpc::Status::OK;
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
        // Query account nonce from state machine
        state::Address address = state::Address::from_hex(request->address());
        auto account_opt = state_machine_->get_account(address);
        
        if (account_opt.has_value()) {
            response->set_nonce(account_opt.value().nonce);
        } else {
            response->set_nonce(0);
        }
        
        return grpc::Status::OK;
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
        crypto::Blake3Hash tx_hash = crypto::Blake3Hash::from_hex(request->transaction_hash());
        
        // Check mempool for pending transactions
        if (mempool_->has_transaction(tx_hash)) {
            response->set_status(sarafu::GetTransactionStatusResponse::STATUS_PENDING);
            response->set_in_mempool(true);
            return grpc::Status::OK;
        }
        
        // Check storage for finalized transactions
        auto receipt_opt = state_machine_->get_transaction_receipt(tx_hash);
        if (receipt_opt.has_value()) {
            const auto& receipt = receipt_opt.value();
            response->set_status(sarafu::GetTransactionStatusResponse::STATUS_FINALIZED);
            response->set_in_mempool(false);
            response->set_block_height(receipt.block_height);
            response->set_block_hash(receipt.block_hash.to_hex());
            response->set_success(receipt.success);
            return grpc::Status::OK;
        }
        
        // Transaction not found
        response->set_status(sarafu::GetTransactionStatusResponse::STATUS_NOT_FOUND);
        return grpc::Status::OK;
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
        // Query transaction receipt from storage
        crypto::Blake3Hash tx_hash = crypto::Blake3Hash::from_hex(request->transaction_hash());
        auto receipt_opt = state_machine_->get_transaction_receipt(tx_hash);
        
        if (!receipt_opt.has_value()) {
            response->set_found(false);
            return grpc::Status::OK;
        }
        
        const auto& receipt = receipt_opt.value();
        
        // Populate response
        response->set_found(true);
        response->set_transaction_hash(tx_hash.to_hex());
        response->set_block_height(receipt.block_height);
        response->set_block_hash(receipt.block_hash.to_hex());
        response->set_transaction_index(receipt.transaction_index);
        response->set_success(receipt.success);
        response->set_gas_used(receipt.gas_used);
        response->set_cumulative_gas_used(receipt.cumulative_gas_used);
        
        if (!receipt.error_message.empty()) {
            response->set_error_message(receipt.error_message);
        }
        
        return grpc::Status::OK;
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
        // Query block from consensus engine
        uint64_t height = request->height();
        auto block_opt = consensus_->get_block_by_height(height);
        
        if (!block_opt.has_value()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Block not found at height");
        }
        
        const auto& block = block_opt.value();
        
        // Populate response
        response->set_height(block.height);
        response->set_hash(block.hash.to_hex());
        response->set_parent_hash(block.parent_hash.to_hex());
        response->set_state_root(block.state_root.to_hex());
        response->set_timestamp(block.timestamp);
        response->set_proposer_id(block.proposer_id.to_hex());
        
        // Add transactions
        for (const auto& tx : block.transactions) {
            auto* tx_msg = response->add_transactions();
            tx_msg->set_from(tx.from.to_hex());
            tx_msg->set_to(tx.to.to_hex());
            tx_msg->set_amount(tx.amount);
            tx_msg->set_nonce(tx.nonce);
            tx_msg->set_fee(tx.fee);
            tx_msg->set_gas_limit(tx.gas_limit);
            tx_msg->set_chain_id(tx.chain_id);
            tx_msg->set_signature(std::string(tx.signature.bytes(), tx.signature.bytes() + tx.signature.size()));
        }
        
        return grpc::Status::OK;
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
        // Query block from consensus engine
        crypto::Blake3Hash hash = crypto::Blake3Hash::from_hex(request->hash());
        auto block_opt = consensus_->get_block_by_hash(hash);
        
        if (!block_opt.has_value()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Block not found with hash");
        }
        
        const auto& block = block_opt.value();
        
        // Populate response
        response->set_height(block.height);
        response->set_hash(block.hash.to_hex());
        response->set_parent_hash(block.parent_hash.to_hex());
        response->set_state_root(block.state_root.to_hex());
        response->set_timestamp(block.timestamp);
        response->set_proposer_id(block.proposer_id.to_hex());
        
        // Add transactions
        for (const auto& tx : block.transactions) {
            auto* tx_msg = response->add_transactions();
            tx_msg->set_from(tx.from.to_hex());
            tx_msg->set_to(tx.to.to_hex());
            tx_msg->set_amount(tx.amount);
            tx_msg->set_nonce(tx.nonce);
            tx_msg->set_fee(tx.fee);
            tx_msg->set_gas_limit(tx.gas_limit);
            tx_msg->set_chain_id(tx.chain_id);
            tx_msg->set_signature(std::string(tx.signature.bytes(), tx.signature.bytes() + tx.signature.size()));
        }
        
        return grpc::Status::OK;
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
        // Get epoch from request, or use current epoch if not specified
        uint64_t epoch = request->has_epoch() ? request->epoch() : consensus_->get_current_epoch();
        
        // Query validator set from validator registry
        auto validator_set = validators_->get_validator_set(epoch);
        
        if (!validator_set.has_value()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Validator set not found for epoch");
        }
        
        // Populate response
        response->set_epoch(epoch);
        response->set_total_stake(validator_set->total_stake);
        
        // Add validators
        for (const auto& validator : validator_set->validators) {
            auto* validator_msg = response->add_validators();
            validator_msg->set_validator_id(validator.id.to_hex());
            validator_msg->set_consensus_key(validator.consensus_key.to_hex());
            validator_msg->set_bonded_stake(validator.bonded_stake);
            validator_msg->set_is_active(validator.is_active);
        }
        
        return grpc::Status::OK;
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
        // Query current epoch from consensus engine
        uint64_t epoch = consensus_->get_current_epoch();
        uint64_t epoch_start = epoch * 10000;  // 10,000 blocks per epoch
        uint64_t epoch_end = epoch_start + 9999;
        
        response->set_epoch(epoch);
        response->set_epoch_start_height(epoch_start);
        response->set_epoch_end_height(epoch_end);
        
        return grpc::Status::OK;
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
        // Query base fee from fee market
        uint64_t base_fee = fee_market_->get_base_fee();
        response->set_base_fee(base_fee);
        return grpc::Status::OK;
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
        // Get base fee from fee market
        uint64_t base_fee = fee_market_->get_base_fee();
        
        // Calculate estimated fee: base_fee * gas_limit
        uint64_t gas_limit = request->gas_limit();
        uint64_t estimated_fee = base_fee * gas_limit;
        
        // Total fee includes priority fee if specified
        uint64_t priority_fee = request->has_priority_fee() ? request->priority_fee() : 0;
        uint64_t total_fee = estimated_fee + priority_fee;
        
        response->set_estimated_fee(estimated_fee);
        response->set_base_fee(base_fee);
        response->set_total_fee(total_fee);
        return grpc::Status::OK;
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
        // Get chain ID from consensus engine
        uint64_t chain_id = consensus_->get_chain_id();
        response->set_chain_id(chain_id);
        return grpc::Status::OK;
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
