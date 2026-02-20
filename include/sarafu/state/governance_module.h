#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace sarafu {
namespace state {

/**
 * ParameterType defines the governance-modifiable parameters.
 * 
 * Requirements: 14.7
 */
enum class ParameterType {
    BlockSize,              // Maximum block size in bytes
    GasLimit,               // Maximum gas per block
    IssuanceCoefficient,    // k in rt = k·σ (0.05 ≤ k ≤ 0.2)
    SlashingAlpha,          // α in slashing formula
    SlashingBeta,           // β in slashing formula
    ValidatorSetSize,       // N (number of active validators)
    MinimumSelfBond,        // Minimum stake required for validators
    BaseFeeAdjustment       // γ in base fee adjustment formula
};

/**
 * ProposalStatus tracks the lifecycle of a governance proposal.
 * 
 * Requirements: 14.1, 14.2, 14.3
 */
enum class ProposalStatus {
    Pending,    // Proposal created but not yet active
    Active,     // Voting period in progress
    Approved,   // Passed voting, in timelock period
    Rejected,   // Failed to meet approval threshold or quorum
    Executed    // Timelock expired and parameter change applied
};

// Forward declaration for ValidatorID
using ValidatorID = uint64_t;
using Address = std::vector<uint8_t>;

/**
 * Proposal represents a governance proposal to change a protocol parameter.
 * 
 * A proposal goes through the following lifecycle:
 * 1. Submission: Requires ≥0.1% stake backing
 * 2. Voting: 7-day period with stake-weighted votes
 * 3. Finalization: Check quorum (≥10%) and approval (≥2/3)
 * 4. Timelock: Safety delay based on parameter type
 * 5. Execution: Apply parameter change
 * 
 * Requirements: 14.1, 14.2, 14.3, 14.4, 14.5, 14.6
 */
struct Proposal {
    uint64_t id;
    ParameterType parameter;
    std::string description;
    std::vector<uint8_t> new_value;
    Address proposer;
    uint64_t stake_backing;
    uint64_t created_at_height;
    uint64_t voting_ends_at_height;
    uint64_t timelock_ends_at_height;
    ProposalStatus status;
    uint64_t votes_for;
    uint64_t votes_against;
    std::map<ValidatorID, bool> votes;  // ValidatorID -> vote (true=for, false=against)

    Proposal()
        : id(0),
          parameter(ParameterType::BlockSize),
          stake_backing(0),
          created_at_height(0),
          voting_ends_at_height(0),
          timelock_ends_at_height(0),
          status(ProposalStatus::Pending),
          votes_for(0),
          votes_against(0) {}
};

/**
 * GovernanceModule manages on-chain parameter voting.
 * 
 * The governance system allows validators to propose and vote on protocol
 * parameter changes with the following rules:
 * 
 * - Proposal threshold: ≥0.1% of total stake backing required
 * - Voting period: 7 days (in blocks)
 * - Approval threshold: ≥2/3 of votes in favor
 * - Quorum: ≥10% participation
 * - Timelock durations:
 *   - Safety-critical (slashing, issuance): 14 days
 *   - Administrative (validator set size, min bond): 7 days
 *   - Performance (block size, gas limit): 3 days
 * 
 * Requirements: 14.1, 14.2, 14.3, 14.4, 14.5, 14.6, 14.7, 14.8
 */
class GovernanceModule {
public:
    /**
     * Construct a GovernanceModule.
     * 
     * @param blocks_per_day Number of blocks produced per day (default: 43200 for 2s blocks)
     */
    explicit GovernanceModule(uint64_t blocks_per_day = 43200);

    ~GovernanceModule();

    /**
     * Submit a proposal for voting.
     * 
     * Requirements:
     * - Backing stake must be ≥0.1% of total stake
     * - Parameter type must be valid
     * - New value must pass validation
     * 
     * @param parameter The parameter to modify
     * @param new_value The proposed new value (serialized)
     * @param description Human-readable description of the proposal
     * @param proposer Address of the proposer
     * @param backers List of validators backing the proposal
     * @param validator_stakes Map of validator stakes
     * @param total_stake Total bonded stake in the network
     * @param current_height Current block height
     * @return Proposal ID if successful, error otherwise
     * 
     * Requirements: 14.1
     */
    std::optional<uint64_t> submit_proposal(
        ParameterType parameter,
        const std::vector<uint8_t>& new_value,
        const std::string& description,
        const Address& proposer,
        const std::vector<ValidatorID>& backers,
        const std::map<ValidatorID, uint64_t>& validator_stakes,
        uint64_t total_stake,
        uint64_t current_height
    );

    /**
     * Vote on a proposal.
     * 
     * Requirements:
     * - Proposal must be in Active status
     * - Voter must be a validator with stake
     * - Voter cannot vote twice on the same proposal
     * - Voting period must not have ended
     * 
     * @param proposal_id The proposal to vote on
     * @param voter The validator casting the vote
     * @param in_favor true to vote for, false to vote against
     * @param voter_stake The stake weight of the voter
     * @param current_height Current block height
     * @return true if vote was recorded, false otherwise
     * 
     * Requirements: 14.2
     */
    bool vote(
        uint64_t proposal_id,
        ValidatorID voter,
        bool in_favor,
        uint64_t voter_stake,
        uint64_t current_height
    );

    /**
     * Finalize voting on a proposal after the voting period ends.
     * 
     * Checks:
     * - Voting period has ended
     * - Quorum: ≥10% of total stake participated
     * - Approval: ≥2/3 of votes in favor
     * 
     * If approved, sets timelock based on parameter type.
     * If rejected, sets status to Rejected.
     * 
     * @param proposal_id The proposal to finalize
     * @param total_stake Total bonded stake in the network
     * @param current_height Current block height
     * @return true if finalization succeeded, false otherwise
     * 
     * Requirements: 14.3, 14.4, 14.5, 14.6
     */
    bool finalize_voting(
        uint64_t proposal_id,
        uint64_t total_stake,
        uint64_t current_height
    );

    /**
     * Execute a proposal after the timelock period expires.
     * 
     * Requirements:
     * - Proposal must be in Approved status
     * - Timelock period must have expired
     * - Parameter value must still be valid
     * 
     * Note: This method marks the proposal as Executed but does not
     * actually apply the parameter change. The caller must apply the
     * change to the appropriate system component.
     * 
     * @param proposal_id The proposal to execute
     * @param current_height Current block height
     * @return true if execution succeeded, false otherwise
     * 
     * Requirements: 14.7
     */
    bool execute_proposal(
        uint64_t proposal_id,
        uint64_t current_height
    );

    /**
     * Get a proposal by ID.
     * 
     * @param proposal_id The proposal ID
     * @return The proposal if found, std::nullopt otherwise
     */
    std::optional<Proposal> get_proposal(uint64_t proposal_id) const;

    /**
     * Get all proposals.
     * 
     * @return Map of proposal ID to Proposal
     */
    const std::map<uint64_t, Proposal>& get_all_proposals() const {
        return proposals_;
    }

    /**
     * Get the number of proposals.
     * 
     * @return Number of proposals
     */
    size_t proposal_count() const {
        return proposals_.size();
    }

    /**
     * Validate a parameter value.
     * 
     * Checks that the proposed value is within acceptable bounds:
     * - IssuanceCoefficient: 0.05 ≤ k ≤ 0.2
     * - Other parameters: type-specific validation
     * 
     * @param type The parameter type
     * @param value The proposed value (serialized)
     * @return true if valid, false otherwise
     * 
     * Requirements: 11.5, 14.7
     */
    bool validate_parameter_value(ParameterType type, const std::vector<uint8_t>& value) const;

private:
    /**
     * Calculate timelock duration based on parameter type.
     * 
     * Timelock durations:
     * - Safety-critical (IssuanceCoefficient, SlashingAlpha, SlashingBeta): 14 days
     * - Administrative (ValidatorSetSize, MinimumSelfBond): 7 days
     * - Performance (BlockSize, GasLimit, BaseFeeAdjustment): 3 days
     * 
     * @param type The parameter type
     * @return Timelock duration in blocks
     * 
     * Requirements: 14.4, 14.5, 14.6
     */
    uint64_t calculate_timelock(ParameterType type) const;

    std::map<uint64_t, Proposal> proposals_;
    uint64_t next_proposal_id_;
    uint64_t blocks_per_day_;

    // Constants
    static constexpr double PROPOSAL_THRESHOLD = 0.001;  // 0.1%
    static constexpr double APPROVAL_THRESHOLD = 0.667;  // 2/3
    static constexpr double QUORUM_THRESHOLD = 0.10;     // 10%
    static constexpr uint64_t VOTING_PERIOD_DAYS = 7;
    static constexpr uint64_t TIMELOCK_SAFETY_DAYS = 14;
    static constexpr uint64_t TIMELOCK_ADMIN_DAYS = 7;
    static constexpr uint64_t TIMELOCK_PERFORMANCE_DAYS = 3;
};

} // namespace state
} // namespace sarafu
