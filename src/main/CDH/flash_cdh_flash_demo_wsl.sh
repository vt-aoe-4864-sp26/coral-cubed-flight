#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." >/dev/null 2>&1 && pwd)"

SKIP_BUILD=false
SKIP_FLASH=false

for arg in "$@"; do
    case $arg in
        --no-build|-nb) SKIP_BUILD=true ;;
        --no-flash|-nf) SKIP_FLASH=true ;;
    esac
done

source "$PROJECT_ROOT/coraldev/bin/activate" || { echo "Failed to activate venv"; exit 1; }
cd "$PROJECT_ROOT" || exit 1

if [ "$SKIP_BUILD" = "false" ]; then
    echo "Building CDH flash demo with west..."
    west build -d build_cdh_flash_demo -p always -b coral_stm32 "$PROJECT_ROOT/src/main/CDH_flash_demo" \
        -- -DBOARD_ROOT="$PROJECT_ROOT/src/main/CDH" \
        || { echo "Build failed"; exit 1; }
else
    echo "Skipping build."
fi

if [ "$SKIP_FLASH" = "false" ]; then
    echo "Flashing CDH flash demo with st-flash..."
    st-flash --reset write "$PROJECT_ROOT/build_cdh_flash_demo/zephyr/zephyr.bin" 0x08000000

    if [ $? -ne 0 ]; then
        echo "Flash failed."
        exit 1
    else
        echo "Flash successful."
    fi
else
    echo "Skipping flash."
fi
