#include "sarafu/crypto/ed25519.h"
#include <sodium.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sarafu {
namespace crypto {

// Initialize libsodium (called once)
static bool sodium_initialized = []() {
    if (sodium_init() < 0) {
        throw std::runtime_error("Failed to initialize libsodium");
    }
    return true;
}();

// ============================================================================
// Ed25519_PublicKey Implementation
// ============================================================================

Ed25519_PublicKey::Ed25519_PublicKey() {
    data_.fill(0);
}

Ed25519_PublicKey::Ed25519_PublicKey(const KeyArray& data) : data_(data) {}

Ed25519_PublicKey::Ed25519_PublicKey(const std::vector<uint8_t>& data) {
    if (data.size() != KEY_SIZE) {
        throw std::invalid_argument("Ed25519_PublicKey: data must be exactly 32 bytes");
    }
    std::copy(data.begin(), data.end(), data_.begin());
}

bool Ed25519_PublicKey::operator==(const Ed25519_PublicKey& other) const {
    return data_ == other.data_;
}

bool Ed25519_PublicKey::operator!=(const Ed25519_PublicKey& other) const {
    return data_ != other.data_;
}

bool Ed25519_PublicKey::operator<(const Ed25519_PublicKey& other) const {
    return data_ < other.data_;
}

std::vector<uint8_t> Ed25519_PublicKey::serialize() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

std::string Ed25519_PublicKey::to_hex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

Ed25519_PublicKey Ed25519_PublicKey::from_hex(const std::string& hex) {
    if (hex.length() != KEY_SIZE * 2) {
        throw std::invalid_argument("Ed25519_PublicKey::from_hex: hex string must be 64 characters");
    }
    
    KeyArray data;
    for (size_t i = 0; i < KEY_SIZE; ++i) {
        std::string byte_str = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    
    return Ed25519_PublicKey(data);
}

// ============================================================================
// Ed25519_Signature Implementation
// ============================================================================

Ed25519_Signature::Ed25519_Signature() {
    data_.fill(0);
}

Ed25519_Signature::Ed25519_Signature(const SignatureArray& data) : data_(data) {}

Ed25519_Signature::Ed25519_Signature(const std::vector<uint8_t>& data) {
    if (data.size() != SIGNATURE_SIZE) {
        throw std::invalid_argument("Ed25519_Signature: data must be exactly 64 bytes");
    }
    std::copy(data.begin(), data.end(), data_.begin());
}

bool Ed25519_Signature::operator==(const Ed25519_Signature& other) const {
    return data_ == other.data_;
}

bool Ed25519_Signature::operator!=(const Ed25519_Signature& other) const {
    return data_ != other.data_;
}

std::vector<uint8_t> Ed25519_Signature::serialize() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

std::string Ed25519_Signature::to_hex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

Ed25519_Signature Ed25519_Signature::from_hex(const std::string& hex) {
    if (hex.length() != SIGNATURE_SIZE * 2) {
        throw std::invalid_argument("Ed25519_Signature::from_hex: hex string must be 128 characters");
    }
    
    SignatureArray data;
    for (size_t i = 0; i < SIGNATURE_SIZE; ++i) {
        std::string byte_str = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    
    return Ed25519_Signature(data);
}

// ============================================================================
// Ed25519_PrivateKey Implementation
// ============================================================================

Ed25519_PrivateKey::Ed25519_PrivateKey() {
    data_.fill(0);
}

Ed25519_PrivateKey::Ed25519_PrivateKey(const KeyArray& data) : data_(data) {}

Ed25519_PublicKey Ed25519_PrivateKey::public_key() const {
    // In libsodium's Ed25519 format, the private key is 64 bytes:
    // [32-byte seed][32-byte public key]
    // Extract the public key portion (last 32 bytes)
    Ed25519_PublicKey::KeyArray pubkey_data;
    std::copy(data_.begin() + 32, data_.end(), pubkey_data.begin());
    return Ed25519_PublicKey(pubkey_data);
}

std::vector<uint8_t> Ed25519_PrivateKey::serialize() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

std::string Ed25519_PrivateKey::to_hex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

Ed25519_PrivateKey Ed25519_PrivateKey::from_hex(const std::string& hex) {
    if (hex.length() != KEY_SIZE * 2) {
        throw std::invalid_argument("Ed25519_PrivateKey::from_hex: hex string must be 128 characters");
    }
    
    KeyArray data;
    for (size_t i = 0; i < KEY_SIZE; ++i) {
        std::string byte_str = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    
    return Ed25519_PrivateKey(data);
}

Ed25519_PrivateKey::~Ed25519_PrivateKey() {
    // Securely zero out the private key memory
    sodium_memzero(data_.data(), KEY_SIZE);
}

// ============================================================================
// Ed25519 Cryptographic Operations
// ============================================================================

namespace Ed25519 {

std::pair<Ed25519_PublicKey, Ed25519_PrivateKey> generate_keypair() {
    Ed25519_PublicKey::KeyArray public_key_data;
    Ed25519_PrivateKey::KeyArray private_key_data;
    
    // Generate keypair using libsodium
    if (crypto_sign_keypair(public_key_data.data(), private_key_data.data()) != 0) {
        throw std::runtime_error("Failed to generate Ed25519 keypair");
    }
    
    return {Ed25519_PublicKey(public_key_data), Ed25519_PrivateKey(private_key_data)};
}

Ed25519_Signature sign(
    const uint8_t* message,
    size_t message_len,
    const Ed25519_PrivateKey& private_key
) {
    Ed25519_Signature::SignatureArray signature_data;
    unsigned long long signature_len;
    
    // Sign the message using libsodium
    if (crypto_sign_detached(
            signature_data.data(),
            &signature_len,
            message,
            message_len,
            private_key.bytes()
        ) != 0) {
        throw std::runtime_error("Failed to sign message with Ed25519");
    }
    
    if (signature_len != Ed25519_Signature::SIGNATURE_SIZE) {
        throw std::runtime_error("Ed25519 signature has unexpected size");
    }
    
    return Ed25519_Signature(signature_data);
}

Ed25519_Signature sign(
    const std::vector<uint8_t>& message,
    const Ed25519_PrivateKey& private_key
) {
    return sign(message.data(), message.size(), private_key);
}

bool verify(
    const Ed25519_Signature& signature,
    const uint8_t* message,
    size_t message_len,
    const Ed25519_PublicKey& public_key
) {
    // Verify the signature using libsodium
    int result = crypto_sign_verify_detached(
        signature.bytes(),
        message,
        message_len,
        public_key.bytes()
    );
    
    return result == 0;
}

bool verify(
    const Ed25519_Signature& signature,
    const std::vector<uint8_t>& message,
    const Ed25519_PublicKey& public_key
) {
    return verify(signature, message.data(), message.size(), public_key);
}

} // namespace Ed25519

} // namespace crypto
} // namespace sarafu
