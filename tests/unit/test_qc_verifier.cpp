#include "sarafu/consensus/qc_verifier.h"
#include "sarafu/consensus/vote_aggregator.h"
#include "sarafu/crypto/bls12_381.h"
#include <gtest/gtest.h>

using namespace sarafu::consensus;
using namespace sarafu::crypto;
using namespace sarafu::state;

class QCVerifierTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a simple validator set with 3 validators
        validators_.clear();
        validator_keys_.clear();

        for (int i = 0; i < 3; ++i) {
            auto [bls_public_key, bls_private_key] = BLS12_381::generate_keypair();
            auto [ed_public_key, ed_private_key] = Ed25519::generate_keypair();

            std::vector<uint8_t> id_bytes(32, static_cast<uint8_t>(i));
            ValidatorID id(id_bytes);

            Validator validator(id, bls_public_key, ed_public_key, 1000000);
            validator.status = ValidatorStatus::Active;

            validators_.push_back(validator);
            validator_keys_[id] = bls_private_key;
        }

        validator_set_ = ValidatorSet(0, validators_, 3000000);
    }

    Vote create_vote(
        const ValidatorID& validator_id,
        uint64_t block_height,
        const Blake3Hash& block_hash,
        uint64_t view_number
    ) {
        auto it = validator_keys_.find(validator_id);
        if (it == validator_keys_.end()) {
            throw std::runtime_error("Validator private key not found");
        }

        std::vector<uint8_t> message = block_hash.serialize();
        BLS12_381_Signature signature = BLS12_381::sign(message, it->second);

        return Vote(validator_id, block_height, block_hash, view_number, signature);
    }

    std::vector<Validator> validators_;
    std::map<ValidatorID, BLS12_381_PrivateKey> validator_keys_;
    ValidatorSet validator_set_;
};

TEST_F(QCVerifierTest, VerifyValidQC) {
    VoteAggregator aggregator(validator_set_);

    uint64_t block_height = 100;
    std::vector<uint8_t> hash_data(32, 0xAB);
    Blake3Hash block_hash(hash_data);
    uint64_t view_number = 1;

    // Add votes from all 3 validators (100% stake)
    for (const auto& validator : validators_) {
        Vote vote = create_vote(validator.id, block_height, block_hash, view_number);
        aggregator.add_vote(vote);
    }

    // Create QC
    auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
    ASSERT_TRUE(qc_opt.has_value());

    QuorumCertificate qc = qc_opt.value();

    // Verify QC
    EXPECT_TRUE(QCVerifier::verify_qc(qc, validator_set_));
}

TEST_F(QCVerifierTest, CheckSupermajority) {
    VoteAggregator aggregator(validator_set_);

    uint64_t block_height = 100;
    std::vector<uint8_t> hash_data(32, 0xAB);
    Blake3Hash block_hash(hash_data);
    uint64_t view_number = 1;

    // Add votes from 2 out of 3 validators (66.67% stake, which is ≥2/3)
    for (size_t i = 0; i < 2; ++i) {
        Vote vote = create_vote(validators_[i].id, block_height, block_hash, view_number);
        aggregator.add_vote(vote);
    }

    // Create QC
    auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
    ASSERT_TRUE(qc_opt.has_value());

    QuorumCertificate qc = qc_opt.value();

    // Check supermajority
    EXPECT_TRUE(QCVerifier::check_supermajority(qc, validator_set_));
}

TEST_F(QCVerifierTest, RejectQCWithInsufficientStake) {
    // Create a QC manually with insufficient stake
    uint64_t block_height = 100;
    std::vector<uint8_t> hash_data(32, 0xAB);
    Blake3Hash block_hash(hash_data);
    uint64_t view_number = 1;

    // Create a QC with only 1 validator (33.33% stake, which is <2/3)
    Vote vote = create_vote(validators_[0].id, block_height, block_hash, view_number);

    QuorumCertificate qc(
        block_height,
        block_hash,
        view_number,
        vote.signature,
        {validators_[0].id},
        1000000  // Only 1/3 of total stake
    );

    // Should fail supermajority check
    EXPECT_FALSE(QCVerifier::check_supermajority(qc, validator_set_));
}

TEST_F(QCVerifierTest, VerifySignersInSet) {
    VoteAggregator aggregator(validator_set_);

    uint64_t block_height = 100;
    std::vector<uint8_t> hash_data(32, 0xAB);
    Blake3Hash block_hash(hash_data);
    uint64_t view_number = 1;

    // Add votes from all validators
    for (const auto& validator : validators_) {
        Vote vote = create_vote(validator.id, block_height, block_hash, view_number);
        aggregator.add_vote(vote);
    }

    // Create QC
    auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
    ASSERT_TRUE(qc_opt.has_value());

    QuorumCertificate qc = qc_opt.value();

    // Verify all signers are in the set
    EXPECT_TRUE(QCVerifier::verify_signers_in_set(qc, validator_set_));
}

TEST_F(QCVerifierTest, RejectQCWithUnknownSigner) {
    VoteAggregator aggregator(validator_set_);

    uint64_t block_height = 100;
    std::vector<uint8_t> hash_data(32, 0xAB);
    Blake3Hash block_hash(hash_data);
    uint64_t view_number = 1;

    // Add votes from all validators
    for (const auto& validator : validators_) {
        Vote vote = create_vote(validator.id, block_height, block_hash, view_number);
        aggregator.add_vote(vote);
    }

    // Create QC
    auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
    ASSERT_TRUE(qc_opt.has_value());

    QuorumCertificate qc = qc_opt.value();

    // Add an unknown signer
    std::vector<uint8_t> unknown_id_bytes(32, 0xFF);
    ValidatorID unknown_id(unknown_id_bytes);
    qc.signers.push_back(unknown_id);

    // Should fail signer verification
    EXPECT_FALSE(QCVerifier::verify_signers_in_set(qc, validator_set_));
}

TEST_F(QCVerifierTest, CalculateSignerStake) {
    VoteAggregator aggregator(validator_set_);

    uint64_t block_height = 100;
    std::vector<uint8_t> hash_data(32, 0xAB);
    Blake3Hash block_hash(hash_data);
    uint64_t view_number = 1;

    // Add votes from 2 validators
    for (size_t i = 0; i < 2; ++i) {
        Vote vote = create_vote(validators_[i].id, block_height, block_hash, view_number);
        aggregator.add_vote(vote);
    }

    // Create QC
    auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
    ASSERT_TRUE(qc_opt.has_value());

    QuorumCertificate qc = qc_opt.value();

    // Calculate stake
    uint64_t stake = QCVerifier::calculate_signer_stake(qc, validator_set_);

    // Should be 2 * 1000000 = 2000000
    EXPECT_EQ(stake, 2000000);
}

TEST_F(QCVerifierTest, VerifyAggregatedSignature) {
    VoteAggregator aggregator(validator_set_);

    uint64_t block_height = 100;
    std::vector<uint8_t> hash_data(32, 0xAB);
    Blake3Hash block_hash(hash_data);
    uint64_t view_number = 1;

    // Add votes from all validators
    for (const auto& validator : validators_) {
        Vote vote = create_vote(validator.id, block_height, block_hash, view_number);
        aggregator.add_vote(vote);
    }

    // Create QC
    auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
    ASSERT_TRUE(qc_opt.has_value());

    QuorumCertificate qc = qc_opt.value();

    // Verify aggregated signature
    EXPECT_TRUE(QCVerifier::verify_aggregated_signature(qc, validator_set_));
}

TEST_F(QCVerifierTest, RejectQCWithInvalidSignature) {
    VoteAggregator aggregator(validator_set_);

    uint64_t block_height = 100;
    std::vector<uint8_t> hash_data(32, 0xAB);
    Blake3Hash block_hash(hash_data);
    uint64_t view_number = 1;

    // Add votes from all validators
    for (const auto& validator : validators_) {
        Vote vote = create_vote(validator.id, block_height, block_hash, view_number);
        aggregator.add_vote(vote);
    }

    // Create QC
    auto qc_opt = aggregator.aggregate_votes(block_height, block_hash, view_number);
    ASSERT_TRUE(qc_opt.has_value());

    QuorumCertificate qc = qc_opt.value();

    // Corrupt the signature by flipping a bit
    auto sig_data = qc.aggregated_signature.serialize();
    sig_data[0] ^= 0x01;
    qc.aggregated_signature = BLS12_381_Signature(sig_data);

    // Should fail signature verification
    EXPECT_FALSE(QCVerifier::verify_aggregated_signature(qc, validator_set_));
}
