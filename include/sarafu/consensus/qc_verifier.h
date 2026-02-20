#pragma once

#include "sarafu/consensus/block.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/crypto/bls12_381.h"

namespace sarafu {
namespace consensus {

/**
 * QCVerifier verifies Quorum Certificates.
 * 
 * The verifier checks:
 * - Aggregated signature validity
 * - Supermajority stake requirement (≥2/3)
 * - Signer membership in validator set
 * 
 * Requirements: 1.2, 9.3
 */
class QCVerifier {
public:
    /**
     * Verify a Quorum Certificate against a validator set.
     * 
     * This performs complete verification:
     * 1. Check that signers represent ≥2/3 of total stake
     * 2. Verify all signers are in the validator set
     * 3. Verify the aggregated BLS signature
     * 
     * @param qc The Quorum Certificate to verify
     * @param validator_set The validator set to verify against
     * @return true if QC is valid, false otherwise
     */
    static bool verify_qc(
        const QuorumCertificate& qc,
        const ValidatorSet& validator_set
    );

    /**
     * Check if a QC has supermajority stake (≥2/3).
     * 
     * This verifies that the total_stake_signed in the QC is at least
     * 2/3 of the total stake in the validator set.
     * 
     * @param qc The Quorum Certificate to check
     * @param validator_set The validator set to check against
     * @return true if QC has ≥2/3 stake, false otherwise
     */
    static bool check_supermajority(
        const QuorumCertificate& qc,
        const ValidatorSet& validator_set
    );

    /**
     * Verify the aggregated signature in a QC.
     * 
     * This collects the public keys of all signers from the validator set
     * and verifies the aggregated BLS signature against the block hash.
     * 
     * @param qc The Quorum Certificate to verify
     * @param validator_set The validator set containing signer public keys
     * @return true if signature is valid, false otherwise
     */
    static bool verify_aggregated_signature(
        const QuorumCertificate& qc,
        const ValidatorSet& validator_set
    );

    /**
     * Verify that all signers are in the validator set.
     * 
     * This checks that every signer ID in the QC corresponds to a
     * validator in the validator set.
     * 
     * @param qc The Quorum Certificate to check
     * @param validator_set The validator set to check against
     * @return true if all signers are in the set, false otherwise
     */
    static bool verify_signers_in_set(
        const QuorumCertificate& qc,
        const ValidatorSet& validator_set
    );

    /**
     * Calculate the actual total stake of signers.
     * 
     * This sums up the bonded stake of all signers in the QC
     * by looking them up in the validator set.
     * 
     * @param qc The Quorum Certificate
     * @param validator_set The validator set containing stake information
     * @return Total stake of all signers, or 0 if any signer not found
     */
    static uint64_t calculate_signer_stake(
        const QuorumCertificate& qc,
        const ValidatorSet& validator_set
    );

private:
    /**
     * Find a validator by ID in the validator set.
     * 
     * @param validator_id The validator ID to find
     * @param validator_set The validator set to search
     * @return Pointer to validator if found, nullptr otherwise
     */
    static const Validator* find_validator(
        const ValidatorID& validator_id,
        const ValidatorSet& validator_set
    );
};

} // namespace consensus
} // namespace sarafu
