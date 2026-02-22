#!/bin/bash
# Setup GitHub Issue Labels for Security Audit Findings
# This script creates all necessary labels for tracking audit findings

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "=========================================="
echo "GitHub Audit Labels Setup"
echo "=========================================="
echo ""

# Check if gh CLI is installed
if ! command -v gh &> /dev/null; then
    echo -e "${RED}Error: GitHub CLI (gh) is not installed${NC}"
    echo "Please install it from: https://cli.github.com/"
    exit 1
fi

# Check if user is authenticated
if ! gh auth status &> /dev/null; then
    echo -e "${RED}Error: Not authenticated with GitHub CLI${NC}"
    echo "Please run: gh auth login"
    exit 1
fi

echo -e "${GREEN}Creating audit severity labels...${NC}"

# Severity Labels
gh label create "audit-critical" \
    --description "Critical severity audit finding" \
    --color "B60205" \
    --force 2>/dev/null && echo "  ✓ audit-critical" || echo "  ⚠ audit-critical already exists"

gh label create "audit-high" \
    --description "High severity audit finding" \
    --color "D93F0B" \
    --force 2>/dev/null && echo "  ✓ audit-high" || echo "  ⚠ audit-high already exists"

gh label create "audit-medium" \
    --description "Medium severity audit finding" \
    --color "FBCA04" \
    --force 2>/dev/null && echo "  ✓ audit-medium" || echo "  ⚠ audit-medium already exists"

gh label create "audit-low" \
    --description "Low severity audit finding" \
    --color "0E8A16" \
    --force 2>/dev/null && echo "  ✓ audit-low" || echo "  ⚠ audit-low already exists"

gh label create "audit-info" \
    --description "Informational audit finding" \
    --color "D4C5F9" \
    --force 2>/dev/null && echo "  ✓ audit-info" || echo "  ⚠ audit-info already exists"

echo ""
echo -e "${GREEN}Creating audit status labels...${NC}"

# Status Labels
gh label create "audit-open" \
    --description "Audit finding is open" \
    --color "E99695" \
    --force 2>/dev/null && echo "  ✓ audit-open" || echo "  ⚠ audit-open already exists"

gh label create "audit-in-progress" \
    --description "Fix is in progress" \
    --color "FEF2C0" \
    --force 2>/dev/null && echo "  ✓ audit-in-progress" || echo "  ⚠ audit-in-progress already exists"

gh label create "audit-fixed" \
    --description "Fix is implemented" \
    --color "C2E0C6" \
    --force 2>/dev/null && echo "  ✓ audit-fixed" || echo "  ⚠ audit-fixed already exists"

gh label create "audit-verified" \
    --description "Fix is verified by auditor" \
    --color "0E8A16" \
    --force 2>/dev/null && echo "  ✓ audit-verified" || echo "  ⚠ audit-verified already exists"

gh label create "audit-closed" \
    --description "Finding is closed" \
    --color "5319E7" \
    --force 2>/dev/null && echo "  ✓ audit-closed" || echo "  ⚠ audit-closed already exists"

echo ""
echo -e "${GREEN}Creating audit firm labels...${NC}"

# Audit Firm Labels
gh label create "audit-firm-a" \
    --description "Finding from Audit Firm A" \
    --color "BFD4F2" \
    --force 2>/dev/null && echo "  ✓ audit-firm-a" || echo "  ⚠ audit-firm-a already exists"

gh label create "audit-firm-b" \
    --description "Finding from Audit Firm B" \
    --color "D4C5F9" \
    --force 2>/dev/null && echo "  ✓ audit-firm-b" || echo "  ⚠ audit-firm-b already exists"

echo ""
echo -e "${GREEN}Creating audit component labels...${NC}"

# Component Labels
gh label create "audit-consensus" \
    --description "Consensus layer finding" \
    --color "C5DEF5" \
    --force 2>/dev/null && echo "  ✓ audit-consensus" || echo "  ⚠ audit-consensus already exists"

gh label create "audit-crypto" \
    --description "Cryptographic operations finding" \
    --color "F9D0C4" \
    --force 2>/dev/null && echo "  ✓ audit-crypto" || echo "  ⚠ audit-crypto already exists"

gh label create "audit-slashing" \
    --description "Slashing logic finding" \
    --color "FEF2C0" \
    --force 2>/dev/null && echo "  ✓ audit-slashing" || echo "  ⚠ audit-slashing already exists"

gh label create "audit-economic" \
    --description "Economic mechanisms finding" \
    --color "C2E0C6" \
    --force 2>/dev/null && echo "  ✓ audit-economic" || echo "  ⚠ audit-economic already exists"

gh label create "audit-network" \
    --description "Network layer finding" \
    --color "BFD4F2" \
    --force 2>/dev/null && echo "  ✓ audit-network" || echo "  ⚠ audit-network already exists"

gh label create "audit-state" \
    --description "State management finding" \
    --color "D4C5F9" \
    --force 2>/dev/null && echo "  ✓ audit-state" || echo "  ⚠ audit-state already exists"

echo ""
echo -e "${GREEN}Creating audit milestone...${NC}"

# Create milestone for audit remediation
gh api repos/:owner/:repo/milestones \
    -X POST \
    -f title="Security Audit Remediation" \
    -f description="Track remediation of security audit findings" \
    -f state="open" \
    2>/dev/null && echo "  ✓ Milestone created" || echo "  ⚠ Milestone already exists or creation failed"

echo ""
echo "=========================================="
echo -e "${GREEN}Audit labels setup complete!${NC}"
echo "=========================================="
echo ""
echo "Created Labels:"
echo ""
echo "Severity Labels:"
echo "  • audit-critical (red)"
echo "  • audit-high (orange)"
echo "  • audit-medium (yellow)"
echo "  • audit-low (green)"
echo "  • audit-info (purple)"
echo ""
echo "Status Labels:"
echo "  • audit-open"
echo "  • audit-in-progress"
echo "  • audit-fixed"
echo "  • audit-verified"
echo "  • audit-closed"
echo ""
echo "Firm Labels:"
echo "  • audit-firm-a"
echo "  • audit-firm-b"
echo ""
echo "Component Labels:"
echo "  • audit-consensus"
echo "  • audit-crypto"
echo "  • audit-slashing"
echo "  • audit-economic"
echo "  • audit-network"
echo "  • audit-state"
echo ""
echo "Milestone:"
echo "  • Security Audit Remediation"
echo ""
echo "Usage Example:"
echo "  Create an issue for a critical consensus finding from Firm A:"
echo "  gh issue create \\"
echo "    --title \"[AUDIT-A-001] Consensus safety violation\" \\"
echo "    --label \"audit-critical,audit-open,audit-firm-a,audit-consensus\" \\"
echo "    --milestone \"Security Audit Remediation\" \\"
echo "    --body \"[Finding details]\"
echo ""
