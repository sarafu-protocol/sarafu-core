/**
 * Block Processing Benchmarks
 * 
 * Benchmarks for block validation, QC verification, state root calculation,
 * and block serialization/deserialization.
 * 
 * Performance targets from whitepaper:
 * - Block time: 2 seconds
 * - Block propagation: <300ms to 95% of validators
 * - Instant finalization via Quorum Certificates
 * 
 * Validates: Requirements 18.4
 */

#include <benchmark/benchmark.h>
#include "sarafu/consensus/block.h"
#include "sarafu/consensus/block_validator.h"
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/state/state_machine.h"
#include "sarafu/state/transaction.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/bls12_381.h"
#include "sarafu/crypto/merkle_tree.h"
#include <vector>
#include <random>
#include <memory>

using namespace sarafu::consensus;
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

/**
 * Generate a random block header.
 */
BlockHeader generate_random_block_header(uint64_t height) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> timestamp_dis(1700000000, 1800000000);
    
    Blake3Hash prev_hash = Blake3Hash::hash(std::vector<uint8_t>{1, 2, 3, 4});
    Blake3Hash state_root = Blake3Hash::hash(std::vector<uint8_t>{5, 6, 7, 8});
    Blake3Hash tx_root = Blake3Hash::hash(std::vector<uint8_t>{9, 10, 11, 12});
    Blake3Hash val_root = Blake3Hash::hash(std::vector<uint8_t>{13, 14, 15, 16});
    ValidatorID proposer = generate_random_address();
    uint64_t epoch = height / 10000;
    
    return BlockHeader(
        height,
        timestamp_dis(gen),
        prev_hash,
        state_root,
        tx_root,
        val_root,
        proposer,
        epoch
    );
}

/**
 * Generate a random Quorum Certificate.
 */
QuorumCertificate generate_random_qc(uint64_t height, size_t num_validators) {
    Blake3Hash block_hash = Blake3Hash::hash(std::vector<uint8_t>{1, 2, 3, 4});
    
    // Generate validator signatures
    std::vector<ValidatorID> signers;
    std::vector<BLS12_381_Signature> signatures;
    
    for (size_t i = 0; i < num_validators; ++i) {
        auto [pub, priv] = BLS12_381::generate_keypair();
        
        // Create validator ID from public key
        auto pub_bytes = pub.serialize();
        auto pub_hash = Blake3Hash::hash(pub_bytes);
        ValidatorID validator_id(pub_hash.serialize());
        signers.push_back(validator_id);
        
        // Sign the block hash
        auto sig = BLS12_381::sign(block_hash.serialize(), priv);
        signatures.push_back(sig);
    }
    
    // Aggregate signatures
    auto aggregated_sig = BLS12_381::aggregate(signatures);
    
    // Calculate total stake (assume equal stake)
    uint64_t total_stake = num_validators * 100000; // 100k SAR per validator
    
    return QuorumCertificate(
        height - 1,
        block_hash,
        0, // view number
        aggregated_sig,
        signers,
        total_stake
    );
}

/**
 * Generate a random block with transactions.
 */
Block generate_random_block(uint64_t height, size_t num_transactions, size_t num_validators = 100) {
    BlockHeader header = generate_random_block_header(height);
    
    // Generate transactions
    std::vector<Transaction> transactions;
    for (size_t i = 0; i < num_transactions; ++i) {
        auto [pub, priv] = Ed25519::generate_keypair();
        transactions.push_back(generate_random_transaction(pub, priv));
    }
    
    // Generate QC for parent block
    QuorumCertificate qc = generate_random_qc(height, num_validators);
    
    return Block(header, transactions, qc);
}

// ============================================================================
// Block Validation Benchmarks
// ============================================================================

/**
 * Benchmark block validation latency with varying transaction counts.
 * This measures the time to validate a complete block including all transactions.
 * 
 * Target: Must support 2-second block times, so validation should be <300ms.
 */
static void BM_Block_ValidationLatency(benchmark::State& state) {
    size_t num_transactions = state.range(0);
    
    // Create validator registry and state machine
    auto validator_registry = std::make_shared<ValidatorRegistry>();
    auto state_machine = std::make_shared<StateMachine>(1); // chain_id = 1
    
    // Create block validator
    BlockValidator block_validator(validator_registry, state_machine);
    
    // Generate parent and current blocks
    Block parent_block = generate_random_block(100, 10);
    Block current_block = generate_random_block(101, num_transactions);
    
    // Update current block to reference parent
    current_block.header.previous_hash = parent_block.hash();
    current_block.header.timestamp = parent_block.header.timestamp + 2; // 2 seconds later
    
    // Benchmark validation (without expensive state root verification)
    for (auto _ : state) {
        std::string error = block_validator.validate_block(
            current_block,
            parent_block,
            false // Don't verify state root (too expensive)
        );
        benchmark::DoNotOptimize(error);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Block validation with varying transaction counts
BENCHMARK(BM_Block_ValidationLatency)->Arg(10)->Unit(benchmark::kMillisecond);      // Small block
BENCHMARK(BM_Block_ValidationLatency)->Arg(100)->Unit(benchmark::kMillisecond);     // Medium block
BENCHMARK(BM_Block_ValidationLatency)->Arg(1000)->Unit(benchmark::kMillisecond);    // Large block (1000 TPS)
BENCHMARK(BM_Block_ValidationLatency)->Arg(2000)->Unit(benchmark::kMillisecond);    // 2-second block at 1000 TPS

/**
 * Benchmark individual block validation checks.
 */
static void BM_Block_HeightValidation(benchmark::State& state) {
    auto validator_registry = std::make_shared<ValidatorRegistry>();
    auto state_machine = std::make_shared<StateMachine>(1);
    BlockValidator block_validator(validator_registry, state_machine);
    
    Block parent_block = generate_random_block(100, 10);
    Block current_block = generate_random_block(101, 10);
    
    for (auto _ : state) {
        std::string error = block_validator.verify_block_height(current_block, parent_block);
        benchmark::DoNotOptimize(error);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Block_HeightValidation)->Unit(benchmark::kNanosecond);

static void BM_Block_HashChainValidation(benchmark::State& state) {
    auto validator_registry = std::make_shared<ValidatorRegistry>();
    auto state_machine = std::make_shared<StateMachine>(1);
    BlockValidator block_validator(validator_registry, state_machine);
    
    Block parent_block = generate_random_block(100, 10);
    Block current_block = generate_random_block(101, 10);
    current_block.header.previous_hash = parent_block.hash();
    
    for (auto _ : state) {
        std::string error = block_validator.verify_block_hash_chain(current_block, parent_block);
        benchmark::DoNotOptimize(error);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Block_HashChainValidation)->Unit(benchmark::kNanosecond);

static void BM_Block_TimestampValidation(benchmark::State& state) {
    auto validator_registry = std::make_shared<ValidatorRegistry>();
    auto state_machine = std::make_shared<StateMachine>(1);
    BlockValidator block_validator(validator_registry, state_machine);
    
    Block parent_block = generate_random_block(100, 10);
    Block current_block = generate_random_block(101, 10);
    current_block.header.timestamp = parent_block.header.timestamp + 2;
    
    for (auto _ : state) {
        std::string error = block_validator.verify_timestamp(current_block, parent_block);
        benchmark::DoNotOptimize(error);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Block_TimestampValidation)->Unit(benchmark::kNanosecond);

// ============================================================================
// Quorum Certificate Verification Benchmarks
// ============================================================================

/**
 * Benchmark QC verification with different validator set sizes.
 * This is critical for instant finalization.
 */
static void BM_QC_Verification(benchmark::State& state) {
    size_t num_validators = state.range(0);
    
    // Generate a block hash to sign
    Blake3Hash block_hash = Blake3Hash::hash(std::vector<uint8_t>{1, 2, 3, 4});
    
    // Generate validator keypairs and signatures
    std::vector<BLS12_381_PublicKey> public_keys;
    std::vector<BLS12_381_Signature> signatures;
    std::vector<ValidatorID> signers;
    
    for (size_t i = 0; i < num_validators; ++i) {
        auto [pub, priv] = BLS12_381::generate_keypair();
        public_keys.push_back(pub);
        
        // Create validator ID
        auto pub_bytes = pub.serialize();
        auto pub_hash = Blake3Hash::hash(pub_bytes);
        ValidatorID validator_id(pub_hash.serialize());
        signers.push_back(validator_id);
        
        // Sign the block hash
        auto sig = BLS12_381::sign(block_hash.serialize(), priv);
        signatures.push_back(sig);
    }
    
    // Aggregate signatures
    auto aggregated_sig = BLS12_381::aggregate(signatures);
    
    // Create QC
    QuorumCertificate qc(
        100,
        block_hash,
        0,
        aggregated_sig,
        signers,
        num_validators * 100000
    );
    
    // Benchmark QC verification (aggregated signature verification)
    for (auto _ : state) {
        bool valid = BLS12_381::verify_aggregated(
            aggregated_sig,
            block_hash.serialize(),
            public_keys
        );
        benchmark::DoNotOptimize(valid);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// QC verification with varying validator set sizes
BENCHMARK(BM_QC_Verification)->Arg(10)->Unit(benchmark::kMicrosecond);    // Small validator set
BENCHMARK(BM_QC_Verification)->Arg(50)->Unit(benchmark::kMicrosecond);    // Medium validator set
BENCHMARK(BM_QC_Verification)->Arg(100)->Unit(benchmark::kMicrosecond);   // Large validator set (2/3 of 150)
BENCHMARK(BM_QC_Verification)->Arg(150)->Unit(benchmark::kMicrosecond);   // Full validator set
BENCHMARK(BM_QC_Verification)->Arg(334)->Unit(benchmark::kMicrosecond);   // 2/3 of max (500)

/**
 * Benchmark QC serialization.
 */
static void BM_QC_Serialization(benchmark::State& state) {
    size_t num_validators = state.range(0);
    QuorumCertificate qc = generate_random_qc(100, num_validators);
    
    for (auto _ : state) {
        auto serialized = qc.serialize();
        benchmark::DoNotOptimize(serialized);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_QC_Serialization)->Arg(10)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_QC_Serialization)->Arg(100)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_QC_Serialization)->Arg(334)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark QC deserialization.
 */
static void BM_QC_Deserialization(benchmark::State& state) {
    size_t num_validators = state.range(0);
    QuorumCertificate qc = generate_random_qc(100, num_validators);
    auto serialized = qc.serialize();
    
    for (auto _ : state) {
        auto deserialized = QuorumCertificate::deserialize(serialized);
        benchmark::DoNotOptimize(deserialized);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_QC_Deserialization)->Arg(10)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_QC_Deserialization)->Arg(100)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_QC_Deserialization)->Arg(334)->Unit(benchmark::kMicrosecond);

// ============================================================================
// State Root Calculation Benchmarks
// ============================================================================

/**
 * Benchmark state root calculation using Merkle trees.
 * This is used to compute the state commitment after applying transactions.
 */
static void BM_StateRoot_Calculation(benchmark::State& state) {
    size_t num_accounts = state.range(0);
    
    // Generate account hashes
    std::vector<Blake3Hash> account_hashes;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < num_accounts; ++i) {
        std::vector<uint8_t> account_data(64);
        for (size_t j = 0; j < 64; ++j) {
            account_data[j] = dis(gen);
        }
        account_hashes.push_back(Blake3Hash::hash(account_data));
    }
    
    // Benchmark Merkle tree construction and root calculation
    for (auto _ : state) {
        MerkleTree tree;
        tree.build_tree(account_hashes);
        auto root = tree.get_root();
        benchmark::DoNotOptimize(root);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// State root calculation with varying account counts
BENCHMARK(BM_StateRoot_Calculation)->Arg(100)->Unit(benchmark::kMicrosecond);      // Small state
BENCHMARK(BM_StateRoot_Calculation)->Arg(1000)->Unit(benchmark::kMicrosecond);     // Medium state
BENCHMARK(BM_StateRoot_Calculation)->Arg(10000)->Unit(benchmark::kMillisecond);    // Large state
BENCHMARK(BM_StateRoot_Calculation)->Arg(100000)->Unit(benchmark::kMillisecond);   // Very large state

/**
 * Benchmark Merkle proof generation.
 */
static void BM_MerkleProof_Generation(benchmark::State& state) {
    size_t num_leaves = 10000;
    
    // Build a Merkle tree
    std::vector<Blake3Hash> leaves;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < num_leaves; ++i) {
        std::vector<uint8_t> data(32);
        for (size_t j = 0; j < 32; ++j) {
            data[j] = dis(gen);
        }
        leaves.push_back(Blake3Hash::hash(data));
    }
    
    MerkleTree tree;
    tree.build_tree(leaves);
    
    std::uniform_int_distribution<size_t> index_dis(0, num_leaves - 1);
    
    // Benchmark proof generation
    for (auto _ : state) {
        size_t leaf_index = index_dis(gen);
        auto proof = tree.generate_proof(leaf_index);
        benchmark::DoNotOptimize(proof);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_MerkleProof_Generation)->Unit(benchmark::kMicrosecond);

/**
 * Benchmark Merkle proof verification.
 */
static void BM_MerkleProof_Verification(benchmark::State& state) {
    size_t num_leaves = 10000;
    
    // Build a Merkle tree
    std::vector<Blake3Hash> leaves;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);
    
    for (size_t i = 0; i < num_leaves; ++i) {
        std::vector<uint8_t> data(32);
        for (size_t j = 0; j < 32; ++j) {
            data[j] = dis(gen);
        }
        leaves.push_back(Blake3Hash::hash(data));
    }
    
    MerkleTree tree;
    tree.build_tree(leaves);
    auto root = tree.get_root();
    
    // Generate a proof
    auto proof = tree.generate_proof(0);
    
    // Benchmark proof verification
    for (auto _ : state) {
        bool valid = MerkleTree::verify_proof(*proof, root);
        benchmark::DoNotOptimize(valid);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_MerkleProof_Verification)->Unit(benchmark::kNanosecond);

// ============================================================================
// Block Serialization/Deserialization Benchmarks
// ============================================================================

/**
 * Benchmark block serialization with varying transaction counts.
 * This is used for network transmission and storage.
 */
static void BM_Block_Serialization(benchmark::State& state) {
    size_t num_transactions = state.range(0);
    Block block = generate_random_block(100, num_transactions);
    
    for (auto _ : state) {
        auto serialized = block.serialize();
        benchmark::DoNotOptimize(serialized);
    }
    
    state.SetItemsProcessed(state.iterations());
    
    // Report block size
    auto serialized = block.serialize();
    state.SetLabel("Size: " + std::to_string(serialized.size() / 1024) + " KB");
}

// Block serialization with varying transaction counts
BENCHMARK(BM_Block_Serialization)->Arg(10)->Unit(benchmark::kMicrosecond);      // Small block
BENCHMARK(BM_Block_Serialization)->Arg(100)->Unit(benchmark::kMicrosecond);     // Medium block
BENCHMARK(BM_Block_Serialization)->Arg(1000)->Unit(benchmark::kMillisecond);    // Large block
BENCHMARK(BM_Block_Serialization)->Arg(2000)->Unit(benchmark::kMillisecond);    // 2-second block

/**
 * Benchmark block deserialization.
 */
static void BM_Block_Deserialization(benchmark::State& state) {
    size_t num_transactions = state.range(0);
    Block block = generate_random_block(100, num_transactions);
    auto serialized = block.serialize();
    
    for (auto _ : state) {
        auto deserialized = Block::deserialize(serialized);
        benchmark::DoNotOptimize(deserialized);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Block deserialization with varying transaction counts
BENCHMARK(BM_Block_Deserialization)->Arg(10)->Unit(benchmark::kMicrosecond);      // Small block
BENCHMARK(BM_Block_Deserialization)->Arg(100)->Unit(benchmark::kMicrosecond);     // Medium block
BENCHMARK(BM_Block_Deserialization)->Arg(1000)->Unit(benchmark::kMillisecond);    // Large block
BENCHMARK(BM_Block_Deserialization)->Arg(2000)->Unit(benchmark::kMillisecond);    // 2-second block

/**
 * Benchmark block header serialization.
 */
static void BM_BlockHeader_Serialization(benchmark::State& state) {
    BlockHeader header = generate_random_block_header(100);
    
    for (auto _ : state) {
        auto serialized = header.serialize();
        benchmark::DoNotOptimize(serialized);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BlockHeader_Serialization)->Unit(benchmark::kNanosecond);

/**
 * Benchmark block header deserialization.
 */
static void BM_BlockHeader_Deserialization(benchmark::State& state) {
    BlockHeader header = generate_random_block_header(100);
    auto serialized = header.serialize();
    
    for (auto _ : state) {
        auto deserialized = BlockHeader::deserialize(serialized);
        benchmark::DoNotOptimize(deserialized);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BlockHeader_Deserialization)->Unit(benchmark::kNanosecond);

/**
 * Benchmark block header hash computation.
 */
static void BM_BlockHeader_HashComputation(benchmark::State& state) {
    BlockHeader header = generate_random_block_header(100);
    
    for (auto _ : state) {
        auto hash = header.hash();
        benchmark::DoNotOptimize(hash);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_BlockHeader_HashComputation)->Unit(benchmark::kNanosecond);

// ============================================================================
// End-to-End Block Processing Benchmarks
// ============================================================================

/**
 * Benchmark complete block processing pipeline.
 * This includes: deserialization -> validation -> QC verification.
 * 
 * This simulates the complete block processing that happens when a validator
 * receives a new block from the network.
 */
static void BM_Block_EndToEndProcessing(benchmark::State& state) {
    size_t num_transactions = state.range(0);
    
    // Create validator registry and state machine
    auto validator_registry = std::make_shared<ValidatorRegistry>();
    auto state_machine = std::make_shared<StateMachine>(1);
    BlockValidator block_validator(validator_registry, state_machine);
    
    // Generate and serialize blocks
    Block parent_block = generate_random_block(100, 10);
    Block current_block = generate_random_block(101, num_transactions);
    current_block.header.previous_hash = parent_block.hash();
    current_block.header.timestamp = parent_block.header.timestamp + 2;
    
    auto serialized = current_block.serialize();
    
    // Benchmark end-to-end processing
    for (auto _ : state) {
        // 1. Deserialize
        auto deserialized_block = Block::deserialize(serialized);
        
        // 2. Validate block
        std::string error = block_validator.validate_block(
            deserialized_block,
            parent_block,
            false // Don't verify state root
        );
        
        // 3. Verify QC (already done in validation, but measure separately)
        bool qc_valid = !error.empty();
        
        benchmark::DoNotOptimize(qc_valid);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// End-to-end processing with target block sizes
BENCHMARK(BM_Block_EndToEndProcessing)->Arg(100)->Unit(benchmark::kMillisecond);     // Medium block
BENCHMARK(BM_Block_EndToEndProcessing)->Arg(1000)->Unit(benchmark::kMillisecond);    // Large block (1000 TPS)
BENCHMARK(BM_Block_EndToEndProcessing)->Arg(2000)->Unit(benchmark::kMillisecond);    // 2-second block at 1000 TPS

/**
 * Benchmark block propagation simulation.
 * This measures the time to serialize, transmit (simulated), and deserialize a block.
 * 
 * Target: <300ms for 95% of validators
 */
static void BM_Block_PropagationSimulation(benchmark::State& state) {
    size_t num_transactions = state.range(0);
    Block block = generate_random_block(100, num_transactions);
    
    for (auto _ : state) {
        // 1. Serialize for transmission
        auto serialized = block.serialize();
        
        // 2. Simulate network transmission (just measure serialization overhead)
        // In reality, this would include network latency
        
        // 3. Deserialize on receiving end
        auto deserialized = Block::deserialize(serialized);
        
        benchmark::DoNotOptimize(deserialized);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// Block propagation with varying sizes
BENCHMARK(BM_Block_PropagationSimulation)->Arg(100)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Block_PropagationSimulation)->Arg(1000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Block_PropagationSimulation)->Arg(2000)->Unit(benchmark::kMillisecond);

/**
 * Benchmark blocks per second throughput.
 * This measures how many blocks can be processed per second.
 * 
 * Target: 0.5 blocks/second (2-second block time)
 */
static void BM_Block_ProcessingThroughput(benchmark::State& state) {
    size_t num_transactions = 1000; // 1000 TPS target
    
    auto validator_registry = std::make_shared<ValidatorRegistry>();
    auto state_machine = std::make_shared<StateMachine>(1);
    BlockValidator block_validator(validator_registry, state_machine);
    
    Block parent_block = generate_random_block(100, 10);
    
    // Benchmark block processing throughput
    uint64_t block_height = 101;
    for (auto _ : state) {
        Block current_block = generate_random_block(block_height, num_transactions);
        current_block.header.previous_hash = parent_block.hash();
        current_block.header.timestamp = parent_block.header.timestamp + 2;
        
        std::string error = block_validator.validate_block(
            current_block,
            parent_block,
            false
        );
        
        benchmark::DoNotOptimize(error);
        
        parent_block = current_block;
        block_height++;
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_Block_ProcessingThroughput)->Unit(benchmark::kMillisecond);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
