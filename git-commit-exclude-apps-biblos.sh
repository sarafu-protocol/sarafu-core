#!/bin/bash

# git-commit-exclude-apps-biblos.sh
# Script to commit all changes except files in apps/ and biblos/ directories

echo "=== Git Commit - Excluding apps/ and biblos/ ==="
echo ""

# Show git status
echo "Current git status:"
echo "-----------------"
git status --short

echo ""
echo "Files that will be committed (excluding apps/ and biblos/):"
echo "--------------------------------------------------------"
git status --short | grep -v "^[? ]*apps/" | grep -v "^[? ]*biblos/" | grep -v "^[? ]*\./apps/" | grep -v "^[? ]*\./biblos/"

echo ""
echo "Files that will be EXCLUDED (in apps/ and biblos/):"
echo "------------------------------------------------"
git status --short | grep -E "(apps/|biblos/)" || echo "None"

echo ""
echo "To commit all changes (excluding apps/ and biblos/):"
echo "  git add ."
echo "  git reset -- apps/ biblos/"
echo "  git commit -m 'Your commit message'"
echo ""
echo "Or run: ./commit-core.sh"
echo ""