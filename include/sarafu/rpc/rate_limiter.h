#pragma once

#include <string>
#include <map>
#include <mutex>
#include <chrono>
#include <deque>

namespace sarafu {
namespace rpc {

/**
 * Rate limiter configuration
 */
struct RateLimitConfig {
    size_t max_requests_per_window;  // Maximum requests allowed in the time window
    std::chrono::seconds time_window; // Time window for rate limiting
    
    RateLimitConfig()
        : max_requests_per_window(100),
          time_window(std::chrono::seconds(60)) {}
    
    RateLimitConfig(size_t max_requests, std::chrono::seconds window)
        : max_requests_per_window(max_requests),
          time_window(window) {}
};

/**
 * Rate limiter using sliding window algorithm
 * Tracks requests per IP address to prevent abuse
 */
class RateLimiter {
public:
    explicit RateLimiter(const RateLimitConfig& config = RateLimitConfig());
    
    /**
     * Check if a request from the given IP should be allowed
     * @param ip_address The IP address making the request
     * @return true if the request is allowed, false if rate limit exceeded
     */
    bool AllowRequest(const std::string& ip_address);
    
    /**
     * Get the number of requests made by an IP in the current window
     */
    size_t GetRequestCount(const std::string& ip_address) const;
    
    /**
     * Get the time until the rate limit resets for an IP
     */
    std::chrono::seconds GetTimeUntilReset(const std::string& ip_address) const;
    
    /**
     * Clear rate limit data for an IP address
     */
    void ClearIP(const std::string& ip_address);
    
    /**
     * Clear all rate limit data
     */
    void ClearAll();
    
    /**
     * Update the rate limit configuration
     */
    void UpdateConfig(const RateLimitConfig& config);

private:
    struct RequestHistory {
        std::deque<std::chrono::steady_clock::time_point> timestamps;
    };
    
    void CleanupOldRequests(RequestHistory& history);
    
    RateLimitConfig config_;
    std::map<std::string, RequestHistory> request_history_;
    mutable std::mutex mutex_;
};

/**
 * Authentication credentials
 */
struct Credentials {
    std::string username;
    std::string password_hash;  // SHA-256 hash of password
    std::vector<std::string> allowed_methods;  // Empty means all methods allowed
};

/**
 * Authentication manager
 * Handles optional authentication for RPC requests
 */
class AuthManager {
public:
    AuthManager();
    
    /**
     * Enable authentication
     */
    void Enable();
    
    /**
     * Disable authentication
     */
    void Disable();
    
    /**
     * Check if authentication is enabled
     */
    bool IsEnabled() const;
    
    /**
     * Add a user with credentials
     */
    void AddUser(const Credentials& credentials);
    
    /**
     * Remove a user
     */
    void RemoveUser(const std::string& username);
    
    /**
     * Authenticate a request
     * @param username The username
     * @param password The password (will be hashed)
     * @param method The RPC method being called
     * @return true if authentication succeeds, false otherwise
     */
    bool Authenticate(const std::string& username, 
                     const std::string& password,
                     const std::string& method) const;
    
    /**
     * Verify a token (for token-based authentication)
     */
    bool VerifyToken(const std::string& token, const std::string& method) const;
    
    /**
     * Generate a token for a user (for token-based authentication)
     */
    std::string GenerateToken(const std::string& username);

private:
    std::string HashPassword(const std::string& password) const;
    bool VerifyPassword(const std::string& password, const std::string& hash) const;
    
    bool enabled_;
    std::map<std::string, Credentials> users_;
    std::map<std::string, std::string> tokens_;  // token -> username
    mutable std::mutex mutex_;
};

/**
 * RPC middleware that combines rate limiting and authentication
 */
class RpcMiddleware {
public:
    RpcMiddleware(std::shared_ptr<RateLimiter> rate_limiter,
                  std::shared_ptr<AuthManager> auth_manager);
    
    /**
     * Check if a request should be allowed
     * @param ip_address The IP address making the request
     * @param username Optional username for authentication
     * @param password Optional password for authentication
     * @param token Optional authentication token
     * @param method The RPC method being called
     * @param error_message Output parameter for error message if request is denied
     * @return true if request is allowed, false otherwise
     */
    bool AllowRequest(const std::string& ip_address,
                     const std::string& username,
                     const std::string& password,
                     const std::string& token,
                     const std::string& method,
                     std::string& error_message);
    
    /**
     * Get rate limiter
     */
    std::shared_ptr<RateLimiter> GetRateLimiter() const { return rate_limiter_; }
    
    /**
     * Get auth manager
     */
    std::shared_ptr<AuthManager> GetAuthManager() const { return auth_manager_; }

private:
    std::shared_ptr<RateLimiter> rate_limiter_;
    std::shared_ptr<AuthManager> auth_manager_;
};

} // namespace rpc
} // namespace sarafu
