#include "sarafu/consensus/qc_verifier.h"
#include <algorithm>

namespace sarafu {
namespace consensus {

bool QCVerifier::verify_qc(
    const QuorumCertificate& qc,
    const ValidatorSet& validator_set
) {
    // Check supermajority (≥2/3 stake)
    if (!check_supermajority(qc, validator_set)) {
        return false;
    }

    // Verify all signers are in the validator set
    if (!verify_signers_in_set(qc, validator_set)) {
        return false;
    }

    // Verify aggregated signature
    if (!verify_aggregated_signature(qc, validator_set)) {
        return false;
    }

    return true;
}

bool QCVerifier::check_supermajority(
    const QuorumCertificate& qc,
    const ValidatorSet& validator_set
) {
    // Calculate required stake (ceiling of 2/3)
    uint64_t required_stake = (validator_set.total_stake * 2 + 2) / 3;

    // Check if QC has enough stake
    return qc.total_stake_signed >= required_stake;
}

bool QCVerifier::verify_aggregated_signature(
    const QuorumCertificate& qc,
    const ValidatorSet& validator_set
) {
    // Collect public keys of all signers
    std::vector<crypto::BLS12_381_PublicKey> signer_public_keys;
    signer_public_keys.reserve(qc.signers.size());

    for (const auto& signer_id : qc.signers) {
        const Validator* validator = find_validator(signer_id, validator_set);
        if (validator == nullptr) {
            return false;  // Signer not found in validator set
        }
        signer_public_keys.push_back(validator->consensus_key);
    }

    // Verify aggregated signature against block hash
    std::vector<uint8_t> message = qc.block_hash.serialize();
    return crypto::BLS12_381::verify_aggregated(
        qc.aggregated_signature,
        message,
        signer_public_keys
    );
}

bool QCVerifier::verify_signers_in_set(
    const QuorumCertificate& qc,
    const ValidatorSet& validator_set
) {
    // Check that all signers are in the validator set
    for (const auto& signer_id : qc.signers) {
        const Validator* validator = find_validator(signer_id, validator_set);
        if (validator == nullptr) {
            return false;
        }
    }
    return true;
}

uint64_t QCVerifier::calculate_signer_stake(
    const QuorumCertificate& qc,
    const ValidatorSet& validator_set
) {
    uint64_t total_stake = 0;

    for (const auto& signer_id : qc.signers) {
        const Validator* validator = find_validator(signer_id, validator_set);
        if (validator == nullptr) {
            return 0;  // Signer not found
        }
        total_stake += validator->bonded_stake;
    }

    return total_stake;
}

const Validator* QCVerifier::find_validator(
    const ValidatorID& validator_id,
    const ValidatorSet& validator_set
) {
    for (const auto& validator : validator_set.validators) {
        if (validator.id == validator_id) {
            return &validator;
        }
    }
    return nullptr;
}

} // namespace consensus
} // namespace sarafu
