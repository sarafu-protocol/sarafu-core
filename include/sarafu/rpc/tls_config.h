#pragma once

#include <string>

namespace sarafu {
namespace rpc {

struct TlsConfig {
    bool enabled = false;
    std::string cert_path;
    std::string key_path;
    std::string ca_path;
    bool require_client_auth = false;
};

} // namespace rpc
} // namespace sarafu
