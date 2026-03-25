#include "sarafu/rpc/websocket_server.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <deque>
#include <functional>
#include <iostream>
#include <sstream>

namespace sarafu {
namespace rpc {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(WebSocketServer* server) : server_(server) {}
    virtual ~WebSocketSession() = default;

    virtual void Start() = 0;
    virtual void Send(std::string message) = 0;
    virtual void Close() = 0;

protected:
    WebSocketServer* server_;
};

class WebSocketSessionPlain final : public WebSocketSession {
public:
    WebSocketSessionPlain(tcp::socket socket, WebSocketServer* server)
        : WebSocketSession(server), ws_(std::move(socket)) {}

    void Start() override {
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res) {
                res.set(beast::http::field::server, std::string("sarafu-websocket"));
            }
        ));

        auto self = shared_from_this();
        ws_.async_accept([self](beast::error_code ec) {
            if (ec) {
                return;
            }
            auto* session = static_cast<WebSocketSessionPlain*>(self.get());
            session->server_->OnOpen(static_cast<ConnectionHandle>(self.get()));
            session->DoRead();
        });
    }

    void Send(std::string message) override {
        auto self = shared_from_this();
        asio::post(ws_.get_executor(), [self, message = std::move(message)]() mutable {
            auto* session = static_cast<WebSocketSessionPlain*>(self.get());
            bool writing = !session->write_queue_.empty();
            session->write_queue_.push_back(std::move(message));
            if (!writing) {
                session->DoWrite();
            }
        });
    }

    void Close() override {
        auto self = shared_from_this();
        asio::post(ws_.get_executor(), [self]() {
            auto* session = static_cast<WebSocketSessionPlain*>(self.get());
            beast::error_code ec;
            session->ws_.close(websocket::close_code::normal, ec);
        });
    }

private:
    void DoRead() {
        auto self = shared_from_this();
        ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t) {
            auto* session = static_cast<WebSocketSessionPlain*>(self.get());
            if (ec) {
                session->server_->OnClose(static_cast<ConnectionHandle>(self.get()));
                return;
            }
            std::string message = beast::buffers_to_string(session->buffer_.data());
            session->buffer_.consume(session->buffer_.size());
            session->server_->OnMessage(static_cast<ConnectionHandle>(self.get()), message);
            session->DoRead();
        });
    }

    void DoWrite() {
        auto self = shared_from_this();
        ws_.text(true);
        ws_.async_write(asio::buffer(write_queue_.front()),
            [self](beast::error_code ec, std::size_t) {
                auto* session = static_cast<WebSocketSessionPlain*>(self.get());
                if (ec) {
                    session->server_->OnClose(static_cast<ConnectionHandle>(self.get()));
                    return;
                }
                session->write_queue_.pop_front();
                if (!session->write_queue_.empty()) {
                    session->DoWrite();
                }
            }
        );
    }

    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    std::deque<std::string> write_queue_;
};

class WebSocketSessionTls final : public WebSocketSession {
public:
    WebSocketSessionTls(tcp::socket socket, ssl::context& ctx, WebSocketServer* server)
        : WebSocketSession(server), ws_(std::move(socket), ctx) {}

    void Start() override {
        ws_.next_layer().async_handshake(ssl::stream_base::server, [self = shared_from_this()](beast::error_code ec) {
            if (ec) {
                return;
            }
            auto* session = static_cast<WebSocketSessionTls*>(self.get());
            session->ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
            session->ws_.set_option(websocket::stream_base::decorator(
                [](websocket::response_type& res) {
                    res.set(beast::http::field::server, std::string("sarafu-websocket"));
                }
            ));

            session->ws_.async_accept([self](beast::error_code accept_ec) {
                if (accept_ec) {
                    return;
                }
                auto* tls_session = static_cast<WebSocketSessionTls*>(self.get());
                tls_session->server_->OnOpen(static_cast<ConnectionHandle>(self.get()));
                tls_session->DoRead();
            });
        });
    }

    void Send(std::string message) override {
        auto self = shared_from_this();
        asio::post(ws_.get_executor(), [self, message = std::move(message)]() mutable {
            auto* session = static_cast<WebSocketSessionTls*>(self.get());
            bool writing = !session->write_queue_.empty();
            session->write_queue_.push_back(std::move(message));
            if (!writing) {
                session->DoWrite();
            }
        });
    }

    void Close() override {
        auto self = shared_from_this();
        asio::post(ws_.get_executor(), [self]() {
            auto* session = static_cast<WebSocketSessionTls*>(self.get());
            beast::error_code ec;
            session->ws_.close(websocket::close_code::normal, ec);
        });
    }

private:
    void DoRead() {
        auto self = shared_from_this();
        ws_.async_read(buffer_, [self](beast::error_code ec, std::size_t) {
            auto* session = static_cast<WebSocketSessionTls*>(self.get());
            if (ec) {
                session->server_->OnClose(static_cast<ConnectionHandle>(self.get()));
                return;
            }
            std::string message = beast::buffers_to_string(session->buffer_.data());
            session->buffer_.consume(session->buffer_.size());
            session->server_->OnMessage(static_cast<ConnectionHandle>(self.get()), message);
            session->DoRead();
        });
    }

    void DoWrite() {
        auto self = shared_from_this();
        ws_.text(true);
        ws_.async_write(asio::buffer(write_queue_.front()),
            [self](beast::error_code ec, std::size_t) {
                auto* session = static_cast<WebSocketSessionTls*>(self.get());
                if (ec) {
                    session->server_->OnClose(static_cast<ConnectionHandle>(self.get()));
                    return;
                }
                session->write_queue_.pop_front();
                if (!session->write_queue_.empty()) {
                    session->DoWrite();
                }
            }
        );
    }

    websocket::stream<ssl::stream<tcp::socket>> ws_;
    beast::flat_buffer buffer_;
    std::deque<std::string> write_queue_;
};

WebSocketServer::WebSocketServer() : running_(false) {}

WebSocketServer::~WebSocketServer() {
    Stop();
}

void WebSocketServer::Start(const std::string& address, const TlsConfig& tls_config) {
    if (running_) {
        return;
    }
    address_ = address;
    running_ = true;
    tls_enabled_ = tls_config.enabled;
    std::cout << "WebSocket server starting on " << address
              << (tls_config.enabled ? " (TLS)" : "") << std::endl;

    size_t colon_pos = address.find(':');
    std::string host = "0.0.0.0";
    std::string port = "8081";

    if (colon_pos != std::string::npos) {
        host = address.substr(0, colon_pos);
        port = address.substr(colon_pos + 1);
    } else {
        port = address;
    }

    io_context_ = std::make_unique<asio::io_context>(1);

    if (tls_enabled_) {
        if (tls_config.cert_path.empty() || tls_config.key_path.empty()) {
            std::cerr << "WebSocket TLS enabled but cert/key not configured; refusing to start." << std::endl;
            running_ = false;
            return;
        }
        if (tls_config.require_client_auth && tls_config.ca_path.empty()) {
            std::cerr << "WebSocket TLS client auth enabled but CA path not configured; refusing to start." << std::endl;
            running_ = false;
            return;
        }

        ssl_context_ = std::make_unique<ssl::context>(ssl::context::tls_server);
        ssl_context_->set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2 |
            ssl::context::no_sslv3 |
            ssl::context::single_dh_use
        );
        ssl_context_->use_certificate_chain_file(tls_config.cert_path);
        ssl_context_->use_private_key_file(tls_config.key_path, ssl::context::pem);
        if (tls_config.require_client_auth) {
            ssl_context_->load_verify_file(tls_config.ca_path);
            ssl_context_->set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
        } else {
            ssl_context_->set_verify_mode(ssl::verify_none);
        }
    }
    tcp::resolver resolver(*io_context_);
    auto results = resolver.resolve(host, port);
    auto endpoint = results.begin()->endpoint();

    acceptor_ = std::make_unique<tcp::acceptor>(*io_context_);
    acceptor_->open(endpoint.protocol());
    acceptor_->set_option(asio::socket_base::reuse_address(true));
    acceptor_->bind(endpoint);
    acceptor_->listen();

    std::function<void()> do_accept;
    do_accept = [this, &do_accept]() {
        acceptor_->async_accept([this, &do_accept](beast::error_code ec, tcp::socket socket) {
            if (!running_) {
                return;
            }
            if (!ec) {
                std::shared_ptr<WebSocketSession> session;
                if (tls_enabled_ && ssl_context_) {
                    session = std::make_shared<WebSocketSessionTls>(std::move(socket), *ssl_context_, this);
                } else {
                    session = std::make_shared<WebSocketSessionPlain>(std::move(socket), this);
                }
                RegisterSession(session);
                session->Start();
            }
            if (running_) {
                do_accept();
            }
        });
    };

    do_accept();

    server_thread_ = std::thread([this]() {
        io_context_->run();
    });
}

void WebSocketServer::Stop() {
    if (running_) {
        running_ = false;
        std::cout << "WebSocket server stopped" << std::endl;

        if (acceptor_) {
            beast::error_code ec;
            acceptor_->close(ec);
        }

        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            for (const auto& session : sessions_) {
                session->Close();
            }
            sessions_.clear();
            session_lookup_.clear();
        }

        if (io_context_) {
            io_context_->stop();
        }
        if (server_thread_.joinable()) {
            server_thread_.join();
        }

        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_.new_blocks.clear();
        subscriptions_.new_transactions.clear();
        subscriptions_.validator_set_changes.clear();
    }
}

void WebSocketServer::RegisterSession(const std::shared_ptr<WebSocketSession>& session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.insert(session);
    session_lookup_[static_cast<ConnectionHandle>(session.get())] = session;
}

void WebSocketServer::RemoveSession(ConnectionHandle conn) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = session_lookup_.find(conn);
    if (it == session_lookup_.end()) {
        return;
    }
    auto session = it->second.lock();
    if (session) {
        sessions_.erase(session);
    }
    session_lookup_.erase(it);
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
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    return sessions_.size();
}

void WebSocketServer::OnOpen(ConnectionHandle conn) {
    std::cout << "WebSocket connection opened" << std::endl;

    std::string welcome = R"({
        \"type\": \"welcome\",
        \"message\": \"Connected to Sarafu WebSocket API\",
        \"subscriptions\": [\"new_blocks\", \"new_transactions\", \"validator_set_changes\"]
    })";
    SendMessage(conn, welcome);
}

void WebSocketServer::OnClose(ConnectionHandle conn) {
    std::cout << "WebSocket connection closed" << std::endl;

    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_.new_blocks.erase(conn);
        subscriptions_.new_transactions.erase(conn);
        subscriptions_.validator_set_changes.erase(conn);
    }

    RemoveSession(conn);
}

void WebSocketServer::OnMessage(ConnectionHandle conn, const std::string& message) {
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
    std::shared_ptr<WebSocketSession> session;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = session_lookup_.find(conn);
        if (it != session_lookup_.end()) {
            session = it->second.lock();
        }
    }

    if (session) {
        session->Send(message);
    }
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
