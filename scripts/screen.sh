#!/bin/bash

# Help message
show_help() {
    echo "Usage: ./screen.sh <mode> [arguments] [-s|--session <session_name>]"
    echo
    echo "Runs the specified mode in a detached screen session"
    echo
    echo "Modes:"
    echo "  desync       Run the desync checker"
    echo "               Usage: ./screen.sh desync <data_directory> [-s session_name]"
    echo
    echo "  analyze      Run the corruption analyzer (event-based estimates)"
    echo "               Usage: ./screen.sh analyze <results.txt_path> [-s session_name]"
    echo
    echo "  subruns      Run the subrun corruption density analyzer"
    echo "               Usage: ./screen.sh subruns <results.txt_path> [sample_fraction] [-s session_name]"
    echo
    echo "Options:"
    echo "  -s, --session <name>  Specify custom screen session name (default: desync_scan)"
    echo "  -h, --help           Display this help message"
    echo
    echo "Examples:"
    echo "  ./screen.sh desync /path/to/data"
    echo "  ./screen.sh analyze scripts/results.txt -s my_analysis"
    echo "  ./screen.sh subruns scripts/results.txt 0.1 --session run248_check"
    echo
    echo "Screen commands:"
    echo "  Attach to session: screen -r <session_name>"
    echo "  List sessions: screen -ls"
    echo "  Detach from session: Ctrl-a d"
    echo "  Kill session: screen -S <session_name> -X quit"
}

# Default session name
DEFAULT_SESSION="desync_scan"
SESSION=""

# Parse arguments to extract session name if provided
ARGS=()
while [[ $# -gt 0 ]]; do
    case $1 in
        -s|--session)
            if [[ -n $2 && $2 != -* ]]; then
                SESSION="$2"
                shift 2
            else
                echo "[ERROR] Session name required after -s/--session"
                exit 1
            fi
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

# Set default session name if not provided
if [[ -z "$SESSION" ]]; then
    SESSION="$DEFAULT_SESSION"
fi

# Check if at least one argument is provided (after removing session args)
if [ ${#ARGS[@]} -eq 0 ]; then
    echo "[ERROR] No mode specified"
    show_help
    exit 1
fi

# Validate mode
MODE="${ARGS[0]}"
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

# Check for existing session with same name
if screen -list | grep -q "$SESSION"; then
    echo "[WARNING] Screen session '$SESSION' already exists."
    echo "Options:"
    echo "  1) Kill existing session and start new one"
    echo "  2) Attach to existing session"
    echo "  3) Cancel and choose different session name"
    read -p "Choose option (1/2/3): " choice
    
    case $choice in
        1)
            echo "[INFO] Killing existing screen session: $SESSION"
            screen -S "$SESSION" -X quit
            sleep 1  # Give it a moment to clean up
            ;;
        2)
            echo "[INFO] Attaching to existing session: $SESSION"
            screen -r "$SESSION"
            exit 0
            ;;
        3)
            echo "[INFO] Cancelled. Use -s option to specify different session name."
            exit 0
            ;;
        *)
            echo "[ERROR] Invalid choice. Exiting."
            exit 1
            ;;
    esac
fi

# Build the command with proper argument escaping
SCRIPT_DIR=$(dirname "$(realpath "$0")")
RUN_SCRIPT="$SCRIPT_DIR/run.sh"

# Check if run.sh exists
if [[ ! -f "$RUN_SCRIPT" ]]; then
    echo "[ERROR] run.sh not found at: $RUN_SCRIPT"
    exit 1
fi

# Create the command array for proper argument handling
CMD_ARRAY=("$RUN_SCRIPT")
for arg in "${ARGS[@]}"; do
    CMD_ARRAY+=("$arg")
done

# Create a temporary script to run in screen
TEMP_SCRIPT=$(mktemp)
cat > "$TEMP_SCRIPT" << 'EOF'
#!/bin/bash
# Execute the command
"$@"
EXIT_CODE=$?

echo ""
echo "=========================================="
if [ $EXIT_CODE -eq 0 ]; then
    echo "[INFO] Command completed successfully"
else
    echo "[ERROR] Command failed with exit code: $EXIT_CODE"
fi
echo "=========================================="
echo ""
echo "Options:"
echo "  Press Enter to exit screen session"
echo "  Press Ctrl-a d to detach (keep session running)"
echo "  Type 'exit' to close session"
echo ""

# Wait for user input
read -p "Action: " action
EOF

chmod +x "$TEMP_SCRIPT"

# Start new screen session
echo "[INFO] Starting screen session '$SESSION'"
echo "[INFO] Command: ${CMD_ARRAY[*]}"
echo ""

# Use the temporary script to wrap our command
screen -dmS "$SESSION" "$TEMP_SCRIPT" "${CMD_ARRAY[@]}"

# Clean up temp script after a delay (in background)
(sleep 5; rm -f "$TEMP_SCRIPT") &

echo "[INFO] Screen session '$SESSION' started successfully!"
echo ""
echo "Screen commands:"
echo "  Attach to session: screen -r $SESSION"
echo "  List all sessions: screen -ls"
echo "  Detach from session: Ctrl-a d (while attached)"
echo "  Kill session: screen -S $SESSION -X quit"
echo ""
echo "Tip: You can attach immediately with: screen -r $SESSION"