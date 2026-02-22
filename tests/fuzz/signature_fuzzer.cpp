/**
 * Signature Verification Fuzzer
 * 
 * This fuzzer tests the robustness of signature verification functions
 * against malformed and invalid inputs. It verifies that:
 * - Ed25519 verification does not crash on invalid signatures/keys
 * - BLS12-381 verification does not crash on invalid signatures/keys
 * - Signature aggregation handles malformed inputs safely
 * - No undefined behavior occurs with random inputs
 * 
 * The fuzzer tests both signature schemes used in Sarafu:
 * - Ed25519: Used for transaction signatures (64-byte signatures, 32-byte keys)
 * - BLS12-381: Used for consensus signatures (96-byte signatures, 48-byte keys)
 * 
 * Requirements: 19.1, 19.4
 * Validates: Requirements 19.6, 19.7
 */

#include "sarafu/crypto/ed25519.h"
#include "sarafu/crypto/bls12_381.h"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <stdexcept>

using namespace sarafu::crypto;

/**
 * Fuzz Ed25519 signature verification.
 * 
 * Input format (variable length):
 * - Bytes 0-31: Public key (32 bytes) - if available
 * - Bytes 32-95: Signature (64 bytes) - if available
 * - Bytes 96+: Message (variable length)
 * 
 * The fuzzer tries various combinations of valid/invalid keys and signatures.
 */
static void fuzz_ed25519(const uint8_t* data, size_t size) {
    // Need at least 96 bytes for key + signature
    if (size < 96) {
        return;
    }
    
    try {
        // Extract public key (32 bytes)
        std::vector<uint8_t> pubkey_data(data, data + 32);
        Ed25519_PublicKey pubkey(pubkey_data);
        
        // Extract signature (64 bytes)
        std::vector<uint8_t> sig_data(data + 32, data + 96);
        Ed25519_Signature signature(sig_data);
        
        // Extract message (remaining bytes)
        std::vector<uint8_t> message(data + 96, data + size);
        
        // Verify signature - should not crash regardless of input validity
        bool result = Ed25519::verify(signature, message, pubkey);
        
        // Use result to prevent optimization
        (void)result;
        
    } catch (const std::exception& e) {
        // Exceptions are acceptable for invalid inputs
        (void)e.what();
    }
}

/**
 * Fuzz BLS12-381 signature verification.
 * 
 * Input format (variable length):
 * - Bytes 0-47: Public key (48 bytes) - if available
 * - Bytes 48-143: Signature (96 bytes) - if available
 * - Bytes 144+: Message (variable length)
 * 
 * The fuzzer tries various combinations of valid/invalid keys and signatures.
 */
static void fuzz_bls12_381(const uint8_t* data, size_t size) {
    // Need at least 144 bytes for key + signature
    if (size < 144) {
        return;
    }
    
    try {
        // Extract public key (48 bytes)
        std::vector<uint8_t> pubkey_data(data, data + 48);
        BLS12_381_PublicKey pubkey(pubkey_data);
        
        // Extract signature (96 bytes)
        std::vector<uint8_t> sig_data(data + 48, data + 144);
        BLS12_381_Signature signature(sig_data);
        
        // Extract message (remaining bytes)
        std::vector<uint8_t> message(data + 144, data + size);
        
        // Verify signature - should not crash regardless of input validity
        bool result = BLS12_381::verify(signature, message, pubkey);
        
        // Use result to prevent optimization
        (void)result;
        
    } catch (const std::exception& e) {
        // Exceptions are acceptable for invalid inputs
        (void)e.what();
    }
}

/**
 * Fuzz BLS12-381 signature aggregation.
 * 
 * Input format:
 * - Byte 0: Number of signatures to aggregate (N)
 * - Bytes 1+: N * 96 bytes of signature data
 * 
 * Tests that aggregation handles malformed signatures safely.
 */
static void fuzz_bls_aggregation(const uint8_t* data, size_t size) {
    if (size < 1) {
        return;
    }
    
    // First byte determines number of signatures
    uint8_t num_sigs = data[0] % 10;  // Limit to 10 signatures
    
    if (num_sigs == 0) {
        return;
    }
    
    size_t required_size = 1 + (num_sigs * 96);
    if (size < required_size) {
        return;
    }
    
    try {
        std::vector<BLS12_381_Signature> signatures;
        
        // Extract signatures
        for (uint8_t i = 0; i < num_sigs; i++) {
            size_t offset = 1 + (i * 96);
            std::vector<uint8_t> sig_data(data + offset, data + offset + 96);
            signatures.emplace_back(sig_data);
        }
        
        // Attempt aggregation - should not crash
        BLS12_381_Signature aggregated = BLS12_381::aggregate(signatures);
        
        // Use result to prevent optimization
        (void)aggregated.size();
        
    } catch (const std::exception& e) {
        // Exceptions are acceptable for invalid inputs
        (void)e.what();
    }
}

/**
 * LibFuzzer entry point.
 * 
 * This function is called by libFuzzer with random byte sequences.
 * It tests signature verification functions with malformed inputs.
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
    
    // Use first byte to determine which function to fuzz
    uint8_t mode = data[0] % 3;
    
    switch (mode) {
        case 0:
            // Fuzz Ed25519 verification
            fuzz_ed25519(data + 1, size - 1);
            break;
        case 1:
            // Fuzz BLS12-381 verification
            fuzz_bls12_381(data + 1, size - 1);
            break;
        case 2:
            // Fuzz BLS12-381 aggregation
            fuzz_bls_aggregation(data + 1, size - 1);
            break;
    }
    
    // Return 0 to continue fuzzing
    return 0;
}
