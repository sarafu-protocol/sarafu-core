#!/bin/bash

# Quick commit script for individual files
# Usage: ./quick-commit.sh "Commit message" file1 file2 file3 ...

if [ $# -lt 2 ]; then
    echo "Usage: $0 \"commit message\" file1 file2 ..."
    exit 1
fi

COMMIT_MSG="$1"
shift

for file in "$@"; do
    if [ -f "$file" ]; then
        echo "Committing: $file"
        git add "$file"
        git commit -m "$COMMIT_MSG: $(basename "$file")" "$file"
    else
        echo "File not found: $file"
    fi
done

echo "Done!"