
#!/bin/bash

set -u

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR" || exit 1

while true; do
    sleep 10

    git add -u

    git add -- main.cpp index.html autocommit.sh

    if ! git diff --cached --quiet; then
        git commit -m "Auto-update: $(date '+%Y-%m-%d %H:%M:%S')" || continue
        git push origin main || echo "Push failed; commit is saved locally."
    fi
done
