#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

echo "=== Calculating Build Jobs ==="
TOTAL_CORES=$(nproc)
JOBS=$(( (TOTAL_CORES * 3 + 3) / 4 ))

echo "Detected $TOTAL_CORES cores. Running Make with -j$JOBS"
echo ""

echo "=== Configuring CMake ==="
cmake -B out -S .
echo ""

echo "=== Building Project ==="
make -C out -j"$JOBS"
echo ""

echo "=== Staging Flashtool Dependencies ==="
# flashtool.py evaluates filesystem paths relative to --build_dir.
# Since CMake generated paths relative to the project root (.), 
# we must run flashtool with '--build_dir .' to prevent a path offset error.
# We temporarily copy the required flashing tools here to satisfy the script.
find out/coralmicro_build -type f -name "elf_loader" -exec cp {} . \;
find out/coralmicro_build -type f -name "flashloader" -exec cp {} . \; 2>/dev/null || true
find out/coralmicro_build -type f -name "mklfs" -exec cp {} . \;

echo "=== Flashing to Board ==="
python3 ../../../third-party/coralmicro/scripts/flashtool.py \
    --build_dir . \
    --elf_path out/coralmicro-app

echo "=== Cleaning Up ==="
# Remove the staged binaries and the generated filesystem binary 
# to keep your source tree clean.
rm -f elf_loader flashloader mklfs __lfs.bin

echo ""
echo "=== Success! ==="