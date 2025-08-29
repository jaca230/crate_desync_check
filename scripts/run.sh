#!/bin/bash
# Save working directory
ORIG_DIR=$(pwd)
SCRIPT_DIR=$(dirname "$(realpath "$0")")
BASE_DIR="$SCRIPT_DIR/.."
EXEC="$BASE_DIR/build/bin/unpacker"

if [ ! -f "$EXEC" ]; then
    echo "[ERROR] Executable not found at $EXEC"; exit 1
fi

# Default run
ARGS=("$@")
echo "[run.sh, INFO] Running unpacker with args: ${ARGS[*]}"
"$EXEC" "${ARGS[@]}"
cd "$ORIG_DIR"
