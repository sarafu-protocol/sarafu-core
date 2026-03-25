#pragma once

#include <google/protobuf/message.h>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "sarafu/rpc/tls_config.h"

namespace httplib {
class Server;
}

namespace sarafu {
namespace rpc {

// Forward declaration
class GrpcServer;

/**
 * HTTP request structure
 */
struct HttpRequest {
    std::string method;  // GET, POST, etc.
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
    std::map<std::string, std::string> query_params;
};

/**
 * HTTP response structure
 */
struct HttpResponse {
    int status_code;
    std::map<std::string, std::string> headers;
    std::string body;
    
    HttpResponse() : status_code(200) {
        headers["Content-Type"] = "application/json";
    }
};

/**
 * JSON REST gateway that wraps the gRPC server
 * Provides HTTP/JSON endpoints for all RPC methods
 */
class RestGateway {
public:
    /**
     * Constructor
     * @param grpc_server Pointer to the gRPC server to wrap
     */
    explicit RestGateway(std::shared_ptr<GrpcServer> grpc_server);

    ~RestGateway() = default;

    /**
     * Handle an HTTP request
     * Routes the request to the appropriate handler
     */
    HttpResponse HandleRequest(const HttpRequest& request);

    /**
     * Start the HTTP server
     * @param address Address to bind (e.g., "0.0.0.0:8080")
     */
    void Start(const std::string& address, const TlsConfig& tls_config);

    /**
     * Stop the HTTP server
     */
    void Stop();

private:
    // Handler functions for each endpoint
    HttpResponse HandleSubmitTransaction(const HttpRequest& request);
    HttpResponse HandleGetAccount(const HttpRequest& request);
    HttpResponse HandleGetAccountBalance(const HttpRequest& request);
    HttpResponse HandleGetAccountNonce(const HttpRequest& request);
    HttpResponse HandleGetTransactionStatus(const HttpRequest& request);
    HttpResponse HandleGetTransactionReceipt(const HttpRequest& request);
    HttpResponse HandleGetBlockByHeight(const HttpRequest& request);
    HttpResponse HandleGetBlockByHash(const HttpRequest& request);
    HttpResponse HandleGetValidatorSet(const HttpRequest& request);
    HttpResponse HandleGetCurrentEpoch(const HttpRequest& request);
    HttpResponse HandleGetBaseFee(const HttpRequest& request);
    HttpResponse HandleEstimateFee(const HttpRequest& request);
    HttpResponse HandleGetChainID(const HttpRequest& request);
    HttpResponse HandleGetNodeInfo(const HttpRequest& request);

    // Helper functions
    HttpResponse CreateErrorResponse(int status_code, const std::string& message);
    HttpResponse CreateSuccessResponse(const std::string& json_body);
    std::string SerializeToJson(const google::protobuf::Message& message);
    bool ParseFromJson(const std::string& json, google::protobuf::Message* message);
    std::string BytesToHex(const std::string& bytes);
    std::string HexToBytes(const std::string& hex);

    std::shared_ptr<GrpcServer> grpc_server_;
    bool running_;
    std::shared_ptr<httplib::Server> http_server_;  // Keep server alive
    std::thread server_thread_;  // Keep thread handle
};

/**
 * Simple HTTP server implementation
 * This is a minimal implementation for the REST gateway
 */
class HttpServer {
public:
    using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

    HttpServer();
    ~HttpServer();

    /**
     * Set the request handler
     */
    void SetHandler(RequestHandler handler);

    /**
     * Start listening on the given address
     */
    void Listen(const std::string& address, const TlsConfig& tls_config);

    /**
     * Stop the server
     */
    void Stop();

private:
    RequestHandler handler_;
    bool running_;
    std::string address_;
    std::shared_ptr<httplib::Server> server_;
    std::thread server_thread_;
};

} // namespace rpc
} // namespace sarafu
