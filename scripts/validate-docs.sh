#!/bin/bash
# Documentation validation script for Sarafu blockchain
# Validates documentation links, structure, and completeness

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
DOCS_DIR="${DOCS_DIR:-docs}"
CHECK_LINKS="${CHECK_LINKS:-true}"
CHECK_STRUCTURE="${CHECK_STRUCTURE:-true}"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Documentation Validation${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Track errors
ERRORS=0
WARNINGS=0

# Function to check if a file exists
check_file_exists() {
    local file=$1
    local description=$2
    
    if [ -f "$file" ]; then
        echo -e "${GREEN}✓${NC} $description: $file"
        return 0
    else
        echo -e "${RED}✗${NC} $description missing: $file"
        ((ERRORS++))
        return 1
    fi
}

# Function to check for broken internal links
check_internal_links() {
    local file=$1
    local broken=0
    
    # Extract markdown links [text](path)
    grep -oP '\[.*?\]\(\K[^)]+' "$file" 2>/dev/null | while read -r link; do
        # Skip external links (http/https)
        if [[ "$link" =~ ^https?:// ]]; then
            continue
        fi
        
        # Skip anchors
        if [[ "$link" =~ ^# ]]; then
            continue
        fi
        
        # Resolve relative path
        local dir=$(dirname "$file")
        local target="$dir/$link"
        
        # Check if target exists
        if [ ! -f "$target" ] && [ ! -d "$target" ]; then
            echo -e "${YELLOW}⚠️  Broken link in $file: $link${NC}"
            ((WARNINGS++))
            broken=1
        fi
    done
    
    return $broken
}

# Check documentation structure
if [ "$CHECK_STRUCTURE" = "true" ]; then
    echo -e "${BLUE}Checking documentation structure...${NC}"
    echo ""
    
    # Core documentation files
    check_file_exists "README.md" "Main README"
    check_file_exists "CLI_USAGE.md" "CLI usage guide"
    check_file_exists "Sarafu-Whitepater.md" "Whitepaper"
    
    # Operations documentation
    check_file_exists "$DOCS_DIR/TESTNET_VALIDATOR_GUIDE.md" "Testnet validator guide"
    check_file_exists "$DOCS_DIR/operations/INCIDENT_RESPONSE.md" "Incident response runbook"
    check_file_exists "$DOCS_DIR/operations/BACKUP_RECOVERY.md" "Backup and recovery guide"
    
    # Architecture documentation
    check_file_exists "$DOCS_DIR/CONSENSUS_AND_NETWORKING.md" "Consensus and networking design"
    check_file_exists "$DOCS_DIR/VERSION_COMPATIBILITY.md" "Version compatibility matrix"
    
    # Audit documentation
    check_file_exists "$DOCS_DIR/AUDIT_PACKAGE.md" "Audit package guide"
    check_file_exists "$DOCS_DIR/AUDIT_WORKFLOW.md" "Audit workflow"
    
    # Performance and testing
    check_file_exists "$DOCS_DIR/PERFORMANCE_REPORT.md" "Performance report"
    check_file_exists "$DOCS_DIR/NETWORK_VALIDATION_REPORT.md" "Network validation report"
    
    echo ""
fi

# Check for broken links
if [ "$CHECK_LINKS" = "true" ]; then
    echo -e "${BLUE}Checking for broken internal links...${NC}"
    echo ""
    
    # Find all markdown files
    find . -name "*.md" -type f | while read -r file; do
        # Skip node_modules and build directories
        if [[ "$file" =~ node_modules|build|\.git ]]; then
            continue
        fi
        
        check_internal_links "$file"
    done
    
    echo ""
fi

# Check for required sections in key documents
echo -e "${BLUE}Checking required documentation sections...${NC}"
echo ""

# Check README has essential sections
if [ -f "README.md" ]; then
    if grep -q "## Features" README.md && \
       grep -q "## Building" README.md && \
       grep -q "## Usage" README.md; then
        echo -e "${GREEN}✓${NC} README.md has essential sections"
    else
        echo -e "${YELLOW}⚠️  README.md missing some essential sections${NC}"
        ((WARNINGS++))
    fi
fi

# Check validator guide has setup instructions
if [ -f "$DOCS_DIR/TESTNET_VALIDATOR_GUIDE.md" ]; then
    if grep -q "Hardware Requirements" "$DOCS_DIR/TESTNET_VALIDATOR_GUIDE.md" && \
       grep -q "Installation" "$DOCS_DIR/TESTNET_VALIDATOR_GUIDE.md" && \
       grep -q "Configuration" "$DOCS_DIR/TESTNET_VALIDATOR_GUIDE.md"; then
        echo -e "${GREEN}✓${NC} Validator guide has essential sections"
    else
        echo -e "${YELLOW}⚠️  Validator guide missing some essential sections${NC}"
        ((WARNINGS++))
    fi
fi

# Check incident response has runbooks
if [ -f "$DOCS_DIR/operations/INCIDENT_RESPONSE.md" ]; then
    if grep -q "Consensus Failure" "$DOCS_DIR/operations/INCIDENT_RESPONSE.md" && \
       grep -q "Network Partition" "$DOCS_DIR/operations/INCIDENT_RESPONSE.md"; then
        echo -e "${GREEN}✓${NC} Incident response has runbooks"
    else
        echo -e "${YELLOW}⚠️  Incident response missing some runbooks${NC}"
        ((WARNINGS++))
    fi
fi

echo ""

# Check for TODO/FIXME in documentation
echo -e "${BLUE}Checking for incomplete documentation...${NC}"
echo ""

TODO_COUNT=$(grep -r "TODO\|FIXME\|TBD" docs/ --include="*.md" 2>/dev/null | wc -l || echo 0)
if [ "$TODO_COUNT" -gt 0 ]; then
    echo -e "${YELLOW}⚠️  Found $TODO_COUNT TODO/FIXME/TBD markers in documentation${NC}"
    grep -r "TODO\|FIXME\|TBD" docs/ --include="*.md" -n 2>/dev/null | head -10
    if [ "$TODO_COUNT" -gt 10 ]; then
        echo -e "${YELLOW}   ... and $((TODO_COUNT - 10)) more${NC}"
    fi
    ((WARNINGS++))
else
    echo -e "${GREEN}✓${NC} No TODO/FIXME/TBD markers found"
fi

echo ""

# Summary
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Validation Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✓ All documentation checks passed${NC}"
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}⚠️  Documentation validation completed with $WARNINGS warnings${NC}"
    exit 0
else
    echo -e "${RED}✗ Documentation validation failed${NC}"
    echo -e "${RED}  Errors: $ERRORS${NC}"
    echo -e "${YELLOW}  Warnings: $WARNINGS${NC}"
    exit 1
fi
