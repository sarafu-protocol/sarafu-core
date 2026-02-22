#pragma once

#include "sarafu/consensus/slashing_detector.h"
#include "sarafu/crypto/blake3_hash.h"
#include "sarafu/crypto/bls12_381.h"
#include <string>
#include <vector>
#include <optional>

namespace sarafu {
namespace consensus {

/**
 * Network state for circuit breaker.
 */
enum class NetworkState {
    Normal,      // Operating normally
    Halted,      // Consensus halted due to anomaly
    Resuming     // In process of resuming after halt
};

/**
 * Message to halt the network.
 * Requires 2/3 validator agreement.
 */
struct HaltMessage {
    uint64_t halt_height;
    std::string reason;
    crypto::Blake3Hash state_root;
    std::vector<crypto::BLS12_381_Signature> validator_signatures;
    
    HaltMessage();
    HaltMessage(uint64_t height, const std::string& rsn, 
                const crypto::Blake3Hash& root);
    
    std::vector<uint8_t> serialize() const;
    static HaltMessage deserialize(const std::vector<uint8_t>& data);
    
    // Verify 2/3 validator signatures
    bool verify_signatures(const std::vector<crypto::BLS12_381_PublicKey>& validator_pubkeys) const;
};

/**
 * Message to resume the network after halt.
 * Requires 2/3 validator agreement and manual verification.
 */
struct ResumeMessage {
    uint64_t resume_height;
    std::string resolution;
    crypto::Blake3Hash verified_state_root;
    std::vector<crypto::BLS12_381_Signature> validator_signatures;
    
    ResumeMessage();
    ResumeMessage(uint64_t height, const std::string& res,
                  const crypto::Blake3Hash& root);
    
    std::vector<uint8_t> serialize() const;
    static ResumeMessage deserialize(const std::vector<uint8_t>& data);
    
    // Verify 2/3 validator signatures
    bool verify_signatures(const std::vector<crypto::BLS12_381_PublicKey>& validator_pubkeys) const;
};



/**
 * Slashing anomaly detector.
 * Detects unusual slashing patterns that may indicate bugs or attacks.
 */
class SlashingAnomalyDetector {
public:
    struct Config {
        double max_slashing_rate_per_epoch;      // e.g., 0.05 (5% of validators)
        size_t max_simultaneous_slashings;       // e.g., 10 validators at once
        double max_stake_slashed_per_epoch;      // e.g., 0.10 (10% of total stake)
        size_t detection_window_epochs;          // e.g., 3 epochs for trend analysis
        
        Config()
            : max_slashing_rate_per_epoch(0.05),
              max_simultaneous_slashings(10),
              max_stake_slashed_per_epoch(0.10),
              detection_window_epochs(3) {}
    };
    
    explicit SlashingAnomalyDetector(const Config& config = Config());
    
    /**
     * Check if recent slashing events are anomalous.
     * 
     * @param recent_events Slashing events in detection window
     * @param total_stake Total bonded stake
     * @param total_validators Total active validators
     * @param current_epoch Current epoch number
     * @return true if anomaly detected
     */
    bool is_anomalous(
        const std::vector<SlashingEvent>& recent_events,
        uint64_t total_stake,
        uint32_t total_validators,
        uint64_t current_epoch
    );
    
    /**
     * Get description of detected anomaly.
     * 
     * @return Anomaly description
     */
    std::string get_anomaly_reason() const;
    
private:
    Config config_;
    std::string anomaly_reason_;
};

/**
 * Circuit breaker for automatic network halt on anomalies.
 * 
 * Monitors slashing events and automatically halts consensus if
 * anomalous patterns detected. Requires manual intervention to resume.
 */
class CircuitBreaker {
public:
    explicit CircuitBreaker(const SlashingAnomalyDetector::Config& config = 
                           SlashingAnomalyDetector::Config());
    
    /**
     * Get current network state.
     * 
     * @return Network state (Normal, Halted, Resuming)
     */
    NetworkState get_state() const;
    
    /**
     * Check slashing events and potentially trigger halt.
     * 
     * @param recent_events Recent slashing events
     * @param total_stake Total bonded stake
     * @param total_validators Total active validators
     * @param current_epoch Current epoch
     * @param current_height Current block height
     * @param state_root Current state root
     * @return HaltMessage if halt triggered, nullopt otherwise
     */
    std::optional<HaltMessage> check_and_halt(
        const std::vector<SlashingEvent>& recent_events,
        uint64_t total_stake,
        uint32_t total_validators,
        uint64_t current_epoch,
        uint64_t current_height,
        const crypto::Blake3Hash& state_root
    );
    
    /**
     * Process a halt message from another validator.
     * 
     * @param halt_msg Halt message
     * @param validator_pubkeys Validator public keys for verification
     * @return true if halt accepted
     */
    bool process_halt_message(
        const HaltMessage& halt_msg,
        const std::vector<crypto::BLS12_381_PublicKey>& validator_pubkeys
    );
    
    /**
     * Process a resume message.
     * 
     * @param resume_msg Resume message
     * @param validator_pubkeys Validator public keys for verification
     * @return true if resume accepted
     */
    bool process_resume_message(
        const ResumeMessage& resume_msg,
        const std::vector<crypto::BLS12_381_PublicKey>& validator_pubkeys
    );
    
    /**
     * Get halt information if halted.
     * 
     * @return HaltMessage if halted, nullopt otherwise
     */
    std::optional<HaltMessage> get_halt_info() const;
    
private:
    SlashingAnomalyDetector detector_;
    NetworkState state_;
    std::optional<HaltMessage> halt_info_;
};

} // namespace consensus
} // namespace sarafu
