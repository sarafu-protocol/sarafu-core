/**
 * Cryptographic Operation Benchmarks
 * 
 * Benchmarks for Blake3 hashing, Ed25519 signature verification,
 * and BLS12-381 signature verification and aggregation.
 * 
 * Performance targets from whitepaper:
 * - Transaction throughput: ≥1000 TPS
 * - Block time: 2 seconds
 * - Instant finalization via Quorum Certificates
 * 
 * Validates: Requirements 18.2
 */

#include <benchmark/benchmark.h>
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/bls12_381.h"
#include <vector>
#include <random>

using namespace sarafu::crypto;

// ============================================================================
// Blake3 Hashing Benchmarks
// ============================================================================

/**
 * Benchmark Blake3 hashing throughput across different input sizes.
 * Tests from 64 bytes (small transaction) to 1MB (large block).
 */
static void BM_Blake3_Hash(benchmark::State& state) {
    // Generate random data of specified size
    size_t data_size = state.range(0);
    std::vector<uint8_t> data(data_size);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < data_size; ++i) {
        data[i] = dis(gen);
    }
    
    // Benchmark hashing
    for (auto _ : state) {
        auto hash = Blake3Hash::hash(data);
        benchmark::DoNotOptimize(hash);
    }
    
    // Report throughput in bytes per second
    state.SetBytesProcessed(state.iterations() * data_size);
}

// Blake3 benchmarks with varying input sizes
BENCHMARK(BM_Blake3_Hash)->Arg(64)->Unit(benchmark::kMicrosecond);           // Small transaction
BENCHMARK(BM_Blake3_Hash)->Arg(256)->Unit(benchmark::kMicrosecond);          // Average transaction
BENCHMARK(BM_Blake3_Hash)->Arg(1024)->Unit(benchmark::kMicrosecond);         // Large transaction (1KB)
BENCHMARK(BM_Blake3_Hash)->Arg(4096)->Unit(benchmark::kMicrosecond);         // Very large transaction (4KB)
BENCHMARK(BM_Blake3_Hash)->Arg(16384)->Unit(benchmark::kMicrosecond);        // Small block (16KB)
BENCHMARK(BM_Blake3_Hash)->Arg(65536)->Unit(benchmark::kMicrosecond);        // Medium block (64KB)
BENCHMARK(BM_Blake3_Hash)->Arg(262144)->Unit(benchmark::kMicrosecond);       // Large block (256KB)
BENCHMARK(BM_Blake3_Hash)->Arg(1048576)->Unit(benchmark::kMillisecond);      // Very large block (1MB)

// ============================================================================
// Ed25519 Signature Verification Benchmarks
// ============================================================================

/**
 * Benchmark Ed25519 signature verification performance.
 * Ed25519 is used for transaction signatures.
 */
static void BM_Ed25519_SignatureVerification(benchmark::State& state) {
    // Generate a keypair
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Create a test message (typical transaction size)
    std::vector<uint8_t> message(256);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < message.size(); ++i) {
        message[i] = dis(gen);
    }
    
    // Sign the message
    auto signature = Ed25519::sign(message, private_key);
    
    // Benchmark verification
    for (auto _ : state) {
        bool valid = Ed25519::verify(signature, message, public_key);
        benchmark::DoNotOptimize(valid);
    }
    
    // Report throughput in verifications per second
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Ed25519_SignatureVerification)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark Ed25519 signature generation performance.
 */
static void BM_Ed25519_SignatureGeneration(benchmark::State& state) {
    // Generate a keypair
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Create a test message
    std::vector<uint8_t> message(256);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < message.size(); ++i) {
        message[i] = dis(gen);
    }
    
    // Benchmark signing
    for (auto _ : state) {
        auto signature = Ed25519::sign(message, private_key);
        benchmark::DoNotOptimize(signature);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Ed25519_SignatureGeneration)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark batch Ed25519 signature verification.
 * Simulates verifying all transactions in a block.
 */
static void BM_Ed25519_BatchVerification(benchmark::State& state) {
    size_t num_signatures = state.range(0);
    
    // Generate keypairs and signatures
    std::vector<Ed25519_PublicKey> public_keys;
    std::vector<Ed25519_Signature> signatures;
    std::vector<std::vector<uint8_t>> messages;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < num_signatures; ++i) {
        auto [pub, priv] = Ed25519::generate_keypair();
        public_keys.push_back(pub);
        
        std::vector<uint8_t> message(256);
        for (size_t j = 0; j < message.size(); ++j) {
            message[j] = dis(gen);
        }
        messages.push_back(message);
        
        signatures.push_back(Ed25519::sign(message, priv));
    }
    
    // Benchmark batch verification
    for (auto _ : state) {
        size_t valid_count = 0;
        for (size_t i = 0; i < num_signatures; ++i) {
            if (Ed25519::verify(signatures[i], messages[i], public_keys[i])) {
                valid_count++;
            }
        }
        benchmark::DoNotOptimize(valid_count);
    }
    
    state.SetItemsProcessed(state.iterations() * num_signatures);
}

// Batch verification with varying transaction counts
BENCHMARK(BM_Ed25519_BatchVerification)->Arg(10)->Unit(benchmark::kMicrosecond);      // Small block
BENCHMARK(BM_Ed25519_BatchVerification)->Arg(100)->Unit(benchmark::kMicrosecond);     // Medium block
BENCHMARK(BM_Ed25519_BatchVerification)->Arg(1000)->Unit(benchmark::kMillisecond);    // Large block (1000 TPS target)
BENCHMARK(BM_Ed25519_BatchVerification)->Arg(2000)->Unit(benchmark::kMillisecond);    // 2-second block at 1000 TPS

// ============================================================================
// BLS12-381 Signature Verification Benchmarks
// ============================================================================

/**
 * Benchmark BLS12-381 signature verification performance.
 * BLS12-381 is used for validator signatures in consensus.
 */
static void BM_BLS12_381_SignatureVerification(benchmark::State& state) {
    // Generate a keypair
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    
    // Create a test message (block hash)
    std::vector<uint8_t> message(32);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < message.size(); ++i) {
        message[i] = dis(gen);
    }
    
    // Sign the message
    auto signature = BLS12_381::sign(message, private_key);
    
    // Benchmark verification
    for (auto _ : state) {
        bool valid = BLS12_381::verify(signature, message, public_key);
        benchmark::DoNotOptimize(valid);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BLS12_381_SignatureVerification)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark BLS12-381 signature generation performance.
 */
static void BM_BLS12_381_SignatureGeneration(benchmark::State& state) {
    // Generate a keypair
    auto [public_key, private_key] = BLS12_381::generate_keypair();
    
    // Create a test message
    std::vector<uint8_t> message(32);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < message.size(); ++i) {
        message[i] = dis(gen);
    }
    
    // Benchmark signing
    for (auto _ : state) {
        auto signature = BLS12_381::sign(message, private_key);
        benchmark::DoNotOptimize(signature);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BLS12_381_SignatureGeneration)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark BLS12-381 signature aggregation.
 * This is critical for Quorum Certificate construction.
 */
static void BM_BLS12_381_SignatureAggregation(benchmark::State& state) {
    size_t num_signatures = state.range(0);
    
    // Generate signatures from multiple validators
    std::vector<BLS12_381_Signature> signatures;
    std::vector<uint8_t> message(32);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < message.size(); ++i) {
        message[i] = dis(gen);
    }
    
    for (size_t i = 0; i < num_signatures; ++i) {
        auto [pub, priv] = BLS12_381::generate_keypair();
        signatures.push_back(BLS12_381::sign(message, priv));
    }
    
    // Benchmark aggregation
    for (auto _ : state) {
        auto aggregated = BLS12_381::aggregate(signatures);
        benchmark::DoNotOptimize(aggregated);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Aggregation with varying validator counts
BENCHMARK(BM_BLS12_381_SignatureAggregation)->Arg(10)->Unit(benchmark::kMicrosecond);    // Small validator set
BENCHMARK(BM_BLS12_381_SignatureAggregation)->Arg(50)->Unit(benchmark::kMicrosecond);    // Medium validator set
BENCHMARK(BM_BLS12_381_SignatureAggregation)->Arg(100)->Unit(benchmark::kMicrosecond);   // Large validator set (2/3 of 150)
BENCHMARK(BM_BLS12_381_SignatureAggregation)->Arg(150)->Unit(benchmark::kMicrosecond);   // Full validator set
BENCHMARK(BM_BLS12_381_SignatureAggregation)->Arg(334)->Unit(benchmark::kMicrosecond);   // 2/3 of max (500)

/**
 * Benchmark BLS12-381 aggregated signature verification.
 * This verifies a Quorum Certificate.
 */
static void BM_BLS12_381_AggregatedVerification(benchmark::State& state) {
    size_t num_validators = state.range(0);
    
    // Generate keypairs and signatures
    std::vector<BLS12_381_PublicKey> public_keys;
    std::vector<BLS12_381_Signature> signatures;
    std::vector<uint8_t> message(32);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < message.size(); ++i) {
        message[i] = dis(gen);
    }
    
    for (size_t i = 0; i < num_validators; ++i) {
        auto [pub, priv] = BLS12_381::generate_keypair();
        public_keys.push_back(pub);
        signatures.push_back(BLS12_381::sign(message, priv));
    }
    
    // Aggregate signatures
    auto aggregated_signature = BLS12_381::aggregate(signatures);
    
    // Benchmark aggregated verification
    for (auto _ : state) {
        bool valid = BLS12_381::verify_aggregated(aggregated_signature, message, public_keys);
        benchmark::DoNotOptimize(valid);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Aggregated verification with varying validator counts
BENCHMARK(BM_BLS12_381_AggregatedVerification)->Arg(10)->Unit(benchmark::kMicrosecond);    // Small validator set
BENCHMARK(BM_BLS12_381_AggregatedVerification)->Arg(50)->Unit(benchmark::kMicrosecond);    // Medium validator set
BENCHMARK(BM_BLS12_381_AggregatedVerification)->Arg(100)->Unit(benchmark::kMicrosecond);   // Large validator set (2/3 of 150)
BENCHMARK(BM_BLS12_381_AggregatedVerification)->Arg(150)->Unit(benchmark::kMicrosecond);   // Full validator set
BENCHMARK(BM_BLS12_381_AggregatedVerification)->Arg(334)->Unit(benchmark::kMicrosecond);   // 2/3 of max (500)

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
