#include "sarafu/rpc/websocket_server.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace sarafu {
namespace rpc {

WebSocketServer::WebSocketServer() : running_(false) {}

WebSocketServer::~WebSocketServer() {
    Stop();
}

void WebSocketServer::Start(const std::string& address) {
    address_ = address;
    running_ = true;
    std::cout << "WebSocket server starting on " << address << std::endl;
    
    // TODO: Implement actual WebSocket server using a library like websocketpp or Boost.Beast
    // This would set up the WebSocket endpoint and start accepting connections
}

void WebSocketServer::Stop() {
    if (running_) {
        running_ = false;
        std::cout << "WebSocket server stopped" << std::endl;
        
        // Clear all subscriptions
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_.new_blocks.clear();
        subscriptions_.new_transactions.clear();
        subscriptions_.validator_set_changes.clear();
    }
}

void WebSocketServer::Subscribe(ConnectionHandle conn, SubscriptionType type) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    switch (type) {
        case SubscriptionType::NewBlocks:
            subscriptions_.new_blocks.insert(conn);
            std::cout << "Client subscribed to new blocks" << std::endl;
            break;
        case SubscriptionType::NewTransactions:
            subscriptions_.new_transactions.insert(conn);
            std::cout << "Client subscribed to new transactions" << std::endl;
            break;
        case SubscriptionType::ValidatorSetChanges:
            subscriptions_.validator_set_changes.insert(conn);
            std::cout << "Client subscribed to validator set changes" << std::endl;
            break;
    }
}

void WebSocketServer::Unsubscribe(ConnectionHandle conn, SubscriptionType type) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    switch (type) {
        case SubscriptionType::NewBlocks:
            subscriptions_.new_blocks.erase(conn);
            break;
        case SubscriptionType::NewTransactions:
            subscriptions_.new_transactions.erase(conn);
            break;
        case SubscriptionType::ValidatorSetChanges:
            subscriptions_.validator_set_changes.erase(conn);
            break;
    }
}

void WebSocketServer::BroadcastNewBlock(const NewBlockEvent& event) {
    std::string message = SerializeNewBlockEvent(event);
    BroadcastToSubscribers(SubscriptionType::NewBlocks, message);
}

void WebSocketServer::BroadcastNewTransaction(const NewTransactionEvent& event) {
    std::string message = SerializeNewTransactionEvent(event);
    BroadcastToSubscribers(SubscriptionType::NewTransactions, message);
}

void WebSocketServer::BroadcastValidatorSetChange(const ValidatorSetChangeEvent& event) {
    std::string message = SerializeValidatorSetChangeEvent(event);
    BroadcastToSubscribers(SubscriptionType::ValidatorSetChanges, message);
}

size_t WebSocketServer::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    std::set<ConnectionHandle> all_connections;
    all_connections.insert(subscriptions_.new_blocks.begin(), subscriptions_.new_blocks.end());
    all_connections.insert(subscriptions_.new_transactions.begin(), subscriptions_.new_transactions.end());
    all_connections.insert(subscriptions_.validator_set_changes.begin(), subscriptions_.validator_set_changes.end());
    
    return all_connections.size();
}

void WebSocketServer::OnOpen(ConnectionHandle conn) {
    std::cout << "WebSocket connection opened" << std::endl;
    
    // Send welcome message
    std::string welcome = R"({
        "type": "welcome",
        "message": "Connected to Sarafu WebSocket API",
        "subscriptions": ["new_blocks", "new_transactions", "validator_set_changes"]
    })";
    SendMessage(conn, welcome);
}

void WebSocketServer::OnClose(ConnectionHandle conn) {
    std::cout << "WebSocket connection closed" << std::endl;
    
    // Remove from all subscriptions
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    subscriptions_.new_blocks.erase(conn);
    subscriptions_.new_transactions.erase(conn);
    subscriptions_.validator_set_changes.erase(conn);
}

void WebSocketServer::OnMessage(ConnectionHandle conn, const std::string& message) {
    // Parse message and handle subscription requests
    // Expected format: {"action": "subscribe", "type": "new_blocks"}
    
    if (message.find("\"subscribe\"") != std::string::npos) {
        HandleSubscribeMessage(conn, message);
    } else if (message.find("\"unsubscribe\"") != std::string::npos) {
        HandleUnsubscribeMessage(conn, message);
    } else {
        std::string error = R"({"type": "error", "message": "Unknown action"})";
        SendMessage(conn, error);
    }
}

void WebSocketServer::HandleSubscribeMessage(ConnectionHandle conn, const std::string& message) {
    // Parse subscription type from message
    SubscriptionType type;
    
    if (message.find("\"new_blocks\"") != std::string::npos) {
        type = SubscriptionType::NewBlocks;
    } else if (message.find("\"new_transactions\"") != std::string::npos) {
        type = SubscriptionType::NewTransactions;
    } else if (message.find("\"validator_set_changes\"") != std::string::npos) {
        type = SubscriptionType::ValidatorSetChanges;
    } else {
        std::string error = R"({"type": "error", "message": "Invalid subscription type"})";
        SendMessage(conn, error);
        return;
    }
    
    Subscribe(conn, type);
    
    std::string response = R"({"type": "subscribed", "subscription": ")" + message + R"("})";
    SendMessage(conn, response);
}

void WebSocketServer::HandleUnsubscribeMessage(ConnectionHandle conn, const std::string& message) {
    // Parse subscription type from message
    SubscriptionType type;
    
    if (message.find("\"new_blocks\"") != std::string::npos) {
        type = SubscriptionType::NewBlocks;
    } else if (message.find("\"new_transactions\"") != std::string::npos) {
        type = SubscriptionType::NewTransactions;
    } else if (message.find("\"validator_set_changes\"") != std::string::npos) {
        type = SubscriptionType::ValidatorSetChanges;
    } else {
        std::string error = R"({"type": "error", "message": "Invalid subscription type"})";
        SendMessage(conn, error);
        return;
    }
    
    Unsubscribe(conn, type);
    
    std::string response = R"({"type": "unsubscribed", "subscription": ")" + message + R"("})";
    SendMessage(conn, response);
}

void WebSocketServer::SendMessage(ConnectionHandle conn, const std::string& message) {
    // TODO: Implement actual WebSocket message sending
    // This would use the WebSocket library to send the message to the connection
    std::cout << "Sending message to client: " << message << std::endl;
}

void WebSocketServer::BroadcastToSubscribers(SubscriptionType type, const std::string& message) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    
    const std::set<ConnectionHandle>* subscribers = nullptr;
    
    switch (type) {
        case SubscriptionType::NewBlocks:
            subscribers = &subscriptions_.new_blocks;
            break;
        case SubscriptionType::NewTransactions:
            subscribers = &subscriptions_.new_transactions;
            break;
        case SubscriptionType::ValidatorSetChanges:
            subscribers = &subscriptions_.validator_set_changes;
            break;
    }
    
    if (subscribers) {
        for (auto conn : *subscribers) {
            SendMessage(conn, message);
        }
    }
}

std::string WebSocketServer::SerializeNewBlockEvent(const NewBlockEvent& event) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"new_block\","
        << "\"height\":" << event.height << ","
        << "\"block_hash\":\"" << event.block_hash << "\","
        << "\"timestamp\":" << event.timestamp << ","
        << "\"proposer\":\"" << event.proposer << "\","
        << "\"transaction_count\":" << event.transaction_count
        << "}";
    return oss.str();
}

std::string WebSocketServer::SerializeNewTransactionEvent(const NewTransactionEvent& event) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"new_transaction\","
        << "\"tx_hash\":\"" << event.tx_hash << "\","
        << "\"from\":\"" << event.from << "\","
        << "\"to\":\"" << event.to << "\","
        << "\"amount\":" << event.amount << ","
        << "\"fee\":" << event.fee
        << "}";
    return oss.str();
}

std::string WebSocketServer::SerializeValidatorSetChangeEvent(const ValidatorSetChangeEvent& event) {
    std::ostringstream oss;
    oss << "{"
        << "\"type\":\"validator_set_change\","
        << "\"epoch\":" << event.epoch << ","
        << "\"validator_count\":" << event.validator_count << ","
        << "\"total_stake\":" << event.total_stake << ","
        << "\"added_validators\":[";
    
    for (size_t i = 0; i < event.added_validators.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << event.added_validators[i] << "\"";
    }
    
    oss << "],\"removed_validators\":[";
    
    for (size_t i = 0; i < event.removed_validators.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "\"" << event.removed_validators[i] << "\"";
    }
    
    oss << "]}";
    return oss.str();
}

// EventPublisher implementation

EventPublisher::EventPublisher(std::shared_ptr<WebSocketServer> ws_server)
    : ws_server_(ws_server) {}

void EventPublisher::PublishNewBlock(const NewBlockEvent& event) {
    if (ws_server_) {
        ws_server_->BroadcastNewBlock(event);
    }
}

void EventPublisher::PublishNewTransaction(const NewTransactionEvent& event) {
    if (ws_server_) {
        ws_server_->BroadcastNewTransaction(event);
    }
}

void EventPublisher::PublishValidatorSetChange(const ValidatorSetChangeEvent& event) {
    if (ws_server_) {
        ws_server_->BroadcastValidatorSetChange(event);
    }
}

} // namespace rpc
} // namespace sarafu
