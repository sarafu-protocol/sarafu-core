/**
 * Database Operation Benchmarks
 * 
 * Benchmarks for RocksDB read/write latency and batch write throughput.
 * These benchmarks validate that database operations don't become a bottleneck
 * for the target throughput of ≥1000 TPS.
 * 
 * Performance targets from whitepaper:
 * - Transaction throughput: ≥1000 TPS
 * - Block time: 2 seconds
 * - Disk I/O: <100 MB/s sustained
 * 
 * Validates: Requirements 18.5
 */

#include <benchmark/benchmark.h>
#include "sarafu/storage/database.h"
#include "sarafu/storage/state_storage.h"
#include "sarafu/state/account.h"
#include "sarafu/state/transaction.h"
#include "sarafu/consensus/block.h"
#include "sarafu/crypto/blake3_hash.h"
#include <vector>
#include <random>
#include <filesystem>
#include <memory>

using namespace sarafu::storage;
using namespace sarafu::state;
using namespace sarafu::consensus;
using namespace sarafu::crypto;

// ============================================================================
// Test Data Generation Utilities
// ============================================================================

/**
 * Generate random key of specified size.
 */
std::vector<uint8_t> generate_random_key(size_t size) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    std::vector<uint8_t> key(size);
    for (size_t i = 0; i < size; ++i) {
        key[i] = dis(gen);
    }
    return key;
}

/**
 * Generate random value of specified size.
 */
std::vector<uint8_t> generate_random_value(size_t size) {
    return generate_random_key(size);
}

/**
 * Generate random address.
 */
Address generate_random_address() {
    auto key_bytes = generate_random_key(32);
    return Address(key_bytes);
}

/**
 * Generate random account with balance and nonce.
 */
Account generate_random_account() {
    auto address = generate_random_address();
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> balance_dis(1000, 1000000);
    static std::uniform_int_distribution<uint64_t> nonce_dis(0, 1000);
    
    Account account;
    account.address = address;
    account.balance = balance_dis(gen);
    account.nonce = nonce_dis(gen);
    return account;
}

/**
 * Create a temporary database for benchmarking.
 */
std::unique_ptr<Database> create_temp_database() {
    static int counter = 0;
    std::string path = "/tmp/sarafu_bench_db_" + std::to_string(counter++);
    
    // Remove existing database if present
    std::filesystem::remove_all(path);
    
    auto result = Database::open(path);
    if (result.is_error()) {
        throw std::runtime_error("Failed to open database: " + result.error());
    }
    
    return std::move(result.value());
}

// ============================================================================
// Single Read/Write Latency Benchmarks
// ============================================================================

/**
 * Benchmark single key-value write latency.
 * Measures the time to write a single key-value pair to the database.
 */
static void BM_Database_SingleWrite(benchmark::State& state) {
    auto db = create_temp_database();
    size_t value_size = state.range(0);
    
    for (auto _ : state) {
        auto key = generate_random_key(32);
        auto value = generate_random_value(value_size);
        
        auto result = db->put(ColumnFamily::Accounts, key, value);
        if (result.is_error()) {
            state.SkipWithError(result.error().c_str());
            return;
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Single write with varying value sizes
BENCHMARK(BM_Database_SingleWrite)->Arg(64)->Unit(benchmark::kMicrosecond);      // Small value
BENCHMARK(BM_Database_SingleWrite)->Arg(256)->Unit(benchmark::kMicrosecond);     // Account state size
BENCHMARK(BM_Database_SingleWrite)->Arg(1024)->Unit(benchmark::kMicrosecond);    // Transaction size
BENCHMARK(BM_Database_SingleWrite)->Arg(4096)->Unit(benchmark::kMicrosecond);    // Large value

/**
 * Benchmark single key-value read latency.
 * Measures the time to read a single key-value pair from the database.
 */
static void BM_Database_SingleRead(benchmark::State& state) {
    auto db = create_temp_database();
    size_t value_size = state.range(0);
    
    // Pre-populate database with keys
    std::vector<std::vector<uint8_t>> keys;
    for (int i = 0; i < 1000; ++i) {
        auto key = generate_random_key(32);
        auto value = generate_random_value(value_size);
        db->put(ColumnFamily::Accounts, key, value);
        keys.push_back(key);
    }
    
    size_t key_index = 0;
    for (auto _ : state) {
        auto result = db->get(ColumnFamily::Accounts, keys[key_index % keys.size()]);
        if (result.is_error()) {
            state.SkipWithError(result.error().c_str());
            return;
        }
        benchmark::DoNotOptimize(result.value());
        key_index++;
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Single read with varying value sizes
BENCHMARK(BM_Database_SingleRead)->Arg(64)->Unit(benchmark::kMicrosecond);       // Small value
BENCHMARK(BM_Database_SingleRead)->Arg(256)->Unit(benchmark::kMicrosecond);      // Account state size
BENCHMARK(BM_Database_SingleRead)->Arg(1024)->Unit(benchmark::kMicrosecond);     // Transaction size
BENCHMARK(BM_Database_SingleRead)->Arg(4096)->Unit(benchmark::kMicrosecond);     // Large value

// ============================================================================
// Batch Write Throughput Benchmarks
// ============================================================================

/**
 * Benchmark batch write throughput.
 * Measures the throughput of writing multiple key-value pairs in a single batch.
 */
static void BM_Database_BatchWrite(benchmark::State& state) {
    auto db = create_temp_database();
    size_t batch_size = state.range(0);
    size_t value_size = 256;  // Typical account state size
    
    for (auto _ : state) {
        BatchWrite batch;
        
        // Prepare batch operations
        for (size_t i = 0; i < batch_size; ++i) {
            auto key = generate_random_key(32);
            auto value = generate_random_value(value_size);
            batch.put(ColumnFamily::Accounts, key, value);
        }
        
        // Execute batch
        auto result = db->write_batch(batch);
        if (result.is_error()) {
            state.SkipWithError(result.error().c_str());
            return;
        }
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
    state.SetBytesProcessed(state.iterations() * batch_size * (32 + value_size));
}

// Batch write with varying batch sizes
BENCHMARK(BM_Database_BatchWrite)->Arg(10)->Unit(benchmark::kMicrosecond);       // Small batch
BENCHMARK(BM_Database_BatchWrite)->Arg(100)->Unit(benchmark::kMicrosecond);      // Medium batch
BENCHMARK(BM_Database_BatchWrite)->Arg(1000)->Unit(benchmark::kMillisecond);     // Large batch (1000 TPS target)
BENCHMARK(BM_Database_BatchWrite)->Arg(2000)->Unit(benchmark::kMillisecond);     // 2-second block at 1000 TPS

/**
 * Benchmark batch write throughput in operations per second.
 * This measures the sustained throughput for batch writes.
 */
static void BM_Database_BatchWriteThroughput(benchmark::State& state) {
    auto db = create_temp_database();
    size_t batch_size = state.range(0);
    size_t value_size = 256;
    
    uint64_t total_operations = 0;
    
    for (auto _ : state) {
        BatchWrite batch;
        
        for (size_t i = 0; i < batch_size; ++i) {
            auto key = generate_random_key(32);
            auto value = generate_random_value(value_size);
            batch.put(ColumnFamily::Accounts, key, value);
        }
        
        auto result = db->write_batch(batch);
        if (result.is_error()) {
            state.SkipWithError(result.error().c_str());
            return;
        }
        
        total_operations += batch_size;
    }
    
    state.SetItemsProcessed(total_operations);
    state.counters["ops_per_sec"] = benchmark::Counter(
        total_operations, 
        benchmark::Counter::kIsRate
    );
}

// Throughput measurement for different batch sizes
BENCHMARK(BM_Database_BatchWriteThroughput)->Arg(100)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Database_BatchWriteThroughput)->Arg(1000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Database_BatchWriteThroughput)->Arg(2000)->Unit(benchmark::kMillisecond);

// ============================================================================
// Batch Read Throughput Benchmarks
// ============================================================================

/**
 * Benchmark batch read throughput.
 * Measures the throughput of reading multiple key-value pairs sequentially.
 */
static void BM_Database_BatchRead(benchmark::State& state) {
    auto db = create_temp_database();
    size_t batch_size = state.range(0);
    size_t value_size = 256;
    
    // Pre-populate database
    std::vector<std::vector<uint8_t>> keys;
    for (size_t i = 0; i < batch_size; ++i) {
        auto key = generate_random_key(32);
        auto value = generate_random_value(value_size);
        db->put(ColumnFamily::Accounts, key, value);
        keys.push_back(key);
    }
    
    for (auto _ : state) {
        for (const auto& key : keys) {
            auto result = db->get(ColumnFamily::Accounts, key);
            if (result.is_error()) {
                state.SkipWithError(result.error().c_str());
                return;
            }
            benchmark::DoNotOptimize(result.value());
        }
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
    state.SetBytesProcessed(state.iterations() * batch_size * (32 + value_size));
}

// Batch read with varying batch sizes
BENCHMARK(BM_Database_BatchRead)->Arg(10)->Unit(benchmark::kMicrosecond);        // Small batch
BENCHMARK(BM_Database_BatchRead)->Arg(100)->Unit(benchmark::kMicrosecond);       // Medium batch
BENCHMARK(BM_Database_BatchRead)->Arg(1000)->Unit(benchmark::kMillisecond);      // Large batch
BENCHMARK(BM_Database_BatchRead)->Arg(2000)->Unit(benchmark::kMillisecond);      // 2-second block

// ============================================================================
// State Query Performance Benchmarks
// ============================================================================

/**
 * Benchmark account lookup performance.
 * Measures the time to query account state by address.
 */
static void BM_StateStorage_AccountLookup(benchmark::State& state) {
    auto db = create_temp_database();
    auto storage = std::make_shared<StateStorage>(std::move(db));
    
    // Pre-populate with accounts
    std::vector<Address> addresses;
    for (int i = 0; i < 1000; ++i) {
        auto account = generate_random_account();
        storage->store_account(account);
        addresses.push_back(account.address);
    }
    
    size_t address_index = 0;
    for (auto _ : state) {
        auto result = storage->get_account(addresses[address_index % addresses.size()]);
        if (result.is_error()) {
            state.SkipWithError(result.error().c_str());
            return;
        }
        benchmark::DoNotOptimize(result.value());
        address_index++;
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_StateStorage_AccountLookup)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark account balance query performance.
 * Measures the time to query and extract balance from account state.
 */
static void BM_StateStorage_BalanceQuery(benchmark::State& state) {
    auto db = create_temp_database();
    auto storage = std::make_shared<StateStorage>(std::move(db));
    
    // Pre-populate with accounts
    std::vector<Address> addresses;
    for (int i = 0; i < 1000; ++i) {
        auto account = generate_random_account();
        storage->store_account(account);
        addresses.push_back(account.address);
    }
    
    size_t address_index = 0;
    for (auto _ : state) {
        auto result = storage->get_account(addresses[address_index % addresses.size()]);
        if (result.is_error()) {
            state.SkipWithError(result.error().c_str());
            return;
        }
        uint64_t balance = result.value().balance;
        benchmark::DoNotOptimize(balance);
        address_index++;
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_StateStorage_BalanceQuery)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark batch account storage.
 * Measures the throughput of storing multiple accounts atomically.
 */
static void BM_StateStorage_BatchAccountStorage(benchmark::State& state) {
    auto db = create_temp_database();
    auto storage = std::make_shared<StateStorage>(std::move(db));
    size_t batch_size = state.range(0);
    
    for (auto _ : state) {
        std::vector<Account> accounts;
        for (size_t i = 0; i < batch_size; ++i) {
            accounts.push_back(generate_random_account());
        }
        
        auto result = storage->store_accounts_batch(accounts);
        if (result.is_error()) {
            state.SkipWithError(result.error().c_str());
            return;
        }
    }
    
    state.SetItemsProcessed(state.iterations() * batch_size);
}

// Batch account storage with varying batch sizes
BENCHMARK(BM_StateStorage_BatchAccountStorage)->Arg(10)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_StateStorage_BatchAccountStorage)->Arg(100)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_StateStorage_BatchAccountStorage)->Arg(1000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_StateStorage_BatchAccountStorage)->Arg(2000)->Unit(benchmark::kMillisecond);

/**
 * Benchmark account existence check.
 * Measures the time to check if an account exists without retrieving full state.
 */
static void BM_StateStorage_AccountExists(benchmark::State& state) {
    auto db = create_temp_database();
    auto storage = std::make_shared<StateStorage>(std::move(db));
    
    // Pre-populate with accounts
    std::vector<Address> addresses;
    for (int i = 0; i < 1000; ++i) {
        auto account = generate_random_account();
        storage->store_account(account);
        addresses.push_back(account.address);
    }
    
    size_t address_index = 0;
    for (auto _ : state) {
        bool exists = storage->account_exists(addresses[address_index % addresses.size()]);
        benchmark::DoNotOptimize(exists);
        address_index++;
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_StateStorage_AccountExists)->Unit(benchmark::kMicrosecond);

// ============================================================================
// Mixed Read/Write Workload Benchmarks
// ============================================================================

/**
 * Benchmark mixed read/write workload.
 * Simulates realistic blockchain workload with 70% reads and 30% writes.
 */
static void BM_Database_MixedWorkload(benchmark::State& state) {
    auto db = create_temp_database();
    size_t num_keys = 1000;
    size_t value_size = 256;
    
    // Pre-populate database
    std::vector<std::vector<uint8_t>> keys;
    for (size_t i = 0; i < num_keys; ++i) {
        auto key = generate_random_key(32);
        auto value = generate_random_value(value_size);
        db->put(ColumnFamily::Accounts, key, value);
        keys.push_back(key);
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> op_dis(0, 99);
    std::uniform_int_distribution<size_t> key_dis(0, num_keys - 1);
    
    for (auto _ : state) {
        int op = op_dis(gen);
        size_t key_idx = key_dis(gen);
        
        if (op < 70) {
            // 70% reads
            auto result = db->get(ColumnFamily::Accounts, keys[key_idx]);
            if (result.is_ok()) {
                benchmark::DoNotOptimize(result.value());
            }
        } else {
            // 30% writes
            auto value = generate_random_value(value_size);
            db->put(ColumnFamily::Accounts, keys[key_idx], value);
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Database_MixedWorkload)->Unit(benchmark::kMicrosecond);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
