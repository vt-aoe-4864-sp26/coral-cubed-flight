# Tensor Processing Unit (TPU)

**Path:** `src/main/TPU/`
**Primary File:** `demo.cc`

## Primary Functionality
The TPU leverages the Google Coral Dev Board Micro hardware. The current application (`demo.cc`) serves as a test integration of FreeRTOS tasks alongside Google Coral libraries. 

It reads serial characters sent over its console (`ConsoleM7`) to execute commands (e.g., turning a User LED on and off by passing `'1'` and `'0'`). It demonstrates basic IO mapping and task yielding functionality to maintain responsiveness.

## Flashing & Building
We use `flashtool.sh` to configure CMake and build the TPU firmware directly into the `.elf` format intended for the coralmicro scripts.

### Flashtool Usage
Run the script to cleanly provision, build, and flash the target:
```bash
./flashtool.sh
```

### Steps Executed
Unlike standard Makefile wrappers, the TPU flashtool operates in sequence:
1. Calculates cores and initiates `cmake -B out -S .` to generate Unix Makefiles
2. Performs parallel build with `make -C out -j<JOBS>`
3. Stages dependencies (e.g., `elf_loader`, `flashloader`, and `mklfs`) into the `out/` branch
4. Executes the upstream python deployer: `flashtool.py --build_dir out --elf_path out/coralmicro-app --nodata`
