#include "sarafu/consensus/consensus_engine.h"
#include "sarafu/network/network_layer.h"
#include "sarafu/state/mempool.h"
#include <algorithm>
#include <ctime>
#include <stdexcept>

namespace sarafu {
namespace consensus {

ConsensusEngine::ConsensusEngine(
    std::shared_ptr<ValidatorRegistry> validator_registry,
    std::shared_ptr<state::StateMachine> state_machine,
    const Config& config
)
    : config_(config),
      validator_registry_(validator_registry),
      state_machine_(state_machine),
      current_view_(0),
      finalized_height_(0),
      current_height_(0)
{
    if (!validator_registry_) {
        throw std::invalid_argument("validator_registry cannot be null");
    }
    if (!state_machine_) {
        throw std::invalid_argument("state_machine cannot be null");
    }

    // Initialize vote aggregator with current validator set
    vote_aggregator_ = std::make_unique<VoteAggregator>(
        validator_registry_->current_set()
    );

    // Initialize highest_qc with genesis values
    highest_qc_ = QuorumCertificate();
}

ConsensusEngine::~ConsensusEngine() = default;

bool ConsensusEngine::is_finalized(const crypto::Blake3Hash& block_hash) const {
    auto it = finalized_blocks_.find(block_hash);
    return it != finalized_blocks_.end() && it->second;
}

void ConsensusEngine::mark_finalized(
    const crypto::Blake3Hash& block_hash,
    const QuorumCertificate& qc
) {
    // Verify QC is valid
    if (!QCVerifier::verify_qc(qc, validator_registry_->current_set())) {
        return;
    }

    // Update highest QC if this QC is for a higher block
    if (qc.block_height > highest_qc_.block_height) {
        highest_qc_ = qc;
        finalized_height_ = qc.block_height;
    }

    // Mark block as finalized
    finalized_blocks_[block_hash] = true;
}

std::optional<Block> ConsensusEngine::propose_block(
    const ValidatorID& proposer_id,
    const crypto::BLS12_381_PrivateKey& proposer_key,
    state::Mempool& mempool,
    uint64_t base_fee
) {
    // Verify proposer is the current leader
    auto leader = get_current_leader();
    if (!leader || *leader != proposer_id) {
        return std::nullopt;
    }

    // Get parent block (most recent block)
    std::optional<Block> parent_block;
    if (current_height_ > 0) {
        parent_block = get_block_by_height(current_height_);
        if (!parent_block) {
            return std::nullopt;
        }
    }

    // Select transactions from mempool
    auto transactions = mempool.get_transactions_for_block(
        config_.max_block_gas,
        base_fee
    );

    // Limit number of transactions
    if (transactions.size() > config_.max_transactions_per_block) {
        transactions.resize(config_.max_transactions_per_block);
    }

    // Create block header
    BlockHeader header;
    header.height = current_height_ + 1;
    header.timestamp = std::time(nullptr);  // Unix timestamp
    header.previous_hash = parent_block ? parent_block->hash() : crypto::Blake3Hash();
    header.proposer = proposer_id;
    header.epoch = header.height / 10000;  // Epoch every 10,000 blocks

    // Compute transaction root (Merkle tree of transactions)
    std::vector<crypto::Blake3Hash> tx_hashes;
    for (const auto& tx : transactions) {
        tx_hashes.push_back(tx.hash());
    }
    if (!tx_hashes.empty()) {
        crypto::MerkleTree tx_tree;
        tx_tree.build_tree(tx_hashes);
        header.transactions_root = tx_tree.get_root();
    } else {
        header.transactions_root = crypto::Blake3Hash();
    }

    // Compute validator set root
    header.validator_set_root = validator_registry_->compute_validator_set_root(
        validator_registry_->current_set()
    );

    // Execute transactions to compute state root
    Block temp_block(header, transactions, highest_qc_);
    header.state_root = state_machine_->execute_block(temp_block, base_fee, proposer_id);

    // Create block with highest QC as justify
    Block block(header, transactions, highest_qc_);

    // Store block
    store_block(block);

    return block;
}

bool ConsensusEngine::on_receive_block(const Block& block) {
    // Verify block validity
    if (!verify_block(block)) {
        return false;
    }

    // Store block
    store_block(block);

    // Update current height if this block is higher
    if (block.header.height > current_height_) {
        current_height_ = block.header.height;
    }

    // Block is valid - validators would create votes here
    // In a real implementation, this would trigger vote creation and broadcasting
    return true;
}

std::optional<QuorumCertificate> ConsensusEngine::on_receive_vote(const Vote& vote) {
    // Add vote to aggregator
    if (!vote_aggregator_->add_vote(vote)) {
        return std::nullopt;
    }

    // Try to aggregate votes into QC
    auto qc = vote_aggregator_->aggregate_votes(
        vote.block_height,
        vote.block_hash,
        vote.view_number
    );

    // If QC was created, mark block as finalized
    if (qc) {
        mark_finalized(vote.block_hash, *qc);
    }

    return qc;
}

bool ConsensusEngine::trigger_view_change() {
    // Increment view number
    current_view_++;

    // Select new leader
    auto new_leader = get_current_leader();
    if (!new_leader) {
        return false;
    }

    // In a real implementation, this would:
    // 1. Broadcast view-change message
    // 2. Wait for ≥2/3 stake to agree on view change
    // 3. New leader proposes block

    return true;
}

std::optional<ValidatorID> ConsensusEngine::get_current_leader() const {
    return select_leader(current_view_);
}

void ConsensusEngine::set_genesis_block(const Block& genesis_block) {
    if (genesis_block.header.height != 0) {
        throw std::invalid_argument("Genesis block must have height 0");
    }

    store_block(genesis_block);
    current_height_ = 0;
    finalized_height_ = 0;

    // Genesis block is automatically finalized
    finalized_blocks_[genesis_block.hash()] = true;

    // Create genesis QC
    highest_qc_ = QuorumCertificate(
        0,
        genesis_block.hash(),
        0,
        crypto::BLS12_381_Signature(),
        {},
        0
    );
}

std::optional<Block> ConsensusEngine::get_block(const crypto::Blake3Hash& block_hash) const {
    auto it = block_store_.find(block_hash);
    if (it != block_store_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<Block> ConsensusEngine::get_block_by_height(uint64_t height) const {
    auto it = height_index_.find(height);
    if (it != height_index_.end()) {
        return get_block(it->second);
    }
    return std::nullopt;
}

void ConsensusEngine::store_block(const Block& block) {
    auto block_hash = block.hash();
    block_store_[block_hash] = block;
    height_index_[block.header.height] = block_hash;
}

bool ConsensusEngine::verify_block(const Block& block) const {
    // Verify block extends highest QC
    if (!extends_highest_qc(block)) {
        return false;
    }

    // Verify proposer is current leader
    if (!is_current_leader(block.header.proposer)) {
        return false;
    }

    // Verify block height is exactly one greater than parent
    if (block.header.height > 0) {
        auto parent = get_block(block.header.previous_hash);
        if (!parent) {
            return false;
        }
        if (block.header.height != parent->header.height + 1) {
            return false;
        }

        // Verify timestamp is greater than parent
        if (block.header.timestamp <= parent->header.timestamp) {
            return false;
        }
    }

    // Verify proposer is in active validator set
    const auto& validator_set = validator_registry_->current_set();
    if (!validator_set.is_active(block.header.proposer)) {
        return false;
    }

    // Verify justify QC is valid
    if (block.header.height > 0) {
        if (!QCVerifier::verify_qc(block.justify, validator_registry_->current_set())) {
            return false;
        }
    }

    // All checks passed
    return true;
}

bool ConsensusEngine::extends_highest_qc(const Block& block) const {
    // Genesis block doesn't need to extend anything
    if (block.header.height == 0) {
        return true;
    }

    // Block must have a justify QC
    if (block.justify.block_height == 0 && highest_qc_.block_height > 0) {
        return false;
    }

    // Block's justify must be at least as high as our highest QC
    if (block.justify.block_height < highest_qc_.block_height) {
        return false;
    }

    // Block must extend from the justified block
    if (block.header.height <= block.justify.block_height) {
        return false;
    }

    return true;
}

bool ConsensusEngine::is_current_leader(const ValidatorID& proposer_id) const {
    auto leader = get_current_leader();
    return leader && *leader == proposer_id;
}

Vote ConsensusEngine::create_vote(
    const Block& block,
    const ValidatorID& voter_id,
    const crypto::BLS12_381_PrivateKey& voter_key
) const {
    // Create vote message
    auto block_hash = block.hash();

    // Sign the block hash
    auto signature = crypto::BLS12_381::sign(
        block_hash.bytes(),
        block_hash.size(),
        voter_key
    );

    return Vote(voter_id, block.header.height, block_hash, current_view_, signature);
}

std::optional<ValidatorID> ConsensusEngine::select_leader(uint64_t view_number) const {
    const auto& validator_set = validator_registry_->current_set();
    auto active_validators = validator_set.get_active_validators();

    if (active_validators.empty()) {
        return std::nullopt;
    }

    // Round-robin by stake: leader_index = view_number % active_validator_count
    size_t leader_index = view_number % active_validators.size();
    return active_validators[leader_index].id;
}

// ============================================================================
// Network Integration
// ============================================================================

void ConsensusEngine::set_network_layer(std::shared_ptr<network::NetworkLayer> network) {
    network_layer_ = network;
}

void ConsensusEngine::broadcast_block(const Block& block) {
    if (!network_layer_) {
        return;  // No network layer configured
    }
    
    // Serialize block
    std::vector<uint8_t> payload = block.serialize();
    
    // Create network message
    network::NetworkMessage message(network::MessageType::Block, payload);
    
    // Broadcast to all peers
    network_layer_->broadcast(message);
}

void ConsensusEngine::broadcast_vote(const Vote& vote) {
    if (!network_layer_) {
        return;  // No network layer configured
    }
    
    // Serialize vote - Vote struct doesn't have serialize method yet, use placeholder
    std::vector<uint8_t> payload;
    // TODO: Implement Vote serialization
    
    // Create network message
    network::NetworkMessage message(network::MessageType::Vote, payload);
    
    // Broadcast to all peers
    network_layer_->broadcast(message);
}

} // namespace consensus
} // namespace sarafu
