#pragma once

#include "sarafu/consensus/block.h"
#include "sarafu/consensus/validator.h"
#include "sarafu/consensus/vote_aggregator.h"
#include "sarafu/consensus/qc_verifier.h"
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/state/state_machine.h"
#include "sarafu/crypto/blake3_hash.h"
#include <map>
#include <optional>
#include <memory>

namespace sarafu {

// Forward declarations
namespace state {
    class Mempool;
}

namespace consensus {

/**
 * ConsensusEngine implements the HotStuff BFT consensus protocol.
 * 
 * HotStuff provides:
 * - O(n) communication complexity (linear in number of validators)
 * - Immediate finalization via Quorum Certificates
 * - Safety when <1/3 of total stake is Byzantine
 * - Liveness when ≥2/3 of total stake is responsive
 * 
 * The protocol uses a three-phase commit:
 * 1. Prepare: Leader proposes block with QC for parent
 * 2. Pre-Commit: Validators vote if block is valid and extends highest QC
 * 3. Commit: Leader aggregates votes into QC
 * 4. Finalize: Block with QC is finalized (irreversible)
 * 
 * Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7
 */
class ConsensusEngine {
public:
    /**
     * Configuration parameters for the consensus engine.
     */
    struct Config {
        uint64_t block_time_ms;           // Target block time in milliseconds (default: 2000ms)
        uint64_t view_timeout_ms;         // Timeout for view change (default: 4000ms)
        uint64_t max_transactions_per_block;  // Maximum transactions per block
        uint64_t max_block_gas;           // Maximum gas per block
        
        Config()
            : block_time_ms(2000),
              view_timeout_ms(4000),
              max_transactions_per_block(10000),
              max_block_gas(30000000) {}
    };

    /**
     * Construct a ConsensusEngine.
     * 
     * @param validator_registry The validator registry
     * @param state_machine The state machine for transaction execution
     * @param config Configuration parameters
     */
    ConsensusEngine(
        std::shared_ptr<ValidatorRegistry> validator_registry,
        std::shared_ptr<state::StateMachine> state_machine,
        const Config& config = Config()
    );

    ~ConsensusEngine();

    /**
     * Get the current view number.
     * 
     * The view number increments on each view change (leader timeout).
     * 
     * @return Current view number
     * 
     * Requirements: 1.1
     */
    uint64_t current_view() const { return current_view_; }

    /**
     * Get the highest Quorum Certificate.
     * 
     * The highest QC represents the most recent finalized block.
     * 
     * @return The highest QC
     * 
     * Requirements: 1.1
     */
    const QuorumCertificate& highest_qc() const { return highest_qc_; }

    /**
     * Get the finalized height.
     * 
     * This is the height of the block referenced by the highest QC.
     * 
     * @return Finalized block height
     * 
     * Requirements: 1.1, 1.3
     */
    uint64_t finalized_height() const { return finalized_height_; }

    /**
     * Get the current block height.
     * 
     * @return Current block height
     */
    uint64_t current_height() const { return current_height_; }

    /**
     * Check if a block is finalized.
     * 
     * A block is finalized if it has a valid QC with ≥2/3 stake signatures.
     * 
     * @param block_hash The block hash to check
     * @return true if the block is finalized, false otherwise
     * 
     * Requirements: 1.3
     */
    bool is_finalized(const crypto::Blake3Hash& block_hash) const;

    /**
     * Mark a block as finalized.
     * 
     * This updates the finalized_height and highest_qc when a QC is produced.
     * 
     * @param block_hash The block hash
     * @param qc The Quorum Certificate
     * 
     * Requirements: 1.3
     */
    void mark_finalized(const crypto::Blake3Hash& block_hash, const QuorumCertificate& qc);

    /**
     * Propose a new block (called by leader).
     * 
     * The leader:
     * 1. Selects transactions from mempool
     * 2. Creates block with parent QC (justify)
     * 3. Signs block with validator's BLS key
     * 4. Broadcasts block to validators
     * 
     * @param proposer_id The proposer's validator ID
     * @param proposer_key The proposer's BLS private key
     * @param mempool The mempool to select transactions from
     * @param base_fee The current base fee
     * @return The proposed block, or std::nullopt if proposal failed
     * 
     * Requirements: 1.4
     */
    std::optional<Block> propose_block(
        const ValidatorID& proposer_id,
        const crypto::BLS12_381_PrivateKey& proposer_key,
        state::Mempool& mempool,
        uint64_t base_fee
    );

    /**
     * Process a received block.
     * 
     * This validates the block and creates a vote if valid:
     * 1. Verify block extends highest QC
     * 2. Verify block proposer is current leader
     * 3. Verify all transactions are valid
     * 4. Create and broadcast vote if block is valid
     * 
     * @param block The received block
     * @return true if block is valid and vote was created, false otherwise
     * 
     * Requirements: 1.2
     */
    bool on_receive_block(const Block& block);

    /**
     * Process a received vote.
     * 
     * This adds the vote to the aggregator and attempts to create a QC
     * if ≥2/3 stake has voted for the block.
     * 
     * @param vote The received vote
     * @return QuorumCertificate if ≥2/3 stake reached, std::nullopt otherwise
     * 
     * Requirements: 1.2
     */
    std::optional<QuorumCertificate> on_receive_vote(const Vote& vote);

    /**
     * Trigger a view change due to leader timeout.
     * 
     * This increments the view number and selects a new leader using
     * round-robin by stake.
     * 
     * @return true if view change was successful, false otherwise
     * 
     * Requirements: 1.5
     */
    bool trigger_view_change();

    /**
     * Get the current leader for the current view.
     * 
     * Leader selection uses round-robin by stake:
     * leader_index = view_number % active_validator_count
     * 
     * @return The current leader's validator ID, or std::nullopt if no validators
     * 
     * Requirements: 1.5
     */
    std::optional<ValidatorID> get_current_leader() const;

    /**
     * Set the genesis block.
     * 
     * This initializes the consensus engine with the genesis block.
     * 
     * @param genesis_block The genesis block
     */
    void set_genesis_block(const Block& genesis_block);

    /**
     * Get a block by hash.
     * 
     * @param block_hash The block hash
     * @return The block if found, std::nullopt otherwise
     */
    std::optional<Block> get_block(const crypto::Blake3Hash& block_hash) const;

    /**
     * Get a block by height.
     * 
     * @param height The block height
     * @return The block if found, std::nullopt otherwise
     */
    std::optional<Block> get_block_by_height(uint64_t height) const;

    /**
     * Store a block in the block store.
     * 
     * @param block The block to store
     */
    void store_block(const Block& block);

private:
    /**
     * Verify block validity.
     * 
     * Checks:
     * - Block height is exactly one greater than parent
     * - Previous block hash matches parent
     * - Timestamp is greater than parent
     * - Proposer is in active validator set
     * - Block extends highest QC
     * - All transactions are valid
     * - State root matches computed state
     * 
     * @param block The block to verify
     * @return true if block is valid, false otherwise
     */
    bool verify_block(const Block& block) const;

    /**
     * Verify that a block extends the highest QC.
     * 
     * @param block The block to verify
     * @return true if block extends highest QC, false otherwise
     */
    bool extends_highest_qc(const Block& block) const;

    /**
     * Verify that the proposer is the current leader.
     * 
     * @param proposer_id The proposer's validator ID
     * @return true if proposer is the current leader, false otherwise
     */
    bool is_current_leader(const ValidatorID& proposer_id) const;

    /**
     * Create a vote for a block.
     * 
     * @param block The block to vote for
     * @param voter_id The voter's validator ID
     * @param voter_key The voter's BLS private key
     * @return The vote
     */
    Vote create_vote(
        const Block& block,
        const ValidatorID& voter_id,
        const crypto::BLS12_381_PrivateKey& voter_key
    ) const;

    /**
     * Select the leader for a given view.
     * 
     * Uses round-robin by stake: leader_index = view_number % active_validator_count
     * 
     * @param view_number The view number
     * @return The leader's validator ID, or std::nullopt if no validators
     */
    std::optional<ValidatorID> select_leader(uint64_t view_number) const;

    // Configuration
    Config config_;

    // Validator registry
    std::shared_ptr<ValidatorRegistry> validator_registry_;

    // State machine
    std::shared_ptr<state::StateMachine> state_machine_;

    // Vote aggregator
    std::unique_ptr<VoteAggregator> vote_aggregator_;

    // Current view number
    uint64_t current_view_;

    // Highest Quorum Certificate
    QuorumCertificate highest_qc_;

    // Finalized block height
    uint64_t finalized_height_;

    // Current block height
    uint64_t current_height_;

    // Block store: hash -> block
    std::map<crypto::Blake3Hash, Block> block_store_;

    // Block height index: height -> hash
    std::map<uint64_t, crypto::Blake3Hash> height_index_;

    // Finalized blocks: hash -> true
    std::map<crypto::Blake3Hash, bool> finalized_blocks_;
};

} // namespace consensus
} // namespace sarafu
