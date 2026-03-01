#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

export PATH=$PATH:/home/jackr/repos/coral-cubed-flight/make/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi/bin

make || { echo -e "Build Errors"; exit 1; }

st-flash write build/cdh.bin 0x8000000