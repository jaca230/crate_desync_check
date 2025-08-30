#!/bin/bash

# Help message
show_help() {
    echo "Usage: ./screen.sh <mode> [arguments]"
    echo
    echo "Runs the specified mode in a detached screen session"
    echo
    echo "Modes:"
    echo "  desync       Run the desync checker"
    echo "               Usage: ./screen.sh desync <data_directory>"
    echo
    echo "  analyze      Run the corruption analyzer (event-based estimates)"
    echo "               Usage: ./screen.sh analyze <results.txt_path>"
    echo
    echo "  subruns      Run the subrun corruption density analyzer"
    echo "               Usage: ./screen.sh subruns <results.txt_path>"
    echo
    echo "  -h, --help   Display this help message"
    echo
    echo "Examples:"
    echo "  ./screen.sh desync /path/to/data"
    echo "  ./screen.sh analyze scripts/results.txt"
    echo "  ./screen.sh subruns scripts/results.txt"
    echo
    echo "Screen commands:"
    echo "  Attach to session: screen -r desync_scan"
    echo "  Detach from session: Ctrl-a d"
    echo "  Kill session: screen -S desync_scan -X quit"
}

# Check if at least one argument is provided
if [ $# -eq 0 ]; then
    echo "[ERROR] No mode specified"
    show_help
    exit 1
fi

# Check for help
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    show_help
    exit 0
fi

# Validate mode
MODE="$1"
case "$MODE" in
    desync|desync_check|analyze|analyzer|subruns|subrun)
        # Valid modes
        ;;
    *)
        echo "[ERROR] Unknown mode: $MODE"
        show_help
        exit 1
        ;;
esac

SESSION="desync_scan"

# Kill old session if it exists
if screen -list | grep -q "$SESSION"; then
    echo "[INFO] Killing old screen session: $SESSION"
    screen -S "$SESSION" -X quit
    sleep 1  # Give it a moment to clean up
fi

# Build the command with proper argument escaping
SCRIPT_DIR=$(dirname "$(realpath "$0")")
RUN_SCRIPT="$SCRIPT_DIR/run.sh"

# Create the command string with proper quoting
COMMAND="$RUN_SCRIPT"
for arg in "$@"; do
    # Escape arguments that might contain spaces or special characters
    COMMAND="$COMMAND \"$arg\""
done

# Start new screen session
echo "[INFO] Starting screen session '$SESSION' with command: $COMMAND"
screen -dmS "$SESSION" bash -c "$COMMAND; echo '[INFO] Command completed. Press Enter to exit or Ctrl-a d to detach.'; read"

echo "[INFO] Started screen session '$SESSION'."
echo "Attach with: screen -r $SESSION"
echo "Detach with: Ctrl-a d"
echo "Kill session: screen -S $SESSION -X quit"