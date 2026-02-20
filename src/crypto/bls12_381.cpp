#include "sarafu/crypto/bls12_381.h"
#include <blst.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <random>

namespace sarafu {
namespace crypto {

// Domain Separation Tag (DST) for BLS signatures
// This is used to separate different uses of BLS signatures
static const char* DST = "SARAFU_BLS_SIG_BLS12381G2_XMD:SHA-256_SSWU_RO_";
static const size_t DST_LEN = strlen(DST);

// ============================================================================
// BLS12_381_PublicKey Implementation
// ============================================================================

BLS12_381_PublicKey::BLS12_381_PublicKey() {
    data_.fill(0);
}

BLS12_381_PublicKey::BLS12_381_PublicKey(const KeyArray& data) : data_(data) {}

BLS12_381_PublicKey::BLS12_381_PublicKey(const std::vector<uint8_t>& data) {
    if (data.size() != KEY_SIZE) {
        throw std::invalid_argument("BLS12_381_PublicKey: data must be exactly 48 bytes");
    }
    std::copy(data.begin(), data.end(), data_.begin());
}

bool BLS12_381_PublicKey::operator==(const BLS12_381_PublicKey& other) const {
    return data_ == other.data_;
}

bool BLS12_381_PublicKey::operator!=(const BLS12_381_PublicKey& other) const {
    return data_ != other.data_;
}

bool BLS12_381_PublicKey::operator<(const BLS12_381_PublicKey& other) const {
    return data_ < other.data_;
}

std::vector<uint8_t> BLS12_381_PublicKey::serialize() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

std::string BLS12_381_PublicKey::to_hex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

BLS12_381_PublicKey BLS12_381_PublicKey::from_hex(const std::string& hex) {
    if (hex.length() != KEY_SIZE * 2) {
        throw std::invalid_argument("BLS12_381_PublicKey::from_hex: hex string must be 96 characters");
    }
    
    KeyArray data;
    for (size_t i = 0; i < KEY_SIZE; ++i) {
        std::string byte_str = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    
    return BLS12_381_PublicKey(data);
}

// ============================================================================
// BLS12_381_Signature Implementation
// ============================================================================

BLS12_381_Signature::BLS12_381_Signature() {
    data_.fill(0);
}

BLS12_381_Signature::BLS12_381_Signature(const SignatureArray& data) : data_(data) {}

BLS12_381_Signature::BLS12_381_Signature(const std::vector<uint8_t>& data) {
    if (data.size() != SIGNATURE_SIZE) {
        throw std::invalid_argument("BLS12_381_Signature: data must be exactly 96 bytes");
    }
    std::copy(data.begin(), data.end(), data_.begin());
}

bool BLS12_381_Signature::operator==(const BLS12_381_Signature& other) const {
    return data_ == other.data_;
}

bool BLS12_381_Signature::operator!=(const BLS12_381_Signature& other) const {
    return data_ != other.data_;
}

std::vector<uint8_t> BLS12_381_Signature::serialize() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

std::string BLS12_381_Signature::to_hex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

BLS12_381_Signature BLS12_381_Signature::from_hex(const std::string& hex) {
    if (hex.length() != SIGNATURE_SIZE * 2) {
        throw std::invalid_argument("BLS12_381_Signature::from_hex: hex string must be 192 characters");
    }
    
    SignatureArray data;
    for (size_t i = 0; i < SIGNATURE_SIZE; ++i) {
        std::string byte_str = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    
    return BLS12_381_Signature(data);
}

// ============================================================================
// BLS12_381_PrivateKey Implementation
// ============================================================================

BLS12_381_PrivateKey::BLS12_381_PrivateKey() {
    data_.fill(0);
}

BLS12_381_PrivateKey::BLS12_381_PrivateKey(const KeyArray& data) : data_(data) {}

BLS12_381_PublicKey BLS12_381_PrivateKey::public_key() const {
    blst_p1 pk_point;
    blst_scalar scalar;
    
    // Convert private key bytes to scalar
    blst_scalar_from_bendian(&scalar, data_.data());
    
    // Multiply generator by scalar to get public key point
    blst_sk_to_pk_in_g1(&pk_point, &scalar);
    
    // Serialize public key to compressed form (48 bytes)
    BLS12_381_PublicKey::KeyArray pk_bytes;
    blst_p1_compress(pk_bytes.data(), &pk_point);
    
    return BLS12_381_PublicKey(pk_bytes);
}

std::vector<uint8_t> BLS12_381_PrivateKey::serialize() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

std::string BLS12_381_PrivateKey::to_hex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

BLS12_381_PrivateKey BLS12_381_PrivateKey::from_hex(const std::string& hex) {
    if (hex.length() != KEY_SIZE * 2) {
        throw std::invalid_argument("BLS12_381_PrivateKey::from_hex: hex string must be 64 characters");
    }
    
    KeyArray data;
    for (size_t i = 0; i < KEY_SIZE; ++i) {
        std::string byte_str = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    
    return BLS12_381_PrivateKey(data);
}

BLS12_381_PrivateKey::~BLS12_381_PrivateKey() {
    // Securely zero out the private key memory
    std::fill(data_.begin(), data_.end(), 0);
}

// ============================================================================
// BLS12_381 Cryptographic Operations
// ============================================================================

namespace BLS12_381 {

std::pair<BLS12_381_PublicKey, BLS12_381_PrivateKey> generate_keypair() {
    // Generate random seed (at least 32 bytes recommended)
    std::array<uint8_t, 32> seed;
    
    // Use C++ random for seed generation
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);
    
    for (size_t i = 0; i < seed.size(); ++i) {
        seed[i] = static_cast<uint8_t>(dis(gen));
    }
    
    // Use blst_keygen to derive a proper private key from the seed
    // This ensures the key is in the correct range
    blst_scalar sk_scalar;
    blst_keygen(&sk_scalar, seed.data(), seed.size(), nullptr, 0);
    
    // Convert scalar to bytes
    BLS12_381_PrivateKey::KeyArray sk_bytes;
    blst_bendian_from_scalar(sk_bytes.data(), &sk_scalar);
    
    BLS12_381_PrivateKey private_key(sk_bytes);
    BLS12_381_PublicKey public_key = private_key.public_key();
    
    return {public_key, private_key};
}

BLS12_381_Signature sign(
    const uint8_t* message,
    size_t message_len,
    const BLS12_381_PrivateKey& private_key
) {
    blst_scalar scalar;
    blst_p2 hash_point;
    blst_p2 sig_point;
    
    // Convert private key bytes to scalar
    blst_scalar_from_bendian(&scalar, private_key.bytes());
    
    // Hash message to curve point in G2
    blst_hash_to_g2(&hash_point, 
                    message, 
                    message_len,
                    reinterpret_cast<const uint8_t*>(DST),
                    DST_LEN,
                    nullptr,  // aug (augmentation)
                    0);       // aug_len
    
    // Sign: multiply hash point by private key scalar
    blst_sign_pk_in_g1(&sig_point, &hash_point, &scalar);
    
    // Serialize signature to compressed form (96 bytes for G2)
    BLS12_381_Signature::SignatureArray sig_bytes;
    blst_p2_compress(sig_bytes.data(), &sig_point);
    
    return BLS12_381_Signature(sig_bytes);
}

BLS12_381_Signature sign(
    const std::vector<uint8_t>& message,
    const BLS12_381_PrivateKey& private_key
) {
    return sign(message.data(), message.size(), private_key);
}

bool verify(
    const BLS12_381_Signature& signature,
    const uint8_t* message,
    size_t message_len,
    const BLS12_381_PublicKey& public_key
) {
    blst_p1_affine pk_affine;
    blst_p2_affine sig_affine;
    
    // Deserialize public key
    if (blst_p1_uncompress(&pk_affine, public_key.bytes()) != BLST_SUCCESS) {
        return false;
    }
    
    // Validate public key is in the correct subgroup
    if (!blst_p1_affine_in_g1(&pk_affine)) {
        return false;
    }
    
    // Deserialize signature
    if (blst_p2_uncompress(&sig_affine, signature.bytes()) != BLST_SUCCESS) {
        return false;
    }
    
    // Validate signature is in the correct subgroup
    if (!blst_p2_affine_in_g2(&sig_affine)) {
        return false;
    }
    
    // Verify signature
    BLST_ERROR err = blst_core_verify_pk_in_g1(
        &pk_affine,
        &sig_affine,
        true,  // hash_or_encode
        message,
        message_len,
        reinterpret_cast<const uint8_t*>(DST),
        DST_LEN,
        nullptr,  // aug
        0         // aug_len
    );
    
    return err == BLST_SUCCESS;
}

bool verify(
    const BLS12_381_Signature& signature,
    const std::vector<uint8_t>& message,
    const BLS12_381_PublicKey& public_key
) {
    return verify(signature, message.data(), message.size(), public_key);
}

BLS12_381_Signature aggregate(
    const std::vector<BLS12_381_Signature>& signatures
) {
    if (signatures.empty()) {
        throw std::invalid_argument("Cannot aggregate empty signature list");
    }
    
    // Start with the first signature
    blst_p2_affine first_sig_affine;
    if (blst_p2_uncompress(&first_sig_affine, signatures[0].bytes()) != BLST_SUCCESS) {
        throw std::runtime_error("Failed to deserialize first signature");
    }
    
    // Validate first signature
    if (!blst_p2_affine_in_g2(&first_sig_affine)) {
        throw std::runtime_error("First signature is not in G2 subgroup");
    }
    
    blst_p2 aggregated;
    blst_p2_from_affine(&aggregated, &first_sig_affine);
    
    // Aggregate remaining signatures
    for (size_t i = 1; i < signatures.size(); ++i) {
        blst_p2_affine sig_affine;
        if (blst_p2_uncompress(&sig_affine, signatures[i].bytes()) != BLST_SUCCESS) {
            throw std::runtime_error("Failed to deserialize signature at index " + std::to_string(i));
        }
        
        // Validate signature
        if (!blst_p2_affine_in_g2(&sig_affine)) {
            throw std::runtime_error("Signature at index " + std::to_string(i) + " is not in G2 subgroup");
        }
        
        blst_p2 sig_point;
        blst_p2_from_affine(&sig_point, &sig_affine);
        
        // Add to aggregated signature
        blst_p2_add(&aggregated, &aggregated, &sig_point);
    }
    
    // Serialize aggregated signature
    BLS12_381_Signature::SignatureArray agg_bytes;
    blst_p2_compress(agg_bytes.data(), &aggregated);
    
    return BLS12_381_Signature(agg_bytes);
}

bool verify_aggregated(
    const BLS12_381_Signature& aggregated_signature,
    const uint8_t* message,
    size_t message_len,
    const std::vector<BLS12_381_PublicKey>& public_keys
) {
    if (public_keys.empty()) {
        throw std::invalid_argument("Cannot verify with empty public key list");
    }
    
    // For BLS signatures on the same message, we can aggregate public keys
    // and verify: e(agg_pk, H(m)) == e(G1, agg_sig)
    
    // Deserialize aggregated signature
    blst_p2_affine agg_sig_affine;
    if (blst_p2_uncompress(&agg_sig_affine, aggregated_signature.bytes()) != BLST_SUCCESS) {
        return false;
    }
    
    // Validate signature is in the correct subgroup
    if (!blst_p2_affine_in_g2(&agg_sig_affine)) {
        return false;
    }
    
    // Aggregate public keys
    blst_p1_affine first_pk_affine;
    if (blst_p1_uncompress(&first_pk_affine, public_keys[0].bytes()) != BLST_SUCCESS) {
        return false;
    }
    
    // Validate first public key
    if (!blst_p1_affine_in_g1(&first_pk_affine)) {
        return false;
    }
    
    blst_p1 aggregated_pk;
    blst_p1_from_affine(&aggregated_pk, &first_pk_affine);
    
    for (size_t i = 1; i < public_keys.size(); ++i) {
        blst_p1_affine pk_affine;
        if (blst_p1_uncompress(&pk_affine, public_keys[i].bytes()) != BLST_SUCCESS) {
            return false;
        }
        
        // Validate public key
        if (!blst_p1_affine_in_g1(&pk_affine)) {
            return false;
        }
        
        blst_p1 pk_point;
        blst_p1_from_affine(&pk_point, &pk_affine);
        
        // Add to aggregated public key
        blst_p1_add(&aggregated_pk, &aggregated_pk, &pk_point);
    }
    
    // Convert aggregated public key to affine
    blst_p1_affine agg_pk_affine;
    blst_p1_to_affine(&agg_pk_affine, &aggregated_pk);
    
    // Verify aggregated signature with aggregated public key
    BLST_ERROR err = blst_core_verify_pk_in_g1(
        &agg_pk_affine,
        &agg_sig_affine,
        true,  // hash_or_encode
        message,
        message_len,
        reinterpret_cast<const uint8_t*>(DST),
        DST_LEN,
        nullptr,  // aug
        0         // aug_len
    );
    
    return err == BLST_SUCCESS;
}

bool verify_aggregated(
    const BLS12_381_Signature& aggregated_signature,
    const std::vector<uint8_t>& message,
    const std::vector<BLS12_381_PublicKey>& public_keys
) {
    return verify_aggregated(aggregated_signature, message.data(), message.size(), public_keys);
}

} // namespace BLS12_381

} // namespace crypto
} // namespace sarafu
