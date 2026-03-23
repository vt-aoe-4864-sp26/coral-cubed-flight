#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." >/dev/null 2>&1 && pwd)"

# Activate Python Virtual Environment
source "$PROJECT_ROOT/coraldev/bin/activate" || { echo -e "Failed to activate venv"; exit 1; }

cd "$PROJECT_ROOT" || exit 1

echo "Building COM with west..."
west build -d build_com -p always -b nrf52833dk/nrf52833 src/main/COM || { echo -e "Build Errors"; exit 1; }

echo "flashing com with openocd..."
cd "$SCRIPT_DIR/tools/" || exit 1

openocd -f interface/stlink.cfg -f target/nrf52.cfg -c "transport select hla_swd; init; reset halt; nrf5 mass_erase; program \"$PROJECT_ROOT/build_com/zephyr/zephyr.hex\" verify reset exit"