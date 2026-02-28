#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

cd "$SCRIPT_DIR/tools/" || exit 1
source sourcefile.txt
cd "$SCRIPT_DIR" || exit 1

make

cd "$SCRIPT_DIR/tools/" || exit 1
openocd -f stlink.cfg -f nrf52.cfg -c "init; halt; program \"$SCRIPT_DIR/build/com.bin\"; reset; exit"
