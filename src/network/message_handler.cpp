#include "sarafu/network/message_handler.h"
#include <cstring>
#include <stdexcept>

namespace sarafu {
namespace network {

// ============================================================================
// Vote Implementation
// ============================================================================

Vote::Vote()
    : block_height(0),
      block_hash(),
      view_number(0),
      validator_id(),
      signature() {}

Vote::Vote(
    uint64_t height,
    const crypto::Blake3Hash& hash,
    uint64_t view,
    const consensus::ValidatorID& validator,
    const crypto::BLS12_381_Signature& sig
)
    : block_height(height),
      block_hash(hash),
      view_number(view),
      validator_id(validator),
      signature(sig) {}

std::vector<uint8_t> Vote::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(8 + 32 + 8 + 32 + 96);
    
    // Serialize block_height (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        result.push_back((block_height >> (i * 8)) & 0xFF);
    }
    
    // Serialize block_hash (32 bytes)
    auto hash_bytes = block_hash.data();
    result.insert(result.end(), hash_bytes.begin(), hash_bytes.end());
    
    // Serialize view_number (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        result.push_back((view_number >> (i * 8)) & 0xFF);
    }
    
    // Serialize validator_id (32 bytes)
    auto validator_bytes = validator_id.data();
    result.insert(result.end(), validator_bytes.begin(), validator_bytes.end());
    
    // Serialize signature (96 bytes)
    auto sig_bytes = signature.serialize();
    result.insert(result.end(), sig_bytes.begin(), sig_bytes.end());
    
    return result;
}

Vote Vote::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8 + 32 + 8 + 32 + 96) {
        throw std::invalid_argument("Vote::deserialize: data too short");
    }
    
    size_t offset = 0;
    
    // Deserialize block_height
    uint64_t height = 0;
    for (int i = 0; i < 8; ++i) {
        height = (height << 8) | data[offset++];
    }
    
    // Deserialize block_hash
    std::vector<uint8_t> hash_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash hash(hash_bytes);
    offset += 32;
    
    // Deserialize view_number
    uint64_t view = 0;
    for (int i = 0; i < 8; ++i) {
        view = (view << 8) | data[offset++];
    }
    
    // Deserialize validator_id
    std::vector<uint8_t> validator_bytes(data.begin() + offset, data.begin() + offset + 32);
    consensus::ValidatorID validator(validator_bytes);
    offset += 32;
    
    // Deserialize signature
    std::vector<uint8_t> sig_bytes(data.begin() + offset, data.begin() + offset + 96);
    // TODO: BLS12_381_Signature needs a deserialize method or constructor from bytes
    crypto::BLS12_381_Signature sig;  // Default for now
    
    return Vote(height, hash, view, validator, sig);
}

bool Vote::operator==(const Vote& other) const {
    return block_height == other.block_height &&
           block_hash == other.block_hash &&
           view_number == other.view_number &&
           validator_id == other.validator_id &&
           signature == other.signature;
}

bool Vote::operator!=(const Vote& other) const {
    return !(*this == other);
}

// ============================================================================
// BlockRequest Implementation
// ============================================================================

BlockRequest::BlockRequest()
    : type(RequestType::ByHeight),
      height(0),
      hash(),
      requester_id() {}

BlockRequest BlockRequest::by_height(uint64_t h, const consensus::ValidatorID& requester) {
    BlockRequest req;
    req.type = RequestType::ByHeight;
    req.height = h;
    req.requester_id = requester;
    return req;
}

BlockRequest BlockRequest::by_hash(const crypto::Blake3Hash& hash, const consensus::ValidatorID& requester) {
    BlockRequest req;
    req.type = RequestType::ByHash;
    req.hash = hash;
    req.requester_id = requester;
    return req;
}

std::vector<uint8_t> BlockRequest::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(1 + 8 + 32 + 32);
    
    // Serialize type (1 byte)
    result.push_back(static_cast<uint8_t>(type));
    
    // Serialize height (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        result.push_back((height >> (i * 8)) & 0xFF);
    }
    
    // Serialize hash (32 bytes)
    auto hash_bytes = hash.data();
    result.insert(result.end(), hash_bytes.begin(), hash_bytes.end());
    
    // Serialize requester_id (32 bytes)
    auto requester_bytes = requester_id.data();
    result.insert(result.end(), requester_bytes.begin(), requester_bytes.end());
    
    return result;
}

BlockRequest BlockRequest::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 1 + 8 + 32 + 32) {
        throw std::invalid_argument("BlockRequest::deserialize: data too short");
    }
    
    size_t offset = 0;
    
    // Deserialize type
    RequestType req_type = static_cast<RequestType>(data[offset++]);
    
    // Deserialize height
    uint64_t h = 0;
    for (int i = 0; i < 8; ++i) {
        h = (h << 8) | data[offset++];
    }
    
    // Deserialize hash
    std::vector<uint8_t> hash_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash req_hash(hash_bytes);
    offset += 32;
    
    // Deserialize requester_id
    std::vector<uint8_t> requester_bytes(data.begin() + offset, data.begin() + offset + 32);
    consensus::ValidatorID requester(requester_bytes);
    
    BlockRequest req;
    req.type = req_type;
    req.height = h;
    req.hash = req_hash;
    req.requester_id = requester;
    
    return req;
}

bool BlockRequest::operator==(const BlockRequest& other) const {
    return type == other.type &&
           height == other.height &&
           hash == other.hash &&
           requester_id == other.requester_id;
}

bool BlockRequest::operator!=(const BlockRequest& other) const {
    return !(*this == other);
}

// ============================================================================
// BlockResponse Implementation
// ============================================================================

BlockResponse::BlockResponse()
    : block(),
      found(false),
      error("") {}

BlockResponse::BlockResponse(const consensus::Block& blk, bool fnd, const std::string& err)
    : block(blk),
      found(fnd),
      error(err) {}

std::vector<uint8_t> BlockResponse::serialize() const {
    std::vector<uint8_t> result;
    
    // Serialize found flag (1 byte)
    result.push_back(found ? 1 : 0);
    
    // Serialize error length (4 bytes, big-endian)
    uint32_t error_len = static_cast<uint32_t>(error.size());
    result.push_back((error_len >> 24) & 0xFF);
    result.push_back((error_len >> 16) & 0xFF);
    result.push_back((error_len >> 8) & 0xFF);
    result.push_back(error_len & 0xFF);
    
    // Serialize error
    result.insert(result.end(), error.begin(), error.end());
    
    // Serialize block if found
    if (found) {
        auto block_bytes = block.serialize();
        result.insert(result.end(), block_bytes.begin(), block_bytes.end());
    }
    
    return result;
}

BlockResponse BlockResponse::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 1 + 4) {
        throw std::invalid_argument("BlockResponse::deserialize: data too short");
    }
    
    size_t offset = 0;
    
    // Deserialize found flag
    bool fnd = (data[offset++] != 0);
    
    // Deserialize error length
    uint32_t error_len = (static_cast<uint32_t>(data[offset]) << 24) |
                         (static_cast<uint32_t>(data[offset + 1]) << 16) |
                         (static_cast<uint32_t>(data[offset + 2]) << 8) |
                         static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    if (data.size() < offset + error_len) {
        throw std::invalid_argument("BlockResponse::deserialize: invalid error length");
    }
    
    // Deserialize error
    std::string err(data.begin() + offset, data.begin() + offset + error_len);
    offset += error_len;
    
    // Deserialize block if found
    consensus::Block blk;
    if (fnd) {
        std::vector<uint8_t> block_bytes(data.begin() + offset, data.end());
        blk = consensus::Block::deserialize(block_bytes);
    }
    
    return BlockResponse(blk, fnd, err);
}

bool BlockResponse::operator==(const BlockResponse& other) const {
    return found == other.found &&
           error == other.error &&
           (!found || block == other.block);
}

bool BlockResponse::operator!=(const BlockResponse& other) const {
    return !(*this == other);
}

// ============================================================================
// ValidatorInfo Implementation
// ============================================================================

ValidatorInfo::ValidatorInfo()
    : validator_id(),
      consensus_key(),
      bonded_stake(0),
      is_active(false) {}

ValidatorInfo::ValidatorInfo(
    const consensus::ValidatorID& id,
    const crypto::BLS12_381_PublicKey& key,
    uint64_t stake,
    bool active
)
    : validator_id(id),
      consensus_key(key),
      bonded_stake(stake),
      is_active(active) {}

std::vector<uint8_t> ValidatorInfo::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(32 + 48 + 8 + 1);
    
    // Serialize validator_id (32 bytes)
    auto id_bytes = validator_id.data();
    result.insert(result.end(), id_bytes.begin(), id_bytes.end());
    
    // Serialize consensus_key (48 bytes)
    auto key_bytes = consensus_key.serialize();
    result.insert(result.end(), key_bytes.begin(), key_bytes.end());
    
    // Serialize bonded_stake (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        result.push_back((bonded_stake >> (i * 8)) & 0xFF);
    }
    
    // Serialize is_active (1 byte)
    result.push_back(is_active ? 1 : 0);
    
    return result;
}

ValidatorInfo ValidatorInfo::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 32 + 48 + 8 + 1) {
        throw std::invalid_argument("ValidatorInfo::deserialize: data too short");
    }
    
    size_t offset = 0;
    
    // Deserialize validator_id
    std::vector<uint8_t> id_bytes(data.begin() + offset, data.begin() + offset + 32);
    consensus::ValidatorID id(id_bytes);
    offset += 32;
    
    // Deserialize consensus_key
    std::vector<uint8_t> key_bytes(data.begin() + offset, data.begin() + offset + 48);
    // TODO: BLS12_381_PublicKey needs a deserialize method or constructor from bytes
    crypto::BLS12_381_PublicKey key(key_bytes);
    offset += 48;
    
    // Deserialize bonded_stake
    uint64_t stake = 0;
    for (int i = 0; i < 8; ++i) {
        stake = (stake << 8) | data[offset++];
    }
    
    // Deserialize is_active
    bool active = (data[offset] != 0);
    
    return ValidatorInfo(id, key, stake, active);
}

bool ValidatorInfo::operator==(const ValidatorInfo& other) const {
    return validator_id == other.validator_id &&
           consensus_key == other.consensus_key &&
           bonded_stake == other.bonded_stake &&
           is_active == other.is_active;
}

bool ValidatorInfo::operator!=(const ValidatorInfo& other) const {
    return !(*this == other);
}

// ============================================================================
// ValidatorSetUpdate Implementation
// ============================================================================

ValidatorSetUpdate::ValidatorSetUpdate()
    : epoch(0),
      validators(),
      total_stake(0),
      validator_set_root(),
      transition_qc() {}

ValidatorSetUpdate::ValidatorSetUpdate(
    uint64_t ep,
    const std::vector<ValidatorInfo>& vals,
    uint64_t stake,
    const crypto::Blake3Hash& root,
    const consensus::QuorumCertificate& qc
)
    : epoch(ep),
      validators(vals),
      total_stake(stake),
      validator_set_root(root),
      transition_qc(qc) {}

std::vector<uint8_t> ValidatorSetUpdate::serialize() const {
    std::vector<uint8_t> result;
    
    // Serialize epoch (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        result.push_back((epoch >> (i * 8)) & 0xFF);
    }
    
    // Serialize validator count (4 bytes, big-endian)
    uint32_t validator_count = static_cast<uint32_t>(validators.size());
    result.push_back((validator_count >> 24) & 0xFF);
    result.push_back((validator_count >> 16) & 0xFF);
    result.push_back((validator_count >> 8) & 0xFF);
    result.push_back(validator_count & 0xFF);
    
    // Serialize validators
    for (const auto& validator : validators) {
        auto validator_bytes = validator.serialize();
        result.insert(result.end(), validator_bytes.begin(), validator_bytes.end());
    }
    
    // Serialize total_stake (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        result.push_back((total_stake >> (i * 8)) & 0xFF);
    }
    
    // Serialize validator_set_root (32 bytes)
    auto root_bytes = validator_set_root.data();
    result.insert(result.end(), root_bytes.begin(), root_bytes.end());
    
    // Serialize transition_qc
    auto qc_bytes = transition_qc.serialize();
    result.insert(result.end(), qc_bytes.begin(), qc_bytes.end());
    
    return result;
}

ValidatorSetUpdate ValidatorSetUpdate::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8 + 4) {
        throw std::invalid_argument("ValidatorSetUpdate::deserialize: data too short");
    }
    
    size_t offset = 0;
    
    // Deserialize epoch
    uint64_t ep = 0;
    for (int i = 0; i < 8; ++i) {
        ep = (ep << 8) | data[offset++];
    }
    
    // Deserialize validator count
    uint32_t validator_count = (static_cast<uint32_t>(data[offset]) << 24) |
                               (static_cast<uint32_t>(data[offset + 1]) << 16) |
                               (static_cast<uint32_t>(data[offset + 2]) << 8) |
                               static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    // Deserialize validators
    std::vector<ValidatorInfo> vals;
    vals.reserve(validator_count);
    for (uint32_t i = 0; i < validator_count; ++i) {
        if (offset + (32 + 48 + 8 + 1) > data.size()) {
            throw std::invalid_argument("ValidatorSetUpdate::deserialize: invalid validator data");
        }
        std::vector<uint8_t> validator_bytes(data.begin() + offset, data.begin() + offset + (32 + 48 + 8 + 1));
        vals.push_back(ValidatorInfo::deserialize(validator_bytes));
        offset += (32 + 48 + 8 + 1);
    }
    
    // Deserialize total_stake
    if (offset + 8 > data.size()) {
        throw std::invalid_argument("ValidatorSetUpdate::deserialize: data too short for total_stake");
    }
    uint64_t stake = 0;
    for (int i = 0; i < 8; ++i) {
        stake = (stake << 8) | data[offset++];
    }
    
    // Deserialize validator_set_root
    if (offset + 32 > data.size()) {
        throw std::invalid_argument("ValidatorSetUpdate::deserialize: data too short for validator_set_root");
    }
    std::vector<uint8_t> root_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash root(root_bytes);
    offset += 32;
    
    // Deserialize transition_qc
    std::vector<uint8_t> qc_bytes(data.begin() + offset, data.end());
    consensus::QuorumCertificate qc = consensus::QuorumCertificate::deserialize(qc_bytes);
    
    return ValidatorSetUpdate(ep, vals, stake, root, qc);
}

bool ValidatorSetUpdate::operator==(const ValidatorSetUpdate& other) const {
    return epoch == other.epoch &&
           validators == other.validators &&
           total_stake == other.total_stake &&
           validator_set_root == other.validator_set_root &&
           transition_qc == other.transition_qc;
}

bool ValidatorSetUpdate::operator!=(const ValidatorSetUpdate& other) const {
    return !(*this == other);
}

// ============================================================================
// MessageSerializer Implementation
// ============================================================================

std::vector<uint8_t> MessageSerializer::serialize_transaction(const state::Transaction& tx) {
    return tx.serialize();
}

state::Transaction MessageSerializer::deserialize_transaction(const std::vector<uint8_t>& data) {
    return state::Transaction::deserialize(data);
}

std::vector<uint8_t> MessageSerializer::serialize_block(const consensus::Block& block) {
    return block.serialize();
}

consensus::Block MessageSerializer::deserialize_block(const std::vector<uint8_t>& data) {
    return consensus::Block::deserialize(data);
}

std::vector<uint8_t> MessageSerializer::serialize_vote(const Vote& vote) {
    return vote.serialize();
}

Vote MessageSerializer::deserialize_vote(const std::vector<uint8_t>& data) {
    return Vote::deserialize(data);
}

std::vector<uint8_t> MessageSerializer::serialize_block_request(const BlockRequest& request) {
    return request.serialize();
}

BlockRequest MessageSerializer::deserialize_block_request(const std::vector<uint8_t>& data) {
    return BlockRequest::deserialize(data);
}

std::vector<uint8_t> MessageSerializer::serialize_block_response(const BlockResponse& response) {
    return response.serialize();
}

BlockResponse MessageSerializer::deserialize_block_response(const std::vector<uint8_t>& data) {
    return BlockResponse::deserialize(data);
}

std::vector<uint8_t> MessageSerializer::serialize_validator_set_update(const ValidatorSetUpdate& update) {
    return update.serialize();
}

ValidatorSetUpdate MessageSerializer::deserialize_validator_set_update(const std::vector<uint8_t>& data) {
    return ValidatorSetUpdate::deserialize(data);
}

// ============================================================================
// MessageHandlerRegistry Implementation
// ============================================================================

MessageHandlerRegistry::MessageHandlerRegistry(NetworkLayer& network)
    : network_(network),
      transaction_handler_(),
      block_handler_(),
      vote_handler_(),
      block_request_handler_(),
      block_response_handler_(),
      validator_set_update_handler_() {
    
    // Register internal message handlers with the network layer
    network_.on_message(MessageType::Transaction, 
        [this](const NetworkMessage& msg, const PeerID& sender) {
            handle_transaction_message(msg, sender);
        });
    
    network_.on_message(MessageType::Block,
        [this](const NetworkMessage& msg, const PeerID& sender) {
            handle_block_message(msg, sender);
        });
    
    network_.on_message(MessageType::Vote,
        [this](const NetworkMessage& msg, const PeerID& sender) {
            handle_vote_message(msg, sender);
        });
    
    network_.on_message(MessageType::BlockRequest,
        [this](const NetworkMessage& msg, const PeerID& sender) {
            handle_block_request_message(msg, sender);
        });
    
    network_.on_message(MessageType::BlockResponse,
        [this](const NetworkMessage& msg, const PeerID& sender) {
            handle_block_response_message(msg, sender);
        });
    
    network_.on_message(MessageType::ValidatorSetUpdate,
        [this](const NetworkMessage& msg, const PeerID& sender) {
            handle_validator_set_update_message(msg, sender);
        });
}

void MessageHandlerRegistry::register_transaction_handler(TransactionHandler handler) {
    transaction_handler_ = handler;
}

void MessageHandlerRegistry::register_block_handler(BlockHandler handler) {
    block_handler_ = handler;
}

void MessageHandlerRegistry::register_vote_handler(VoteHandler handler) {
    vote_handler_ = handler;
}

void MessageHandlerRegistry::register_block_request_handler(BlockRequestHandler handler) {
    block_request_handler_ = handler;
}

void MessageHandlerRegistry::register_block_response_handler(BlockResponseHandler handler) {
    block_response_handler_ = handler;
}

void MessageHandlerRegistry::register_validator_set_update_handler(ValidatorSetUpdateHandler handler) {
    validator_set_update_handler_ = handler;
}

void MessageHandlerRegistry::handle_transaction_message(const NetworkMessage& msg, const PeerID& sender) {
    if (transaction_handler_) {
        try {
            auto tx = MessageSerializer::deserialize_transaction(msg.payload);
            transaction_handler_(tx, sender);
        } catch (const std::exception& e) {
            // Log error and update peer reputation
            network_.update_reputation(sender, -10);
        }
    }
}

void MessageHandlerRegistry::handle_block_message(const NetworkMessage& msg, const PeerID& sender) {
    if (block_handler_) {
        try {
            auto block = MessageSerializer::deserialize_block(msg.payload);
            block_handler_(block, sender);
        } catch (const std::exception& e) {
            // Log error and update peer reputation
            network_.update_reputation(sender, -10);
        }
    }
}

void MessageHandlerRegistry::handle_vote_message(const NetworkMessage& msg, const PeerID& sender) {
    if (vote_handler_) {
        try {
            auto vote = MessageSerializer::deserialize_vote(msg.payload);
            vote_handler_(vote, sender);
        } catch (const std::exception& e) {
            // Log error and update peer reputation
            network_.update_reputation(sender, -10);
        }
    }
}

void MessageHandlerRegistry::handle_block_request_message(const NetworkMessage& msg, const PeerID& sender) {
    if (block_request_handler_) {
        try {
            auto request = MessageSerializer::deserialize_block_request(msg.payload);
            block_request_handler_(request, sender);
        } catch (const std::exception& e) {
            // Log error and update peer reputation
            network_.update_reputation(sender, -10);
        }
    }
}

void MessageHandlerRegistry::handle_block_response_message(const NetworkMessage& msg, const PeerID& sender) {
    if (block_response_handler_) {
        try {
            auto response = MessageSerializer::deserialize_block_response(msg.payload);
            block_response_handler_(response, sender);
        } catch (const std::exception& e) {
            // Log error and update peer reputation
            network_.update_reputation(sender, -10);
        }
    }
}

void MessageHandlerRegistry::handle_validator_set_update_message(const NetworkMessage& msg, const PeerID& sender) {
    if (validator_set_update_handler_) {
        try {
            auto update = MessageSerializer::deserialize_validator_set_update(msg.payload);
            validator_set_update_handler_(update, sender);
        } catch (const std::exception& e) {
            // Log error and update peer reputation
            network_.update_reputation(sender, -10);
        }
    }
}

} // namespace network
} // namespace sarafu
