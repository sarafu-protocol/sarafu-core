#include <gtest/gtest.h>
#include "sarafu/consensus/validator.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/ed25519.h"

using namespace sarafu::consensus;
using namespace sarafu::crypto;
using namespace sarafu::state;

class ValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate test keys
        auto bls_keypair = BLS12_381::generate_keypair();
        auto ed_keypair = Ed25519::generate_keypair();
        
        bls_pubkey = bls_keypair.first;
        ed_pubkey = ed_keypair.first;
        
        // Create test validator ID
        validator_id = Address::from_public_key(ed_pubkey.serialize());
    }
    
    BLS12_381_PublicKey bls_pubkey;
    Ed25519_PublicKey ed_pubkey;
    ValidatorID validator_id;
};

TEST_F(ValidatorTest, DefaultConstructor) {
    Validator v;
    
    EXPECT_EQ(v.id, Address::zero());
    EXPECT_EQ(v.bonded_stake, 0);
    EXPECT_EQ(v.status, ValidatorStatus::Standby);
    EXPECT_EQ(v.jailed_until_epoch, 0);
    EXPECT_EQ(v.consecutive_downtime_epochs, 0);
    EXPECT_EQ(v.blocks_signed_this_epoch, 0);
    EXPECT_EQ(v.blocks_missed_this_epoch, 0);
}

TEST_F(ValidatorTest, BasicConstructor) {
    Validator v(validator_id, bls_pubkey, ed_pubkey, 1000000);
    
    EXPECT_EQ(v.id, validator_id);
    EXPECT_EQ(v.consensus_key, bls_pubkey);
    EXPECT_EQ(v.withdrawal_key, ed_pubkey);
    EXPECT_EQ(v.bonded_stake, 1000000);
    EXPECT_EQ(v.status, ValidatorStatus::Standby);
    EXPECT_EQ(v.jailed_until_epoch, 0);
    EXPECT_EQ(v.consecutive_downtime_epochs, 0);
    EXPECT_EQ(v.blocks_signed_this_epoch, 0);
    EXPECT_EQ(v.blocks_missed_this_epoch, 0);
}

TEST_F(ValidatorTest, FullConstructor) {
    Validator v(
        validator_id,
        bls_pubkey,
        ed_pubkey,
        1000000,
        ValidatorStatus::Active,
        5,
        2,
        9500,
        500
    );
    
    EXPECT_EQ(v.id, validator_id);
    EXPECT_EQ(v.consensus_key, bls_pubkey);
    EXPECT_EQ(v.withdrawal_key, ed_pubkey);
    EXPECT_EQ(v.bonded_stake, 1000000);
    EXPECT_EQ(v.status, ValidatorStatus::Active);
    EXPECT_EQ(v.jailed_until_epoch, 5);
    EXPECT_EQ(v.consecutive_downtime_epochs, 2);
    EXPECT_EQ(v.blocks_signed_this_epoch, 9500);
    EXPECT_EQ(v.blocks_missed_this_epoch, 500);
}

TEST_F(ValidatorTest, Serialization) {
    Validator v1(
        validator_id,
        bls_pubkey,
        ed_pubkey,
        1000000,
        ValidatorStatus::Active,
        5,
        2,
        9500,
        500
    );
    
    // Serialize
    auto serialized = v1.serialize();
    
    // Expected size: 32 + 48 + 32 + 8 + 1 + 8 + 8 + 8 + 8 = 153 bytes
    EXPECT_EQ(serialized.size(), 153);
    
    // Deserialize
    Validator v2 = Validator::deserialize(serialized);
    
    // Verify all fields match
    EXPECT_EQ(v1, v2);
}

TEST_F(ValidatorTest, Hash) {
    Validator v(validator_id, bls_pubkey, ed_pubkey, 1000000);
    
    auto hash1 = v.hash();
    auto hash2 = v.hash();
    
    // Hash should be deterministic
    EXPECT_EQ(hash1, hash2);
    
    // Hash should be non-zero
    EXPECT_NE(hash1, Blake3Hash::zero());
}

TEST_F(ValidatorTest, Comparison) {
    Validator v1(validator_id, bls_pubkey, ed_pubkey, 1000000);
    Validator v2(validator_id, bls_pubkey, ed_pubkey, 1000000);
    
    EXPECT_EQ(v1, v2);
    EXPECT_FALSE(v1 != v2);
}

TEST_F(ValidatorTest, StakeOrdering) {
    // Create validators with different stakes
    auto id1 = Address::from_public_key(std::vector<uint8_t>(32, 1));
    auto id2 = Address::from_public_key(std::vector<uint8_t>(32, 2));
    auto id3 = Address::from_public_key(std::vector<uint8_t>(32, 3));
    
    Validator v1(id1, bls_pubkey, ed_pubkey, 1000000);  // High stake
    Validator v2(id2, bls_pubkey, ed_pubkey, 500000);   // Low stake
    Validator v3(id3, bls_pubkey, ed_pubkey, 1000000);  // Same stake as v1
    
    // Higher stake should come first (v1 < v2 means v1 ranks higher)
    EXPECT_TRUE(v1 < v2);
    EXPECT_FALSE(v2 < v1);
    
    // Equal stakes should use lexicographic ordering by ID
    if (id1 < id3) {
        EXPECT_TRUE(v1 < v3);
    } else {
        EXPECT_TRUE(v3 < v1);
    }
}

// ============================================================================
// ValidatorSet Tests
// ============================================================================

class ValidatorSetTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test validators
        for (int i = 0; i < 5; ++i) {
            auto bls_keypair = BLS12_381::generate_keypair();
            auto ed_keypair = Ed25519::generate_keypair();
            auto id = Address::from_public_key(ed_keypair.first.serialize());
            
            ValidatorStatus status = (i < 3) ? ValidatorStatus::Active : ValidatorStatus::Standby;
            uint64_t stake = 1000000 - (i * 100000);  // Decreasing stakes
            
            Validator v(id, bls_keypair.first, ed_keypair.first, stake, status, 0, 0, 0, 0);
            validators.push_back(v);
        }
        
        total_stake = 1000000 + 900000 + 800000;  // Sum of active validators
    }
    
    std::vector<Validator> validators;
    uint64_t total_stake;
};

TEST_F(ValidatorSetTest, DefaultConstructor) {
    ValidatorSet vs;
    
    EXPECT_EQ(vs.epoch, 0);
    EXPECT_EQ(vs.validators.size(), 0);
    EXPECT_EQ(vs.total_stake, 0);
    EXPECT_EQ(vs.merkle_root, Blake3Hash::zero());
}

TEST_F(ValidatorSetTest, BasicConstructor) {
    ValidatorSet vs(1, validators, total_stake);
    
    EXPECT_EQ(vs.epoch, 1);
    EXPECT_EQ(vs.validators.size(), 5);
    EXPECT_EQ(vs.total_stake, total_stake);
    EXPECT_EQ(vs.merkle_root, Blake3Hash::zero());
}

TEST_F(ValidatorSetTest, FullConstructor) {
    auto root = Blake3Hash::hash("test_root");
    ValidatorSet vs(1, validators, total_stake, root);
    
    EXPECT_EQ(vs.epoch, 1);
    EXPECT_EQ(vs.validators.size(), 5);
    EXPECT_EQ(vs.total_stake, total_stake);
    EXPECT_EQ(vs.merkle_root, root);
}

TEST_F(ValidatorSetTest, GetActiveValidators) {
    ValidatorSet vs(1, validators, total_stake);
    
    auto active = vs.get_active_validators();
    
    EXPECT_EQ(active.size(), 3);
    for (const auto& v : active) {
        EXPECT_EQ(v.status, ValidatorStatus::Active);
    }
}

TEST_F(ValidatorSetTest, GetStandbyValidators) {
    ValidatorSet vs(1, validators, total_stake);
    
    auto standby = vs.get_standby_validators();
    
    EXPECT_EQ(standby.size(), 2);
    for (const auto& v : standby) {
        EXPECT_EQ(v.status, ValidatorStatus::Standby);
    }
}

TEST_F(ValidatorSetTest, FindValidator) {
    ValidatorSet vs(1, validators, total_stake);
    
    // Find existing validator
    const Validator* found = vs.find_validator(validators[0].id);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(*found, validators[0]);
    
    // Find non-existing validator
    auto fake_id = Address::from_public_key(std::vector<uint8_t>(32, 99));
    const Validator* not_found = vs.find_validator(fake_id);
    EXPECT_EQ(not_found, nullptr);
}

TEST_F(ValidatorSetTest, IsActive) {
    ValidatorSet vs(1, validators, total_stake);
    
    // Check active validators
    EXPECT_TRUE(vs.is_active(validators[0].id));
    EXPECT_TRUE(vs.is_active(validators[1].id));
    EXPECT_TRUE(vs.is_active(validators[2].id));
    
    // Check standby validators
    EXPECT_FALSE(vs.is_active(validators[3].id));
    EXPECT_FALSE(vs.is_active(validators[4].id));
    
    // Check non-existing validator
    auto fake_id = Address::from_public_key(std::vector<uint8_t>(32, 99));
    EXPECT_FALSE(vs.is_active(fake_id));
}

TEST_F(ValidatorSetTest, Serialization) {
    auto root = Blake3Hash::hash("test_root");
    ValidatorSet vs1(1, validators, total_stake, root);
    
    // Serialize
    auto serialized = vs1.serialize();
    
    // Expected size: 8 (epoch) + 8 (total_stake) + 32 (merkle_root) + 4 (num_validators) + 5*153 (validators)
    EXPECT_EQ(serialized.size(), 52 + 5 * 153);
    
    // Deserialize
    ValidatorSet vs2 = ValidatorSet::deserialize(serialized);
    
    // Verify all fields match
    EXPECT_EQ(vs1, vs2);
}

TEST_F(ValidatorSetTest, Comparison) {
    ValidatorSet vs1(1, validators, total_stake);
    ValidatorSet vs2(1, validators, total_stake);
    
    EXPECT_EQ(vs1, vs2);
    EXPECT_FALSE(vs1 != vs2);
}

TEST_F(ValidatorSetTest, EmptyValidatorSet) {
    ValidatorSet vs(0, std::vector<Validator>(), 0);
    
    EXPECT_EQ(vs.validators.size(), 0);
    EXPECT_EQ(vs.get_active_validators().size(), 0);
    EXPECT_EQ(vs.get_standby_validators().size(), 0);
    
    // Serialization should work with empty set
    auto serialized = vs.serialize();
    ValidatorSet vs2 = ValidatorSet::deserialize(serialized);
    EXPECT_EQ(vs, vs2);
}
