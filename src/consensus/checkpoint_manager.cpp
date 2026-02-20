#include "sarafu/consensus/checkpoint_manager.h"
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace sarafu {
namespace consensus {

// WeakSubjectivityCheckpoint implementation

WeakSubjectivityCheckpoint::WeakSubjectivityCheckpoint()
    : block_height(0),
      block_hash(),
      epoch(0),
      timestamp(0),
      state_root(),
      validator_set_root(),
      aggregated_signature(),
      signers(),
      total_stake_signed(0) {}

WeakSubjectivityCheckpoint::WeakSubjectivityCheckpoint(
    uint64_t height,
    const crypto::Blake3Hash& hash,
    uint64_t ep,
    uint64_t ts,
    const crypto::Blake3Hash& state_rt,
    const crypto::Blake3Hash& val_rt
) : block_height(height),
    block_hash(hash),
    epoch(ep),
    timestamp(ts),
    state_root(state_rt),
    validator_set_root(val_rt),
    aggregated_signature(),
    signers(),
    total_stake_signed(0) {}

crypto::Blake3Hash WeakSubjectivityCheckpoint::hash() const {
    std::vector<uint8_t> data;
    data.reserve(32 + 32 + 8 + 8 + 8 + 32 + 32);
    
    // Serialize checkpoint data (excluding signature fields)
    auto height_bytes = reinterpret_cast<const uint8_t*>(&block_height);
    data.insert(data.end(), height_bytes, height_bytes + sizeof(block_height));
    
    auto block_hash_bytes = block_hash.serialize();
    data.insert(data.end(), block_hash_bytes.begin(), block_hash_bytes.end());
    
    auto epoch_bytes = reinterpret_cast<const uint8_t*>(&epoch);
    data.insert(data.end(), epoch_bytes, epoch_bytes + sizeof(epoch));
    
    auto timestamp_bytes = reinterpret_cast<const uint8_t*>(&timestamp);
    data.insert(data.end(), timestamp_bytes, timestamp_bytes + sizeof(timestamp));
    
    auto state_root_bytes = state_root.serialize();
    data.insert(data.end(), state_root_bytes.begin(), state_root_bytes.end());
    
    auto val_root_bytes = validator_set_root.serialize();
    data.insert(data.end(), val_root_bytes.begin(), val_root_bytes.end());
    
    return crypto::Blake3Hash::hash(data);
}

std::vector<uint8_t> WeakSubjectivityCheckpoint::serialize() const {
    std::vector<uint8_t> data;
    
    // Serialize all fields
    auto height_bytes = reinterpret_cast<const uint8_t*>(&block_height);
    data.insert(data.end(), height_bytes, height_bytes + sizeof(block_height));
    
    auto block_hash_bytes = block_hash.serialize();
    data.insert(data.end(), block_hash_bytes.begin(), block_hash_bytes.end());
    
    auto epoch_bytes = reinterpret_cast<const uint8_t*>(&epoch);
    data.insert(data.end(), epoch_bytes, epoch_bytes + sizeof(epoch));
    
    auto timestamp_bytes = reinterpret_cast<const uint8_t*>(&timestamp);
    data.insert(data.end(), timestamp_bytes, timestamp_bytes + sizeof(timestamp));
    
    auto state_root_bytes = state_root.serialize();
    data.insert(data.end(), state_root_bytes.begin(), state_root_bytes.end());
    
    auto val_root_bytes = validator_set_root.serialize();
    data.insert(data.end(), val_root_bytes.begin(), val_root_bytes.end());
    
    // Serialize signature
    auto sig_bytes = aggregated_signature.serialize();
    data.insert(data.end(), sig_bytes.begin(), sig_bytes.end());
    
    // Serialize signers count
    uint64_t signers_count = signers.size();
    auto count_bytes = reinterpret_cast<const uint8_t*>(&signers_count);
    data.insert(data.end(), count_bytes, count_bytes + sizeof(signers_count));
    
    // Serialize each signer
    for (const auto& signer : signers) {
        auto signer_bytes = signer.serialize();
        data.insert(data.end(), signer_bytes.begin(), signer_bytes.end());
    }
    
    // Serialize total stake
    auto stake_bytes = reinterpret_cast<const uint8_t*>(&total_stake_signed);
    data.insert(data.end(), stake_bytes, stake_bytes + sizeof(total_stake_signed));
    
    return data;
}

WeakSubjectivityCheckpoint WeakSubjectivityCheckpoint::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8 + 32 + 8 + 8 + 32 + 32 + 96 + 8 + 8) {
        throw std::invalid_argument("Insufficient data for checkpoint deserialization");
    }
    
    size_t offset = 0;
    WeakSubjectivityCheckpoint checkpoint;
    
    // Deserialize block_height
    std::memcpy(&checkpoint.block_height, data.data() + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    // Deserialize block_hash
    std::vector<uint8_t> block_hash_bytes(data.begin() + offset, data.begin() + offset + 32);
    checkpoint.block_hash = crypto::Blake3Hash(block_hash_bytes);
    offset += 32;
    
    // Deserialize epoch
    std::memcpy(&checkpoint.epoch, data.data() + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    // Deserialize timestamp
    std::memcpy(&checkpoint.timestamp, data.data() + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    // Deserialize state_root
    std::vector<uint8_t> state_root_bytes(data.begin() + offset, data.begin() + offset + 32);
    checkpoint.state_root = crypto::Blake3Hash(state_root_bytes);
    offset += 32;
    
    // Deserialize validator_set_root
    std::vector<uint8_t> val_root_bytes(data.begin() + offset, data.begin() + offset + 32);
    checkpoint.validator_set_root = crypto::Blake3Hash(val_root_bytes);
    offset += 32;
    
    // Deserialize aggregated_signature
    std::vector<uint8_t> sig_bytes(data.begin() + offset, data.begin() + offset + 96);
    checkpoint.aggregated_signature = crypto::BLS12_381_Signature(sig_bytes);
    offset += 96;
    
    // Deserialize signers count
    uint64_t signers_count;
    std::memcpy(&signers_count, data.data() + offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);
    
    // Deserialize signers
    checkpoint.signers.reserve(signers_count);
    for (uint64_t i = 0; i < signers_count; ++i) {
        if (offset + 32 > data.size()) {
            throw std::invalid_argument("Insufficient data for signer deserialization");
        }
        std::vector<uint8_t> signer_bytes(data.begin() + offset, data.begin() + offset + 32);
        checkpoint.signers.push_back(state::Address(signer_bytes));
        offset += 32;
    }
    
    // Deserialize total_stake_signed
    if (offset + sizeof(uint64_t) > data.size()) {
        throw std::invalid_argument("Insufficient data for total_stake_signed");
    }
    std::memcpy(&checkpoint.total_stake_signed, data.data() + offset, sizeof(uint64_t));
    
    return checkpoint;
}

bool WeakSubjectivityCheckpoint::operator==(const WeakSubjectivityCheckpoint& other) const {
    return block_height == other.block_height &&
           block_hash == other.block_hash &&
           epoch == other.epoch &&
           timestamp == other.timestamp &&
           state_root == other.state_root &&
           validator_set_root == other.validator_set_root &&
           aggregated_signature == other.aggregated_signature &&
           signers == other.signers &&
           total_stake_signed == other.total_stake_signed;
}

bool WeakSubjectivityCheckpoint::operator!=(const WeakSubjectivityCheckpoint& other) const {
    return !(*this == other);
}

// CheckpointManager implementation

CheckpointManager::CheckpointManager(const Config& config)
    : config_(config) {}

bool CheckpointManager::should_produce_checkpoint(uint64_t block_height) const {
    return block_height > 0 && (block_height % config_.checkpoint_interval) == 0;
}

WeakSubjectivityCheckpoint CheckpointManager::produce_checkpoint(const Block& block) const {
    return WeakSubjectivityCheckpoint(
        block.header.height,
        block.hash(),
        block.header.epoch,
        block.header.timestamp,
        block.header.state_root,
        block.header.validator_set_root
    );
}

crypto::BLS12_381_Signature CheckpointManager::sign_checkpoint(
    const WeakSubjectivityCheckpoint& checkpoint,
    const ValidatorID& validator_id,
    const crypto::BLS12_381_PrivateKey& private_key
) const {
    // Sign the checkpoint hash
    std::vector<uint8_t> message = checkpoint.hash().serialize();
    return crypto::BLS12_381::sign(message, private_key);
}

std::optional<WeakSubjectivityCheckpoint> CheckpointManager::aggregate_signatures(
    const WeakSubjectivityCheckpoint& checkpoint,
    const std::map<ValidatorID, crypto::BLS12_381_Signature>& signatures,
    const ValidatorSet& validator_set
) const {
    // Calculate required stake (≥2/3)
    uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;
    
    // Collect signatures and calculate total stake
    std::vector<crypto::BLS12_381_Signature> sig_list;
    std::vector<ValidatorID> signer_list;
    uint64_t total_stake = 0;
    
    for (const auto& [validator_id, signature] : signatures) {
        // Find validator in set
        bool found = false;
        for (const auto& validator : validator_set.validators) {
            if (validator.id == validator_id) {
                sig_list.push_back(signature);
                signer_list.push_back(validator_id);
                total_stake += validator.bonded_stake;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Signature from non-validator, skip
            continue;
        }
    }
    
    // Check if we have enough stake
    if (total_stake < required_stake) {
        return std::nullopt;
    }
    
    // Aggregate signatures
    crypto::BLS12_381_Signature aggregated_sig = crypto::BLS12_381::aggregate(sig_list);
    
    // Create signed checkpoint
    WeakSubjectivityCheckpoint signed_checkpoint = checkpoint;
    signed_checkpoint.aggregated_signature = aggregated_sig;
    signed_checkpoint.signers = signer_list;
    signed_checkpoint.total_stake_signed = total_stake;
    
    return signed_checkpoint;
}

std::vector<uint8_t> CheckpointManager::serialize_checkpoint(
    const WeakSubjectivityCheckpoint& checkpoint
) const {
    return checkpoint.serialize();
}

bool CheckpointManager::verify_checkpoint(
    const WeakSubjectivityCheckpoint& checkpoint,
    const ValidatorSet& validator_set
) const {
    // Check supermajority (≥2/3 stake)
    uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;
    if (checkpoint.total_stake_signed < required_stake) {
        return false;
    }
    
    // Collect public keys of signers and verify they're in validator set
    std::vector<crypto::BLS12_381_PublicKey> signer_public_keys;
    uint64_t verified_stake = 0;
    
    for (const auto& signer_id : checkpoint.signers) {
        bool found = false;
        for (const auto& validator : validator_set.validators) {
            if (validator.id == signer_id) {
                signer_public_keys.push_back(validator.consensus_key);
                verified_stake += validator.bonded_stake;
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Signer not in validator set
            return false;
        }
    }
    
    // Verify stake matches
    if (verified_stake != checkpoint.total_stake_signed) {
        return false;
    }
    
    // Verify aggregated signature
    std::vector<uint8_t> message = checkpoint.hash().serialize();
    return crypto::BLS12_381::verify_aggregated(
        checkpoint.aggregated_signature,
        message,
        signer_public_keys
    );
}

bool CheckpointManager::check_checkpoint_age(
    uint64_t checkpoint_height,
    uint64_t current_height
) const {
    if (current_height < checkpoint_height) {
        return false;
    }
    
    uint64_t age = current_height - checkpoint_height;
    return age < config_.max_checkpoint_age;
}

bool CheckpointManager::verify_chain_includes_checkpoint(
    const WeakSubjectivityCheckpoint& checkpoint,
    const Block& chain_block
) const {
    // Verify block height matches
    if (chain_block.header.height != checkpoint.block_height) {
        return false;
    }
    
    // Verify block hash matches
    if (chain_block.hash() != checkpoint.block_hash) {
        return false;
    }
    
    // Verify state root matches
    if (chain_block.header.state_root != checkpoint.state_root) {
        return false;
    }
    
    // Verify validator set root matches
    if (chain_block.header.validator_set_root != checkpoint.validator_set_root) {
        return false;
    }
    
    return true;
}

bool CheckpointManager::reject_chain_without_checkpoint(
    const WeakSubjectivityCheckpoint& checkpoint,
    const Block& chain_block
) const {
    // Return false (reject) if chain doesn't include checkpoint
    return !verify_chain_includes_checkpoint(checkpoint, chain_block);
}

} // namespace consensus
} // namespace sarafu
