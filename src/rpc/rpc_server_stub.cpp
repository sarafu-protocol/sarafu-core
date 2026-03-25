#include "sarafu/rpc/rpc_server.h"
#include "sarafu/logging/logger.h"
#include "sarafu/state/state_machine.h"
#include "sarafu/state/fee_market.h"
#include "sarafu/state/account.h"
#include "sarafu/state/mempool.h"
#include "sarafu/consensus/validator_registry.h"
#include <thread>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

namespace sarafu {
namespace rpc {

RpcServer::RpcServer(
    const RpcServerConfig& config,
    std::shared_ptr<state::StateMachine> state_machine,
    std::shared_ptr<state::Mempool> mempool,
    std::shared_ptr<consensus::ConsensusEngine> consensus,
    std::shared_ptr<consensus::ValidatorRegistry> validators,
    std::shared_ptr<state::FeeMarket> fee_market
)
    : config_(config),
      state_machine_(state_machine),
      mempool_(mempool),
      consensus_(consensus),
      validators_(validators),
      fee_market_(fee_market),
      running_(false) {
}

RpcServer::~RpcServer() {
    if (running_) {
        Stop();
    }
}

void RpcServer::Start() {
    if (running_) {
        LOG_WARN("RpcServer", "RPC server is already running");
        return;
    }

    LOG_INFO("RpcServer", "Starting RPC server...");

    // Start REST HTTP server
    if (config_.enable_rest) {
        // Parse address
        size_t colon_pos = config_.rest_address.find(':');
        std::string host = "0.0.0.0";
        int port = 8080;
        
        if (colon_pos != std::string::npos) {
            host = config_.rest_address.substr(0, colon_pos);
            port = std::stoi(config_.rest_address.substr(colon_pos + 1));
        }
        
        LOG_INFO("RpcServer", "Starting REST API server on " + host + ":" + std::to_string(port));
        
        // Create HTTP server
        http_server_ = std::make_shared<httplib::Server>();
        
        // Health check
        http_server_->Get("/health", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("{\"status\":\"ok\"}", "application/json");
        });
        
        // Chain ID
        http_server_->Get("/api/v1/chain_id", [this](const httplib::Request&, httplib::Response& res) {
            uint32_t chain_id = config_.chain_id;
            std::string json = "{\"chain_id\":" + std::to_string(chain_id) + "}";
            res.set_content(json, "application/json");
        });
        
        // Validators
        http_server_->Get("/api/v1/validators", [this](const httplib::Request&, httplib::Response& res) {
            // Get validators from registry
            std::stringstream ss;
            ss << "{\"validators\":[";
            
            if (validators_) {
                const auto& validator_set = validators_->current_set();
                for (size_t i = 0; i < validator_set.validators.size(); ++i) {
                    if (i > 0) ss << ",";
                    const auto& val = validator_set.validators[i];
                    ss << "{";
                    // Convert consensus key bytes to vector
                    std::vector<uint8_t> consensus_key_vec(val.consensus_key.bytes(), val.consensus_key.bytes() + val.consensus_key.size());
                    ss << "\"consensus_pubkey\":\"0x" << BytesToHex(consensus_key_vec) << "\",";
                    // Convert address bytes to vector
                    std::vector<uint8_t> address_vec(val.id.bytes(), val.id.bytes() + val.id.size());
                    ss << "\"withdrawal_address\":\"0x" << BytesToHex(address_vec) << "\",";
                    ss << "\"stake\":\"" << val.bonded_stake << "\",";
                    ss << "\"active\":" << (val.status == consensus::ValidatorStatus::Active ? "true" : "false");
                    ss << "}";
                }
            }
            
            ss << "]}";
            res.set_content(ss.str(), "application/json");
        });
        
        // Account balance
        http_server_->Get(R"(/api/v1/account/([^/]+)/balance)", [this](const httplib::Request& req, httplib::Response& res) {
            std::string address_hex = req.matches[1].str();
            // Remove 0x prefix if present
            if (address_hex.substr(0, 2) == "0x") {
                address_hex = address_hex.substr(2);
            }
            
            std::vector<uint8_t> address_bytes = HexToBytes(address_hex);
            state::Address address(address_bytes);
            uint64_t balance = 0;
            
            if (state_machine_) {
                auto account = state_machine_->get_account(address);
                balance = account.balance;
            }
            
            std::string json = "{\"address\":\"0x" + address_hex + "\",\"balance\":\"" + std::to_string(balance) + "\"}";
            res.set_content(json, "application/json");
        });
        
        // Account nonce
        http_server_->Get(R"(/api/v1/account/([^/]+)/nonce)", [this](const httplib::Request& req, httplib::Response& res) {
            std::string address_hex = req.matches[1].str();
            if (address_hex.substr(0, 2) == "0x") {
                address_hex = address_hex.substr(2);
            }
            
            std::vector<uint8_t> address_bytes = HexToBytes(address_hex);
            state::Address address(address_bytes);
            uint64_t nonce = 0;
            
            if (state_machine_) {
                auto account = state_machine_->get_account(address);
                nonce = account.nonce;
            }
            
            std::string json = "{\"address\":\"0x" + address_hex + "\",\"nonce\":" + std::to_string(nonce) + "}";
            res.set_content(json, "application/json");
        });
        
        // Base fee
        http_server_->Get("/api/v1/fee/base", [this](const httplib::Request&, httplib::Response& res) {
            uint64_t base_fee = fee_market_ ? fee_market_->current_base_fee() : 1000;
            std::string json = "{\"base_fee\":" + std::to_string(base_fee) + "}";
            res.set_content(json, "application/json");
        });
        
        // Current epoch (reads from validator registry)
        http_server_->Get("/api/v1/epoch", [this](const httplib::Request&, httplib::Response& res) {
            uint64_t epoch = 0;
            uint64_t epoch_start_height = 0;
            uint64_t epoch_end_height = 0;
            
            if (validators_) {
                const auto& validator_set = validators_->current_set();
                epoch = validator_set.epoch;
                // Estimate heights based on epoch (10,000 blocks per epoch)
                epoch_start_height = epoch * 10000;
                epoch_end_height = (epoch + 1) * 10000 - 1;
            }
            
            std::stringstream ss;
            ss << "{";
            ss << "\"epoch\":" << epoch << ",";
            ss << "\"epoch_start_height\":" << epoch_start_height << ",";
            ss << "\"epoch_end_height\":" << epoch_end_height;
            ss << "}";
            res.set_content(ss.str(), "application/json");
        });
        
        // Submit transaction
        http_server_->Post("/api/v1/transaction", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                // Parse JSON request
                nlohmann::json tx_json = nlohmann::json::parse(req.body);
                
                // Extract transaction fields
                std::vector<uint8_t> from_vec = tx_json["from"].get<std::vector<uint8_t>>();
                std::vector<uint8_t> to_vec = tx_json["to"].get<std::vector<uint8_t>>();
                uint64_t amount = std::stoull(tx_json["amount"].get<std::string>());
                uint64_t nonce = tx_json["nonce"].get<uint64_t>();
                uint64_t fee = tx_json.value("fee", 1000000);
                uint64_t gas_limit = tx_json.value("gas_limit", 21000);
                uint32_t chain_id = tx_json.value("chain_id", 1337);
                
                // Convert vectors to Address
                if (from_vec.size() != 32 || to_vec.size() != 32) {
                    res.status = 400;
                    res.set_content("{\"error\":\"Invalid address size\"}", "application/json");
                    return;
                }
                
                state::Address::AddressArray from_data, to_data;
                std::copy(from_vec.begin(), from_vec.end(), from_data.begin());
                std::copy(to_vec.begin(), to_vec.end(), to_data.begin());
                
                state::Address from_addr(from_data);
                state::Address to_addr(to_data);
                
                // Get signature if provided
                crypto::Ed25519_Signature sig;
                if (tx_json.contains("signature")) {
                    std::vector<uint8_t> signature = tx_json["signature"].get<std::vector<uint8_t>>();
                    if (signature.size() >= 64) {
                        crypto::Ed25519_Signature::SignatureArray sig_data;
                        std::copy_n(signature.begin(), 64, sig_data.begin());
                        sig = crypto::Ed25519_Signature(sig_data);
                    }
                }
                
                // Create transaction
                state::Transaction tx(
                    from_addr,
                    to_addr,
                    amount,
                    nonce,
                    fee,
                    gas_limit,
                    chain_id
                );
                
                // Set signature
                tx.signature = sig;
                
                // Submit to mempool
                if (mempool_) {
                    bool accepted = mempool_->add_transaction(tx);
                    if (accepted) {
                        // Get transaction hash
                        auto tx_hash = tx.hash();
                        std::vector<uint8_t> hash_vec(tx_hash.bytes(), tx_hash.bytes() + tx_hash.size());
                        
                        std::stringstream ss;
                        ss << "{\"accepted\":true,\"tx_hash\":\"0x" << BytesToHex(hash_vec) << "\"}";
                        res.set_content(ss.str(), "application/json");
                    } else {
                        res.status = 400;
                        res.set_content("{\"error\":\"Transaction rejected by mempool\"}", "application/json");
                    }
                } else {
                    res.status = 503;
                    res.set_content("{\"error\":\"Mempool not available\"}", "application/json");
                }
            } catch (const std::exception& e) {
                res.status = 400;
                std::string error_msg = "{\"error\":\"" + std::string(e.what()) + "\"}";
                res.set_content(error_msg, "application/json");
            }
        });
        
        // Get transaction status
        http_server_->Get(R"(/api/v1/transaction/([0-9a-fA-F]+)/status)", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                std::string tx_hash_str = req.matches[1];
                
                // For now, return pending status since we don't have block production
                // In production, this would check if the transaction is in mempool, pending, or confirmed
                std::stringstream ss;
                ss << "{";
                ss << "\"tx_hash\":\"0x" << tx_hash_str << "\",";
                ss << "\"status\":\"pending\",";
                ss << "\"confirmations\":0";
                ss << "}";
                res.set_content(ss.str(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                std::string error_msg = "{\"error\":\"" + std::string(e.what()) + "\"}";
                res.set_content(error_msg, "application/json");
            }
        });
        
        // Handle OPTIONS preflight requests for CORS
        http_server_->Options(".*", [](const httplib::Request&, httplib::Response& res) {
            res.status = 204; // No Content
        });
        
        // Enable CORS
        http_server_->set_default_headers({
            {"Access-Control-Allow-Origin", "*"},
            {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
            {"Access-Control-Allow-Headers", "Content-Type"}
        });
        
        // Start server in background thread
        server_thread_ = std::thread([this, host, port]() {
            LOG_INFO("RpcServer", "HTTP server listening on " + host + ":" + std::to_string(port));
            http_server_->listen(host.c_str(), port);
        });
    }

    // gRPC server (not yet integrated — REST API covers all endpoints)
    if (config_.enable_grpc) {
        LOG_INFO("RpcServer", "gRPC endpoint configured at: " + config_.grpc_address + " (pending integration)");
    }

    // WebSocket server (event subscription model ready, transport pending integration)
    if (config_.enable_websocket) {
        LOG_INFO("RpcServer", "WebSocket endpoint configured at: " + config_.websocket_address + " (pending integration)");
    }

    running_ = true;
    LOG_INFO("RpcServer", "RPC server started");
}

void RpcServer::Stop() {
    if (!running_) {
        return;
    }

    LOG_INFO("RpcServer", "Stopping RPC server...");

    // Stop HTTP server
    if (http_server_) {
        http_server_->stop();
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    running_ = false;
    LOG_INFO("RpcServer", "RPC server stopped");
}

// Helper functions
std::string RpcServer::BytesToHex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : bytes) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> RpcServer::HexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

} // namespace rpc
} // namespace sarafu
