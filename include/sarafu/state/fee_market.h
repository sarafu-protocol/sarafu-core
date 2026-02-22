#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace sarafu {
namespace state {

/**
 * FeeMarketState tracks the current state of the EIP-1559 style fee market.
 * 
 * The fee market implements dynamic base fee adjustment based on block gas usage:
 * - base_fee: The minimum fee per gas unit that must be paid (and is burned)
 * - target_gas_per_block: The target gas usage (50% of max_gas_per_block)
 * - max_gas_per_block: The maximum gas allowed per block
 * - adjustment_coefficient: γ = 0.125, controls how quickly base fee adjusts
 * 
 * Requirements: 12.1, 12.3, 12.4
 */
struct FeeMarketState {
    uint64_t base_fee;
    uint64_t target_gas_per_block;
    uint64_t max_gas_per_block;
    double adjustment_coefficient;  // γ = 0.125

    FeeMarketState()
        : base_fee(1000),  // Default initial base fee
          target_gas_per_block(0),
          max_gas_per_block(0),
          adjustment_coefficient(0.125) {}

    FeeMarketState(uint64_t initial_base_fee, uint64_t max_gas)
        : base_fee(initial_base_fee),
          target_gas_per_block(max_gas / 2),  // 50% of max
          max_gas_per_block(max_gas),
          adjustment_coefficient(0.125) {}
};

/**
 * FeeMarket implements EIP-1559 style dynamic base fee adjustment.
 * 
 * The base fee adjusts each block based on gas usage:
 * - If gas_used > target_gas: base fee increases
 * - If gas_used < target_gas: base fee decreases
 * - If gas_used = target_gas: base fee stays constant
 * 
 * Formula: BaseFeet+1 = BaseFeet·(1 + (GasUsed-TargetGas)/TargetGas·γ)
 * 
 * The adjustment is clamped to ±12.5% per block to prevent extreme volatility.
 * 
 * Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.8
 */
class FeeMarket {
public:
    /**
     * Construct a FeeMarket with initial base fee and max gas per block.
     * 
     * @param initial_base_fee The starting base fee (default: 1000)
     * @param max_gas_per_block The maximum gas allowed per block
     */
    explicit FeeMarket(uint64_t initial_base_fee = 1000, uint64_t max_gas_per_block = 10000000);

    ~FeeMarket();

    /**
     * Calculate the next block's base fee based on gas used in current block.
     * 
     * Formula: BaseFeet+1 = BaseFeet·(1 + (GasUsed-TargetGas)/TargetGas·γ)
     * 
     * The adjustment is clamped to ±12.5% per block:
     * - Maximum increase: base_fee * 1.125
     * - Maximum decrease: base_fee * 0.875
     * 
     * @param gas_used The amount of gas used in the current block
     * @return The base fee for the next block
     * 
     * Requirements: 12.2, 12.5
     */
    uint64_t calculate_next_base_fee(uint64_t gas_used);

    /**
     * Update the base fee to the next value based on gas used.
     * 
     * This calls calculate_next_base_fee() and updates the state.
     * 
     * @param gas_used The amount of gas used in the current block
     * @return The new base fee
     */
    uint64_t update_base_fee(uint64_t gas_used);

    /**
     * Get the current base fee.
     * 
     * @return The current base fee per gas unit
     */
    uint64_t current_base_fee() const { return state_.base_fee; }

    /**
     * Get the target gas per block (50% of max).
     * 
     * @return The target gas per block
     */
    uint64_t target_gas() const { return state_.target_gas_per_block; }

    /**
     * Get the maximum gas per block.
     * 
     * @return The maximum gas per block
     */
    uint64_t max_gas() const { return state_.max_gas_per_block; }

    /**
     * Get the adjustment coefficient γ.
     * 
     * @return The adjustment coefficient (0.125)
     */
    double adjustment_coefficient() const { return state_.adjustment_coefficient; }

    /**
     * Get the current fee market state.
     * 
     * @return The current FeeMarketState
     */
    const FeeMarketState& state() const { return state_; }

    /**
     * Estimate the total fee for a transaction.
     * 
     * Total fee = (base_fee * gas_limit) + priority_fee
     * 
     * @param gas_limit The gas limit for the transaction
     * @param priority_fee The optional priority fee (tip to proposer)
     * @return The estimated total fee
     */
    uint64_t estimate_fee(uint64_t gas_limit, uint64_t priority_fee = 0) const;

    /**
     * Check if a transaction fee meets the minimum requirement.
     * 
     * A transaction must pay at least base_fee * gas_limit.
     * 
     * @param transaction_fee The total fee offered by the transaction
     * @param gas_limit The gas limit for the transaction
     * @return true if the fee is sufficient, false otherwise
     * 
     * Requirements: 12.8
     */
    bool is_fee_sufficient(uint64_t transaction_fee, uint64_t gas_limit) const;

    /**
     * Set the maximum gas per block (updates target gas accordingly).
     * 
     * @param max_gas The new maximum gas per block
     */
    void set_max_gas_per_block(uint64_t max_gas);
    
    /**
     * Calculate burned fees for a block.
     * 
     * Burned amount = base_fee * gas_used
     * 
     * @param gas_used Gas used in the block
     * @return Amount of fees burned
     * 
     * Requirements: 12.3
     */
    uint64_t calculate_burned_fees(uint64_t gas_used) const;
    
    /**
     * Calculate priority fees for validators.
     * 
     * Priority fees = total_fees - burned_fees
     * 
     * @param total_fees Total fees paid in transactions
     * @param gas_used Gas used in the block
     * @return Priority fees for validators
     * 
     * Requirements: 12.4
     */
    uint64_t calculate_priority_fees(uint64_t total_fees, uint64_t gas_used) const;
    
    /**
     * Get total fees burned (cumulative).
     * 
     * @return Total fees burned since genesis
     */
    uint64_t get_total_burned() const { return total_burned_; }
    
    /**
     * Record burned fees for metrics.
     * 
     * @param amount Amount burned in this block
     */
    void record_burned_fees(uint64_t amount);

private:
    FeeMarketState state_;
    uint64_t total_burned_;

    // Constants for clamping
    static constexpr double MAX_ADJUSTMENT_FACTOR = 1.125;  // +12.5%
    static constexpr double MIN_ADJUSTMENT_FACTOR = 0.875;  // -12.5%
};

} // namespace state
} // namespace sarafu
