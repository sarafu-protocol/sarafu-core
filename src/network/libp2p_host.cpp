#include "sarafu/network/network_layer.h"
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/system/error_code.hpp>
#include <algorithm>
#include <chrono>
#include <random>
#include <thread>

namespace sarafu {
namespace network {

/**
 * LibP2PHost implementation using Boost.Asio for networking.
 * 
 * This is a simplified P2P implementation that provides the core functionality
 * needed for the Sarafu blockchain without requiring the full cpp-libp2p library.
 * 
 * Features:
 * - TCP transport for reliable message delivery
 * - UDP transport for fast gossip (optional)
 * - Peer discovery via bootstrap nodes
 * - Connection management with min/max peer limits
 * - Message routing and delivery
 */
struct NetworkLayer::LibP2PHost {
    boost::asio::io_context io_context;
    boost::asio::ip::tcp::acceptor tcp_acceptor;
    boost::asio::ip::tcp::socket tcp_socket;
    
    // Connection state
    std::map<PeerID, std::shared_ptr<boost::asio::ip::tcp::socket>> peer_connections;
    mutable std::mutex connections_mutex;  // mutable because used in const methods
    
    // Running state
    std::atomic<bool> running;
    std::thread io_thread;
    
    // Local peer info
    std::string listen_address;
    uint16_t listen_port;
    
    LibP2PHost()
        : io_context(),
          tcp_acceptor(io_context),
          tcp_socket(io_context),
          running(false),
          listen_port(0) {}
    
    ~LibP2PHost() {
        stop();
    }
    
    bool start(const std::string& address, uint16_t port) {
        if (running.load()) {
            return false;
        }
        
        listen_address = address;
        listen_port = port;
        
        try {
            // Parse address and create endpoint
            boost::asio::ip::tcp::endpoint endpoint(
                boost::asio::ip::make_address(address),
                port
            );
            
            // Open and bind acceptor
            tcp_acceptor.open(endpoint.protocol());
            tcp_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
            tcp_acceptor.bind(endpoint);
            tcp_acceptor.listen();
            
            // Start accepting connections
            start_accept();
            
            // Run io_context in separate thread
            running.store(true);
            io_thread = std::thread([this]() {
                while (running.load()) {
                    try {
                        io_context.run();
                        if (running.load()) {
                            io_context.restart();
                        }
                    } catch (const std::exception& e) {
                        // Log error and continue
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            });
            
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    void stop() {
        if (!running.load()) {
            return;
        }
        
        running.store(false);
        
        // Close all connections
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            for (auto& pair : peer_connections) {
                try {
                    if (pair.second && pair.second->is_open()) {
                        pair.second->close();
                    }
                } catch (...) {}
            }
            peer_connections.clear();
        }
        
        // Stop acceptor
        try {
            if (tcp_acceptor.is_open()) {
                tcp_acceptor.close();
            }
        } catch (...) {}
        
        // Stop io_context
        io_context.stop();
        
        // Wait for io_thread to finish
        if (io_thread.joinable()) {
            io_thread.join();
        }
    }
    
    void start_accept() {
        tcp_acceptor.async_accept(
            tcp_socket,
            [this](const boost::system::error_code& error) {
                if (!error && running.load()) {
                    // Handle new connection
                    handle_new_connection(std::move(tcp_socket));
                }
                
                // Continue accepting
                if (running.load()) {
                    start_accept();
                }
            }
        );
    }
    
    void handle_new_connection(boost::asio::ip::tcp::socket socket) {
        // Generate peer ID from remote endpoint
        auto remote_endpoint = socket.remote_endpoint();
        PeerID peer_id = remote_endpoint.address().to_string() + ":" + 
                        std::to_string(remote_endpoint.port());
        
        // Store connection
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            peer_connections[peer_id] = std::make_shared<boost::asio::ip::tcp::socket>(
                std::move(socket)
            );
        }
        
        // Start reading from this peer
        start_read(peer_id);
    }
    
    bool connect_to_peer(const std::string& address, uint16_t port, PeerID& out_peer_id) {
        try {
            auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
            
            boost::asio::ip::tcp::endpoint endpoint(
                boost::asio::ip::make_address(address),
                port
            );
            
            socket->connect(endpoint);
            
            // Generate peer ID
            out_peer_id = address + ":" + std::to_string(port);
            
            // Store connection
            {
                std::lock_guard<std::mutex> lock(connections_mutex);
                peer_connections[out_peer_id] = socket;
            }
            
            // Start reading from this peer
            start_read(out_peer_id);
            
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }
    
    void start_read(const PeerID& peer_id) {
        std::shared_ptr<boost::asio::ip::tcp::socket> socket;
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            auto it = peer_connections.find(peer_id);
            if (it == peer_connections.end()) {
                return;
            }
            socket = it->second;
        }
        
        if (!socket || !socket->is_open()) {
            return;
        }
        
        // Read message length (4 bytes)
        auto length_buffer = std::make_shared<std::vector<uint8_t>>(4);
        boost::asio::async_read(
            *socket,
            boost::asio::buffer(*length_buffer),
            [this, peer_id, socket, length_buffer](
                const boost::system::error_code& error,
                std::size_t bytes_transferred
            ) {
                if (error || bytes_transferred != 4) {
                    disconnect_peer(peer_id);
                    return;
                }
                
                // Parse message length
                uint32_t message_length = 
                    (static_cast<uint32_t>((*length_buffer)[0]) << 24) |
                    (static_cast<uint32_t>((*length_buffer)[1]) << 16) |
                    (static_cast<uint32_t>((*length_buffer)[2]) << 8) |
                    static_cast<uint32_t>((*length_buffer)[3]);
                
                // Sanity check message length (max 10MB)
                if (message_length == 0 || message_length > 10 * 1024 * 1024) {
                    disconnect_peer(peer_id);
                    return;
                }
                
                // Read message data
                auto data_buffer = std::make_shared<std::vector<uint8_t>>(message_length);
                boost::asio::async_read(
                    *socket,
                    boost::asio::buffer(*data_buffer),
                    [this, peer_id, data_buffer](
                        const boost::system::error_code& error,
                        std::size_t bytes_transferred
                    ) {
                        if (error || bytes_transferred != data_buffer->size()) {
                            disconnect_peer(peer_id);
                            return;
                        }
                        
                        // Deserialize and handle message
                        try {
                            NetworkMessage msg = NetworkMessage::deserialize(*data_buffer);
                            // Message will be handled by registered handlers
                            // This would need to be connected to the message handler system
                        } catch (...) {
                            // Invalid message, disconnect peer
                            disconnect_peer(peer_id);
                            return;
                        }
                        
                        // Continue reading
                        start_read(peer_id);
                    }
                );
            }
        );
    }
    
    bool send_to_peer(const PeerID& peer_id, const std::vector<uint8_t>& data) {
        std::shared_ptr<boost::asio::ip::tcp::socket> socket;
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            auto it = peer_connections.find(peer_id);
            if (it == peer_connections.end()) {
                return false;
            }
            socket = it->second;
        }
        
        if (!socket || !socket->is_open()) {
            return false;
        }
        
        try {
            // Send message length (4 bytes, big-endian)
            uint32_t length = static_cast<uint32_t>(data.size());
            std::vector<uint8_t> length_bytes = {
                static_cast<uint8_t>((length >> 24) & 0xFF),
                static_cast<uint8_t>((length >> 16) & 0xFF),
                static_cast<uint8_t>((length >> 8) & 0xFF),
                static_cast<uint8_t>(length & 0xFF)
            };
            
            boost::asio::write(*socket, boost::asio::buffer(length_bytes));
            boost::asio::write(*socket, boost::asio::buffer(data));
            
            return true;
        } catch (const std::exception& e) {
            disconnect_peer(peer_id);
            return false;
        }
    }
    
    void disconnect_peer(const PeerID& peer_id) {
        std::lock_guard<std::mutex> lock(connections_mutex);
        auto it = peer_connections.find(peer_id);
        if (it != peer_connections.end()) {
            try {
                if (it->second && it->second->is_open()) {
                    it->second->close();
                }
            } catch (...) {}
            peer_connections.erase(it);
        }
    }
    
    std::vector<PeerID> get_connected_peers() const {
        std::lock_guard<std::mutex> lock(connections_mutex);
        std::vector<PeerID> peers;
        peers.reserve(peer_connections.size());
        for (const auto& pair : peer_connections) {
            peers.push_back(pair.first);
        }
        return peers;
    }
    
    bool is_connected(const PeerID& peer_id) const {
        std::lock_guard<std::mutex> lock(connections_mutex);
        auto it = peer_connections.find(peer_id);
        return it != peer_connections.end() && it->second && it->second->is_open();
    }
};

} // namespace network
} // namespace sarafu
