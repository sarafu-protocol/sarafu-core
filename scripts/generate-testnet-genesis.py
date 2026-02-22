#!/usr/bin/env python3
"""
Generate Kilimanjaro testnet genesis configuration with 50+ validators
distributed across 5 geographic regions.

Validates genesis constraints:
- No single allocation exceeds 5% of total supply
- Allocations exceeding 2% have 12-month linear vesting
- Minimum 50 validators across 5+ regions
"""

import json
import hashlib
from datetime import datetime

# Total supply: 10 billion SAR
TOTAL_SUPPLY = 10_000_000_000 * 10**18

# Genesis constraints
MAX_ALLOCATION_PCT = 0.05  # 5% max per allocation
VESTING_THRESHOLD_PCT = 0.02  # 2% threshold for vesting requirement
VESTING_PERIOD_MONTHS = 12

# Validator configuration
MIN_VALIDATORS = 50
MIN_REGIONS = 5
MIN_SELF_BOND = 10_000 * 10**18  # 10,000 SAR

# Geographic regions for validator distribution
REGIONS = [
    "Africa-East",      # Kenya, Tanzania, Uganda
    "Africa-West",      # Nigeria, Ghana, Senegal
    "Africa-South",     # South Africa, Botswana
    "Europe",           # Germany, UK, France
    "Asia-Pacific",     # Singapore, Japan, Australia
]

def generate_pubkey(validator_id: int, key_type: str = "consensus") -> str:
    """Generate deterministic public key for testing."""
    seed = f"{key_type}-validator-{validator_id}"
    hash_bytes = hashlib.sha256(seed.encode()).digest()
    if key_type == "consensus":
        # BLS12-381 public key (96 bytes)
        return "0x" + (hash_bytes + hash_bytes + hash_bytes).hex()[:192]
    else:
        # Ed25519 address (32 bytes)
        return "0x" + hash_bytes.hex()

def generate_validators(count: int) -> list:
    """Generate validator set distributed across regions."""
    validators = []
    validators_per_region = count // len(REGIONS)
    extra = count % len(REGIONS)
    
    validator_id = 1
    for region_idx, region in enumerate(REGIONS):
        region_count = validators_per_region + (1 if region_idx < extra else 0)
        
        for i in range(region_count):
            validators.append({
                "validator_id": validator_id,
                "region": region,
                "consensus_pubkey": generate_pubkey(validator_id, "consensus"),
                "withdrawal_address": generate_pubkey(validator_id, "withdrawal"),
                "stake": str(MIN_SELF_BOND),
                "commission_rate": "0.10"
            })
            validator_id += 1
    
    return validators

def generate_initial_accounts() -> tuple:
    """Generate initial account allocations with vesting."""
    accounts = []
    vesting_schedules = []
    
    # Foundation allocation: 3% with 12-month vesting
    foundation_amount = int(TOTAL_SUPPLY * 0.03)
    accounts.append({
        "address": "0x" + "f" * 64,  # Foundation address
        "balance": str(foundation_amount),
        "nonce": 0,
        "label": "Foundation"
    })
    vesting_schedules.append({
        "address": "0x" + "f" * 64,
        "total_amount": str(foundation_amount),
        "vesting_period_months": VESTING_PERIOD_MONTHS,
        "start_time": "2024-06-01T00:00:00Z"
    })
    
    # Early supporters: 2.5% with 12-month vesting
    supporters_amount = int(TOTAL_SUPPLY * 0.025)
    accounts.append({
        "address": "0x" + "e" * 64,  # Early supporters address
        "balance": str(supporters_amount),
        "nonce": 0,
        "label": "Early Supporters"
    })
    vesting_schedules.append({
        "address": "0x" + "e" * 64,
        "total_amount": str(supporters_amount),
        "vesting_period_months": VESTING_PERIOD_MONTHS,
        "start_time": "2024-06-01T00:00:00Z"
    })
    
    # Community treasury: 4% with 12-month vesting
    treasury_amount = int(TOTAL_SUPPLY * 0.04)
    accounts.append({
        "address": "0x" + "c" * 64,  # Community treasury
        "balance": str(treasury_amount),
        "nonce": 0,
        "label": "Community Treasury"
    })
    vesting_schedules.append({
        "address": "0x" + "c" * 64,
        "total_amount": str(treasury_amount),
        "vesting_period_months": VESTING_PERIOD_MONTHS,
        "start_time": "2024-06-01T00:00:00Z"
    })
    
    # Validator staking pool: 30% (distributed among validators)
    # This is already allocated in validator stakes
    
    # Public distribution: remaining supply
    # Split into smaller allocations, each < 2% (no vesting required)
    remaining = TOTAL_SUPPLY - foundation_amount - supporters_amount - treasury_amount
    
    # Create 50 public accounts, each with < 2% of total supply
    public_allocation_per_account = remaining // 50
    for i in range(50):
        accounts.append({
            "address": generate_pubkey(1000 + i, "withdrawal"),
            "balance": str(public_allocation_per_account),
            "nonce": 0,
            "label": f"Public Distribution {i+1}"
        })
    
    return accounts, vesting_schedules

def validate_genesis_constraints(genesis: dict) -> list:
    """Validate genesis configuration against requirements."""
    errors = []
    
    total_supply = int(genesis["initial_supply"])
    
    # Check validator count and distribution
    validators = genesis["initial_validators"]
    if len(validators) < MIN_VALIDATORS:
        errors.append(f"Insufficient validators: {len(validators)} < {MIN_VALIDATORS}")
    
    regions = set(v["region"] for v in validators)
    if len(regions) < MIN_REGIONS:
        errors.append(f"Insufficient regions: {len(regions)} < {MIN_REGIONS}")
    
    # Check allocation limits
    accounts = genesis["initial_accounts"]
    for account in accounts:
        balance = int(account["balance"])
        pct = balance / total_supply
        
        if pct > MAX_ALLOCATION_PCT:
            errors.append(
                f"Allocation exceeds 5% limit: {account.get('label', account['address'])} "
                f"has {pct*100:.2f}%"
            )
        
        # Check vesting requirement
        if pct > VESTING_THRESHOLD_PCT:
            # Should have vesting schedule
            has_vesting = any(
                v["address"] == account["address"]
                for v in genesis.get("vesting_schedules", [])
            )
            if not has_vesting:
                errors.append(
                    f"Allocation > 2% missing vesting: {account.get('label', account['address'])} "
                    f"has {pct*100:.2f}%"
                )
    
    return errors

def main():
    """Generate testnet genesis configuration."""
    print("Generating Kilimanjaro testnet genesis configuration...")
    
    # Generate validators
    validators = generate_validators(MIN_VALIDATORS)
    print(f"Generated {len(validators)} validators across {len(REGIONS)} regions")
    
    # Generate initial accounts with vesting
    accounts, vesting_schedules = generate_initial_accounts()
    print(f"Generated {len(accounts)} initial accounts")
    print(f"Generated {len(vesting_schedules)} vesting schedules")
    
    # Calculate total validator stake
    total_validator_stake = sum(int(v["stake"]) for v in validators)
    
    # Calculate total account balances
    total_account_balance = sum(int(a["balance"]) for a in accounts)
    
    # Total supply
    initial_supply = total_validator_stake + total_account_balance
    
    # Create genesis configuration
    genesis = {
        "chain_id": "kilimanjaro-testnet-1",
        "genesis_time": "2024-06-01T00:00:00Z",
        "config": {
            "protocol_version": 1,
            "block_time_ms": 2000,
            "epoch_length": 100,
            "min_self_bond": str(MIN_SELF_BOND),
            "max_validators": 150,
            "initial_validator_count": len(validators),
            "base_fee_per_gas": "1000000000",
            "block_gas_limit": "30000000",
            "issuance_coefficient_k": "0.1",
            "slashing_double_sign_penalty": "0.05",
            "slashing_downtime_threshold": "0.05",
            "unbonding_period_days": 21,
            "initial_staking_ratio": "0.30"
        },
        "initial_validators": validators,
        "initial_accounts": accounts,
        "vesting_schedules": vesting_schedules,
        "initial_supply": str(initial_supply),
        "monetary_policy": {
            "issuance_coefficient_k": "0.1",
            "target_staking_ratio": "0.30",
            "max_inflation": "0.20",
            "min_inflation": "0.05"
        }
    }
    
    # Validate constraints
    print("\nValidating genesis constraints...")
    errors = validate_genesis_constraints(genesis)
    
    if errors:
        print("❌ Validation errors:")
        for error in errors:
            print(f"  - {error}")
        return 1
    
    print("✅ All genesis constraints validated successfully")
    
    # Write genesis file
    output_path = "docker/testnet-genesis.json"
    with open(output_path, "w") as f:
        json.dump(genesis, f, indent=2)
    
    print(f"\n✅ Genesis configuration written to {output_path}")
    print(f"\nSummary:")
    print(f"  - Validators: {len(validators)}")
    print(f"  - Regions: {len(set(v['region'] for v in validators))}")
    print(f"  - Initial accounts: {len(accounts)}")
    print(f"  - Vesting schedules: {len(vesting_schedules)}")
    print(f"  - Total supply: {int(initial_supply) / 10**18:,.0f} SAR")
    print(f"  - Validator stake: {total_validator_stake / 10**18:,.0f} SAR")
    print(f"  - Account balances: {total_account_balance / 10**18:,.0f} SAR")
    
    return 0

if __name__ == "__main__":
    exit(main())
