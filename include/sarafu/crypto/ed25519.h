#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace sarafu {
namespace crypto {

/**
 * Ed25519_PublicKey represents a 32-byte Ed25519 public key.
 * 
 * Ed25519 is used for transaction signatures in the Sarafu blockchain,
 * providing fast verification and hardware wallet support.
 */
class Ed25519_PublicKey {
public:
    static constexpr size_t KEY_SIZE = 32;
    using KeyArray = std::array<uint8_t, KEY_SIZE>;

    // Constructors
    Ed25519_PublicKey();
    explicit Ed25519_PublicKey(const KeyArray& data);
    explicit Ed25519_PublicKey(const std::vector<uint8_t>& data);

    // Accessors
    const KeyArray& data() const { return data_; }
    const uint8_t* bytes() const { return data_.data(); }
    size_t size() const { return KEY_SIZE; }

    // Comparison operators
    bool operator==(const Ed25519_PublicKey& other) const;
    bool operator!=(const Ed25519_PublicKey& other) const;
    bool operator<(const Ed25519_PublicKey& other) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    std::string to_hex() const;
    static Ed25519_PublicKey from_hex(const std::string& hex);

private:
    KeyArray data_;
};

/**
 * Ed25519_Signature represents a 64-byte Ed25519 signature.
 */
class Ed25519_Signature {
public:
    static constexpr size_t SIGNATURE_SIZE = 64;
    using SignatureArray = std::array<uint8_t, SIGNATURE_SIZE>;

    // Constructors
    Ed25519_Signature();
    explicit Ed25519_Signature(const SignatureArray& data);
    explicit Ed25519_Signature(const std::vector<uint8_t>& data);

    // Accessors
    const SignatureArray& data() const { return data_; }
    const uint8_t* bytes() const { return data_.data(); }
    size_t size() const { return SIGNATURE_SIZE; }

    // Comparison operators
    bool operator==(const Ed25519_Signature& other) const;
    bool operator!=(const Ed25519_Signature& other) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    std::string to_hex() const;
    static Ed25519_Signature from_hex(const std::string& hex);

private:
    SignatureArray data_;
};

/**
 * Ed25519_PrivateKey represents a 64-byte Ed25519 private key (seed + public key).
 * 
 * Note: libsodium stores the private key as 64 bytes (32-byte seed + 32-byte public key).
 */
class Ed25519_PrivateKey {
public:
    static constexpr size_t KEY_SIZE = 64;
    using KeyArray = std::array<uint8_t, KEY_SIZE>;

    // Constructors
    Ed25519_PrivateKey();
    explicit Ed25519_PrivateKey(const KeyArray& data);

    // Accessors
    const KeyArray& data() const { return data_; }
    const uint8_t* bytes() const { return data_.data(); }
    size_t size() const { return KEY_SIZE; }

    // Extract public key from private key
    Ed25519_PublicKey public_key() const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    std::string to_hex() const;
    static Ed25519_PrivateKey from_hex(const std::string& hex);

    // Secure memory cleanup
    ~Ed25519_PrivateKey();

private:
    KeyArray data_;
};

/**
 * Ed25519 cryptographic operations using libsodium.
 */
namespace Ed25519 {

/**
 * Generate a new Ed25519 key pair.
 * 
 * @return A pair of (public_key, private_key)
 */
std::pair<Ed25519_PublicKey, Ed25519_PrivateKey> generate_keypair();

/**
 * Sign a message using Ed25519.
 * 
 * @param message The message to sign
 * @param message_len Length of the message
 * @param private_key The private key to sign with
 * @return The Ed25519 signature
 */
Ed25519_Signature sign(
    const uint8_t* message,
    size_t message_len,
    const Ed25519_PrivateKey& private_key
);

/**
 * Sign a message using Ed25519 (vector overload).
 * 
 * @param message The message to sign
 * @param private_key The private key to sign with
 * @return The Ed25519 signature
 */
Ed25519_Signature sign(
    const std::vector<uint8_t>& message,
    const Ed25519_PrivateKey& private_key
);

/**
 * Verify an Ed25519 signature.
 * 
 * @param signature The signature to verify
 * @param message The message that was signed
 * @param message_len Length of the message
 * @param public_key The public key to verify against
 * @return true if the signature is valid, false otherwise
 */
bool verify(
    const Ed25519_Signature& signature,
    const uint8_t* message,
    size_t message_len,
    const Ed25519_PublicKey& public_key
);

/**
 * Verify an Ed25519 signature (vector overload).
 * 
 * @param signature The signature to verify
 * @param message The message that was signed
 * @param public_key The public key to verify against
 * @return true if the signature is valid, false otherwise
 */
bool verify(
    const Ed25519_Signature& signature,
    const std::vector<uint8_t>& message,
    const Ed25519_PublicKey& public_key
);

} // namespace Ed25519

} // namespace crypto
} // namespace sarafu
