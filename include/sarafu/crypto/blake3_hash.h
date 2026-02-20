#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sarafu {
namespace crypto {

/**
 * Blake3Hash represents a 32-byte Blake3 hash value.
 * 
 * This class provides a wrapper around the Blake3 cryptographic hash function,
 * which is used throughout the Sarafu blockchain for:
 * - Block hashing
 * - Transaction hashing
 * - Merkle tree construction
 * - State commitments
 */
class Blake3Hash {
public:
    static constexpr size_t HASH_SIZE = 32;
    using HashArray = std::array<uint8_t, HASH_SIZE>;

    // Constructors
    Blake3Hash();
    explicit Blake3Hash(const HashArray& data);
    explicit Blake3Hash(const std::vector<uint8_t>& data);

    // Hash computation
    static Blake3Hash hash(const uint8_t* data, size_t length);
    static Blake3Hash hash(const std::vector<uint8_t>& data);
    static Blake3Hash hash(const std::string& data);

    // Accessors
    const HashArray& data() const { return data_; }
    const uint8_t* bytes() const { return data_.data(); }
    size_t size() const { return HASH_SIZE; }

    // Comparison operators
    bool operator==(const Blake3Hash& other) const;
    bool operator!=(const Blake3Hash& other) const;
    bool operator<(const Blake3Hash& other) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    std::string to_hex() const;
    static Blake3Hash from_hex(const std::string& hex);

    // Zero hash (all zeros)
    static Blake3Hash zero();

private:
    HashArray data_;
};

} // namespace crypto
} // namespace sarafu
