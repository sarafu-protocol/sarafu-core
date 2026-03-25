#!/bin/bash

# git-commit-all.sh - Commit all changes with individual commits for each file

echo "Starting git commit process for all modified files..."
echo "=================================================="
echo ""

# First, let's check what's changed
echo "Checking git status..."
git status

echo ""
echo "Files to be committed:"
git status --porcelain

echo ""
read -p "Do you want to proceed with committing all changes? (y/n): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]
then
    echo "Aborted."
    exit 1
fi

echo ""
echo "Starting individual commits..."

# Get list of modified and new files
FILES_TO_COMMIT=$(git status --porcelain | grep -E '^[MADRCU?]' | awk '{print $2}')

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
echo "All files have been committed individually!"
echo ""
echo "Commit summary:"
git log --oneline -10
echo ""
echo "To push to remote: git push origin $(git branch --show-current)"
echo "To view status: git status"
echo "To view commit history: git log --oneline -20"