#pragma once

#include <cstdint>
#include <optional>

namespace sarafu {
namespace state {

/**
 * AdaptiveInflationParameters defines the inflation control system.
 * 
 * The system implements:
 * - Target annual inflation: 3-3.5%
 * - Dynamic band: 3-6%
 * - Absolute hard cap: 8%
 * - Smoothing: exponential adjustment over 100 epochs (α = 0.01 per epoch)
 * - Max annual inflation change: 1%
 * - Inflation responds to staking ratio deviation from 65% target
 * 
 * All parameters are consensus-enforced and not runtime-configurable.
 */
struct AdaptiveInflationParameters {
    // Target parameters (consensus-enforced)
    static constexpr double TARGET_INFLATION_MIN = 0.03;      // 3%
    static constexpr double TARGET_INFLATION_MAX = 0.035;     // 3.5%
    static constexpr double DYNAMIC_BAND_MIN = 0.03;          // 3%
    static constexpr double DYNAMIC_BAND_MAX = 0.06;          // 6%
    static constexpr double ABSOLUTE_HARD_CAP = 0.08;         // 8%
    static constexpr double MAX_ANNUAL_CHANGE = 0.01;         // 1%
    static constexpr double TARGET_STAKING_RATIO = 0.65;      // 65%
    static constexpr double SMOOTHING_ALPHA = 0.01;           // 1% per epoch
    static constexpr uint64_t SMOOTHING_EPOCHS = 100;         // 100 epochs
    
    // Current state
    double current_inflation_rate;
    double target_inflation_rate;
    uint64_t last_adjustment_epoch;
    
    AdaptiveInflationParameters()
        : current_inflation_rate(TARGET_INFLATION_MIN),
          target_inflation_rate(TARGET_INFLATION_MIN),
          last_adjustment_epoch(0) {}
};

/**
 * AdaptiveInflationEngine implements the adaptive inflation mechanism.
 * 
 * The engine adjusts inflation based on staking ratio deviation:
 * - If staking_ratio < 65%: Increase inflation to incentivize staking
 * - If staking_ratio > 65%: Decrease inflation to reduce dilution
 * - Adjustments are smoothed exponentially over 100 epochs
 * - All changes respect the hard cap and max annual change limits
 * 
 * Formula:
 *   deviation = staking_ratio - TARGET_STAKING_RATIO
 *   target_inflation = clamp(base_inflation - k * deviation, BAND_MIN, BAND_MAX)
 *   current_inflation = current_inflation + α * (target_inflation - current_inflation)
 *   final_inflation = min(current_inflation, ABSOLUTE_HARD_CAP)
 */
class AdaptiveInflationEngine {
public:
    AdaptiveInflationEngine();
    
    /**
     * Calculate the target inflation rate based on staking ratio.
     * 
     * @param staking_ratio Current staking ratio (bonded_stake / total_supply)
     * @return Target inflation rate within dynamic band
     */
    double calculate_target_inflation(double staking_ratio) const;
    
    /**
     * Update inflation rate with exponential smoothing.
     * 
     * This applies the smoothing formula:
     *   new_rate = current_rate + α * (target_rate - current_rate)
     * 
     * @param current_epoch Current epoch number
     * @param staking_ratio Current staking ratio
     * @return New inflation rate after smoothing
     */
    double update_inflation_rate(uint64_t current_epoch, double staking_ratio);
    
    /**
     * Get the current inflation rate.
     * 
     * @return Current annual inflation rate
     */
    double current_inflation_rate() const {
        return params_.current_inflation_rate;
    }
    
    /**
     * Get the target inflation rate.
     * 
     * @return Target annual inflation rate
     */
    double target_inflation_rate() const {
        return params_.target_inflation_rate;
    }
    
    /**
     * Validate that inflation rate respects all constraints.
     * 
     * Checks:
     * - Rate is within absolute hard cap
     * - Annual change is within max limit
     * 
     * @param proposed_rate The proposed inflation rate
     * @param previous_rate The previous inflation rate
     * @return true if valid, false otherwise
     */
    bool validate_inflation_rate(double proposed_rate, double previous_rate) const;
    
    /**
     * Get the inflation parameters.
     * 
     * @return Current inflation parameters
     */
    const AdaptiveInflationParameters& parameters() const {
        return params_;
    }

private:
    AdaptiveInflationParameters params_;
    
    // Responsiveness coefficient for staking ratio deviation
    static constexpr double STAKING_RESPONSE_COEFFICIENT = 0.05;
};

} // namespace state
} // namespace sarafu
