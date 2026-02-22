#include "sarafu/state/paper_token.h"
#include "sarafu/crypto/ed25519.h"
#include <cstring>
#include <stdexcept>

namespace sarafu {
namespace state {

// ============================================================================
// PaperToken Implementation
// ============================================================================

PaperToken::PaperToken()
    : token_id(crypto::Blake3Hash::zero()),
      amount(0),
      hash_lock(crypto::Blake3Hash::zero()),
      creator_address(Address::zero()),
      creation_height(0),
      refund_height(0),
      consumed(false) {}

PaperToken::PaperToken(
    const crypto::Blake3Hash& id,
    uint64_t amt,
    const crypto::Blake3Hash& lock,
    const Address& creator,
    uint64_t creation_h,
    uint64_t refund_h
) : token_id(id),
    amount(amt),
    hash_lock(lock),
    creator_address(creator),
    creation_height(creation_h),
    refund_height(refund_h),
    consumed(false) {}

crypto::Blake3Hash PaperToken::compute_token_id(
    const crypto::Blake3Hash& hash_lock,
    const Address& creator,
    uint64_t creation_height
) {
    std::vector<uint8_t> data;
    data.reserve(32 + 32 + 8);
    
    // Append hash_lock
    auto hash_bytes = hash_lock.serialize();
    data.insert(data.end(), hash_bytes.begin(), hash_bytes.end());
    
    // Append creator_address
    auto addr_bytes = creator.serialize();
    data.insert(data.end(), addr_bytes.begin(), addr_bytes.end());
    
    // Append creation_height (big-endian)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((creation_height >> (i * 8)) & 0xFF));
    }
    
    return crypto::Blake3Hash::hash(data);
}

bool PaperToken::can_refund(uint64_t current_height) const {
    return !consumed && current_height >= refund_height;
}

bool PaperToken::is_redeemable(uint64_t current_height) const {
    return !consumed && current_height < refund_height;
}

std::vector<uint8_t> PaperToken::serialize() const {
    std::vector<uint8_t> data;
    data.reserve(32 + 8 + 32 + 32 + 8 + 8 + 1);
    
    // token_id (32 bytes)
    auto id_bytes = token_id.serialize();
    data.insert(data.end(), id_bytes.begin(), id_bytes.end());
    
    // amount (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((amount >> (i * 8)) & 0xFF));
    }
    
    // hash_lock (32 bytes)
    auto lock_bytes = hash_lock.serialize();
    data.insert(data.end(), lock_bytes.begin(), lock_bytes.end());
    
    // creator_address (32 bytes)
    auto addr_bytes = creator_address.serialize();
    data.insert(data.end(), addr_bytes.begin(), addr_bytes.end());
    
    // creation_height (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((creation_height >> (i * 8)) & 0xFF));
    }
    
    // refund_height (8 bytes, big-endian)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((refund_height >> (i * 8)) & 0xFF));
    }
    
    // consumed (1 byte)
    data.push_back(consumed ? 1 : 0);
    
    return data;
}

PaperToken PaperToken::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() != 32 + 8 + 32 + 32 + 8 + 8 + 1) {
        throw std::invalid_argument("PaperToken::deserialize: invalid data size");
    }
    
    size_t offset = 0;
    
    // token_id
    std::vector<uint8_t> id_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash token_id(id_bytes);
    offset += 32;
    
    // amount
    uint64_t amount = 0;
    for (int i = 0; i < 8; ++i) {
        amount = (amount << 8) | data[offset + i];
    }
    offset += 8;
    
    // hash_lock
    std::vector<uint8_t> lock_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash hash_lock(lock_bytes);
    offset += 32;
    
    // creator_address
    std::vector<uint8_t> addr_bytes(data.begin() + offset, data.begin() + offset + 32);
    Address creator_address(addr_bytes);
    offset += 32;
    
    // creation_height
    uint64_t creation_height = 0;
    for (int i = 0; i < 8; ++i) {
        creation_height = (creation_height << 8) | data[offset + i];
    }
    offset += 8;
    
    // refund_height
    uint64_t refund_height = 0;
    for (int i = 0; i < 8; ++i) {
        refund_height = (refund_height << 8) | data[offset + i];
    }
    offset += 8;
    
    // consumed
    bool consumed = data[offset] != 0;
    
    PaperToken token(token_id, amount, hash_lock, creator_address, creation_height, refund_height);
    token.consumed = consumed;
    return token;
}

crypto::Blake3Hash PaperToken::hash() const {
    return crypto::Blake3Hash::hash(serialize());
}

bool PaperToken::operator==(const PaperToken& other) const {
    return token_id == other.token_id &&
           amount == other.amount &&
           hash_lock == other.hash_lock &&
           creator_address == other.creator_address &&
           creation_height == other.creation_height &&
           refund_height == other.refund_height &&
           consumed == other.consumed;
}

bool PaperToken::operator!=(const PaperToken& other) const {
    return !(*this == other);
}

// ============================================================================
// CreatePaperTokenTx Implementation
// ============================================================================

CreatePaperTokenTx::CreatePaperTokenTx()
    : from(Address::zero()),
      amount(0),
      hash_lock(crypto::Blake3Hash::zero()),
      refund_delay_days(0),
      nonce(0),
      fee(0),
      gas_limit(0),
      chain_id(0),
      signature() {}

CreatePaperTokenTx::CreatePaperTokenTx(
    const Address& from_addr,
    uint64_t amt,
    const crypto::Blake3Hash& lock,
    uint32_t refund_delay,
    uint64_t n,
    uint64_t f,
    uint64_t gas,
    uint32_t chain
) : from(from_addr),
    amount(amt),
    hash_lock(lock),
    refund_delay_days(refund_delay),
    nonce(n),
    fee(f),
    gas_limit(gas),
    chain_id(chain),
    signature() {}

crypto::Blake3Hash CreatePaperTokenTx::hash() const {
    return crypto::Blake3Hash::hash(serialize_for_signing());
}

std::vector<uint8_t> CreatePaperTokenTx::serialize_for_signing() const {
    std::vector<uint8_t> data;
    data.reserve(32 + 8 + 32 + 4 + 8 + 8 + 8 + 4);
    
    // from (32 bytes)
    auto from_bytes = from.serialize();
    data.insert(data.end(), from_bytes.begin(), from_bytes.end());
    
    // amount (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((amount >> (i * 8)) & 0xFF));
    }
    
    // hash_lock (32 bytes)
    auto lock_bytes = hash_lock.serialize();
    data.insert(data.end(), lock_bytes.begin(), lock_bytes.end());
    
    // refund_delay_days (4 bytes)
    for (int i = 3; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((refund_delay_days >> (i * 8)) & 0xFF));
    }
    
    // nonce (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((nonce >> (i * 8)) & 0xFF));
    }
    
    // fee (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((fee >> (i * 8)) & 0xFF));
    }
    
    // gas_limit (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((gas_limit >> (i * 8)) & 0xFF));
    }
    
    // chain_id (4 bytes)
    for (int i = 3; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((chain_id >> (i * 8)) & 0xFF));
    }
    
    return data;
}

std::vector<uint8_t> CreatePaperTokenTx::serialize() const {
    auto data = serialize_for_signing();
    
    // Append signature (64 bytes)
    auto sig_bytes = signature.serialize();
    data.insert(data.end(), sig_bytes.begin(), sig_bytes.end());
    
    return data;
}

CreatePaperTokenTx CreatePaperTokenTx::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() != 32 + 8 + 32 + 4 + 8 + 8 + 8 + 4 + 64) {
        throw std::invalid_argument("CreatePaperTokenTx::deserialize: invalid data size");
    }
    
    size_t offset = 0;
    
    // from
    std::vector<uint8_t> from_bytes(data.begin() + offset, data.begin() + offset + 32);
    Address from(from_bytes);
    offset += 32;
    
    // amount
    uint64_t amount = 0;
    for (int i = 0; i < 8; ++i) {
        amount = (amount << 8) | data[offset + i];
    }
    offset += 8;
    
    // hash_lock
    std::vector<uint8_t> lock_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash hash_lock(lock_bytes);
    offset += 32;
    
    // refund_delay_days
    uint32_t refund_delay_days = 0;
    for (int i = 0; i < 4; ++i) {
        refund_delay_days = (refund_delay_days << 8) | data[offset + i];
    }
    offset += 4;
    
    // nonce
    uint64_t nonce = 0;
    for (int i = 0; i < 8; ++i) {
        nonce = (nonce << 8) | data[offset + i];
    }
    offset += 8;
    
    // fee
    uint64_t fee = 0;
    for (int i = 0; i < 8; ++i) {
        fee = (fee << 8) | data[offset + i];
    }
    offset += 8;
    
    // gas_limit
    uint64_t gas_limit = 0;
    for (int i = 0; i < 8; ++i) {
        gas_limit = (gas_limit << 8) | data[offset + i];
    }
    offset += 8;
    
    // chain_id
    uint32_t chain_id = 0;
    for (int i = 0; i < 4; ++i) {
        chain_id = (chain_id << 8) | data[offset + i];
    }
    offset += 4;
    
    // signature
    std::vector<uint8_t> sig_bytes(data.begin() + offset, data.begin() + offset + 64);
    crypto::Ed25519_Signature signature(sig_bytes);
    
    CreatePaperTokenTx tx(from, amount, hash_lock, refund_delay_days, nonce, fee, gas_limit, chain_id);
    tx.signature = signature;
    return tx;
}

void CreatePaperTokenTx::sign(const crypto::Ed25519_PrivateKey& private_key) {
    auto tx_hash = hash();
    signature = crypto::Ed25519::sign(tx_hash.serialize(), private_key);
}

bool CreatePaperTokenTx::verify_signature(const crypto::Ed25519_PublicKey& public_key) const {
    auto tx_hash = hash();
    return crypto::Ed25519::verify(signature, tx_hash.serialize(), public_key);
}

bool CreatePaperTokenTx::operator==(const CreatePaperTokenTx& other) const {
    return from == other.from &&
           amount == other.amount &&
           hash_lock == other.hash_lock &&
           refund_delay_days == other.refund_delay_days &&
           nonce == other.nonce &&
           fee == other.fee &&
           gas_limit == other.gas_limit &&
           chain_id == other.chain_id &&
           signature == other.signature;
}

bool CreatePaperTokenTx::operator!=(const CreatePaperTokenTx& other) const {
    return !(*this == other);
}

// ============================================================================
// RedeemPaperTokenTx Implementation
// ============================================================================

RedeemPaperTokenTx::RedeemPaperTokenTx()
    : token_id(crypto::Blake3Hash::zero()),
      secret(),
      destination(Address::zero()),
      redeemer(Address::zero()),
      nonce(0),
      fee(0),
      gas_limit(0),
      chain_id(0),
      signature() {}

RedeemPaperTokenTx::RedeemPaperTokenTx(
    const crypto::Blake3Hash& id,
    const std::vector<uint8_t>& sec,
    const Address& dest,
    const Address& redeemer_addr,
    uint64_t n,
    uint64_t f,
    uint64_t gas,
    uint32_t chain
) : token_id(id),
    secret(sec),
    destination(dest),
    redeemer(redeemer_addr),
    nonce(n),
    fee(f),
    gas_limit(gas),
    chain_id(chain),
    signature() {}

crypto::Blake3Hash RedeemPaperTokenTx::compute_signature_message() const {
    std::vector<uint8_t> data;
    data.reserve(32 + 32 + 8);
    
    // hash(secret)
    auto secret_hash = crypto::Blake3Hash::hash(secret);
    auto hash_bytes = secret_hash.serialize();
    data.insert(data.end(), hash_bytes.begin(), hash_bytes.end());
    
    // destination
    auto dest_bytes = destination.serialize();
    data.insert(data.end(), dest_bytes.begin(), dest_bytes.end());
    
    // nonce
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((nonce >> (i * 8)) & 0xFF));
    }
    
    return crypto::Blake3Hash::hash(data);
}

crypto::Blake3Hash RedeemPaperTokenTx::hash() const {
    return crypto::Blake3Hash::hash(serialize_for_signing());
}

std::vector<uint8_t> RedeemPaperTokenTx::serialize_for_signing() const {
    std::vector<uint8_t> data;
    
    // token_id (32 bytes)
    auto id_bytes = token_id.serialize();
    data.insert(data.end(), id_bytes.begin(), id_bytes.end());
    
    // secret (variable length, prefixed with 4-byte length)
    uint32_t secret_len = static_cast<uint32_t>(secret.size());
    for (int i = 3; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((secret_len >> (i * 8)) & 0xFF));
    }
    data.insert(data.end(), secret.begin(), secret.end());
    
    // destination (32 bytes)
    auto dest_bytes = destination.serialize();
    data.insert(data.end(), dest_bytes.begin(), dest_bytes.end());
    
    // redeemer (32 bytes)
    auto redeemer_bytes = redeemer.serialize();
    data.insert(data.end(), redeemer_bytes.begin(), redeemer_bytes.end());
    
    // nonce (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((nonce >> (i * 8)) & 0xFF));
    }
    
    // fee (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((fee >> (i * 8)) & 0xFF));
    }
    
    // gas_limit (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((gas_limit >> (i * 8)) & 0xFF));
    }
    
    // chain_id (4 bytes)
    for (int i = 3; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((chain_id >> (i * 8)) & 0xFF));
    }
    
    return data;
}

std::vector<uint8_t> RedeemPaperTokenTx::serialize() const {
    auto data = serialize_for_signing();
    
    // Append signature (64 bytes)
    auto sig_bytes = signature.serialize();
    data.insert(data.end(), sig_bytes.begin(), sig_bytes.end());
    
    return data;
}

RedeemPaperTokenTx RedeemPaperTokenTx::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 32 + 4 + 32 + 32 + 8 + 8 + 8 + 4 + 64) {
        throw std::invalid_argument("RedeemPaperTokenTx::deserialize: data too small");
    }
    
    size_t offset = 0;
    
    // token_id
    std::vector<uint8_t> id_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash token_id(id_bytes);
    offset += 32;
    
    // secret length
    uint32_t secret_len = 0;
    for (int i = 0; i < 4; ++i) {
        secret_len = (secret_len << 8) | data[offset + i];
    }
    offset += 4;
    
    // secret
    if (offset + secret_len > data.size()) {
        throw std::invalid_argument("RedeemPaperTokenTx::deserialize: invalid secret length");
    }
    std::vector<uint8_t> secret(data.begin() + offset, data.begin() + offset + secret_len);
    offset += secret_len;
    
    // destination
    std::vector<uint8_t> dest_bytes(data.begin() + offset, data.begin() + offset + 32);
    Address destination(dest_bytes);
    offset += 32;
    
    // redeemer
    std::vector<uint8_t> redeemer_bytes(data.begin() + offset, data.begin() + offset + 32);
    Address redeemer(redeemer_bytes);
    offset += 32;
    
    // nonce
    uint64_t nonce = 0;
    for (int i = 0; i < 8; ++i) {
        nonce = (nonce << 8) | data[offset + i];
    }
    offset += 8;
    
    // fee
    uint64_t fee = 0;
    for (int i = 0; i < 8; ++i) {
        fee = (fee << 8) | data[offset + i];
    }
    offset += 8;
    
    // gas_limit
    uint64_t gas_limit = 0;
    for (int i = 0; i < 8; ++i) {
        gas_limit = (gas_limit << 8) | data[offset + i];
    }
    offset += 8;
    
    // chain_id
    uint32_t chain_id = 0;
    for (int i = 0; i < 4; ++i) {
        chain_id = (chain_id << 8) | data[offset + i];
    }
    offset += 4;
    
    // signature
    std::vector<uint8_t> sig_bytes(data.begin() + offset, data.begin() + offset + 64);
    crypto::Ed25519_Signature signature(sig_bytes);
    
    RedeemPaperTokenTx tx(token_id, secret, destination, redeemer, nonce, fee, gas_limit, chain_id);
    tx.signature = signature;
    return tx;
}

void RedeemPaperTokenTx::sign(const crypto::Ed25519_PrivateKey& private_key) {
    auto msg = compute_signature_message();
    signature = crypto::Ed25519::sign(msg.serialize(), private_key);
}

bool RedeemPaperTokenTx::verify_signature(const crypto::Ed25519_PublicKey& public_key) const {
    auto msg = compute_signature_message();
    return crypto::Ed25519::verify(signature, msg.serialize(), public_key);
}

bool RedeemPaperTokenTx::verify_secret(const crypto::Blake3Hash& hash_lock) const {
    auto secret_hash = crypto::Blake3Hash::hash(secret);
    return secret_hash == hash_lock;
}

bool RedeemPaperTokenTx::operator==(const RedeemPaperTokenTx& other) const {
    return token_id == other.token_id &&
           secret == other.secret &&
           destination == other.destination &&
           redeemer == other.redeemer &&
           nonce == other.nonce &&
           fee == other.fee &&
           gas_limit == other.gas_limit &&
           chain_id == other.chain_id &&
           signature == other.signature;
}

bool RedeemPaperTokenTx::operator!=(const RedeemPaperTokenTx& other) const {
    return !(*this == other);
}

// ============================================================================
// RefundPaperTokenTx Implementation
// ============================================================================

RefundPaperTokenTx::RefundPaperTokenTx()
    : token_id(crypto::Blake3Hash::zero()),
      destination(Address::zero()),
      from(Address::zero()),
      nonce(0),
      fee(0),
      gas_limit(0),
      chain_id(0),
      signature() {}

RefundPaperTokenTx::RefundPaperTokenTx(
    const crypto::Blake3Hash& id,
    const Address& dest,
    const Address& from_addr,
    uint64_t n,
    uint64_t f,
    uint64_t gas,
    uint32_t chain
) : token_id(id),
    destination(dest),
    from(from_addr),
    nonce(n),
    fee(f),
    gas_limit(gas),
    chain_id(chain),
    signature() {}

crypto::Blake3Hash RefundPaperTokenTx::hash() const {
    return crypto::Blake3Hash::hash(serialize_for_signing());
}

std::vector<uint8_t> RefundPaperTokenTx::serialize_for_signing() const {
    std::vector<uint8_t> data;
    data.reserve(32 + 32 + 32 + 8 + 8 + 8 + 4);
    
    // token_id (32 bytes)
    auto id_bytes = token_id.serialize();
    data.insert(data.end(), id_bytes.begin(), id_bytes.end());
    
    // destination (32 bytes)
    auto dest_bytes = destination.serialize();
    data.insert(data.end(), dest_bytes.begin(), dest_bytes.end());
    
    // from (32 bytes)
    auto from_bytes = from.serialize();
    data.insert(data.end(), from_bytes.begin(), from_bytes.end());
    
    // nonce (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((nonce >> (i * 8)) & 0xFF));
    }
    
    // fee (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((fee >> (i * 8)) & 0xFF));
    }
    
    // gas_limit (8 bytes)
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((gas_limit >> (i * 8)) & 0xFF));
    }
    
    // chain_id (4 bytes)
    for (int i = 3; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((chain_id >> (i * 8)) & 0xFF));
    }
    
    return data;
}

std::vector<uint8_t> RefundPaperTokenTx::serialize() const {
    auto data = serialize_for_signing();
    
    // Append signature (64 bytes)
    auto sig_bytes = signature.serialize();
    data.insert(data.end(), sig_bytes.begin(), sig_bytes.end());
    
    return data;
}

RefundPaperTokenTx RefundPaperTokenTx::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() != 32 + 32 + 32 + 8 + 8 + 8 + 4 + 64) {
        throw std::invalid_argument("RefundPaperTokenTx::deserialize: invalid data size");
    }
    
    size_t offset = 0;
    
    // token_id
    std::vector<uint8_t> id_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash token_id(id_bytes);
    offset += 32;
    
    // destination
    std::vector<uint8_t> dest_bytes(data.begin() + offset, data.begin() + offset + 32);
    Address destination(dest_bytes);
    offset += 32;
    
    // from
    std::vector<uint8_t> from_bytes(data.begin() + offset, data.begin() + offset + 32);
    Address from(from_bytes);
    offset += 32;
    
    // nonce
    uint64_t nonce = 0;
    for (int i = 0; i < 8; ++i) {
        nonce = (nonce << 8) | data[offset + i];
    }
    offset += 8;
    
    // fee
    uint64_t fee = 0;
    for (int i = 0; i < 8; ++i) {
        fee = (fee << 8) | data[offset + i];
    }
    offset += 8;
    
    // gas_limit
    uint64_t gas_limit = 0;
    for (int i = 0; i < 8; ++i) {
        gas_limit = (gas_limit << 8) | data[offset + i];
    }
    offset += 8;
    
    // chain_id
    uint32_t chain_id = 0;
    for (int i = 0; i < 4; ++i) {
        chain_id = (chain_id << 8) | data[offset + i];
    }
    offset += 4;
    
    // signature
    std::vector<uint8_t> sig_bytes(data.begin() + offset, data.begin() + offset + 64);
    crypto::Ed25519_Signature signature(sig_bytes);
    
    RefundPaperTokenTx tx(token_id, destination, from, nonce, fee, gas_limit, chain_id);
    tx.signature = signature;
    return tx;
}

void RefundPaperTokenTx::sign(const crypto::Ed25519_PrivateKey& private_key) {
    auto tx_hash = hash();
    signature = crypto::Ed25519::sign(tx_hash.serialize(), private_key);
}

bool RefundPaperTokenTx::verify_signature(const crypto::Ed25519_PublicKey& public_key) const {
    auto tx_hash = hash();
    return crypto::Ed25519::verify(signature, tx_hash.serialize(), public_key);
}

bool RefundPaperTokenTx::operator==(const RefundPaperTokenTx& other) const {
    return token_id == other.token_id &&
           destination == other.destination &&
           from == other.from &&
           nonce == other.nonce &&
           fee == other.fee &&
           gas_limit == other.gas_limit &&
           chain_id == other.chain_id &&
           signature == other.signature;
}

bool RefundPaperTokenTx::operator!=(const RefundPaperTokenTx& other) const {
    return !(*this == other);
}

} // namespace state
} // namespace sarafu
