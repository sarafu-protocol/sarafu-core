# Git Commit Scripts

This directory contains scripts to help with committing changes to the repository.

## Available Scripts

### 1. `git-commit-all.sh`

The main script for committing all changes with individual commits for each file.

**Usage:**

```bash
./git-commit-all.sh
```

This script will:

1. Show you all modified and new files
2. Ask for confirmation before proceeding
3. Commit each file individually with appropriate commit messages
4. Show a summary of commits made

### 2. `git-commit-individual.sh`

A more detailed script that commits files by category (C++ files, documentation, configs, etc.)

**Usage:**

```bash
./git-commit-individual.sh
```

### 3. `commit-all-changes.sh`

An alternative script that groups files by type and commits them.

### 4. `git-commit.sh`

The original simple script for basic git operations.

## How to Use

1. **First, review what will be committed:**

   ```bash
   git status
   ```

2. **Run the commit script:**

   ```bash
   ./git-commit-all.sh
   ```

3. **Review the changes:**

   ```bash
   git log --oneline -10
   ```

4. **Push to remote repository:**
   ```bash
   git push origin $(git branch --show-current)
   ```

## What Gets Committed

The scripts will:

- Commit each file individually with descriptive commit messages
- Group files by type (code, docs, configs, scripts)
- Use conventional commit messages
- Preserve your git history with meaningful commit messages

## Customization

You can edit the commit messages in the scripts to match your project's conventions.

## Important Notes

1. **Backup your work** before running scripts
2. **Review changes** before committing
3. **Test the scripts** on a small set of files first
4. The scripts will **not** commit ignored files (as per `.gitignore`)

## .gitignore Updates

The `.gitignore` file has been updated to exclude:

- Build artifacts
- Node modules
- IDE files
- Generated files
- Log files
- Sensitive configuration

## Troubleshooting

If you encounter issues:

1. Check that you have git installed: `git --version`
2. Ensure you're in a git repository
3. Make sure you have write permissions
4. Check for uncommitted changes with `git status`

## Quick Start

```bash
# Make scripts executable
chmod +x git-commit-all.sh git-commit-individual.sh commit-all-changes.sh

# Run the main script
./git-commit-all.sh
```

## Notes

- The scripts will not force push or modify remote branches
- Each file gets its own commit for better history tracking
- Commit messages follow conventional commit format
