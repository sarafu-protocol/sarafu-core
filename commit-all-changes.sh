#!/bin/bash

# commit-all-changes.sh
# Script to commit all changes with individual commits for each file

echo "Starting commit process for all modified and new files..."
echo "=================================================="
echo ""

# First, let's check git status
echo "Current git status:"
git status --short

echo ""
echo "Staging all changes..."
git add .

echo ""
echo "Creating individual commits for each file type..."

# Function to commit files by pattern
commit_files_by_pattern() {
    pattern="$1"
    message="$2"
    
    echo ""
    echo "Processing: $message"
    echo "Pattern: $pattern"
    
    # Find files matching pattern
    files=$(find . -name "$pattern" -type f)
    
    for file in $files; do
        if [ -f "$file" ]; then
            echo "  Committing: $file"
            git add "$file"
            git commit -m "$message: $(basename "$file")" "$file"
        fi
    done
}

# Commit by file type
echo ""
echo "Committing C++ files..."
find . -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | while read file; do
    if [ -f "$file" ]; then
        echo "  Committing: $file"
        git add "$file"
        git commit -m "feat: Update $(basename "$file")" "$file"
    fi
done

echo ""
echo "Committing documentation files..."
find . -name "*.md" -o -name "*.txt" | while read file; do
    if [ -f "$file" ]; then
        echo "  Committing: $file"
        git add "$file"
        git commit -m "docs: Update $(basename "$file")" "$file"
    fi
done

echo ""
echo "Committing configuration files..."
find . -name "*.toml" -o -name "*.yml" -o -name "*.yaml" -o -name "*.json" | while read file; do
    if [ -f "$file" ]; then
        echo "  Committing: $file"
        git add "$file"
        git commit -m "config: Update $(basename "$file")" "$file"
    fi
done

echo ""
echo "Committing script files..."
find . -name "*.sh" -o -name "*.py" -o -name "*.js" | while read file; do
    if [ -f "$file" ]; then
        echo "  Committing: $file"
        git add "$file"
        git commit -m "chore: Update $(basename "$file")" "$file"
    fi
done

echo ""
echo "All changes have been committed individually."
echo ""
echo "To push to remote:"
echo "  git push origin $(git branch --show-current)"
echo ""
echo "Commit summary:"
git log --oneline -10