#!/bin/bash
SESSION="desync_scan"

# Kill old session if it exists
if screen -list | grep -q "$SESSION"; then
    echo "[INFO] Killing old screen session: $SESSION"
    screen -S "$SESSION" -X quit
fi

# Start new screen
screen -dmS "$SESSION" bash -c "./run.sh \"$@\"; exec bash"

echo "[INFO] Started screen session '$SESSION'."
echo "Attach with: screen -r $SESSION"
echo "Detach with: Ctrl-a d"
