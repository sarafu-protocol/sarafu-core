#include "sarafu/rpc/rate_limiter.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <random>

namespace sarafu {
namespace rpc {

// RateLimiter implementation

RateLimiter::RateLimiter(const RateLimitConfig& config)
    : config_(config) {}

bool RateLimiter::AllowRequest(const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto& history = request_history_[ip_address];
    CleanupOldRequests(history);
    
    // Check if rate limit exceeded
    if (history.timestamps.size() >= config_.max_requests_per_window) {
        std::cout << "Rate limit exceeded for IP: " << ip_address 
                  << " (" << history.timestamps.size() << "/" 
                  << config_.max_requests_per_window << " requests)" << std::endl;
        return false;
    }
    
    // Record this request
    history.timestamps.push_back(std::chrono::steady_clock::now());
    return true;
}

size_t RateLimiter::GetRequestCount(const std::string& ip_address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = request_history_.find(ip_address);
    if (it == request_history_.end()) {
        return 0;
    }
    
    return it->second.timestamps.size();
}

std::chrono::seconds RateLimiter::GetTimeUntilReset(const std::string& ip_address) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = request_history_.find(ip_address);
    if (it == request_history_.end() || it->second.timestamps.empty()) {
        return std::chrono::seconds(0);
    }
    
    auto oldest_request = it->second.timestamps.front();
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - oldest_request);
    
    if (elapsed >= config_.time_window) {
        return std::chrono::seconds(0);
    }
    
    return config_.time_window - elapsed;
}

void RateLimiter::ClearIP(const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(mutex_);
    request_history_.erase(ip_address);
}

void RateLimiter::ClearAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    request_history_.clear();
}

void RateLimiter::UpdateConfig(const RateLimitConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void RateLimiter::CleanupOldRequests(RequestHistory& history) {
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - config_.time_window;
    
    // Remove timestamps older than the time window
    while (!history.timestamps.empty() && history.timestamps.front() < cutoff) {
        history.timestamps.pop_front();
    }
}

// AuthManager implementation

AuthManager::AuthManager() : enabled_(false) {}

void AuthManager::Enable() {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = true;
    std::cout << "Authentication enabled" << std::endl;
}

void AuthManager::Disable() {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = false;
    std::cout << "Authentication disabled" << std::endl;
}

bool AuthManager::IsEnabled() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

void AuthManager::AddUser(const Credentials& credentials) {
    std::lock_guard<std::mutex> lock(mutex_);
    users_[credentials.username] = credentials;
    std::cout << "User added: " << credentials.username << std::endl;
}

void AuthManager::RemoveUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    users_.erase(username);
    
    // Remove any tokens for this user
    for (auto it = tokens_.begin(); it != tokens_.end();) {
        if (it->second == username) {
            it = tokens_.erase(it);
        } else {
            ++it;
        }
    }
    
    std::cout << "User removed: " << username << std::endl;
}

bool AuthManager::Authenticate(const std::string& username,
                               const std::string& password,
                               const std::string& method) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!enabled_) {
        return true;  // Authentication disabled, allow all
    }
    
    auto it = users_.find(username);
    if (it == users_.end()) {
        std::cout << "Authentication failed: user not found: " << username << std::endl;
        return false;
    }
    
    const auto& credentials = it->second;
    
    // Verify password
    if (!VerifyPassword(password, credentials.password_hash)) {
        std::cout << "Authentication failed: invalid password for user: " << username << std::endl;
        return false;
    }
    
    // Check if method is allowed
    if (!credentials.allowed_methods.empty()) {
        auto method_it = std::find(credentials.allowed_methods.begin(),
                                   credentials.allowed_methods.end(),
                                   method);
        if (method_it == credentials.allowed_methods.end()) {
            std::cout << "Authentication failed: method not allowed: " << method 
                      << " for user: " << username << std::endl;
            return false;
        }
    }
    
    return true;
}

bool AuthManager::VerifyToken(const std::string& token, const std::string& method) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!enabled_) {
        return true;  // Authentication disabled, allow all
    }
    
    auto it = tokens_.find(token);
    if (it == tokens_.end()) {
        std::cout << "Authentication failed: invalid token" << std::endl;
        return false;
    }
    
    const std::string& username = it->second;
    auto user_it = users_.find(username);
    if (user_it == users_.end()) {
        std::cout << "Authentication failed: user not found for token" << std::endl;
        return false;
    }
    
    const auto& credentials = user_it->second;
    
    // Check if method is allowed
    if (!credentials.allowed_methods.empty()) {
        auto method_it = std::find(credentials.allowed_methods.begin(),
                                   credentials.allowed_methods.end(),
                                   method);
        if (method_it == credentials.allowed_methods.end()) {
            std::cout << "Authentication failed: method not allowed: " << method << std::endl;
            return false;
        }
    }
    
    return true;
}

std::string AuthManager::GenerateToken(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if user exists
    if (users_.find(username) == users_.end()) {
        return "";
    }
    
    // Generate random token
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) {
        oss << std::setw(2) << dis(gen);
    }
    
    std::string token = oss.str();
    tokens_[token] = username;
    
    std::cout << "Token generated for user: " << username << std::endl;
    return token;
}

std::string AuthManager::HashPassword(const std::string& password) const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(password.c_str()), 
           password.length(), hash);
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::setw(2) << static_cast<int>(hash[i]);
    }
    
    return oss.str();
}

bool AuthManager::VerifyPassword(const std::string& password, const std::string& hash) const {
    return HashPassword(password) == hash;
}

// RpcMiddleware implementation

RpcMiddleware::RpcMiddleware(std::shared_ptr<RateLimiter> rate_limiter,
                             std::shared_ptr<AuthManager> auth_manager)
    : rate_limiter_(rate_limiter), auth_manager_(auth_manager) {}

bool RpcMiddleware::AllowRequest(const std::string& ip_address,
                                const std::string& username,
                                const std::string& password,
                                const std::string& token,
                                const std::string& method,
                                std::string& error_message) {
    // Check rate limit first
    if (!rate_limiter_->AllowRequest(ip_address)) {
        error_message = "Rate limit exceeded. Please try again later.";
        auto time_until_reset = rate_limiter_->GetTimeUntilReset(ip_address);
        error_message += " Time until reset: " + std::to_string(time_until_reset.count()) + " seconds.";
        return false;
    }
    
    // Check authentication if enabled
    if (auth_manager_->IsEnabled()) {
        bool authenticated = false;
        
        // Try token authentication first
        if (!token.empty()) {
            authenticated = auth_manager_->VerifyToken(token, method);
        }
        // Fall back to username/password authentication
        else if (!username.empty() && !password.empty()) {
            authenticated = auth_manager_->Authenticate(username, password, method);
        }
        
        if (!authenticated) {
            error_message = "Authentication failed. Please provide valid credentials.";
            return false;
        }
    }
    
    return true;
}

} // namespace rpc
} // namespace sarafu
