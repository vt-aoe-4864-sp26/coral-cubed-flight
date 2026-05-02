# Tensor Processing Unit (TPU)

**Path:** `src/main/TPU/`
**Primary File:** `coral_main.cc`

## Primary Functionality
The TPU module runs the Google Coral Edge TPU payload for the satellite. It handles UART-based
command reception (via the TAB protocol), triggers ML inference on the on-board Edge TPU, and
returns detection results back to the satellite bus.

The application supports two hardware targets, selectable at build time:

| Flag     | SDK                        | Board                    |
|----------|----------------------------|--------------------------|
| `--dev`  | `third-party/coralmicro`   | Coral Dev Board Micro    |
| `--pico` | `third-party/coralpico`    | Coral Pico Board         |

Both SDKs expose an identical HAL (FreeRTOS, LittleFS, Edge TPU driver, TFLite Micro), so no
source-code changes are required when switching targets — only the CMake configuration differs.

## Directory Structure

```
src/main/TPU/
├── CMakeLists.txt          # Build config — selects SDK via CORAL_TARGET
├── flashtool.sh            # Build & flash script (requires --dev or --pico)
├── coral_main.cc           # Application entry point (app_main)
├── configs.h               # VAR_CODE definitions and model/image paths
├── README.md               # This file
├── libs/
│   ├── inference.cc/.h     # Edge TPU initialization and model runner
│   ├── uart.cc/.h          # UART task, TAB protocol handler, TX flush
│   └── tab.c/.h            # TAB protocol state machine (shared library)
├── models/
│   └── ssd_mobilenet_v2_face_quant_postprocess_edgetpu.tflite
├── images/
│   ├── denby.rgb           # Test image for inference
│   └── blk.rgb             # Alternate test image for inference
└── out/                    # Build output (gitignored)
```

## Flashing & Building

We use `flashtool.sh` to configure CMake, build the firmware, and flash it to the board.
**You must specify a target board** with either `--dev` or `--pico`.

### Flashtool Usage

```bash
# Build and flash for the Coral Dev Board Micro
./flashtool.sh --dev

# Build and flash for the Coral Pico Board
./flashtool.sh --pico
```

### Optional Flags

| Flag                | Description                                      |
|---------------------|--------------------------------------------------|
| `-nb` / `--no-build`| Skip the CMake configure and make steps           |
| `-nf` / `--no-flash`| Build only — do not flash to hardware             |
| `-nd` / `--no-data` | Flash firmware but skip data files (models/images)|

Flags can be combined:
```bash
# Build-only for pico, no flash
./flashtool.sh --pico -nf

# Flash previously-built firmware, skip data
./flashtool.sh --dev -nb -nd
```

### Steps Executed
1. Validates the mandatory `--dev` / `--pico` target flag
2. Calculates cores and runs `cmake -B out -S . -DCORAL_TARGET=<target>`
3. Performs parallel build with `make -C out -j<JOBS>`
4. Stages dependencies (`elf_loader`, `flashloader`, `mklfs`) into `out/`
5. Invokes the SDK's `flashtool.py` to deploy to the board
