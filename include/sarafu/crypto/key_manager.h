#pragma once

#include "sarafu/crypto/bls12_381.h"
#include <memory>
#include <string>
#include <vector>

namespace sarafu {
namespace crypto {

/**
 * Abstract interface for cryptographic key management.
 * 
 * Supports both software keys (for testing) and HSM-backed keys
 * (for production). All signing operations go through this interface,
 * ensuring keys never leave the HSM in production.
 */
class KeyManager {
public:
    virtual ~KeyManager() = default;
    
    /**
     * Sign a message with the managed key.
     * 
     * @param message Message to sign
     * @return BLS12-381 signature
     */
    virtual BLS12_381_Signature sign(const std::vector<uint8_t>& message) = 0;
    
    /**
     * Get the public key corresponding to the managed private key.
     * 
     * @return BLS12-381 public key
     */
    virtual BLS12_381_PublicKey get_public_key() const = 0;
    
    /**
     * Check if this key is backed by an HSM.
     * 
     * @return true if HSM-backed, false if software key
     */
    virtual bool is_hsm_backed() const = 0;
    
    /**
     * Get a description of the key storage (for logging).
     * 
     * @return Description string (e.g., "YubiHSM2 slot 0", "Software key")
     */
    virtual std::string get_description() const = 0;
};

/**
 * Factory for creating KeyManager instances.
 */
class KeyManagerFactory {
public:
    /**
     * Create a KeyManager from configuration.
     * 
     * @param config Configuration parameters
     * @return KeyManager instance
     */
    static std::unique_ptr<KeyManager> create_from_config(
        const std::string& config_path
    );
    
    /**
     * Create an HSM-backed KeyManager.
     * 
     * @param pkcs11_module Path to PKCS#11 module (e.g., /usr/lib/libykcs11.so)
     * @param slot_id HSM slot ID
     * @param pin HSM PIN
     * @param key_label Key label in HSM
     * @return HSM KeyManager instance
     */
    static std::unique_ptr<KeyManager> create_hsm_key_manager(
        const std::string& pkcs11_module,
        uint32_t slot_id,
        const std::string& pin,
        const std::string& key_label
    );
    
    /**
     * Create a software KeyManager (testnet only).
     * 
     * @param secret_key BLS12-381 secret key
     * @return Software KeyManager instance
     */
    static std::unique_ptr<KeyManager> create_software_key_manager(
        const BLS12_381_SecretKey& secret_key
    );
};

} // namespace crypto
} // namespace sarafu
