#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "sarafu/rpc/tls_config.h"

namespace sarafu {
namespace rpc {

class WebSocketSession;

/**
 * Subscription types for WebSocket clients
 */
enum class SubscriptionType {
    NewBlocks,
    NewTransactions,
    ValidatorSetChanges
};

/**
 * WebSocket connection handle
 */
using ConnectionHandle = void*;

/**
 * Event data structures
 */
struct NewBlockEvent {
    uint64_t height;
    std::string block_hash;
    uint64_t timestamp;
    std::string proposer;
    size_t transaction_count;
};

struct NewTransactionEvent {
    std::string tx_hash;
    std::string from;
    std::string to;
    uint64_t amount;
    uint64_t fee;
};

struct ValidatorSetChangeEvent {
    uint64_t epoch;
    size_t validator_count;
    uint64_t total_stake;
    std::vector<std::string> added_validators;
    std::vector<std::string> removed_validators;
};

/**
 * WebSocket server for real-time event subscriptions
 * Provides subscriptions for new blocks, transactions, and validator set changes
 */
class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();

    /**
     * Start the WebSocket server
     * @param address Address to bind (e.g., "0.0.0.0:8081")
     */
    void Start(const std::string& address, const TlsConfig& tls_config);

    /**
     * Stop the WebSocket server
     */
    void Stop();

    /**
     * Subscribe a connection to a specific event type
     */
    void Subscribe(ConnectionHandle conn, SubscriptionType type);

    /**
     * Unsubscribe a connection from a specific event type
     */
    void Unsubscribe(ConnectionHandle conn, SubscriptionType type);

    /**
     * Broadcast a new block event to all subscribed clients
     */
    void BroadcastNewBlock(const NewBlockEvent& event);

    /**
     * Broadcast a new transaction event to all subscribed clients
     */
    void BroadcastNewTransaction(const NewTransactionEvent& event);

    /**
     * Broadcast a validator set change event to all subscribed clients
     */
    void BroadcastValidatorSetChange(const ValidatorSetChangeEvent& event);

    /**
     * Get the number of active connections
     */
    size_t GetConnectionCount() const;

private:
    friend class WebSocketSession;
    friend class WebSocketSessionPlain;
    friend class WebSocketSessionTls;

    // Connection management
    void OnOpen(ConnectionHandle conn);
    void OnClose(ConnectionHandle conn);
    void OnMessage(ConnectionHandle conn, const std::string& message);
    void RegisterSession(const std::shared_ptr<WebSocketSession>& session);
    void RemoveSession(ConnectionHandle conn);

    // Message handling
    void HandleSubscribeMessage(ConnectionHandle conn, const std::string& message);
    void HandleUnsubscribeMessage(ConnectionHandle conn, const std::string& message);

    // Helper functions
    void SendMessage(ConnectionHandle conn, const std::string& message);
    void BroadcastToSubscribers(SubscriptionType type, const std::string& message);
    std::string SerializeNewBlockEvent(const NewBlockEvent& event);
    std::string SerializeNewTransactionEvent(const NewTransactionEvent& event);
    std::string SerializeValidatorSetChangeEvent(const ValidatorSetChangeEvent& event);

    // Subscription tracking
    struct Subscriptions {
        std::set<ConnectionHandle> new_blocks;
        std::set<ConnectionHandle> new_transactions;
        std::set<ConnectionHandle> validator_set_changes;
    };

    Subscriptions subscriptions_;
    mutable std::mutex subscriptions_mutex_;
    
    bool running_;
    std::string address_;
    mutable std::mutex sessions_mutex_;
    std::unordered_set<std::shared_ptr<WebSocketSession>> sessions_;
    std::unordered_map<ConnectionHandle, std::weak_ptr<WebSocketSession>> session_lookup_;
    std::unique_ptr<boost::asio::io_context> io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::unique_ptr<boost::asio::ssl::context> ssl_context_;
    bool tls_enabled_ = false;
    std::thread server_thread_;
};

/**
 * Event publisher interface
 * Components can use this to publish events to WebSocket subscribers
 */
class EventPublisher {
public:
    explicit EventPublisher(std::shared_ptr<WebSocketServer> ws_server);

    /**
     * Publish a new block event
     */
    void PublishNewBlock(const NewBlockEvent& event);

    /**
     * Publish a new transaction event
     */
    void PublishNewTransaction(const NewTransactionEvent& event);

    /**
     * Publish a validator set change event
     */
    void PublishValidatorSetChange(const ValidatorSetChangeEvent& event);

private:
    std::shared_ptr<WebSocketServer> ws_server_;
};

} // namespace rpc
} // namespace sarafu
