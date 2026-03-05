#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

echo "=== Calculating Build Jobs ==="
TOTAL_CORES=$(nproc)
JOBS=$(( (TOTAL_CORES * 3 + 3) / 4 ))
echo "Running Make with -j$JOBS"

echo "=== Configuring CMake ==="
cmake -B out -S .

echo "=== Building Project ==="
make -C out -j"$JOBS"

echo "=== Staging Flashtool Dependencies ==="
# We MUST point flashtool to 'out' so it can read CMake's target data
# and properly build the LittleFS filesystem. We copy the bootloader 
# tools into 'out' so the Python script can find them.
find out/coralmicro_build -type f -name "elf_loader" -exec cp {} out/ \;
find out/coralmicro_build -type f -name "flashloader" -exec cp {} out/ \; 2>/dev/null || true
find out/coralmicro_build -type f -name "mklfs" -exec cp {} out/ \;

echo "=== Flashing to Board ==="
python3 ../../../third-party/coralmicro/scripts/flashtool.py \
    --build_dir out \
    --elf_path out/coralmicro-app --nodata

echo ""
echo "=== Success! ==="