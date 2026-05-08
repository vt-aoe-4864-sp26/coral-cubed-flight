#!/bin/bash

# Navigate to the directory of this script
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
REPO_ROOT="$( cd "$DIR/../../../.." >/dev/null 2>&1 && pwd )"

cd "$DIR"

# Check if virtual environment exists
if [ -f "$REPO_ROOT/coraldev/bin/activate" ]; then
    echo "Activating virtual environment..."
    source "$REPO_ROOT/coraldev/bin/activate"
else
    echo "Warning: Virtual environment not found at $REPO_ROOT/coraldev"
fi

echo "Starting Flatsat Operations API in background..."
# Start API with reload for development convenience
uvicorn api:app --reload --host 127.0.0.1 --port 8000 &
API_PID=$!

# Ensure the API is killed when this script exits
cleanup() {
    echo ""
    echo "Shutting down API (PID: $API_PID)..."
    kill $API_PID 2>/dev/null
}
trap cleanup EXIT

# Give the API a second to spin up
sleep 2

echo "Starting Control GUI..."
python3 gui.py

# The script will wait here until gui.py is closed.
# Once closed, the trap 'cleanup' will run.
