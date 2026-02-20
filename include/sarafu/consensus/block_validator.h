#pragma once

#include "sarafu/consensus/block.h"
#include "sarafu/consensus/validator_registry.h"
#include "sarafu/state/state_machine.h"
#include "sarafu/crypto/blake3_hash.h"
#include <memory>
#include <string>

namespace sarafu {
namespace consensus {

/**
 * BlockValidator validates blocks according to consensus rules.
 * 
 * Validation checks:
 * - Block height increments by exactly 1
 * - Previous hash matches parent block
 * - Timestamp is monotonically increasing
 * - All transactions are valid
 * - Proposer is in the active validator set
 * - State root matches computed state
 * - Transactions root matches computed Merkle root
 * 
 * Requirements: 17.2, 17.3, 17.4, 17.5, 17.8
 */
class BlockValidator {
public:
    /**
     * Construct a BlockValidator.
     * 
     * @param validator_registry The validator registry for checking proposer membership
     * @param state_machine The state machine for transaction validation
     */
    BlockValidator(
        std::shared_ptr<ValidatorRegistry> validator_registry,
        std::shared_ptr<state::StateMachine> state_machine
    );

    /**
     * Validate a block against all consensus rules.
     * 
     * This performs all validation checks:
     * 1. Block height increment
     * 2. Block hash chain
     * 3. Timestamp monotonicity
     * 4. Transaction validity
     * 5. Proposer membership
     * 6. State root consistency (optional, expensive)
     * 7. Transactions root consistency
     * 
     * @param block The block to validate
     * @param parent_block The parent block
     * @param verify_state_root Whether to verify state root (expensive)
     * @return Empty string if valid, error message otherwise
     */
    std::string validate_block(
        const Block& block,
        const Block& parent_block,
        bool verify_state_root = false
    );

    /**
     * Verify that block height = parent height + 1.
     * 
     * Requirements: 17.2
     * 
     * @param block The block to check
     * @param parent_block The parent block
     * @return Empty string if valid, error message otherwise
     */
    std::string verify_block_height(
        const Block& block,
        const Block& parent_block
    ) const;

    /**
     * Verify that previous_hash matches parent block hash.
     * 
     * Requirements: 17.3
     * 
     * @param block The block to check
     * @param parent_block The parent block
     * @return Empty string if valid, error message otherwise
     */
    std::string verify_block_hash_chain(
        const Block& block,
        const Block& parent_block
    ) const;

    /**
     * Verify that timestamp > parent timestamp (monotonicity).
     * 
     * Requirements: 17.4
     * 
     * @param block The block to check
     * @param parent_block The parent block
     * @return Empty string if valid, error message otherwise
     */
    std::string verify_timestamp(
        const Block& block,
        const Block& parent_block
    ) const;

    /**
     * Verify that all transactions in the block are valid.
     * 
     * This checks each transaction for:
     * - Valid signature
     * - Correct nonce
     * - Sufficient balance
     * - Valid chain ID
     * 
     * Requirements: 17.5
     * 
     * @param block The block to check
     * @return Empty string if valid, error message otherwise
     */
    std::string verify_transactions(const Block& block);

    /**
     * Verify that the proposer is in the active validator set.
     * 
     * Requirements: 17.8
     * 
     * @param block The block to check
     * @return Empty string if valid, error message otherwise
     */
    std::string verify_proposer(const Block& block) const;

    /**
     * Verify that the state root matches the computed state after applying transactions.
     * 
     * This is an expensive operation that requires executing all transactions.
     * 
     * Requirements: 17.6
     * 
     * @param block The block to check
     * @return Empty string if valid, error message otherwise
     */
    std::string verify_state_root(const Block& block);

    /**
     * Verify that the transactions root matches the Merkle root of all transactions.
     * 
     * Requirements: 17.7
     * 
     * @param block The block to check
     * @return Empty string if valid, error message otherwise
     */
    std::string verify_transactions_root(const Block& block) const;

private:
    std::shared_ptr<ValidatorRegistry> validator_registry_;
    std::shared_ptr<state::StateMachine> state_machine_;
};

} // namespace consensus
} // namespace sarafu
