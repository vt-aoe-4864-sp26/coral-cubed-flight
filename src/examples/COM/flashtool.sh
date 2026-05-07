#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." >/dev/null 2>&1 && pwd)"

OPENOCD_DIR="$SCRIPT_DIR/tools"

# ─── Argument Parsing ─────────────────────────────────────────────────────────
SKIP_BUILD=false
SKIP_FLASH=false
DO_RESET=false
APP_BLINK=false
APP_GPIO=false
APP_UART=false

for arg in "$@"; do
    case $arg in
        --no-build|-nb) SKIP_BUILD=true ;;
        --no-flash|-nf) SKIP_FLASH=true ;;
        --reset|-rst)   DO_RESET=true ;;
        --blink)        APP_BLINK=true ;;
        --gpio)         APP_GPIO=true ;;
        --uart)         APP_UART=true ;;
    esac
done

# ─── App Selection ────────────────────────────────────────────────────────────
APP_COUNT=0
[ "$APP_BLINK" = true ] && APP_COUNT=$((APP_COUNT + 1))
[ "$APP_GPIO" = true ]  && APP_COUNT=$((APP_COUNT + 1))
[ "$APP_UART" = true ]  && APP_COUNT=$((APP_COUNT + 1))

if [ "$APP_COUNT" -gt 1 ]; then
    echo "ERROR: Only one app flag may be specified (--blink, --gpio, or --uart)."
    exit 1
fi

# Default to blink if no app selected
if [ "$APP_COUNT" -eq 0 ]; then
    APP_BLINK=true
fi

# Resolve source directory
if [ "$APP_BLINK" = true ]; then
    APP_NAME="blink"
elif [ "$APP_GPIO" = true ]; then
    APP_NAME="gpio"
elif [ "$APP_UART" = true ]; then
    APP_NAME="uart"
fi

APP_SRC="src/examples/COM/apps/$APP_NAME"
echo "Selected COM example app: $APP_NAME"

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
    echo "Building COM example ($APP_NAME) with west..."
    west build -d build_com_examples -p always -b coral_nrf52833 "$APP_SRC" \
        -- -DBOARD_ROOT="$PROJECT_ROOT/src/examples/COM" \
        -DCONF_FILE="$PROJECT_ROOT/src/examples/COM/prj.conf" \
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
            program \"$PROJECT_ROOT/build_com_examples/zephyr/zephyr.hex\" verify; \
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