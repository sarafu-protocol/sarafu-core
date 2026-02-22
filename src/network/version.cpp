#include "version.h"
#include <sstream>
#include <regex>
#include <fstream>
#include <nlohmann/json.hpp>

namespace sarafu {
namespace network {

// Current node version - update this for each release
static const Version CURRENT_VERSION(1, 0, 0, 1);

bool Version::parse(const std::string& version_str, Version& out) {
    // Expected format: v<major>.<minor>.<patch>-p<protocol>
    // Example: v1.0.0-p1
    std::regex version_regex(R"(v(\d+)\.(\d+)\.(\d+)-p(\d+))");
    std::smatch matches;

    if (!std::regex_match(version_str, matches, version_regex)) {
        return false;
    }

    try {
        out.major = std::stoul(matches[1].str());
        out.minor = std::stoul(matches[2].str());
        out.patch = std::stoul(matches[3].str());
        out.protocol_version = std::stoul(matches[4].str());
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

std::string Version::to_string() const {
    std::ostringstream oss;
    oss << "v" << major << "." << minor << "." << patch << "-p" << protocol_version;
    return oss.str();
}

bool Version::is_compatible_with(const Version& other) const {
    // Versions are compatible if they have the same protocol version
    return protocol_version == other.protocol_version;
}

bool Version::operator<(const Version& other) const {
    if (major != other.major) return major < other.major;
    if (minor != other.minor) return minor < other.minor;
    if (patch != other.patch) return patch < other.patch;
    return protocol_version < other.protocol_version;
}

bool Version::operator==(const Version& other) const {
    return major == other.major &&
           minor == other.minor &&
           patch == other.patch &&
           protocol_version == other.protocol_version;
}

bool Version::operator!=(const Version& other) const {
    return !(*this == other);
}

Version get_node_version() {
    return CURRENT_VERSION;
}

uint32_t get_protocol_version_from_genesis(const std::string& genesis_path) {
    try {
        std::ifstream file(genesis_path);
        if (!file.is_open()) {
            // Default to protocol version 1 if genesis file not found
            return 1;
        }

        nlohmann::json genesis;
        file >> genesis;

        // Check if protocol_version is specified in genesis config
        if (genesis.contains("config") && genesis["config"].contains("protocol_version")) {
            return genesis["config"]["protocol_version"].get<uint32_t>();
        }

        // Default to protocol version 1 for backward compatibility
        return 1;
    } catch (const std::exception&) {
        // Default to protocol version 1 on any error
        return 1;
    }
}

bool verify_peer_compatibility(const Version& peer_version, std::string& error_message) {
    Version local_version = get_node_version();

    if (!local_version.is_compatible_with(peer_version)) {
        error_message = get_incompatibility_message(local_version, peer_version);
        return false;
    }

    return true;
}

std::string get_incompatibility_message(const Version& local, const Version& peer) {
    std::ostringstream oss;
    oss << "Incompatible protocol version detected:\n"
        << "  Local version:  " << local.to_string() << " (protocol " << local.protocol_version << ")\n"
        << "  Peer version:   " << peer.to_string() << " (protocol " << peer.protocol_version << ")\n"
        << "\n"
        << "Protocol versions must match for nodes to communicate.\n"
        << "Please upgrade your node software to a compatible version.\n"
        << "\n"
        << "Compatibility matrix:\n"
        << "  Protocol v1: Node versions v1.0.0 - v1.9.9\n"
        << "  Protocol v2: Node versions v2.0.0 - v2.9.9\n";
    return oss.str();
}

} // namespace network
} // namespace sarafu
