#include "sarafu/rpc/rest_gateway.h"
#include "sarafu/rpc/grpc_server.h"
#include "sarafu/version.h"
#include <google/protobuf/util/json_util.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <httplib.h>

namespace sarafu {
namespace rpc {

RestGateway::RestGateway(std::shared_ptr<GrpcServer> grpc_server)
    : grpc_server_(grpc_server), running_(false) {}

HttpResponse RestGateway::HandleRequest(const HttpRequest& request) {
    // Route based on path and method
    if (request.method == "POST" && request.path == "/api/v1/transaction") {
        return HandleSubmitTransaction(request);
    } else if (request.method == "GET" && request.path.find("/api/v1/account/") == 0 && 
               request.path.find("/balance") != std::string::npos) {
        return HandleGetAccountBalance(request);
    } else if (request.method == "GET" && request.path.find("/api/v1/account/") == 0 && 
               request.path.find("/nonce") != std::string::npos) {
        return HandleGetAccountNonce(request);
    } else if (request.method == "GET" && request.path.find("/api/v1/account/") == 0) {
        return HandleGetAccount(request);
    } else if (request.method == "GET" && request.path.find("/api/v1/transaction/") == 0 && 
               request.path.find("/status") != std::string::npos) {
        return HandleGetTransactionStatus(request);
    } else if (request.method == "GET" && request.path.find("/api/v1/transaction/") == 0 && 
               request.path.find("/receipt") != std::string::npos) {
        return HandleGetTransactionReceipt(request);
    } else if (request.method == "GET" && request.path.find("/api/v1/block/height/") == 0) {
        return HandleGetBlockByHeight(request);
    } else if (request.method == "GET" && request.path.find("/api/v1/block/hash/") == 0) {
        return HandleGetBlockByHash(request);
    } else if (request.method == "GET" && request.path == "/api/v1/validators") {
        return HandleGetValidatorSet(request);
    } else if (request.method == "GET" && request.path == "/api/v1/epoch") {
        return HandleGetCurrentEpoch(request);
    } else if (request.method == "GET" && request.path == "/api/v1/fee/base") {
        return HandleGetBaseFee(request);
    } else if (request.method == "POST" && request.path == "/api/v1/fee/estimate") {
        return HandleEstimateFee(request);
    } else if (request.method == "GET" && request.path == "/api/v1/chain_id") {
        return HandleGetChainID(request);
    } else if (request.method == "GET" && request.path == "/api/v1/node/info") {
        return HandleGetNodeInfo(request);
    }

    return CreateErrorResponse(404, "Endpoint not found");
}

void RestGateway::Start(const std::string& address, const TlsConfig& tls_config) {
    running_ = true;
    
    // Parse address (format: "host:port" or "0.0.0.0:8080")
    size_t colon_pos = address.find(':');
    std::string host = "0.0.0.0";
    int port = 8080;
    
    if (colon_pos != std::string::npos) {
        host = address.substr(0, colon_pos);
        port = std::stoi(address.substr(colon_pos + 1));
    } else {
        port = std::stoi(address);
    }
    
    std::cout << "REST gateway starting on " << host << ":" << port << std::endl;
    
    // Create HTTP server (keep it alive as member variable)
    if (tls_config.enabled) {
        if (tls_config.cert_path.empty() || tls_config.key_path.empty()) {
            std::cerr << "REST TLS enabled but cert/key not configured; refusing to start." << std::endl;
            running_ = false;
            return;
        }

        const char* client_ca = nullptr;
        if (tls_config.require_client_auth) {
            if (tls_config.ca_path.empty()) {
                std::cerr << "REST TLS client auth enabled but CA path not configured; refusing to start." << std::endl;
                running_ = false;
                return;
            }
            client_ca = tls_config.ca_path.c_str();
        }

        auto ssl_server = std::make_shared<httplib::SSLServer>(
            tls_config.cert_path.c_str(),
            tls_config.key_path.c_str(),
            client_ca
        );

        if (!ssl_server->is_valid()) {
            std::cerr << "Failed to initialize REST TLS server; check certificate/key paths." << std::endl;
            running_ = false;
            return;
        }

        http_server_ = ssl_server;
    } else {
        http_server_ = std::make_shared<httplib::Server>();
    }
    
    // Health check endpoint
    http_server_->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });
    
    // Chain ID endpoint
    http_server_->Get("/api/v1/chain_id", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/chain_id";
        auto http_res = HandleGetChainID(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });
    
    // Node info endpoint
    http_server_->Get("/api/v1/node/info", [this](const httplib::Request&, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/node/info";
        auto http_res = HandleGetNodeInfo(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });
    
    // Validators endpoint
    http_server_->Get("/api/v1/validators", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/validators";
        
        // Parse query parameters
        if (req.has_param("epoch")) {
            http_req.query_params["epoch"] = req.get_param_value("epoch");
        }
        
        auto http_res = HandleGetValidatorSet(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });

    // Account info endpoint
    http_server_->Get(R"(/api/v1/account/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/account/" + req.matches[1].str();
        auto http_res = HandleGetAccount(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });
    
    // Account balance endpoint
    http_server_->Get(R"(/api/v1/account/([^/]+)/balance)", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/account/" + req.matches[1].str() + "/balance";
        auto http_res = HandleGetAccountBalance(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });
    
    // Account nonce endpoint
    http_server_->Get(R"(/api/v1/account/([^/]+)/nonce)", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/account/" + req.matches[1].str() + "/nonce";
        auto http_res = HandleGetAccountNonce(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });

    // Transaction status endpoint
    http_server_->Get(R"(/api/v1/transaction/([^/]+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/transaction/" + req.matches[1].str() + "/status";
        auto http_res = HandleGetTransactionStatus(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });

    // Transaction receipt endpoint
    http_server_->Get(R"(/api/v1/transaction/([^/]+)/receipt)", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/transaction/" + req.matches[1].str() + "/receipt";
        auto http_res = HandleGetTransactionReceipt(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });

    // Block by height endpoint
    http_server_->Get(R"(/api/v1/block/height/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/block/height/" + req.matches[1].str();
        auto http_res = HandleGetBlockByHeight(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });

    // Block by hash endpoint
    http_server_->Get(R"(/api/v1/block/hash/([^/]+))", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/block/hash/" + req.matches[1].str();
        auto http_res = HandleGetBlockByHash(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });

    // Current epoch endpoint
    http_server_->Get("/api/v1/epoch", [this](const httplib::Request&, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/epoch";
        auto http_res = HandleGetCurrentEpoch(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });
    
    // Base fee endpoint
    http_server_->Get("/api/v1/fee/base", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "GET";
        http_req.path = "/api/v1/fee/base";
        auto http_res = HandleGetBaseFee(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });

    // Estimate fee endpoint
    http_server_->Post("/api/v1/fee/estimate", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "POST";
        http_req.path = "/api/v1/fee/estimate";
        http_req.body = req.body;
        auto http_res = HandleEstimateFee(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });
    
    // Submit transaction endpoint
    http_server_->Post("/api/v1/transaction", [this](const httplib::Request& req, httplib::Response& res) {
        HttpRequest http_req;
        http_req.method = "POST";
        http_req.path = "/api/v1/transaction";
        http_req.body = req.body;
        auto http_res = HandleSubmitTransaction(http_req);
        res.status = http_res.status_code;
        res.set_content(http_res.body, "application/json");
    });
    
    // Enable CORS
    http_server_->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });

    http_server_->Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });
    
    // Start server in a separate thread
    server_thread_ = std::thread([this, host, port, tls_config]() {
        std::cout << "HTTP server listening on " << host << ":" << port
                  << (tls_config.enabled ? " (TLS)" : "") << std::endl;
        http_server_->listen(host.c_str(), port);
    });
}

void RestGateway::Stop() {
    running_ = false;
    if (http_server_) {
        http_server_->stop();
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    std::cout << "REST gateway stopped" << std::endl;
}

HttpResponse RestGateway::HandleSubmitTransaction(const HttpRequest& request) {
    try {
        sarafu::TransactionMessage tx_msg;
        if (!ParseFromJson(request.body, &tx_msg)) {
            return CreateErrorResponse(400, "Invalid JSON format");
        }

        sarafu::SubmitTransactionResponse response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->SubmitTransaction(&context, &tx_msg, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetAccount(const HttpRequest& request) {
    try {
        // Extract address from path: /api/v1/account/{address}
        std::string address_hex = request.path.substr(std::string("/api/v1/account/").length());
        
        sarafu::GetAccountRequest req;
        req.set_address(HexToBytes(address_hex));
        
        sarafu::GetAccountResponse response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetAccount(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetAccountBalance(const HttpRequest& request) {
    try {
        // Extract address from path: /api/v1/account/{address}/balance
        size_t start = std::string("/api/v1/account/").length();
        size_t end = request.path.find("/balance");
        std::string address_hex = request.path.substr(start, end - start);
        
        sarafu::GetAccountBalanceRequest req;
        req.set_address(HexToBytes(address_hex));
        
        sarafu::GetAccountBalanceResponse response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetAccountBalance(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetAccountNonce(const HttpRequest& request) {
    try {
        size_t start = std::string("/api/v1/account/").length();
        size_t end = request.path.find("/nonce");
        std::string address_hex = request.path.substr(start, end - start);
        
        sarafu::GetAccountNonceRequest req;
        req.set_address(HexToBytes(address_hex));
        
        sarafu::GetAccountNonceResponse response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetAccountNonce(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetTransactionStatus(const HttpRequest& request) {
    try {
        size_t start = std::string("/api/v1/transaction/").length();
        size_t end = request.path.find("/status");
        std::string tx_hash_hex = request.path.substr(start, end - start);
        
        sarafu::GetTransactionStatusRequest req;
        req.set_tx_hash(HexToBytes(tx_hash_hex));
        
        sarafu::GetTransactionStatusResponse response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetTransactionStatus(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetTransactionReceipt(const HttpRequest& request) {
    try {
        size_t start = std::string("/api/v1/transaction/").length();
        size_t end = request.path.find("/receipt");
        std::string tx_hash_hex = request.path.substr(start, end - start);
        
        sarafu::GetTransactionReceiptRequest req;
        req.set_tx_hash(HexToBytes(tx_hash_hex));
        
        sarafu::GetTransactionReceiptResponse response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetTransactionReceipt(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetBlockByHeight(const HttpRequest& request) {
    try {
        std::string height_str = request.path.substr(std::string("/api/v1/block/height/").length());
        uint64_t height = std::stoull(height_str);
        
        sarafu::GetBlockByHeightRequest req;
        req.set_height(height);
        
        sarafu::BlockMessage response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetBlockByHeight(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetBlockByHash(const HttpRequest& request) {
    try {
        std::string hash_hex = request.path.substr(std::string("/api/v1/block/hash/").length());
        
        sarafu::GetBlockByHashRequest req;
        req.set_hash(HexToBytes(hash_hex));
        
        sarafu::BlockMessage response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetBlockByHash(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetValidatorSet(const HttpRequest& request) {
    try {
        sarafu::GetValidatorSetRequest req;
        
        // Check for epoch query parameter
        auto it = request.query_params.find("epoch");
        if (it != request.query_params.end()) {
            req.set_epoch(std::stoull(it->second));
        } else {
            req.set_epoch(0);  // Current epoch
        }
        
        sarafu::ValidatorSetUpdateMessage response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetValidatorSet(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetCurrentEpoch(const HttpRequest& request) {
    try {
        sarafu::Empty req;
        sarafu::GetCurrentEpochResponse response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetCurrentEpoch(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetBaseFee(const HttpRequest& request) {
    try {
        sarafu::Empty req;
        sarafu::GetBaseFeeResponse response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetBaseFee(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleEstimateFee(const HttpRequest& request) {
    try {
        sarafu::EstimateFeeRequest req;
        if (!ParseFromJson(request.body, &req)) {
            return CreateErrorResponse(400, "Invalid JSON format");
        }
        
        sarafu::EstimateFeeResponse response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->EstimateFee(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetChainID(const HttpRequest& request) {
    try {
        sarafu::Empty req;
        sarafu::GetChainIDResponse response;
        grpc::ServerContext context;
        
        auto status = grpc_server_->GetChainID(&context, &req, &response);
        
        if (status.ok()) {
            return CreateSuccessResponse(SerializeToJson(response));
        } else {
            return CreateErrorResponse(500, status.error_message());
        }
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::HandleGetNodeInfo(const HttpRequest& request) {
    try {
        sarafu::Empty req;
        sarafu::GetChainIDResponse response;
        grpc::ServerContext context;

        auto status = grpc_server_->GetChainID(&context, &req, &response);
        if (!status.ok()) {
            return CreateErrorResponse(500, status.error_message());
        }

        std::ostringstream oss;
        oss << "{"
            << "\"chain_id\":" << response.chain_id() << ","
            << "\"version\":\"" << sarafu::VERSION << "\""
            << "}";
        return CreateSuccessResponse(oss.str());
    } catch (const std::exception& e) {
        return CreateErrorResponse(500, std::string("Internal error: ") + e.what());
    }
}

HttpResponse RestGateway::CreateErrorResponse(int status_code, const std::string& message) {
    HttpResponse response;
    response.status_code = status_code;
    response.body = "{\"error\":\"" + message + "\"}";
    return response;
}

HttpResponse RestGateway::CreateSuccessResponse(const std::string& json_body) {
    HttpResponse response;
    response.status_code = 200;
    response.body = json_body;
    return response;
}

std::string RestGateway::SerializeToJson(const google::protobuf::Message& message) {
    std::string json_output;
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    
    auto status = google::protobuf::util::MessageToJsonString(message, &json_output, options);
    if (!status.ok()) {
        return "{\"error\":\"Failed to serialize to JSON\"}";
    }
    
    return json_output;
}

bool RestGateway::ParseFromJson(const std::string& json, google::protobuf::Message* message) {
    auto status = google::protobuf::util::JsonStringToMessage(json, message);
    return status.ok();
}

std::string RestGateway::BytesToHex(const std::string& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char c : bytes) {
        oss << std::setw(2) << static_cast<int>(c);
    }
    return oss.str();
}

std::string RestGateway::HexToBytes(const std::string& hex) {
    std::string bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        char byte = static_cast<char>(std::stoi(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

// HttpServer implementation

HttpServer::HttpServer() : running_(false) {}

HttpServer::~HttpServer() {
    Stop();
}

void HttpServer::SetHandler(RequestHandler handler) {
    handler_ = handler;
}

void HttpServer::Listen(const std::string& address, const TlsConfig& tls_config) {
    address_ = address;
    running_ = true;
    std::cout << "HTTP server listening on " << address << std::endl;

    size_t colon_pos = address.find(':');
    std::string host = "0.0.0.0";
    int port = 8080;
    if (colon_pos != std::string::npos) {
        host = address.substr(0, colon_pos);
        port = std::stoi(address.substr(colon_pos + 1));
    } else {
        port = std::stoi(address);
    }

    if (tls_config.enabled) {
        if (tls_config.cert_path.empty() || tls_config.key_path.empty()) {
            std::cerr << "HTTP server TLS enabled but cert/key not configured; refusing to start." << std::endl;
            running_ = false;
            return;
        }

        const char* client_ca = nullptr;
        if (tls_config.require_client_auth) {
            if (tls_config.ca_path.empty()) {
                std::cerr << "HTTP server TLS client auth enabled but CA path not configured; refusing to start." << std::endl;
                running_ = false;
                return;
            }
            client_ca = tls_config.ca_path.c_str();
        }

        auto ssl_server = std::make_shared<httplib::SSLServer>(
            tls_config.cert_path.c_str(),
            tls_config.key_path.c_str(),
            client_ca
        );

        if (!ssl_server->is_valid()) {
            std::cerr << "Failed to initialize HTTP TLS server; check certificate/key paths." << std::endl;
            running_ = false;
            return;
        }

        server_ = ssl_server;
    } else {
        server_ = std::make_shared<httplib::Server>();
    }

    auto handler = [this](const httplib::Request& req, httplib::Response& res) {
        if (!handler_) {
            res.status = 500;
            res.set_content("{\"error\":\"handler not set\"}", "application/json");
            return;
        }

        HttpRequest request;
        request.method = req.method;
        request.path = req.path;
        request.body = req.body;
        for (const auto& header : req.headers) {
            request.headers.emplace(header.first, header.second);
        }
        for (const auto& param : req.params) {
            request.query_params.emplace(param.first, param.second);
        }

        HttpResponse response = handler_(request);
        res.status = response.status_code;
        for (const auto& header : response.headers) {
            res.set_header(header.first.c_str(), header.second.c_str());
        }
        res.set_content(response.body, "application/json");
    };

    server_->Get(R"(.*)", handler);
    server_->Post(R"(.*)", handler);
    server_->Put(R"(.*)", handler);
    server_->Delete(R"(.*)", handler);
    server_->Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) { res.status = 204; });

    server_thread_ = std::thread([this, host, port]() {
        server_->listen(host.c_str(), port);
    });
}

void HttpServer::Stop() {
    running_ = false;
    if (server_) {
        server_->stop();
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}

} // namespace rpc
} // namespace sarafu
