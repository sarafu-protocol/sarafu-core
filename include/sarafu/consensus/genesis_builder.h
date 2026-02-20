#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "sarafu/consensus/block.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/state/account.h"

namespace sarafu {
namespace consensus {

/**
 * VestingSchedule represents a linear vesting schedule for genesis allocations.
 * 
 * For allocations exceeding 2% of total supply, a 12-month linear vesting
 * schedule is enforced to prevent instant cartel formation at genesis.
 */
struct VestingSchedule {
    uint64_t total_amount;           // Total amount to be vested
    uint64_t vested_amount;          // Amount already vested
    uint64_t start_timestamp;        // Vesting start time (genesis timestamp)
    uint64_t duration_seconds;       // Vesting duration (12 months = 31,536,000 seconds)
    
    // Constructors
    VestingSchedule();
    VestingSchedule(
        uint64_t total,
        uint64_t start_time,
        uint64_t duration
    );
    
    /**
     * Calculate the amount that can be withdrawn at a given timestamp.
     * 
     * @param current_timestamp The current time
     * @return The amount available for withdrawal
     */
    uint64_t calculate_vested_amount(uint64_t current_timestamp) const;
    
    /**
     * Check if vesting is complete.
     * 
     * @param current_timestamp The current time
     * @return true if all tokens are vested
     */
    bool is_fully_vested(uint64_t current_timestamp) const;
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static VestingSchedule deserialize(const std::vector<uint8_t>& data);
};

/**
 * GenesisAllocation represents a token allocation in the genesis block.
 * 
 * Each allocation has:
 * - address: The recipient address
 * - amount: The number of tokens allocated
 * - category: Description of the allocation category (e.g., "Foundation", "Validators")
 * - vesting_schedule: Optional vesting schedule for large allocations (>2% of supply)
 */
struct GenesisAllocation {
    state::Address address;
    uint64_t amount;
    std::string category;
    std::optional<VestingSchedule> vesting_schedule;
    
    // Constructors
    GenesisAllocation();
    GenesisAllocation(
        const state::Address& addr,
        uint64_t amt,
        const std::string& cat
    );
    GenesisAllocation(
        const state::Address& addr,
        uint64_t amt,
        const std::string& cat,
        const VestingSchedule& vesting
    );
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static GenesisAllocation deserialize(const std::vector<uint8_t>& data);
};

/**
 * GenesisConfig contains the configuration for genesis block creation.
 * 
 * This includes:
 * - total_supply: The initial total supply of tokens
 * - genesis_timestamp: The timestamp of the genesis block
 * - initial_epoch: The starting epoch number (always 0)
 * - validator_set_size: The number of active validators (N)
 * - minimum_self_bond: The minimum stake required for active validators
 * - chain_id: The unique identifier for this blockchain network
 */
struct GenesisConfig {
    uint64_t total_supply;
    uint64_t genesis_timestamp;
    uint64_t initial_epoch;
    size_t validator_set_size;
    uint64_t minimum_self_bond;
    uint32_t chain_id;
    
    // Constructors
    GenesisConfig();
    GenesisConfig(
        uint64_t supply,
        uint64_t timestamp,
        size_t val_set_size,
        uint64_t min_bond,
        uint32_t chain
    );
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    static GenesisConfig deserialize(const std::vector<uint8_t>& data);
};

/**
 * GenesisBuilder constructs the genesis block with initial allocations and validator set.
 * 
 * The builder enforces the following constraints:
 * 1. No single allocation exceeds 5% of total supply (Requirement 16.2)
 * 2. Allocations exceeding 2% of total supply have 12-month vesting (Requirement 16.3)
 * 3. Total allocations do not exceed total supply
 * 4. Genesis validator set is properly initialized
 * 5. Initial epoch is set to 0 (Requirement 16.6)
 * 
 * Usage:
 *   GenesisBuilder builder(config);
 *   builder.add_allocation(address1, amount1, "Foundation");
 *   builder.add_allocation(address2, amount2, "Validators");
 *   builder.create_genesis_validator_set(validators);
 *   Block genesis = builder.build();
 */
class GenesisBuilder {
public:
    /**
     * Construct a GenesisBuilder with the given configuration.
     * 
     * @param config The genesis configuration
     */
    explicit GenesisBuilder(const GenesisConfig& config);
    
    /**
     * Add a token allocation to the genesis block.
     * 
     * This method enforces:
     * - No allocation exceeds 5% of total supply
     * - Allocations >2% automatically get 12-month vesting
     * - Total allocations do not exceed total supply
     * 
     * @param address The recipient address
     * @param amount The number of tokens to allocate
     * @param category The allocation category description
     * @return true if allocation was added successfully, false otherwise
     */
    bool add_allocation(
        const state::Address& address,
        uint64_t amount,
        const std::string& category
    );
    
    /**
     * Add a token allocation with an explicit vesting schedule.
     * 
     * This method is used for allocations >2% of total supply that require
     * a 12-month linear vesting schedule.
     * 
     * @param address The recipient address
     * @param amount The number of tokens to allocate
     * @param category The allocation category description
     * @param vesting_duration_seconds The vesting duration (default: 12 months)
     * @return true if allocation was added successfully, false otherwise
     */
    bool add_vesting_allocation(
        const state::Address& address,
        uint64_t amount,
        const std::string& category,
        uint64_t vesting_duration_seconds = 31536000  // 12 months
    );
    
    /**
     * Create the genesis validator set.
     * 
     * This method:
     * - Initializes the validator set for epoch 0
     * - Selects the top N validators by stake
     * - Enforces minimum self-bond requirement
     * - Computes the validator set Merkle root
     * 
     * @param validators The list of genesis validators
     * @return true if validator set was created successfully, false otherwise
     */
    bool create_genesis_validator_set(const std::vector<Validator>& validators);
    
    /**
     * Build the genesis block.
     * 
     * This method:
     * - Creates accounts for all allocations
     * - Initializes the validator set
     * - Constructs the genesis block header
     * - Sets epoch to 0
     * - Computes state root and validator set root
     * 
     * @return The genesis block
     * @throws std::runtime_error if genesis block cannot be built
     */
    Block build();
    
    /**
     * Get the list of all allocations.
     * 
     * @return Vector of genesis allocations
     */
    const std::vector<GenesisAllocation>& get_allocations() const;
    
    /**
     * Get the genesis validator set.
     * 
     * @return The validator set for epoch 0
     */
    const ValidatorSet& get_validator_set() const;
    
    /**
     * Get the total amount allocated.
     * 
     * @return Sum of all allocation amounts
     */
    uint64_t get_total_allocated() const;
    
    /**
     * Check if an allocation amount exceeds the 5% cap.
     * 
     * @param amount The allocation amount to check
     * @return true if amount exceeds 5% of total supply
     */
    bool exceeds_allocation_cap(uint64_t amount) const;
    
    /**
     * Check if an allocation amount requires vesting (>2% of supply).
     * 
     * @param amount The allocation amount to check
     * @return true if amount exceeds 2% of total supply
     */
    bool requires_vesting(uint64_t amount) const;
    
    /**
     * Validate all allocations and constraints.
     * 
     * @return true if all constraints are satisfied
     */
    bool validate() const;

private:
    GenesisConfig config_;
    std::vector<GenesisAllocation> allocations_;
    ValidatorSet validator_set_;
    uint64_t total_allocated_;
    bool validator_set_initialized_;
    
    // Constants
    static constexpr double ALLOCATION_CAP_PERCENTAGE = 0.05;  // 5%
    static constexpr double VESTING_THRESHOLD_PERCENTAGE = 0.02;  // 2%
    static constexpr uint64_t TWELVE_MONTHS_SECONDS = 31536000;  // 365 days
    
    /**
     * Calculate the 5% allocation cap.
     * 
     * @return Maximum allowed allocation amount
     */
    uint64_t calculate_allocation_cap() const;
    
    /**
     * Calculate the 2% vesting threshold.
     * 
     * @return Threshold above which vesting is required
     */
    uint64_t calculate_vesting_threshold() const;
    
    /**
     * Compute the state root from all allocations.
     * 
     * @return Merkle root of all genesis accounts
     */
    crypto::Blake3Hash compute_state_root() const;
};

} // namespace consensus
} // namespace sarafu
