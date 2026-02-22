/**
 * Transaction Processing Benchmarks
 * 
 * Benchmarks for transaction validation, serialization, and deserialization.
 * 
 * Performance targets from whitepaper:
 * - Transaction throughput: ≥1000 TPS
 * - Block time: 2 seconds
 * - Full blocks with 2000 transactions (1000 TPS * 2 seconds)
 * 
 * Validates: Requirements 18.3
 */

#include <benchmark/benchmark.h>
#include "sarafu/state/transaction.h"
#include "sarafu/state/transaction_validator.h"
#include "sarafu/state/account_manager.h"
#include "sarafu/crypto/ed25519.h"
#include <vector>
#include <random>

using namespace sarafu::state;
using namespace sarafu::crypto;

// ============================================================================
// Test Data Generation Helpers
// ============================================================================

/**
 * Generate a random address for testing.
 */
Address generate_random_address() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    std::vector<uint8_t> addr_bytes(32);
    for (size_t i = 0; i < 32; ++i) {
        addr_bytes[i] = dis(gen);
    }
    
    return Address(addr_bytes);
}

/**
 * Generate a random transaction with valid signature.
 */
Transaction generate_random_transaction(
    const Ed25519_PublicKey& public_key,
    const Ed25519_PrivateKey& private_key,
    uint32_t chain_id = 1
) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> amount_dis(1000, 1000000);
    std::uniform_int_distribution<uint64_t> nonce_dis(0, 1000000);
    std::uniform_int_distribution<uint64_t> fee_dis(100, 10000);
    std::uniform_int_distribution<uint64_t> gas_dis(21000, 100000);
    
    // Create address from public key
    auto pub_bytes = public_key.serialize();
    auto pub_hash = Blake3Hash::hash(pub_bytes);
    Address from(pub_hash.serialize());
    
    Address to = generate_random_address();
    
    Transaction tx(
        from,
        to,
        amount_dis(gen),
        nonce_dis(gen),
        fee_dis(gen),
        gas_dis(gen),
        chain_id
    );
    
    // Sign the transaction
    tx.sign(private_key);
    
    return tx;
}

// ============================================================================
// Transaction Validation Benchmarks
// ============================================================================

/**
 * Benchmark transaction signature verification.
 * This is the most expensive part of transaction validation.
 */
static void BM_Transaction_SignatureVerification(benchmark::State& state) {
    // Generate a keypair
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Create a signed transaction
    Transaction tx = generate_random_transaction(public_key, private_key);
    
    // Benchmark signature verification
    for (auto _ : state) {
        bool valid = tx.verify_signature(public_key);
        benchmark::DoNotOptimize(valid);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Transaction_SignatureVerification)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark full transaction validation including all checks.
 * This includes signature verification, nonce checking, balance validation, etc.
 */
static void BM_Transaction_FullValidation(benchmark::State& state) {
    // Generate a keypair
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Create address from public key
    auto pub_bytes = public_key.serialize();
    auto pub_hash = Blake3Hash::hash(pub_bytes);
    Address from(pub_hash.serialize());
    
    // Create account manager with test account
    AccountManager account_manager;
    account_manager.create_account(from, 1000000000); // 1 billion SAR
    
    // Create validator
    TransactionValidator validator(1); // chain_id = 1
    
    // Create a signed transaction
    Transaction tx = generate_random_transaction(public_key, private_key, 1);
    
    // Benchmark full validation
    for (auto _ : state) {
        auto result = validator.validate_transaction(tx, public_key, account_manager);
        benchmark::DoNotOptimize(result);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Transaction_FullValidation)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark batch transaction validation.
 * Simulates validating all transactions in a block.
 */
static void BM_Transaction_BatchValidation(benchmark::State& state) {
    size_t num_transactions = state.range(0);
    
    // Generate keypairs and transactions
    std::vector<Ed25519_PublicKey> public_keys;
    std::vector<Transaction> transactions;
    
    AccountManager account_manager;
    TransactionValidator validator(1);
    
    for (size_t i = 0; i < num_transactions; ++i) {
        auto [pub, priv] = Ed25519::generate_keypair();
        public_keys.push_back(pub);
        
        // Create address and fund account
        auto pub_bytes = pub.serialize();
        auto pub_hash = Blake3Hash::hash(pub_bytes);
        Address from(pub_hash.serialize());
        account_manager.create_account(from, 1000000000);
        
        transactions.push_back(generate_random_transaction(pub, priv, 1));
    }
    
    // Benchmark batch validation
    for (auto _ : state) {
        size_t valid_count = 0;
        for (size_t i = 0; i < num_transactions; ++i) {
            auto result = validator.validate_transaction(
                transactions[i],
                public_keys[i],
                account_manager
            );
            if (!result.has_value()) {
                valid_count++;
            }
        }
        benchmark::DoNotOptimize(valid_count);
    }
    
    state.SetItemsProcessed(state.iterations() * num_transactions);
}

// Batch validation with varying transaction counts
BENCHMARK(BM_Transaction_BatchValidation)->Arg(10)->Unit(benchmark::kMicrosecond);      // Small block
BENCHMARK(BM_Transaction_BatchValidation)->Arg(100)->Unit(benchmark::kMicrosecond);     // Medium block
BENCHMARK(BM_Transaction_BatchValidation)->Arg(1000)->Unit(benchmark::kMillisecond);    // Large block (1000 TPS target)
BENCHMARK(BM_Transaction_BatchValidation)->Arg(2000)->Unit(benchmark::kMillisecond);    // 2-second block at 1000 TPS

/**
 * Benchmark transaction validation throughput.
 * Measures how many transactions per second can be validated.
 */
static void BM_Transaction_ValidationThroughput(benchmark::State& state) {
    // Generate a keypair
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Create address and fund account
    auto pub_bytes = public_key.serialize();
    auto pub_hash = Blake3Hash::hash(pub_bytes);
    Address from(pub_hash.serialize());
    
    AccountManager account_manager;
    account_manager.create_account(from, 1000000000);
    
    TransactionValidator validator(1);
    
    // Create a signed transaction
    Transaction tx = generate_random_transaction(public_key, private_key, 1);
    
    // Benchmark validation throughput
    for (auto _ : state) {
        auto result = validator.validate_transaction(tx, public_key, account_manager);
        benchmark::DoNotOptimize(result);
    }
    
    // Report throughput in transactions per second
    state.SetItemsProcessed(state.iterations());
    state.SetLabel("TPS");
}

BENCHMARK(BM_Transaction_ValidationThroughput)->Unit(benchmark::kMicrosecond);

// ============================================================================
// Transaction Serialization Benchmarks
// ============================================================================

/**
 * Benchmark transaction serialization (with signature).
 * This is used for network transmission and storage.
 */
static void BM_Transaction_Serialization(benchmark::State& state) {
    // Generate a keypair and transaction
    auto [public_key, private_key] = Ed25519::generate_keypair();
    Transaction tx = generate_random_transaction(public_key, private_key);
    
    // Benchmark serialization
    for (auto _ : state) {
        auto serialized = tx.serialize();
        benchmark::DoNotOptimize(serialized);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Transaction_Serialization)->Unit(benchmark::kNanosecond);

/**
 * Benchmark transaction serialization for signing (without signature).
 * This is used to compute the transaction hash for signing.
 */
static void BM_Transaction_SerializationForSigning(benchmark::State& state) {
    // Generate a keypair and transaction
    auto [public_key, private_key] = Ed25519::generate_keypair();
    Transaction tx = generate_random_transaction(public_key, private_key);
    
    // Benchmark serialization for signing
    for (auto _ : state) {
        auto serialized = tx.serialize_for_signing();
        benchmark::DoNotOptimize(serialized);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Transaction_SerializationForSigning)->Unit(benchmark::kNanosecond);

/**
 * Benchmark batch transaction serialization.
 * Simulates serializing all transactions in a block for network transmission.
 */
static void BM_Transaction_BatchSerialization(benchmark::State& state) {
    size_t num_transactions = state.range(0);
    
    // Generate transactions
    std::vector<Transaction> transactions;
    for (size_t i = 0; i < num_transactions; ++i) {
        auto [pub, priv] = Ed25519::generate_keypair();
        transactions.push_back(generate_random_transaction(pub, priv));
    }
    
    // Benchmark batch serialization
    for (auto _ : state) {
        std::vector<std::vector<uint8_t>> serialized_txs;
        serialized_txs.reserve(num_transactions);
        
        for (const auto& tx : transactions) {
            serialized_txs.push_back(tx.serialize());
        }
        
        benchmark::DoNotOptimize(serialized_txs);
    }
    
    state.SetItemsProcessed(state.iterations() * num_transactions);
}

// Batch serialization with varying transaction counts
BENCHMARK(BM_Transaction_BatchSerialization)->Arg(10)->Unit(benchmark::kMicrosecond);      // Small block
BENCHMARK(BM_Transaction_BatchSerialization)->Arg(100)->Unit(benchmark::kMicrosecond);     // Medium block
BENCHMARK(BM_Transaction_BatchSerialization)->Arg(1000)->Unit(benchmark::kMicrosecond);    // Large block
BENCHMARK(BM_Transaction_BatchSerialization)->Arg(2000)->Unit(benchmark::kMillisecond);    // 2-second block

// ============================================================================
// Transaction Deserialization Benchmarks
// ============================================================================

/**
 * Benchmark transaction deserialization.
 * This is used when receiving transactions from the network.
 */
static void BM_Transaction_Deserialization(benchmark::State& state) {
    // Generate a keypair and transaction
    auto [public_key, private_key] = Ed25519::generate_keypair();
    Transaction tx = generate_random_transaction(public_key, private_key);
    
    // Serialize the transaction
    auto serialized = tx.serialize();
    
    // Benchmark deserialization
    for (auto _ : state) {
        auto deserialized = Transaction::deserialize(serialized);
        benchmark::DoNotOptimize(deserialized);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Transaction_Deserialization)->Unit(benchmark::kNanosecond);

/**
 * Benchmark batch transaction deserialization.
 * Simulates deserializing all transactions in a received block.
 */
static void BM_Transaction_BatchDeserialization(benchmark::State& state) {
    size_t num_transactions = state.range(0);
    
    // Generate and serialize transactions
    std::vector<std::vector<uint8_t>> serialized_txs;
    for (size_t i = 0; i < num_transactions; ++i) {
        auto [pub, priv] = Ed25519::generate_keypair();
        Transaction tx = generate_random_transaction(pub, priv);
        serialized_txs.push_back(tx.serialize());
    }
    
    // Benchmark batch deserialization
    for (auto _ : state) {
        std::vector<Transaction> transactions;
        transactions.reserve(num_transactions);
        
        for (const auto& serialized : serialized_txs) {
            transactions.push_back(Transaction::deserialize(serialized));
        }
        
        benchmark::DoNotOptimize(transactions);
    }
    
    state.SetItemsProcessed(state.iterations() * num_transactions);
}

// Batch deserialization with varying transaction counts
BENCHMARK(BM_Transaction_BatchDeserialization)->Arg(10)->Unit(benchmark::kMicrosecond);      // Small block
BENCHMARK(BM_Transaction_BatchDeserialization)->Arg(100)->Unit(benchmark::kMicrosecond);     // Medium block
BENCHMARK(BM_Transaction_BatchDeserialization)->Arg(1000)->Unit(benchmark::kMicrosecond);    // Large block
BENCHMARK(BM_Transaction_BatchDeserialization)->Arg(2000)->Unit(benchmark::kMillisecond);    // 2-second block

// ============================================================================
// Transaction Hash Computation Benchmarks
// ============================================================================

/**
 * Benchmark transaction hash computation.
 * This is used for signing and verification.
 */
static void BM_Transaction_HashComputation(benchmark::State& state) {
    // Generate a keypair and transaction
    auto [public_key, private_key] = Ed25519::generate_keypair();
    Transaction tx = generate_random_transaction(public_key, private_key);
    
    // Benchmark hash computation
    for (auto _ : state) {
        auto hash = tx.hash();
        benchmark::DoNotOptimize(hash);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Transaction_HashComputation)->Unit(benchmark::kNanosecond);

/**
 * Benchmark transaction signing (hash + signature generation).
 * This is the complete signing operation.
 */
static void BM_Transaction_Signing(benchmark::State& state) {
    // Generate a keypair
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Benchmark signing
    for (auto _ : state) {
        Transaction tx = generate_random_transaction(public_key, private_key);
        benchmark::DoNotOptimize(tx);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Transaction_Signing)->Unit(benchmark::kMicrosecond);

// ============================================================================
// End-to-End Transaction Processing Benchmarks
// ============================================================================

/**
 * Benchmark complete transaction processing pipeline.
 * This includes: deserialization -> validation -> execution simulation.
 */
static void BM_Transaction_EndToEndProcessing(benchmark::State& state) {
    // Generate a keypair
    auto [public_key, private_key] = Ed25519::generate_keypair();
    
    // Create address and fund account
    auto pub_bytes = public_key.serialize();
    auto pub_hash = Blake3Hash::hash(pub_bytes);
    Address from(pub_hash.serialize());
    
    AccountManager account_manager;
    account_manager.create_account(from, 1000000000);
    
    TransactionValidator validator(1);
    
    // Create and serialize a transaction
    Transaction tx = generate_random_transaction(public_key, private_key, 1);
    auto serialized = tx.serialize();
    
    // Benchmark end-to-end processing
    for (auto _ : state) {
        // 1. Deserialize
        auto deserialized_tx = Transaction::deserialize(serialized);
        
        // 2. Validate
        auto validation_result = validator.validate_transaction(
            deserialized_tx,
            public_key,
            account_manager
        );
        
        // 3. Simulate execution (just check validation passed)
        bool processed = !validation_result.has_value();
        
        benchmark::DoNotOptimize(processed);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Transaction_EndToEndProcessing)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark block-level transaction processing throughput.
 * Measures the maximum TPS the system can handle.
 */
static void BM_Transaction_BlockProcessingThroughput(benchmark::State& state) {
    size_t num_transactions = state.range(0);
    
    // Generate keypairs and transactions
    std::vector<Ed25519_PublicKey> public_keys;
    std::vector<std::vector<uint8_t>> serialized_txs;
    
    AccountManager account_manager;
    TransactionValidator validator(1);
    
    for (size_t i = 0; i < num_transactions; ++i) {
        auto [pub, priv] = Ed25519::generate_keypair();
        public_keys.push_back(pub);
        
        // Create address and fund account
        auto pub_bytes = pub.serialize();
        auto pub_hash = Blake3Hash::hash(pub_bytes);
        Address from(pub_hash.serialize());
        account_manager.create_account(from, 1000000000);
        
        Transaction tx = generate_random_transaction(pub, priv, 1);
        serialized_txs.push_back(tx.serialize());
    }
    
    // Benchmark block processing
    for (auto _ : state) {
        size_t processed_count = 0;
        
        for (size_t i = 0; i < num_transactions; ++i) {
            // Deserialize
            auto tx = Transaction::deserialize(serialized_txs[i]);
            
            // Validate
            auto result = validator.validate_transaction(
                tx,
                public_keys[i],
                account_manager
            );
            
            if (!result.has_value()) {
                processed_count++;
            }
        }
        
        benchmark::DoNotOptimize(processed_count);
    }
    
    // Report throughput in transactions per second
    state.SetItemsProcessed(state.iterations() * num_transactions);
    
    // Calculate and display TPS
    double elapsed_seconds = state.iterations() * 
        (static_cast<double>(state.range(0)) / state.items_processed());
    double tps = state.items_processed() / elapsed_seconds;
    state.SetLabel("TPS: " + std::to_string(static_cast<int>(tps)));
}

// Block processing with target throughput
BENCHMARK(BM_Transaction_BlockProcessingThroughput)->Arg(1000)->Unit(benchmark::kMillisecond);  // 1000 TPS target
BENCHMARK(BM_Transaction_BlockProcessingThroughput)->Arg(2000)->Unit(benchmark::kMillisecond);  // 2-second block
BENCHMARK(BM_Transaction_BlockProcessingThroughput)->Arg(5000)->Unit(benchmark::kMillisecond);  // Stress test

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
