```bash
#!/bin/bash

# PRATIKSHA Automatic Git Commit
set -u

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR" || exit 1

while true; do
    sleep 10

    # Only commit actual project files
    git add -- main.cpp index.html

    if ! git diff --cached --quiet; then
        git commit -m "Auto-update: $(date '+%Y-%m-%d %H:%M:%S')"
    fi
done
```