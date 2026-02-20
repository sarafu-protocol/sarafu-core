#include "sarafu/consensus/validator.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace sarafu {
namespace consensus {

// ============================================================================
// Validator Implementation
// ============================================================================

Validator::Validator()
    : id(state::Address::zero()),
      consensus_key(),
      withdrawal_key(),
      bonded_stake(0),
      status(ValidatorStatus::Standby),
      jailed_until_epoch(0),
      consecutive_downtime_epochs(0),
      blocks_signed_this_epoch(0),
      blocks_missed_this_epoch(0) {
}

Validator::Validator(
    const ValidatorID& validator_id,
    const crypto::BLS12_381_PublicKey& consensus_pk,
    const crypto::Ed25519_PublicKey& withdrawal_pk,
    uint64_t stake
)
    : id(validator_id),
      consensus_key(consensus_pk),
      withdrawal_key(withdrawal_pk),
      bonded_stake(stake),
      status(ValidatorStatus::Standby),
      jailed_until_epoch(0),
      consecutive_downtime_epochs(0),
      blocks_signed_this_epoch(0),
      blocks_missed_this_epoch(0) {
}

Validator::Validator(
    const ValidatorID& validator_id,
    const crypto::BLS12_381_PublicKey& consensus_pk,
    const crypto::Ed25519_PublicKey& withdrawal_pk,
    uint64_t stake,
    ValidatorStatus stat,
    uint64_t jailed_until,
    uint64_t consecutive_downtime,
    uint64_t blocks_signed,
    uint64_t blocks_missed
)
    : id(validator_id),
      consensus_key(consensus_pk),
      withdrawal_key(withdrawal_pk),
      bonded_stake(stake),
      status(stat),
      jailed_until_epoch(jailed_until),
      consecutive_downtime_epochs(consecutive_downtime),
      blocks_signed_this_epoch(blocks_signed),
      blocks_missed_this_epoch(blocks_missed) {
}

std::vector<uint8_t> Validator::serialize() const {
    std::vector<uint8_t> result;
    
    // Reserve space for all fields
    // 32 (id) + 48 (consensus_key) + 32 (withdrawal_key) + 8 (stake) + 1 (status) + 8*5 (uint64_t fields)
    result.reserve(32 + 48 + 32 + 8 + 1 + 40);
    
    // Serialize validator ID (32 bytes)
    auto id_bytes = id.serialize();
    result.insert(result.end(), id_bytes.begin(), id_bytes.end());
    
    // Serialize consensus key (48 bytes)
    auto consensus_bytes = consensus_key.serialize();
    result.insert(result.end(), consensus_bytes.begin(), consensus_bytes.end());
    
    // Serialize withdrawal key (32 bytes)
    auto withdrawal_bytes = withdrawal_key.serialize();
    result.insert(result.end(), withdrawal_bytes.begin(), withdrawal_bytes.end());
    
    // Serialize bonded_stake (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((bonded_stake >> (i * 8)) & 0xFF));
    }
    
    // Serialize status (1 byte)
    result.push_back(static_cast<uint8_t>(status));
    
    // Serialize jailed_until_epoch (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((jailed_until_epoch >> (i * 8)) & 0xFF));
    }
    
    // Serialize consecutive_downtime_epochs (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((consecutive_downtime_epochs >> (i * 8)) & 0xFF));
    }
    
    // Serialize blocks_signed_this_epoch (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((blocks_signed_this_epoch >> (i * 8)) & 0xFF));
    }
    
    // Serialize blocks_missed_this_epoch (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((blocks_missed_this_epoch >> (i * 8)) & 0xFF));
    }
    
    return result;
}

Validator Validator::deserialize(const std::vector<uint8_t>& data) {
    // Expected size: 32 + 48 + 32 + 8 + 1 + 8 + 8 + 8 + 8 = 153 bytes
    if (data.size() < 153) {
        throw std::invalid_argument("Invalid validator data: insufficient size");
    }
    
    size_t offset = 0;
    
    // Deserialize validator ID (32 bytes)
    std::vector<uint8_t> id_bytes(data.begin() + offset, data.begin() + offset + 32);
    ValidatorID id(id_bytes);
    offset += 32;
    
    // Deserialize consensus key (48 bytes)
    std::vector<uint8_t> consensus_bytes(data.begin() + offset, data.begin() + offset + 48);
    crypto::BLS12_381_PublicKey consensus_key(consensus_bytes);
    offset += 48;
    
    // Deserialize withdrawal key (32 bytes)
    std::vector<uint8_t> withdrawal_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Ed25519_PublicKey withdrawal_key(withdrawal_bytes);
    offset += 32;
    
    // Deserialize bonded_stake (8 bytes, little-endian)
    uint64_t bonded_stake = 0;
    for (int i = 0; i < 8; ++i) {
        bonded_stake |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;
    
    // Deserialize status (1 byte)
    uint8_t status_byte = data[offset];
    if (status_byte > 3) {
        throw std::invalid_argument("Invalid validator status value");
    }
    ValidatorStatus status = static_cast<ValidatorStatus>(status_byte);
    offset += 1;
    
    // Deserialize jailed_until_epoch (8 bytes, little-endian)
    uint64_t jailed_until_epoch = 0;
    for (int i = 0; i < 8; ++i) {
        jailed_until_epoch |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;
    
    // Deserialize consecutive_downtime_epochs (8 bytes, little-endian)
    uint64_t consecutive_downtime_epochs = 0;
    for (int i = 0; i < 8; ++i) {
        consecutive_downtime_epochs |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;
    
    // Deserialize blocks_signed_this_epoch (8 bytes, little-endian)
    uint64_t blocks_signed_this_epoch = 0;
    for (int i = 0; i < 8; ++i) {
        blocks_signed_this_epoch |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;
    
    // Deserialize blocks_missed_this_epoch (8 bytes, little-endian)
    uint64_t blocks_missed_this_epoch = 0;
    for (int i = 0; i < 8; ++i) {
        blocks_missed_this_epoch |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    
    return Validator(
        id,
        consensus_key,
        withdrawal_key,
        bonded_stake,
        status,
        jailed_until_epoch,
        consecutive_downtime_epochs,
        blocks_signed_this_epoch,
        blocks_missed_this_epoch
    );
}

crypto::Blake3Hash Validator::hash() const {
    auto serialized = serialize();
    return crypto::Blake3Hash::hash(serialized);
}

bool Validator::operator==(const Validator& other) const {
    return id == other.id &&
           consensus_key == other.consensus_key &&
           withdrawal_key == other.withdrawal_key &&
           bonded_stake == other.bonded_stake &&
           status == other.status &&
           jailed_until_epoch == other.jailed_until_epoch &&
           consecutive_downtime_epochs == other.consecutive_downtime_epochs &&
           blocks_signed_this_epoch == other.blocks_signed_this_epoch &&
           blocks_missed_this_epoch == other.blocks_missed_this_epoch;
}

bool Validator::operator!=(const Validator& other) const {
    return !(*this == other);
}

bool Validator::operator<(const Validator& other) const {
    // Sort by stake descending (higher stake comes first)
    if (bonded_stake != other.bonded_stake) {
        return bonded_stake > other.bonded_stake;
    }
    
    // If stakes are equal, use lexicographic ordering by validator ID
    return id < other.id;
}

// ============================================================================
// ValidatorSet Implementation
// ============================================================================

ValidatorSet::ValidatorSet()
    : epoch(0),
      validators(),
      total_stake(0),
      merkle_root(crypto::Blake3Hash::zero()) {
}

ValidatorSet::ValidatorSet(
    uint64_t ep,
    const std::vector<Validator>& vals,
    uint64_t total_stk
)
    : epoch(ep),
      validators(vals),
      total_stake(total_stk),
      merkle_root(crypto::Blake3Hash::zero()) {
}

ValidatorSet::ValidatorSet(
    uint64_t ep,
    const std::vector<Validator>& vals,
    uint64_t total_stk,
    const crypto::Blake3Hash& root
)
    : epoch(ep),
      validators(vals),
      total_stake(total_stk),
      merkle_root(root) {
}

std::vector<Validator> ValidatorSet::get_active_validators() const {
    std::vector<Validator> active;
    for (const auto& validator : validators) {
        if (validator.status == ValidatorStatus::Active) {
            active.push_back(validator);
        }
    }
    return active;
}

std::vector<Validator> ValidatorSet::get_standby_validators() const {
    std::vector<Validator> standby;
    for (const auto& validator : validators) {
        if (validator.status == ValidatorStatus::Standby) {
            standby.push_back(validator);
        }
    }
    return standby;
}

const Validator* ValidatorSet::find_validator(const ValidatorID& id) const {
    for (const auto& validator : validators) {
        if (validator.id == id) {
            return &validator;
        }
    }
    return nullptr;
}

bool ValidatorSet::is_active(const ValidatorID& id) const {
    const Validator* validator = find_validator(id);
    return validator != nullptr && validator->status == ValidatorStatus::Active;
}

std::vector<uint8_t> ValidatorSet::serialize() const {
    std::vector<uint8_t> result;
    
    // Serialize epoch (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((epoch >> (i * 8)) & 0xFF));
    }
    
    // Serialize total_stake (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((total_stake >> (i * 8)) & 0xFF));
    }
    
    // Serialize merkle_root (32 bytes)
    auto root_bytes = merkle_root.serialize();
    result.insert(result.end(), root_bytes.begin(), root_bytes.end());
    
    // Serialize number of validators (4 bytes, little-endian)
    uint32_t num_validators = static_cast<uint32_t>(validators.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<uint8_t>((num_validators >> (i * 8)) & 0xFF));
    }
    
    // Serialize each validator
    for (const auto& validator : validators) {
        auto validator_bytes = validator.serialize();
        result.insert(result.end(), validator_bytes.begin(), validator_bytes.end());
    }
    
    return result;
}

ValidatorSet ValidatorSet::deserialize(const std::vector<uint8_t>& data) {
    // Minimum size: 8 (epoch) + 8 (total_stake) + 32 (merkle_root) + 4 (num_validators) = 52 bytes
    if (data.size() < 52) {
        throw std::invalid_argument("Invalid validator set data: insufficient size");
    }
    
    size_t offset = 0;
    
    // Deserialize epoch (8 bytes, little-endian)
    uint64_t epoch = 0;
    for (int i = 0; i < 8; ++i) {
        epoch |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;
    
    // Deserialize total_stake (8 bytes, little-endian)
    uint64_t total_stake = 0;
    for (int i = 0; i < 8; ++i) {
        total_stake |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;
    
    // Deserialize merkle_root (32 bytes)
    std::vector<uint8_t> root_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash merkle_root(root_bytes);
    offset += 32;
    
    // Deserialize number of validators (4 bytes, little-endian)
    uint32_t num_validators = 0;
    for (int i = 0; i < 4; ++i) {
        num_validators |= static_cast<uint32_t>(data[offset + i]) << (i * 8);
    }
    offset += 4;
    
    // Deserialize each validator
    std::vector<Validator> validators;
    validators.reserve(num_validators);
    
    for (uint32_t i = 0; i < num_validators; ++i) {
        if (offset + 153 > data.size()) {
            throw std::invalid_argument("Invalid validator set data: truncated validator data");
        }
        
        std::vector<uint8_t> validator_bytes(data.begin() + offset, data.begin() + offset + 153);
        validators.push_back(Validator::deserialize(validator_bytes));
        offset += 153;
    }
    
    return ValidatorSet(epoch, validators, total_stake, merkle_root);
}

bool ValidatorSet::operator==(const ValidatorSet& other) const {
    if (epoch != other.epoch || total_stake != other.total_stake || 
        merkle_root != other.merkle_root || validators.size() != other.validators.size()) {
        return false;
    }
    
    for (size_t i = 0; i < validators.size(); ++i) {
        if (validators[i] != other.validators[i]) {
            return false;
        }
    }
    
    return true;
}

bool ValidatorSet::operator!=(const ValidatorSet& other) const {
    return !(*this == other);
}

} // namespace consensus
} // namespace sarafu
