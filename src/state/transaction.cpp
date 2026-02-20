#include "sarafu/state/transaction.h"
#include <stdexcept>
#include <cstring>

namespace sarafu {
namespace state {

// ============================================================================
// Transaction Implementation
// ============================================================================

Transaction::Transaction()
    : from(Address::zero()),
      to(Address::zero()),
      amount(0),
      nonce(0),
      fee(0),
      gas_limit(0),
      chain_id(0),
      signature() {}

Transaction::Transaction(
    const Address& from_addr,
    const Address& to_addr,
    uint64_t amt,
    uint64_t n,
    uint64_t f,
    uint64_t gas,
    uint32_t chain
) : from(from_addr),
    to(to_addr),
    amount(amt),
    nonce(n),
    fee(f),
    gas_limit(gas),
    chain_id(chain),
    signature() {}

std::vector<uint8_t> Transaction::serialize_for_signing() const {
    std::vector<uint8_t> result;
    
    // Calculate total size:
    // from (32) + to (32) + amount (8) + nonce (8) + fee (8) + gas_limit (8) + chain_id (4)
    result.reserve(32 + 32 + 8 + 8 + 8 + 8 + 4);

    // Serialize from address (32 bytes)
    auto from_bytes = from.serialize();
    result.insert(result.end(), from_bytes.begin(), from_bytes.end());

    // Serialize to address (32 bytes)
    auto to_bytes = to.serialize();
    result.insert(result.end(), to_bytes.begin(), to_bytes.end());

    // Serialize amount (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((amount >> (i * 8)) & 0xFF));
    }

    // Serialize nonce (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((nonce >> (i * 8)) & 0xFF));
    }

    // Serialize fee (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((fee >> (i * 8)) & 0xFF));
    }

    // Serialize gas_limit (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((gas_limit >> (i * 8)) & 0xFF));
    }

    // Serialize chain_id (4 bytes, little-endian)
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<uint8_t>((chain_id >> (i * 8)) & 0xFF));
    }

    return result;
}

crypto::Blake3Hash Transaction::hash() const {
    auto serialized = serialize_for_signing();
    return crypto::Blake3Hash::hash(serialized);
}

std::vector<uint8_t> Transaction::serialize() const {
    std::vector<uint8_t> result;
    
    // Calculate total size:
    // from (32) + to (32) + amount (8) + nonce (8) + fee (8) + gas_limit (8) + chain_id (4) + signature (64)
    result.reserve(32 + 32 + 8 + 8 + 8 + 8 + 4 + 64);

    // Serialize from address (32 bytes)
    auto from_bytes = from.serialize();
    result.insert(result.end(), from_bytes.begin(), from_bytes.end());

    // Serialize to address (32 bytes)
    auto to_bytes = to.serialize();
    result.insert(result.end(), to_bytes.begin(), to_bytes.end());

    // Serialize amount (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((amount >> (i * 8)) & 0xFF));
    }

    // Serialize nonce (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((nonce >> (i * 8)) & 0xFF));
    }

    // Serialize fee (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((fee >> (i * 8)) & 0xFF));
    }

    // Serialize gas_limit (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((gas_limit >> (i * 8)) & 0xFF));
    }

    // Serialize chain_id (4 bytes, little-endian)
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<uint8_t>((chain_id >> (i * 8)) & 0xFF));
    }

    // Serialize signature (64 bytes)
    auto sig_bytes = signature.serialize();
    result.insert(result.end(), sig_bytes.begin(), sig_bytes.end());

    return result;
}

Transaction Transaction::deserialize(const std::vector<uint8_t>& data) {
    // Expected size: 32 + 32 + 8 + 8 + 8 + 8 + 4 + 64 = 164 bytes
    const size_t expected_size = 164;
    if (data.size() != expected_size) {
        throw std::invalid_argument("Invalid transaction data size");
    }

    size_t offset = 0;

    // Deserialize from address (32 bytes)
    std::vector<uint8_t> from_bytes(data.begin(), data.begin() + 32);
    Address from_addr(from_bytes);
    offset += 32;

    // Deserialize to address (32 bytes)
    std::vector<uint8_t> to_bytes(data.begin() + offset, data.begin() + offset + 32);
    Address to_addr(to_bytes);
    offset += 32;

    // Deserialize amount (8 bytes, little-endian)
    uint64_t amount = 0;
    for (int i = 0; i < 8; ++i) {
        amount |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;

    // Deserialize nonce (8 bytes, little-endian)
    uint64_t nonce = 0;
    for (int i = 0; i < 8; ++i) {
        nonce |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;

    // Deserialize fee (8 bytes, little-endian)
    uint64_t fee = 0;
    for (int i = 0; i < 8; ++i) {
        fee |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;

    // Deserialize gas_limit (8 bytes, little-endian)
    uint64_t gas_limit = 0;
    for (int i = 0; i < 8; ++i) {
        gas_limit |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;

    // Deserialize chain_id (4 bytes, little-endian)
    uint32_t chain_id = 0;
    for (int i = 0; i < 4; ++i) {
        chain_id |= static_cast<uint32_t>(data[offset + i]) << (i * 8);
    }
    offset += 4;

    // Deserialize signature (64 bytes)
    std::vector<uint8_t> sig_bytes(data.begin() + offset, data.end());
    crypto::Ed25519_Signature sig(sig_bytes);

    // Create transaction and set signature
    Transaction tx(from_addr, to_addr, amount, nonce, fee, gas_limit, chain_id);
    tx.signature = sig;

    return tx;
}

void Transaction::sign(const crypto::Ed25519_PrivateKey& private_key) {
    auto tx_hash = hash();
    signature = crypto::Ed25519::sign(tx_hash.serialize(), private_key);
}

bool Transaction::verify_signature(const crypto::Ed25519_PublicKey& public_key) const {
    auto tx_hash = hash();
    return crypto::Ed25519::verify(signature, tx_hash.serialize(), public_key);
}

bool Transaction::operator==(const Transaction& other) const {
    return from == other.from &&
           to == other.to &&
           amount == other.amount &&
           nonce == other.nonce &&
           fee == other.fee &&
           gas_limit == other.gas_limit &&
           chain_id == other.chain_id &&
           signature == other.signature;
}

bool Transaction::operator!=(const Transaction& other) const {
    return !(*this == other);
}

// ============================================================================
// TransactionReceipt Implementation
// ============================================================================

TransactionReceipt::TransactionReceipt()
    : tx_hash(crypto::Blake3Hash::zero()),
      block_height(0),
      success(false),
      gas_used(0),
      error_message("") {}

TransactionReceipt::TransactionReceipt(
    const crypto::Blake3Hash& hash,
    uint64_t height,
    bool succeeded,
    uint64_t gas,
    const std::string& error
) : tx_hash(hash),
    block_height(height),
    success(succeeded),
    gas_used(gas),
    error_message(error) {}

std::vector<uint8_t> TransactionReceipt::serialize() const {
    std::vector<uint8_t> result;
    
    // Serialize tx_hash (32 bytes)
    auto hash_bytes = tx_hash.serialize();
    result.insert(result.end(), hash_bytes.begin(), hash_bytes.end());

    // Serialize block_height (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((block_height >> (i * 8)) & 0xFF));
    }

    // Serialize success (1 byte)
    result.push_back(success ? 1 : 0);

    // Serialize gas_used (8 bytes, little-endian)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>((gas_used >> (i * 8)) & 0xFF));
    }

    // Serialize error_message length (4 bytes, little-endian)
    uint32_t error_len = static_cast<uint32_t>(error_message.size());
    for (int i = 0; i < 4; ++i) {
        result.push_back(static_cast<uint8_t>((error_len >> (i * 8)) & 0xFF));
    }

    // Serialize error_message
    result.insert(result.end(), error_message.begin(), error_message.end());

    return result;
}

TransactionReceipt TransactionReceipt::deserialize(const std::vector<uint8_t>& data) {
    // Minimum size: 32 + 8 + 1 + 8 + 4 = 53 bytes
    if (data.size() < 53) {
        throw std::invalid_argument("Invalid transaction receipt data size");
    }

    size_t offset = 0;

    // Deserialize tx_hash (32 bytes)
    std::vector<uint8_t> hash_bytes(data.begin(), data.begin() + 32);
    crypto::Blake3Hash hash(hash_bytes);
    offset += 32;

    // Deserialize block_height (8 bytes, little-endian)
    uint64_t height = 0;
    for (int i = 0; i < 8; ++i) {
        height |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;

    // Deserialize success (1 byte)
    bool succeeded = (data[offset] != 0);
    offset += 1;

    // Deserialize gas_used (8 bytes, little-endian)
    uint64_t gas = 0;
    for (int i = 0; i < 8; ++i) {
        gas |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;

    // Deserialize error_message length (4 bytes, little-endian)
    uint32_t error_len = 0;
    for (int i = 0; i < 4; ++i) {
        error_len |= static_cast<uint32_t>(data[offset + i]) << (i * 8);
    }
    offset += 4;

    // Deserialize error_message
    if (offset + error_len != data.size()) {
        throw std::invalid_argument("Invalid error message length in receipt");
    }
    std::string error(data.begin() + offset, data.end());

    return TransactionReceipt(hash, height, succeeded, gas, error);
}

bool TransactionReceipt::operator==(const TransactionReceipt& other) const {
    return tx_hash == other.tx_hash &&
           block_height == other.block_height &&
           success == other.success &&
           gas_used == other.gas_used &&
           error_message == other.error_message;
}

bool TransactionReceipt::operator!=(const TransactionReceipt& other) const {
    return !(*this == other);
}

} // namespace state
} // namespace sarafu
