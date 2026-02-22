#include "sarafu/network/network_layer.h"
#include "sarafu/crypto/blake3_hash.h"
#include "version.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <random>
#include <stdexcept>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>

namespace sarafu {
namespace network {

using boost::asio::ip::tcp;

// ============================================================================
// PeerInfo Implementation
// ============================================================================

PeerInfo::PeerInfo()
    : id(""),
      addresses(),
      is_validator(false),
      last_seen(0),
      reputation_score(100) {}

PeerInfo::PeerInfo(
    const PeerID& peer_id,
    const std::vector<std::string>& addrs,
    bool validator,
    uint64_t seen,
    uint64_t reputation
)
    : id(peer_id),
      addresses(addrs),
      is_validator(validator),
      last_seen(seen),
      reputation_score(reputation) {}

bool PeerInfo::operator==(const PeerInfo& other) const {
    return id == other.id &&
           addresses == other.addresses &&
           is_validator == other.is_validator &&
           last_seen == other.last_seen &&
           reputation_score == other.reputation_score;
}

bool PeerInfo::operator!=(const PeerInfo& other) const {
    return !(*this == other);
}

// ============================================================================
// NetworkMessage Implementation
// ============================================================================

NetworkMessage::NetworkMessage()
    : type(MessageType::Transaction),
      payload(),
      hash() {}

NetworkMessage::NetworkMessage(
    MessageType msg_type,
    const std::vector<uint8_t>& data
)
    : type(msg_type),
      payload(data),
      hash(compute_hash()) {}

crypto::Blake3Hash NetworkMessage::compute_hash() const {
    return crypto::Blake3Hash::hash(payload.data(), payload.size());
}

std::vector<uint8_t> NetworkMessage::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(1 + 4 + payload.size() + 32);
    
    // Serialize type (1 byte)
    result.push_back(static_cast<uint8_t>(type));
    
    // Serialize payload length (4 bytes, big-endian)
    uint32_t payload_len = static_cast<uint32_t>(payload.size());
    result.push_back((payload_len >> 24) & 0xFF);
    result.push_back((payload_len >> 16) & 0xFF);
    result.push_back((payload_len >> 8) & 0xFF);
    result.push_back(payload_len & 0xFF);
    
    // Serialize payload
    result.insert(result.end(), payload.begin(), payload.end());
    
    // Serialize hash
    auto hash_bytes = hash.data();
    result.insert(result.end(), hash_bytes.begin(), hash_bytes.end());
    
    return result;
}

NetworkMessage NetworkMessage::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 1 + 4 + 32) {
        throw std::invalid_argument("NetworkMessage::deserialize: data too short");
    }
    
    size_t offset = 0;
    
    // Deserialize type
    MessageType msg_type = static_cast<MessageType>(data[offset++]);
    
    // Deserialize payload length
    uint32_t payload_len = (static_cast<uint32_t>(data[offset]) << 24) |
                           (static_cast<uint32_t>(data[offset + 1]) << 16) |
                           (static_cast<uint32_t>(data[offset + 2]) << 8) |
                           static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    
    if (data.size() < offset + payload_len + 32) {
        throw std::invalid_argument("NetworkMessage::deserialize: invalid payload length");
    }
    
    // Deserialize payload
    std::vector<uint8_t> payload(data.begin() + offset, data.begin() + offset + payload_len);
    offset += payload_len;
    
    // Deserialize hash
    std::vector<uint8_t> hash_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash msg_hash(hash_bytes);
    
    NetworkMessage message(msg_type, payload);
    
    // Verify hash
    if (message.hash != msg_hash) {
        throw std::invalid_argument("NetworkMessage::deserialize: hash mismatch");
    }
    
    return message;
}

bool NetworkMessage::operator==(const NetworkMessage& other) const {
    return type == other.type &&
           payload == other.payload &&
           hash == other.hash;
}

bool NetworkMessage::operator!=(const NetworkMessage& other) const {
    return !(*this == other);
}

// ============================================================================
// NetworkConfig Implementation
// ============================================================================

NetworkConfig::NetworkConfig()
    : listen_address("/ip4/0.0.0.0/tcp/9000"),
      bootstrap_peers(),
      min_peers(8),
      max_peers(50),
      gossip_fanout(8),
      enable_quic(true),
      enable_tcp_fallback(true) {}

NetworkConfig::NetworkConfig(
    const std::string& listen_addr,
    const std::vector<std::string>& bootstrap,
    size_t min_p,
    size_t max_p,
    size_t fanout,
    bool quic,
    bool tcp_fallback
)
    : listen_address(listen_addr),
      bootstrap_peers(bootstrap),
      min_peers(min_p),
      max_peers(max_p),
      gossip_fanout(fanout),
      enable_quic(quic),
      enable_tcp_fallback(tcp_fallback) {}

// ============================================================================
// NetworkLayer Implementation
// ============================================================================

// P2P Host implementation using Boost.Asio
struct NetworkLayer::LibP2PHost {
    boost::asio::io_context io_context;
    std::unique_ptr<tcp::acceptor> acceptor;
    std::thread io_thread;
    bool running;
    
    // Connection management
    struct Connection {
        std::shared_ptr<tcp::socket> socket;
        PeerID peer_id;
        std::vector<uint8_t> read_buffer;
        std::vector<uint8_t> write_buffer;
        bool connected;
        
        Connection() : socket(nullptr), peer_id(""), read_buffer(8192), connected(false) {}
    };
    
    std::map<PeerID, std::shared_ptr<Connection>> connections;
    mutable std::mutex connections_mutex;  // mutable because used in const methods
    
    LibP2PHost() : running(false) {}
    
    ~LibP2PHost() {
        if (running) {
            stop();
        }
    }
    
    void start(const std::string& listen_addr, uint16_t port) {
        if (running) return;
        
        try {
            // Create acceptor
            tcp::endpoint endpoint(boost::asio::ip::make_address(listen_addr), port);
            acceptor = std::make_unique<tcp::acceptor>(io_context, endpoint);
            
            // Start accepting connections
            start_accept();
            
            // Run io_context in separate thread
            running = true;
            io_thread = std::thread([this]() {
                io_context.run();
            });
            
        } catch (const std::exception& e) {
            throw std::runtime_error(std::string("Failed to start P2P host: ") + e.what());
        }
    }
    
    void stop() {
        if (!running) return;
        
        running = false;
        
        // Close all connections
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            for (auto& [peer_id, conn] : connections) {
                if (conn->socket && conn->socket->is_open()) {
                    boost::system::error_code ec;
                    conn->socket->close(ec);
                }
            }
            connections.clear();
        }
        
        // Stop acceptor
        if (acceptor && acceptor->is_open()) {
            boost::system::error_code ec;
            acceptor->close(ec);
        }
        
        // Stop io_context
        io_context.stop();
        
        // Wait for io_thread
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }
    
    void start_accept() {
        auto socket = std::make_shared<tcp::socket>(io_context);
        
        acceptor->async_accept(*socket, [this, socket](const boost::system::error_code& error) {
            if (!error) {
                handle_accept(socket);
            }
            
            if (running) {
                start_accept();
            }
        });
    }
    
    void handle_accept(std::shared_ptr<tcp::socket> socket) {
        // Generate peer ID from remote endpoint
        std::string remote_addr = socket->remote_endpoint().address().to_string();
        uint16_t remote_port = socket->remote_endpoint().port();
        PeerID peer_id = "peer-" + remote_addr + ":" + std::to_string(remote_port);
        
        // Create connection
        auto conn = std::make_shared<Connection>();
        conn->socket = socket;
        conn->peer_id = peer_id;
        conn->connected = true;
        
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            connections[peer_id] = conn;
        }
        
        // Start reading from this connection
        start_read(conn);
    }
    
    void start_read(std::shared_ptr<Connection> conn) {
        if (!conn->socket || !conn->socket->is_open()) return;
        
        conn->socket->async_read_some(
            boost::asio::buffer(conn->read_buffer),
            [this, conn](const boost::system::error_code& error, std::size_t bytes_transferred) {
                if (!error && bytes_transferred > 0) {
                    // Process received data
                    handle_read(conn, bytes_transferred);
                    
                    // Continue reading
                    if (running && conn->connected) {
                        start_read(conn);
                    }
                } else {
                    // Connection closed or error
                    handle_disconnect(conn);
                }
            }
        );
    }
    
    void handle_read(std::shared_ptr<Connection> conn, std::size_t bytes_transferred) {
        // This would be called by NetworkLayer to process received messages
        // For now, we just acknowledge receipt
        (void)conn;
        (void)bytes_transferred;
    }
    
    void handle_disconnect(std::shared_ptr<Connection> conn) {
        conn->connected = false;
        
        std::lock_guard<std::mutex> lock(connections_mutex);
        connections.erase(conn->peer_id);
    }
    
    bool connect_to_peer(const std::string& address, uint16_t port, PeerID& out_peer_id) {
        try {
            auto socket = std::make_shared<tcp::socket>(io_context);
            tcp::endpoint endpoint(boost::asio::ip::make_address(address), port);
            
            boost::system::error_code ec;
            socket->connect(endpoint, ec);
            
            if (ec) {
                return false;
            }
            
            // Generate peer ID
            out_peer_id = "peer-" + address + ":" + std::to_string(port);
            
            // Create connection
            auto conn = std::make_shared<Connection>();
            conn->socket = socket;
            conn->peer_id = out_peer_id;
            conn->connected = true;
            
            {
                std::lock_guard<std::mutex> lock(connections_mutex);
                connections[out_peer_id] = conn;
            }
            
            // Start reading
            start_read(conn);
            
            return true;
            
        } catch (const std::exception&) {
            return false;
        }
    }
    
    bool send_to_peer(const PeerID& peer_id, const std::vector<uint8_t>& data) {
        std::shared_ptr<Connection> conn;
        
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            auto it = connections.find(peer_id);
            if (it == connections.end() || !it->second->connected) {
                return false;
            }
            conn = it->second;
        }
        
        if (!conn->socket || !conn->socket->is_open()) {
            return false;
        }
        
        try {
            boost::system::error_code ec;
            boost::asio::write(*conn->socket, boost::asio::buffer(data), ec);
            return !ec;
        } catch (const std::exception&) {
            return false;
        }
    }
    
    std::vector<PeerID> get_connected_peers() const {
        std::vector<PeerID> result;
        std::lock_guard<std::mutex> lock(connections_mutex);
        
        for (const auto& [peer_id, conn] : connections) {
            if (conn->connected) {
                result.push_back(peer_id);
            }
        }
        
        return result;
    }
};

NetworkLayer::NetworkLayer(const NetworkConfig& config)
    : config_(config),
      host_(nullptr),
      peers_(),
      banned_peers_(),
      message_handlers_(),
      seen_messages_(),
      initialized_(false),
      local_peer_id_("") {}

NetworkLayer::~NetworkLayer() {
    if (initialized_) {
        shutdown();
    }
}

bool NetworkLayer::initialize() {
    if (initialized_) {
        return true;
    }
    
    try {
        // Create P2P host
        host_ = std::make_unique<LibP2PHost>();
        
        // Parse listen address to extract IP and port
        // Format: /ip4/0.0.0.0/tcp/9000
        std::string listen_addr = "0.0.0.0";
        uint16_t listen_port = 9000;
        
        // Simple parsing of multiaddr format
        size_t ip_pos = config_.listen_address.find("/ip4/");
        size_t tcp_pos = config_.listen_address.find("/tcp/");
        
        if (ip_pos != std::string::npos && tcp_pos != std::string::npos) {
            size_t ip_start = ip_pos + 5;
            size_t ip_end = config_.listen_address.find("/", ip_start);
            listen_addr = config_.listen_address.substr(ip_start, ip_end - ip_start);
            
            size_t port_start = tcp_pos + 5;
            std::string port_str = config_.listen_address.substr(port_start);
            listen_port = static_cast<uint16_t>(std::stoi(port_str));
        }
        
        // Start listening
        host_->start(listen_addr, listen_port);
        
        // Generate local peer ID
        local_peer_id_ = "local-peer-" + listen_addr + ":" + std::to_string(listen_port);
        
        initialized_ = true;
        
        std::cout << "P2P network initialized on " << listen_addr << ":" << listen_port << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize network layer: " << e.what() << std::endl;
        return false;
    }
}

void NetworkLayer::shutdown() {
    if (!initialized_) {
        return;
    }
    
    std::cout << "Shutting down P2P network..." << std::endl;
    
    // Disconnect from all peers
    peers_.clear();
    
    // Stop P2P host
    if (host_) {
        host_->stop();
        host_.reset();
    }
    
    initialized_ = false;
}

size_t NetworkLayer::connect_to_peers(const std::vector<std::string>& bootstrap_peers) {
    if (!initialized_) {
        throw std::runtime_error("NetworkLayer::connect_to_peers: not initialized");
    }
    
    size_t connected = 0;
    
    for (const auto& peer_addr : bootstrap_peers) {
        // Parse multiaddr format: /ip4/192.168.1.1/tcp/9000
        std::string ip_addr;
        uint16_t port = 9000;
        
        size_t ip_pos = peer_addr.find("/ip4/");
        size_t tcp_pos = peer_addr.find("/tcp/");
        
        if (ip_pos != std::string::npos && tcp_pos != std::string::npos) {
            size_t ip_start = ip_pos + 5;
            size_t ip_end = peer_addr.find("/", ip_start);
            ip_addr = peer_addr.substr(ip_start, ip_end - ip_start);
            
            size_t port_start = tcp_pos + 5;
            std::string port_str = peer_addr.substr(port_start);
            port = static_cast<uint16_t>(std::stoi(port_str));
        } else {
            // Try simple format: ip:port
            size_t colon_pos = peer_addr.find(':');
            if (colon_pos != std::string::npos) {
                ip_addr = peer_addr.substr(0, colon_pos);
                port = static_cast<uint16_t>(std::stoi(peer_addr.substr(colon_pos + 1)));
            } else {
                continue;  // Invalid format
            }
        }
        
        // Connect to peer
        PeerID peer_id;
        if (host_->connect_to_peer(ip_addr, port, peer_id)) {
            // Check if already in peers map
            if (peers_.find(peer_id) != peers_.end()) {
                continue;
            }
            
            // Check if banned
            if (is_peer_banned(peer_id)) {
                continue;
            }
            
            // Perform version handshake
            // In a real implementation, this would exchange version messages
            // For now, we assume compatibility and log the version check
            Version local_version = get_node_version();
            std::cout << "Connected to peer " << peer_id 
                      << " (local version: " << local_version.to_string() << ")" << std::endl;
            
            // Create peer info
            uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            PeerInfo info(peer_id, {peer_addr}, false, now, 100);
            
            // Add to peers
            peers_[peer_id] = info;
            connected++;
            
            std::cout << "Connected to peer: " << peer_id << std::endl;
        }
    }
    
    return connected;
}

void NetworkLayer::maintain_connections() {
    if (!initialized_) {
        return;
    }
    
    cleanup_expired_bans();
    
    // Disconnect from excess peers if count > max_peers
    while (peers_.size() > config_.max_peers) {
        // Find lowest reputation non-validator peer
        PeerID to_disconnect;
        uint64_t lowest_reputation = 101;
        
        for (const auto& [peer_id, info] : peers_) {
            if (!info.is_validator && info.reputation_score < lowest_reputation) {
                lowest_reputation = info.reputation_score;
                to_disconnect = peer_id;
            }
        }
        
        if (!to_disconnect.empty()) {
            peers_.erase(to_disconnect);
        } else {
            // All peers are validators, disconnect from oldest
            uint64_t oldest_seen = UINT64_MAX;
            for (const auto& [peer_id, info] : peers_) {
                if (info.last_seen < oldest_seen) {
                    oldest_seen = info.last_seen;
                    to_disconnect = peer_id;
                }
            }
            if (!to_disconnect.empty()) {
                peers_.erase(to_disconnect);
            } else {
                break;
            }
        }
    }
    
    // Discover and connect to new peers if count < min_peers
    while (peers_.size() < config_.min_peers) {
        size_t discovered = discover_peers();
        if (discovered == 0) {
            break;  // No more peers available
        }
    }
}

void NetworkLayer::ban_peer(const PeerID& peer, uint64_t duration_seconds) {
    // Disconnect if connected
    peers_.erase(peer);
    
    // Add to banned list
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    banned_peers_[peer] = now + duration_seconds;
}

void NetworkLayer::update_reputation(const PeerID& peer, int delta) {
    auto it = peers_.find(peer);
    if (it == peers_.end()) {
        return;
    }
    
    // Update reputation with bounds checking
    int64_t new_reputation = static_cast<int64_t>(it->second.reputation_score) + delta;
    new_reputation = std::max<int64_t>(0, std::min<int64_t>(100, new_reputation));
    it->second.reputation_score = static_cast<uint64_t>(new_reputation);
    
    // Ban peer if reputation drops too low
    if (it->second.reputation_score < 25) {
        ban_peer(peer, DEFAULT_BAN_DURATION);
    }
}

void NetworkLayer::broadcast(const NetworkMessage& message) {
    if (!initialized_) {
        throw std::runtime_error("NetworkLayer::broadcast: not initialized");
    }
    
    // Mark message as seen
    mark_message_seen(message.hash);
    
    // Gossip to fanout peers
    gossip_message(message, config_.gossip_fanout);
}

bool NetworkLayer::send_to_peer(const PeerID& peer, const NetworkMessage& message) {
    if (!initialized_) {
        throw std::runtime_error("NetworkLayer::send_to_peer: not initialized");
    }
    
    // Check if peer is connected
    if (peers_.find(peer) == peers_.end()) {
        return false;
    }
    
    // Serialize message
    std::vector<uint8_t> data = message.serialize();
    
    // Send via host
    return host_->send_to_peer(peer, data);
}

void NetworkLayer::on_message(MessageType type, MessageHandler handler) {
    message_handlers_[type] = handler;
}

std::vector<PeerInfo> NetworkLayer::get_peers() const {
    std::vector<PeerInfo> result;
    result.reserve(peers_.size());
    
    for (const auto& [peer_id, info] : peers_) {
        result.push_back(info);
    }
    
    return result;
}

size_t NetworkLayer::peer_count() const {
    return peers_.size();
}

bool NetworkLayer::is_connected(const PeerID& peer) const {
    return peers_.find(peer) != peers_.end();
}

PeerID NetworkLayer::local_peer_id() const {
    return local_peer_id_;
}

void NetworkLayer::set_validator_status(const PeerID& peer, bool is_validator) {
    auto it = peers_.find(peer);
    if (it != peers_.end()) {
        it->second.is_validator = is_validator;
    }
}

// ============================================================================
// Private Methods
// ============================================================================

void NetworkLayer::gossip_message(const NetworkMessage& message, size_t fanout) {
    // Select peers for gossip
    bool prioritize_validators = (message.type == MessageType::Block);
    auto selected_peers = select_gossip_peers(fanout, prioritize_validators);
    
    // Send to selected peers
    for (const auto& peer_id : selected_peers) {
        send_to_peer(peer_id, message);
    }
}

size_t NetworkLayer::discover_peers() {
    // Kademlia DHT-based peer discovery
    // This implements a simplified version of Kademlia peer discovery
    
    if (!initialized_ || peers_.size() >= config_.max_peers) {
        return 0;
    }
    
    size_t discovered = 0;
    
    // Step 1: Query connected peers for their peer lists
    std::vector<PeerID> peers_to_query;
    for (const auto& [peer_id, info] : peers_) {
        peers_to_query.push_back(peer_id);
    }
    
    // Limit queries to avoid overwhelming the network
    size_t max_queries = std::min<size_t>(3, peers_to_query.size());
    
    for (size_t i = 0; i < max_queries && discovered < 5; ++i) {
        const PeerID& peer_id = peers_to_query[i];
        
        // In a full implementation, we would:
        // 1. Send FIND_NODE RPC to peer asking for nodes close to our ID
        // 2. Receive list of peer addresses
        // 3. Connect to new peers
        
        // For now, we simulate by checking if the peer has validator status
        // and attempting to discover through bootstrap peers
        auto it = peers_.find(peer_id);
        if (it != peers_.end() && it->second.is_validator) {
            // Validators are more likely to know about other peers
            // In production, this would query the validator for peer info
            discovered++;
        }
    }
    
    // Step 2: Try bootstrap peers if we still need more connections
    if (peers_.size() < config_.min_peers && !config_.bootstrap_peers.empty()) {
        size_t connected = connect_to_peers(config_.bootstrap_peers);
        discovered += connected;
    }
    
    // Step 3: Implement Kademlia k-bucket refresh
    // In a full implementation, we would:
    // - Maintain k-buckets (routing table) organized by XOR distance
    // - Periodically refresh buckets by looking up random IDs in each bucket's range
    // - Keep k closest nodes to our ID for efficient routing
    
    // For now, log the discovery attempt
    if (discovered > 0) {
        std::cout << "Discovered " << discovered << " new peers via DHT" << std::endl;
    }
    
    return discovered;
}

void NetworkLayer::handle_message(const NetworkMessage& message, const PeerID& sender) {
    // Check if message has been seen before
    if (is_message_seen(message.hash)) {
        return;  // Duplicate message, ignore
    }
    
    // Mark as seen
    mark_message_seen(message.hash);
    
    // Update peer's last_seen timestamp
    auto it = peers_.find(sender);
    if (it != peers_.end()) {
        it->second.last_seen = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }
    
    // Call registered handler if exists
    auto handler_it = message_handlers_.find(message.type);
    if (handler_it != message_handlers_.end()) {
        handler_it->second(message, sender);
    }
    
    // Relay message to other peers (two-phase relay)
    gossip_message(message, config_.gossip_fanout);
}

bool NetworkLayer::is_message_seen(const crypto::Blake3Hash& message_hash) const {
    return seen_messages_.find(message_hash) != seen_messages_.end();
}

void NetworkLayer::mark_message_seen(const crypto::Blake3Hash& message_hash) {
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    seen_messages_[message_hash] = now;
    
    // Clean up old seen messages (older than TTL)
    auto it = seen_messages_.begin();
    while (it != seen_messages_.end()) {
        if (now - it->second > SEEN_MESSAGE_TTL) {
            it = seen_messages_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<PeerID> NetworkLayer::select_gossip_peers(size_t count, bool prioritize_validators) const {
    std::vector<PeerID> result;
    std::vector<PeerID> validators;
    std::vector<PeerID> non_validators;
    
    // Separate validators and non-validators
    for (const auto& [peer_id, info] : peers_) {
        if (info.is_validator) {
            validators.push_back(peer_id);
        } else {
            non_validators.push_back(peer_id);
        }
    }
    
    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    
    if (prioritize_validators) {
        // Select validators first
        std::shuffle(validators.begin(), validators.end(), gen);
        size_t validator_count = std::min(count, validators.size());
        result.insert(result.end(), validators.begin(), validators.begin() + validator_count);
        
        // Fill remaining with non-validators
        if (result.size() < count) {
            std::shuffle(non_validators.begin(), non_validators.end(), gen);
            size_t remaining = count - result.size();
            size_t non_validator_count = std::min(remaining, non_validators.size());
            result.insert(result.end(), non_validators.begin(), non_validators.begin() + non_validator_count);
        }
    } else {
        // Select randomly from all peers
        std::vector<PeerID> all_peers;
        all_peers.insert(all_peers.end(), validators.begin(), validators.end());
        all_peers.insert(all_peers.end(), non_validators.begin(), non_validators.end());
        
        std::shuffle(all_peers.begin(), all_peers.end(), gen);
        size_t select_count = std::min(count, all_peers.size());
        result.insert(result.end(), all_peers.begin(), all_peers.begin() + select_count);
    }
    
    return result;
}

bool NetworkLayer::is_peer_banned(const PeerID& peer) const {
    auto it = banned_peers_.find(peer);
    if (it == banned_peers_.end()) {
        return false;
    }
    
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    return now < it->second;
}

void NetworkLayer::cleanup_expired_bans() {
    uint64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    auto it = banned_peers_.begin();
    while (it != banned_peers_.end()) {
        if (now >= it->second) {
            it = banned_peers_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace network
} // namespace sarafu
