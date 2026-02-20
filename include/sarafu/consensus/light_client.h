#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include "sarafu/consensus/block.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/bls12_381.h"

namespace sarafu {
namespace consensus {

/**
 * LightClientState represents the minimal state needed by a light client
 * to verify finalized block headers.
 * 
 * The light client tracks:
 * - current_epoch: The current epoch number
 * - validator_set_root: Merkle root of the current epoch's validator set
 * - finalized_height: The highest finalized block height
 * - finalized_block_hash: Hash of the highest finalized block
 * 
 * This state is updated as the light client verifies new headers and
 * epoch transitions.
 * 
 * Requirements: 5.5, 13.3
 */
struct LightClientState {
    uint64_t current_epoch;
    crypto::Blake3Hash validator_set_root;
    uint64_t finalized_height;
    crypto::Blake3Hash finalized_block_hash;

    // Constructors
    LightClientState();
    LightClientState(
        uint64_t epoch,
        const crypto::Blake3Hash& val_set_root,
        uint64_t height,
        const crypto::Blake3Hash& block_hash
    );

    // Serialization
    std::vector<uint8_t> serialize() const;
    static LightClientState deserialize(const std::vector<uint8_t>& data);

    // Comparison
    bool operator==(const LightClientState& other) const;
    bool operator!=(const LightClientState& other) const;
};

/**
 * HeaderProof contains all the data needed to verify a finalized block header.
 * 
 * The proof includes:
 * - header: The block header being verified
 * - qc: Quorum Certificate proving ≥2/3 stake signed the block
 * - validator_set_merkle_proof: Merkle proof showing signing validators are in the validator set
 * - signing_validators: List of validators who signed (with their stakes)
 * 
 * The light client uses this proof to verify:
 * 1. The header is from the correct epoch
 * 2. The signing validators are in the validator set (via Merkle proof)
 * 3. The signing validators represent ≥2/3 of total stake
 * 4. The aggregated BLS signature is valid
 * 
 * Target proof size: ≤5 KB for N≤500 validators
 * 
 * Requirements: 5.1, 5.2, 5.3, 5.4, 5.7, 5.8
 */
struct HeaderProof {
    BlockHeader header;
    QuorumCertificate qc;
    std::vector<crypto::Blake3Hash> validator_set_merkle_proof;
    std::vector<Validator> signing_validators;
    uint64_t total_validator_set_stake;  // Total stake in the validator set (for supermajority check)

    // Constructors
    HeaderProof();
    HeaderProof(
        const BlockHeader& hdr,
        const QuorumCertificate& quorum_cert,
        const std::vector<crypto::Blake3Hash>& merkle_proof,
        const std::vector<Validator>& signers,
        uint64_t total_stake
    );

    // Serialization
    std::vector<uint8_t> serialize() const;
    static HeaderProof deserialize(const std::vector<uint8_t>& data);

    /**
     * Calculate the size of this proof in bytes.
     * Used to verify the proof size requirement (≤5 KB).
     */
    size_t size_bytes() const;

    // Comparison
    bool operator==(const HeaderProof& other) const;
    bool operator!=(const HeaderProof& other) const;
};

/**
 * LightClient verifies finalized block headers with minimal bandwidth.
 * 
 * The light client uses:
 * - BLS12-381 aggregated signatures (96 bytes)
 * - Binary Merkle proofs for validator set membership
 * - Target proof size: ≤5 KB for validator sets up to N=500
 * 
 * Verification algorithm:
 * 1. Verify proof.header.epoch == state.current_epoch
 * 2. Verify proof.header.height > state.finalized_height
 * 3. Verify proof.header.previous_hash extends finalized chain
 * 4. Verify validator_set_merkle_proof against state.validator_set_root
 * 5. Calculate total_stake_signed from proof.signing_validators
 * 6. Verify total_stake_signed >= (2/3) * total_stake
 * 7. Verify BLS aggregated signature
 * 
 * Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8
 */
class LightClient {
public:
    // Constructors
    LightClient();

    /**
     * Initialize the light client with a trusted checkpoint.
     * 
     * The checkpoint must be obtained from a trusted source (e.g., block explorer,
     * validator website, community resources) and must be less than 1,000,000 blocks old.
     * 
     * @param checkpoint The trusted light client state to start from
     * @return true if initialization succeeded, false otherwise
     * 
     * Requirements: 5.5, 13.3
     */
    bool initialize(const LightClientState& checkpoint);

    /**
     * Verify a header proof and update the light client state.
     * 
     * This method performs all verification steps:
     * 1. Epoch consistency check
     * 2. Height progression check
     * 3. Chain extension check
     * 4. Validator set Merkle proof verification
     * 5. Supermajority stake check (≥2/3)
     * 6. BLS aggregated signature verification
     * 
     * If all checks pass, the light client state is updated to the new finalized height.
     * 
     * @param proof The header proof to verify
     * @return true if the proof is valid and state was updated, false otherwise
     * 
     * Requirements: 5.1, 5.2, 5.3, 5.4, 5.7
     */
    bool verify_header(const HeaderProof& proof);

    /**
     * Update to a new epoch with a transition Quorum Certificate.
     * 
     * The transition QC must be signed by ≥2/3 of the previous epoch's validators
     * to authorize the new validator set.
     * 
     * @param new_epoch The new epoch number (must be current_epoch + 1)
     * @param new_validator_set_root Merkle root of the new validator set
     * @param transition_qc QC signed by ≥2/3 of previous epoch validators
     * @return true if the epoch update succeeded, false otherwise
     * 
     * Requirements: 5.6
     */
    bool update_epoch(
        uint64_t new_epoch,
        const crypto::Blake3Hash& new_validator_set_root,
        const QuorumCertificate& transition_qc
    );

    /**
     * Get the current light client state.
     * 
     * @return The current state
     */
    const LightClientState& state() const { return state_; }

    /**
     * Check if the light client has been initialized.
     * 
     * @return true if initialized, false otherwise
     */
    bool is_initialized() const { return initialized_; }

private:
    /**
     * Verify BLS aggregated signature.
     * 
     * @param signature The aggregated signature
     * @param message The message that was signed
     * @param public_keys Vector of public keys that signed
     * @return true if the signature is valid, false otherwise
     */
    bool verify_aggregated_signature(
        const crypto::BLS12_381_Signature& signature,
        const crypto::Blake3Hash& message,
        const std::vector<crypto::BLS12_381_PublicKey>& public_keys
    ) const;

    /**
     * Verify Merkle proof for validator set membership.
     * 
     * @param leaf The leaf hash to verify
     * @param proof The Merkle proof (sibling hashes)
     * @param root The expected root hash
     * @return true if the proof is valid, false otherwise
     */
    bool verify_merkle_proof(
        const crypto::Blake3Hash& leaf,
        const std::vector<crypto::Blake3Hash>& proof,
        const crypto::Blake3Hash& root
    ) const;

    /**
     * Check if signing validators represent a supermajority (≥2/3 stake).
     * 
     * @param signers Vector of signing validators
     * @param total_stake Total stake in the validator set
     * @return true if signers have ≥2/3 stake, false otherwise
     */
    bool has_supermajority(
        const std::vector<Validator>& signers,
        uint64_t total_stake
    ) const;

    /**
     * Calculate total stake from a list of validators.
     * 
     * @param validators Vector of validators
     * @return Sum of bonded_stake from all validators
     */
    uint64_t calculate_total_stake(const std::vector<Validator>& validators) const;

    LightClientState state_;
    bool initialized_;
};

} // namespace consensus
} // namespace sarafu
