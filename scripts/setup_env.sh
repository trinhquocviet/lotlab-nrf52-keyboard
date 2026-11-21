#!/usr/bin/env bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"

echo "Setting up Python virtual environment with 'uv'..."

# Check if uv is installed
if ! command -v uv &> /dev/null; then
    echo "Error: 'uv' is not installed."
    echo "Please install it first: curl -LsSf https://astral.sh/uv/install.sh | sh"
    exit 1
fi

# Create virtual environment if it doesn't exist
if [ ! -d "$VENV_DIR" ]; then
    echo "Creating virtual environment at $VENV_DIR..."
    uv venv "$VENV_DIR"
fi

# Install dependencies
echo "Installing dependencies..."
uv pip install -r "$SCRIPT_DIR/requirements.txt"

echo ""
echo "✅ Setup complete!"
echo "To use the scripts in the virtual environment, you can:"
echo "1. Activate it manually: source $VENV_DIR/bin/activate"
echo "2. OR just use 'uv run':"
echo "   uv run scripts/enter_isp.py"
echo "   uv run scripts/enter_dfu.py"
