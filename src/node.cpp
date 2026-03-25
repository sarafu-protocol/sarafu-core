#include "sarafu/node.h"
#include "sarafu/storage/database.h"
#include "sarafu/consensus/genesis_builder.h"
#include "sarafu/consensus/block.h"
#include "sarafu/state/fee_market.h"
#include "sarafu/state/monetary_policy_engine.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/bls12_381.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace sarafu {

Node::Node(const config::Configuration& config)
    : config_(config),
      is_validator_(false),
      validator_id_(state::Address::zero()),
      running_(false),
      shutdown_requested_(false) {
}

Node::~Node() {
    if (running_.load()) {
        Stop();
    }
}

bool Node::Initialize() {
    LOG_INFO("Node", "Initializing Sarafu node...");

    // 1. Initialize logging
    if (!InitializeLogging()) {
        std::cerr << "Failed to initialize logging" << std::endl;
        return false;
    }

    // 2. Initialize storage
    if (!InitializeStorage()) {
        LOG_ERROR("Node", "Failed to initialize storage");
        return false;
    }

    // 3. Load or create genesis
    if (!LoadOrCreateGenesis()) {
        LOG_ERROR("Node", "Failed to load or create genesis");
        return false;
    }

    // 4. Initialize components
    if (!InitializeComponents()) {
        LOG_ERROR("Node", "Failed to initialize components");
        return false;
    }

    // 5. Load validator keys if in validator mode
    if (config_.GetValidatorConfig().is_validator) {
        if (!LoadValidatorKeys()) {
            LOG_ERROR("Node", "Failed to load validator keys");
            return false;
        }
        is_validator_ = true;
        LOG_INFO("Node", "Running in validator mode");
    } else {
        LOG_INFO("Node", "Running in full node mode");
    }

    LOG_INFO("Node", "Node initialization complete");
    return true;
}

bool Node::Start() {
    if (running_.load()) {
        LOG_WARN("Node", "Node is already running");
        return false;
    }

    LOG_INFO("Node", "Starting Sarafu node...");

    // 1. Start network layer
    if (!network_layer_->initialize()) {
        LOG_ERROR("Node", "Failed to initialize network layer");
        return false;
    }

    // 2. Connect to bootstrap peers
    const auto& bootstrap_peers = config_.GetNetworkConfig().bootstrap_peers;
    if (!bootstrap_peers.empty()) {
        LOG_INFO("Node", "Connecting to bootstrap peers...");
        size_t connected = network_layer_->connect_to_peers(bootstrap_peers);
        LOG_INFO("Node", "Connected to " + std::to_string(connected) + " bootstrap peers");
    }

    // 3. Register network message handlers
    RegisterNetworkHandlers();

    // 4. Start RPC server
    LOG_INFO("Node", "Starting RPC server...");
    rpc_server_->Start();

    // 5. Start background tasks
    StartBackgroundTasks();

    running_.store(true);
    LOG_INFO("Node", "Node started successfully");

    return true;
}

void Node::Stop() {
    if (!running_.load()) {
        return;
    }

    LOG_INFO("Node", "Stopping Sarafu node...");
    shutdown_requested_.store(true);

    // 1. Stop background tasks
    StopBackgroundTasks();

    // 2. Stop RPC server
    LOG_INFO("Node", "Stopping RPC server...");
    rpc_server_->Stop();

    // 3. Shutdown network layer
    LOG_INFO("Node", "Shutting down network layer...");
    network_layer_->shutdown();

    // 4. Database will be flushed automatically in destructor

    running_.store(false);
    LOG_INFO("Node", "Node stopped");
}

bool Node::InitializeLogging() {
    const auto& log_config = config_.GetLogConfig();

    // Set log level
    logging::LogLevel level = logging::LogLevel::INFO;
    if (log_config.log_level == "DEBUG") {
        level = logging::LogLevel::DEBUG;
    } else if (log_config.log_level == "INFO") {
        level = logging::LogLevel::INFO;
    } else if (log_config.log_level == "WARN") {
        level = logging::LogLevel::WARN;
    } else if (log_config.log_level == "ERROR") {
        level = logging::LogLevel::ERROR;
    }

    logging::Logger::instance().set_level(level);

    // Set log file if specified
    if (!log_config.log_file.empty()) {
        // Logger constructor with file path will be used
        // For now, we use the singleton which logs to stdout
        LOG_INFO("Node", "Logging to: " + log_config.log_file);
    }

    return true;
}

bool Node::InitializeStorage() {
    const auto& storage_config = config_.GetStorageConfig();

    LOG_INFO("Node", "Opening database at: " + storage_config.data_directory);

    // Open database using factory method
    auto db_result = storage::Database::open(storage_config.data_directory);
    if (!db_result.is_ok()) {
        LOG_ERROR("Node", "Failed to open database: " + db_result.error());
        return false;
    }

    database_ = std::shared_ptr<storage::Database>(std::move(db_result.value()));

    // Create state storage wrapper
    storage_ = std::make_shared<storage::StateStorage>(database_);

    LOG_INFO("Node", "Database opened successfully");
    return true;
}

bool Node::LoadOrCreateGenesis() {
    const auto& genesis_config = config_.GetGenesisConfig();
    const std::string& genesis_file = genesis_config.genesis_file_path;

    LOG_INFO("Node", "Loading genesis from: " + genesis_file);

    // Check if genesis block already exists in database
    auto genesis_result = storage_->get_block_by_height(0);
    if (genesis_result.is_ok()) {
        LOG_INFO("Node", "Genesis block already exists in database");
        return true;
    }

    // Genesis doesn't exist - for now, we'll just mark that we need to initialize
    // accounts from the genesis file in InitializeComponents
    LOG_INFO("Node", "Genesis block will be created during component initialization");
    
    return true;
}

bool Node::InitializeComponents() {
    LOG_INFO("Node", "Initializing components...");

    // 1. Create state machine
    state_machine_ = std::make_shared<state::StateMachine>(config_.GetChainId());

    // Load genesis allocations from file and initialize accounts directly
    const auto& genesis_config = config_.GetGenesisConfig();
    const std::string& genesis_file = genesis_config.genesis_file_path;
    
    std::ifstream file(genesis_file);
    if (file.is_open()) {
        try {
            nlohmann::json genesis_json;
            file >> genesis_json;
            
            if (genesis_json.contains("initial_accounts")) {
                LOG_INFO("Node", "Initializing accounts from genesis file...");
                
                for (const auto& account : genesis_json["initial_accounts"]) {
                    std::string address_str = account["address"].get<std::string>();
                    std::string balance_str = account["balance"].get<std::string>();
                    
                    // Remove 0x prefix if present
                    if (address_str.substr(0, 2) == "0x") {
                        address_str = address_str.substr(2);
                    }
                    
                    // Parse address
                    if (address_str.length() == 64) {
                        state::Address::AddressArray addr_data;
                        for (size_t i = 0; i < 32; ++i) {
                            addr_data[i] = static_cast<uint8_t>(
                                std::stoi(address_str.substr(i * 2, 2), nullptr, 16)
                            );
                        }
                        state::Address addr(addr_data);
                        
                        uint64_t balance = std::stoull(balance_str);
                        
                        // Create account directly in state machine
                        state_machine_->get_account_manager().create_account(addr, balance);
                        
                        LOG_INFO("Node", "Initialized account: " + address_str + " with balance: " + balance_str);
                    }
                }
                
                LOG_INFO("Node", "Genesis accounts initialized successfully");
            }
        } catch (const std::exception& e) {
            LOG_WARN("Node", "Failed to load genesis allocations: " + std::string(e.what()));
        }
    } else {
        LOG_WARN("Node", "Could not open genesis file for account initialization");
    }

    // 2. Create validator registry
    consensus::ValidatorRegistry::Config validator_config;
    validator_config.blocks_per_epoch = config_.GetConsensusConfig().epoch_length;
    validator_registry_ = std::make_shared<consensus::ValidatorRegistry>(validator_config);

    // 3. Create consensus engine
    consensus::ConsensusEngine::Config consensus_config;
    consensus_config.block_time_ms = config_.GetConsensusConfig().block_time_ms;
    consensus_config.view_timeout_ms = config_.GetConsensusConfig().view_change_timeout_ms;
    consensus_engine_ = std::make_shared<consensus::ConsensusEngine>(
        validator_registry_,
        state_machine_,
        consensus_config
    );
    consensus_engine_->set_network_layer(network_layer_);

    // 3a. Ensure genesis block exists in persistent storage and in the consensus engine
    auto genesis_in_storage = storage_->get_block_by_height(0);
    if (genesis_in_storage.is_ok()) {
        // Database already has a genesis block – trust it as canonical
        const auto& genesis_block = genesis_in_storage.value();
        consensus_engine_->set_genesis_block(genesis_block);
        LOG_INFO("Node", "Loaded existing genesis block from storage");
    } else {
        // No persisted genesis block yet – construct a minimal one from current state
        LOG_INFO("Node", "No genesis block found in storage; creating new genesis block");

        consensus::BlockHeader header;
        header.height = 0;
        header.timestamp = static_cast<uint64_t>(std::time(nullptr));
        header.previous_hash = crypto::Blake3Hash::zero();
        header.proposer = state::Address::zero();
        header.epoch = 0;

        // Empty transaction set for genesis
        std::vector<crypto::Blake3Hash> tx_hashes;
        crypto::MerkleTree tx_tree;
        tx_tree.build_tree(tx_hashes);
        header.transactions_root = tx_tree.get_root();

        // State root based on the initialized accounts
        header.state_root = state_machine_->compute_state_root();

        // Validator set root from the current validator registry
        header.validator_set_root = validator_registry_->compute_validator_set_root(
            validator_registry_->current_set()
        );

        // Empty QC for genesis (no parent to justify)
        consensus::QuorumCertificate genesis_qc(
            0,
            crypto::Blake3Hash::zero(),
            0,
            crypto::BLS12_381_Signature(),
            std::vector<consensus::ValidatorID>(),
            0
        );

        consensus::Block genesis_block(header, {}, genesis_qc);

        auto store_result = storage_->store_block(genesis_block);
        if (!store_result.is_ok()) {
            LOG_ERROR("Node", "Failed to store genesis block: " + store_result.error());
            return false;
        }

        consensus_engine_->set_genesis_block(genesis_block);
        LOG_INFO("Node", "Created and stored new genesis block");
    }

    // 4. Create mempool
    state::Mempool::Config mempool_config;
    mempool_ = std::make_shared<state::Mempool>(mempool_config);
    mempool_->set_account_manager(&state_machine_->get_account_manager());

    // 5. Create network layer
    network::NetworkConfig network_config;
    network_config.listen_address = config_.GetNetworkConfig().listen_address;
    network_config.bootstrap_peers = config_.GetNetworkConfig().bootstrap_peers;
    network_config.min_peers = config_.GetNetworkConfig().min_peers;
    network_config.max_peers = config_.GetNetworkConfig().max_peers;
    network_config.gossip_fanout = config_.GetNetworkConfig().gossip_fanout;
    network_layer_ = std::make_shared<network::NetworkLayer>(network_config);

    // 6. Create RPC server
    rpc::RpcServerConfig rpc_config;
    rpc_config.grpc_address = config_.GetRpcConfig().grpc_address;
    rpc_config.rest_address = config_.GetRpcConfig().rest_address;
    rpc_config.websocket_address = config_.GetRpcConfig().websocket_address;
    rpc_config.enable_grpc = config_.GetRpcConfig().enable_grpc;
    rpc_config.enable_rest = config_.GetRpcConfig().enable_rest;
    rpc_config.enable_websocket = config_.GetRpcConfig().enable_websocket;
    rpc_config.rest_tls.enabled = config_.GetRpcConfig().enable_rest_tls;
    rpc_config.rest_tls.cert_path = config_.GetRpcConfig().tls_cert_path;
    rpc_config.rest_tls.key_path = config_.GetRpcConfig().tls_key_path;
    rpc_config.rest_tls.ca_path = config_.GetRpcConfig().tls_ca_path;
    rpc_config.rest_tls.require_client_auth = config_.GetRpcConfig().tls_require_client_auth;
    rpc_config.websocket_tls.enabled = config_.GetRpcConfig().enable_websocket_tls;
    rpc_config.websocket_tls.cert_path = config_.GetRpcConfig().tls_cert_path;
    rpc_config.websocket_tls.key_path = config_.GetRpcConfig().tls_key_path;
    rpc_config.websocket_tls.ca_path = config_.GetRpcConfig().tls_ca_path;
    rpc_config.websocket_tls.require_client_auth = config_.GetRpcConfig().tls_require_client_auth;
    rpc_config.enable_rate_limiting = config_.GetRpcConfig().enable_rate_limiting;
    rpc_config.max_requests_per_minute = config_.GetRpcConfig().max_requests_per_minute;
    rpc_config.enable_authentication = config_.GetRpcConfig().enable_authentication;
    rpc_config.chain_id = config_.GetChainId();

    // Create fee market and monetary policy for RPC
    fee_market_ = std::make_shared<state::FeeMarket>();
    auto fee_market = fee_market_;
    
    // Create monetary policy with initial values (would come from genesis)
    uint64_t initial_supply = 1000000000;  // 1 billion tokens
    uint64_t initial_bonded_stake = 0;     // No stake initially
    auto monetary_policy = std::make_shared<state::MonetaryPolicyEngine>(
        initial_supply,
        initial_bonded_stake
    );

    rpc_server_ = std::make_shared<rpc::RpcServer>(
        rpc_config,
        state_machine_,
        mempool_,
        consensus_engine_,
        validator_registry_,
        fee_market
    );

    LOG_INFO("Node", "All components initialized");
    return true;
}

bool Node::LoadValidatorKeys() {
    const auto& validator_config = config_.GetValidatorConfig();

    LOG_INFO("Node", "Loading validator keys...");

    // Load consensus key (BLS12-381)
    const std::string& consensus_key_path = validator_config.consensus_key_path;
    if (consensus_key_path.empty()) {
        LOG_ERROR("Node", "Consensus key path not specified");
        return false;
    }

    std::ifstream consensus_file(consensus_key_path, std::ios::binary);
    if (!consensus_file.is_open()) {
        LOG_ERROR("Node", "Failed to open consensus key file: " + consensus_key_path);
        return false;
    }

    // Read consensus private key
    std::vector<uint8_t> consensus_key_data(
        (std::istreambuf_iterator<char>(consensus_file)),
        std::istreambuf_iterator<char>()
    );
    consensus_file.close();

    if (consensus_key_data.size() != 32) {
        LOG_ERROR("Node", "Invalid consensus key size");
        return false;
    }

    // Convert vector to array
    crypto::BLS12_381_PrivateKey::KeyArray consensus_key_array;
    std::copy(consensus_key_data.begin(), consensus_key_data.end(), consensus_key_array.begin());
    consensus_private_key_ = crypto::BLS12_381_PrivateKey(consensus_key_array);

    // Derive validator ID from consensus public key
    auto consensus_public_key = consensus_private_key_.public_key();
    // Validator ID is the hash of the public key
    auto pubkey_bytes = consensus_public_key.serialize();
    crypto::Blake3Hash pubkey_hash = crypto::Blake3Hash::hash(pubkey_bytes);
    validator_id_ = state::Address(pubkey_hash.data());

    // Load withdrawal key (Ed25519)
    const std::string& withdrawal_key_path = validator_config.withdrawal_key_path;
    if (withdrawal_key_path.empty()) {
        LOG_ERROR("Node", "Withdrawal key path not specified");
        return false;
    }

    std::ifstream withdrawal_file(withdrawal_key_path, std::ios::binary);
    if (!withdrawal_file.is_open()) {
        LOG_ERROR("Node", "Failed to open withdrawal key file: " + withdrawal_key_path);
        return false;
    }

    // Read withdrawal private key
    std::vector<uint8_t> withdrawal_key_data(
        (std::istreambuf_iterator<char>(withdrawal_file)),
        std::istreambuf_iterator<char>()
    );
    withdrawal_file.close();

    if (withdrawal_key_data.size() != 32) {
        LOG_ERROR("Node", "Invalid withdrawal key size");
        return false;
    }

    // Convert vector to array
    crypto::Ed25519_PrivateKey::KeyArray withdrawal_key_array;
    std::copy(withdrawal_key_data.begin(), withdrawal_key_data.end(), withdrawal_key_array.begin());
    withdrawal_private_key_ = crypto::Ed25519_PrivateKey(withdrawal_key_array);

    LOG_INFO("Node", "Validator keys loaded successfully");
    LOG_INFO("Node", "Validator ID: " + validator_id_.to_hex());

    // Register validator in the validator registry if not already registered
    auto validator = validator_registry_->get_validator(validator_id_);
    if (!validator.has_value()) {
        LOG_INFO("Node", "Registering validator in registry...");
        
        auto consensus_pubkey = consensus_private_key_.public_key();
        auto withdrawal_pubkey = withdrawal_private_key_.public_key();
        
        // Register with initial stake (would come from configuration)
        uint64_t initial_stake = 100000;  // Minimum self-bond
        
        if (!validator_registry_->add_validator(
            validator_id_,
            consensus_pubkey,
            withdrawal_pubkey,
            initial_stake
        )) {
            LOG_ERROR("Node", "Failed to register validator");
            return false;
        }
        
        LOG_INFO("Node", "Validator registered successfully");
    } else {
        LOG_INFO("Node", "Validator already registered in registry");
    }

    return true;
}

void Node::RegisterNetworkHandlers() {
    LOG_INFO("Node", "Registering network message handlers...");

    // Register transaction handler
    network_layer_->on_message(
        network::MessageType::Transaction,
        [this](const network::NetworkMessage& msg, const network::PeerID& sender) {
            HandleTransaction(msg, sender);
        }
    );

    // Register block handler
    network_layer_->on_message(
        network::MessageType::Block,
        [this](const network::NetworkMessage& msg, const network::PeerID& sender) {
            HandleBlock(msg, sender);
        }
    );

    // Register vote handler
    network_layer_->on_message(
        network::MessageType::Vote,
        [this](const network::NetworkMessage& msg, const network::PeerID& sender) {
            HandleVote(msg, sender);
        }
    );

    LOG_INFO("Node", "Network message handlers registered");
}

void Node::StartBackgroundTasks() {
    LOG_INFO("Node", "Starting background tasks...");

    // Start mempool pruning task
    background_threads_.emplace_back([this]() {
        MempoolPruningTask();
    });

    // Start connection maintenance task
    background_threads_.emplace_back([this]() {
        ConnectionMaintenanceTask();
    });

    // Start block proposal task if validator
    if (is_validator_) {
        background_threads_.emplace_back([this]() {
            BlockProposalTask();
        });
    }

    LOG_INFO("Node", "Background tasks started");
}

void Node::StopBackgroundTasks() {
    LOG_INFO("Node", "Stopping background tasks...");

    // Wait for all background threads to finish
    for (auto& thread : background_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    background_threads_.clear();
    LOG_INFO("Node", "Background tasks stopped");
}

void Node::MempoolPruningTask() {
    while (!shutdown_requested_.load()) {
        // Prune old transactions every 60 seconds
        std::this_thread::sleep_for(std::chrono::seconds(60));

        if (shutdown_requested_.load()) {
            break;
        }

        LOG_DEBUG("Node", "Pruning old transactions from mempool");
        mempool_->prune_old_transactions();
    }
}

void Node::ConnectionMaintenanceTask() {
    while (!shutdown_requested_.load()) {
        // Maintain connections every 30 seconds
        std::this_thread::sleep_for(std::chrono::seconds(30));

        if (shutdown_requested_.load()) {
            break;
        }

        LOG_DEBUG("Node", "Maintaining network connections");
        network_layer_->maintain_connections();
    }
}

void Node::BlockProposalTask() {
    while (!shutdown_requested_.load()) {
        // Check if we should propose a block
        const uint64_t block_time_ms = config_.GetConsensusConfig().block_time_ms;
        std::this_thread::sleep_for(std::chrono::milliseconds(block_time_ms));

        if (shutdown_requested_.load()) {
            break;
        }

        // Check if we are the current leader
        auto leader = consensus_engine_->get_current_leader();
        if (!leader.has_value()) {
            continue;
        }

        if (leader.value() == validator_id_) {
            // We are the leader - propose a block
            LOG_DEBUG("Node", "Proposing block as leader");

            // Get current base fee from fee market
            uint64_t base_fee = fee_market_ ? fee_market_->current_base_fee() : 1000;

            // Propose block
            auto block = consensus_engine_->propose_block(
                validator_id_,
                consensus_private_key_,
                *mempool_,
                base_fee
            );

            if (block.has_value()) {
                LOG_INFO("Node", "Proposed block at height " + std::to_string(block->header.height));

                // Store block locally
                consensus_engine_->store_block(block.value());

                // Broadcast block to network
                network::NetworkMessage msg(
                    network::MessageType::Block,
                    block->serialize()
                );
                network_layer_->broadcast(msg);

                // Record our signature
                validator_registry_->record_signature(validator_id_, block->header.height);
            } else {
                LOG_WARN("Node", "Failed to propose block");
            }
        } else {
            // We are not the leader - wait for block and vote
            LOG_DEBUG("Node", "Waiting for block from leader: " + leader.value().to_hex());
        }
    }
}

void Node::HandleTransaction(const network::NetworkMessage& message, const network::PeerID& sender) {
    LOG_DEBUG("Node", "Received transaction from peer: " + sender);

    try {
        // Deserialize transaction from network payload
        state::Transaction tx = state::Transaction::deserialize(message.payload);

        // Add to mempool (mempool performs its own validation)
        bool accepted = mempool_->add_transaction(tx);
        if (accepted) {
            LOG_DEBUG("Node", "Transaction accepted into mempool: " + tx.hash().to_hex());
        } else {
            LOG_DEBUG("Node", "Transaction rejected by mempool");
        }
    } catch (const std::exception& e) {
        LOG_WARN("Node", "Failed to deserialize transaction from " + sender + ": " + e.what());
    }
}

void Node::HandleBlock(const network::NetworkMessage& message, const network::PeerID& sender) {
    LOG_DEBUG("Node", "Received block from peer: " + sender);

    try {
        // Deserialize block from network payload
        consensus::Block block = consensus::Block::deserialize(message.payload);

        // Process block with consensus engine (validates and stores)
        bool valid = consensus_engine_->on_receive_block(block);
        if (!valid) {
            LOG_WARN("Node", "Received invalid block at height " + std::to_string(block.header.height));
            return;
        }

        LOG_INFO("Node", "Accepted block at height " + std::to_string(block.header.height));

        // If we are a validator, create and broadcast a vote
        if (is_validator_) {
            consensus::Vote vote = CreateVote(block);

            // Broadcast vote to network
            network::NetworkMessage vote_msg(
                network::MessageType::Vote,
                vote.serialize()
            );
            network_layer_->broadcast(vote_msg);

            // Record our signature
            validator_registry_->record_signature(validator_id_, block.header.height);

            LOG_DEBUG("Node", "Created and broadcast vote for block at height " + std::to_string(block.header.height));
        }
    } catch (const std::exception& e) {
        LOG_WARN("Node", "Failed to process block from " + sender + ": " + e.what());
    }
}

void Node::HandleVote(const network::NetworkMessage& message, const network::PeerID& sender) {
    LOG_DEBUG("Node", "Received vote from peer: " + sender);

    try {
        // Deserialize vote from network payload (176-byte format)
        consensus::Vote vote = consensus::Vote::deserialize(message.payload);

        // Process vote with consensus engine
        auto qc = consensus_engine_->on_receive_vote(vote);

        // If a QC was created (≥2/3 stake reached), mark block as finalized
        if (qc.has_value()) {
            LOG_INFO("Node", "Quorum Certificate created for block at height " +
                     std::to_string(qc->block_height));

            // Mark block as finalized
            consensus_engine_->mark_finalized(qc->block_hash, qc.value());
        }
    } catch (const std::exception& e) {
        LOG_WARN("Node", "Failed to process vote from " + sender + ": " + e.what());
    }
}

consensus::Vote Node::CreateVote(const consensus::Block& block) {
    if (!is_validator_) {
        throw std::runtime_error("Cannot create vote: not in validator mode");
    }

    // Create vote structure
    consensus::Vote vote;
    vote.validator_id = validator_id_;
    vote.block_height = block.header.height;
    vote.block_hash = block.hash();
    vote.view_number = consensus_engine_->current_view();

    // Sign the vote with BLS private key
    // The signature is over the block hash
    auto block_hash_bytes = block.hash().data();
    vote.signature = crypto::BLS12_381::sign(
        block_hash_bytes.data(),
        block_hash_bytes.size(),
        consensus_private_key_
    );

    LOG_DEBUG("Node", "Created vote for block at height " + std::to_string(block.header.height));

    return vote;
}

} // namespace sarafu
