#include "sarafu/state/governance_module.h"
#include <algorithm>
#include <cstring>

namespace sarafu {
namespace state {

GovernanceModule::GovernanceModule(uint64_t blocks_per_day)
    : next_proposal_id_(1), blocks_per_day_(blocks_per_day) {}

GovernanceModule::~GovernanceModule() = default;

std::optional<uint64_t> GovernanceModule::submit_proposal(
    ParameterType parameter,
    const std::vector<uint8_t>& new_value,
    const std::string& description,
    const Address& proposer,
    const std::vector<ValidatorID>& backers,
    const std::map<ValidatorID, uint64_t>& validator_stakes,
    uint64_t total_stake,
    uint64_t current_height
) {
    // Validate parameter value
    if (!validate_parameter_value(parameter, new_value)) {
        return std::nullopt;
    }

    // Calculate total backing stake
    uint64_t backing_stake = 0;
    for (const auto& backer : backers) {
        auto it = validator_stakes.find(backer);
        if (it != validator_stakes.end()) {
            backing_stake += it->second;
        }
    }

    // Check proposal threshold (≥0.1% of total stake)
    // Use integer arithmetic to avoid floating point precision issues
    // backing_stake >= total_stake * 0.001
    // backing_stake * 1000 >= total_stake
    if (backing_stake * 1000 < total_stake) {
        return std::nullopt;
    }

    // Create proposal
    Proposal proposal;
    proposal.id = next_proposal_id_++;
    proposal.parameter = parameter;
    proposal.description = description;
    proposal.new_value = new_value;
    proposal.proposer = proposer;
    proposal.stake_backing = backing_stake;
    proposal.created_at_height = current_height;
    proposal.voting_ends_at_height = current_height + (VOTING_PERIOD_DAYS * blocks_per_day_);
    proposal.timelock_ends_at_height = 0;  // Set after approval
    proposal.status = ProposalStatus::Active;
    proposal.votes_for = 0;
    proposal.votes_against = 0;

    proposals_[proposal.id] = proposal;

    return proposal.id;
}

bool GovernanceModule::vote(
    uint64_t proposal_id,
    ValidatorID voter,
    bool in_favor,
    uint64_t voter_stake,
    uint64_t current_height
) {
    // Find proposal
    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        return false;
    }

    Proposal& proposal = it->second;

    // Check proposal is active
    if (proposal.status != ProposalStatus::Active) {
        return false;
    }

    // Check voting period hasn't ended
    if (current_height >= proposal.voting_ends_at_height) {
        return false;
    }

    // Check voter hasn't already voted
    if (proposal.votes.find(voter) != proposal.votes.end()) {
        return false;
    }

    // Check voter has stake
    if (voter_stake == 0) {
        return false;
    }

    // Record vote
    proposal.votes[voter] = in_favor;

    // Update vote tallies
    if (in_favor) {
        proposal.votes_for += voter_stake;
    } else {
        proposal.votes_against += voter_stake;
    }

    return true;
}

bool GovernanceModule::finalize_voting(
    uint64_t proposal_id,
    uint64_t total_stake,
    uint64_t current_height
) {
    // Find proposal
    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        return false;
    }

    Proposal& proposal = it->second;

    // Check proposal is active
    if (proposal.status != ProposalStatus::Active) {
        return false;
    }

    // Check voting period has ended
    if (current_height < proposal.voting_ends_at_height) {
        return false;
    }

    // Calculate participation
    uint64_t total_votes = proposal.votes_for + proposal.votes_against;
    
    // Check quorum (≥10% participation)
    // Use integer arithmetic to avoid floating point precision issues
    // total_votes >= total_stake * 0.10
    // Multiply both sides by 10: total_votes * 10 >= total_stake
    if (total_votes * 10 < total_stake) {
        proposal.status = ProposalStatus::Rejected;
        return true;
    }

    // Check approval (≥2/3 of votes in favor)
    // Use integer arithmetic to avoid floating point precision issues
    // votes_for >= total_votes * 2/3
    // votes_for * 3 >= total_votes * 2
    if (proposal.votes_for * 3 >= total_votes * 2) {
        // Approved - set timelock
        proposal.status = ProposalStatus::Approved;
        uint64_t timelock_duration = calculate_timelock(proposal.parameter);
        proposal.timelock_ends_at_height = current_height + timelock_duration;
    } else {
        // Rejected
        proposal.status = ProposalStatus::Rejected;
    }

    return true;
}

bool GovernanceModule::execute_proposal(
    uint64_t proposal_id,
    uint64_t current_height
) {
    // Find proposal
    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        return false;
    }

    Proposal& proposal = it->second;

    // Check proposal is approved
    if (proposal.status != ProposalStatus::Approved) {
        return false;
    }

    // Check timelock has expired
    if (current_height < proposal.timelock_ends_at_height) {
        return false;
    }

    // Validate parameter value again (in case bounds changed)
    if (!validate_parameter_value(proposal.parameter, proposal.new_value)) {
        return false;
    }

    // Mark as executed
    proposal.status = ProposalStatus::Executed;

    return true;
}

std::optional<Proposal> GovernanceModule::get_proposal(uint64_t proposal_id) const {
    auto it = proposals_.find(proposal_id);
    if (it == proposals_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool GovernanceModule::validate_parameter_value(
    ParameterType type,
    const std::vector<uint8_t>& value
) const {
    switch (type) {
        case ParameterType::IssuanceCoefficient: {
            // Value should be a double: 0.05 ≤ k ≤ 0.2
            if (value.size() != sizeof(double)) {
                return false;
            }
            double k;
            std::memcpy(&k, value.data(), sizeof(double));
            return k >= 0.05 && k <= 0.2;
        }

        case ParameterType::SlashingAlpha: {
            // Value should be a double: 0 < α ≤ 1
            if (value.size() != sizeof(double)) {
                return false;
            }
            double alpha;
            std::memcpy(&alpha, value.data(), sizeof(double));
            return alpha > 0.0 && alpha <= 1.0;
        }

        case ParameterType::SlashingBeta: {
            // Value should be a double: 0 < β ≤ 1
            if (value.size() != sizeof(double)) {
                return false;
            }
            double beta;
            std::memcpy(&beta, value.data(), sizeof(double));
            return beta > 0.0 && beta <= 1.0;
        }

        case ParameterType::BlockSize: {
            // Value should be a uint64_t: > 0
            if (value.size() != sizeof(uint64_t)) {
                return false;
            }
            uint64_t size;
            std::memcpy(&size, value.data(), sizeof(uint64_t));
            return size > 0;
        }

        case ParameterType::GasLimit: {
            // Value should be a uint64_t: > 0
            if (value.size() != sizeof(uint64_t)) {
                return false;
            }
            uint64_t limit;
            std::memcpy(&limit, value.data(), sizeof(uint64_t));
            return limit > 0;
        }

        case ParameterType::ValidatorSetSize: {
            // Value should be a uint64_t: 100 ≤ N ≤ 500
            if (value.size() != sizeof(uint64_t)) {
                return false;
            }
            uint64_t n;
            std::memcpy(&n, value.data(), sizeof(uint64_t));
            return n >= 100 && n <= 500;
        }

        case ParameterType::MinimumSelfBond: {
            // Value should be a uint64_t: > 0
            if (value.size() != sizeof(uint64_t)) {
                return false;
            }
            uint64_t bond;
            std::memcpy(&bond, value.data(), sizeof(uint64_t));
            return bond > 0;
        }

        case ParameterType::BaseFeeAdjustment: {
            // Value should be a double: 0 < γ ≤ 1
            if (value.size() != sizeof(double)) {
                return false;
            }
            double gamma;
            std::memcpy(&gamma, value.data(), sizeof(double));
            return gamma > 0.0 && gamma <= 1.0;
        }

        default:
            return false;
    }
}

uint64_t GovernanceModule::calculate_timelock(ParameterType type) const {
    switch (type) {
        // Safety-critical parameters: 14 days
        case ParameterType::IssuanceCoefficient:
        case ParameterType::SlashingAlpha:
        case ParameterType::SlashingBeta:
            return TIMELOCK_SAFETY_DAYS * blocks_per_day_;

        // Administrative parameters: 7 days
        case ParameterType::ValidatorSetSize:
        case ParameterType::MinimumSelfBond:
            return TIMELOCK_ADMIN_DAYS * blocks_per_day_;

        // Performance parameters: 3 days
        case ParameterType::BlockSize:
        case ParameterType::GasLimit:
        case ParameterType::BaseFeeAdjustment:
            return TIMELOCK_PERFORMANCE_DAYS * blocks_per_day_;

        default:
            return TIMELOCK_ADMIN_DAYS * blocks_per_day_;
    }
}

} // namespace state
} // namespace sarafu
