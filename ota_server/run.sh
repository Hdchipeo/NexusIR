#!/bin/bash
# Get the directory where this script is located
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

# Activate the virtual environment
if [ -d ".venv" ]; then
    source .venv/bin/activate
else
    echo "Error: Virtual environment (.venv) not found. Please run environment setup first."
    exit 1
fi

# Run the OTA server
echo "[*] Starting NexusIR OTA Server..."
python3 main.py
