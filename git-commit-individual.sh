#!/bin/bash

# git-commit-individual.sh - Script to add and commit each file individually with descriptive messages

# Function to commit a file with a specific message
commit_file() {
    local file="$1"
    local message="$2"
    
    echo "Committing: $file"
    echo "Message: $message"
    
    # Add the specific file
    git add "$file"
    
    # Commit with message
    git commit -m "$message" "$file"
    
    if [ $? -eq 0 ]; then
        echo "✓ Committed: $file"
    else
        echo "✗ Failed to commit: $file"
    fi
    echo ""
}

# Make sure we're in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo "Error: Not a git repository"
    exit 1
fi

echo "Starting individual git commit process..."
echo "========================================"
echo ""

# First, commit .gitignore
if [ -f .gitignore ]; then
    commit_file ".gitignore" "chore: Update .gitignore with project exclusions and build artifacts"
fi

# Commit README files
if [ -f "README.md" ]; then
    commit_file "README.md" "docs: Update project README with comprehensive information"
fi

# Commit Sarafu whitepaper files
if [ -f "Sarafu-Whitepaper-v5.0.md" ]; then
    commit_file "Sarafu-Whitepaper-v5.0.md" "docs: Add Sarafu Whitepaper v5.0 with updated specifications"
fi

if [ -f "Sarafu-Whitepaper.md" ]; then
    commit_file "Sarafu-Whitepaper.md" "docs: Update Sarafu Whitepaper documentation"
fi

# Commit configuration files
if [ -f "CMakeLists.txt" ]; then
    commit_file "CMakeLists.txt" "build: Update CMake build configuration"
fi

if [ -f "Makefile" ]; then
    commit_file "Makefile" "build: Update Makefile for build automation"
fi

if [ -f "config.mainnet.toml" ]; then
    commit_file "config.mainnet.toml" "config: Update mainnet configuration"
fi

if [ -f "config.testnet.toml" ]; then
    commit_file "config.testnet.toml" "config: Update testnet configuration"
fi

# Commit header files (.h)
echo "Committing header files..."
for file in $(find . -name "*.h" -type f | grep -v node_modules | grep -v build | head -30); do
    if [ -f "$file" ]; then
        filename=$(basename "$file")
        commit_file "$file" "feat: Update header file $filename"
    fi
done

# Commit source files (.cpp)
echo "Committing source files..."
for file in $(find . -name "*.cpp" -type f | grep -v node_modules | grep -v build | head -30); do
    if [ -f "$file" ]; then
        filename=$(basename "$file")
        commit_file "$file" "feat: Update source file $filename"
    fi
done

# Commit biblos documentation files
echo "Committing biblos documentation files..."
if [ -f "biblos/03_Consensus_Deep_Dive.md" ]; then
    commit_file "biblos/03_Consensus_Deep_Dive.md" "docs: Update Consensus Deep Dive documentation"
fi

if [ -f "biblos/04_State_Machine_and_Transactions.md" ]; then
    commit_file "biblos/04_State_Machine_and_Transactions.md" "docs: Update State Machine and Transactions documentation"
fi

if [ -f "biblos/06_Staking_and_Validator_Economics.md" ]; then
    commit_file "biblos/06_Staking_and_Validator_Economics.md" "docs: Update Staking and Validator Economics documentation"
fi

if [ -f "biblos/07_Governance_Constitution.md" ]; then
    commit_file "biblos/07_Governance_Constitution.md" "docs: Update Governance Constitution documentation"
fi

if [ -f "biblos/08_Paper_Token_System.md" ]; then
    commit_file "biblos/08_Paper_Token_System.md" "docs: Update Paper Token System documentation"
fi

if [ -f "biblos/09_RPC_APIs_and_Client_Integration.md" ]; then
    commit_file "biblos/09_RPC_APIs_and_Client_Integration.md" "docs: Update RPC APIs and Client Integration documentation"
fi

if [ -f "biblos/10_Building_DApps_on_Sarafu.md" ]; then
    commit_file "biblos/10_Building_DApps_on_Sarafu.md" "docs: Update Building DApps on Sarafu documentation"
fi

if [ -f "biblos/COMPLETION_STATUS.md" ]; then
    commit_file "biblos/COMPLETION_STATUS.md" "docs: Update completion status documentation"
fi

if [ -f "biblos/mkdocs.yml" ]; then
    commit_file "biblos/mkdocs.yml" "docs: Update mkdocs configuration"
fi

if [ -f "biblos/mkdocs-pdf.yml" ]; then
    commit_file "biblos/mkdocs-pdf.yml" "docs: Update mkdocs PDF configuration"
fi

# Commit portfolio application files
echo "Committing portfolio application files..."
if [ -f "apps/portolio/index.html" ]; then
    commit_file "apps/portolio/index.html" "feat: Update portfolio index page"
fi

if [ -f "apps/portolio/portfolio.html" ]; then
    commit_file "apps/portolio/portfolio.html" "feat: Update portfolio main page"
fi

if [ -f "apps/portolio/style.css" ]; then
    commit_file "apps/portolio/style.css" "style: Update portfolio styles"
fi

if [ -f "apps/portolio/script.js" ]; then
    commit_file "apps/portolio/script.js" "feat: Update portfolio JavaScript functionality"
fi

# Commit test files
echo "Committing test files..."
for file in $(find tests/ -name "*.cpp" -type f | head -20); do
    if [ -f "$file" ]; then
        filename=$(basename "$file")
        commit_file "$file" "test: Update test file $filename"
    fi
done

# Commit any remaining modified files
echo "Committing any remaining modified files..."
git status --porcelain | while read status file; do
    if [[ "$status" == "M" || "$status" == "A" ]]; then
        if [ -f "$file" ]; then
            filename=$(basename "$file")
            commit_file "$file" "chore: Update $filename"
        fi
    fi
done

echo "========================================"
echo "Individual git commit process completed!"
echo ""
echo "Summary:"
echo "  Total commits made: $(git log --oneline | wc -l | tr -d ' ') (including previous commits)"
echo ""
echo "To push changes to remote repository:"
echo "  git push origin $(git branch --show-current)"
echo ""
echo "To see the commit history:"
echo "  git log --oneline --graph --decorate -20"
echo ""
echo "To see the current status:"
echo "  git status"
echo ""
echo "To see what files are staged:"
echo "  git diff --cached --name-only"