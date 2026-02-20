#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace sarafu {
namespace crypto {

/**
 * BLS12_381_PublicKey represents a 48-byte BLS12-381 public key.
 * 
 * BLS12-381 is used for validator signatures in the Sarafu blockchain,
 * enabling signature aggregation for compact Quorum Certificates.
 */
class BLS12_381_PublicKey {
public:
    static constexpr size_t KEY_SIZE = 48;
    using KeyArray = std::array<uint8_t, KEY_SIZE>;

    // Constructors
    BLS12_381_PublicKey();
    explicit BLS12_381_PublicKey(const KeyArray& data);
    explicit BLS12_381_PublicKey(const std::vector<uint8_t>& data);

    // Accessors
    const KeyArray& data() const { return data_; }
    const uint8_t* bytes() const { return data_.data(); }
    size_t size() const { return KEY_SIZE; }

    // Comparison operators
    bool operator==(const BLS12_381_PublicKey& other) const;
    bool operator!=(const BLS12_381_PublicKey& other) const;
    bool operator<(const BLS12_381_PublicKey& other) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    std::string to_hex() const;
    static BLS12_381_PublicKey from_hex(const std::string& hex);

private:
    KeyArray data_;
};

/**
 * BLS12_381_Signature represents a 96-byte BLS12-381 signature.
 * 
 * BLS signatures can be aggregated: multiple signatures on the same message
 * can be combined into a single 96-byte signature.
 * 
 * Note: In the minimal-pubkey-size variant (pk in G1, sig in G2),
 * signatures are 96 bytes and public keys are 48 bytes.
 */
class BLS12_381_Signature {
public:
    static constexpr size_t SIGNATURE_SIZE = 96;
    using SignatureArray = std::array<uint8_t, SIGNATURE_SIZE>;

    // Constructors
    BLS12_381_Signature();
    explicit BLS12_381_Signature(const SignatureArray& data);
    explicit BLS12_381_Signature(const std::vector<uint8_t>& data);

    // Accessors
    const SignatureArray& data() const { return data_; }
    const uint8_t* bytes() const { return data_.data(); }
    size_t size() const { return SIGNATURE_SIZE; }

    // Comparison operators
    bool operator==(const BLS12_381_Signature& other) const;
    bool operator!=(const BLS12_381_Signature& other) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    std::string to_hex() const;
    static BLS12_381_Signature from_hex(const std::string& hex);

private:
    SignatureArray data_;
};

/**
 * BLS12_381_PrivateKey represents a 32-byte BLS12-381 private key (scalar).
 */
class BLS12_381_PrivateKey {
public:
    static constexpr size_t KEY_SIZE = 32;
    using KeyArray = std::array<uint8_t, KEY_SIZE>;

    // Constructors
    BLS12_381_PrivateKey();
    explicit BLS12_381_PrivateKey(const KeyArray& data);

    // Accessors
    const KeyArray& data() const { return data_; }
    const uint8_t* bytes() const { return data_.data(); }
    size_t size() const { return KEY_SIZE; }

    // Derive public key from private key
    BLS12_381_PublicKey public_key() const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    std::string to_hex() const;
    static BLS12_381_PrivateKey from_hex(const std::string& hex);

    // Secure memory cleanup
    ~BLS12_381_PrivateKey();

private:
    KeyArray data_;
};

/**
 * BLS12-381 cryptographic operations using blst library.
 */
namespace BLS12_381 {

/**
 * Generate a new BLS12-381 key pair.
 * 
 * @return A pair of (public_key, private_key)
 */
std::pair<BLS12_381_PublicKey, BLS12_381_PrivateKey> generate_keypair();

/**
 * Sign a message using BLS12-381.
 * 
 * @param message The message to sign
 * @param message_len Length of the message
 * @param private_key The private key to sign with
 * @return The BLS12-381 signature
 */
BLS12_381_Signature sign(
    const uint8_t* message,
    size_t message_len,
    const BLS12_381_PrivateKey& private_key
);

/**
 * Sign a message using BLS12-381 (vector overload).
 * 
 * @param message The message to sign
 * @param private_key The private key to sign with
 * @return The BLS12-381 signature
 */
BLS12_381_Signature sign(
    const std::vector<uint8_t>& message,
    const BLS12_381_PrivateKey& private_key
);

/**
 * Verify a BLS12-381 signature.
 * 
 * @param signature The signature to verify
 * @param message The message that was signed
 * @param message_len Length of the message
 * @param public_key The public key to verify against
 * @return true if the signature is valid, false otherwise
 */
bool verify(
    const BLS12_381_Signature& signature,
    const uint8_t* message,
    size_t message_len,
    const BLS12_381_PublicKey& public_key
);

/**
 * Verify a BLS12-381 signature (vector overload).
 * 
 * @param signature The signature to verify
 * @param message The message that was signed
 * @param public_key The public key to verify against
 * @return true if the signature is valid, false otherwise
 */
bool verify(
    const BLS12_381_Signature& signature,
    const std::vector<uint8_t>& message,
    const BLS12_381_PublicKey& public_key
);

/**
 * Aggregate multiple BLS12-381 signatures into a single signature.
 * 
 * All signatures must be on the same message. The aggregated signature
 * is the same size (48 bytes) as individual signatures.
 * 
 * @param signatures Vector of signatures to aggregate
 * @return The aggregated signature
 * @throws std::invalid_argument if signatures vector is empty
 */
BLS12_381_Signature aggregate(
    const std::vector<BLS12_381_Signature>& signatures
);

/**
 * Verify an aggregated BLS12-381 signature against multiple public keys.
 * 
 * This performs batch verification: checks that the aggregated signature
 * is valid for the given message signed by all the provided public keys.
 * 
 * @param aggregated_signature The aggregated signature to verify
 * @param message The message that was signed
 * @param message_len Length of the message
 * @param public_keys Vector of public keys that signed the message
 * @return true if the aggregated signature is valid, false otherwise
 * @throws std::invalid_argument if public_keys vector is empty
 */
bool verify_aggregated(
    const BLS12_381_Signature& aggregated_signature,
    const uint8_t* message,
    size_t message_len,
    const std::vector<BLS12_381_PublicKey>& public_keys
);

/**
 * Verify an aggregated BLS12-381 signature (vector overload).
 * 
 * @param aggregated_signature The aggregated signature to verify
 * @param message The message that was signed
 * @param public_keys Vector of public keys that signed the message
 * @return true if the aggregated signature is valid, false otherwise
 * @throws std::invalid_argument if public_keys vector is empty
 */
bool verify_aggregated(
    const BLS12_381_Signature& aggregated_signature,
    const std::vector<uint8_t>& message,
    const std::vector<BLS12_381_PublicKey>& public_keys
);

} // namespace BLS12_381

} // namespace crypto
} // namespace sarafu
