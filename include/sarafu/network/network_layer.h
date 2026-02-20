#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "sarafu/crypto/blake3_hash.h"

namespace sarafu {
namespace network {

/**
 * PeerID represents a unique identifier for a peer in the network.
 * 
 * This is typically derived from the peer's public key in libp2p.
 */
using PeerID = std::string;

/**
 * PeerInfo contains information about a connected peer.
 * 
 * Includes:
 * - id: Unique peer identifier
 * - addresses: List of multiaddresses where the peer can be reached
 * - is_validator: Whether this peer is a validator in the active set
 * - last_seen: Unix timestamp of last communication
 * - reputation_score: Score tracking peer behavior (0-100)
 */
struct PeerInfo {
    PeerID id;
    std::vector<std::string> addresses;
    bool is_validator;
    uint64_t last_seen;
    uint64_t reputation_score;

    // Constructors
    PeerInfo();
    PeerInfo(
        const PeerID& peer_id,
        const std::vector<std::string>& addrs,
        bool validator,
        uint64_t seen,
        uint64_t reputation
    );

    // Comparison
    bool operator==(const PeerInfo& other) const;
    bool operator!=(const PeerInfo& other) const;
};

/**
 * MessageType identifies the type of network message.
 * 
 * Types:
 * - Transaction: A signed transaction to be included in a block
 * - Block: A proposed block with transactions
 * - Vote: A validator's vote on a block
 * - BlockRequest: Request for a specific block by height or hash
 * - BlockResponse: Response containing requested block data
 * - ValidatorSetUpdate: Notification of validator set changes at epoch boundary
 */
enum class MessageType : uint8_t {
    Transaction = 0,
    Block = 1,
    Vote = 2,
    BlockRequest = 3,
    BlockResponse = 4,
    ValidatorSetUpdate = 5
};

/**
 * NetworkMessage represents a message transmitted over the network.
 * 
 * Contains:
 * - type: The message type
 * - payload: Serialized message data
 * - hash: Blake3 hash of the payload for deduplication
 */
struct NetworkMessage {
    MessageType type;
    std::vector<uint8_t> payload;
    crypto::Blake3Hash hash;

    // Constructors
    NetworkMessage();
    NetworkMessage(
        MessageType msg_type,
        const std::vector<uint8_t>& data
    );

    /**
     * Compute the message hash from the payload.
     * 
     * @return Blake3 hash of the payload
     */
    crypto::Blake3Hash compute_hash() const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    static NetworkMessage deserialize(const std::vector<uint8_t>& data);

    // Comparison
    bool operator==(const NetworkMessage& other) const;
    bool operator!=(const NetworkMessage& other) const;
};

/**
 * NetworkConfig contains configuration parameters for the network layer.
 * 
 * Parameters:
 * - listen_address: Multiaddress to listen on (e.g., "/ip4/0.0.0.0/tcp/9000")
 * - bootstrap_peers: List of bootstrap node addresses for initial peer discovery
 * - min_peers: Minimum number of peers to maintain (default: 8)
 * - max_peers: Maximum number of peers to maintain (default: 50)
 * - gossip_fanout: Number of peers to forward messages to (default: 8)
 * - enable_quic: Whether to use QUIC transport (default: true)
 * - enable_tcp_fallback: Whether to fall back to TCP if QUIC fails (default: true)
 */
struct NetworkConfig {
    std::string listen_address;
    std::vector<std::string> bootstrap_peers;
    size_t min_peers;
    size_t max_peers;
    size_t gossip_fanout;
    bool enable_quic;
    bool enable_tcp_fallback;

    // Default constructor with sensible defaults
    NetworkConfig();
    NetworkConfig(
        const std::string& listen_addr,
        const std::vector<std::string>& bootstrap,
        size_t min_p = 8,
        size_t max_p = 50,
        size_t fanout = 8,
        bool quic = true,
        bool tcp_fallback = true
    );
};

/**
 * MessageHandler is a callback function for processing received messages.
 * 
 * Parameters:
 * - message: The received network message
 * - sender: The peer ID of the message sender
 */
using MessageHandler = std::function<void(const NetworkMessage&, const PeerID&)>;

/**
 * NetworkLayer implements the libp2p-based peer-to-peer networking layer.
 * 
 * Responsibilities:
 * - Peer discovery using Kademlia DHT
 * - Connection management (maintain 8-50 peers)
 * - Message broadcasting using structured epidemic broadcast
 * - Transport layer (QUIC primary, TCP fallback)
 * - Peer reputation tracking and banning
 * 
 * The NetworkLayer provides efficient block propagation targeting 95% of
 * validators within 300ms using two-phase relay with fanout factor of 8.
 * 
 * Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.8, 6.9, 29.1-29.8
 */
class NetworkLayer {
public:
    /**
     * Construct a NetworkLayer with the given configuration.
     * 
     * @param config Network configuration parameters
     */
    explicit NetworkLayer(const NetworkConfig& config);

    /**
     * Destructor - cleans up libp2p resources.
     */
    ~NetworkLayer();

    // Disable copy and move (libp2p resources are not copyable)
    NetworkLayer(const NetworkLayer&) = delete;
    NetworkLayer& operator=(const NetworkLayer&) = delete;
    NetworkLayer(NetworkLayer&&) = delete;
    NetworkLayer& operator=(NetworkLayer&&) = delete;

    /**
     * Initialize the network layer and start listening.
     * 
     * This method:
     * - Initializes libp2p host
     * - Configures QUIC transport with TCP fallback
     * - Sets up Kademlia DHT for peer discovery
     * - Starts listening on the configured address
     * 
     * @return true if initialization succeeded, false otherwise
     */
    bool initialize();

    /**
     * Shutdown the network layer and disconnect from all peers.
     */
    void shutdown();

    /**
     * Connect to bootstrap peers for initial peer discovery.
     * 
     * This method connects to the configured bootstrap nodes and
     * uses Kademlia DHT to discover additional peers.
     * 
     * @param bootstrap_peers List of bootstrap peer multiaddresses
     * @return Number of successful connections
     */
    size_t connect_to_peers(const std::vector<std::string>& bootstrap_peers);

    /**
     * Maintain peer connections to keep count within min/max bounds.
     * 
     * This method:
     * - Disconnects from excess peers if count > max_peers
     * - Discovers and connects to new peers if count < min_peers
     * - Prioritizes connections to validators in the active set
     * - Implements exponential backoff for failed reconnections
     * 
     * Should be called periodically (e.g., every 30 seconds).
     */
    void maintain_connections();

    /**
     * Ban a peer for protocol violations.
     * 
     * Banned peers are disconnected and prevented from reconnecting
     * for the specified duration.
     * 
     * @param peer The peer ID to ban
     * @param duration_seconds Ban duration in seconds (default: 24 hours)
     */
    void ban_peer(const PeerID& peer, uint64_t duration_seconds = 86400);

    /**
     * Update the reputation score for a peer.
     * 
     * Reputation scores range from 0-100:
     * - 100: Perfect behavior
     * - 50-99: Good behavior
     * - 25-49: Questionable behavior
     * - 0-24: Poor behavior (may lead to ban)
     * 
     * @param peer The peer ID
     * @param delta Change in reputation (positive or negative)
     */
    void update_reputation(const PeerID& peer, int delta);

    /**
     * Broadcast a message to all connected peers using gossip protocol.
     * 
     * This implements structured epidemic broadcast:
     * - Select gossip_fanout random peers
     * - Send message to selected peers
     * - Each peer repeats the process (two-phase relay)
     * - Track seen messages to prevent duplicates
     * 
     * For blocks, prioritizes sending to validators first.
     * 
     * @param message The message to broadcast
     */
    void broadcast(const NetworkMessage& message);

    /**
     * Send a message to a specific peer.
     * 
     * @param peer The peer ID to send to
     * @param message The message to send
     * @return true if send succeeded, false otherwise
     */
    bool send_to_peer(const PeerID& peer, const NetworkMessage& message);

    /**
     * Register a message handler for a specific message type.
     * 
     * When a message of the specified type is received, the handler
     * will be called with the message and sender peer ID.
     * 
     * @param type The message type to handle
     * @param handler The callback function to invoke
     */
    void on_message(MessageType type, MessageHandler handler);

    /**
     * Get the list of currently connected peers.
     * 
     * @return Vector of peer information
     */
    std::vector<PeerInfo> get_peers() const;

    /**
     * Get the number of connected peers.
     * 
     * @return Peer count
     */
    size_t peer_count() const;

    /**
     * Check if a peer is currently connected.
     * 
     * @param peer The peer ID to check
     * @return true if connected, false otherwise
     */
    bool is_connected(const PeerID& peer) const;

    /**
     * Get the local peer ID.
     * 
     * @return This node's peer ID
     */
    PeerID local_peer_id() const;

    /**
     * Mark a peer as a validator in the active set.
     * 
     * This prioritizes maintaining connections to validators.
     * 
     * @param peer The peer ID
     * @param is_validator Whether the peer is a validator
     */
    void set_validator_status(const PeerID& peer, bool is_validator);

private:
    /**
     * Gossip a message to a subset of peers.
     * 
     * @param message The message to gossip
     * @param fanout Number of peers to send to
     */
    void gossip_message(const NetworkMessage& message, size_t fanout);

    /**
     * Discover new peers using Kademlia DHT.
     * 
     * @return Number of new peers discovered
     */
    size_t discover_peers();

    /**
     * Handle an incoming message from a peer.
     * 
     * @param message The received message
     * @param sender The sender's peer ID
     */
    void handle_message(const NetworkMessage& message, const PeerID& sender);

    /**
     * Check if a message has been seen before (for deduplication).
     * 
     * @param message_hash The message hash
     * @return true if seen, false otherwise
     */
    bool is_message_seen(const crypto::Blake3Hash& message_hash) const;

    /**
     * Mark a message as seen.
     * 
     * @param message_hash The message hash
     */
    void mark_message_seen(const crypto::Blake3Hash& message_hash);

    /**
     * Select random peers for gossip, prioritizing validators.
     * 
     * @param count Number of peers to select
     * @param prioritize_validators Whether to prioritize validators
     * @return Vector of selected peer IDs
     */
    std::vector<PeerID> select_gossip_peers(size_t count, bool prioritize_validators) const;

    /**
     * Check if a peer is currently banned.
     * 
     * @param peer The peer ID
     * @return true if banned, false otherwise
     */
    bool is_peer_banned(const PeerID& peer) const;

    /**
     * Remove expired bans.
     */
    void cleanup_expired_bans();

    // Configuration
    NetworkConfig config_;

    // Libp2p host (opaque pointer to avoid exposing libp2p headers)
    struct LibP2PHost;
    std::unique_ptr<LibP2PHost> host_;

    // Peer management
    std::map<PeerID, PeerInfo> peers_;
    std::map<PeerID, uint64_t> banned_peers_;  // peer_id -> ban_expiry_timestamp

    // Message handlers
    std::map<MessageType, MessageHandler> message_handlers_;

    // Seen messages (for deduplication)
    std::map<crypto::Blake3Hash, uint64_t> seen_messages_;  // hash -> timestamp

    // State
    bool initialized_;
    PeerID local_peer_id_;

    // Constants
    static constexpr size_t DEFAULT_MIN_PEERS = 8;
    static constexpr size_t DEFAULT_MAX_PEERS = 50;
    static constexpr size_t DEFAULT_GOSSIP_FANOUT = 8;
    static constexpr uint64_t DEFAULT_BAN_DURATION = 86400;  // 24 hours
    static constexpr uint64_t SEEN_MESSAGE_TTL = 3600;  // 1 hour
};

} // namespace network
} // namespace sarafu
