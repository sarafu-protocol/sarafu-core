#pragma once

#include <cstdint>
#include <vector>
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/state/account.h"
#include "sarafu/state/transaction.h"

namespace sarafu {
namespace consensus {

/**
 * ValidatorID represents a unique identifier for a validator.
 * 
 * This is typically derived from the validator's address or public key.
 */
using ValidatorID = state::Address;

/**
 * QuorumCertificate represents a collection of signatures from ≥2/3 of total stake
 * confirming a block.
 * 
 * A QC proves that a block has been accepted by a supermajority of validators,
 * making it finalized and irreversible.
 */
struct QuorumCertificate {
    uint64_t block_height;
    crypto::Blake3Hash block_hash;
    uint64_t view_number;
    crypto::BLS12_381_Signature aggregated_signature;  // 96 bytes
    std::vector<ValidatorID> signers;
    uint64_t total_stake_signed;

    // Constructors
    QuorumCertificate();
    QuorumCertificate(
        uint64_t height,
        const crypto::Blake3Hash& hash,
        uint64_t view,
        const crypto::BLS12_381_Signature& sig,
        const std::vector<ValidatorID>& signer_list,
        uint64_t stake
    );

    // Serialization
    std::vector<uint8_t> serialize() const;
    static QuorumCertificate deserialize(const std::vector<uint8_t>& data);

    // Comparison
    bool operator==(const QuorumCertificate& other) const;
    bool operator!=(const QuorumCertificate& other) const;
};

/**
 * BlockHeader contains the metadata for a block.
 * 
 * The header includes:
 * - height: Block number in the chain (genesis = 0)
 * - timestamp: Unix timestamp in seconds
 * - previous_hash: Hash of the parent block
 * - state_root: Merkle root of all account states after applying this block
 * - transactions_root: Merkle root of all transactions in this block
 * - validator_set_root: Merkle root of the current validator set
 * - proposer: Address of the validator who proposed this block
 * - epoch: The epoch number (changes every 10,000 blocks)
 */
struct BlockHeader {
    uint64_t height;
    uint64_t timestamp;
    crypto::Blake3Hash previous_hash;
    crypto::Blake3Hash state_root;
    crypto::Blake3Hash transactions_root;
    crypto::Blake3Hash validator_set_root;
    ValidatorID proposer;
    uint64_t epoch;

    // Constructors
    BlockHeader();
    BlockHeader(
        uint64_t h,
        uint64_t ts,
        const crypto::Blake3Hash& prev_hash,
        const crypto::Blake3Hash& state_rt,
        const crypto::Blake3Hash& tx_rt,
        const crypto::Blake3Hash& val_rt,
        const ValidatorID& prop,
        uint64_t ep
    );

    /**
     * Compute the block header hash.
     * 
     * This hash uniquely identifies the block and is used in:
     * - Block validation
     * - Quorum Certificate creation
     * - Chain linking (previous_hash)
     * 
     * @return Blake3 hash of the block header
     */
    crypto::Blake3Hash hash() const;

    /**
     * Serialize the block header for hashing and storage.
     * 
     * @return Serialized header bytes
     */
    std::vector<uint8_t> serialize() const;

    /**
     * Deserialize a block header from bytes.
     * 
     * @param data The serialized header data
     * @return The deserialized block header
     * @throws std::invalid_argument if data is invalid
     */
    static BlockHeader deserialize(const std::vector<uint8_t>& data);

    // Comparison operators
    bool operator==(const BlockHeader& other) const;
    bool operator!=(const BlockHeader& other) const;
};

/**
 * Block represents a complete block in the blockchain.
 * 
 * A block consists of:
 * - header: Block metadata and commitments
 * - transactions: List of transactions included in this block
 * - justify: Quorum Certificate for the parent block (proves parent is finalized)
 * 
 * The justify QC links this block to the finalized chain and proves that
 * the parent block was accepted by ≥2/3 of validators.
 */
struct Block {
    BlockHeader header;
    std::vector<state::Transaction> transactions;
    QuorumCertificate justify;  // QC for parent block

    // Constructors
    Block();
    Block(
        const BlockHeader& hdr,
        const std::vector<state::Transaction>& txs,
        const QuorumCertificate& qc
    );

    /**
     * Compute the block hash (same as header hash).
     * 
     * @return Blake3 hash of the block header
     */
    crypto::Blake3Hash hash() const;

    /**
     * Serialize the complete block including header, transactions, and QC.
     * 
     * @return Serialized block bytes
     */
    std::vector<uint8_t> serialize() const;

    /**
     * Deserialize a block from bytes.
     * 
     * @param data The serialized block data
     * @return The deserialized block
     * @throws std::invalid_argument if data is invalid
     */
    static Block deserialize(const std::vector<uint8_t>& data);

    // Comparison operators
    bool operator==(const Block& other) const;
    bool operator!=(const Block& other) const;
};

} // namespace consensus
} // namespace sarafu
