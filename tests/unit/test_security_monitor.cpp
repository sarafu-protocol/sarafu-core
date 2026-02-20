#include <gtest/gtest.h>
#include "sarafu/monitoring/security_monitor.h"
#include <thread>
#include <chrono>

using namespace sarafu::monitoring;

class SecurityMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor_ = &SecurityMonitor::instance();
    }

    SecurityMonitor* monitor_;
};

TEST_F(SecurityMonitorTest, StakeConcentration) {
    SecurityMonitor monitor;
    
    monitor.update_stake_concentration("entity1", 1000, 10000);
    monitor.update_stake_concentration("entity2", 2000, 10000);
    monitor.update_stake_concentration("entity3", 500, 10000);
    
    auto concentration = monitor.get_stake_concentration();
    
    ASSERT_EQ(concentration.size(), 3);
    
    // Should be sorted by stake percentage descending
    EXPECT_EQ(concentration[0].entity_id, "entity2");
    EXPECT_DOUBLE_EQ(concentration[0].stake_percentage, 0.20);
    
    EXPECT_EQ(concentration[1].entity_id, "entity1");
    EXPECT_DOUBLE_EQ(concentration[1].stake_percentage, 0.10);
    
    EXPECT_EQ(concentration[2].entity_id, "entity3");
    EXPECT_DOUBLE_EQ(concentration[2].stake_percentage, 0.05);
}

TEST_F(SecurityMonitorTest, StakeConcentrationAlert) {
    SecurityMonitor monitor;
    
    // Below threshold - no alert
    monitor.update_stake_concentration("entity1", 1500, 10000);
    EXPECT_FALSE(monitor.check_stake_concentration_alert(0.20));
    
    // Above threshold - should alert
    monitor.update_stake_concentration("entity2", 2500, 10000);
    EXPECT_TRUE(monitor.check_stake_concentration_alert(0.20));
}

TEST_F(SecurityMonitorTest, GeographicDistribution) {
    SecurityMonitor monitor;
    
    monitor.update_geographic_distribution("US", 50, 3000);
    monitor.update_geographic_distribution("EU", 30, 2500);
    monitor.update_geographic_distribution("Asia", 20, 1500);
    
    auto distribution = monitor.get_geographic_distribution();
    
    ASSERT_EQ(distribution.size(), 3);
    
    // Should be sorted by stake percentage descending
    EXPECT_EQ(distribution[0].jurisdiction, "US");
    EXPECT_EQ(distribution[1].jurisdiction, "EU");
    EXPECT_EQ(distribution[2].jurisdiction, "Asia");
}

TEST_F(SecurityMonitorTest, NakamotoCoefficient) {
    SecurityMonitor monitor;
    
    // Set up distribution where 3 jurisdictions control >50%
    monitor.update_stake_concentration("total", 10000, 10000);
    monitor.update_geographic_distribution("US", 50, 3000);
    monitor.update_geographic_distribution("EU", 30, 2500);
    monitor.update_geographic_distribution("Asia", 20, 2000);
    monitor.update_geographic_distribution("Other", 10, 2500);
    
    double nakamoto = monitor.calculate_nakamoto_coefficient();
    
    // Should need 3 jurisdictions to control >50%
    EXPECT_DOUBLE_EQ(nakamoto, 3.0);
}

TEST_F(SecurityMonitorTest, BlockPropagation) {
    SecurityMonitor monitor;
    
    // Record some propagation times
    monitor.record_block_propagation(1, std::chrono::milliseconds(100));
    monitor.record_block_propagation(2, std::chrono::milliseconds(200));
    monitor.record_block_propagation(3, std::chrono::milliseconds(300));
    monitor.record_block_propagation(4, std::chrono::milliseconds(400));
    monitor.record_block_propagation(5, std::chrono::milliseconds(500));
    
    double p95 = monitor.get_propagation_95th_percentile();
    
    // 95th percentile should be around 500ms
    EXPECT_GE(p95, 450.0);
    EXPECT_LE(p95, 500.0);
}

TEST_F(SecurityMonitorTest, BlockPropagationAlert) {
    SecurityMonitor monitor;
    
    // Record times below threshold
    for (int i = 0; i < 100; i++) {
        monitor.record_block_propagation(i, std::chrono::milliseconds(300));
    }
    
    EXPECT_FALSE(monitor.check_propagation_alert(std::chrono::milliseconds(500)));
    
    // Record times above threshold
    for (int i = 100; i < 200; i++) {
        monitor.record_block_propagation(i, std::chrono::milliseconds(600));
    }
    
    EXPECT_TRUE(monitor.check_propagation_alert(std::chrono::milliseconds(500)));
}

TEST_F(SecurityMonitorTest, ValidatorLiveness) {
    SecurityMonitor monitor;
    
    monitor.update_validator_liveness("validator1", 950, 50);
    monitor.update_validator_liveness("validator2", 980, 20);
    monitor.update_validator_liveness("validator3", 900, 100);
    
    auto liveness = monitor.get_validator_liveness();
    
    ASSERT_EQ(liveness.size(), 3);
    
    // Check liveness calculations
    for (const auto& v : liveness) {
        if (v.validator_id == "validator1") {
            EXPECT_DOUBLE_EQ(v.liveness_percentage, 0.95);
            EXPECT_FALSE(v.is_online);  // 95% is not > 95%
        } else if (v.validator_id == "validator2") {
            EXPECT_DOUBLE_EQ(v.liveness_percentage, 0.98);
            EXPECT_TRUE(v.is_online);
        } else if (v.validator_id == "validator3") {
            EXPECT_DOUBLE_EQ(v.liveness_percentage, 0.90);
            EXPECT_FALSE(v.is_online);
        }
    }
}

TEST_F(SecurityMonitorTest, ValidatorLivenessAlert) {
    SecurityMonitor monitor;
    
    // 8 online, 2 offline = 20% offline
    for (int i = 0; i < 8; i++) {
        monitor.update_validator_liveness("validator" + std::to_string(i), 980, 20);
    }
    for (int i = 8; i < 10; i++) {
        monitor.update_validator_liveness("validator" + std::to_string(i), 900, 100);
    }
    
    double offline_pct = monitor.calculate_offline_percentage();
    EXPECT_DOUBLE_EQ(offline_pct, 0.20);
    
    // Should alert when > 10% offline
    EXPECT_TRUE(monitor.check_liveness_alert(0.10));
}

TEST_F(SecurityMonitorTest, DoubleSignDetection) {
    SecurityMonitor monitor;
    
    EXPECT_EQ(monitor.get_double_sign_count(), 0);
    
    monitor.record_double_sign_attempt("validator1", 1000);
    EXPECT_EQ(monitor.get_double_sign_count(), 1);
    
    monitor.record_double_sign_attempt("validator2", 1001);
    monitor.record_double_sign_attempt("validator1", 1002);
    EXPECT_EQ(monitor.get_double_sign_count(), 3);
}

TEST_F(SecurityMonitorTest, AlertManagement) {
    SecurityMonitor monitor;
    
    monitor.add_alert(AlertLevel::Info, "test", "Info message");
    monitor.add_alert(AlertLevel::Warning, "test", "Warning message");
    monitor.add_alert(AlertLevel::Critical, "test", "Critical message");
    
    auto alerts = monitor.get_recent_alerts(10);
    
    ASSERT_EQ(alerts.size(), 3);
    EXPECT_EQ(alerts[0].level, AlertLevel::Info);
    EXPECT_EQ(alerts[1].level, AlertLevel::Warning);
    EXPECT_EQ(alerts[2].level, AlertLevel::Critical);
}

TEST_F(SecurityMonitorTest, ClearOldAlerts) {
    SecurityMonitor monitor;
    
    monitor.add_alert(AlertLevel::Info, "test", "Message 1");
    
    // Wait a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    monitor.add_alert(AlertLevel::Info, "test", "Message 2");
    
    // Clear alerts older than 0 seconds (should clear first one)
    monitor.clear_old_alerts(0);
    
    auto alerts = monitor.get_recent_alerts(10);
    
    // Should have at least the recent alert
    EXPECT_GE(alerts.size(), 1);
}

TEST_F(SecurityMonitorTest, PrometheusExport) {
    SecurityMonitor monitor;
    
    monitor.update_stake_concentration("entity1", 2000, 10000);
    monitor.update_validator_liveness("validator1", 950, 50);
    monitor.record_block_propagation(1, std::chrono::milliseconds(300));
    monitor.record_double_sign_attempt("validator2", 1000);
    
    std::string prometheus_output = monitor.export_security_metrics();
    
    // Check that metrics are present
    EXPECT_TRUE(prometheus_output.find("sarafu_stake_concentration") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("sarafu_nakamoto_coefficient") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("sarafu_block_propagation_p95") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("sarafu_validator_offline_percentage") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("sarafu_double_sign_attempts") != std::string::npos);
    EXPECT_TRUE(prometheus_output.find("sarafu_security_alerts") != std::string::npos);
}

TEST_F(SecurityMonitorTest, SingletonInstance) {
    SecurityMonitor& monitor1 = SecurityMonitor::instance();
    SecurityMonitor& monitor2 = SecurityMonitor::instance();
    
    // Should be the same instance
    EXPECT_EQ(&monitor1, &monitor2);
}
