#include "sarafu/crypto/blake3_hash.h"
#include <blake3.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sarafu {
namespace crypto {

Blake3Hash::Blake3Hash() {
    data_.fill(0);
}

Blake3Hash::Blake3Hash(const HashArray& data) : data_(data) {}

Blake3Hash::Blake3Hash(const std::vector<uint8_t>& data) {
    if (data.size() != HASH_SIZE) {
        throw std::invalid_argument("Blake3Hash: data must be exactly 32 bytes");
    }
    std::copy(data.begin(), data.end(), data_.begin());
}

Blake3Hash Blake3Hash::hash(const uint8_t* data, size_t length) {
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, data, length);
    
    HashArray output;
    blake3_hasher_finalize(&hasher, output.data(), HASH_SIZE);
    
    return Blake3Hash(output);
}

Blake3Hash Blake3Hash::hash(const std::vector<uint8_t>& data) {
    return hash(data.data(), data.size());
}

Blake3Hash Blake3Hash::hash(const std::string& data) {
    return hash(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool Blake3Hash::operator==(const Blake3Hash& other) const {
    return data_ == other.data_;
}

bool Blake3Hash::operator!=(const Blake3Hash& other) const {
    return data_ != other.data_;
}

bool Blake3Hash::operator<(const Blake3Hash& other) const {
    return data_ < other.data_;
}

std::vector<uint8_t> Blake3Hash::serialize() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

std::string Blake3Hash::to_hex() const {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (uint8_t byte : data_) {
        oss << std::setw(2) << static_cast<int>(byte);
    }
    return oss.str();
}

Blake3Hash Blake3Hash::from_hex(const std::string& hex) {
    if (hex.length() != HASH_SIZE * 2) {
        throw std::invalid_argument("Blake3Hash::from_hex: hex string must be 64 characters");
    }
    
    HashArray data;
    for (size_t i = 0; i < HASH_SIZE; ++i) {
        std::string byte_str = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    
    return Blake3Hash(data);
}

Blake3Hash Blake3Hash::zero() {
    return Blake3Hash();
}

} // namespace crypto
} // namespace sarafu
