#include <gtest/gtest.h>
#include "sarafu/state/account.h"
#include "sarafu/crypto/blake3_hash.h"

using namespace sarafu::state;
using namespace sarafu::crypto;

// ============================================================================
// Address Tests
// ============================================================================

TEST(AddressTest, DefaultConstructor) {
    Address addr;
    EXPECT_TRUE(addr.is_zero());
    EXPECT_EQ(addr.size(), 32);
}

TEST(AddressTest, ConstructorFromArray) {
    Address::AddressArray data;
    for (size_t i = 0; i < Address::ADDRESS_SIZE; ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    Address addr(data);
    EXPECT_FALSE(addr.is_zero());
    EXPECT_EQ(addr.data(), data);
}

TEST(AddressTest, ConstructorFromVector) {
    std::vector<uint8_t> data(32);
    for (size_t i = 0; i < 32; ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    Address addr(data);
    EXPECT_FALSE(addr.is_zero());
}

TEST(AddressTest, ConstructorFromVectorInvalidSize) {
    std::vector<uint8_t> data(16); // Wrong size
    EXPECT_THROW(Address addr(data), std::invalid_argument);
}

TEST(AddressTest, FromPublicKey) {
    std::vector<uint8_t> public_key(32, 0xAB);
    Address addr = Address::from_public_key(public_key);
    EXPECT_FALSE(addr.is_zero());
    
    // Same public key should produce same address
    Address addr2 = Address::from_public_key(public_key);
    EXPECT_EQ(addr, addr2);
}

TEST(AddressTest, Equality) {
    Address::AddressArray data;
    data.fill(0x42);
    Address addr1(data);
    Address addr2(data);
    Address addr3;
    
    EXPECT_EQ(addr1, addr2);
    EXPECT_NE(addr1, addr3);
}

TEST(AddressTest, Comparison) {
    Address::AddressArray data1, data2;
    data1.fill(0x01);
    data2.fill(0x02);
    
    Address addr1(data1);
    Address addr2(data2);
    
    EXPECT_LT(addr1, addr2);
    EXPECT_FALSE(addr2 < addr1);
}

TEST(AddressTest, Serialization) {
    Address::AddressArray data;
    for (size_t i = 0; i < Address::ADDRESS_SIZE; ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    Address addr(data);
    
    auto serialized = addr.serialize();
    EXPECT_EQ(serialized.size(), 32);
    
    Address addr2(serialized);
    EXPECT_EQ(addr, addr2);
}

TEST(AddressTest, HexConversion) {
    Address::AddressArray data;
    data.fill(0xFF);
    Address addr(data);
    
    std::string hex = addr.to_hex();
    EXPECT_EQ(hex.length(), 64);
    EXPECT_EQ(hex, std::string(64, 'f'));
    
    Address addr2 = Address::from_hex(hex);
    EXPECT_EQ(addr, addr2);
}

TEST(AddressTest, HexConversionInvalidLength) {
    EXPECT_THROW(Address::from_hex("abc"), std::invalid_argument);
}

TEST(AddressTest, ZeroAddress) {
    Address zero = Address::zero();
    EXPECT_TRUE(zero.is_zero());
    
    Address::AddressArray data;
    data.fill(0);
    Address zero2(data);
    EXPECT_TRUE(zero2.is_zero());
    EXPECT_EQ(zero, zero2);
}

// ============================================================================
// Account Tests
// ============================================================================

TEST(AccountTest, DefaultConstructor) {
    Account account;
    EXPECT_TRUE(account.address.is_zero());
    EXPECT_EQ(account.balance, 0);
    EXPECT_EQ(account.nonce, 0);
    EXPECT_EQ(account.code_hash, Blake3Hash::zero());
}

TEST(AccountTest, ConstructorWithParameters) {
    Address::AddressArray addr_data;
    addr_data.fill(0x42);
    Address addr(addr_data);
    
    Account account(addr, 1000, 5);
    EXPECT_EQ(account.address, addr);
    EXPECT_EQ(account.balance, 1000);
    EXPECT_EQ(account.nonce, 5);
    EXPECT_EQ(account.code_hash, Blake3Hash::zero());
}

TEST(AccountTest, ConstructorWithCodeHash) {
    Address::AddressArray addr_data;
    addr_data.fill(0x42);
    Address addr(addr_data);
    
    Blake3Hash code_hash = Blake3Hash::hash("contract code");
    
    Account account(addr, 1000, 5, code_hash);
    EXPECT_EQ(account.address, addr);
    EXPECT_EQ(account.balance, 1000);
    EXPECT_EQ(account.nonce, 5);
    EXPECT_EQ(account.code_hash, code_hash);
}

TEST(AccountTest, Serialization) {
    Address::AddressArray addr_data;
    addr_data.fill(0x42);
    Address addr(addr_data);
    
    Account account(addr, 1000, 5);
    
    auto serialized = account.serialize();
    EXPECT_EQ(serialized.size(), 32 + 8 + 8 + 32); // address + balance + nonce + code_hash
    
    Account account2 = Account::deserialize(serialized);
    EXPECT_EQ(account, account2);
}

TEST(AccountTest, SerializationWithCodeHash) {
    Address::AddressArray addr_data;
    addr_data.fill(0x42);
    Address addr(addr_data);
    
    Blake3Hash code_hash = Blake3Hash::hash("contract code");
    Account account(addr, 1000, 5, code_hash);
    
    auto serialized = account.serialize();
    Account account2 = Account::deserialize(serialized);
    
    EXPECT_EQ(account, account2);
    EXPECT_EQ(account2.code_hash, code_hash);
}

TEST(AccountTest, DeserializationInvalidSize) {
    std::vector<uint8_t> invalid_data(50); // Wrong size
    EXPECT_THROW(Account::deserialize(invalid_data), std::invalid_argument);
}

TEST(AccountTest, Hash) {
    Address::AddressArray addr_data;
    addr_data.fill(0x42);
    Address addr(addr_data);
    
    Account account1(addr, 1000, 5);
    Account account2(addr, 1000, 5);
    Account account3(addr, 2000, 5); // Different balance
    
    // Same account state should produce same hash
    EXPECT_EQ(account1.hash(), account2.hash());
    
    // Different account state should produce different hash
    EXPECT_NE(account1.hash(), account3.hash());
}

TEST(AccountTest, Equality) {
    Address::AddressArray addr_data;
    addr_data.fill(0x42);
    Address addr(addr_data);
    
    Account account1(addr, 1000, 5);
    Account account2(addr, 1000, 5);
    Account account3(addr, 2000, 5);
    
    EXPECT_EQ(account1, account2);
    EXPECT_NE(account1, account3);
}

TEST(AccountTest, SerializationRoundTrip) {
    // Test with various values
    Address::AddressArray addr_data;
    for (size_t i = 0; i < Address::ADDRESS_SIZE; ++i) {
        addr_data[i] = static_cast<uint8_t>(i);
    }
    Address addr(addr_data);
    
    Account original(addr, UINT64_MAX, 12345);
    
    auto serialized = original.serialize();
    Account deserialized = Account::deserialize(serialized);
    
    EXPECT_EQ(original.address, deserialized.address);
    EXPECT_EQ(original.balance, deserialized.balance);
    EXPECT_EQ(original.nonce, deserialized.nonce);
    EXPECT_EQ(original.code_hash, deserialized.code_hash);
}

TEST(AccountTest, LargeBalanceAndNonce) {
    Address addr = Address::zero();
    
    Account account(addr, UINT64_MAX, UINT64_MAX);
    
    auto serialized = account.serialize();
    Account deserialized = Account::deserialize(serialized);
    
    EXPECT_EQ(account.balance, deserialized.balance);
    EXPECT_EQ(account.nonce, deserialized.nonce);
}
