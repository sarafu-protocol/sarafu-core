#!/bin/bash
# Audit Package Preparation Script
# Prepares a comprehensive package for security audit firms

set -e

# Configuration
PACKAGE_NAME="sarafu-audit-package-$(date +%Y%m%d)"
PACKAGE_DIR="audit-packages/${PACKAGE_NAME}"
TIMESTAMP=$(date -u +"%Y-%m-%d %H:%M:%S UTC")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=========================================="
echo "Sarafu Blockchain Audit Package Generator"
echo "=========================================="
echo ""

# Create package directory
echo -e "${GREEN}Creating package directory...${NC}"
mkdir -p "${PACKAGE_DIR}"/{code,docs,tests,reports}

# 1. Create code snapshot
echo -e "${GREEN}Creating code snapshot...${NC}"
git archive --format=tar.gz --prefix=sarafu-blockchain/ HEAD -o "${PACKAGE_DIR}/code/sarafu-blockchain-source.tar.gz"
echo "  ✓ Source code archived"

# Get current git commit info
GIT_COMMIT=$(git rev-parse HEAD)
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
GIT_TAG=$(git describe --tags --always)

# 2. Copy documentation
echo -e "${GREEN}Copying documentation...${NC}"
cp -r docs/* "${PACKAGE_DIR}/docs/" 2>/dev/null || echo "  ⚠ No docs directory found"
cp README.md "${PACKAGE_DIR}/docs/" 2>/dev/null || echo "  ⚠ No README.md found"
cp Sarafu-Whitepater.md "${PACKAGE_DIR}/docs/" 2>/dev/null || echo "  ⚠ No whitepaper found"
cp CLI_USAGE.md "${PACKAGE_DIR}/docs/" 2>/dev/null || echo "  ⚠ No CLI_USAGE.md found"
echo "  ✓ Documentation copied"

# 3. Copy test suite
echo -e "${GREEN}Copying test suite...${NC}"
cp -r tests/* "${PACKAGE_DIR}/tests/" 2>/dev/null || echo "  ⚠ No tests directory found"
echo "  ✓ Test suite copied"

# 4. Run tests and collect results
echo -e "${GREEN}Running test suite and collecting results...${NC}"
if [ -f "scripts/run-all-tests.sh" ]; then
    echo "  Running all tests (this may take a while)..."
    bash scripts/run-all-tests.sh > "${PACKAGE_DIR}/reports/test-results.txt" 2>&1 || echo "  ⚠ Some tests failed (see test-results.txt)"
    echo "  ✓ Test results collected"
else
    echo "  ⚠ Test runner script not found, skipping test execution"
fi

# 5. Generate code coverage report
echo -e "${GREEN}Generating code coverage report...${NC}"
if [ -f "scripts/generate-coverage.sh" ]; then
    bash scripts/generate-coverage.sh > "${PACKAGE_DIR}/reports/coverage-summary.txt" 2>&1 || echo "  ⚠ Coverage generation failed"
    if [ -d "build/coverage" ]; then
        cp -r build/coverage "${PACKAGE_DIR}/reports/coverage-html"
        echo "  ✓ Coverage report generated"
    fi
else
    echo "  ⚠ Coverage script not found, skipping coverage report"
fi

# 6. Run code audit
echo -e "${GREEN}Running code audit...${NC}"
if [ -f "scripts/audit-code.sh" ]; then
    bash scripts/audit-code.sh > "${PACKAGE_DIR}/reports/code-audit.txt" 2>&1 || echo "  ⚠ Code audit found issues (see code-audit.txt)"
    echo "  ✓ Code audit completed"
else
    echo "  ⚠ Code audit script not found, skipping audit"
fi

# 7. Run performance benchmarks
echo -e "${GREEN}Running performance benchmarks...${NC}"
if [ -f "scripts/run-benchmarks.sh" ]; then
    bash scripts/run-benchmarks.sh > "${PACKAGE_DIR}/reports/benchmark-results.txt" 2>&1 || echo "  ⚠ Benchmarks failed"
    echo "  ✓ Benchmark results collected"
else
    echo "  ⚠ Benchmark script not found, skipping benchmarks"
fi

# 8. Collect dependency information
echo -e "${GREEN}Collecting dependency information...${NC}"
cat > "${PACKAGE_DIR}/reports/dependencies.txt" << EOF
Sarafu Blockchain Dependencies
Generated: ${TIMESTAMP}

Build System:
- CMake $(cmake --version 2>/dev/null | head -n1 || echo "not found")
- Make $(make --version 2>/dev/null | head -n1 || echo "not found")

Compiler:
- GCC $(gcc --version 2>/dev/null | head -n1 || echo "not found")
- Clang $(clang --version 2>/dev/null | head -n1 || echo "not found")

External Dependencies (from CMakeLists.txt):
- Protocol Buffers
- gRPC
- RocksDB
- libp2p
- libsodium
- BLST (BLS12-381)
- Blake3
- Google Test
- RapidCheck
- Google Benchmark

For detailed dependency versions, see CMakeLists.txt in the source archive.
EOF
echo "  ✓ Dependency information collected"

# 9. Generate package manifest
echo -e "${GREEN}Generating package manifest...${NC}"
cat > "${PACKAGE_DIR}/MANIFEST.md" << EOF
# Sarafu Blockchain Security Audit Package

**Generated:** ${TIMESTAMP}  
**Package Name:** ${PACKAGE_NAME}  
**Git Commit:** ${GIT_COMMIT}  
**Git Branch:** ${GIT_BRANCH}  
**Git Tag:** ${GIT_TAG}

## Package Contents

### 1. Source Code
- \`code/sarafu-blockchain-source.tar.gz\` - Complete source code snapshot

### 2. Documentation
- \`docs/\` - All project documentation
  - Architecture documentation
  - API reference
  - Operational guides
  - Whitepaper

### 3. Test Suite
- \`tests/\` - Complete test suite
  - Unit tests
  - Property-based tests
  - Integration tests
  - Stress tests
  - Fuzz tests

### 4. Test Reports
- \`reports/test-results.txt\` - Test execution results
- \`reports/coverage-summary.txt\` - Code coverage summary
- \`reports/coverage-html/\` - HTML coverage report
- \`reports/code-audit.txt\` - Code quality audit results
- \`reports/benchmark-results.txt\` - Performance benchmark results
- \`reports/dependencies.txt\` - Dependency information

### 5. Audit Documentation
- \`docs/AUDIT_PACKAGE.md\` - Audit scope and instructions
- \`MANIFEST.md\` - This file

## Audit Scope

The security audit should cover the following areas:

### 1. Consensus Implementation
- HotStuff BFT consensus protocol
- View-change mechanism
- Block finalization via Quorum Certificates
- Leader election and rotation

### 2. Cryptographic Operations
- Blake3 hashing implementation
- Ed25519 signature verification
- BLS12-381 signature aggregation
- Merkle tree construction and verification
- Key generation and management

### 3. Slashing Logic
- Double-sign detection
- Quadratic correlated penalty calculation
- Slashing penalty distribution
- Validator jailing and unjailing

### 4. Economic Mechanisms
- Issuance rate calculation (k * total_supply)
- Fee market dynamics
- Validator reward distribution
- Staking and unbonding logic

### 5. Network Security
- Peer authentication and authorization
- Message validation and sanitization
- DoS protection mechanisms
- Network partition handling

### 6. State Management
- Transaction validation
- Balance updates and conservation
- State root calculation
- Database integrity

## Severity Levels

Findings should be classified using the following severity levels:

- **Critical**: Can cause loss of funds, consensus failure, or network halt
- **High**: Can cause significant security degradation or economic loss
- **Medium**: Can cause minor security issues or unexpected behavior
- **Low**: Code quality issues or best practice violations
- **Informational**: Suggestions for improvement

## Contact Information

For questions or clarifications during the audit:
- Email: security@sarafu.network
- GitHub: https://github.com/sarafu-network/sarafu-blockchain

## Verification

Package integrity can be verified using:
\`\`\`bash
sha256sum -c checksums.txt
\`\`\`

EOF
echo "  ✓ Package manifest generated"

# 10. Generate checksums
echo -e "${GREEN}Generating checksums...${NC}"
cd "${PACKAGE_DIR}"
find . -type f -exec sha256sum {} \; > checksums.txt
cd - > /dev/null
echo "  ✓ Checksums generated"

# 11. Create final archive
echo -e "${GREEN}Creating final archive...${NC}"
tar -czf "${PACKAGE_NAME}.tar.gz" -C audit-packages "${PACKAGE_NAME}"
ARCHIVE_SIZE=$(du -h "${PACKAGE_NAME}.tar.gz" | cut -f1)
echo "  ✓ Archive created: ${PACKAGE_NAME}.tar.gz (${ARCHIVE_SIZE})"

# 12. Generate final checksum
ARCHIVE_CHECKSUM=$(sha256sum "${PACKAGE_NAME}.tar.gz" | cut -d' ' -f1)

# Summary
echo ""
echo "=========================================="
echo -e "${GREEN}Audit package prepared successfully!${NC}"
echo "=========================================="
echo ""
echo "Package Details:"
echo "  Name: ${PACKAGE_NAME}"
echo "  Location: ${PACKAGE_NAME}.tar.gz"
echo "  Size: ${ARCHIVE_SIZE}"
echo "  SHA256: ${ARCHIVE_CHECKSUM}"
echo ""
echo "Package Contents:"
echo "  - Source code snapshot (git ${GIT_COMMIT:0:8})"
echo "  - Complete documentation"
echo "  - Test suite and results"
echo "  - Code coverage reports"
echo "  - Performance benchmarks"
echo "  - Dependency information"
echo ""
echo "Next Steps:"
echo "  1. Review the package contents in: ${PACKAGE_DIR}/"
echo "  2. Share ${PACKAGE_NAME}.tar.gz with audit firms"
echo "  3. Provide the SHA256 checksum for verification"
echo "  4. Refer auditors to docs/AUDIT_PACKAGE.md for scope"
echo ""
