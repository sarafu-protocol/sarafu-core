#include "sarafu/monitoring/security_monitor.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace sarafu {
namespace monitoring {

uint64_t SecurityMonitor::calculateNakamotoCoefficient(
    const std::vector<ValidatorInfo>& validators) {
    
    // Group validators by entity and sum their stake
    auto entity_distribution = calculateEntityStakeDistribution(validators);
    
    // Calculate total stake
    double total_stake = 0.0;
    for (const auto& [entity, stake] : entity_distribution) {
        total_stake += stake;
    }
    
    return calculateCoefficientFromDistribution(entity_distribution, total_stake);
}

uint64_t SecurityMonitor::calculateJurisdictionalNakamoto(
    const std::vector<ValidatorInfo>& validators) {
    
    // Group validators by jurisdiction and sum their stake
    auto jurisdiction_distribution = calculateJurisdictionStakeDistribution(validators);
    
    // Calculate total stake
    double total_stake = 0.0;
    for (const auto& [jurisdiction, stake] : jurisdiction_distribution) {
        total_stake += stake;
    }
    
    return calculateCoefficientFromDistribution(jurisdiction_distribution, total_stake);
}

double SecurityMonitor::calculateAttackCost(
    const std::vector<ValidatorInfo>& validators,
    double sar_price_usd,
    double liquidity_depth_usd,
    double slippage_factor) {
    
    // Calculate total stake
    double total_stake = 0.0;
    for (const auto& validator : validators) {
        total_stake += validator.stake_sar;
    }
    
    // Calculate 1/3 of total stake needed for attack
    double attack_stake = total_stake / 3.0;
    
    // Base cost without slippage
    double base_cost = attack_stake * sar_price_usd;
    
    // Apply slippage factor if purchase exceeds liquidity depth
    double adjusted_cost = base_cost;
    if (base_cost > liquidity_depth_usd) {
        // Apply slippage to the portion exceeding liquidity depth
        double excess = base_cost - liquidity_depth_usd;
        adjusted_cost = liquidity_depth_usd + (excess * slippage_factor);
    }
    
    return adjusted_cost;
}

double SecurityMonitor::calculateHHI(const std::vector<ValidatorInfo>& validators) {
    // Group validators by entity
    auto entity_distribution = calculateEntityStakeDistribution(validators);
    
    // Calculate total stake
    double total_stake = 0.0;
    for (const auto& [entity, stake] : entity_distribution) {
        total_stake += stake;
    }
    
    if (total_stake == 0.0) {
        return 0.0;
    }
    
    // Calculate HHI as sum of squared market shares (in percentage points)
    double hhi = 0.0;
    for (const auto& [entity, stake] : entity_distribution) {
        double market_share_percent = (stake / total_stake) * 100.0;
        hhi += market_share_percent * market_share_percent;
    }
    
    return hhi;
}

std::map<std::string, double> SecurityMonitor::calculateEntityStakeDistribution(
    const std::vector<ValidatorInfo>& validators) {
    
    std::map<std::string, double> distribution;
    
    for (const auto& validator : validators) {
        // Use validator ID as entity if entity_id is empty
        std::string entity = validator.entity_id.empty() ? validator.id : validator.entity_id;
        distribution[entity] += validator.stake_sar;
    }
    
    return distribution;
}

std::map<std::string, double> SecurityMonitor::calculateJurisdictionStakeDistribution(
    const std::vector<ValidatorInfo>& validators) {
    
    std::map<std::string, double> distribution;
    
    for (const auto& validator : validators) {
        // Use "UNKNOWN" if jurisdiction is empty
        std::string jurisdiction = validator.jurisdiction.empty() ? "UNKNOWN" : validator.jurisdiction;
        distribution[jurisdiction] += validator.stake_sar;
    }
    
    return distribution;
}

uint64_t SecurityMonitor::calculateCoefficientFromDistribution(
    const std::map<std::string, double>& stake_distribution,
    double total_stake) {
    
    if (total_stake == 0.0 || stake_distribution.empty()) {
        return 0;
    }
    
    // Create sorted vector of stakes (descending order)
    std::vector<double> stakes;
    stakes.reserve(stake_distribution.size());
    for (const auto& [entity, stake] : stake_distribution) {
        stakes.push_back(stake);
    }
    std::sort(stakes.begin(), stakes.end(), std::greater<double>());
    
    // Calculate threshold (1/3 of total stake)
    double threshold = total_stake / 3.0;
    
    // Count entities needed to exceed threshold
    double cumulative_stake = 0.0;
    uint64_t count = 0;
    
    for (double stake : stakes) {
        cumulative_stake += stake;
        count++;
        
        if (cumulative_stake > threshold) {
            break;
        }
    }
    
    return count;
}

} // namespace monitoring
} // namespace sarafu
