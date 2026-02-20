#pragma once

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include "sarafu/state/account.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/blake3_hash.h"

namespace sarafu {
namespace test_utils {

/**
 * Generate a random address for testing.
 */
inline state::Address generate_random_address() {
    std::vector<uint8_t> random_bytes(32);
    for (size_t i = 0; i < 32; ++i) {
        random_bytes[i] = static_cast<uint8_t>(rand() % 256);
    }
    return state::Address(random_bytes);
}

/**
 * Derive an address from a public key.
 * 
 * In a real implementation, this would hash the public key.
 * For testing, we'll use a simplified approach.
 */
inline state::Address address_from_public_key(const crypto::Ed25519_PublicKey& public_key) {
    // Hash the public key to get an address
    auto pubkey_bytes = public_key.serialize();
    auto hash = crypto::Blake3Hash::hash(pubkey_bytes);
    return state::Address(hash.serialize());
}

} // namespace test_utils
} // namespace sarafu
