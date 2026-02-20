#include "sarafu/rpc/rest_gateway.h"
#include "sarafu/rpc/grpc_server.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <google/protobuf/util/json_util.h>

namespace sarafu {
namespace rpc {

RestGateway::RestGateway(std::shared_ptr<GrpcServer> grpc_server)
    : grpc_server_(grpc_server), running_(false) {}

HttpResponse RestGateway::HandleRequest(const HttpRequest& request) {
    // Route based on path and method
    if (request.method == "POST" && request.path == "/api/v1/transaction") {
        return HandleSubmitTransaction(request);
    } else if (request.method == "GET" && request.path.find("/api/v1/account/") == 0) {
        return HandleGetAccount(request);
    } else if (request.method == "GET" && request.path.find("/api/v1/account/") == 0 && 
               request.path.find("/balance") != std::string::npos) {
        return HandleGetAccountBalance(request);
    } else if (request.method == "GET" && request.path.find("/api/v1/account/") == 0 && 
               request.path.find("/nonce") != std::string::npos) {
        return HandleGetAccountNonce(request);
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
    }

    return CreateErrorResponse(404, "Endpoint not found");
}

void RestGateway::Start(const std::string& address) {
    running_ = true;
    std::cout << "REST gateway starting on " << address << std::endl;
    // TODO: Implement actual HTTP server
    // This would typically use a library like cpp-httplib or Boost.Beast
}

void RestGateway::Stop() {
    running_ = false;
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
    options.always_print_primitive_fields = true;
    
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

// HttpServer implementation (minimal stub)

HttpServer::HttpServer() : running_(false) {}

HttpServer::~HttpServer() {
    Stop();
}

void HttpServer::SetHandler(RequestHandler handler) {
    handler_ = handler;
}

void HttpServer::Listen(const std::string& address) {
    address_ = address;
    running_ = true;
    std::cout << "HTTP server listening on " << address << std::endl;
    // TODO: Implement actual HTTP server using a library like cpp-httplib or Boost.Beast
}

void HttpServer::Stop() {
    running_ = false;
}

} // namespace rpc
} // namespace sarafu
