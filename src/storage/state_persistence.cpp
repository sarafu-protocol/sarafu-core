#include "sarafu/storage/state_persistence.h"
#include <cstring>

namespace sarafu {
namespace storage {

StatePersistence::StatePersistence(std::shared_ptr<StateStorage> storage)
    : storage_(std::move(storage)) {}

StatePersistence::~StatePersistence() = default;

Result<void> StatePersistence::save_state(
    uint64_t current_height,
    const std::vector<state::Account>& accounts
) {
    // Save current height to metadata
    std::vector<uint8_t> height_bytes(sizeof(uint64_t));
    std::memcpy(height_bytes.data(), &current_height, sizeof(uint64_t));
    
    auto height_result = storage_->store_metadata(LAST_HEIGHT_KEY, height_bytes);
    if (height_result.is_error()) {
        return Result<void>::error(
            std::string("Failed to save height: ") + height_result.error()
        );
    }

    // Save all accounts in a batch for efficiency
    auto accounts_result = storage_->store_accounts_batch(accounts);
    if (accounts_result.is_error()) {
        return Result<void>::error(
            std::string("Failed to save accounts: ") + accounts_result.error()
        );
    }

    return Result<void>::ok();
}

Result<uint64_t> StatePersistence::load_state() {
    // Check if state exists
    if (!state_exists()) {
        return Result<uint64_t>::error("No saved state found");
    }

    // Load last height
    return get_last_height();
}

Result<uint64_t> StatePersistence::get_last_height() const {
    auto result = storage_->get_metadata(LAST_HEIGHT_KEY);
    if (result.is_error()) {
        return Result<uint64_t>::error(result.error());
    }

    const auto& height_bytes = result.value();
    if (height_bytes.size() != sizeof(uint64_t)) {
        return Result<uint64_t>::error("Invalid height data");
    }

    uint64_t height;
    std::memcpy(&height, height_bytes.data(), sizeof(uint64_t));
    return Result<uint64_t>::ok(height);
}

Result<uint64_t> StatePersistence::prune_old_state(uint64_t current_height) {
    // Calculate the pruning threshold
    // Keep blocks within the weak subjectivity period
    if (current_height <= weak_subjectivity_period()) {
        // Not enough blocks to prune yet
        return Result<uint64_t>::ok(0);
    }

    uint64_t prune_until_height = current_height - weak_subjectivity_period();

    // Get the last pruned height (if any)
    uint64_t last_pruned_height = 0;
    auto pruned_result = storage_->get_metadata(PRUNED_UNTIL_HEIGHT_KEY);
    if (pruned_result.is_ok()) {
        const auto& pruned_bytes = pruned_result.value();
        if (pruned_bytes.size() == sizeof(uint64_t)) {
            std::memcpy(&last_pruned_height, pruned_bytes.data(), sizeof(uint64_t));
        }
    }

    // Prune blocks from last_pruned_height to prune_until_height
    uint64_t blocks_pruned = 0;
    for (uint64_t height = last_pruned_height; height < prune_until_height; ++height) {
        // Check if block exists
        if (!storage_->block_exists(height)) {
            continue;
        }

        // Get the block to find transaction hashes
        auto block_result = storage_->get_block_by_height(height);
        if (block_result.is_error()) {
            // Skip this block if we can't read it
            continue;
        }

        const auto& block = block_result.value();

        // Delete transaction receipts for this block
        for (const auto& tx : block.transactions) {
            auto tx_hash = tx.hash();
            storage_->delete_transaction_receipt(tx_hash);
        }

        // Delete the block
        auto delete_result = storage_->delete_block(height);
        if (delete_result.is_ok()) {
            blocks_pruned++;
        }
    }

    // Update the last pruned height
    std::vector<uint8_t> pruned_bytes(sizeof(uint64_t));
    std::memcpy(pruned_bytes.data(), &prune_until_height, sizeof(uint64_t));
    storage_->store_metadata(PRUNED_UNTIL_HEIGHT_KEY, pruned_bytes);

    return Result<uint64_t>::ok(blocks_pruned);
}

bool StatePersistence::state_exists() const {
    auto result = storage_->get_metadata(LAST_HEIGHT_KEY);
    return result.is_ok();
}

} // namespace storage
} // namespace sarafu
