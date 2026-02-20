#include <gtest/gtest.h>
#include "sarafu/network/network_layer.h"
#include "sarafu/network/message_handler.h"

using namespace sarafu::network;
using namespace sarafu::crypto;
using namespace sarafu::consensus;
using namespace sarafu::state;

// ============================================================================
// NetworkMessage Tests
// ============================================================================

TEST(NetworkMessageTest, ConstructorAndHash) {
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    NetworkMessage msg(MessageType::Transaction, payload);
    
    EXPECT_EQ(msg.type, MessageType::Transaction);
    EXPECT_EQ(msg.payload, payload);
    
    // Hash should be computed
    Blake3Hash expected_hash = Blake3Hash::hash(payload.data(), payload.size());
    EXPECT_EQ(msg.hash, expected_hash);
}

TEST(NetworkMessageTest, Serialization) {
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    NetworkMessage msg(MessageType::Block, payload);
    
    // Serialize
    auto serialized = msg.serialize();
    
    // Deserialize
    NetworkMessage deserialized = NetworkMessage::deserialize(serialized);
    
    EXPECT_EQ(deserialized.type, msg.type);
    EXPECT_EQ(deserialized.payload, msg.payload);
    EXPECT_EQ(deserialized.hash, msg.hash);
}

TEST(NetworkMessageTest, Equality) {
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    NetworkMessage msg1(MessageType::Transaction, payload);
    NetworkMessage msg2(MessageType::Transaction, payload);
    NetworkMessage msg3(MessageType::Block, payload);
    
    EXPECT_EQ(msg1, msg2);
    EXPECT_NE(msg1, msg3);
}

// ============================================================================
// PeerInfo Tests
// ============================================================================

TEST(PeerInfoTest, Constructor) {
    PeerID peer_id = "peer-123";
    std::vector<std::string> addresses = {"/ip4/127.0.0.1/tcp/9000"};
    PeerInfo info(peer_id, addresses, true, 12345, 100);
    
    EXPECT_EQ(info.id, peer_id);
    EXPECT_EQ(info.addresses, addresses);
    EXPECT_TRUE(info.is_validator);
    EXPECT_EQ(info.last_seen, 12345);
    EXPECT_EQ(info.reputation_score, 100);
}

TEST(PeerInfoTest, DefaultConstructor) {
    PeerInfo info;
    
    EXPECT_EQ(info.id, "");
    EXPECT_TRUE(info.addresses.empty());
    EXPECT_FALSE(info.is_validator);
    EXPECT_EQ(info.last_seen, 0);
    EXPECT_EQ(info.reputation_score, 100);
}

// ============================================================================
// NetworkConfig Tests
// ============================================================================

TEST(NetworkConfigTest, DefaultConstructor) {
    NetworkConfig config;
    
    EXPECT_EQ(config.listen_address, "/ip4/0.0.0.0/tcp/9000");
    EXPECT_TRUE(config.bootstrap_peers.empty());
    EXPECT_EQ(config.min_peers, 8);
    EXPECT_EQ(config.max_peers, 50);
    EXPECT_EQ(config.gossip_fanout, 8);
    EXPECT_TRUE(config.enable_quic);
    EXPECT_TRUE(config.enable_tcp_fallback);
}

TEST(NetworkConfigTest, CustomConstructor) {
    std::vector<std::string> bootstrap = {"/ip4/127.0.0.1/tcp/9001"};
    NetworkConfig config("/ip4/0.0.0.0/tcp/9002", bootstrap, 10, 100, 16, false, true);
    
    EXPECT_EQ(config.listen_address, "/ip4/0.0.0.0/tcp/9002");
    EXPECT_EQ(config.bootstrap_peers, bootstrap);
    EXPECT_EQ(config.min_peers, 10);
    EXPECT_EQ(config.max_peers, 100);
    EXPECT_EQ(config.gossip_fanout, 16);
    EXPECT_FALSE(config.enable_quic);
    EXPECT_TRUE(config.enable_tcp_fallback);
}

// ============================================================================
// NetworkLayer Tests
// ============================================================================

TEST(NetworkLayerTest, Initialization) {
    NetworkConfig config;
    NetworkLayer network(config);
    
    EXPECT_TRUE(network.initialize());
    EXPECT_EQ(network.peer_count(), 0);
    EXPECT_FALSE(network.local_peer_id().empty());
    
    network.shutdown();
}

TEST(NetworkLayerTest, ConnectToPeers) {
    NetworkConfig config;
    config.bootstrap_peers = {
        "/ip4/127.0.0.1/tcp/9001",
        "/ip4/127.0.0.1/tcp/9002"
    };
    
    NetworkLayer network(config);
    EXPECT_TRUE(network.initialize());
    
    // Connect to bootstrap peers (stub implementation)
    size_t connected = network.connect_to_peers(config.bootstrap_peers);
    
    // In stub implementation, this creates fake peers
    EXPECT_EQ(connected, 2);
    EXPECT_EQ(network.peer_count(), 2);
    
    network.shutdown();
}

TEST(NetworkLayerTest, BanPeer) {
    NetworkConfig config;
    NetworkLayer network(config);
    EXPECT_TRUE(network.initialize());
    
    // Connect to a peer
    std::vector<std::string> peers = {"/ip4/127.0.0.1/tcp/9001"};
    network.connect_to_peers(peers);
    EXPECT_EQ(network.peer_count(), 1);
    
    // Get the peer ID
    auto peer_list = network.get_peers();
    ASSERT_FALSE(peer_list.empty());
    PeerID peer_id = peer_list[0].id;
    
    // Ban the peer
    network.ban_peer(peer_id, 3600);
    
    // Peer should be disconnected
    EXPECT_EQ(network.peer_count(), 0);
    EXPECT_FALSE(network.is_connected(peer_id));
    
    network.shutdown();
}

TEST(NetworkLayerTest, UpdateReputation) {
    NetworkConfig config;
    NetworkLayer network(config);
    EXPECT_TRUE(network.initialize());
    
    // Connect to a peer
    std::vector<std::string> peers = {"/ip4/127.0.0.1/tcp/9001"};
    network.connect_to_peers(peers);
    
    auto peer_list = network.get_peers();
    ASSERT_FALSE(peer_list.empty());
    PeerID peer_id = peer_list[0].id;
    
    // Initial reputation should be 100
    EXPECT_EQ(peer_list[0].reputation_score, 100);
    
    // Decrease reputation
    network.update_reputation(peer_id, -10);
    peer_list = network.get_peers();
    EXPECT_EQ(peer_list[0].reputation_score, 90);
    
    // Increase reputation
    network.update_reputation(peer_id, 5);
    peer_list = network.get_peers();
    EXPECT_EQ(peer_list[0].reputation_score, 95);
    
    network.shutdown();
}

TEST(NetworkLayerTest, SetValidatorStatus) {
    NetworkConfig config;
    NetworkLayer network(config);
    EXPECT_TRUE(network.initialize());
    
    // Connect to a peer
    std::vector<std::string> peers = {"/ip4/127.0.0.1/tcp/9001"};
    network.connect_to_peers(peers);
    
    auto peer_list = network.get_peers();
    ASSERT_FALSE(peer_list.empty());
    PeerID peer_id = peer_list[0].id;
    
    // Initially not a validator
    EXPECT_FALSE(peer_list[0].is_validator);
    
    // Mark as validator
    network.set_validator_status(peer_id, true);
    peer_list = network.get_peers();
    EXPECT_TRUE(peer_list[0].is_validator);
    
    network.shutdown();
}

TEST(NetworkLayerTest, MessageHandlerRegistration) {
    NetworkConfig config;
    NetworkLayer network(config);
    EXPECT_TRUE(network.initialize());
    
    bool handler_called = false;
    
    // Register a message handler
    network.on_message(MessageType::Transaction, 
        [&handler_called](const NetworkMessage& msg, const PeerID& sender) {
            handler_called = true;
        });
    
    // Handler should be registered (we can't easily test invocation in stub)
    // This test just verifies registration doesn't crash
    
    network.shutdown();
}

// ============================================================================
// Vote Tests
// ============================================================================

TEST(VoteTest, ConstructorAndSerialization) {
    std::string test_data = "test";
    Blake3Hash block_hash = Blake3Hash::hash(test_data);
    Address validator_id = Address::from_hex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    BLS12_381_Signature signature;  // Default signature
    
    Vote vote(100, block_hash, 5, validator_id, signature);
    
    EXPECT_EQ(vote.block_height, 100);
    EXPECT_EQ(vote.block_hash, block_hash);
    EXPECT_EQ(vote.view_number, 5);
    EXPECT_EQ(vote.validator_id, validator_id);
    
    // Serialize and deserialize
    auto serialized = vote.serialize();
    Vote deserialized = Vote::deserialize(serialized);
    
    EXPECT_EQ(deserialized, vote);
}

// ============================================================================
// BlockRequest Tests
// ============================================================================

TEST(BlockRequestTest, ByHeight) {
    Address requester = Address::from_hex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    BlockRequest request = BlockRequest::by_height(100, requester);
    
    EXPECT_EQ(request.type, BlockRequest::RequestType::ByHeight);
    EXPECT_EQ(request.height, 100);
    EXPECT_EQ(request.requester_id, requester);
    
    // Serialize and deserialize
    auto serialized = request.serialize();
    BlockRequest deserialized = BlockRequest::deserialize(serialized);
    
    EXPECT_EQ(deserialized, request);
}

TEST(BlockRequestTest, ByHash) {
    std::string test_data = "test";
    Blake3Hash hash = Blake3Hash::hash(test_data);
    Address requester = Address::from_hex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    BlockRequest request = BlockRequest::by_hash(hash, requester);
    
    EXPECT_EQ(request.type, BlockRequest::RequestType::ByHash);
    EXPECT_EQ(request.hash, hash);
    EXPECT_EQ(request.requester_id, requester);
    
    // Serialize and deserialize
    auto serialized = request.serialize();
    BlockRequest deserialized = BlockRequest::deserialize(serialized);
    
    EXPECT_EQ(deserialized, request);
}

// ============================================================================
// ValidatorInfo Tests
// ============================================================================

TEST(ValidatorInfoTest, ConstructorAndSerialization) {
    Address validator_id = Address::from_hex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    BLS12_381_PublicKey consensus_key;  // Default key
    
    ValidatorInfo info(validator_id, consensus_key, 1000000, true);
    
    EXPECT_EQ(info.validator_id, validator_id);
    EXPECT_EQ(info.bonded_stake, 1000000);
    EXPECT_TRUE(info.is_active);
    
    // Serialize and deserialize
    auto serialized = info.serialize();
    ValidatorInfo deserialized = ValidatorInfo::deserialize(serialized);
    
    EXPECT_EQ(deserialized, info);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
