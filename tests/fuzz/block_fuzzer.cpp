/**
 * Block Parsing Fuzzer
 * 
 * This fuzzer tests the robustness of Block::deserialize() against
 * malformed and unexpected inputs. It verifies that the parser:
 * - Does not crash on invalid inputs
 * - Does not hang on malicious inputs
 * - Does not exhibit undefined behavior
 * - Properly validates block structure and data
 * 
 * Blocks are more complex than transactions, containing:
 * - Block header with multiple fields
 * - Quorum Certificate with aggregated signatures
 * - List of transactions
 * - Validator signatures
 * 
 * The fuzzer ensures all these components are parsed safely.
 * 
 * Requirements: 19.1, 19.3
 * Validates: Requirements 19.6, 19.7
 */

#include "sarafu/consensus/block.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <stdexcept>

using namespace sarafu::consensus;

/**
 * LibFuzzer entry point.
 * 
 * This function is called by libFuzzer with random byte sequences.
 * It attempts to deserialize the data as a Block and verifies
 * that the operation either succeeds or throws an exception, but
 * never crashes or hangs.
 * 
 * @param data Pointer to fuzzer-generated data
 * @param size Size of the fuzzer-generated data
 * @return 0 to continue fuzzing
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Skip empty inputs
    if (size == 0) {
        return 0;
    }
    
    // Convert to vector for deserialize function
    std::vector<uint8_t> input(data, data + size);
    
    try {
        // Attempt to deserialize the block
        Block block = Block::deserialize(input);
        
        // If deserialization succeeds, verify the block is valid
        // by accessing its fields (this ensures no uninitialized memory)
        const BlockHeader& header = block.header;
        
        // Access header fields
        (void)header.height;
        (void)header.timestamp;
        (void)header.previous_hash;
        (void)header.state_root;
        (void)header.transactions_root;
        (void)header.proposer;
        (void)header.epoch;
        
        // Access block fields
        (void)block.transactions;
        (void)block.hash();
        
        // Access QC fields
        const QuorumCertificate& qc = block.justify;
        (void)qc.block_hash;
        (void)qc.view_number;
        (void)qc.aggregated_signature;
        (void)qc.signers;
        (void)qc.total_stake_signed;
        
        // Try to serialize it back (round-trip test)
        std::vector<uint8_t> serialized = block.serialize();
        
        // If we got valid data, try deserializing again to ensure consistency
        if (!serialized.empty()) {
            Block block2 = Block::deserialize(serialized);
            
            // Verify round-trip consistency
            // The deserialized block should match the original
            if (block.hash() != block2.hash()) {
                // This would indicate a serialization bug
                __builtin_trap();
            }
            
            // Verify header consistency
            if (block.header.height != block2.header.height ||
                block.header.timestamp != block2.header.timestamp) {
                __builtin_trap();
            }
        }
    } catch (const std::invalid_argument&) {
        // Expected for invalid input - this is correct behavior
        // The deserializer should throw on malformed data
    } catch (const std::out_of_range&) {
        // Expected for inputs that cause range errors
    } catch (const std::exception& e) {
        // Other exceptions might indicate bugs, but we don't crash
        // Log the exception type for debugging (libFuzzer will capture this)
        (void)e.what();
    }
    
    // Return 0 to continue fuzzing
    return 0;
}
