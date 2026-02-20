#pragma once

#include "sarafu/consensus/block.h"
#include "sarafu/consensus/block_validator.h"
#include "sarafu/consensus/checkpoint_manager.h"
#include "sarafu/state/state_machine.h"
#include "sarafu/network/network_layer.h"
#include "sarafu/storage/database.h"
#include <memory>
#include <vector>
#include <functional>

namespace sarafu {
namespace consensus {

// Type alias for checkpoint
using Checkpoint = WeakSubjectivityCheckpoint;

/**
 * SyncStatus represents the current synchronization state.
 */
enum class SyncStatus {
    NotStarted,      // Sync has not begun
    Syncing,         // Currently syncing blocks
    FastSyncing,     // Fast sync from checkpoint
    Synced,          // Fully synced with network
    Failed           // Sync failed
};

/**
 * SyncProgress tracks synchronization progress.
 */
struct SyncProgress {
    SyncStatus status;
    uint64_t current_height;
    uint64_t target_height;
    uint64_t blocks_downloaded;
    uint64_t blocks_verified;
    double progress_percentage;

    SyncProgress()
        : status(SyncStatus::NotStarted),
          current_height(0),
          target_height(0),
          blocks_downloaded(0),
          blocks_verified(0),
          progress_percentage(0.0) {}
};

/**
 * SyncManager handles blockchain synchronization.
 * 
 * Responsibilities:
 * - Download blocks from peers in batches
 * - Verify each block's Quorum Certificate
 * - Validate and execute blocks
 * - Support fast sync from weak subjectivity checkpoints
 * - Track sync progress percentage
 * 
 * Synchronization modes:
 * 1. Full sync: Download and verify all blocks from genesis
 * 2. Fast sync: Start from a weak subjectivity checkpoint
 * 
 * Requirements: 28.1, 28.2, 28.3, 28.4, 28.5, 28.6, 28.7, 28.8
 */
class SyncManager {
public:
    /**
     * Configuration for the sync manager.
     */
    struct Config {
        size_t batch_size;              // Number of blocks to request per batch
        size_t max_concurrent_requests; // Maximum concurrent block requests
        uint64_t request_timeout_ms;    // Timeout for block requests in milliseconds
        bool enable_fast_sync;          // Whether to enable fast sync mode
        
        Config()
            : batch_size(100),
              max_concurrent_requests(5),
              request_timeout_ms(5000),
              enable_fast_sync(true) {}
    };

    /**
     * Construct a SyncManager.
     * 
     * @param network_layer The network layer for requesting blocks
     * @param block_validator The block validator for verifying blocks
     * @param state_machine The state machine for executing blocks
     * @param database The database for persisting blocks
     * @param checkpoint_manager The checkpoint manager for fast sync
     * @param config Sync configuration
     */
    SyncManager(
        std::shared_ptr<network::NetworkLayer> network_layer,
        std::shared_ptr<BlockValidator> block_validator,
        std::shared_ptr<state::StateMachine> state_machine,
        std::shared_ptr<storage::Database> database,
        std::shared_ptr<CheckpointManager> checkpoint_manager,
        const Config& config = Config()
    );

    /**
     * Start synchronization from genesis.
     * 
     * This downloads and verifies the genesis block, then proceeds
     * to sync all subsequent blocks.
     * 
     * Requirements: 28.1
     * 
     * @return true if sync started successfully, false otherwise
     */
    bool start_sync();

    /**
     * Start fast sync from a weak subjectivity checkpoint.
     * 
     * This method:
     * 1. Obtains a trusted checkpoint (< 1M blocks old)
     * 2. Verifies the checkpoint signature (≥2/3 stake)
     * 3. Downloads blocks from the checkpoint forward
     * 4. Verifies only recent state in detail
     * 
     * Requirements: 28.5, 28.6
     * 
     * @param checkpoint The trusted checkpoint to sync from
     * @return true if fast sync started successfully, false otherwise
     */
    bool fast_sync(const Checkpoint& checkpoint);

    /**
     * Download blocks from peers in batches.
     * 
     * This method requests blocks from multiple peers in parallel,
     * using the configured batch size and concurrency limits.
     * 
     * Requirements: 28.2
     * 
     * @param start_height The starting block height
     * @param end_height The ending block height
     * @return Vector of downloaded blocks
     */
    std::vector<Block> download_blocks(uint64_t start_height, uint64_t end_height);

    /**
     * Verify and apply a block to the blockchain.
     * 
     * This method:
     * 1. Verifies the block's Quorum Certificate (≥2/3 stake)
     * 2. Validates the block against all consensus rules
     * 3. Executes all transactions in the block
     * 4. Updates the state machine
     * 5. Persists the block to storage
     * 
     * Requirements: 28.3, 28.4
     * 
     * @param block The block to verify and apply
     * @return true if block was successfully applied, false otherwise
     */
    bool verify_and_apply_block(const Block& block);

    /**
     * Get the current sync progress.
     * 
     * Requirements: 28.7
     * 
     * @return SyncProgress containing current status and percentage
     */
    SyncProgress get_progress() const;

    /**
     * Check if the node is fully synced.
     * 
     * Requirements: 28.8
     * 
     * @return true if synced, false otherwise
     */
    bool is_synced() const;

    /**
     * Stop synchronization.
     */
    void stop_sync();

    /**
     * Set a callback to be invoked when sync progress updates.
     * 
     * @param callback Function to call with progress updates
     */
    void set_progress_callback(std::function<void(const SyncProgress&)> callback);

private:
    /**
     * Request a block from a peer.
     * 
     * @param peer The peer to request from
     * @param height The block height to request
     * @return The requested block, or nullptr if request failed
     */
    std::unique_ptr<Block> request_block_from_peer(
        const network::PeerID& peer,
        uint64_t height
    );

    /**
     * Select the best peer to request a block from.
     * 
     * Prioritizes peers with:
     * - High reputation scores
     * - Low latency
     * - Validator status
     * 
     * @return The selected peer ID, or empty string if no peers available
     */
    network::PeerID select_peer_for_request() const;

    /**
     * Update sync progress and invoke callback if set.
     */
    void update_progress();

    /**
     * Calculate progress percentage.
     * 
     * @return Progress as a percentage (0.0 to 100.0)
     */
    double calculate_progress_percentage() const;

    /**
     * Get the highest block height from connected peers.
     * 
     * @return The target height to sync to
     */
    uint64_t get_target_height() const;

    /**
     * Verify a block's Quorum Certificate.
     * 
     * @param block The block to verify
     * @return true if QC is valid, false otherwise
     */
    bool verify_quorum_certificate(const Block& block) const;

    /**
     * Store a block in the database.
     * 
     * @param block The block to store
     * @return true if storage succeeded, false otherwise
     */
    bool store_block(const Block& block);

    /**
     * Get the current block height from the database.
     * 
     * @return The current height
     */
    uint64_t get_current_height() const;

    std::shared_ptr<network::NetworkLayer> network_layer_;
    std::shared_ptr<BlockValidator> block_validator_;
    std::shared_ptr<state::StateMachine> state_machine_;
    std::shared_ptr<storage::Database> database_;
    std::shared_ptr<CheckpointManager> checkpoint_manager_;
    Config config_;

    SyncProgress progress_;
    std::function<void(const SyncProgress&)> progress_callback_;
    bool stop_requested_;
};

} // namespace consensus
} // namespace sarafu
