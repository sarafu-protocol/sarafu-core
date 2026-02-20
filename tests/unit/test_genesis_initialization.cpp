#include "sarafu/consensus/genesis_builder.h"
#include "sarafu/state/state_machine.h"
#include "sarafu/consensus/validator.h"
#include <gtest/gtest.h>

using namespace sarafu::consensus;
using namespace sarafu::state;
using namespace sarafu::crypto;

/**
 * Unit tests for genesis block initialization.
 * 
 * Tests the initialize_from_genesis() method which:
 * - Loads genesis allocations and creates accounts
 * - Initializes the validator set for epoch 0
 * - Sets the initial epoch to 0
 * - Computes and verifies the genesis state root
 * 
 * Requirements: 16.1, 16.6, 16.7
 */
class GenesisInitializationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test genesis configuration
        total_supply_ = 1000000000000ULL;  // 1 trillion tokens
        genesis_timestamp_ = 1700000000;
        validator_set_size_ = 10;
        minimum_self_bond_ = 1000000;
        chain_id_ = 1;
        
        config_ = GenesisConfig(
            total_supply_,
            genesis_timestamp_,
            validator_set_size_,
            minimum_self_bond_,
            chain_id_
        );
    }

    // Helper to create a test address
    Address create_test_address(uint8_t seed) {
        std::vector<uint8_t> data(32, seed);
        return Address(data);
    }

    // Helper to create a test validator
    Validator create_test_validator(uint8_t seed, uint64_t stake) {
        Address id = create_test_address(seed);
        BLS12_381_PublicKey consensus_key;
        Ed25519_PublicKey withdrawal_key;
        
        return Validator(id, consensus_key, withdrawal_key, stake);
    }

    uint64_t total_supply_;
    uint64_t genesis_timestamp_;
    size_t validator_set_size_;
    uint64_t minimum_self_bond_;
    uint32_t chain_id_;
    GenesisConfig config_;
};

/**
 * Test: Genesis block initialization succeeds with valid data
 */
TEST_F(GenesisInitializationTest, InitializationSucceedsWithValidData) {
    // Create genesis builder
    GenesisBuilder builder(config_);
    
    // Add some allocations
    Address addr1 = create_test_address(1);
    Address addr2 = create_test_address(2);
    Address addr3 = create_test_address(3);
    
    ASSERT_TRUE(builder.add_allocation(addr1, 100000000, "Foundation"));
    ASSERT_TRUE(builder.add_allocation(addr2, 200000000, "Validators"));
    ASSERT_TRUE(builder.add_allocation(addr3, 300000000, "Community"));
    
    // Create genesis validators
    std::vector<Validator> validators;
    for (int i = 0; i < 10; ++i) {
        validators.push_back(create_test_validator(100 + i, minimum_self_bond_ * (i + 1)));
    }
    
    ASSERT_TRUE(builder.create_genesis_validator_set(validators));
    
    // Build genesis block
    Block genesis_block = builder.build();
    
    // Verify genesis block properties
    EXPECT_EQ(genesis_block.header.height, 0);
    EXPECT_EQ(genesis_block.header.epoch, 0);
    EXPECT_EQ(genesis_block.header.timestamp, genesis_timestamp_);
    EXPECT_TRUE(genesis_block.transactions.empty());
    
    // Initialize state machine from genesis
    StateMachine state_machine(chain_id_);
    bool result = state_machine.initialize_from_genesis(
        genesis_block,
        builder.get_allocations()
    );
    
    ASSERT_TRUE(result);
    
    // Verify accounts were created
    Account account1 = state_machine.get_account(addr1);
    EXPECT_EQ(account1.balance, 100000000);
    EXPECT_EQ(account1.nonce, 0);
    
    Account account2 = state_machine.get_account(addr2);
    EXPECT_EQ(account2.balance, 200000000);
    EXPECT_EQ(account2.nonce, 0);
    
    Account account3 = state_machine.get_account(addr3);
    EXPECT_EQ(account3.balance, 300000000);
    EXPECT_EQ(account3.nonce, 0);
    
    // Verify initial height is 0
    EXPECT_EQ(state_machine.get_current_height(), 0);
}

/**
 * Test: Genesis block must have height 0
 */
TEST_F(GenesisInitializationTest, RejectsNonZeroHeight) {
    GenesisBuilder builder(config_);
    
    Address addr = create_test_address(1);
    builder.add_allocation(addr, 100000000, "Test");
    
    std::vector<Validator> validators;
    validators.push_back(create_test_validator(100, minimum_self_bond_));
    builder.create_genesis_validator_set(validators);
    
    Block genesis_block = builder.build();
    
    // Manually modify height to non-zero
    genesis_block.header.height = 1;
    
    StateMachine state_machine(chain_id_);
    bool result = state_machine.initialize_from_genesis(
        genesis_block,
        builder.get_allocations()
    );
    
    EXPECT_FALSE(result);
}

/**
 * Test: Genesis block must have epoch 0
 */
TEST_F(GenesisInitializationTest, RejectsNonZeroEpoch) {
    GenesisBuilder builder(config_);
    
    Address addr = create_test_address(1);
    builder.add_allocation(addr, 100000000, "Test");
    
    std::vector<Validator> validators;
    validators.push_back(create_test_validator(100, minimum_self_bond_));
    builder.create_genesis_validator_set(validators);
    
    Block genesis_block = builder.build();
    
    // Manually modify epoch to non-zero
    genesis_block.header.epoch = 1;
    
    StateMachine state_machine(chain_id_);
    bool result = state_machine.initialize_from_genesis(
        genesis_block,
        builder.get_allocations()
    );
    
    EXPECT_FALSE(result);
}

/**
 * Test: State root must match allocations
 */
TEST_F(GenesisInitializationTest, RejectsInvalidStateRoot) {
    GenesisBuilder builder(config_);
    
    Address addr = create_test_address(1);
    builder.add_allocation(addr, 100000000, "Test");
    
    std::vector<Validator> validators;
    validators.push_back(create_test_validator(100, minimum_self_bond_));
    builder.create_genesis_validator_set(validators);
    
    Block genesis_block = builder.build();
    
    // Manually modify state root to invalid value
    genesis_block.header.state_root = Blake3Hash();
    
    StateMachine state_machine(chain_id_);
    bool result = state_machine.initialize_from_genesis(
        genesis_block,
        builder.get_allocations()
    );
    
    EXPECT_FALSE(result);
}

/**
 * Test: Multiple allocations are all created
 */
TEST_F(GenesisInitializationTest, CreatesMultipleAllocations) {
    GenesisBuilder builder(config_);
    
    // Add 20 allocations
    std::vector<Address> addresses;
    for (int i = 0; i < 20; ++i) {
        Address addr = create_test_address(i);
        addresses.push_back(addr);
        uint64_t amount = 1000000 * (i + 1);
        ASSERT_TRUE(builder.add_allocation(addr, amount, "Test"));
    }
    
    std::vector<Validator> validators;
    validators.push_back(create_test_validator(100, minimum_self_bond_));
    builder.create_genesis_validator_set(validators);
    
    Block genesis_block = builder.build();
    
    StateMachine state_machine(chain_id_);
    bool result = state_machine.initialize_from_genesis(
        genesis_block,
        builder.get_allocations()
    );
    
    ASSERT_TRUE(result);
    
    // Verify all accounts were created
    for (int i = 0; i < 20; ++i) {
        Account account = state_machine.get_account(addresses[i]);
        uint64_t expected_balance = 1000000 * (i + 1);
        EXPECT_EQ(account.balance, expected_balance);
        EXPECT_EQ(account.nonce, 0);
    }
}

/**
 * Test: Large allocations with vesting are created
 */
TEST_F(GenesisInitializationTest, CreatesLargeAllocationsWithVesting) {
    GenesisBuilder builder(config_);
    
    // Add allocation above 2% (requires vesting)
    Address addr = create_test_address(1);
    uint64_t large_amount = static_cast<uint64_t>(total_supply_ * 0.03);  // 3%
    
    ASSERT_TRUE(builder.add_allocation(addr, large_amount, "Foundation"));
    
    std::vector<Validator> validators;
    validators.push_back(create_test_validator(100, minimum_self_bond_));
    builder.create_genesis_validator_set(validators);
    
    Block genesis_block = builder.build();
    
    StateMachine state_machine(chain_id_);
    bool result = state_machine.initialize_from_genesis(
        genesis_block,
        builder.get_allocations()
    );
    
    ASSERT_TRUE(result);
    
    // Verify account was created with full allocation
    // (vesting is enforced at withdrawal time, not at account creation)
    Account account = state_machine.get_account(addr);
    EXPECT_EQ(account.balance, large_amount);
    EXPECT_EQ(account.nonce, 0);
    
    // Verify allocation has vesting schedule
    const auto& allocations = builder.get_allocations();
    ASSERT_FALSE(allocations.empty());
    EXPECT_TRUE(allocations[0].vesting_schedule.has_value());
}

/**
 * Test: Empty genesis (no allocations) works
 */
TEST_F(GenesisInitializationTest, EmptyGenesisWorks) {
    GenesisBuilder builder(config_);
    
    // No allocations, just validators
    std::vector<Validator> validators;
    validators.push_back(create_test_validator(100, minimum_self_bond_));
    builder.create_genesis_validator_set(validators);
    
    Block genesis_block = builder.build();
    
    StateMachine state_machine(chain_id_);
    bool result = state_machine.initialize_from_genesis(
        genesis_block,
        builder.get_allocations()
    );
    
    ASSERT_TRUE(result);
    EXPECT_EQ(state_machine.get_current_height(), 0);
}

/**
 * Test: Genesis initialization clears existing state
 */
TEST_F(GenesisInitializationTest, ClearsExistingState) {
    GenesisBuilder builder(config_);
    
    Address addr1 = create_test_address(1);
    builder.add_allocation(addr1, 100000000, "Test");
    
    std::vector<Validator> validators;
    validators.push_back(create_test_validator(100, minimum_self_bond_));
    builder.create_genesis_validator_set(validators);
    
    Block genesis_block = builder.build();
    
    StateMachine state_machine(chain_id_);
    
    // Create some existing state
    Address addr2 = create_test_address(2);
    state_machine.get_account_manager().create_account(addr2, 999999);
    
    // Initialize from genesis should clear existing state
    bool result = state_machine.initialize_from_genesis(
        genesis_block,
        builder.get_allocations()
    );
    
    ASSERT_TRUE(result);
    
    // Verify genesis account exists
    Account account1 = state_machine.get_account(addr1);
    EXPECT_EQ(account1.balance, 100000000);
    
    // Verify old account was cleared
    Account account2 = state_machine.get_account(addr2);
    EXPECT_EQ(account2.balance, 0);  // Default account has 0 balance
}

/**
 * Test: Genesis validator set is initialized
 */
TEST_F(GenesisInitializationTest, InitializesValidatorSet) {
    GenesisBuilder builder(config_);
    
    Address addr = create_test_address(1);
    builder.add_allocation(addr, 100000000, "Test");
    
    // Create validators with different stakes
    std::vector<Validator> validators;
    for (int i = 0; i < 15; ++i) {
        validators.push_back(create_test_validator(100 + i, minimum_self_bond_ * (i + 1)));
    }
    
    ASSERT_TRUE(builder.create_genesis_validator_set(validators));
    
    Block genesis_block = builder.build();
    
    // Verify validator set in genesis block
    const ValidatorSet& val_set = builder.get_validator_set();
    EXPECT_EQ(val_set.epoch, 0);
    EXPECT_EQ(val_set.validators.size(), 15);
    
    // Top 10 should be active
    auto active_validators = val_set.get_active_validators();
    EXPECT_EQ(active_validators.size(), validator_set_size_);
    
    // Remaining 5 should be standby
    auto standby_validators = val_set.get_standby_validators();
    EXPECT_EQ(standby_validators.size(), 5);
    
    StateMachine state_machine(chain_id_);
    bool result = state_machine.initialize_from_genesis(
        genesis_block,
        builder.get_allocations()
    );
    
    ASSERT_TRUE(result);
}

/**
 * Test: Genesis block has correct structure
 */
TEST_F(GenesisInitializationTest, GenesisBlockHasCorrectStructure) {
    GenesisBuilder builder(config_);
    
    Address addr = create_test_address(1);
    builder.add_allocation(addr, 100000000, "Test");
    
    std::vector<Validator> validators;
    validators.push_back(create_test_validator(100, minimum_self_bond_));
    builder.create_genesis_validator_set(validators);
    
    Block genesis_block = builder.build();
    
    // Verify block structure
    EXPECT_EQ(genesis_block.header.height, 0);
    EXPECT_EQ(genesis_block.header.epoch, 0);
    EXPECT_EQ(genesis_block.header.timestamp, genesis_timestamp_);
    EXPECT_TRUE(genesis_block.header.proposer.is_zero());  // No proposer for genesis
    EXPECT_TRUE(genesis_block.transactions.empty());  // No transactions in genesis
    
    // Verify QC is empty (no parent to justify)
    EXPECT_EQ(genesis_block.justify.block_height, 0);
    EXPECT_EQ(genesis_block.justify.total_stake_signed, 0);
    EXPECT_TRUE(genesis_block.justify.signers.empty());
}

/**
 * Test: Total supply constraint is enforced
 */
TEST_F(GenesisInitializationTest, TotalSupplyConstraintEnforced) {
    GenesisBuilder builder(config_);
    
    // Try to allocate more than total supply
    // Each allocation must be ≤5% of supply
    uint64_t five_percent = static_cast<uint64_t>(total_supply_ * 0.05);
    
    // Add allocations up to total supply
    int num_allocations = 0;
    uint64_t total_allocated = 0;
    
    for (int i = 0; i < 100; ++i) {
        Address addr = create_test_address(i);
        uint64_t amount = five_percent;
        
        if (total_allocated + amount > total_supply_) {
            // This should fail (exceeds total supply)
            EXPECT_FALSE(builder.add_allocation(addr, amount, "Test"));
            break;
        }
        
        ASSERT_TRUE(builder.add_allocation(addr, amount, "Test"));
        total_allocated += amount;
        num_allocations++;
    }
    
    // Verify we allocated close to total supply
    EXPECT_GT(num_allocations, 0);
    EXPECT_LE(builder.get_total_allocated(), total_supply_);
}

/**
 * Test: Minimum self-bond requirement is enforced
 */
TEST_F(GenesisInitializationTest, MinimumSelfBondEnforced) {
    GenesisBuilder builder(config_);
    
    Address addr = create_test_address(1);
    builder.add_allocation(addr, 100000000, "Test");
    
    // Create validators with stake below minimum
    std::vector<Validator> validators;
    validators.push_back(create_test_validator(100, minimum_self_bond_ - 1));
    
    // Should fail due to insufficient stake
    EXPECT_FALSE(builder.create_genesis_validator_set(validators));
}

