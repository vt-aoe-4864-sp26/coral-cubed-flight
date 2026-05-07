#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." >/dev/null 2>&1 && pwd)"

# ─── Argument Parsing ─────────────────────────────────────────────────────────
SKIP_BUILD=false
SKIP_FLASH=false
APP_BLINK=false
APP_GPIO=false
APP_UART=false

for arg in "$@"; do
    case $arg in
        --no-build|-nb) SKIP_BUILD=true ;;
        --no-flash|-nf) SKIP_FLASH=true ;;
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

APP_SRC="src/examples/CDH/apps/$APP_NAME"
echo "Selected CDH example app: $APP_NAME"

# ─── Activate venv ────────────────────────────────────────────────────────────
source "$PROJECT_ROOT/coraldev/bin/activate" || { echo "Failed to activate venv"; exit 1; }
cd "$PROJECT_ROOT" || exit 1

# ─── Build ────────────────────────────────────────────────────────────────────
if [ "$SKIP_BUILD" = "false" ]; then
    echo "Building CDH example ($APP_NAME) with west..."
    west build -d build_cdh_examples -p always -b coral_stm32 "$APP_SRC" \
        -- -DBOARD_ROOT="$PROJECT_ROOT/src/examples/CDH" \
        -DCONF_FILE="$PROJECT_ROOT/src/examples/CDH/prj.conf" \
        || { echo "Build failed"; exit 1; }
else
    echo "Skipping build."
fi

# ─── Flash ────────────────────────────────────────────────────────────────────
if [ "$SKIP_FLASH" = "false" ]; then
    echo "Flashing CDH with st-flash..."
    
    st-flash --reset write "$PROJECT_ROOT/build_cdh_examples/zephyr/zephyr.bin" 0x08000000

    if [ $? -ne 0 ]; then
        echo "Flash failed."
        exit 1
    else
        echo "Flash successful." 
    fi
else
    echo "Skipping flash."
fi