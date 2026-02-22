#include "sarafu/state/adaptive_inflation.h"
#include <algorithm>
#include <cmath>

namespace sarafu {
namespace state {

AdaptiveInflationEngine::AdaptiveInflationEngine()
    : params_() {}

double AdaptiveInflationEngine::calculate_target_inflation(double staking_ratio) const {
    // Calculate deviation from target staking ratio
    double deviation = staking_ratio - AdaptiveInflationParameters::TARGET_STAKING_RATIO;
    
    // Base inflation is the midpoint of target range
    double base_inflation = (AdaptiveInflationParameters::TARGET_INFLATION_MIN + 
                            AdaptiveInflationParameters::TARGET_INFLATION_MAX) / 2.0;
    
    // Adjust inflation inversely to staking ratio deviation
    // If staking is low, increase inflation to incentivize staking
    // If staking is high, decrease inflation to reduce dilution
    double target = base_inflation - (STAKING_RESPONSE_COEFFICIENT * deviation);
    
    // Clamp to dynamic band
    target = std::max(target, AdaptiveInflationParameters::DYNAMIC_BAND_MIN);
    target = std::min(target, AdaptiveInflationParameters::DYNAMIC_BAND_MAX);
    
    return target;
}

double AdaptiveInflationEngine::update_inflation_rate(
    uint64_t current_epoch,
    double staking_ratio
) {
    // Calculate target inflation based on current staking ratio
    double target = calculate_target_inflation(staking_ratio);
    params_.target_inflation_rate = target;
    
    // Apply exponential smoothing
    // new_rate = current_rate + α * (target_rate - current_rate)
    double smoothed_rate = params_.current_inflation_rate + 
                          AdaptiveInflationParameters::SMOOTHING_ALPHA * 
                          (target - params_.current_inflation_rate);
    
    // Enforce max annual change constraint
    double max_change = AdaptiveInflationParameters::MAX_ANNUAL_CHANGE;
    double rate_change = smoothed_rate - params_.current_inflation_rate;
    
    if (std::abs(rate_change) > max_change) {
        if (rate_change > 0) {
            smoothed_rate = params_.current_inflation_rate + max_change;
        } else {
            smoothed_rate = params_.current_inflation_rate - max_change;
        }
    }
    
    // Enforce absolute hard cap
    smoothed_rate = std::min(smoothed_rate, AdaptiveInflationParameters::ABSOLUTE_HARD_CAP);
    
    // Ensure rate stays within dynamic band
    smoothed_rate = std::max(smoothed_rate, AdaptiveInflationParameters::DYNAMIC_BAND_MIN);
    smoothed_rate = std::min(smoothed_rate, AdaptiveInflationParameters::DYNAMIC_BAND_MAX);
    
    // Update state
    params_.current_inflation_rate = smoothed_rate;
    params_.last_adjustment_epoch = current_epoch;
    
    return smoothed_rate;
}

bool AdaptiveInflationEngine::validate_inflation_rate(
    double proposed_rate,
    double previous_rate
) const {
    // Check absolute hard cap
    if (proposed_rate > AdaptiveInflationParameters::ABSOLUTE_HARD_CAP) {
        return false;
    }
    
    // Check dynamic band
    if (proposed_rate < AdaptiveInflationParameters::DYNAMIC_BAND_MIN ||
        proposed_rate > AdaptiveInflationParameters::DYNAMIC_BAND_MAX) {
        return false;
    }
    
    // Check max annual change
    double change = std::abs(proposed_rate - previous_rate);
    if (change > AdaptiveInflationParameters::MAX_ANNUAL_CHANGE) {
        return false;
    }
    
    return true;
}

} // namespace state
} // namespace sarafu
