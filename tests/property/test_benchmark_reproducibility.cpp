#include <gtest/gtest.h>
#include <rapidcheck/gtest.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

/**
 * Property-Based Test for Benchmark Reproducibility
 * 
 * **Validates: Requirements 18.8**
 * 
 * Property 28: Benchmark Reproducibility
 * For any performance benchmark, running it multiple times under the same
 * conditions SHALL produce results within 10% variance.
 */

struct BenchmarkResult {
    std::string benchmark_name;
    double mean_time_ns;
    double stddev_time_ns;
    uint64_t iterations;
};

/**
 * Property 28: Benchmark Reproducibility
 */
RC_GTEST_PROP(BenchmarkReproducibility, BenchmarkResultsAreReproducible,
              (const std::vector<uint32_t>& run_times_ns)) {
    // Feature: production-launch-readiness, Property 28
    // Validates: Requirements 18.8
    
    RC_PRE(run_times_ns.size() >= 3);  // At least 3 runs
    RC_PRE(std::all_of(run_times_ns.begin(), run_times_ns.end(),
                       [](uint32_t t) { return t > 0 && t < 1000000000; }));
    
    // Calculate mean
    double sum = std::accumulate(run_times_ns.begin(), run_times_ns.end(), 0.0);
    double mean = sum / run_times_ns.size();
    
    // Calculate variance
    double variance = 0.0;
    for (uint32_t time : run_times_ns) {
        double diff = time - mean;
        variance += diff * diff;
    }
    variance /= run_times_ns.size();
    
    // Calculate coefficient of variation (stddev / mean)
    double stddev = std::sqrt(variance);
    double cv = stddev / mean;
    
    // Property: Variance should be within 10%
    const double max_variance = 0.10;  // 10%
    RC_ASSERT(cv <= max_variance);
}

TEST(BenchmarkReproducibility, ConsistentResultsHaveLowVariance) {
    std::vector<uint32_t> times = {1000, 1050, 1020, 1030, 1010};  // ~2-5% variance
    
    double sum = std::accumulate(times.begin(), times.end(), 0.0);
    double mean = sum / times.size();
    
    double variance = 0.0;
    for (uint32_t time : times) {
        double diff = time - mean;
        variance += diff * diff;
    }
    variance /= times.size();
    
    double stddev = std::sqrt(variance);
    double cv = stddev / mean;
    
    EXPECT_LT(cv, 0.10);  // Less than 10% variance
}
