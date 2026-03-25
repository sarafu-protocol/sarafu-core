#!/bin/bash

# git-commit-all.sh - Main script for committing Sarafu blockchain changes
# Excludes apps/ and biblos/ directories from commits

echo "========================================"
echo "Sarafu Blockchain - Git Commit Helper"
echo "========================================"
echo ""
echo "This script will help you commit changes to the Sarafu blockchain repository."
echo "Note: apps/ and biblos/ directories are excluded from commits."
echo ""

# Show current git status
echo "Current git status:"
echo "-----------------"
git status --short

echo ""
echo "Files that will be committed (excluding apps/ and biblos/):"
echo "--------------------------------------------------------"
git status --short | grep -v "^[?MADRCU?].*apps/" | grep -v "^[?MADRCU?].*biblos/"

echo ""
echo "Files that will be EXCLUDED (in apps/ and biblos/):"
echo "------------------------------------------------"
git status --short | grep -E "(apps/|biblos/)" || echo "None"

echo ""
echo "Options:"
echo "1. Commit all changes (excluding apps/ and biblos/)"
echo "2. View what will be committed"
echo "3. View .gitignore rules"
echo "4. Exit"
echo ""
read -p "Select option (1-4): " choice

case $choice in
    1)
        echo ""
        echo "Staging files (excluding apps/ and biblos/)..."
        git add .
        git reset -- apps/ biblos/
        
        echo ""
        echo "Files staged for commit:"
        git diff --cached --name-only
        
        echo ""
        read -p "Enter commit message: " commit_msg
        
        if [ -z "$commit_msg" ]; then
            commit_msg="Update: $(date +"%Y-%m-%d %H:%M:%S")"
        fi
        
        echo ""
        echo "Committing changes..."
        git commit -m "$commit_msg"
        
        echo ""
        echo "✓ Changes committed successfully!"
        echo "To push to remote: git push origin $(git branch --show-current)"
        ;;
        
    2)
        echo ""
        echo "Files that will be committed (excluding apps/ and biblos/):"
        echo "--------------------------------------------------------"
        git status --short | grep -v "^[?MADRCU?].*apps/" | grep -v "^[?MADRCU?].*biblos/"
        echo ""
        echo "Total files to commit: $(git status --short | grep -v "^[?MADRCU?].*apps/" | grep -v "^[?MADRCU?].*biblos/" | wc -l)"
        ;;
        
    3)
        echo ""
        echo "Current .gitignore rules for apps/ and biblos/:"
        echo "---------------------------------------------"
        grep -E "(apps/|biblos/)" .gitignore || echo "No specific rules found for apps/ or biblos/"
        echo ""
        echo "Full .gitignore rules for exclusion:"
        echo "--------------------------------"
        grep -A2 -B2 "apps/\|biblos/" .gitignore || echo "No specific rules found"
        ;;
        
    4)
        echo "Exiting..."
        exit 0
        ;;
        
    *)
        echo "Invalid option"
        ;;
esac

echo ""
echo "Script completed."