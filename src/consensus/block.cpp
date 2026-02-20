#include "sarafu/consensus/block.h"
#include <cstring>
#include <stdexcept>

namespace sarafu {
namespace consensus {

// ============================================================================
// QuorumCertificate Implementation
// ============================================================================

QuorumCertificate::QuorumCertificate()
    : block_height(0)
    , block_hash(crypto::Blake3Hash::zero())
    , view_number(0)
    , aggregated_signature()
    , signers()
    , total_stake_signed(0)
{
}

QuorumCertificate::QuorumCertificate(
    uint64_t height,
    const crypto::Blake3Hash& hash,
    uint64_t view,
    const crypto::BLS12_381_Signature& sig,
    const std::vector<ValidatorID>& signer_list,
    uint64_t stake
)
    : block_height(height)
    , block_hash(hash)
    , view_number(view)
    , aggregated_signature(sig)
    , signers(signer_list)
    , total_stake_signed(stake)
{
}

std::vector<uint8_t> QuorumCertificate::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(8 + 32 + 8 + 96 + 8 + signers.size() * 32 + 8);

    // Serialize block_height (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(block_height >> (i * 8)));
    }

    // Serialize block_hash (32 bytes)
    auto hash_bytes = block_hash.serialize();
    result.insert(result.end(), hash_bytes.begin(), hash_bytes.end());

    // Serialize view_number (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(view_number >> (i * 8)));
    }

    // Serialize aggregated_signature (96 bytes)
    auto sig_bytes = aggregated_signature.serialize();
    result.insert(result.end(), sig_bytes.begin(), sig_bytes.end());

    // Serialize signers count (8 bytes)
    uint64_t signers_count = signers.size();
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(signers_count >> (i * 8)));
    }

    // Serialize each signer (32 bytes each)
    for (const auto& signer : signers) {
        auto signer_bytes = signer.serialize();
        result.insert(result.end(), signer_bytes.begin(), signer_bytes.end());
    }

    // Serialize total_stake_signed (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(total_stake_signed >> (i * 8)));
    }

    return result;
}

QuorumCertificate QuorumCertificate::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8 + 32 + 8 + 96 + 8 + 8) {
        throw std::invalid_argument("QuorumCertificate data too short");
    }

    size_t offset = 0;

    // Deserialize block_height
    uint64_t height = 0;
    for (int i = 0; i < 8; ++i) {
        height |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    // Deserialize block_hash
    std::vector<uint8_t> hash_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash hash(hash_bytes);
    offset += 32;

    // Deserialize view_number
    uint64_t view = 0;
    for (int i = 0; i < 8; ++i) {
        view |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    // Deserialize aggregated_signature
    std::vector<uint8_t> sig_bytes(data.begin() + offset, data.begin() + offset + 96);
    crypto::BLS12_381_Signature sig(sig_bytes);
    offset += 96;

    // Deserialize signers count
    uint64_t signers_count = 0;
    for (int i = 0; i < 8; ++i) {
        signers_count |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    // Deserialize signers
    std::vector<ValidatorID> signer_list;
    signer_list.reserve(signers_count);
    for (uint64_t i = 0; i < signers_count; ++i) {
        if (offset + 32 > data.size()) {
            throw std::invalid_argument("QuorumCertificate data truncated");
        }
        std::vector<uint8_t> signer_bytes(data.begin() + offset, data.begin() + offset + 32);
        signer_list.emplace_back(signer_bytes);
        offset += 32;
    }

    // Deserialize total_stake_signed
    uint64_t stake = 0;
    for (int i = 0; i < 8; ++i) {
        if (offset >= data.size()) {
            throw std::invalid_argument("QuorumCertificate data truncated");
        }
        stake |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    return QuorumCertificate(height, hash, view, sig, signer_list, stake);
}

bool QuorumCertificate::operator==(const QuorumCertificate& other) const {
    return block_height == other.block_height &&
           block_hash == other.block_hash &&
           view_number == other.view_number &&
           aggregated_signature == other.aggregated_signature &&
           signers == other.signers &&
           total_stake_signed == other.total_stake_signed;
}

bool QuorumCertificate::operator!=(const QuorumCertificate& other) const {
    return !(*this == other);
}

// ============================================================================
// BlockHeader Implementation
// ============================================================================

BlockHeader::BlockHeader()
    : height(0)
    , timestamp(0)
    , previous_hash(crypto::Blake3Hash::zero())
    , state_root(crypto::Blake3Hash::zero())
    , transactions_root(crypto::Blake3Hash::zero())
    , validator_set_root(crypto::Blake3Hash::zero())
    , proposer(state::Address::zero())
    , epoch(0)
{
}

BlockHeader::BlockHeader(
    uint64_t h,
    uint64_t ts,
    const crypto::Blake3Hash& prev_hash,
    const crypto::Blake3Hash& state_rt,
    const crypto::Blake3Hash& tx_rt,
    const crypto::Blake3Hash& val_rt,
    const ValidatorID& prop,
    uint64_t ep
)
    : height(h)
    , timestamp(ts)
    , previous_hash(prev_hash)
    , state_root(state_rt)
    , transactions_root(tx_rt)
    , validator_set_root(val_rt)
    , proposer(prop)
    , epoch(ep)
{
}

crypto::Blake3Hash BlockHeader::hash() const {
    return crypto::Blake3Hash::hash(serialize());
}

std::vector<uint8_t> BlockHeader::serialize() const {
    std::vector<uint8_t> result;
    result.reserve(8 + 8 + 32 + 32 + 32 + 32 + 32 + 8);

    // Serialize height (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(height >> (i * 8)));
    }

    // Serialize timestamp (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(timestamp >> (i * 8)));
    }

    // Serialize previous_hash (32 bytes)
    auto prev_hash_bytes = previous_hash.serialize();
    result.insert(result.end(), prev_hash_bytes.begin(), prev_hash_bytes.end());

    // Serialize state_root (32 bytes)
    auto state_root_bytes = state_root.serialize();
    result.insert(result.end(), state_root_bytes.begin(), state_root_bytes.end());

    // Serialize transactions_root (32 bytes)
    auto tx_root_bytes = transactions_root.serialize();
    result.insert(result.end(), tx_root_bytes.begin(), tx_root_bytes.end());

    // Serialize validator_set_root (32 bytes)
    auto val_root_bytes = validator_set_root.serialize();
    result.insert(result.end(), val_root_bytes.begin(), val_root_bytes.end());

    // Serialize proposer (32 bytes)
    auto proposer_bytes = proposer.serialize();
    result.insert(result.end(), proposer_bytes.begin(), proposer_bytes.end());

    // Serialize epoch (8 bytes)
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(epoch >> (i * 8)));
    }

    return result;
}

BlockHeader BlockHeader::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8 + 8 + 32 + 32 + 32 + 32 + 32 + 8) {
        throw std::invalid_argument("BlockHeader data too short");
    }

    size_t offset = 0;

    // Deserialize height
    uint64_t h = 0;
    for (int i = 0; i < 8; ++i) {
        h |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    // Deserialize timestamp
    uint64_t ts = 0;
    for (int i = 0; i < 8; ++i) {
        ts |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    // Deserialize previous_hash
    std::vector<uint8_t> prev_hash_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash prev_hash(prev_hash_bytes);
    offset += 32;

    // Deserialize state_root
    std::vector<uint8_t> state_root_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash state_rt(state_root_bytes);
    offset += 32;

    // Deserialize transactions_root
    std::vector<uint8_t> tx_root_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash tx_rt(tx_root_bytes);
    offset += 32;

    // Deserialize validator_set_root
    std::vector<uint8_t> val_root_bytes(data.begin() + offset, data.begin() + offset + 32);
    crypto::Blake3Hash val_rt(val_root_bytes);
    offset += 32;

    // Deserialize proposer
    std::vector<uint8_t> proposer_bytes(data.begin() + offset, data.begin() + offset + 32);
    ValidatorID prop(proposer_bytes);
    offset += 32;

    // Deserialize epoch
    uint64_t ep = 0;
    for (int i = 0; i < 8; ++i) {
        ep |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    return BlockHeader(h, ts, prev_hash, state_rt, tx_rt, val_rt, prop, ep);
}

bool BlockHeader::operator==(const BlockHeader& other) const {
    return height == other.height &&
           timestamp == other.timestamp &&
           previous_hash == other.previous_hash &&
           state_root == other.state_root &&
           transactions_root == other.transactions_root &&
           validator_set_root == other.validator_set_root &&
           proposer == other.proposer &&
           epoch == other.epoch;
}

bool BlockHeader::operator!=(const BlockHeader& other) const {
    return !(*this == other);
}

// ============================================================================
// Block Implementation
// ============================================================================

Block::Block()
    : header()
    , transactions()
    , justify()
{
}

Block::Block(
    const BlockHeader& hdr,
    const std::vector<state::Transaction>& txs,
    const QuorumCertificate& qc
)
    : header(hdr)
    , transactions(txs)
    , justify(qc)
{
}

crypto::Blake3Hash Block::hash() const {
    return header.hash();
}

std::vector<uint8_t> Block::serialize() const {
    std::vector<uint8_t> result;

    // Serialize header
    auto header_bytes = header.serialize();
    result.insert(result.end(), header_bytes.begin(), header_bytes.end());

    // Serialize transaction count (8 bytes)
    uint64_t tx_count = transactions.size();
    for (int i = 0; i < 8; ++i) {
        result.push_back(static_cast<uint8_t>(tx_count >> (i * 8)));
    }

    // Serialize each transaction
    for (const auto& tx : transactions) {
        auto tx_bytes = tx.serialize();
        // Prepend transaction size (8 bytes)
        uint64_t tx_size = tx_bytes.size();
        for (int i = 0; i < 8; ++i) {
            result.push_back(static_cast<uint8_t>(tx_size >> (i * 8)));
        }
        result.insert(result.end(), tx_bytes.begin(), tx_bytes.end());
    }

    // Serialize justify QC
    auto qc_bytes = justify.serialize();
    result.insert(result.end(), qc_bytes.begin(), qc_bytes.end());

    return result;
}

Block Block::deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 8 + 8 + 32 + 32 + 32 + 32 + 32 + 8 + 8) {
        throw std::invalid_argument("Block data too short");
    }

    size_t offset = 0;

    // Deserialize header (fixed size: 8 + 8 + 32 + 32 + 32 + 32 + 32 + 8 = 184 bytes)
    std::vector<uint8_t> header_bytes(data.begin(), data.begin() + 184);
    BlockHeader hdr = BlockHeader::deserialize(header_bytes);
    offset += 184;

    // Deserialize transaction count
    uint64_t tx_count = 0;
    for (int i = 0; i < 8; ++i) {
        if (offset >= data.size()) {
            throw std::invalid_argument("Block data truncated");
        }
        tx_count |= static_cast<uint64_t>(data[offset++]) << (i * 8);
    }

    // Deserialize transactions
    std::vector<state::Transaction> txs;
    txs.reserve(tx_count);
    for (uint64_t i = 0; i < tx_count; ++i) {
        // Read transaction size
        if (offset + 8 > data.size()) {
            throw std::invalid_argument("Block data truncated");
        }
        uint64_t tx_size = 0;
        for (int j = 0; j < 8; ++j) {
            tx_size |= static_cast<uint64_t>(data[offset++]) << (j * 8);
        }

        // Read transaction data
        if (offset + tx_size > data.size()) {
            throw std::invalid_argument("Block data truncated");
        }
        std::vector<uint8_t> tx_bytes(data.begin() + offset, data.begin() + offset + tx_size);
        txs.push_back(state::Transaction::deserialize(tx_bytes));
        offset += tx_size;
    }

    // Deserialize justify QC (remaining data)
    std::vector<uint8_t> qc_bytes(data.begin() + offset, data.end());
    QuorumCertificate qc = QuorumCertificate::deserialize(qc_bytes);

    return Block(hdr, txs, qc);
}

bool Block::operator==(const Block& other) const {
    return header == other.header &&
           transactions == other.transactions &&
           justify == other.justify;
}

bool Block::operator!=(const Block& other) const {
    return !(*this == other);
}

} // namespace consensus
} // namespace sarafu
