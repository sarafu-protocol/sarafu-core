#pragma once

#include "sarafu/config/configuration.h"
#include "sarafu/storage/state_storage.h"
#include "sarafu/state/state_machine.h"
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/consensus/consensus_engine.h"
#include "sarafu/state/mempool.h"
#include "sarafu/network/network_layer.h"
#include "sarafu/rpc/rpc_server.h"
#include "sarafu/logging/logger.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"
#include <memory>
#include <atomic>
#include <thread>

namespace sarafu {

/**
 * Node is the main integration class for the Sarafu blockchain.
 * 
 * It integrates all components:
 * - Storage: Persistent state storage using RocksDB
 * - StateMachine: Transaction execution and account management
 * - ValidatorRegistry: Validator set management and epoch transitions
 * - ConsensusEngine: HotStuff BFT consensus protocol
 * - Mempool: Pending transaction management
 * - NetworkLayer: P2P networking with libp2p
 * - RpcServer: gRPC, REST, and WebSocket interfaces
 * 
 * The Node class provides lifecycle management:
 * - Initialize all components with configuration
 * - Start all services (networking, consensus, RPC)
 * - Handle graceful shutdown on signals (SIGINT, SIGTERM)
 * 
 * Requirements: 25.1, 25.2, 25.3, 25.4
 */
class Node {
public:
    /**
     * Construct a Node with the given configuration.
     * 
     * @param config The node configuration
     */
    explicit Node(const config::Configuration& config);

    /**
     * Destructor - ensures clean shutdown.
     */
    ~Node();

    // Disable copy and move
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    /**
     * Initialize the node.
     * 
     * This method:
     * 1. Initializes logging
     * 2. Opens the database
     * 3. Loads or creates genesis block
     * 4. Initializes all components
     * 5. Loads validator keys if in validator mode
     * 
     * @return true if initialization succeeded, false otherwise
     */
    bool Initialize();

    /**
     * Start the node.
     * 
     * This method:
     * 1. Starts the network layer
     * 2. Connects to bootstrap peers
     * 3. Starts the consensus engine
     * 4. Starts the RPC server
     * 5. Starts background tasks (mempool pruning, connection maintenance)
     * 
     * @return true if start succeeded, false otherwise
     */
    bool Start();

    /**
     * Stop the node gracefully.
     * 
     * This method:
     * 1. Stops accepting new RPC requests
     * 2. Stops the consensus engine
     * 3. Disconnects from all peers
     * 4. Flushes pending database writes
     * 5. Shuts down all services
     */
    void Stop();

    /**
     * Check if the node is running.
     * 
     * @return true if running, false otherwise
     */
    bool IsRunning() const { return running_.load(); }

    /**
     * Check if the node is in validator mode.
     * 
     * @return true if validator, false otherwise
     */
    bool IsValidator() const { return is_validator_; }

    /**
     * Get the validator ID (only valid if IsValidator() is true).
     * 
     * @return The validator ID
     */
    const consensus::ValidatorID& GetValidatorId() const { return validator_id_; }

    /**
     * Get the node configuration.
     * 
     * @return Reference to the configuration
     */
    const config::Configuration& GetConfig() const { return config_; }

    /**
     * Get the state machine.
     * 
     * @return Shared pointer to the state machine
     */
    std::shared_ptr<state::StateMachine> GetStateMachine() const { return state_machine_; }

    /**
     * Get the consensus engine.
     * 
     * @return Shared pointer to the consensus engine
     */
    std::shared_ptr<consensus::ConsensusEngine> GetConsensusEngine() const { return consensus_engine_; }

    /**
     * Get the validator registry.
     * 
     * @return Shared pointer to the validator registry
     */
    std::shared_ptr<consensus::ValidatorRegistry> GetValidatorRegistry() const { return validator_registry_; }

    /**
     * Get the mempool.
     * 
     * @return Shared pointer to the mempool
     */
    std::shared_ptr<state::Mempool> GetMempool() const { return mempool_; }

    /**
     * Get the network layer.
     * 
     * @return Shared pointer to the network layer
     */
    std::shared_ptr<network::NetworkLayer> GetNetworkLayer() const { return network_layer_; }

    /**
     * Get the RPC server.
     * 
     * @return Shared pointer to the RPC server
     */
    std::shared_ptr<rpc::RpcServer> GetRpcServer() const { return rpc_server_; }

private:
    /**
     * Initialize logging from configuration.
     * 
     * @return true if successful, false otherwise
     */
    bool InitializeLogging();

    /**
     * Initialize storage (open database).
     * 
     * @return true if successful, false otherwise
     */
    bool InitializeStorage();

    /**
     * Load or create the genesis block.
     * 
     * @return true if successful, false otherwise
     */
    bool LoadOrCreateGenesis();

    /**
     * Initialize all components.
     * 
     * @return true if successful, false otherwise
     */
    bool InitializeComponents();

    /**
     * Load validator keys if in validator mode.
     * 
     * Requirements: 15.1, 15.2
     * 
     * @return true if successful, false otherwise
     */
    bool LoadValidatorKeys();

    /**
     * Register network message handlers.
     */
    void RegisterNetworkHandlers();

    /**
     * Start background tasks (mempool pruning, connection maintenance).
     */
    void StartBackgroundTasks();

    /**
     * Stop background tasks.
     */
    void StopBackgroundTasks();

    /**
     * Background task: Prune old transactions from mempool.
     */
    void MempoolPruningTask();

    /**
     * Background task: Maintain network connections.
     */
    void ConnectionMaintenanceTask();

    /**
     * Background task: Propose blocks if validator and leader.
     */
    void BlockProposalTask();

    /**
     * Handle received transaction from network.
     * 
     * @param message The network message
     * @param sender The sender peer ID
     */
    void HandleTransaction(const network::NetworkMessage& message, const network::PeerID& sender);

    /**
     * Handle received block from network.
     * 
     * @param message The network message
     * @param sender The sender peer ID
     */
    void HandleBlock(const network::NetworkMessage& message, const network::PeerID& sender);

    /**
     * Handle received vote from network.
     * 
     * @param message The network message
     * @param sender The sender peer ID
     */
    void HandleVote(const network::NetworkMessage& message, const network::PeerID& sender);

    /**
     * Create a vote for a block (validator only).
     * 
     * Requirements: 15.1, 15.2
     * 
     * @param block The block to vote for
     * @return The created vote
     */
    consensus::Vote CreateVote(const consensus::Block& block);

    // Configuration
    config::Configuration config_;

    // Core components
    std::shared_ptr<storage::Database> database_;
    std::shared_ptr<storage::StateStorage> storage_;
    std::shared_ptr<state::StateMachine> state_machine_;
    std::shared_ptr<consensus::ValidatorRegistry> validator_registry_;
    std::shared_ptr<consensus::ConsensusEngine> consensus_engine_;
    std::shared_ptr<state::Mempool> mempool_;
    std::shared_ptr<network::NetworkLayer> network_layer_;
    std::shared_ptr<rpc::RpcServer> rpc_server_;

    // Validator mode
    bool is_validator_;
    consensus::ValidatorID validator_id_;
    crypto::BLS12_381_PrivateKey consensus_private_key_;
    crypto::Ed25519_PrivateKey withdrawal_private_key_;

    // State
    std::atomic<bool> running_;
    std::atomic<bool> shutdown_requested_;

    // Background task threads
    std::vector<std::thread> background_threads_;
};

} // namespace sarafu
