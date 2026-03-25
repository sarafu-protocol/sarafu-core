#!/bin/bash

# git-commit-core.sh - Commit only core blockchain files (exclude apps/ and biblos/)

echo "Starting git commit process for CORE FILES ONLY..."
echo "=================================================="
echo ""

# First, let's check what's changed
echo "Checking git status..."
git status

echo ""
echo "Files to be committed (excluding apps/ and biblos/):"
git status --porcelain | grep -v "^[? ]*apps/" | grep -v "^[? ]*biblos/"

echo ""
read -p "Do you want to proceed with committing core files only? (y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]
then
    echo "Aborted."
    exit 1
fi

echo ""
echo "Starting individual commits for core files..."

# Get list of modified and new files, excluding apps and biblos
FILES_TO_COMMIT=$(git status --porcelain | grep -v "^[? ]*apps/" | grep -v "^[? ]*biblos/" | grep -E '^[MADRCU?]' | awk '{print $2}')

if [ -z "$FILES_TO_COMMIT" ]; then
    echo "No core files to commit (excluding apps/ and biblos/)."
    exit 0
fi

for file in $FILES_TO_COMMIT; do
    if [ -f "$file" ]; then
        echo ""
        echo "Committing: $file"
        git add "$file"
        
        # Create a commit message based on file type and name
        filename=$(basename "$file")
        extension="${filename##*.}"
        
        case "$extension" in
            "md"|"txt"|"MD"|"TXT")
                commit_msg="docs: Update documentation $filename"
                ;;
            "cpp"|"h"|"hpp"|"c"|"hxx"|"hpp")
                commit_msg="feat: Update source file $filename"
                ;;
            "js"|"jsx"|"ts"|"tsx")
                commit_msg="feat: Update JavaScript/TypeScript file $filename"
                ;;
            "css"|"scss"|"sass"|"less")
                commit_msg="style: Update stylesheet $filename"
                ;;
            "html"|"htm"|"xhtml")
                commit_msg="feat: Update HTML file $filename"
                ;;
            "json"|"yaml"|"yml"|"toml")
                commit_msg="config: Update configuration $filename"
                ;;
            "sh"|"bash"|"zsh")
                commit_msg="chore: Update script $filename"
                ;;
            *)
                commit_msg="chore: Update $filename"
                ;;
        esac
        
        git commit -m "$commit_msg" "$file"
        echo "✓ Committed: $filename"
    fi
done

echo ""
echo "=================================================="
echo "Core files have been committed (apps/ and biblos/ excluded)!"
echo ""
echo "Commit summary:"
git log --oneline -10
echo ""
echo "To push to remote: git push origin $(git branch --show-current)"
echo "To view status: git status"
echo "To view commit history: git log --oneline -20"
echo ""
echo "Note: apps/ and biblos/ directories are excluded from commits as per .gitignore"