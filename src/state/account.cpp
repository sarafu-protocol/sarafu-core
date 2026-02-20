#include "sarafu/state/account.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace sarafu {
namespace state {

// ============================================================================
// Address Implementation
// ============================================================================

Address::Address() : data_{} {}

Address::Address(const AddressArray& data) : data_(data) {}

Address::Address(const std::vector<uint8_t>& data) {
    if (data.size() != ADDRESS_SIZE) {
        throw std::invalid_argument("Address data must be exactly 32 bytes");
    }
    std::copy(data.begin(), data.end(), data_.begin());
}

Address Address::from_public_key(const std::vector<uint8_t>& public_key) {
    // Hash the public key with Blake3 to get the address
    auto hash = crypto::Blake3Hash::hash(public_key);
    return Address(hash.data());
}

bool Address::operator==(const Address& other) const {
    return data_ == other.data_;
}

bool Address::operator!=(const Address& other) const {
    return !(*this == other);
}

bool Address::operator<(const Address& other) const {
    return data_ < other.data_;
}

std::vector<uint8_t> Address::serialize() const {
    return std::vector<uint8_t>(data_.begin(), data_.end());
}

std::string Address::to_hex() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (const auto& byte : data_) {
        ss << std::setw(2) << static_cast<int>(byte);
    }
    return ss.str();
}

Address Address::from_hex(const std::string& hex) {
    if (hex.length() != ADDRESS_SIZE * 2) {
        throw std::invalid_argument("Hex string must be exactly 64 characters");
    }

    AddressArray data;
    for (size_t i = 0; i < ADDRESS_SIZE; ++i) {
        std::string byte_str = hex.substr(i * 2, 2);
        data[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    return Address(data);
}

Address Address::zero() {
    return Address();
}

bool Address::is_zero() const {
    for (const auto& byte : data_) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Account Implementation
// ============================================================================

Account::Account() 
    : address(Address::zero()), 
      balance(0), 
      nonce(0), 
      code_hash(crypto::Blake3Hash::zero()) {}

Account::Account(const Address& addr, uint64_t bal, uint64_t n)
    : address(addr), 
      balance(bal), 
      nonce(n), 
      code_hash(crypto::Blake3Hash::zero()) {}

Account::Account(const Address& addr, uint64_t bal, uint64_t n, const crypto::Blake3Hash& code)
    : address(addr), 
      balance(bal), 
      nonce(n), 
      code_hash(code) {}

std::vector<uint8_t> Account::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(Address::ADDRESS_SIZE + 8 + 8 + crypto::Blake3Hash::HASH_SIZE);

    // Serialize address (32 bytes)
    auto addr_bytes = address.serialize();
    result.insert(result.end(), addr_bytes.begin(), addr_bytes.end());

    // Serialize balance (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((balance >> (i * 8)) & 0xFF));
    }

    // Serialize nonce (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((nonce >> (i * 8)) & 0xFF));
    }

    // Serialize code_hash (32 bytes)
    auto code_bytes = code_hash.serialize();
    result.insert(result.end(), code_bytes.begin(), code_bytes.end());

    return result;
}

Account Account::deserialize(const std::vector<uint8_t>& data) {
    const size_t expected_size = Address::ADDRESS_SIZE + 8 + 8 + crypto::Blake3Hash::HASH_SIZE;
    if (data.size() != expected_size) {
        throw std::invalid_argument("Invalid account data size");
    }

    size_t offset = 0;

    // Deserialize address (32 bytes)
    std::vector<uint8_t> addr_bytes(data.begin(), data.begin() + Address::ADDRESS_SIZE);
    Address addr(addr_bytes);
    offset += Address::ADDRESS_SIZE;

    // Deserialize balance (8 bytes, little-endian)
    uint64_t balance = 0;
    for (int i = 0; i < 8; ++i) {
        balance |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;

    // Deserialize nonce (8 bytes, little-endian)
    uint64_t nonce = 0;
    for (int i = 0; i < 8; ++i) {
        nonce |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;

    // Deserialize code_hash (32 bytes)
    std::vector<uint8_t> code_bytes(data.begin() + offset, data.end());
    crypto::Blake3Hash code_hash(code_bytes);

    return Account(addr, balance, nonce, code_hash);
}

crypto::Blake3Hash Account::hash() const {
    auto serialized = serialize();
    return crypto::Blake3Hash::hash(serialized);
}

bool Account::operator==(const Account& other) const {
    return address == other.address &&
           balance == other.balance &&
           nonce == other.nonce &&
           code_hash == other.code_hash;
}

bool Account::operator!=(const Account& other) const {
    return !(*this == other);
}

} // namespace state
} // namespace sarafu
