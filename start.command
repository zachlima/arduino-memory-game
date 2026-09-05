#!/bin/bash
# Starts the leaderboard server and the serial listener together, then opens
# the leaderboard in a browser. Press Ctrl+C to stop both at once.
#
# Setup (only needs to be done once):
#   chmod +x start.command
#   pip3 install pyserial requests
#
# Run:
#   ./start.command   (or just double-click it in Finder)

# Move into the folder this script lives in, so it works no matter
# where you run it from.
cd "$(dirname "$0")"

SERVER_PID=""
LISTENER_PID=""

# set before launching anything so an early Ctrl+C still cleans up
trap 'echo ""; echo "Stopping server and listener..."; kill $SERVER_PID $LISTENER_PID 2>/dev/null' EXIT

echo "Starting leaderboard server..."
node server.js &
SERVER_PID=$!

# Give the server a moment to boot before opening the page
sleep 1

echo "Opening leaderboard in browser..."
open http://localhost:3000

echo "Starting serial listener..."
python3 -u listener.py &
LISTENER_PID=$!

wait
