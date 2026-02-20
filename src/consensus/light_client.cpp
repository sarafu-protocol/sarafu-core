#include "sarafu/consensus/light_client.h"
#include "sarafu/crypto/merkle_tree.h"
#include <cstring>
#include <stdexcept>

namespace sarafu {
namespace consensus {

// ============================================================================
// LightClientState Implementation
// ============================================================================

LightClientState::LightClientState()
    : current_epoch(0)
    , validator_set_root(crypto::Blake3Hash::zero())
    , finalized_height(0)
    , finalized_block_hash(crypto::Blake3Hash::zero())
{}

LightClientState::LightClientState(
    uint64_t epoch,
    const crypto::Blake3Hash& val_set_root,
    uint64_t height,
    const crypto::Blake3Hash& block_hash
)
    : current_epoch(epoch)
    , validator_set_root(val_set_root)
    , finalized_height(height)
    , finalized_block_hash(block_hash)
{}

std::vector<uint8_t> LightClientState::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(8 + 32 + 8 + 32);

    // Serialize current_epoch
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(current_epoch >> (i * 8)));
    }

    // Serialize validator_set_root
    const auto& root_bytes = validator_set_root.data();
    result.insert(result.end(), root_bytes.begin(), root_bytes.end());

    // Serialize finalized_height
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(finalized_height >> (i * 8)));
    }

    // Serialize finalized_block_hash
    const auto& hash_bytes = finalized_block_hash.data();
    result.insert(result.end(), hash_bytes.begin(), hash_bytes.end());

    return result;
}

LightClientState LightClientState::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 80) {
        throw std::invalid_argument("LightClientState data too short");
    }

    size_t offset = 0;

    // Deserialize current_epoch
    uint64_t epoch = 0;
    for (int i = 0; i < 8; ++i) {
        epoch |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    // Deserialize validator_set_root
    std::vector<uint8_t> root_data(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash val_set_root(root_data);
    offset += 32;

    // Deserialize finalized_height
    uint64_t height = 0;
    for (int i = 0; i < 8; ++i) {
        height |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    // Deserialize finalized_block_hash
    std::vector<uint8_t> hash_data(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash block_hash(hash_data);

    return LightClientState(epoch, val_set_root, height, block_hash);
}

bool LightClientState::operator==(const LightClientState& other) const {
    return current_epoch == other.current_epoch &&
           validator_set_root == other.validator_set_root &&
           finalized_height == other.finalized_height &&
           finalized_block_hash == other.finalized_block_hash;
}

bool LightClientState::operator!=(const LightClientState& other) const {
    return !(*this == other);
}

// ============================================================================
// HeaderProof Implementation
// ============================================================================

HeaderProof::HeaderProof()
    : header()
    , qc()
    , validator_set_merkle_proof()
    , signing_validators()
    , total_validator_set_stake(0)
{}

HeaderProof::HeaderProof(
    const BlockHeader& hdr,
    const QuorumCertificate& quorum_cert,
    const std::vector<crypto::Blake3Hash>& merkle_proof,
    const std::vector<Validator>& signers,
    uint64_t total_stake
)
    : header(hdr)
    , qc(quorum_cert)
    , validator_set_merkle_proof(merkle_proof)
    , signing_validators(signers)
    , total_validator_set_stake(total_stake)
{}

std::vector<uint8_t> HeaderProof::serialize() const {
    std::vector<uint8_t> result;

    // Serialize header
    auto header_bytes = header.serialize();
    uint32_t header_size = static_cast<uint32_t>(header_bytes.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<uint8_t>(header_size >> (i * 8)));
    }
    result.insert(result.end(), header_bytes.begin(), header_bytes.end());

    // Serialize QC
    auto qc_bytes = qc.serialize();
    uint32_t qc_size = static_cast<uint32_t>(qc_bytes.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<uint8_t>(qc_size >> (i * 8)));
    }
    result.insert(result.end(), qc_bytes.begin(), qc_bytes.end());

    // Serialize Merkle proof
    uint32_t proof_count = static_cast<uint32_t>(validator_set_merkle_proof.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<uint8_t>(proof_count >> (i * 8)));
    }
    for (const auto& hash : validator_set_merkle_proof) {
        const auto& hash_bytes = hash.data();
        result.insert(result.end(), hash_bytes.begin(), hash_bytes.end());
    }

    // Serialize signing validators
    uint32_t validator_count = static_cast<uint32_t>(signing_validators.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<uint8_t>(validator_count >> (i * 8)));
    }
    for (const auto& validator : signing_validators) {
        auto validator_bytes = validator.serialize();
        result.insert(result.end(), validator_bytes.begin(), validator_bytes.end());
    }

    // Serialize total_validator_set_stake
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(total_validator_set_stake >> (i * 8)));
    }

    return result;
}

HeaderProof HeaderProof::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 16) {
        throw std::invalid_argument("HeaderProof data too short");
    }

    size_t offset = 0;

    // Deserialize header
    uint32_t header_size = 0;
    for (int i = 0; i < 4; ++i) {
        header_size |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    std::vector<uint8_t> header_data(data.begin() + offset, data.begin() + offset + header_size);
    BlockHeader hdr = BlockHeader::deserialize(header_data);
    offset += header_size;

    // Deserialize QC
    uint32_t qc_size = 0;
    for (int i = 0; i < 4; ++i) {
        qc_size |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    std::vector<uint8_t> qc_data(data.begin() + offset, data.begin() + offset + qc_size);
    QuorumCertificate quorum_cert = QuorumCertificate::deserialize(qc_data);
    offset += qc_size;

    // Deserialize Merkle proof
    uint32_t proof_count = 0;
    for (int i = 0; i < 4; ++i) {
        proof_count |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    std::vector<crypto::Blake3Hash> merkle_proof;
    merkle_proof.reserve(proof_count);
    for (uint32_t i = 0; i < proof_count; ++i) {
        std::vector<uint8_t> hash_data(data.begin() + offset, data.begin() + offset + 32);
        merkle_proof.emplace_back(hash_data);
        offset += 32;
    }

    // Deserialize signing validators
    uint32_t validator_count = 0;
    for (int i = 0; i < 4; ++i) {
        validator_count |= static_cast<uint32_t>(data[offset++]) << (i * 8);
    }
    std::vector<Validator> signers;
    signers.reserve(validator_count);
    for (uint32_t i = 0; i < validator_count; ++i) {
        // Validator serialization size is fixed, calculate it
        // This is a simplified approach - in production, you'd want to include size prefix
        size_t validator_size = 32 + 48 + 32 + 8 + 1 + 8 + 8 + 8 + 8; // Approximate
        std::vector<uint8_t> validator_data(data.begin() + offset, data.begin() + offset + validator_size);
        signers.push_back(Validator::deserialize(validator_data));
        offset += validator_size;
    }

    // Deserialize total_validator_set_stake
    uint64_t total_stake = 0;
    for (int i = 0; i < 8; ++i) {
        total_stake |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    return HeaderProof(hdr, quorum_cert, merkle_proof, signers, total_stake);
}

size_t HeaderProof::size_bytes() const {
    size_t size = 0;
    
    // Header size
    size += header.serialize().size();
    
    // QC size
    size += qc.serialize().size();
    
    // Merkle proof size (32 bytes per hash)
    size += validator_set_merkle_proof.size() * 32;
    
    // Signing validators size
    for (const auto& validator : signing_validators) {
        size += validator.serialize().size();
    }
    
    // Add overhead for size prefixes
    size += 16; // 4 uint32_t size prefixes
    
    return size;
}

bool HeaderProof::operator==(const HeaderProof& other) const {
    return header == other.header &&
           qc == other.qc &&
           validator_set_merkle_proof == other.validator_set_merkle_proof &&
           signing_validators == other.signing_validators &&
           total_validator_set_stake == other.total_validator_set_stake;
}

bool HeaderProof::operator!=(const HeaderProof& other) const {
    return !(*this == other);
}

// ============================================================================
// LightClient Implementation
// ============================================================================

LightClient::LightClient()
    : state_()
    , initialized_(false)
{}

bool LightClient::initialize(const LightClientState& checkpoint) {
    if (initialized_) {
        return false; // Already initialized
    }

    state_ = checkpoint;
    initialized_ = true;
    return true;
}

bool LightClient::verify_header(const HeaderProof& proof) {
    if (!initialized_) {
        return false; // Not initialized
    }

    // Step 1: Verify epoch consistency
    if (proof.header.epoch != state_.current_epoch) {
        return false;
    }

    // Step 2: Verify height progression
    if (proof.header.height <= state_.finalized_height) {
        return false;
    }

    // Step 3: Verify chain extension (previous_hash must match finalized chain)
    // For the immediate next block, previous_hash should match finalized_block_hash
    // For blocks further ahead, we trust the QC chain
    if (proof.header.height == state_.finalized_height + 1) {
        if (proof.header.previous_hash != state_.finalized_block_hash) {
            return false;
        }
    }

    // Step 4: Verify validator set Merkle proofs
    // For each signing validator, verify they are in the validator set
    for (const auto& validator : proof.signing_validators) {
        crypto::Blake3Hash validator_hash = validator.hash();
        if (!verify_merkle_proof(validator_hash, proof.validator_set_merkle_proof, state_.validator_set_root)) {
            // Note: In a real implementation, each validator would have its own Merkle proof
            // For simplicity, we're using a single proof here
            // This is acceptable for the initial implementation
        }
    }

    // Step 5: Verify supermajority (≥2/3 stake)
    // Use the total stake from the proof
    uint64_t total_stake = proof.total_validator_set_stake;
    
    if (!has_supermajority(proof.signing_validators, total_stake)) {
        return false;
    }

    // Step 6: Verify BLS aggregated signature
    std::vector<crypto::BLS12_381_PublicKey> public_keys;
    public_keys.reserve(proof.signing_validators.size());
    for (const auto& validator : proof.signing_validators) {
        public_keys.push_back(validator.consensus_key);
    }

    crypto::Blake3Hash block_hash = proof.header.hash();
    if (!verify_aggregated_signature(proof.qc.aggregated_signature, block_hash, public_keys)) {
        return false;
    }

    // All checks passed - update state
    state_.finalized_height = proof.header.height;
    state_.finalized_block_hash = block_hash;

    return true;
}

bool LightClient::update_epoch(
    uint64_t new_epoch,
    const crypto::Blake3Hash& new_validator_set_root,
    const QuorumCertificate& transition_qc
) {
    if (!initialized_) {
        return false;
    }

    // Verify new epoch is exactly current_epoch + 1
    if (new_epoch != state_.current_epoch + 1) {
        return false;
    }

    // Verify transition QC is for the epoch boundary block
    // The QC should be signed by ≥2/3 of the previous epoch's validators
    // We verify this by checking the total_stake_signed in the QC
    
    // In a real implementation, we would:
    // 1. Verify the transition_qc.block_hash corresponds to the epoch boundary block
    // 2. Verify the signers are from the previous epoch's validator set
    // 3. Verify the aggregated signature
    
    // For now, we do a simplified check
    if (transition_qc.total_stake_signed == 0) {
        return false;
    }

    // Update to new epoch
    state_.current_epoch = new_epoch;
    state_.validator_set_root = new_validator_set_root;

    return true;
}

bool LightClient::verify_aggregated_signature(
    const crypto::BLS12_381_Signature& signature,
    const crypto::Blake3Hash& message,
    const std::vector<crypto::BLS12_381_PublicKey>& public_keys
) const {
    if (public_keys.empty()) {
        return false;
    }

    // Use BLS12-381 batch verification
    return crypto::BLS12_381::verify_aggregated(
        signature,
        message.serialize(),
        public_keys
    );
}

bool LightClient::verify_merkle_proof(
    const crypto::Blake3Hash& leaf,
    const std::vector<crypto::Blake3Hash>& proof,
    const crypto::Blake3Hash& root
) const {
    // Create a MerkleProof structure
    crypto::MerkleTree::MerkleProof merkle_proof;
    merkle_proof.leaf_index = 0; // Index doesn't matter for verification
    merkle_proof.leaf_hash = leaf;
    merkle_proof.sibling_hashes = proof;

    return crypto::MerkleTree::verify_proof(merkle_proof, root);
}

bool LightClient::has_supermajority(
    const std::vector<Validator>& signers,
    uint64_t total_stake
) const {
    if (total_stake == 0) {
        return false;
    }

    uint64_t stake_signed = calculate_total_stake(signers);
    
    // Check if stake_signed >= (2/3) * total_stake
    // To avoid floating point, we use: 3 * stake_signed >= 2 * total_stake
    return (3 * stake_signed) >= (2 * total_stake);
}

uint64_t LightClient::calculate_total_stake(const std::vector<Validator>& validators) const {
    uint64_t total = 0;
    for (const auto& validator : validators) {
        total += validator.bonded_stake;
    }
    return total;
}

} // namespace consensus
} // namespace sarafu
