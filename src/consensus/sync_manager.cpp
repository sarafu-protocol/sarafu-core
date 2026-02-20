#include "sarafu/consensus/sync_manager.h"
#include "sarafu/consensus/qc_verifier.h"
#include "sarafu/network/message_handler.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

namespace sarafu {
namespace consensus {

SyncManager::SyncManager(
    std::shared_ptr<network::NetworkLayer> network_layer,
    std::shared_ptr<BlockValidator> block_validator,
    std::shared_ptr<state::StateMachine> state_machine,
    std::shared_ptr<storage::Database> database,
    std::shared_ptr<CheckpointManager> checkpoint_manager,
    const Config& config
)
    : network_layer_(network_layer),
      block_validator_(block_validator),
      state_machine_(state_machine),
      database_(database),
      checkpoint_manager_(checkpoint_manager),
      config_(config),
      stop_requested_(false) {
    progress_.status = SyncStatus::NotStarted;
}

bool SyncManager::start_sync() {
    if (progress_.status == SyncStatus::Syncing || progress_.status == SyncStatus::FastSyncing) {
        return false;  // Already syncing
    }

    progress_.status = SyncStatus::Syncing;
    progress_.current_height = get_current_height();
    progress_.target_height = get_target_height();
    stop_requested_ = false;

    update_progress();

    // Start syncing from current height + 1
    uint64_t start_height = progress_.current_height + 1;
    uint64_t end_height = progress_.target_height;

    // Download and apply blocks in batches
    while (start_height <= end_height && !stop_requested_) {
        uint64_t batch_end = std::min(start_height + config_.batch_size - 1, end_height);
        
        // Download batch of blocks
        std::vector<Block> blocks = download_blocks(start_height, batch_end);
        
        if (blocks.empty()) {
            // Failed to download blocks, retry or fail
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        // Verify and apply each block
        for (const auto& block : blocks) {
            if (!verify_and_apply_block(block)) {
                progress_.status = SyncStatus::Failed;
                update_progress();
                return false;
            }
            
            progress_.current_height = block.header.height;
            progress_.blocks_verified++;
            update_progress();
        }

        start_height = batch_end + 1;
    }

    if (stop_requested_) {
        progress_.status = SyncStatus::NotStarted;
    } else {
        progress_.status = SyncStatus::Synced;
    }
    
    update_progress();
    return true;
}

bool SyncManager::fast_sync(const Checkpoint& checkpoint) {
    if (!config_.enable_fast_sync) {
        return false;
    }

    // Verify checkpoint age
    uint64_t current_height = get_current_height();
    if (!checkpoint_manager_->check_checkpoint_age(checkpoint.block_height, current_height)) {
        return false;
    }

    progress_.status = SyncStatus::FastSyncing;
    progress_.current_height = checkpoint.block_height;
    progress_.target_height = get_target_height();
    stop_requested_ = false;

    update_progress();

    // Start syncing from checkpoint height + 1
    uint64_t start_height = checkpoint.block_height + 1;
    uint64_t end_height = progress_.target_height;

    // Download and apply blocks in batches
    while (start_height <= end_height && !stop_requested_) {
        uint64_t batch_end = std::min(start_height + config_.batch_size - 1, end_height);
        
        // Download batch of blocks
        std::vector<Block> blocks = download_blocks(start_height, batch_end);
        
        if (blocks.empty()) {
            // Failed to download blocks, retry or fail
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        // Verify and apply each block
        for (const auto& block : blocks) {
            if (!verify_and_apply_block(block)) {
                progress_.status = SyncStatus::Failed;
                update_progress();
                return false;
            }
            
            progress_.current_height = block.header.height;
            progress_.blocks_verified++;
            update_progress();
        }

        start_height = batch_end + 1;
    }

    if (stop_requested_) {
        progress_.status = SyncStatus::NotStarted;
    } else {
        progress_.status = SyncStatus::Synced;
    }
    
    update_progress();
    return true;
}

std::vector<Block> SyncManager::download_blocks(uint64_t start_height, uint64_t end_height) {
    std::vector<Block> blocks;
    blocks.reserve(end_height - start_height + 1);

    // Download blocks from peers
    for (uint64_t height = start_height; height <= end_height; ++height) {
        if (stop_requested_) {
            break;
        }

        // Select a peer to request from
        network::PeerID peer = select_peer_for_request();
        if (peer.empty()) {
            // No peers available
            break;
        }

        // Request block from peer
        auto block = request_block_from_peer(peer, height);
        if (block) {
            blocks.push_back(*block);
            progress_.blocks_downloaded++;
        } else {
            // Failed to download block, stop batch
            break;
        }
    }

    return blocks;
}

bool SyncManager::verify_and_apply_block(const Block& block) {
    // 1. Verify Quorum Certificate
    if (!verify_quorum_certificate(block)) {
        return false;
    }

    // 2. Validate block (we need the parent block for validation)
    // For now, we'll skip parent validation in sync mode
    // In a full implementation, we would fetch the parent from storage
    
    // 3. Execute transactions and update state
    // This is simplified - in production, we'd execute the block properly
    
    // 4. Store block in database
    if (!store_block(block)) {
        return false;
    }

    return true;
}

SyncProgress SyncManager::get_progress() const {
    return progress_;
}

bool SyncManager::is_synced() const {
    return progress_.status == SyncStatus::Synced;
}

void SyncManager::stop_sync() {
    stop_requested_ = true;
}

void SyncManager::set_progress_callback(std::function<void(const SyncProgress&)> callback) {
    progress_callback_ = callback;
}

std::unique_ptr<Block> SyncManager::request_block_from_peer(
    const network::PeerID& peer,
    uint64_t height
) {
    // Create a block request message
    network::BlockRequest request = network::BlockRequest::by_height(
        height,
        state::Address::zero()  // Requester ID (could be our validator ID)
    );
    
    // Serialize the request
    std::vector<uint8_t> request_data = request.serialize();
    
    // Create network message
    network::NetworkMessage msg(network::MessageType::BlockRequest, request_data);
    
    // Send request to peer
    if (!network_layer_->send_to_peer(peer, msg)) {
        return nullptr;
    }
    
    // Wait for response with timeout
    // In a full implementation, this would use a promise/future pattern
    // or a callback-based approach. For now, we'll use a simple polling approach.
    
    // Store the request in a pending requests map
    // This is simplified - production code would use proper async handling
    std::this_thread::sleep_for(std::chrono::milliseconds(config_.request_timeout_ms));
    
    // For now, return nullptr to indicate the request is pending
    // A full implementation would have a response handler that fills in the block
    return nullptr;
}

network::PeerID SyncManager::select_peer_for_request() const {
    // Get connected peers
    auto peers = network_layer_->get_peers();
    
    if (peers.empty()) {
        return "";
    }

    // Prioritize validators with high reputation
    std::vector<network::PeerInfo> validators;
    std::vector<network::PeerInfo> non_validators;
    
    for (const auto& peer : peers) {
        if (peer.is_validator) {
            validators.push_back(peer);
        } else {
            non_validators.push_back(peer);
        }
    }

    // Sort by reputation score (descending)
    auto sort_by_reputation = [](const network::PeerInfo& a, const network::PeerInfo& b) {
        return a.reputation_score > b.reputation_score;
    };
    
    std::sort(validators.begin(), validators.end(), sort_by_reputation);
    std::sort(non_validators.begin(), non_validators.end(), sort_by_reputation);

    // Prefer validators
    if (!validators.empty()) {
        return validators[0].id;
    }
    
    if (!non_validators.empty()) {
        return non_validators[0].id;
    }

    return "";
}

void SyncManager::update_progress() {
    progress_.progress_percentage = calculate_progress_percentage();
    
    if (progress_callback_) {
        progress_callback_(progress_);
    }
}

double SyncManager::calculate_progress_percentage() const {
    if (progress_.target_height == 0) {
        return 0.0;
    }

    if (progress_.current_height >= progress_.target_height) {
        return 100.0;
    }

    return (static_cast<double>(progress_.current_height) / 
            static_cast<double>(progress_.target_height)) * 100.0;
}

uint64_t SyncManager::get_target_height() const {
    // In a full implementation, this would query peers for their highest block
    // For now, return a placeholder value
    return progress_.current_height + 1000;
}

bool SyncManager::verify_quorum_certificate(const Block& block) const {
    // Verify that the QC has ≥2/3 stake signatures
    // This is a simplified check - a full implementation would:
    // 1. Get the validator set for the block's epoch
    // 2. Verify the aggregated BLS signature
    // 3. Check that signers represent ≥2/3 of total stake
    
    // For now, just check that the QC exists
    return block.justify.total_stake_signed > 0;
}

bool SyncManager::store_block(const Block& block) {
    // Serialize the block
    std::vector<uint8_t> block_data = block.serialize();
    
    // Create key from height
    std::vector<uint8_t> key(sizeof(uint64_t));
    uint64_t height = block.header.height;
    std::memcpy(key.data(), &height, sizeof(uint64_t));
    
    // Store in Blocks column family
    auto result = database_->put(storage::ColumnFamily::Blocks, key, block_data);
    
    if (result.is_error()) {
        return false;
    }

    // Also store block hash -> height mapping
    std::vector<uint8_t> hash_key = block.hash().serialize();
    auto hash_result = database_->put(storage::ColumnFamily::BlockHashes, hash_key, key);
    
    return hash_result.is_ok();
}

uint64_t SyncManager::get_current_height() const {
    // Get the current height from the state machine
    return state_machine_->get_current_height();
}

} // namespace consensus
} // namespace sarafu
