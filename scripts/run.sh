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
    echo "  desync       Run the desync checker (scans directory)"
    echo "               Usage: ./run.sh desync <data_directory>"
    echo
    echo "  inspect      Inspect a single file for desyncs"
    echo "               Usage: ./run.sh inspect <file_path>"
    echo
    echo "  run          Inspect all subruns for a specific run"
    echo "               Usage: ./run.sh run <base_directory> <run_number> [start_subrun]"
    echo
    echo "  analyze      Run the corruption analyzer (event-based estimates)"
    echo "               Usage: ./run.sh analyze <results.txt_path>"
    echo
    echo "  subruns      Run the subrun corruption density analyzer"
    echo "               Usage: ./run.sh subruns <results.txt_path> [sample_fraction]"
    echo "               sample_fraction: 0.0-1.0 (default 1.0 = check all)"
    echo
    echo "  -h, --help   Display this help message"
    echo
    echo "Examples:"
    echo "  ./run.sh desync /path/to/data"
    echo "  ./run.sh inspect /path/to/run12345_00001.mid.lz4"
    echo "  ./run.sh run /path/to/data 12345"
    echo "  ./run.sh run /path/to/data 248 600"
    echo "  ./run.sh analyze scripts/results.txt"
    echo "  ./run.sh subruns scripts/results.txt"
    echo "  ./run.sh subruns scripts/results.txt 0.1"
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
        
    inspect|inspect_file)
        EXEC="$BASE_DIR/build/bin/inspect_file"
        if [ ! -f "$EXEC" ]; then
            echo "[ERROR] File inspector executable not found at $EXEC"
            echo "        Run ./build.sh first"
            exit 1
        fi
        
        if [ $# -ne 1 ]; then
            echo "[ERROR] Inspect mode requires exactly 1 argument: <file_path>"
            echo "Usage: ./run.sh inspect <file_path>"
            exit 1
        fi
        
        echo "[run.sh, INFO] Running inspect_file with file: $1"
        ;;
        
    run|inspect_run)
        EXEC="$BASE_DIR/build/bin/inspect_run"
        if [ ! -f "$EXEC" ]; then
            echo "[ERROR] Run inspector executable not found at $EXEC"
            echo "        Run ./build.sh first"
            exit 1
        fi
        
        if [ $# -lt 2 ] || [ $# -gt 3 ]; then
            echo "[ERROR] Run mode requires 2 or 3 arguments: <base_directory> <run_number> [start_subrun]"
            echo "Usage: ./run.sh run <base_directory> <run_number> [start_subrun]"
            exit 1
        fi
        
        echo "[run.sh, INFO] Running inspect_run with:"
        echo "               Base directory: $1"
        echo "               Run number: $2"
        if [ $# -eq 3 ]; then
            echo "               Starting subrun: $3"
        fi
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
        
    subruns|subrun)
        EXEC="$BASE_DIR/build/bin/analyze_subruns"
        if [ ! -f "$EXEC" ]; then
            echo "[ERROR] Subrun analyzer executable not found at $EXEC"
            echo "        Run ./build.sh first"
            exit 1
        fi
        
        if [ $# -lt 1 ] || [ $# -gt 2 ]; then
            echo "[ERROR] Subruns mode requires 1 or 2 arguments: <results.txt_path> [sample_fraction]"
            echo "Usage: ./run.sh subruns <results.txt_path> [sample_fraction]"
            echo "       sample_fraction: 0.0-1.0 (default 1.0 = check all)"
            exit 1
        fi
        
        echo "[run.sh, INFO] Running analyze_subruns with:"
        echo "               Results file: $1"
        if [ $# -eq 2 ]; then
            echo "               Sample fraction: $2"
        else
            echo "               Sample fraction: 1.0 (default - check all)"
        fi
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