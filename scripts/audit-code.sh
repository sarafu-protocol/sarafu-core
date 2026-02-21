#!/usr/bin/env bash
# Automated Code Audit Script
# Scans src/ directory for placeholder comments and generates a markdown report
# Requirements: 2.1, 2.2, 2.5

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
SRC_DIR="src"
REPORT_FILE="docs/CODE_AUDIT_REPORT.md"
PATTERNS=("TODO" "FIXME" "HACK" "XXX" "UNIMPLEMENTED")

# Check if ripgrep is available, otherwise fall back to grep
if command -v rg &> /dev/null; then
    SEARCH_CMD="rg"
    USE_RIPGREP=true
else
    SEARCH_CMD="grep"
    USE_RIPGREP=false
fi

echo "=================================================="
echo "  Sarafu Blockchain Code Audit"
echo "=================================================="
echo ""
echo "Scanning directory: $SRC_DIR"
echo "Search tool: $SEARCH_CMD"
echo ""

# Initialize counters
total_issues=0
issue_counts=""

# Create report header
mkdir -p "$(dirname "$REPORT_FILE")"
cat > "$REPORT_FILE" << EOF
# Code Audit Report

**Generated:** $(date -u +"%Y-%m-%d %H:%M:%S UTC" 2>/dev/null || date +"%Y-%m-%d %H:%M:%S")

## Overview

This report identifies placeholder comments and incomplete implementations in the production codebase.

## Summary

EOF

# Temporary file for detailed results
TEMP_DETAILS=$(mktemp)

# Function to search for a pattern
search_pattern() {
    local pattern=$1
    local results=""
    
    if [ "$USE_RIPGREP" = true ]; then
        # Using ripgrep with exclusions
        results=$(rg --no-heading --line-number --color never \
            --type cpp --type c \
            --glob '!*.test.cpp' \
            --glob '!*.test.h' \
            --glob '!*_test.cpp' \
            --glob '!*_test.h' \
            --glob '!*/tests/*' \
            --glob '!*/test/*' \
            --glob '!*.md' \
            --glob '!README*' \
            "\b${pattern}\b" "$SRC_DIR" 2>/dev/null || true)
    else
        # Using grep with exclusions
        results=$(grep -rn --include="*.cpp" --include="*.h" \
            --exclude="*.test.cpp" --exclude="*.test.h" \
            --exclude="*_test.cpp" --exclude="*_test.h" \
            "\b${pattern}\b" "$SRC_DIR" 2>/dev/null | \
            grep -v "/tests/" | grep -v "/test/" || true)
    fi
    
    echo "$results"
}

# Search for each pattern
echo "Searching for placeholder patterns..."
echo ""

for pattern in "${PATTERNS[@]}"; do
    echo -n "  Checking for $pattern... "
    
    results=$(search_pattern "$pattern")
    if [ -z "$results" ]; then
        count=0
    else
        count=$(echo "$results" | grep -c "." || echo "0")
    fi
    
    # Store count for summary
    issue_counts="${issue_counts}${pattern}:${count}|"
    total_issues=$((total_issues + count))
    
    if [ "$count" -eq 0 ]; then
        echo -e "${GREEN}✓ Clean${NC}"
    else
        echo -e "${RED}✗ Found $count occurrence(s)${NC}"
    fi
    
    # Append to detailed results
    if [ "$count" -gt 0 ]; then
        {
            echo ""
            echo "### $pattern Comments ($count found)"
            echo ""
            echo '```'
            echo "$results"
            echo '```'
            echo ""
        } >> "$TEMP_DETAILS"
    fi
done

# Build summary table
{
    echo "| Pattern | Count |"
    echo "|---------|-------|"
    for pattern in "${PATTERNS[@]}"; do
        count=$(echo "$issue_counts" | grep -o "${pattern}:[0-9]*" | cut -d: -f2)
        echo "| $pattern | $count |"
    done
    echo ""
    echo "**Total Issues:** $total_issues"
    echo ""
} >> "$REPORT_FILE"

# Append detailed results
if [ "$total_issues" -gt 0 ]; then
    echo "## Detailed Findings" >> "$REPORT_FILE"
    cat "$TEMP_DETAILS" >> "$REPORT_FILE"
fi

# Clean up temp file
rm -f "$TEMP_DETAILS"

# Add recommendations section
cat >> "$REPORT_FILE" << 'EOF'

## Recommendations

1. **TODO Comments**: Replace with actual implementations or create tracked issues
2. **FIXME Comments**: Address the identified problems before production deployment
3. **HACK Comments**: Refactor to use proper solutions instead of workarounds
4. **XXX Comments**: Review and resolve the marked concerns
5. **UNIMPLEMENTED**: Complete all unimplemented functionality

## Next Steps

- Review each identified issue
- Create GitHub issues for items requiring significant work
- Prioritize critical path items for immediate resolution
- Schedule remaining items for pre-mainnet cleanup

---

*This report was generated automatically by `scripts/audit-code.sh`*
EOF

# Print summary
echo ""
echo "=================================================="
echo "  Audit Summary"
echo "=================================================="
echo ""

for pattern in "${PATTERNS[@]}"; do
    count=$(echo "$issue_counts" | grep -o "${pattern}:[0-9]*" | cut -d: -f2)
    if [ -z "$count" ]; then
        count=0
    fi
    if [ "$count" -gt 0 ]; then
        echo -e "  ${RED}$pattern: $count${NC}"
    else
        echo -e "  ${GREEN}$pattern: $count${NC}"
    fi
done

echo ""
echo "  Total Issues: $total_issues"
echo ""
echo "Report saved to: $REPORT_FILE"
echo ""

# Exit with error code if issues found
if [ "$total_issues" -gt 0 ]; then
    echo -e "${YELLOW}⚠ Code audit found $total_issues issue(s)${NC}"
    echo "Review $REPORT_FILE for details"
    exit 1
else
    echo -e "${GREEN}✓ Code audit passed - no placeholder comments found${NC}"
    exit 0
fi
