#pragma once

#include <functional>
#include <memory>
#include "sarafu/network/network_layer.h"
#include "sarafu/consensus/block.h"
#include "sarafu/state/transaction.h"

namespace sarafu {
namespace network {

// Forward declarations
class NetworkLayer;

/**
 * Vote represents a validator's vote on a block in the consensus protocol.
 * 
 * Contains:
 * - block_height: Height of the block being voted on
 * - block_hash: Hash of the block being voted on
 * - view_number: Current view number in HotStuff
 * - validator_id: ID of the validator casting the vote
 * - signature: BLS12-381 signature over the vote
 */
struct Vote {
    uint64_t block_height;
    crypto::Blake3Hash block_hash;
    uint64_t view_number;
    consensus::ValidatorID validator_id;
    crypto::BLS12_381_Signature signature;

    // Constructors
    Vote();
    Vote(
        uint64_t height,
        const crypto::Blake3Hash& hash,
        uint64_t view,
        const consensus::ValidatorID& validator,
        const crypto::BLS12_381_Signature& sig
    );

    // Serialization
    std::vector<uint8_t> serialize() const;
    static Vote deserialize(const std::vector<uint8_t>& data);

    // Comparison
    bool operator==(const Vote& other) const;
    bool operator!=(const Vote& other) const;
};

/**
 * BlockRequest represents a request for a specific block.
 * 
 * Can request by either height or hash.
 */
struct BlockRequest {
    enum class RequestType {
        ByHeight,
        ByHash
    };

    RequestType type;
    uint64_t height;  // Used when type == ByHeight
    crypto::Blake3Hash hash;  // Used when type == ByHash
    consensus::ValidatorID requester_id;

    // Constructors
    BlockRequest();
    static BlockRequest by_height(uint64_t h, const consensus::ValidatorID& requester);
    static BlockRequest by_hash(const crypto::Blake3Hash& hash, const consensus::ValidatorID& requester);

    // Serialization
    std::vector<uint8_t> serialize() const;
    static BlockRequest deserialize(const std::vector<uint8_t>& data);

    // Comparison
    bool operator==(const BlockRequest& other) const;
    bool operator!=(const BlockRequest& other) const;
};

/**
 * BlockResponse represents a response to a block request.
 */
struct BlockResponse {
    consensus::Block block;
    bool found;
    std::string error;

    // Constructors
    BlockResponse();
    BlockResponse(const consensus::Block& blk, bool fnd, const std::string& err = "");

    // Serialization
    std::vector<uint8_t> serialize() const;
    static BlockResponse deserialize(const std::vector<uint8_t>& data);

    // Comparison
    bool operator==(const BlockResponse& other) const;
    bool operator!=(const BlockResponse& other) const;
};

/**
 * ValidatorInfo contains information about a validator in the set.
 */
struct ValidatorInfo {
    consensus::ValidatorID validator_id;
    crypto::BLS12_381_PublicKey consensus_key;
    uint64_t bonded_stake;
    bool is_active;

    // Constructors
    ValidatorInfo();
    ValidatorInfo(
        const consensus::ValidatorID& id,
        const crypto::BLS12_381_PublicKey& key,
        uint64_t stake,
        bool active
    );

    // Serialization
    std::vector<uint8_t> serialize() const;
    static ValidatorInfo deserialize(const std::vector<uint8_t>& data);

    // Comparison
    bool operator==(const ValidatorInfo& other) const;
    bool operator!=(const ValidatorInfo& other) const;
};

/**
 * ValidatorSetUpdate represents a validator set change at an epoch boundary.
 */
struct ValidatorSetUpdate {
    uint64_t epoch;
    std::vector<ValidatorInfo> validators;
    uint64_t total_stake;
    crypto::Blake3Hash validator_set_root;
    consensus::QuorumCertificate transition_qc;

    // Constructors
    ValidatorSetUpdate();
    ValidatorSetUpdate(
        uint64_t ep,
        const std::vector<ValidatorInfo>& vals,
        uint64_t stake,
        const crypto::Blake3Hash& root,
        const consensus::QuorumCertificate& qc
    );

    // Serialization
    std::vector<uint8_t> serialize() const;
    static ValidatorSetUpdate deserialize(const std::vector<uint8_t>& data);

    // Comparison
    bool operator==(const ValidatorSetUpdate& other) const;
    bool operator!=(const ValidatorSetUpdate& other) const;
};

/**
 * MessageSerializer provides serialization/deserialization for network messages.
 * 
 * This class handles conversion between C++ types and Protocol Buffer messages
 * for network transmission.
 */
class MessageSerializer {
public:
    // Transaction serialization
    static std::vector<uint8_t> serialize_transaction(const state::Transaction& tx);
    static state::Transaction deserialize_transaction(const std::vector<uint8_t>& data);

    // Block serialization
    static std::vector<uint8_t> serialize_block(const consensus::Block& block);
    static consensus::Block deserialize_block(const std::vector<uint8_t>& data);

    // Vote serialization
    static std::vector<uint8_t> serialize_vote(const Vote& vote);
    static Vote deserialize_vote(const std::vector<uint8_t>& data);

    // BlockRequest serialization
    static std::vector<uint8_t> serialize_block_request(const BlockRequest& request);
    static BlockRequest deserialize_block_request(const std::vector<uint8_t>& data);

    // BlockResponse serialization
    static std::vector<uint8_t> serialize_block_response(const BlockResponse& response);
    static BlockResponse deserialize_block_response(const std::vector<uint8_t>& data);

    // ValidatorSetUpdate serialization
    static std::vector<uint8_t> serialize_validator_set_update(const ValidatorSetUpdate& update);
    static ValidatorSetUpdate deserialize_validator_set_update(const std::vector<uint8_t>& data);
};

/**
 * MessageHandlerRegistry manages message handlers for different message types.
 * 
 * This class provides a convenient way to register and dispatch message handlers
 * for the network layer.
 */
class MessageHandlerRegistry {
public:
    // Handler function types
    using TransactionHandler = std::function<void(const state::Transaction&, const PeerID&)>;
    using BlockHandler = std::function<void(const consensus::Block&, const PeerID&)>;
    using VoteHandler = std::function<void(const Vote&, const PeerID&)>;
    using BlockRequestHandler = std::function<void(const BlockRequest&, const PeerID&)>;
    using BlockResponseHandler = std::function<void(const BlockResponse&, const PeerID&)>;
    using ValidatorSetUpdateHandler = std::function<void(const ValidatorSetUpdate&, const PeerID&)>;

    /**
     * Construct a MessageHandlerRegistry and register handlers with the network layer.
     * 
     * @param network The network layer to register handlers with
     */
    explicit MessageHandlerRegistry(NetworkLayer& network);

    /**
     * Register a handler for transaction messages.
     * 
     * @param handler The callback function to invoke
     */
    void register_transaction_handler(TransactionHandler handler);

    /**
     * Register a handler for block messages.
     * 
     * @param handler The callback function to invoke
     */
    void register_block_handler(BlockHandler handler);

    /**
     * Register a handler for vote messages.
     * 
     * @param handler The callback function to invoke
     */
    void register_vote_handler(VoteHandler handler);

    /**
     * Register a handler for block request messages.
     * 
     * @param handler The callback function to invoke
     */
    void register_block_request_handler(BlockRequestHandler handler);

    /**
     * Register a handler for block response messages.
     * 
     * @param handler The callback function to invoke
     */
    void register_block_response_handler(BlockResponseHandler handler);

    /**
     * Register a handler for validator set update messages.
     * 
     * @param handler The callback function to invoke
     */
    void register_validator_set_update_handler(ValidatorSetUpdateHandler handler);

private:
    NetworkLayer& network_;
    TransactionHandler transaction_handler_;
    BlockHandler block_handler_;
    VoteHandler vote_handler_;
    BlockRequestHandler block_request_handler_;
    BlockResponseHandler block_response_handler_;
    ValidatorSetUpdateHandler validator_set_update_handler_;

    // Internal message dispatch methods
    void handle_transaction_message(const NetworkMessage& msg, const PeerID& sender);
    void handle_block_message(const NetworkMessage& msg, const PeerID& sender);
    void handle_vote_message(const NetworkMessage& msg, const PeerID& sender);
    void handle_block_request_message(const NetworkMessage& msg, const PeerID& sender);
    void handle_block_response_message(const NetworkMessage& msg, const PeerID& sender);
    void handle_validator_set_update_message(const NetworkMessage& msg, const PeerID& sender);
};

} // namespace network
} // namespace sarafu
