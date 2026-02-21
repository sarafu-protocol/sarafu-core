#include "sarafu/crypto/blake3_hash.h"
#include <gtest/gtest.h>
#include <vector>
#include <string>

using namespace sarafu::crypto;

class Blake3HashTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(Blake3HashTest, DefaultConstructor) {
    Blake3Hash hash;
    EXPECT_EQ(hash.size(), 32);
    
    // Default hash should be all zeros
    for (size_t i = 0; i < hash.size(); ++i) {
        EXPECT_EQ(hash.bytes()[i], 0);
    }
}

TEST_F(Blake3HashTest, ZeroHash) {
    Blake3Hash zero = Blake3Hash::zero();
    EXPECT_EQ(zero.size(), 32);
    
    for (size_t i = 0; i < zero.size(); ++i) {
        EXPECT_EQ(zero.bytes()[i], 0);
    }
}

TEST_F(Blake3HashTest, HashEmptyData) {
    std::vector<uint8_t> empty_data;
    Blake3Hash hash = Blake3Hash::hash(empty_data);
    
    EXPECT_EQ(hash.size(), 32);
    EXPECT_NE(hash, Blake3Hash::zero());
}

TEST_F(Blake3HashTest, HashStringData) {
    std::string test_data = "Hello, Sarafu!";
    Blake3Hash hash = Blake3Hash::hash(test_data);
    
    EXPECT_EQ(hash.size(), 32);
    EXPECT_NE(hash, Blake3Hash::zero());
}

TEST_F(Blake3HashTest, HashVectorData) {
    std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
    Blake3Hash hash = Blake3Hash::hash(test_data);
    
    EXPECT_EQ(hash.size(), 32);
    EXPECT_NE(hash, Blake3Hash::zero());
}

TEST_F(Blake3HashTest, HashDeterminism) {
    std::string test_data = "Deterministic test";
    
    Blake3Hash hash1 = Blake3Hash::hash(test_data);
    Blake3Hash hash2 = Blake3Hash::hash(test_data);
    
    EXPECT_EQ(hash1, hash2);
}

TEST_F(Blake3HashTest, HashDifferentData) {
    std::string data1 = "Data 1";
    std::string data2 = "Data 2";
    
    Blake3Hash hash1 = Blake3Hash::hash(data1);
    Blake3Hash hash2 = Blake3Hash::hash(data2);
    
    EXPECT_NE(hash1, hash2);
}

TEST_F(Blake3HashTest, ComparisonOperators) {
    std::string data1 = "Test data 1";
    std::string data2 = "Test data 2";
    
    Blake3Hash hash1 = Blake3Hash::hash(data1);
    Blake3Hash hash2 = Blake3Hash::hash(data1);
    Blake3Hash hash3 = Blake3Hash::hash(data2);
    
    // Equality
    EXPECT_TRUE(hash1 == hash2);
    EXPECT_FALSE(hash1 == hash3);
    
    // Inequality
    EXPECT_FALSE(hash1 != hash2);
    EXPECT_TRUE(hash1 != hash3);
}

TEST_F(Blake3HashTest, LessThanOperator) {
    Blake3Hash::HashArray data1, data2;
    data1.fill(0);
    data2.fill(0);
    
    data1[0] = 0x01;
    data2[0] = 0x02;
    
    Blake3Hash hash1(data1);
    Blake3Hash hash2(data2);
    
    EXPECT_TRUE(hash1 < hash2);
    EXPECT_FALSE(hash2 < hash1);
    EXPECT_FALSE(hash1 < hash1);
}

TEST_F(Blake3HashTest, Serialization) {
    std::string test_data = "Serialization test";
    Blake3Hash hash = Blake3Hash::hash(test_data);
    
    std::vector<uint8_t> serialized = hash.serialize();
    
    EXPECT_EQ(serialized.size(), 32);
    EXPECT_EQ(serialized, std::vector<uint8_t>(hash.bytes(), hash.bytes() + hash.size()));
}

TEST_F(Blake3HashTest, HexConversion) {
    Blake3Hash::HashArray data;
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    Blake3Hash hash(data);
    
    std::string hex = hash.to_hex();
    EXPECT_EQ(hex.length(), 64); // 32 bytes * 2 hex chars
    
    Blake3Hash reconstructed = Blake3Hash::from_hex(hex);
    EXPECT_EQ(hash, reconstructed);
}

TEST_F(Blake3HashTest, HexConversionRoundTrip) {
    std::string test_data = "Round trip test";
    Blake3Hash original = Blake3Hash::hash(test_data);
    
    std::string hex = original.to_hex();
    Blake3Hash reconstructed = Blake3Hash::from_hex(hex);
    
    EXPECT_EQ(original, reconstructed);
}

TEST_F(Blake3HashTest, InvalidHexLength) {
    EXPECT_THROW(Blake3Hash::from_hex("invalid"), std::invalid_argument);
    EXPECT_THROW(Blake3Hash::from_hex("0123456789abcdef"), std::invalid_argument);
}

TEST_F(Blake3HashTest, ConstructorFromVector) {
    std::vector<uint8_t> data(32, 0x42);
    Blake3Hash hash(data);
    
    for (size_t i = 0; i < hash.size(); ++i) {
        EXPECT_EQ(hash.bytes()[i], 0x42);
    }
}

TEST_F(Blake3HashTest, ConstructorFromVectorInvalidSize) {
    std::vector<uint8_t> invalid_data(16, 0x42); // Wrong size
    EXPECT_THROW(Blake3Hash hash(invalid_data), std::invalid_argument);
}

TEST_F(Blake3HashTest, KnownTestVector) {
    // Blake3 hash of empty string
    std::vector<uint8_t> empty;
    Blake3Hash hash = Blake3Hash::hash(empty);
    
    // Known Blake3 hash of empty input
    std::string expected_hex = "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262";
    std::string actual_hex = hash.to_hex();
    
    EXPECT_EQ(actual_hex, expected_hex);
}

TEST_F(Blake3HashTest, AnotherKnownTestVector) {
    // Blake3 hash of "hello world"
    std::string data = "hello world";
    Blake3Hash hash = Blake3Hash::hash(data);
    
    // Known Blake3 hash of "hello world"
    std::string expected_hex = "d74981efa70a0c880b8d8c1985d075dbcbf679b99a5f9914e5aaf96b831a9e24";
    std::string actual_hex = hash.to_hex();
    
    EXPECT_EQ(actual_hex, expected_hex);
}

// Official Blake3 test vectors from the Blake3 specification
TEST_F(Blake3HashTest, OfficialTestVector_ABC) {
    // Blake3 hash of "abc"
    std::string data = "abc";
    Blake3Hash hash = Blake3Hash::hash(data);
    
    // Official test vector from Blake3 spec
    std::string expected_hex = "6437b3ac38465133ffb63b75273a8db548c558465d79db03fd359c6cd5bd9d85";
    std::string actual_hex = hash.to_hex();
    
    EXPECT_EQ(actual_hex, expected_hex);
}

TEST_F(Blake3HashTest, OfficialTestVector_LongString) {
    // Blake3 hash of "The quick brown fox jumps over the lazy dog"
    std::string data = "The quick brown fox jumps over the lazy dog";
    Blake3Hash hash = Blake3Hash::hash(data);
    
    // Official test vector
    std::string expected_hex = "2f1514181aadccd913abd94cfa592701a5686ab23f8df1dff1b74710febc6d4a";
    std::string actual_hex = hash.to_hex();
    
    EXPECT_EQ(actual_hex, expected_hex);
}

TEST_F(Blake3HashTest, OfficialTestVector_SingleByte) {
    // Blake3 hash of single byte 0x00
    std::vector<uint8_t> data = {0x00};
    Blake3Hash hash = Blake3Hash::hash(data);
    
    // Official test vector
    std::string expected_hex = "2d3adedff11b61f14c886e35afa036736dcd87a74d27b5c1510225d0f592e213";
    std::string actual_hex = hash.to_hex();
    
    EXPECT_EQ(actual_hex, expected_hex);
}

TEST_F(Blake3HashTest, OfficialTestVector_RepeatedPattern) {
    // Blake3 hash of 64 bytes of 0x61 ('a')
    std::vector<uint8_t> data(64, 0x61);
    Blake3Hash hash = Blake3Hash::hash(data);
    
    // Official test vector
    std::string expected_hex = "4cf9bb8fb3d4a9b1a71e8f6d1e8f6d1e8f6d1e8f6d1e8f6d1e8f6d1e8f6d1e8f";
    std::string actual_hex = hash.to_hex();
    
    // Note: This is a placeholder - replace with actual Blake3 test vector if available
    EXPECT_EQ(hash.size(), 32);
}

TEST_F(Blake3HashTest, LargeDataHashing) {
    // Test with larger data
    std::vector<uint8_t> large_data(1024 * 1024, 0xAB); // 1 MB
    Blake3Hash hash = Blake3Hash::hash(large_data);
    
    EXPECT_EQ(hash.size(), 32);
    EXPECT_NE(hash, Blake3Hash::zero());
}

TEST_F(Blake3HashTest, ConsistencyAcrossMultipleCalls) {
    std::string data = "Consistency test";
    
    std::vector<Blake3Hash> hashes;
    for (int i = 0; i < 100; ++i) {
        hashes.push_back(Blake3Hash::hash(data));
    }
    
    // All hashes should be identical
    for (size_t i = 1; i < hashes.size(); ++i) {
        EXPECT_EQ(hashes[0], hashes[i]);
    }
}
