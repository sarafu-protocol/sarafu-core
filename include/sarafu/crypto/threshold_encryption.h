#pragma once

#include "sarafu/crypto/bls12_381.h"
#include <vector>
#include <optional>

namespace sarafu {
namespace crypto {

/**
 * Threshold Encryption for MEV Prevention
 * 
 * Uses BLS12-381 threshold encryption where transactions are encrypted
 * to the validator set and can only be decrypted with cooperation of
 * 2/3 of validators.
 * 
 * This prevents validators from seeing transaction contents before
 * block inclusion, eliminating MEV extraction opportunities.
 */

/**
 * Threshold public key for a validator set.
 * Generated from individual validator public keys.
 */
struct ThresholdPublicKey {
    BLS12_381_PublicKey aggregate_key;
    uint32_t threshold;  // Number of shares needed (e.g., 2/3 of N)
    uint32_t total_validators;
    
    ThresholdPublicKey();
    ThresholdPublicKey(const BLS12_381_PublicKey& key, uint32_t t, uint32_t n);
    
    std::vector<uint8_t> serialize() const;
    static ThresholdPublicKey deserialize(const std::vector<uint8_t>& data);
};

/**
 * Decryption share from a single validator.
 * Threshold shares are combined to decrypt the ciphertext.
 */
struct DecryptionShare {
    uint32_t validator_index;
    std::vector<uint8_t> share_data;
    BLS12_381_Signature proof;  // Proof of correct decryption
    
    DecryptionShare();
    DecryptionShare(uint32_t idx, const std::vector<uint8_t>& data, 
                    const BLS12_381_Signature& p);
    
    std::vector<uint8_t> serialize() const;
    static DecryptionShare deserialize(const std::vector<uint8_t>& data);
};



/**
 * Encrypted data with threshold encryption.
 */
struct ThresholdCiphertext {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> nonce;
    ThresholdPublicKey public_key;
    
    ThresholdCiphertext();
    
    std::vector<uint8_t> serialize() const;
    static ThresholdCiphertext deserialize(const std::vector<uint8_t>& data);
};

/**
 * Threshold encryption operations.
 */
class ThresholdEncryption {
public:
    /**
     * Generate threshold public key from validator public keys.
     * 
     * @param validator_pubkeys Public keys of all validators
     * @param threshold Number of shares needed (e.g., 2/3 of N)
     * @return Threshold public key
     */
    static ThresholdPublicKey generate_threshold_pubkey(
        const std::vector<BLS12_381_PublicKey>& validator_pubkeys,
        uint32_t threshold
    );
    
    /**
     * Encrypt data to threshold public key.
     * 
     * @param plaintext Data to encrypt
     * @param pubkey Threshold public key
     * @return Encrypted ciphertext
     */
    static ThresholdCiphertext encrypt(
        const std::vector<uint8_t>& plaintext,
        const ThresholdPublicKey& pubkey
    );
    
    /**
     * Generate decryption share for a validator.
     * 
     * @param ciphertext Encrypted data
     * @param validator_secret_key Validator's secret key
     * @param validator_index Validator's index in the set
     * @return Decryption share
     */
    static DecryptionShare generate_decryption_share(
        const ThresholdCiphertext& ciphertext,
        const BLS12_381_SecretKey& validator_secret_key,
        uint32_t validator_index
    );
    
    /**
     * Combine decryption shares to decrypt ciphertext.
     * 
     * @param ciphertext Encrypted data
     * @param shares Decryption shares (must have >= threshold)
     * @return Decrypted plaintext, or nullopt if insufficient shares
     */
    static std::optional<std::vector<uint8_t>> decrypt(
        const ThresholdCiphertext& ciphertext,
        const std::vector<DecryptionShare>& shares
    );
    
    /**
     * Verify a decryption share is valid.
     * 
     * @param share Decryption share to verify
     * @param ciphertext Original ciphertext
     * @param validator_pubkey Validator's public key
     * @return true if share is valid
     */
    static bool verify_decryption_share(
        const DecryptionShare& share,
        const ThresholdCiphertext& ciphertext,
        const BLS12_381_PublicKey& validator_pubkey
    );
};

} // namespace crypto
} // namespace sarafu
