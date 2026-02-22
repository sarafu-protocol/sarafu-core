#include "sarafu/consensus/consensus_engine.h"
#include "sarafu/network/network_layer.h"
#include "sarafu/network/message_handler.h"
#include "sarafu/state/mempool.h"
#include "sarafu/state/fee_market.h"
#include "sarafu/logging/logger.h"
#include <chrono>
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
 */
class ConsensusLoop {
public:
    ConsensusLoop(
        std::shared_ptr<ConsensusEngine> consensus_engine,
        std::shared_ptr<network::NetworkLayer> network_layer,
        std::shared_ptr<state::Mempool> mempool,
        std::shared_ptr<state::FeeMarket> fee_market,
        const ValidatorID& validator_id,
        const crypto::BLS12_381_PrivateKey& validator_key,
        bool is_validator
    )
        : consensus_engine_(consensus_engine),
          network_layer_(network_layer),
          mempool_(mempool),
          fee_market_(fee_market),
          validator_id_(validator_id),
          validator_key_(validator_key),
          is_validator_(is_validator),
          running_(false),
          last_block_time_(0) {}
    
    void start() {
        if (running_.load()) {
            return;
        }
        
        running_.store(true);
        
        // Register network message handlers
        register_message_handlers();
        
        // Start consensus loop thread
        consensus_thread_ = std::thread([this]() {
            run_consensus_loop();
        });
        
        LOG_INFO("ConsensusLoop", "Consensus loop started");
    }
    
    void stop() {
        if (!running_.load()) {
            return;
        }
        
        running_.store(false);
        
        if (consensus_thread_.joinable()) {
            consensus_thread_.join();
        }
        
        LOG_INFO("ConsensusLoop", "Consensus loop stopped");
    }
    
private:
    void register_message_handlers() {
        // Register block handler
        network_layer_->on_message(
            network::MessageType::Block,
            [this](const network::NetworkMessage& msg, const network::PeerID& sender) {
                handle_block_message(msg, sender);
            }
        );
        
        // Register vote handler
        network_layer_->on_message(
            network::MessageType::Vote,
            [this](const network::NetworkMessage& msg, const network::PeerID& sender) {
                handle_vote_message(msg, sender);
            }
        );
        
        // Register transaction handler
        network_layer_->on_message(
            network::MessageType::Transaction,
            [this](const network::NetworkMessage& msg, const network::PeerID& sender) {
                handle_transaction_message(msg, sender);
            }
        );
        
        // Register block request handler
        network_layer_->on_message(
            network::MessageType::BlockRequest,
            [this](const network::NetworkMessage& msg, const network::PeerID& sender) {
                handle_block_request_message(msg, sender);
            }
        );
    }
    
    void run_consensus_loop() {
        while (running_.load()) {
            try {
                // Check if we're the current leader
                auto leader = consensus_engine_->get_current_leader();
                bool is_leader = leader.has_value() && 
                                is_validator_ && 
                                *leader == validator_id_;
                
                if (is_leader) {
                    // Propose a new block
                    propose_block();
                }
                
                // Check for view timeout
                check_view_timeout();
                
                // Sleep for a short interval
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
            } catch (const std::exception& e) {
                LOG_ERROR("ConsensusLoop", "Error in consensus loop: " + std::string(e.what()));
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
    }
    
    void propose_block() {
        // Get current time
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // Check if enough time has passed since last block
        uint64_t block_time_ms = 2000;  // 2 seconds
        if (now - last_block_time_ < block_time_ms) {
            return;  // Too soon to propose
        }
        
        // Get current base fee
        uint64_t base_fee = fee_market_->current_base_fee();
        
        // Propose block
        auto block_opt = consensus_engine_->propose_block(
            validator_id_,
            validator_key_,
            *mempool_,
            base_fee
        );
        
        if (block_opt.has_value()) {
            Block block = *block_opt;
            
            // Broadcast block to network
            std::vector<uint8_t> block_data = block.serialize();
            network::NetworkMessage msg(network::MessageType::Block, block_data);
            network_layer_->broadcast(msg);
            
            last_block_time_ = now;
            
            LOG_INFO("ConsensusLoop", 
                "Proposed block at height " + std::to_string(block.header.height));
        }
    }
    
    void check_view_timeout() {
        // Get current time
        uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // Check if view timeout has elapsed
        uint64_t view_timeout_ms = 4000;  // 4 seconds
        if (now - last_block_time_ > view_timeout_ms) {
            // Trigger view change
            if (consensus_engine_->trigger_view_change()) {
                LOG_INFO("ConsensusLoop", "View change triggered");
                last_block_time_ = now;
            }
        }
    }
    
    void handle_block_message(
        const network::NetworkMessage& msg,
        const network::PeerID& sender
    ) {
        try {
            // Deserialize block
            Block block = Block::deserialize(msg.payload);
            
            LOG_INFO("ConsensusLoop", 
                "Received block at height " + std::to_string(block.header.height) + 
                " from " + sender);
            
            // Process block through consensus engine
            bool valid = consensus_engine_->on_receive_block(block);
            
            if (valid) {
                // Update peer reputation positively
                network_layer_->update_reputation(sender, 5);
                
                // Update last block time
                last_block_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
                
                // If we're a validator, create and broadcast vote
                if (is_validator_) {
                    // Vote is created and broadcast by consensus engine
                    // We just need to broadcast it to the network
                    // This is handled internally by on_receive_block
                }
            } else {
                // Invalid block, penalize sender
                network_layer_->update_reputation(sender, -10);
                LOG_WARN("ConsensusLoop", "Received invalid block from " + sender);
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("ConsensusLoop", 
                "Error handling block message: " + std::string(e.what()));
            network_layer_->update_reputation(sender, -10);
        }
    }
    
    void handle_vote_message(
        const network::NetworkMessage& msg,
        const network::PeerID& sender
    ) {
        try {
            // Deserialize vote
            network::Vote vote = network::MessageSerializer::deserialize_vote(msg.payload);
            
            LOG_DEBUG("ConsensusLoop", 
                "Received vote for block at height " + std::to_string(vote.block_height) + 
                " from " + sender);
            
            // Convert network::Vote to consensus::Vote
            // Vote constructor: (validator_id, height, hash, view, signature)
            consensus::Vote consensus_vote(
                vote.validator_id,
                vote.block_height,
                vote.block_hash,
                vote.view_number,
                vote.signature
            );
            
            // Process vote through consensus engine
            auto qc_opt = consensus_engine_->on_receive_vote(consensus_vote);
            
            if (qc_opt.has_value()) {
                // QC formed! Broadcast it
                LOG_INFO("ConsensusLoop", 
                    "Quorum Certificate formed for block at height " + 
                    std::to_string(qc_opt->block_height));
                
                // The QC is already stored in the consensus engine
                // and will be included in the next block proposal
            }
            
            // Update peer reputation
            network_layer_->update_reputation(sender, 1);
            
        } catch (const std::exception& e) {
            LOG_ERROR("ConsensusLoop", 
                "Error handling vote message: " + std::string(e.what()));
            network_layer_->update_reputation(sender, -5);
        }
    }
    
    void handle_transaction_message(
        const network::NetworkMessage& msg,
        const network::PeerID& sender
    ) {
        try {
            // Deserialize transaction
            state::Transaction tx = state::Transaction::deserialize(msg.payload);
            
            LOG_DEBUG("ConsensusLoop", 
                "Received transaction " + tx.hash().to_hex() + " from " + sender);
            
            // Add to mempool
            if (mempool_->add_transaction(tx)) {
                // Transaction accepted
                network_layer_->update_reputation(sender, 1);
            } else {
                // Transaction rejected (duplicate or invalid)
                network_layer_->update_reputation(sender, -2);
            }
            
        } catch (const std::exception& e) {
            LOG_ERROR("ConsensusLoop", 
                "Error handling transaction message: " + std::string(e.what()));
            network_layer_->update_reputation(sender, -5);
        }
    }
    
    void handle_block_request_message(
        const network::NetworkMessage& msg,
        const network::PeerID& sender
    ) {
        try {
            // Deserialize block request
            network::BlockRequest request = 
                network::MessageSerializer::deserialize_block_request(msg.payload);
            
            LOG_DEBUG("ConsensusLoop", 
                "Received block request for height " + std::to_string(request.height) + 
                " from " + sender);
            
            // Get requested block
            std::optional<Block> block_opt;
            
            if (request.type == network::BlockRequest::RequestType::ByHeight) {
                block_opt = consensus_engine_->get_block_by_height(request.height);
            } else if (request.type == network::BlockRequest::RequestType::ByHash) {
                block_opt = consensus_engine_->get_block(request.hash);
            }
            
            // Create response
            network::BlockResponse response;
            if (block_opt.has_value()) {
                response = network::BlockResponse(*block_opt, true, "");
            } else {
                response = network::BlockResponse(Block(), false, "Block not found");
            }
            
            // Send response
            std::vector<uint8_t> response_data = response.serialize();
            network::NetworkMessage response_msg(
                network::MessageType::BlockResponse,
                response_data
            );
            network_layer_->send_to_peer(sender, response_msg);
            
        } catch (const std::exception& e) {
            LOG_ERROR("ConsensusLoop", 
                "Error handling block request: " + std::string(e.what()));
        }
    }
    
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
