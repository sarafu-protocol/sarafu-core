#pragma once

#include <memory>
#include <vector>
#include "sarafu/storage/state_storage.h"
#include "sarafu/state/account.h"
#include "sarafu/consensus/block.h"

namespace sarafu {
namespace storage {

/**
 * StatePersistence manages saving and loading blockchain state.
 * 
 * This class provides:
 * - save_state(): Persist current blockchain state to disk
 * - load_state(): Restore blockchain state on node restart
 * - prune_old_state(): Remove state older than weak subjectivity period
 * 
 * The weak subjectivity period is 1,000,000 blocks (as per requirements).
 * State older than this can be safely pruned since nodes must sync from
 * a checkpoint within this period.
 */
class StatePersistence {
public:
    /**
     * Create a StatePersistence instance.
     * 
     * @param storage The state storage to use
     */
    explicit StatePersistence(std::shared_ptr<StateStorage> storage);

    ~StatePersistence();

    // Disable copy and move
    StatePersistence(const StatePersistence&) = delete;
    StatePersistence& operator=(const StatePersistence&) = delete;
    StatePersistence(StatePersistence&&) = delete;
    StatePersistence& operator=(StatePersistence&&) = delete;

    /**
     * Save the current blockchain state to disk.
     * 
     * This persists:
     * - Current block height
     * - All account states
     * - Recent blocks (within weak subjectivity period)
     * - Transaction receipts
     * 
     * The state is saved atomically to ensure consistency.
     * 
     * @param current_height The current blockchain height
     * @param accounts All account states to persist
     * @return Result indicating success or error
     */
    Result<void> save_state(
        uint64_t current_height,
        const std::vector<state::Account>& accounts
    );

    /**
     * Load blockchain state from disk on node restart.
     * 
     * This restores:
     * - Last known block height
     * - All account states
     * - Recent blocks
     * 
     * @return Result containing the last block height, or an error
     */
    Result<uint64_t> load_state();

    /**
     * Get the last saved block height.
     * 
     * @return Result containing the height, or an error if no state exists
     */
    Result<uint64_t> get_last_height() const;

    /**
     * Prune old state beyond the weak subjectivity period.
     * 
     * This removes:
     * - Blocks older than (current_height - weak_subjectivity_period)
     * - Associated transaction receipts
     * 
     * Account states are NOT pruned as they represent current state.
     * 
     * The weak subjectivity period is 1,000,000 blocks.
     * 
     * @param current_height The current blockchain height
     * @return Result containing the number of blocks pruned, or an error
     */
    Result<uint64_t> prune_old_state(uint64_t current_height);

    /**
     * Check if state exists (node has been initialized).
     * 
     * @return true if state exists, false otherwise
     */
    bool state_exists() const;

    /**
     * Get the weak subjectivity period in blocks.
     * 
     * @return The weak subjectivity period (1,000,000 blocks)
     */
    static constexpr uint64_t weak_subjectivity_period() {
        return 1000000;
    }

private:
    std::shared_ptr<StateStorage> storage_;

    // Metadata keys
    static constexpr const char* LAST_HEIGHT_KEY = "last_height";
    static constexpr const char* GENESIS_HEIGHT_KEY = "genesis_height";
    static constexpr const char* PRUNED_UNTIL_HEIGHT_KEY = "pruned_until_height";
};

} // namespace storage
} // namespace sarafu
