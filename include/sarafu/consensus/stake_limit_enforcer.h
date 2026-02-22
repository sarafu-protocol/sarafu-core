#pragma once

#include "sarafu/consensus/validator.h"
#include "sarafu/state/account.h"
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace sarafu {
namespace consensus {

/**
 * Entity represents a legal entity that may control multiple validators.
 * Used for enforcing stake concentration limits across common ownership.
 */
struct Entity {
    std::string entity_id;                    // Unique identifier
    std::string legal_name;                   // Legal entity name
    std::string jurisdiction;                 // ISO 3166-1 alpha-2 country code
    std::vector<ValidatorID> controlled_validators;
    uint64_t total_stake;
    
    // Proof of distinct ownership (KYC documents, legal filings)
    std::vector<uint8_t> ownership_proof;
    crypto::Ed25519_Signature proof_signature;
    
    Entity();
    Entity(const std::string& id, const std::string& name, 
           const std::string& jurisdiction);
    
    std::vector<uint8_t> serialize() const;
    static Entity deserialize(const std::vector<uint8_t>& data);
};

/**
 * Stake concentration limits configuration.
 */
struct StakeLimits {
    double max_validator_stake_percent;      // e.g., 5.0 (5%)
    double max_entity_stake_percent;         // e.g., 10.0 (10%)
    double max_jurisdiction_stake_percent;   // e.g., 25.0 (25%)
    bool enforcement_enabled;
    
    StakeLimits()
        : max_validator_stake_percent(5.0),
          max_entity_stake_percent(10.0),
          max_jurisdiction_stake_percent(25.0),
          enforcement_enabled(true) {}
};

/**
 * Stake limit violation information.
 */
struct StakeLimitViolation {
    enum class Type {
        ValidatorLimit,
        EntityLimit,
        JurisdictionLimit
    };
    
    Type type;
    std::string identifier;  // Validator ID, entity ID, or jurisdiction code
    uint64_t current_stake;
    uint64_t limit_stake;
    double current_percent;
    double limit_percent;
    
    std::string to_string() const;
};



/**
 * Entity registry for tracking common ownership.
 */
class EntityRegistry {
public:
    /**
     * Register a new entity.
     * 
     * @param entity Entity to register
     * @return true if registered successfully
     */
    bool register_entity(const Entity& entity);
    
    /**
     * Associate a validator with an entity.
     * 
     * @param validator_id Validator ID
     * @param entity_id Entity ID
     * @return true if associated successfully
     */
    bool associate_validator(const ValidatorID& validator_id, 
                            const std::string& entity_id);
    
    /**
     * Get entity for a validator.
     * 
     * @param validator_id Validator ID
     * @return Entity if found, nullopt otherwise
     */
    std::optional<Entity> get_entity_for_validator(const ValidatorID& validator_id) const;
    
    /**
     * Get all validators for an entity.
     * 
     * @param entity_id Entity ID
     * @return Vector of validator IDs
     */
    std::vector<ValidatorID> get_validators_for_entity(const std::string& entity_id) const;
    
    /**
     * Calculate total stake for an entity.
     * 
     * @param entity_id Entity ID
     * @param validator_stakes Map of validator ID to stake
     * @return Total stake
     */
    uint64_t calculate_entity_stake(
        const std::string& entity_id,
        const std::map<ValidatorID, uint64_t>& validator_stakes
    ) const;
    
private:
    std::map<std::string, Entity> entities_;
    std::map<ValidatorID, std::string> validator_to_entity_;
};

/**
 * Jurisdiction tracker for geographic distribution.
 */
class JurisdictionTracker {
public:
    /**
     * Register validator jurisdiction.
     * 
     * @param validator_id Validator ID
     * @param jurisdiction ISO 3166-1 alpha-2 country code
     * @return true if registered successfully
     */
    bool register_jurisdiction(const ValidatorID& validator_id,
                              const std::string& jurisdiction);
    
    /**
     * Get jurisdiction for a validator.
     * 
     * @param validator_id Validator ID
     * @return Jurisdiction code if found, nullopt otherwise
     */
    std::optional<std::string> get_jurisdiction(const ValidatorID& validator_id) const;
    
    /**
     * Calculate stake distribution by jurisdiction.
     * 
     * @param validator_stakes Map of validator ID to stake
     * @return Map of jurisdiction to total stake
     */
    std::map<std::string, uint64_t> calculate_jurisdiction_distribution(
        const std::map<ValidatorID, uint64_t>& validator_stakes
    ) const;
    
private:
    std::map<ValidatorID, std::string> validator_jurisdictions_;
};

/**
 * Stake limit enforcer.
 * Enforces protocol-level stake concentration limits.
 */
class StakeLimitEnforcer {
public:
    explicit StakeLimitEnforcer(const StakeLimits& limits = StakeLimits());
    
    /**
     * Set entity registry.
     * 
     * @param registry Entity registry
     */
    void set_entity_registry(EntityRegistry* registry);
    
    /**
     * Set jurisdiction tracker.
     * 
     * @param tracker Jurisdiction tracker
     */
    void set_jurisdiction_tracker(JurisdictionTracker* tracker);
    
    /**
     * Check if a stake increase would violate limits.
     * 
     * @param validator_id Validator ID
     * @param additional_stake Additional stake to bond
     * @param current_validator_stake Current validator stake
     * @param total_stake Total bonded stake
     * @param all_validator_stakes All validator stakes
     * @return Violation if limits exceeded, nullopt otherwise
     */
    std::optional<StakeLimitViolation> check_stake_increase(
        const ValidatorID& validator_id,
        uint64_t additional_stake,
        uint64_t current_validator_stake,
        uint64_t total_stake,
        const std::map<ValidatorID, uint64_t>& all_validator_stakes
    );
    
    /**
     * Get current stake limits configuration.
     * 
     * @return Stake limits
     */
    StakeLimits get_limits() const;
    
    /**
     * Update stake limits (governance only).
     * Limits can only be made more restrictive.
     * 
     * @param new_limits New limits
     * @return true if update allowed
     */
    bool update_limits(const StakeLimits& new_limits);
    
private:
    StakeLimits limits_;
    EntityRegistry* entity_registry_;
    JurisdictionTracker* jurisdiction_tracker_;
};

} // namespace consensus
} // namespace sarafu
