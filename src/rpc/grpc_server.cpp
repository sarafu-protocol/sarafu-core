#include "sarafu/rpc/grpc_server.h"
#include "sarafu/consensus/consensus_engine.h"
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/state/account.h"
#include "sarafu/state/fee_market.h"
#include "sarafu/state/mempool.h"
#include "sarafu/state/state_machine.h"
#include "sarafu/state/transaction.h"
#include <iostream>

namespace sarafu {
namespace rpc {

namespace {

std::string bytes_from_vector(const std::vector<uint8_t>& data) {
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
}

std::string bytes_from_hash(const crypto::Blake3Hash& hash) {
    auto bytes = hash.serialize();
    return bytes_from_vector(bytes);
}

std::string bytes_from_signature(const crypto::BLS12_381_Signature& sig) {
    auto bytes = sig.serialize();
    return bytes_from_vector(bytes);
}

void fill_block_message(const consensus::Block& block, sarafu::BlockMessage* response) {
    auto* header = response->mutable_header();
    header->set_height(block.header.height);
    header->set_timestamp(block.header.timestamp);
    header->set_previous_hash(bytes_from_hash(block.header.previous_hash));
    header->set_state_root(bytes_from_hash(block.header.state_root));
    header->set_transactions_root(bytes_from_hash(block.header.transactions_root));
    header->set_validator_set_root(bytes_from_hash(block.header.validator_set_root));
    header->set_proposer(bytes_from_vector(block.header.proposer.serialize()));
    header->set_epoch(block.header.epoch);

    auto* qc = response->mutable_justify();
    qc->set_block_height(block.justify.block_height);
    qc->set_block_hash(bytes_from_hash(block.justify.block_hash));
    qc->set_view_number(block.justify.view_number);
    qc->set_aggregated_signature(bytes_from_signature(block.justify.aggregated_signature));
    for (const auto& signer : block.justify.signers) {
        qc->add_signers(bytes_from_vector(signer.serialize()));
    }
    qc->set_total_stake_signed(block.justify.total_stake_signed);

    for (const auto& tx : block.transactions) {
        auto* tx_msg = response->add_transactions();
        tx_msg->set_from(bytes_from_vector(tx.from.serialize()));
        tx_msg->set_to(bytes_from_vector(tx.to.serialize()));
        tx_msg->set_amount(tx.amount);
        tx_msg->set_nonce(tx.nonce);
        tx_msg->set_fee(tx.fee);
        tx_msg->set_gas_limit(tx.gas_limit);
        tx_msg->set_chain_id(tx.chain_id);
        tx_msg->set_signature(bytes_from_vector(tx.signature.serialize()));
    }
}

} // namespace

GrpcServer::GrpcServer(
    std::shared_ptr<state::StateMachine> state_machine,
    std::shared_ptr<state::Mempool> mempool,
    std::shared_ptr<consensus::ConsensusEngine> consensus,
    std::shared_ptr<consensus::ValidatorRegistry> validators,
    std::shared_ptr<state::FeeMarket> fee_market
)
    : state_machine_(std::move(state_machine)),
      mempool_(std::move(mempool)),
      consensus_(std::move(consensus)),
      validators_(std::move(validators)),
      fee_market_(std::move(fee_market)) {}

grpc::Status GrpcServer::SubmitTransaction(
    grpc::ServerContext* context,
    const sarafu::TransactionMessage* request,
    sarafu::SubmitTransactionResponse* response
) {
    try {
        // Convert protobuf message to internal Transaction type
        state::Transaction tx;
        std::vector<uint8_t> from_bytes(request->from().begin(), request->from().end());
        std::vector<uint8_t> to_bytes(request->to().begin(), request->to().end());
        if (from_bytes.size() != state::Address::ADDRESS_SIZE ||
            to_bytes.size() != state::Address::ADDRESS_SIZE) {
            response->set_accepted(false);
            response->set_error("Invalid address length");
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid address length");
        }
        tx.from = state::Address(from_bytes);
        tx.to = state::Address(to_bytes);
        tx.amount = request->amount();
        tx.nonce = request->nonce();
        tx.fee = request->fee();
        tx.gas_limit = request->gas_limit();
        tx.chain_id = request->chain_id();
        
        // Deserialize signature
        std::vector<uint8_t> sig_data(request->signature().begin(), request->signature().end());
        tx.signature = crypto::Ed25519_Signature(sig_data);

        // Add to mempool (validation occurs inside mempool/account manager)
        bool added = mempool_ && mempool_->add_transaction(tx);
        
        if (added) {
            response->set_accepted(true);
            auto hash_bytes = tx.hash().serialize();
            response->set_tx_hash(std::string(reinterpret_cast<const char*>(hash_bytes.data()), hash_bytes.size()));
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
        std::vector<uint8_t> address_bytes(request->address().begin(), request->address().end());
        if (address_bytes.size() != state::Address::ADDRESS_SIZE) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid address length");
        }
        state::Address address(address_bytes);
        auto account_opt = state_machine_ ? state_machine_->get_account(address) : std::optional<state::Account>();
        
        if (account_opt.has_value()) {
            const auto& account = account_opt.value();
            response->set_found(true);
            auto* account_msg = response->mutable_account();
            auto address_vec = account.address.serialize();
            account_msg->set_address(std::string(reinterpret_cast<const char*>(address_vec.data()), address_vec.size()));
            account_msg->set_balance(account.balance);
            account_msg->set_nonce(account.nonce);
            auto code_hash = account.code_hash.serialize();
            account_msg->set_code_hash(std::string(reinterpret_cast<const char*>(code_hash.data()), code_hash.size()));
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
        std::vector<uint8_t> address_bytes(request->address().begin(), request->address().end());
        if (address_bytes.size() != state::Address::ADDRESS_SIZE) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid address length");
        }
        state::Address address(address_bytes);
        auto account_opt = state_machine_ ? state_machine_->get_account(address) : std::optional<state::Account>();
        
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
        std::vector<uint8_t> address_bytes(request->address().begin(), request->address().end());
        if (address_bytes.size() != state::Address::ADDRESS_SIZE) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid address length");
        }
        state::Address address(address_bytes);
        auto account_opt = state_machine_ ? state_machine_->get_account(address) : std::optional<state::Account>();
        
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
        std::vector<uint8_t> hash_bytes(request->tx_hash().begin(), request->tx_hash().end());
        if (hash_bytes.size() != crypto::Blake3Hash::HASH_SIZE) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid transaction hash length");
        }
        crypto::Blake3Hash tx_hash(hash_bytes);
        
        // Check mempool for pending transactions
        if (mempool_ && mempool_->has_transaction(tx_hash)) {
            response->set_status(sarafu::GetTransactionStatusResponse::STATUS_PENDING);
            return grpc::Status::OK;
        }
        
        // Check storage for finalized transactions
        auto receipt_opt = state_machine_ ? state_machine_->get_receipt(tx_hash) : std::optional<state::TransactionReceipt>();
        if (receipt_opt.has_value()) {
            const auto& receipt = receipt_opt.value();
            response->set_status(receipt.success
                ? sarafu::GetTransactionStatusResponse::STATUS_FINALIZED
                : sarafu::GetTransactionStatusResponse::STATUS_FAILED);
            response->set_block_height(receipt.block_height);
            response->set_success(receipt.success);
            if (!receipt.error_message.empty()) {
                response->set_error(receipt.error_message);
            }
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
        std::vector<uint8_t> hash_bytes(request->tx_hash().begin(), request->tx_hash().end());
        if (hash_bytes.size() != crypto::Blake3Hash::HASH_SIZE) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid transaction hash length");
        }
        crypto::Blake3Hash tx_hash(hash_bytes);
        auto receipt_opt = state_machine_ ? state_machine_->get_receipt(tx_hash) : std::optional<state::TransactionReceipt>();
        
        if (!receipt_opt.has_value()) {
            response->set_found(false);
            return grpc::Status::OK;
        }
        
        const auto& receipt = receipt_opt.value();
        
        // Populate response
        response->set_found(true);
        auto* receipt_msg = response->mutable_receipt();
        auto receipt_hash = receipt.tx_hash.serialize();
        receipt_msg->set_tx_hash(std::string(reinterpret_cast<const char*>(receipt_hash.data()), receipt_hash.size()));
        receipt_msg->set_block_height(receipt.block_height);
        receipt_msg->set_success(receipt.success);
        receipt_msg->set_gas_used(receipt.gas_used);
        if (!receipt.error_message.empty()) {
            receipt_msg->set_error_message(receipt.error_message);
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
        auto block_opt = consensus_ ? consensus_->get_block_by_height(height) : std::optional<consensus::Block>();
        
        if (!block_opt.has_value()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Block not found at height");
        }
        
        fill_block_message(block_opt.value(), response);
        
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
        std::vector<uint8_t> hash_bytes(request->hash().begin(), request->hash().end());
        if (hash_bytes.size() != crypto::Blake3Hash::HASH_SIZE) {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Invalid block hash length");
        }
        crypto::Blake3Hash hash(hash_bytes);
        auto block_opt = consensus_ ? consensus_->get_block(hash) : std::optional<consensus::Block>();
        
        if (!block_opt.has_value()) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Block not found with hash");
        }
        
        fill_block_message(block_opt.value(), response);
        
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
        if (!validators_) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Validator registry unavailable");
        }

        const auto& current_set = validators_->current_set();
        uint64_t epoch = request->epoch();
        if (epoch == 0) {
            epoch = current_set.epoch;
        }

        if (epoch != current_set.epoch) {
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Validator set not found for epoch");
        }

        response->set_epoch(current_set.epoch);
        response->set_total_stake(current_set.total_stake);
        response->set_validator_set_root(bytes_from_hash(current_set.merkle_root));

        for (const auto& validator : current_set.validators) {
            auto* validator_msg = response->add_validators();
            validator_msg->set_validator_id(bytes_from_vector(validator.id.serialize()));
            validator_msg->set_consensus_key(bytes_from_vector(validator.consensus_key.serialize()));
            validator_msg->set_bonded_stake(validator.bonded_stake);
            validator_msg->set_is_active(validator.status == consensus::ValidatorStatus::Active);
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
        if (!validators_) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "Validator registry unavailable");
        }
        uint64_t epoch = validators_->current_set().epoch;
        uint64_t epoch_start = epoch * 10000;
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
        uint64_t base_fee = fee_market_ ? fee_market_->current_base_fee() : 0;
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
        uint64_t gas_limit = request->gas_limit();
        uint64_t priority_fee = request->priority_fee();
        uint64_t base_fee = fee_market_ ? fee_market_->current_base_fee() : 0;
        uint64_t estimated_fee = fee_market_ ? fee_market_->estimate_fee(gas_limit, priority_fee) : 0;
        uint64_t total_fee = estimated_fee;
        
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
        if (!state_machine_) {
            return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "State machine unavailable");
        }
        response->set_chain_id(state_machine_->chain_id());
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
