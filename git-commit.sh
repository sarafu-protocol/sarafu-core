#!/bin/bash

# git-commit.sh - Script to add and commit files individually with descriptive messages

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

echo "Starting git commit process..."
echo "========================================"

# Commit .gitignore first
if [ -f .gitignore ]; then
    commit_file ".gitignore" "Update .gitignore with project exclusions"
fi

# Commit documentation files
if [ -f "README.md" ]; then
    commit_file "README.md" "Update README with project information"
fi

# Commit source code files
# C++ source files
for file in $(find . -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | head -20); do
    if [ -f "$file" ]; then
        commit_file "$file" "Update source file: $(basename "$file")"
    fi
done

# Configuration files
if [ -f "CMakeLists.txt" ]; then
    commit_file "CMakeLists.txt" "Update build configuration"
fi

if [ -f "Makefile" ]; then
    commit_file "Makefile" "Update Makefile"
fi

# Configuration files
if [ -f "config.toml" ]; then
    commit_file "config.toml" "Update configuration"
fi

# Documentation files
for file in $(find . -name "*.md" -o -name "*.txt" | grep -v node_modules | head -10); do
    if [ -f "$file" ]; then
        commit_file "$file" "Update documentation: $(basename "$file")"
    fi
done

# Script files
for file in $(find . -name "*.sh" -o -name "*.py" -o -name "*.js" | head -10); do
    if [ -f "$file" ]; then
        commit_file "$file" "Update script: $(basename "$file")"
    fi
done

echo "========================================"
echo "Git commit process completed!"
echo ""
echo "To push changes to remote repository:"
echo "  git push origin $(git branch --show-current)"
echo ""
echo "To see the commit history:"
echo "  git log --oneline --graph --decorate"
echo ""
echo "To see the current status:"
echo "  git status"
echo ""
echo "To see what will be committed:"
echo "  git status --short"