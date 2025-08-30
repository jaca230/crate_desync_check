#!/bin/bash

# Save working directory
ORIG_DIR=$(pwd)
SCRIPT_DIR=$(dirname "$(realpath "$0")")
BASE_DIR="$SCRIPT_DIR/.."

# Help message
show_help() {
    echo "Usage: ./run.sh <mode> [arguments]"
    echo
    echo "Modes:"
    echo "  desync       Run the desync checker"
    echo "               Usage: ./run.sh desync <data_directory>"
    echo
    echo "  analyze      Run the corruption analyzer"
    echo "               Usage: ./run.sh analyze <results.txt_path>"
    echo
    echo "  -h, --help   Display this help message"
    echo
    echo "Examples:"
    echo "  ./run.sh desync /path/to/data"
    echo "  ./run.sh analyze scripts/results.txt"
}

# Check if at least one argument is provided
if [ $# -eq 0 ]; then
    echo "[ERROR] No mode specified"
    show_help
    exit 1
fi

# Parse mode
MODE="$1"
shift  # Remove mode from arguments

case "$MODE" in
    desync|desync_check)
        EXEC="$BASE_DIR/build/bin/desync_checker"
        if [ ! -f "$EXEC" ]; then
            echo "[ERROR] Desync checker executable not found at $EXEC"
            echo "        Run ./build.sh first"
            exit 1
        fi
        
        if [ $# -ne 1 ]; then
            echo "[ERROR] Desync mode requires exactly 1 argument: <data_directory>"
            echo "Usage: ./run.sh desync <data_directory>"
            exit 1
        fi
        
        echo "[run.sh, INFO] Running desync_checker with directory: $1"
        ;;
        
    analyze|analyzer)
        EXEC="$BASE_DIR/build/bin/analyze_results"
        if [ ! -f "$EXEC" ]; then
            echo "[ERROR] Analyzer executable not found at $EXEC"
            echo "        Run ./build.sh first"
            exit 1
        fi
        
        if [ $# -ne 1 ]; then
            echo "[ERROR] Analyze mode requires exactly 1 argument: <results.txt_path>"
            echo "Usage: ./run.sh analyze <results.txt_path>"
            echo "Note: Data directory is automatically extracted from file paths in results.txt"
            exit 1
        fi
        
        echo "[run.sh, INFO] Running analyze_results with:"
        echo "               Results file: $1"
        ;;
        
    -h|--help)
        show_help
        exit 0
        ;;
        
    *)
        echo "[ERROR] Unknown mode: $MODE"
        show_help
        exit 1
        ;;
esac

# Run the selected executable with remaining arguments
"$EXEC" "$@"
EXIT_CODE=$?

# Return to original directory
cd "$ORIG_DIR"

# Exit with the same code as the executable
exit $EXIT_CODE