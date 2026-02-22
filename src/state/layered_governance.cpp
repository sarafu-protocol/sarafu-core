#include "sarafu/state/layered_governance.h"
#include <cmath>
#include <algorithm>

namespace sarafu {
namespace state {

LayeredGovernance::LayeredGovernance(uint64_t blocks_per_day)
    : next_proposal_id_(1),
      blocks_per_day_(blocks_per_day) {}

GovernanceLayer LayeredGovernance::get_layer_for_type(ProposalType type) const {
    switch (type) {
        case ProposalType::SlashingParameters:
        case ProposalType::ValidatorSetSize:
        case ProposalType::BlockTime:
            return GovernanceLayer::Security;
        
        case ProposalType::TreasuryAllocation:
        case ProposalType::GrantProposal:
        case ProposalType::FundingRequest:
            return GovernanceLayer::Treasury;
        
        case ProposalType::InflationBand:
        case ProposalType::IssuanceRules:
        case ProposalType::TreasuryCaps:
        case ProposalType::GovernanceRules:
            return GovernanceLayer::Constitutional;
        
        default:
            return GovernanceLayer::Security;
    }
}

std::optional<uint64_t> LayeredGovernance::submit_proposal(
    ProposalType type,
    const std::vector<uint8_t>& new_value,
    const std::string& description,
    const Address& proposer,
    uint64_t current_height
) {
    LayeredProposal proposal;
    proposal.id = next_proposal_id_++;
    proposal.type = type;
    proposal.layer = get_layer_for_type(type);
    proposal.description = description;
    proposal.new_value = new_value;
    proposal.proposer = proposer;
    proposal.created_at_height = current_height;
    proposal.voting_ends_at_height = current_height + (VOTING_PERIOD_DAYS * blocks_per_day_);
    proposal.status = LayeredProposalStatus::Active;
    
    // Set timelock for Layer 3 proposals
    if (proposal.layer == GovernanceLayer::Constitutional) {
        proposal.timelock_ends_at_height = proposal.voting_ends_at_height + 
                                          calculate_constitutional_timelock();
    }
    
    proposals_[proposal.id] = proposal;
    return proposal.id;
}

VotingPower LayeredGovernance::calculate_voting_power(
    const consensus::ValidatorID& validator_id,
    uint64_t bonded_stake,
    uint64_t total_governance_weight,
    bool is_validator
) const {
    VotingPower power;
    
    // Layer 1: Linear voting (1 bonded SAR = 1 vote)
    power.security_power = bonded_stake;
    
    // Layer 2: Quadratic voting with 2% cap
    // voting_power = min(sqrt(bonded_stake), 2% of total governance weight)
    double sqrt_stake = std::sqrt(static_cast<double>(bonded_stake));
    double cap = static_cast<double>(total_governance_weight) * TREASURY_CAP_PERCENT;
    power.treasury_power = static_cast<uint64_t>(std::min(sqrt_stake, cap));
    
    // Layer 3: Validator and treasury assembly votes
    power.validator_vote = is_validator;
    power.treasury_vote = (bonded_stake > 0);  // Any bonded stake can vote in treasury assembly
    
    return power;
}

bool LayeredGovernance::vote(
    uint64_t proposal_id,
    const consensus::ValidatorID& voter,
    bool in_favor,
    const VotingPower& voting_power,
    uint64_t current_height
) {
    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        return false;
    }
    
    LayeredProposal& proposal = it->second;
    
    // Check proposal is active
    if (proposal.status != LayeredProposalStatus::Active) {
        return false;
    }
    
    // Check voting period hasn't ended
    if (current_height >= proposal.voting_ends_at_height) {
        return false;
    }
    
    // Check if already voted
    if (proposal.voter_choices.count(voter) > 0) {
        return false;
    }
    
    // Record vote based on governance layer
    proposal.voter_choices[voter] = in_favor;
    
    switch (proposal.layer) {
        case GovernanceLayer::Security:
            // Layer 1: Linear voting
            if (in_favor) {
                proposal.votes_for += voting_power.security_power;
            } else {
                proposal.votes_against += voting_power.security_power;
            }
            break;
        
        case GovernanceLayer::Treasury:
            // Layer 2: Quadratic voting with cap
            if (in_favor) {
                proposal.votes_for += voting_power.treasury_power;
            } else {
                proposal.votes_against += voting_power.treasury_power;
            }
            break;
        
        case GovernanceLayer::Constitutional:
            // Layer 3: Dual approval system
            if (voting_power.validator_vote) {
                proposal.validator_voters.insert(voter);
                if (in_favor) {
                    proposal.validator_votes_for++;
                } else {
                    proposal.validator_votes_against++;
                }
            }
            if (voting_power.treasury_vote) {
                proposal.treasury_voters.insert(voter);
                if (in_favor) {
                    proposal.treasury_votes_for += voting_power.treasury_power;
                } else {
                    proposal.treasury_votes_against += voting_power.treasury_power;
                }
            }
            break;
    }
    
    // Lock voting stake
    lock_voting_stake(voter, proposal_id);
    
    return true;
}

bool LayeredGovernance::finalize_voting(
    uint64_t proposal_id,
    uint64_t total_validator_stake,
    uint64_t total_governance_weight,
    uint64_t current_height
) {
    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        return false;
    }
    
    LayeredProposal& proposal = it->second;
    
    // Check voting period has ended
    if (current_height < proposal.voting_ends_at_height) {
        return false;
    }
    
    // Check proposal is active
    if (proposal.status != LayeredProposalStatus::Active) {
        return false;
    }
    
    bool approved = false;
    
    switch (proposal.layer) {
        case GovernanceLayer::Security: {
            // Layer 1: Simple majority (>50%)
            uint64_t total_votes = proposal.votes_for + proposal.votes_against;
            if (total_votes > 0) {
                double approval_rate = static_cast<double>(proposal.votes_for) / 
                                      static_cast<double>(total_votes);
                approved = (approval_rate > LAYER1_THRESHOLD);
            }
            break;
        }
        
        case GovernanceLayer::Treasury: {
            // Layer 2: 2/3 majority
            uint64_t total_votes = proposal.votes_for + proposal.votes_against;
            if (total_votes > 0) {
                double approval_rate = static_cast<double>(proposal.votes_for) / 
                                      static_cast<double>(total_votes);
                approved = (approval_rate >= LAYER2_THRESHOLD);
            }
            break;
        }
        
        case GovernanceLayer::Constitutional: {
            // Layer 3: 2/3 validator approval AND 2/3 treasury approval
            uint64_t total_validator_votes = proposal.validator_votes_for + 
                                            proposal.validator_votes_against;
            uint64_t total_treasury_votes = proposal.treasury_votes_for + 
                                           proposal.treasury_votes_against;
            
            bool validator_approved = false;
            bool treasury_approved = false;
            
            if (total_validator_votes > 0) {
                double validator_rate = static_cast<double>(proposal.validator_votes_for) / 
                                       static_cast<double>(total_validator_votes);
                validator_approved = (validator_rate >= LAYER3_THRESHOLD);
            }
            
            if (total_treasury_votes > 0) {
                double treasury_rate = static_cast<double>(proposal.treasury_votes_for) / 
                                      static_cast<double>(total_treasury_votes);
                treasury_approved = (treasury_rate >= LAYER3_THRESHOLD);
            }
            
            approved = validator_approved && treasury_approved;
            
            if (validator_approved && !treasury_approved) {
                proposal.status = LayeredProposalStatus::ValidatorApproved;
            } else if (!validator_approved && treasury_approved) {
                proposal.status = LayeredProposalStatus::TreasuryApproved;
            }
            break;
        }
    }
    
    if (approved) {
        if (proposal.layer == GovernanceLayer::Constitutional) {
            proposal.status = LayeredProposalStatus::InTimelock;
        } else {
            proposal.status = LayeredProposalStatus::Approved;
        }
    } else {
        proposal.status = LayeredProposalStatus::Rejected;
    }
    
    // Unlock voting stakes for all voters
    for (const auto& [voter, _] : proposal.voter_choices) {
        unlock_voting_stake(voter, proposal_id);
    }
    
    return true;
}

bool LayeredGovernance::execute_proposal(
    uint64_t proposal_id,
    uint64_t current_height
) {
    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        return false;
    }
    
    LayeredProposal& proposal = it->second;
    
    // Check proposal status
    if (proposal.layer == GovernanceLayer::Constitutional) {
        // Layer 3: Check timelock has expired
        if (proposal.status != LayeredProposalStatus::InTimelock) {
            return false;
        }
        if (current_height < proposal.timelock_ends_at_height) {
            return false;
        }
    } else {
        // Layer 1 & 2: Check approved status
        if (proposal.status != LayeredProposalStatus::Approved) {
            return false;
        }
    }
    
    // Mark as executed
    proposal.status = LayeredProposalStatus::Executed;
    
    return true;
}

std::optional<LayeredProposal> LayeredGovernance::get_proposal(uint64_t proposal_id) const {
    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool LayeredGovernance::lock_voting_stake(
    const consensus::ValidatorID& voter,
    uint64_t proposal_id
) {
    locked_stakes_[voter].insert(proposal_id);
    return true;
}

bool LayeredGovernance::unlock_voting_stake(
    const consensus::ValidatorID& voter,
    uint64_t proposal_id
) {
    auto it = locked_stakes_.find(voter);
    if (it != locked_stakes_.end()) {
        it->second.erase(proposal_id);
        if (it->second.empty()) {
            locked_stakes_.erase(it);
        }
    }
    return true;
}

bool LayeredGovernance::is_stake_locked(const consensus::ValidatorID& voter) const {
    auto it = locked_stakes_.find(voter);
    return (it != locked_stakes_.end() && !it->second.empty());
}

uint64_t LayeredGovernance::calculate_constitutional_timelock() const {
    return CONSTITUTIONAL_TIMELOCK_DAYS * blocks_per_day_;
}

} // namespace state
} // namespace sarafu
