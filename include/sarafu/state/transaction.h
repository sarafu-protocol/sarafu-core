#pragma once

#include <cstdint>
#include <vector>
#include "sarafu/state/account.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/ed25519.h"

namespace sarafu {
namespace state {

/**
 * Transaction represents a signed transfer of tokens between accounts.
 * 
 * Each transaction includes:
 * - from: The sender's address
 * - to: The recipient's address
 * - amount: The number of tokens to transfer
 * - nonce: The sender's nonce for replay protection
 * - fee: The total transaction fee (base_fee + priority_fee)
 * - gas_limit: Maximum gas allowed for execution
 * - chain_id: Network identifier to prevent cross-chain replay
 * - signature: Ed25519 signature over the transaction hash
 * 
 * The transaction hash includes the chain_id to prevent replay attacks
 * across different networks.
 */
struct Transaction {
    Address from;
    Address to;
    uint64_t amount;
    uint64_t nonce;
    uint64_t fee;
    uint64_t gas_limit;
    uint32_t chain_id;
    crypto::Ed25519_Signature signature;

    // Constructors
    Transaction();
    Transaction(
        const Address& from_addr,
        const Address& to_addr,
        uint64_t amt,
        uint64_t n,
        uint64_t f,
        uint64_t gas,
        uint32_t chain
    );

    /**
     * Compute the transaction hash for signing/verification.
     * 
     * The hash includes all transaction fields EXCEPT the signature.
     * This hash is what gets signed by the sender's private key.
     * 
     * The chain_id is included in the hash to prevent cross-chain replay attacks.
     * 
     * @return Blake3 hash of the transaction
     */
    crypto::Blake3Hash hash() const;

    /**
     * Serialize the transaction for hashing (without signature).
     * 
     * This serialization is used to compute the transaction hash.
     * The signature is NOT included in this serialization.
     * 
     * @return Serialized transaction bytes
     */
    std::vector<uint8_t> serialize_for_signing() const;

    /**
     * Serialize the complete transaction including signature.
     * 
     * This serialization includes all fields including the signature,
     * and is used for storage and network transmission.
     * 
     * @return Serialized transaction bytes
     */
    std::vector<uint8_t> serialize() const;

    /**
     * Deserialize a transaction from bytes.
     * 
     * @param data The serialized transaction data
     * @return The deserialized transaction
     * @throws std::invalid_argument if data is invalid
     */
    static Transaction deserialize(const std::vector<uint8_t>& data);

    /**
     * Sign the transaction with a private key.
     * 
     * This computes the transaction hash and signs it with the provided
     * private key, storing the signature in the transaction.
     * 
     * @param private_key The Ed25519 private key to sign with
     */
    void sign(const crypto::Ed25519_PrivateKey& private_key);

    /**
     * Verify the transaction signature.
     * 
     * This verifies that the signature is valid for the transaction hash
     * and the sender's public key (derived from the from address).
     * 
     * @param public_key The Ed25519 public key to verify against
     * @return true if the signature is valid, false otherwise
     */
    bool verify_signature(const crypto::Ed25519_PublicKey& public_key) const;

    // Comparison operators
    bool operator==(const Transaction& other) const;
    bool operator!=(const Transaction& other) const;
};

/**
 * TransactionReceipt represents the result of executing a transaction.
 * 
 * Contains:
 * - tx_hash: Hash of the executed transaction
 * - block_height: Height of the block containing the transaction
 * - success: Whether the transaction executed successfully
 * - gas_used: Amount of gas consumed during execution
 * - error_message: Error description if execution failed
 */
struct TransactionReceipt {
    crypto::Blake3Hash tx_hash;
    uint64_t block_height;
    bool success;
    uint64_t gas_used;
    std::string error_message;

    // Constructors
    TransactionReceipt();
    TransactionReceipt(
        const crypto::Blake3Hash& hash,
        uint64_t height,
        bool succeeded,
        uint64_t gas,
        const std::string& error = ""
    );

    // Serialization
    std::vector<uint8_t> serialize() const;
    static TransactionReceipt deserialize(const std::vector<uint8_t>& data);

    // Comparison operators
    bool operator==(const TransactionReceipt& other) const;
    bool operator!=(const TransactionReceipt& other) const;
};

} // namespace state
} // namespace sarafu
