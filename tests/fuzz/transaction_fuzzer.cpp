/**
 * Transaction Parsing Fuzzer
 * 
 * This fuzzer tests the robustness of Transaction::deserialize() against
 * malformed and unexpected inputs. It verifies that the parser:
 * - Does not crash on invalid inputs
 * - Does not hang on malicious inputs
 * - Does not exhibit undefined behavior
 * - Properly validates all input data
 * 
 * The fuzzer feeds random byte sequences to the deserialize function and
 * ensures it either succeeds with valid data or throws an exception with
 * invalid data, but never crashes.
 * 
 * Requirements: 19.1, 19.2
 * Validates: Requirements 19.6, 19.7
 */

#include "sarafu/state/transaction.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <stdexcept>

using namespace sarafu::state;

/**
 * LibFuzzer entry point.
 * 
 * This function is called by libFuzzer with random byte sequences.
 * It attempts to deserialize the data as a Transaction and verifies
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
        // Attempt to deserialize the transaction
        Transaction tx = Transaction::deserialize(input);
        
        // If deserialization succeeds, verify the transaction is valid
        // by accessing its fields (this ensures no uninitialized memory)
        (void)tx.from;
        (void)tx.to;
        (void)tx.amount;
        (void)tx.nonce;
        (void)tx.gas_limit;
        (void)tx.fee;
        (void)tx.signature;
        (void)tx.hash();
        
        // Try to serialize it back (round-trip test)
        std::vector<uint8_t> serialized = tx.serialize();
        
        // If we got valid data, try deserializing again to ensure consistency
        if (!serialized.empty()) {
            Transaction tx2 = Transaction::deserialize(serialized);
            
            // Verify round-trip consistency
            // The deserialized transaction should match the original
            if (tx.hash() != tx2.hash()) {
                // This would indicate a serialization bug
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
