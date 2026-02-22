#include <gtest/gtest.h>
#include "sarafu/state/adaptive_inflation.h"
#include <cmath>

using namespace sarafu::state;

class AdaptiveInflationTest : public ::testing::Test {
protected:
    AdaptiveInflationEngine engine;
};

TEST_F(AdaptiveInflationTest, InitialState) {
    // Initial inflation should be at target minimum
    EXPECT_DOUBLE_EQ(engine.current_inflation_rate(), 
                     AdaptiveInflationParameters::TARGET_INFLATION_MIN);
}

TEST_F(AdaptiveInflationTest, TargetInflationLowStaking) {
    // When staking ratio is below target (65%), inflation should increase
    double low_staking = 0.50;  // 50% staking
    double target = engine.calculate_target_inflation(low_staking);
    
    // Target should be above base inflation
    double base = (AdaptiveInflationParameters::TARGET_INFLATION_MIN + 
                  AdaptiveInflationParameters::TARGET_INFLATION_MAX) / 2.0;
    EXPECT_GT(target, base);
    
    // Should be within dynamic band
    EXPECT_GE(target, AdaptiveInflationParameters::DYNAMIC_BAND_MIN);
    EXPECT_LE(target, AdaptiveInflationParameters::DYNAMIC_BAND_MAX);
}

TEST_F(AdaptiveInflationTest, TargetInflationHighStaking) {
    // When staking ratio is above target (65%), inflation should decrease
    double high_staking = 0.80;  // 80% staking
    double target = engine.calculate_target_inflation(high_staking);
    
    // Target should be below base inflation
    double base = (AdaptiveInflationParameters::TARGET_INFLATION_MIN + 
                  AdaptiveInflationParameters::TARGET_INFLATION_MAX) / 2.0;
    EXPECT_LT(target, base);
    
    // Should be within dynamic band
    EXPECT_GE(target, AdaptiveInflationParameters::DYNAMIC_BAND_MIN);
    EXPECT_LE(target, AdaptiveInflationParameters::DYNAMIC_BAND_MAX);
}

TEST_F(AdaptiveInflationTest, TargetInflationAtTarget) {
    // When staking ratio is at target (65%), inflation should be near base
    double target_staking = 0.65;
    double target = engine.calculate_target_inflation(target_staking);
    
    double base = (AdaptiveInflationParameters::TARGET_INFLATION_MIN + 
                  AdaptiveInflationParameters::TARGET_INFLATION_MAX) / 2.0;
    
    // Should be very close to base inflation
    EXPECT_NEAR(target, base, 0.001);
}

TEST_F(AdaptiveInflationTest, ExponentialSmoothing) {
    // Test that inflation adjusts gradually with smoothing
    double staking_ratio = 0.50;  // Low staking to trigger increase
    
    double initial_rate = engine.current_inflation_rate();
    double rate1 = engine.update_inflation_rate(1, staking_ratio);
    double rate2 = engine.update_inflation_rate(2, staking_ratio);
    
    // Rate should increase gradually
    EXPECT_GT(rate1, initial_rate);
    EXPECT_GT(rate2, rate1);
    
    // Changes should be small (smoothed)
    double change1 = rate1 - initial_rate;
    double change2 = rate2 - rate1;
    EXPECT_LT(change1, 0.01);  // Less than 1% change per epoch
    EXPECT_LT(change2, 0.01);
}

TEST_F(AdaptiveInflationTest, AbsoluteHardCap) {
    // Test that inflation never exceeds 8% hard cap
    // Simulate extreme low staking for many epochs
    for (uint64_t epoch = 1; epoch <= 200; ++epoch) {
        double rate = engine.update_inflation_rate(epoch, 0.10);  // 10% staking
        EXPECT_LE(rate, AdaptiveInflationParameters::ABSOLUTE_HARD_CAP);
    }
}

TEST_F(AdaptiveInflationTest, MaxAnnualChange) {
    // Test that annual change never exceeds 1%
    double initial_rate = engine.current_inflation_rate();
    
    // Try to force large change with extreme staking ratio
    double rate = engine.update_inflation_rate(1, 0.05);  // Very low staking
    
    double change = std::abs(rate - initial_rate);
    EXPECT_LE(change, AdaptiveInflationParameters::MAX_ANNUAL_CHANGE);
}

TEST_F(AdaptiveInflationTest, DynamicBandConstraints) {
    // Test that inflation stays within 3-6% dynamic band
    for (uint64_t epoch = 1; epoch <= 100; ++epoch) {
        // Vary staking ratio
        double staking = 0.30 + (epoch % 50) * 0.01;
        double rate = engine.update_inflation_rate(epoch, staking);
        
        EXPECT_GE(rate, AdaptiveInflationParameters::DYNAMIC_BAND_MIN);
        EXPECT_LE(rate, AdaptiveInflationParameters::DYNAMIC_BAND_MAX);
    }
}

TEST_F(AdaptiveInflationTest, ValidationSuccess) {
    double current_rate = 0.04;  // 4%
    double proposed_rate = 0.045;  // 4.5%
    
    EXPECT_TRUE(engine.validate_inflation_rate(proposed_rate, current_rate));
}

TEST_F(AdaptiveInflationTest, ValidationFailsHardCap) {
    double current_rate = 0.04;
    double proposed_rate = 0.09;  // Exceeds 8% hard cap
    
    EXPECT_FALSE(engine.validate_inflation_rate(proposed_rate, current_rate));
}

TEST_F(AdaptiveInflationTest, ValidationFailsMaxChange) {
    double current_rate = 0.04;
    double proposed_rate = 0.055;  // 1.5% change, exceeds 1% limit
    
    EXPECT_FALSE(engine.validate_inflation_rate(proposed_rate, current_rate));
}

TEST_F(AdaptiveInflationTest, ValidationFailsDynamicBand) {
    double current_rate = 0.04;
    double proposed_rate = 0.025;  // Below 3% dynamic band minimum
    
    EXPECT_FALSE(engine.validate_inflation_rate(proposed_rate, current_rate));
}

TEST_F(AdaptiveInflationTest, ConvergenceToTarget) {
    // Test that inflation converges to target over 100 epochs
    double staking_ratio = 0.65;  // At target
    
    for (uint64_t epoch = 1; epoch <= 100; ++epoch) {
        engine.update_inflation_rate(epoch, staking_ratio);
    }
    
    // After 100 epochs, should be very close to target
    double final_rate = engine.current_inflation_rate();
    double target_rate = engine.target_inflation_rate();
    
    EXPECT_NEAR(final_rate, target_rate, 0.001);
}

TEST_F(AdaptiveInflationTest, ResponsivenessToStakingChanges) {
    // Test that inflation responds to staking ratio changes
    
    // Start with low staking
    for (uint64_t epoch = 1; epoch <= 50; ++epoch) {
        engine.update_inflation_rate(epoch, 0.40);
    }
    double rate_low_staking = engine.current_inflation_rate();
    
    // Switch to high staking
    for (uint64_t epoch = 51; epoch <= 100; ++epoch) {
        engine.update_inflation_rate(epoch, 0.80);
    }
    double rate_high_staking = engine.current_inflation_rate();
    
    // Rate should decrease when staking increases
    EXPECT_LT(rate_high_staking, rate_low_staking);
}
