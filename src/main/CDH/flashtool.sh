#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." >/dev/null 2>&1 && pwd)"

# Activate Python Virtual Environment
source "$PROJECT_ROOT/coraldev/bin/activate" || { echo -e "Failed to activate venv"; exit 1; }

cd "$PROJECT_ROOT" || exit 1

echo "Building CDH with west..."
west build -d build_cdh -p always -b stm32l496g_disco src/main/CDH || { echo -e "Build Errors"; exit 1; }

echo "Flashing CDH with st-flash..."
st-flash write build_cdh/zephyr/zephyr.bin 0x8000000