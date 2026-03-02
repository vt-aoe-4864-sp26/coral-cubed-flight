#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

cd "$SCRIPT_DIR/tools/" || exit 1
source sourcefile.txt
cd "$SCRIPT_DIR" || exit 1

make || { echo -e "Build Errors"; exit 1; }

st-flash write build/cdh.bin 0x8000000