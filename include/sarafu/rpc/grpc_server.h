#pragma once

#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "sarafu.grpc.pb.h"

namespace sarafu {
namespace rpc {

// Forward declarations
class StateMachine;
class Mempool;
class ConsensusEngine;
class ValidatorRegistry;
class FeeMarket;

/**
 * gRPC server implementing the SarafuNode service
 * Provides RPC methods for querying blockchain state and submitting transactions
 */
class GrpcServer final : public sarafu::SarafuNode::Service {
public:
    /**
     * Constructor
     * @param state_machine Pointer to the state machine for account queries
     * @param mempool Pointer to the mempool for transaction submission
     * @param consensus Pointer to the consensus engine for block queries
     * @param validators Pointer to the validator registry
     * @param fee_market Pointer to the fee market for fee estimation
     */
    GrpcServer(
        std::shared_ptr<StateMachine> state_machine,
        std::shared_ptr<Mempool> mempool,
        std::shared_ptr<ConsensusEngine> consensus,
        std::shared_ptr<ValidatorRegistry> validators,
        std::shared_ptr<FeeMarket> fee_market
    );

    ~GrpcServer() = default;

    // RPC method implementations

    /**
     * Submit a transaction to the mempool
     * Validates signature and adds to mempool if valid
     */
    grpc::Status SubmitTransaction(
        grpc::ServerContext* context,
        const sarafu::TransactionMessage* request,
        sarafu::SubmitTransactionResponse* response
    ) override;

    /**
     * Query full account information
     */
    grpc::Status GetAccount(
        grpc::ServerContext* context,
        const sarafu::GetAccountRequest* request,
        sarafu::GetAccountResponse* response
    ) override;

    /**
     * Query account balance
     */
    grpc::Status GetAccountBalance(
        grpc::ServerContext* context,
        const sarafu::GetAccountBalanceRequest* request,
        sarafu::GetAccountBalanceResponse* response
    ) override;

    /**
     * Query account nonce
     */
    grpc::Status GetAccountNonce(
        grpc::ServerContext* context,
        const sarafu::GetAccountNonceRequest* request,
        sarafu::GetAccountNonceResponse* response
    ) override;

    /**
     * Query transaction status
     */
    grpc::Status GetTransactionStatus(
        grpc::ServerContext* context,
        const sarafu::GetTransactionStatusRequest* request,
        sarafu::GetTransactionStatusResponse* response
    ) override;

    /**
     * Query transaction receipt
     */
    grpc::Status GetTransactionReceipt(
        grpc::ServerContext* context,
        const sarafu::GetTransactionReceiptRequest* request,
        sarafu::GetTransactionReceiptResponse* response
    ) override;

    /**
     * Query block by height
     */
    grpc::Status GetBlockByHeight(
        grpc::ServerContext* context,
        const sarafu::GetBlockByHeightRequest* request,
        sarafu::BlockMessage* response
    ) override;

    /**
     * Query block by hash
     */
    grpc::Status GetBlockByHash(
        grpc::ServerContext* context,
        const sarafu::GetBlockByHashRequest* request,
        sarafu::BlockMessage* response
    ) override;

    /**
     * Query validator set for a given epoch
     */
    grpc::Status GetValidatorSet(
        grpc::ServerContext* context,
        const sarafu::GetValidatorSetRequest* request,
        sarafu::ValidatorSetUpdateMessage* response
    ) override;

    /**
     * Query current epoch information
     */
    grpc::Status GetCurrentEpoch(
        grpc::ServerContext* context,
        const sarafu::Empty* request,
        sarafu::GetCurrentEpochResponse* response
    ) override;

    /**
     * Query current base fee
     */
    grpc::Status GetBaseFee(
        grpc::ServerContext* context,
        const sarafu::Empty* request,
        sarafu::GetBaseFeeResponse* response
    ) override;

    /**
     * Estimate transaction fee
     */
    grpc::Status EstimateFee(
        grpc::ServerContext* context,
        const sarafu::EstimateFeeRequest* request,
        sarafu::EstimateFeeResponse* response
    ) override;

    /**
     * Query network chain ID
     */
    grpc::Status GetChainID(
        grpc::ServerContext* context,
        const sarafu::Empty* request,
        sarafu::GetChainIDResponse* response
    ) override;

private:
    std::shared_ptr<StateMachine> state_machine_;
    std::shared_ptr<Mempool> mempool_;
    std::shared_ptr<ConsensusEngine> consensus_;
    std::shared_ptr<ValidatorRegistry> validators_;
    std::shared_ptr<FeeMarket> fee_market_;
};

/**
 * gRPC server runner
 * Manages the lifecycle of the gRPC server
 */
class GrpcServerRunner {
public:
    /**
     * Constructor
     * @param server_address Address to bind the server (e.g., "0.0.0.0:50051")
     * @param service The gRPC service implementation
     */
    GrpcServerRunner(const std::string& server_address, GrpcServer* service);

    ~GrpcServerRunner();

    /**
     * Start the gRPC server
     * Blocks until Shutdown() is called
     */
    void Run();

    /**
     * Shutdown the gRPC server gracefully
     */
    void Shutdown();

private:
    std::string server_address_;
    GrpcServer* service_;
    std::unique_ptr<grpc::Server> server_;
};

} // namespace rpc
} // namespace sarafu
