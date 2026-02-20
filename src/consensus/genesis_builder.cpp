#include "sarafu/consensus/genesis_builder.h"
#include "sarafu/crypto/merkle_tree.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace sarafu {
namespace consensus {

// ============================================================================
// VestingSchedule Implementation
// ============================================================================

VestingSchedule::VestingSchedule()
    : total_amount(0)
    , vested_amount(0)
    , start_timestamp(0)
    , duration_seconds(0) {}

VestingSchedule::VestingSchedule(
    uint64_t total,
    uint64_t start_time,
    uint64_t duration
)
    : total_amount(total)
    , vested_amount(0)
    , start_timestamp(start_time)
    , duration_seconds(duration) {}

uint64_t VestingSchedule::calculate_vested_amount(uint64_t current_timestamp) const {
    if (current_timestamp < start_timestamp) {
        return 0;
    }
    
    uint64_t elapsed = current_timestamp - start_timestamp;
    
    if (elapsed >= duration_seconds) {
        return total_amount;
    }
    
    // Linear vesting: vested = total * (elapsed / duration)
    // Use 128-bit arithmetic to avoid overflow
    __uint128_t vested = (__uint128_t)total_amount * elapsed / duration_seconds;
    return static_cast<uint64_t>(vested);
}

bool VestingSchedule::is_fully_vested(uint64_t current_timestamp) const {
    return current_timestamp >= start_timestamp + duration_seconds;
}

std::vector<uint8_t> VestingSchedule::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(32);
    
    // Serialize fields in order
    auto append_uint64 = [&data](uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }
    };
    
    append_uint64(total_amount);
    append_uint64(vested_amount);
    append_uint64(start_timestamp);
    append_uint64(duration_seconds);
    
    return data;
}

VestingSchedule VestingSchedule::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 32) {
        throw std::invalid_argument("Invalid vesting schedule data size");
    }
    
    auto read_uint64 = [&data](size_t offset) -> uint64_t {
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
        }
        return value;
    };
    
    VestingSchedule schedule;
    schedule.total_amount = read_uint64(0);
    schedule.vested_amount = read_uint64(8);
    schedule.start_timestamp = read_uint64(16);
    schedule.duration_seconds = read_uint64(24);
    
    return schedule;
}

// ============================================================================
// GenesisAllocation Implementation
// ============================================================================

GenesisAllocation::GenesisAllocation()
    : address(state::Address::zero())
    , amount(0)
    , category("")
    , vesting_schedule(std::nullopt) {}

GenesisAllocation::GenesisAllocation(
    const state::Address& addr,
    uint64_t amt,
    const std::string& cat
)
    : address(addr)
    , amount(amt)
    , category(cat)
    , vesting_schedule(std::nullopt) {}

GenesisAllocation::GenesisAllocation(
    const state::Address& addr,
    uint64_t amt,
    const std::string& cat,
    const VestingSchedule& vesting
)
    : address(addr)
    , amount(amt)
    , category(cat)
    , vesting_schedule(vesting) {}

std::vector<uint8_t> GenesisAllocation::serialize() const {
    std::vector<uint8_t> data;
    
    // Serialize address
    auto addr_data = address.serialize();
    data.insert(data.end(), addr_data.begin(), addr_data.end());
    
    // Serialize amount
    for (int i = 0; i < 8; ++i) {
        data.push_back(static_cast<uint8_t>(amount >> (i * 8)));
    }
    
    // Serialize category length and string
    uint32_t cat_len = static_cast<uint32_t>(category.size());
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<uint8_t>(cat_len >> (i * 8)));
    }
    data.insert(data.end(), category.begin(), category.end());
    
    // Serialize vesting schedule flag and data
    if (vesting_schedule.has_value()) {
        data.push_back(1);
        auto vesting_data = vesting_schedule->serialize();
        data.insert(data.end(), vesting_data.begin(), vesting_data.end());
    } else {
        data.push_back(0);
    }
    
    return data;
}

GenesisAllocation GenesisAllocation::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 45) {  // 32 (address) + 8 (amount) + 4 (cat_len) + 1 (vesting flag)
        throw std::invalid_argument("Invalid genesis allocation data size");
    }
    
    size_t offset = 0;
    
    // Deserialize address
    std::vector<uint8_t> addr_data(data.begin(), data.begin() + 32);
    state::Address address(addr_data);
    offset += 32;
    
    // Deserialize amount
    uint64_t amount = 0;
    for (int i = 0; i < 8; ++i) {
        amount |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;
    
    // Deserialize category
    uint32_t cat_len = 0;
    for (int i = 0; i < 4; ++i) {
        cat_len |= static_cast<uint32_t>(data[offset + i]) << (i * 8);
    }
    offset += 4;
    
    std::string category(data.begin() + offset, data.begin() + offset + cat_len);
    offset += cat_len;
    
    // Deserialize vesting schedule
    GenesisAllocation allocation(address, amount, category);
    
    if (offset < data.size() && data[offset] == 1) {
        offset++;
        std::vector<uint8_t> vesting_data(data.begin() + offset, data.end());
        allocation.vesting_schedule = VestingSchedule::deserialize(vesting_data);
    }
    
    return allocation;
}

// ============================================================================
// GenesisConfig Implementation
// ============================================================================

GenesisConfig::GenesisConfig()
    : total_supply(0)
    , genesis_timestamp(0)
    , initial_epoch(0)
    , validator_set_size(0)
    , minimum_self_bond(0)
    , chain_id(0) {}

GenesisConfig::GenesisConfig(
    uint64_t supply,
    uint64_t timestamp,
    size_t val_set_size,
    uint64_t min_bond,
    uint32_t chain
)
    : total_supply(supply)
    , genesis_timestamp(timestamp)
    , initial_epoch(0)  // Always 0 for genesis
    , validator_set_size(val_set_size)
    , minimum_self_bond(min_bond)
    , chain_id(chain) {}

std::vector<uint8_t> GenesisConfig::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(44);
    
    auto append_uint64 = [&data](uint64_t value) {
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<uint8_t>(value >> (i * 8)));
        }
    };
    
    append_uint64(total_supply);
    append_uint64(genesis_timestamp);
    append_uint64(initial_epoch);
    append_uint64(validator_set_size);
    append_uint64(minimum_self_bond);
    
    for (int i = 0; i < 4; ++i) {
        data.push_back(static_cast<uint8_t>(chain_id >> (i * 8)));
    }
    
    return data;
}

GenesisConfig GenesisConfig::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 44) {
        throw std::invalid_argument("Invalid genesis config data size");
    }
    
    auto read_uint64 = [&data](size_t offset) -> uint64_t {
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
        }
        return value;
    };
    
    GenesisConfig config;
    config.total_supply = read_uint64(0);
    config.genesis_timestamp = read_uint64(8);
    config.initial_epoch = read_uint64(16);
    config.validator_set_size = static_cast<size_t>(read_uint64(24));
    config.minimum_self_bond = read_uint64(32);
    
    config.chain_id = 0;
    for (int i = 0; i < 4; ++i) {
        config.chain_id |= static_cast<uint32_t>(data[40 + i]) << (i * 8);
    }
    
    return config;
}

// ============================================================================
// GenesisBuilder Implementation
// ============================================================================

GenesisBuilder::GenesisBuilder(const GenesisConfig& config)
    : config_(config)
    , allocations_()
    , validator_set_()
    , total_allocated_(0)
    , validator_set_initialized_(false) {}

bool GenesisBuilder::add_allocation(
    const state::Address& address,
    uint64_t amount,
    const std::string& category
) {
    // Check if allocation exceeds 5% cap
    if (exceeds_allocation_cap(amount)) {
        return false;
    }
    
    // Check if total would exceed supply
    if (total_allocated_ + amount > config_.total_supply) {
        return false;
    }
    
    // Check if allocation requires vesting (>2%)
    if (requires_vesting(amount)) {
        // Automatically add vesting schedule
        return add_vesting_allocation(address, amount, category);
    }
    
    // Add allocation without vesting
    allocations_.emplace_back(address, amount, category);
    total_allocated_ += amount;
    
    return true;
}

bool GenesisBuilder::add_vesting_allocation(
    const state::Address& address,
    uint64_t amount,
    const std::string& category,
    uint64_t vesting_duration_seconds
) {
    // Check if allocation exceeds 5% cap
    if (exceeds_allocation_cap(amount)) {
        return false;
    }
    
    // Check if total would exceed supply
    if (total_allocated_ + amount > config_.total_supply) {
        return false;
    }
    
    // Create vesting schedule
    VestingSchedule vesting(
        amount,
        config_.genesis_timestamp,
        vesting_duration_seconds
    );
    
    // Add allocation with vesting
    allocations_.emplace_back(address, amount, category, vesting);
    total_allocated_ += amount;
    
    return true;
}

bool GenesisBuilder::create_genesis_validator_set(const std::vector<Validator>& validators) {
    if (validators.empty()) {
        return false;
    }
    
    // Sort validators by stake (descending)
    std::vector<Validator> sorted_validators = validators;
    std::sort(sorted_validators.begin(), sorted_validators.end(),
        [](const Validator& a, const Validator& b) {
            if (a.bonded_stake != b.bonded_stake) {
                return a.bonded_stake > b.bonded_stake;
            }
            // Lexicographic ordering for ties
            return a.id < b.id;
        });
    
    // Select top N validators
    size_t active_count = std::min(config_.validator_set_size, sorted_validators.size());
    std::vector<Validator> active_validators;
    std::vector<Validator> all_validators;
    
    uint64_t total_stake = 0;
    
    for (size_t i = 0; i < sorted_validators.size(); ++i) {
        Validator val = sorted_validators[i];
        
        // Check minimum self-bond requirement for active validators
        if (i < active_count) {
            if (val.bonded_stake < config_.minimum_self_bond) {
                return false;  // Active validator doesn't meet minimum stake
            }
            val.status = ValidatorStatus::Active;
            total_stake += val.bonded_stake;
            active_validators.push_back(val);
        } else {
            val.status = ValidatorStatus::Standby;
        }
        
        // Reset epoch counters for genesis
        val.jailed_until_epoch = 0;
        val.consecutive_downtime_epochs = 0;
        val.blocks_signed_this_epoch = 0;
        val.blocks_missed_this_epoch = 0;
        
        all_validators.push_back(val);
    }
    
    // Create validator set for epoch 0
    validator_set_ = ValidatorSet(config_.initial_epoch, all_validators, total_stake);
    
    // Compute Merkle root
    std::vector<crypto::Blake3Hash> validator_hashes;
    for (const auto& val : all_validators) {
        validator_hashes.push_back(val.hash());
    }
    
    crypto::MerkleTree merkle_tree;
    merkle_tree.build_tree(validator_hashes);
    validator_set_.merkle_root = merkle_tree.get_root();
    
    validator_set_initialized_ = true;
    
    return true;
}

Block GenesisBuilder::build() {
    if (!validate()) {
        throw std::runtime_error("Genesis builder validation failed");
    }
    
    if (!validator_set_initialized_) {
        throw std::runtime_error("Validator set not initialized");
    }
    
    // Compute state root from allocations
    crypto::Blake3Hash state_root = compute_state_root();
    
    // Genesis block has no transactions
    std::vector<state::Transaction> transactions;
    
    // Compute transactions root (empty for genesis)
    crypto::MerkleTree tx_tree;
    tx_tree.build_tree({});
    crypto::Blake3Hash transactions_root = tx_tree.get_root();
    
    // Create genesis block header
    BlockHeader header(
        0,  // height = 0 for genesis
        config_.genesis_timestamp,
        crypto::Blake3Hash(),  // previous_hash is zero for genesis
        state_root,
        transactions_root,
        validator_set_.merkle_root,
        state::Address::zero(),  // proposer is zero for genesis
        config_.initial_epoch  // epoch = 0
    );
    
    // Create empty QC for genesis (no parent to justify)
    QuorumCertificate genesis_qc(
        0,  // block_height
        crypto::Blake3Hash(),  // block_hash
        0,  // view_number
        crypto::BLS12_381_Signature(),  // empty signature
        std::vector<ValidatorID>(),  // no signers
        0  // total_stake_signed
    );
    
    // Create genesis block
    Block genesis_block(header, transactions, genesis_qc);
    
    return genesis_block;
}

const std::vector<GenesisAllocation>& GenesisBuilder::get_allocations() const {
    return allocations_;
}

const ValidatorSet& GenesisBuilder::get_validator_set() const {
    return validator_set_;
}

uint64_t GenesisBuilder::get_total_allocated() const {
    return total_allocated_;
}

bool GenesisBuilder::exceeds_allocation_cap(uint64_t amount) const {
    uint64_t cap = calculate_allocation_cap();
    return amount > cap;
}

bool GenesisBuilder::requires_vesting(uint64_t amount) const {
    uint64_t threshold = calculate_vesting_threshold();
    return amount > threshold;
}

bool GenesisBuilder::validate() const {
    // Check that total allocated doesn't exceed supply
    if (total_allocated_ > config_.total_supply) {
        return false;
    }
    
    // Check that all allocations respect the 5% cap
    uint64_t cap = calculate_allocation_cap();
    for (const auto& allocation : allocations_) {
        if (allocation.amount > cap) {
            return false;
        }
        
        // Check that allocations >2% have vesting
        uint64_t threshold = calculate_vesting_threshold();
        if (allocation.amount > threshold && !allocation.vesting_schedule.has_value()) {
            return false;
        }
    }
    
    return true;
}

uint64_t GenesisBuilder::calculate_allocation_cap() const {
    // 5% of total supply
    return static_cast<uint64_t>(config_.total_supply * ALLOCATION_CAP_PERCENTAGE);
}

uint64_t GenesisBuilder::calculate_vesting_threshold() const {
    // 2% of total supply
    return static_cast<uint64_t>(config_.total_supply * VESTING_THRESHOLD_PERCENTAGE);
}

crypto::Blake3Hash GenesisBuilder::compute_state_root() const {
    // Create account hashes for all allocations
    std::vector<crypto::Blake3Hash> account_hashes;
    
    for (const auto& allocation : allocations_) {
        // Create account with initial balance and nonce 0
        state::Account account(allocation.address, allocation.amount, 0);
        account_hashes.push_back(account.hash());
    }
    
    // Build Merkle tree
    if (account_hashes.empty()) {
        // Empty state root
        return crypto::Blake3Hash();
    }
    
    // Sort account hashes for deterministic ordering (same as state machine)
    std::sort(account_hashes.begin(), account_hashes.end());
    
    crypto::MerkleTree merkle_tree;
    merkle_tree.build_tree(account_hashes);
    return merkle_tree.get_root();
}

} // namespace consensus
} // namespace sarafu
