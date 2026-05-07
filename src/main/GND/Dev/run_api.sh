#!/bin/bash

# Navigate to the script's directory so uvicorn can find api.py
cd "$(dirname "$0")" || exit

echo "Starting the Flatsat Operations API..."
echo "API Docs will be available at: http://127.0.0.1:8000/docs"

# Launch the backend
uvicorn api:app --reload --host 127.0.0.1 --port 8000
