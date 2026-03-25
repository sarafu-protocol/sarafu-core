#!/bin/bash

# Simple script to commit only core blockchain files (excluding apps/ and biblos/)

echo "=== Committing Core Blockchain Files ==="
echo "This will commit all changes except files in apps/ and biblos/ directories"
echo ""

# Show what will be committed
echo "Files to be committed (excluding apps/ and biblos/):"
echo "--------------------------------------------------"
git status --porcelain | grep -v "^[?MADRCU?].*apps/" | grep -v "^[?MADRCU?].*biblos/" | while read status file; do
    echo "  $file"
done

echo ""
read -p "Do you want to proceed with commit? (y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]
then
    echo "Commit cancelled."
    exit 1
fi

# Stage all files
echo "Staging files..."
git add .

# Unstage apps/ and biblos/ directories
echo "Unstaging apps/ and biblos/ directories..."
git reset -- apps/ biblos/

# Show what's staged
echo ""
echo "Files staged for commit:"
git diff --cached --name-only

echo ""
read -p "Enter commit message: " commit_msg

if [ -z "$commit_msg" ]; then
    commit_msg="Update core blockchain files"
fi

git commit -m "$commit_msg"

echo ""
echo "Commit completed!"
echo "To push to remote: git push origin $(git branch --show-current)"