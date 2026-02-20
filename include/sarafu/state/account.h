#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "sarafu/crypto/blake3_hash.h"

namespace sarafu {
namespace state {

/**
 * Address represents a 32-byte account identifier.
 * 
 * Addresses are derived from Ed25519 public keys by hashing them with Blake3.
 * This provides a compact, fixed-size identifier for accounts while maintaining
 * cryptographic security.
 */
class Address {
public:
    static constexpr size_t ADDRESS_SIZE = 32;
    using AddressArray = std::array<uint8_t, ADDRESS_SIZE>;

    // Constructors
    Address();
    explicit Address(const AddressArray& data);
    explicit Address(const std::vector<uint8_t>& data);

    // Create address from Ed25519 public key (by hashing it)
    static Address from_public_key(const std::vector<uint8_t>& public_key);

    // Accessors
    const AddressArray& data() const { return data_; }
    const uint8_t* bytes() const { return data_.data(); }
    size_t size() const { return ADDRESS_SIZE; }

    // Comparison operators
    bool operator==(const Address& other) const;
    bool operator!=(const Address& other) const;
    bool operator<(const Address& other) const;

    // Serialization
    std::vector<uint8_t> serialize() const;
    std::string to_hex() const;
    static Address from_hex(const std::string& hex);

    // Zero address (all zeros)
    static Address zero();
    bool is_zero() const;

private:
    AddressArray data_;
};

/**
 * Account represents the state of a single account in the blockchain.
 * 
 * Each account has:
 * - address: The 32-byte account identifier
 * - balance: The account's token balance in the smallest unit
 * - nonce: A strictly increasing counter for replay protection
 * - code_hash: Hash of smart contract code (for future use, currently zero)
 */
struct Account {
    Address address;
    uint64_t balance;
    uint64_t nonce;
    crypto::Blake3Hash code_hash;

    // Constructors
    Account();
    Account(const Address& addr, uint64_t bal, uint64_t n);
    Account(const Address& addr, uint64_t bal, uint64_t n, const crypto::Blake3Hash& code);

    // Serialization for Merkle tree and storage
    std::vector<uint8_t> serialize() const;
    static Account deserialize(const std::vector<uint8_t>& data);

    // Hash the account state (for Merkle tree)
    crypto::Blake3Hash hash() const;

    // Comparison
    bool operator==(const Account& other) const;
    bool operator!=(const Account& other) const;
};

} // namespace state
} // namespace sarafu
