#!/usr/bin/env python3
"""
Sarafu Blockchain - Testnet Genesis Verification Script

This script verifies that the testnet genesis configuration meets all requirements:
- No single allocation exceeds 5% of total supply
- 12-month linear vesting for allocations exceeding 2% of total supply
- Validator distribution across geographic regions
- Minimum validator count (50)
- Initial staking ratio (30%)
- Initial issuance coefficient k (0.1)
- Initial validator set size N (150)
- Minimum self-bond requirement (10,000 SAR)
"""

import json
import sys
from decimal import Decimal
from collections import defaultdict

# Colors for output
GREEN = '\033[0;32m'
RED = '\033[0;31m'
YELLOW = '\033[1;33m'
NC = '\033[0m'  # No Color

def log_info(msg):
    print(f"{GREEN}[INFO]{NC} {msg}")

def log_error(msg):
    print(f"{RED}[ERROR]{NC} {msg}")

def log_warn(msg):
    print(f"{YELLOW}[WARN]{NC} {msg}")

def verify_genesis(genesis_file):
    """Verify testnet genesis configuration"""
    
    log_info(f"Loading genesis file: {genesis_file}")
    
    try:
        with open(genesis_file, 'r') as f:
            genesis = json.load(f)
    except Exception as e:
        log_error(f"Failed to load genesis file: {e}")
        return False
    
    log_info("✅ Genesis file loaded successfully")
    
    # Extract data
    initial_supply = Decimal(genesis['initial_supply'])
    validators = genesis['initial_validators']
    accounts = genesis['initial_accounts']
    vesting_schedules = genesis['vesting_schedules']
    config = genesis['config']
    
    log_info(f"Initial supply: {initial_supply}")
    log_info(f"Validators: {len(validators)}")
    log_info(f"Accounts: {len(accounts)}")
    log_info(f"Vesting schedules: {len(vesting_schedules)}")
    
    all_checks_passed = True
    
    # Requirement 11.1: No single allocation exceeds 5% of total supply
    log_info("\n=== Checking Requirement 11.1: Allocation limits ===")
    max_allocation_pct = Decimal('0.05')  # 5%
    max_allowed = initial_supply * max_allocation_pct
    
    for account in accounts:
        balance = Decimal(account['balance'])
        pct = (balance / initial_supply) * 100
        
        if balance > max_allowed:
            log_error(f"❌ Account {account['label']} exceeds 5% limit: {pct:.2f}%")
            all_checks_passed = False
        else:
            log_info(f"✅ {account['label']}: {pct:.2f}% (within limit)")
    
    # Requirement 11.2: 12-month vesting for allocations exceeding 2%
    log_info("\n=== Checking Requirement 11.2: Vesting requirements ===")
    vesting_threshold_pct = Decimal('0.02')  # 2%
    vesting_threshold = initial_supply * vesting_threshold_pct
    
    # Create vesting lookup
    vesting_addresses = {v['address']: v for v in vesting_schedules}
    
    for account in accounts:
        balance = Decimal(account['balance'])
        pct = (balance / initial_supply) * 100
        
        if balance > vesting_threshold:
            if account['address'] in vesting_addresses:
                vesting = vesting_addresses[account['address']]
                if vesting['vesting_period_months'] == 12:
                    log_info(f"✅ {account['label']}: {pct:.2f}% has 12-month vesting")
                else:
                    log_error(f"❌ {account['label']}: {pct:.2f}% has {vesting['vesting_period_months']}-month vesting (should be 12)")
                    all_checks_passed = False
            else:
                log_error(f"❌ {account['label']}: {pct:.2f}% exceeds 2% but has no vesting schedule")
                all_checks_passed = False
        else:
            log_info(f"✅ {account['label']}: {pct:.2f}% (below 2% threshold, vesting not required)")
    
    # Requirement 11.4: Minimum 100 validators (testnet uses 50 for validation)
    log_info("\n=== Checking Requirement 11.4: Validator count ===")
    min_validators = 50  # Testnet requirement
    
    if len(validators) >= min_validators:
        log_info(f"✅ Validator count: {len(validators)} (minimum: {min_validators})")
    else:
        log_error(f"❌ Validator count: {len(validators)} (minimum: {min_validators})")
        all_checks_passed = False
    
    # Check geographic distribution (minimum 5 regions)
    log_info("\n=== Checking Requirement 8.4: Geographic distribution ===")
    regions = defaultdict(int)
    for validator in validators:
        regions[validator['region']] += 1
    
    log_info(f"Regions: {len(regions)}")
    for region, count in sorted(regions.items()):
        log_info(f"  {region}: {count} validators")
    
    if len(regions) >= 5:
        log_info(f"✅ Geographic distribution: {len(regions)} regions (minimum: 5)")
    else:
        log_error(f"❌ Geographic distribution: {len(regions)} regions (minimum: 5)")
        all_checks_passed = False
    
    # Requirement 11.5: Initial staking ratio 30%
    log_info("\n=== Checking Requirement 11.5: Initial staking ratio ===")
    expected_staking_ratio = Decimal('0.30')
    actual_staking_ratio = Decimal(config['initial_staking_ratio'])
    
    if actual_staking_ratio == expected_staking_ratio:
        log_info(f"✅ Initial staking ratio: {actual_staking_ratio} (expected: {expected_staking_ratio})")
    else:
        log_error(f"❌ Initial staking ratio: {actual_staking_ratio} (expected: {expected_staking_ratio})")
        all_checks_passed = False
    
    # Requirement 11.6: Initial issuance coefficient k = 0.1
    log_info("\n=== Checking Requirement 11.6: Issuance coefficient ===")
    expected_k = Decimal('0.1')
    actual_k = Decimal(config['issuance_coefficient_k'])
    
    if actual_k == expected_k:
        log_info(f"✅ Issuance coefficient k: {actual_k} (expected: {expected_k})")
    else:
        log_error(f"❌ Issuance coefficient k: {actual_k} (expected: {expected_k})")
        all_checks_passed = False
    
    # Requirement 11.7: Initial validator set size N = 150
    log_info("\n=== Checking Requirement 11.7: Max validator set size ===")
    expected_max_validators = 150
    actual_max_validators = config['max_validators']
    
    if actual_max_validators == expected_max_validators:
        log_info(f"✅ Max validators: {actual_max_validators} (expected: {expected_max_validators})")
    else:
        log_error(f"❌ Max validators: {actual_max_validators} (expected: {expected_max_validators})")
        all_checks_passed = False
    
    # Requirement 11.8: Minimum self-bond 10,000 SAR
    log_info("\n=== Checking Requirement 11.8: Minimum self-bond ===")
    expected_min_bond = Decimal('10000000000000000000000')  # 10,000 SAR with 18 decimals
    actual_min_bond = Decimal(config['min_self_bond'])
    
    if actual_min_bond == expected_min_bond:
        log_info(f"✅ Minimum self-bond: {actual_min_bond / Decimal('1e18')} SAR (expected: 10,000 SAR)")
    else:
        log_error(f"❌ Minimum self-bond: {actual_min_bond / Decimal('1e18')} SAR (expected: 10,000 SAR)")
        all_checks_passed = False
    
    # Verify all validators meet minimum self-bond
    log_info("\n=== Verifying validator stakes ===")
    for validator in validators:
        stake = Decimal(validator['stake'])
        if stake >= expected_min_bond:
            log_info(f"✅ Validator {validator['validator_id']}: {stake / Decimal('1e18')} SAR")
        else:
            log_error(f"❌ Validator {validator['validator_id']}: {stake / Decimal('1e18')} SAR (below minimum)")
            all_checks_passed = False
    
    # Additional checks
    log_info("\n=== Additional configuration checks ===")
    
    # Block time
    if config['block_time_ms'] == 2000:
        log_info(f"✅ Block time: {config['block_time_ms']}ms (2 seconds)")
    else:
        log_warn(f"⚠️  Block time: {config['block_time_ms']}ms (expected: 2000ms)")
    
    # Epoch length
    if config['epoch_length'] == 100:
        log_info(f"✅ Epoch length: {config['epoch_length']} blocks")
    else:
        log_warn(f"⚠️  Epoch length: {config['epoch_length']} blocks (expected: 100)")
    
    # Summary
    log_info("\n=== Verification Summary ===")
    if all_checks_passed:
        log_info("✅ All requirements passed!")
        return True
    else:
        log_error("❌ Some requirements failed. Please review the errors above.")
        return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 verify-testnet-genesis.py <genesis-file>")
        print("Example: python3 verify-testnet-genesis.py docker/testnet-genesis.json")
        sys.exit(1)
    
    genesis_file = sys.argv[1]
    
    print("Sarafu Testnet Genesis Verification")
    print("=" * 50)
    
    success = verify_genesis(genesis_file)
    
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
