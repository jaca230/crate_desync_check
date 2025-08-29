#!/bin/bash

# Get absolute paths
SCRIPT_DIR=$(dirname "$(realpath "$0")")
BASE_DIR=$(realpath "$SCRIPT_DIR/..")

echo "[cleanup.sh] Cleaning project build artifacts in: $BASE_DIR"

# Directories to remove (expandable if needed)
DIRS_TO_CLEAN=(
    "$BASE_DIR/build"
    "$BASE_DIR/bin"
    "$BASE_DIR/lib"
)

for DIR in "${DIRS_TO_CLEAN[@]}"; do
    if [ -d "$DIR" ]; then
        echo "[cleanup.sh] Removing: $(realpath "$DIR")"
        rm -rf "$DIR"
    else
        echo "[cleanup.sh] Skipping: $DIR (does not exist)"
    fi
done

echo "[cleanup.sh] Cleanup complete."
