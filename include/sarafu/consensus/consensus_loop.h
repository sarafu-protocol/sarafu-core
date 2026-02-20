#pragma once

#include "sarafu/consensus/consensus_engine.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/network/network_layer.h"
#include "sarafu/state/mempool.h"
#include "sarafu/state/fee_market.h"
#include "sarafu/crypto/bls12_381.h"
#include <atomic>
#include <memory>
#include <thread>

namespace sarafu {
namespace consensus {

/**
 * ConsensusLoop manages the main consensus protocol execution.
 * 
 * This class coordinates:
 * - Block proposal (for validators)
 * - Block reception and validation
 * - Vote creation and aggregation
 * - View changes on timeout
 * - Network message handling
 * 
 * The consensus loop runs in a separate thread and handles all
 * consensus-related network messages and timing.
 */
class ConsensusLoop {
public:
    /**
     * Construct a ConsensusLoop.
     * 
     * @param consensus_engine The consensus engine
     * @param network_layer The network layer for message passing
     * @param mempool The mempool for transaction selection
     * @param fee_market The fee market for base fee calculation
     * @param validator_id This node's validator ID (if validator)
     * @param validator_key This node's BLS private key (if validator)
     * @param is_validator Whether this node is a validator
     */
    ConsensusLoop(
        std::shared_ptr<ConsensusEngine> consensus_engine,
        std::shared_ptr<network::NetworkLayer> network_layer,
        std::shared_ptr<state::Mempool> mempool,
        std::shared_ptr<state::FeeMarket> fee_market,
        const ValidatorID& validator_id,
        const crypto::BLS12_381_PrivateKey& validator_key,
        bool is_validator
    );
    
    /**
     * Start the consensus loop.
     * 
     * This starts a background thread that:
     * - Proposes blocks when this node is the leader
     * - Processes received blocks and votes
     * - Triggers view changes on timeout
     */
    void start();
    
    /**
     * Stop the consensus loop.
     * 
     * This stops the background thread and cleans up resources.
     */
    void stop();
    
    /**
     * Check if the consensus loop is running.
     * 
     * @return true if running, false otherwise
     */
    bool is_running() const { return running_.load(); }

private:
    /**
     * Register network message handlers.
     */
    void register_message_handlers();
    
    /**
     * Main consensus loop that runs in a background thread.
     */
    void run_consensus_loop();
    
    /**
     * Propose a new block (called when this node is the leader).
     */
    void propose_block();
    
    /**
     * Check if view timeout has elapsed and trigger view change if needed.
     */
    void check_view_timeout();
    
    /**
     * Handle received block message.
     * 
     * @param msg The network message
     * @param sender The peer ID of the sender
     */
    void handle_block_message(
        const network::NetworkMessage& msg,
        const network::PeerID& sender
    );
    
    /**
     * Handle received vote message.
     * 
     * @param msg The network message
     * @param sender The peer ID of the sender
     */
    void handle_vote_message(
        const network::NetworkMessage& msg,
        const network::PeerID& sender
    );
    
    /**
     * Handle received transaction message.
     * 
     * @param msg The network message
     * @param sender The peer ID of the sender
     */
    void handle_transaction_message(
        const network::NetworkMessage& msg,
        const network::PeerID& sender
    );
    
    /**
     * Handle received block request message.
     * 
     * @param msg The network message
     * @param sender The peer ID of the sender
     */
    void handle_block_request_message(
        const network::NetworkMessage& msg,
        const network::PeerID& sender
    );
    
    std::shared_ptr<ConsensusEngine> consensus_engine_;
    std::shared_ptr<network::NetworkLayer> network_layer_;
    std::shared_ptr<state::Mempool> mempool_;
    std::shared_ptr<state::FeeMarket> fee_market_;
    ValidatorID validator_id_;
    crypto::BLS12_381_PrivateKey validator_key_;
    bool is_validator_;
    
    std::atomic<bool> running_;
    std::thread consensus_thread_;
    uint64_t last_block_time_;
};

} // namespace consensus
} // namespace sarafu
