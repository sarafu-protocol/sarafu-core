#include <gtest/gtest.h>
#include "sarafu/monitoring/metrics_collector.h"
#include <thread>
#include <vector>

using namespace sarafu::monitoring;

class MetricsCollectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        collector_ = &MetricsCollector::instance();
    }

    MetricsCollector* collector_;
};

TEST_F(MetricsCollectorTest, InitialValues) {
    MetricsCollector collector;
    
    EXPECT_EQ(collector.get_block_height(), 0);
    EXPECT_EQ(collector.get_finalized_height(), 0);
    EXPECT_EQ(collector.get_peer_count(), 0);
    EXPECT_EQ(collector.get_mempool_size(), 0);
    EXPECT_EQ(collector.get_validator_status(), ValidatorStatus::Standby);
    EXPECT_EQ(collector.get_sync_status(), SyncStatus::NotSynced);
}

TEST_F(MetricsCollectorTest, SetAndGetBlockHeight) {
    MetricsCollector collector;
    
    collector.set_block_height(12345);
    EXPECT_EQ(collector.get_block_height(), 12345);
    
    collector.set_block_height(67890);
    EXPECT_EQ(collector.get_block_height(), 67890);
}

TEST_F(MetricsCollectorTest, SetAndGetFinalizedHeight) {
    MetricsCollector collector;
    
    collector.set_finalized_height(12340);
    EXPECT_EQ(collector.get_finalized_height(), 12340);
}

TEST_F(MetricsCollectorTest, SetAndGetPeerCount) {
    MetricsCollector collector;
    
    collector.set_peer_count(25);
    EXPECT_EQ(collector.get_peer_count(), 25);
}

TEST_F(MetricsCollectorTest, SetAndGetMempoolSize) {
    MetricsCollector collector;
    
    collector.set_mempool_size(150);
    EXPECT_EQ(collector.get_mempool_size(), 150);
}

TEST_F(MetricsCollectorTest, SetAndGetValidatorStatus) {
    MetricsCollector collector;
    
    collector.set_validator_status(ValidatorStatus::Active);
    EXPECT_EQ(collector.get_validator_status(), ValidatorStatus::Active);
    
    collector.set_validator_status(ValidatorStatus::Jailed);
    EXPECT_EQ(collector.get_validator_status(), ValidatorStatus::Jailed);
}

TEST_F(MetricsCollectorTest, SetAndGetSyncStatus) {
    MetricsCollector collector;
    
    collector.set_sync_status(SyncStatus::Syncing);
    EXPECT_EQ(collector.get_sync_status(), SyncStatus::Syncing);
    
    collector.set_sync_status(SyncStatus::Synced);
    EXPECT_EQ(collector.get_sync_status(), SyncStatus::Synced);
}

TEST_F(MetricsCollectorTest, PrometheusExport) {
    MetricsCollector collector;
    
    collector.set_block_height(1000);
    collector.set_finalized_height(995);
    collector.set_peer_count(30);
    collector.set_mempool_size(200);
    collector.set_validator_status(ValidatorStatus::Active);
    collector.set_sync_status(SyncStatus::Synced);
    
    std::string prometheus_output = collector.export_prometheus();
    
    // Check that all metrics are present
    EXPECT_TRUE(prometheus_output.find("sarafu_block_height 1000") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("sarafu_finalized_height 995") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("sarafu_peer_count 30") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("sarafu_mempool_size 200") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("sarafu_validator_status") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("sarafu_sync_status") != std::string::npos);
    
    // Check for Prometheus format headers
    EXPECT_TRUE(prometheus_output.find("# HELP") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("# TYPE") != std::string::npos);
}

TEST_F(MetricsCollectorTest, ThreadSafety) {
    MetricsCollector collector;
    
    // Create multiple threads updating metrics simultaneously
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&collector, i]() {
            for (int j = 0; j < 1000; j++) {
                collector.set_block_height(i * 1000 + j);
                collector.set_peer_count(i + j);
                collector.set_mempool_size(j);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // If we get here without crashing, thread safety is working
    SUCCEED();
}

TEST_F(MetricsCollectorTest, SingletonInstance) {
    MetricsCollector& collector1 = MetricsCollector::instance();
    MetricsCollector& collector2 = MetricsCollector::instance();
    
    // Should be the same instance
    EXPECT_EQ(&collector1, &collector2);
}
