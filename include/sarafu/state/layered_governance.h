#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "sarafu/state/account.h"
#include "sarafu/crypto/blake3_hash.h"

namespace sarafu {
namespace consensus {
    struct Validator;
    using ValidatorID = state::Address;
}

namespace state {

/**
 * GovernanceLayer defines the three governance tiers.
 */
enum class GovernanceLayer {
    Security,        // Layer 1: Linear voting (1 SAR = 1 vote)
    Treasury,        // Layer 2: Quadratic voting with caps
    Constitutional   // Layer 3: Supermajority + timelock
};

/**
 * ProposalType categorizes proposals by governance layer.
 */
enum class ProposalType {
    // Layer 1 - Security Governance
    SlashingParameters,
    ValidatorSetSize,
    BlockTime,
    
    // Layer 2 - Treasury Governance
    TreasuryAllocation,
    GrantProposal,
    FundingRequest,
    
    // Layer 3 - Constitutional Governance
    InflationBand,
    IssuanceRules,
    TreasuryCaps,
    GovernanceRules
};

/**
 * LayeredProposalStatus tracks proposal lifecycle.
 */
enum class LayeredProposalStatus {
    Pending,              // Created but not active
    Active,               // Voting in progress
    ValidatorApproved,    // Layer 3: Passed validator vote
    TreasuryApproved,     // Layer 3: Passed treasury vote
    InTimelock,           // Layer 3: In 21-day timelock
    Approved,             // Passed all requirements
    Rejected,             // Failed to meet thresholds
    Executed              // Applied to chain
};

/**
 * VotingPower represents a voter's power in different governance layers.
 */
struct VotingPower {
    uint64_t security_power;      // Layer 1: Linear (bonded_stake)
    uint64_t treasury_power;      // Layer 2: Quadratic with cap
    bool validator_vote;          // Layer 3: Validator approval
    bool treasury_vote;           // Layer 3: Treasury approval
    
    VotingPower()
        : security_power(0),
          treasury_power(0),
          validator_vote(false),
          treasury_vote(false) {}
};

/**
 * LayeredProposal represents a governance proposal in the three-layer system.
 */
struct LayeredProposal {
    uint64_t id;
    ProposalType type;
    GovernanceLayer layer;
    std::string description;
    std::vector<uint8_t> new_value;
    Address proposer;
    uint64_t created_at_height;
    uint64_t voting_ends_at_height;
    uint64_t timelock_ends_at_height;
    LayeredProposalStatus status;
    
    // Layer 1 & 2 voting
    uint64_t votes_for;
    uint64_t votes_against;
    std::map<consensus::ValidatorID, bool> voter_choices;
    
    // Layer 3 voting
    uint64_t validator_votes_for;
    uint64_t validator_votes_against;
    uint64_t treasury_votes_for;
    uint64_t treasury_votes_against;
    std::set<consensus::ValidatorID> validator_voters;
    std::set<consensus::ValidatorID> treasury_voters;
    
    LayeredProposal()
        : id(0),
          type(ProposalType::SlashingParameters),
          layer(GovernanceLayer::Security),
          created_at_height(0),
          voting_ends_at_height(0),
          timelock_ends_at_height(0),
          status(LayeredProposalStatus::Pending),
          votes_for(0),
          votes_against(0),
          validator_votes_for(0),
          validator_votes_against(0),
          treasury_votes_for(0),
          treasury_votes_against(0) {}
};

/**
 * LayeredGovernance implements the three-tier governance system.
 * 
 * Layer 1 - Security Governance:
 * - Linear voting: 1 bonded SAR = 1 vote
 * - Applies to: slashing, validator set size, block time
 * - Threshold: Simple majority (>50%)
 * 
 * Layer 2 - Treasury Governance:
 * - Voting power = min(sqrt(bonded_stake), 2% of total governance weight cap)
 * - Aggregate voting weight per bonded validator cluster
 * - Applies to: treasury allocations, grants, funding
 * - Threshold: 2/3 majority
 * 
 * Layer 3 - Constitutional Governance:
 * - Requires: ≥2/3 validator approval AND ≥2/3 treasury approval
 * - 21-day timelock before execution
 * - Applies to: inflation band, issuance rules, treasury caps
 */
class LayeredGovernance {
public:
    explicit LayeredGovernance(uint64_t blocks_per_day = 43200);
    
    /**
     * Submit a proposal to the appropriate governance layer.
     * 
     * @param type Proposal type
     * @param new_value Proposed parameter value
     * @param description Human-readable description
     * @param proposer Proposer address
     * @param current_height Current block height
     * @return Proposal ID if successful
     */
    std::optional<uint64_t> submit_proposal(
        ProposalType type,
        const std::vector<uint8_t>& new_value,
        const std::string& description,
        const Address& proposer,
        uint64_t current_height
    );
    
    /**
     * Calculate voting power for a validator in each layer.
     * 
     * @param validator_id Validator ID
     * @param bonded_stake Validator's bonded stake
     * @param total_governance_weight Total governance weight
     * @param is_validator Whether this is an active validator
     * @return Voting power in each layer
     */
    VotingPower calculate_voting_power(
        const consensus::ValidatorID& validator_id,
        uint64_t bonded_stake,
        uint64_t total_governance_weight,
        bool is_validator
    ) const;
    
    /**
     * Cast a vote on a proposal.
     * 
     * @param proposal_id Proposal ID
     * @param voter Voter's validator ID
     * @param in_favor Vote choice (true = for, false = against)
     * @param voting_power Voter's calculated voting power
     * @param current_height Current block height
     * @return true if vote recorded successfully
     */
    bool vote(
        uint64_t proposal_id,
        const consensus::ValidatorID& voter,
        bool in_favor,
        const VotingPower& voting_power,
        uint64_t current_height
    );
    
    /**
     * Finalize voting on a proposal.
     * 
     * Checks thresholds based on governance layer:
     * - Layer 1: >50% of votes
     * - Layer 2: ≥2/3 of votes
     * - Layer 3: ≥2/3 validator votes AND ≥2/3 treasury votes
     * 
     * @param proposal_id Proposal ID
     * @param total_validator_stake Total validator stake
     * @param total_governance_weight Total governance weight
     * @param current_height Current block height
     * @return true if finalization succeeded
     */
    bool finalize_voting(
        uint64_t proposal_id,
        uint64_t total_validator_stake,
        uint64_t total_governance_weight,
        uint64_t current_height
    );
    
    /**
     * Execute a proposal after timelock (Layer 3 only).
     * 
     * @param proposal_id Proposal ID
     * @param current_height Current block height
     * @return true if execution succeeded
     */
    bool execute_proposal(
        uint64_t proposal_id,
        uint64_t current_height
    );
    
    /**
     * Get a proposal by ID.
     * 
     * @param proposal_id Proposal ID
     * @return Proposal if found
     */
    std::optional<LayeredProposal> get_proposal(uint64_t proposal_id) const;
    
    /**
     * Get all proposals.
     * 
     * @return Map of proposal ID to proposal
     */
    const std::map<uint64_t, LayeredProposal>& get_all_proposals() const {
        return proposals_;
    }
    
    /**
     * Lock voting stake during proposal period.
     * 
     * @param voter Voter's validator ID
     * @param proposal_id Proposal ID
     * @return true if stake locked successfully
     */
    bool lock_voting_stake(
        const consensus::ValidatorID& voter,
        uint64_t proposal_id
    );
    
    /**
     * Unlock voting stake after proposal completes.
     * 
     * @param voter Voter's validator ID
     * @param proposal_id Proposal ID
     * @return true if stake unlocked successfully
     */
    bool unlock_voting_stake(
        const consensus::ValidatorID& voter,
        uint64_t proposal_id
    );
    
    /**
     * Check if a validator's stake is locked for voting.
     * 
     * @param voter Voter's validator ID
     * @return true if stake is locked
     */
    bool is_stake_locked(const consensus::ValidatorID& voter) const;

private:
    /**
     * Determine governance layer for a proposal type.
     * 
     * @param type Proposal type
     * @return Governance layer
     */
    GovernanceLayer get_layer_for_type(ProposalType type) const;
    
    /**
     * Calculate timelock duration for Layer 3 proposals.
     * 
     * @return Timelock duration in blocks (21 days)
     */
    uint64_t calculate_constitutional_timelock() const;
    
    std::map<uint64_t, LayeredProposal> proposals_;
    std::map<consensus::ValidatorID, std::set<uint64_t>> locked_stakes_;
    uint64_t next_proposal_id_;
    uint64_t blocks_per_day_;
    
    // Constants
    static constexpr double LAYER1_THRESHOLD = 0.50;         // >50%
    static constexpr double LAYER2_THRESHOLD = 0.667;        // ≥2/3
    static constexpr double LAYER3_THRESHOLD = 0.667;        // ≥2/3
    static constexpr double TREASURY_CAP_PERCENT = 0.02;     // 2% cap
    static constexpr uint64_t VOTING_PERIOD_DAYS = 7;
    static constexpr uint64_t CONSTITUTIONAL_TIMELOCK_DAYS = 21;
};

} // namespace state
} // namespace sarafu
