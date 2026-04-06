#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." >/dev/null 2>&1 && pwd)"

OPENOCD_DIR="$SCRIPT_DIR/tools"

# ─── Argument Parsing ─────────────────────────────────────────────────────────
SKIP_BUILD=false
SKIP_FLASH=false
DO_RESET=false

for arg in "$@"; do
    case $arg in
        --no-build|-nb) SKIP_BUILD=true ;;
        --no-flash|-nf) SKIP_FLASH=true ;;
        --reset|-rst)   DO_RESET=true ;;
    esac
done

# ─── Activate venv ────────────────────────────────────────────────────────────
source "$PROJECT_ROOT/coraldev/bin/activate" || { echo "Failed to activate venv"; exit 1; }
cd "$PROJECT_ROOT" || exit 1

# ─── Reset ────────────────────────────────────────────────────────────────────
if [ "$DO_RESET" = true ]; then
    echo "Resetting/Recovering nRF52 with OpenOCD..."
    cd "$OPENOCD_DIR" || exit 1
    
    openocd -f stlink.cfg -f nrf52.cfg -c "init; nrf52_recover; exit" || { echo "Reset failed"; exit 1; }
    
    # Return to project root for subsequent steps
    cd "$PROJECT_ROOT" || exit 1 
fi

# ─── Build ────────────────────────────────────────────────────────────────────
if [ "$SKIP_BUILD" = false ]; then
    echo "BuildingGND with west..."
    west build -d build_gnd -p always -b coral_nrf52833 src/main/GND/station \
        -- -DBOARD_ROOT="$PROJECT_ROOT/src/main/GND/station" \
        || { echo "Build failed"; exit 1; }
else
    echo "Skipping build."
fi

# ─── Flash ────────────────────────────────────────────────────────────────────
if [ "$SKIP_FLASH" = false ]; then
    echo "Flashing with OpenOCD..."
    cd "$OPENOCD_DIR" || exit 1

    FLASH_LOG=$(openocd \
        -f interface/stlink.cfg \
        -f target/nrf52.cfg \
        -c "transport select hla_swd; \
            init; \
            reset halt; \
            nrf5 mass_erase; \
            program \"$PROJECT_ROOT/build_gnd/zephyr/zephyr.hex\" verify; \
            resume; \
            exit" 2>&1)

    echo "$FLASH_LOG"

    if ! echo "$FLASH_LOG" | grep -q "Verified OK"; then
        echo "Flash failed — verify did not pass"
        exit 1
    fi

    echo "Flash successful."
else
    echo "Skipping flash."
fi