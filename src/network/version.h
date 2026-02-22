#pragma once

#include <cstdint>
#include <string>

namespace sarafu {
namespace network {

/**
 * Version information for the Sarafu blockchain node.
 * 
 * Uses semantic versioning: v<major>.<minor>.<patch>-p<protocol>
 * Example: v1.0.0-p1 (node version 1.0.0, protocol version 1)
 */
struct Version {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint32_t protocol_version;

    /**
     * Create a version from components.
     */
    Version(uint32_t maj, uint32_t min, uint32_t pat, uint32_t proto)
        : major(maj), minor(min), patch(pat), protocol_version(proto) {}

    /**
     * Parse version string (e.g., "v1.0.0-p1").
     * Returns true on success, false on parse error.
     */
    static bool parse(const std::string& version_str, Version& out);

    /**
     * Convert version to string format.
     */
    std::string to_string() const;

    /**
     * Check if two versions are protocol-compatible.
     * Versions are compatible if they have the same protocol_version.
     */
    bool is_compatible_with(const Version& other) const;

    /**
     * Compare versions (for sorting/ordering).
     */
    bool operator<(const Version& other) const;
    bool operator==(const Version& other) const;
    bool operator!=(const Version& other) const;
};

/**
 * Get the current node version.
 */
Version get_node_version();

/**
 * Get the protocol version from genesis configuration.
 */
uint32_t get_protocol_version_from_genesis(const std::string& genesis_path);

/**
 * Verify that a peer's version is compatible with ours.
 * Returns true if compatible, false otherwise.
 * If incompatible, error_message is populated with a clear explanation.
 */
bool verify_peer_compatibility(const Version& peer_version, std::string& error_message);

/**
 * Get a human-readable compatibility error message.
 */
std::string get_incompatibility_message(const Version& local, const Version& peer);

} // namespace network
} // namespace sarafu
