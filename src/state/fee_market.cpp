#include "sarafu/state/fee_market.h"
#include <algorithm>
#include <cmath>

namespace sarafu {
namespace state {

FeeMarket::FeeMarket(uint64_t initial_base_fee, uint64_t max_gas_per_block)
    : state_(initial_base_fee, max_gas_per_block) {}

FeeMarket::~FeeMarket() = default;

uint64_t FeeMarket::calculate_next_base_fee(uint64_t gas_used) {
    // Handle edge case: if target gas is 0, return current base fee
    if (state_.target_gas_per_block == 0) {
        return state_.base_fee;
    }

    // Calculate gas delta from target
    int64_t gas_delta = static_cast<int64_t>(gas_used) - static_cast<int64_t>(state_.target_gas_per_block);

    // Calculate adjustment factor: (GasUsed - TargetGas) / TargetGas * γ
    double adjustment_ratio = static_cast<double>(gas_delta) / static_cast<double>(state_.target_gas_per_block);
    double adjustment_factor = adjustment_ratio * state_.adjustment_coefficient;

    // Calculate new base fee: BaseFeet+1 = BaseFeet * (1 + adjustment_factor)
    double multiplier = 1.0 + adjustment_factor;

    // Clamp adjustment to ±12.5% per block
    multiplier = std::max(MIN_ADJUSTMENT_FACTOR, std::min(MAX_ADJUSTMENT_FACTOR, multiplier));

    // Apply adjustment
    double new_base_fee_double = static_cast<double>(state_.base_fee) * multiplier;

    // Convert to uint64_t, ensuring minimum of 1
    uint64_t new_base_fee = static_cast<uint64_t>(std::round(new_base_fee_double));
    new_base_fee = std::max(uint64_t(1), new_base_fee);

    return new_base_fee;
}

uint64_t FeeMarket::update_base_fee(uint64_t gas_used) {
    uint64_t new_base_fee = calculate_next_base_fee(gas_used);
    state_.base_fee = new_base_fee;
    return new_base_fee;
}

uint64_t FeeMarket::estimate_fee(uint64_t gas_limit, uint64_t priority_fee) const {
    uint64_t base_fee_amount = state_.base_fee * gas_limit;
    return base_fee_amount + priority_fee;
}

bool FeeMarket::is_fee_sufficient(uint64_t transaction_fee, uint64_t gas_limit) const {
    uint64_t minimum_fee = state_.base_fee * gas_limit;
    return transaction_fee >= minimum_fee;
}

void FeeMarket::set_max_gas_per_block(uint64_t max_gas) {
    state_.max_gas_per_block = max_gas;
    state_.target_gas_per_block = max_gas / 2;  // Target is 50% of max
}

} // namespace state
} // namespace sarafu
