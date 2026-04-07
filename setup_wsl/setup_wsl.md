# Windows translation of `setup.sh`

This is a **step-for-step PowerShell translation** of the original `setup.sh`, keeping the same section order and behavior wherever Windows allows it.

## What stayed the same

The PowerShell script preserves the original flow:

1. start in the script directory
2. install host dependencies / flashing tools
3. create and activate `coraldev`
4. initialize submodules
5. download the ARM toolchain into `make/`
6. initialize/update the west workspace
7. install the Zephyr SDK
8. overwrite `third-party/coralmicro/scripts/requirements.txt`
9. run the Coral Micro setup/build scripts

## Where it cannot be exact

These are the places where a literal 1:1 Windows translation is not possible:

### 1) `apt-get install ...`
The original script installs Ubuntu packages. Native Windows does not have exact `apt` equivalents for several of them.

The PowerShell script installs the closest Windows-native essentials with `winget`:
- Python
- Git
- CMake
- Ninja
- 7-Zip

It only warns about the rest:
- `dos2unix`
- `gperf`
- `ccache`
- `dfu-util`
- `device-tree-compiler`
- `gcc`, `g++`, `gcc-multilib`, `g++-multilib`
- `libsdl2-dev`
- `libmagic1`
- `stlink-tools`
- `openocd`

### 2) virtualenv activation path
Linux:
- `source coraldev/bin/activate`

Windows PowerShell:
- `coraldev\Scripts\Activate.ps1`

### 3) ARM toolchain package format
Linux original:
- downloads `arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz`

Windows translation:
- downloads `arm-gnu-toolchain-14.2.rel1-mingw-w64-i686-arm-none-eabi.zip`

### 4) Zephyr SDK install command
Linux original:
- `west sdk install`

Windows translation:
- downloads the Windows Zephyr SDK `.7z`
- extracts it
- runs `setup.cmd`

That is the Windows-native Zephyr SDK install path.

### 5) Coral Micro build scripts
The original script runs:
- `bash setup.sh`
- `bash build.sh`

That is **not** a native PowerShell command sequence.

So this PowerShell translation can only keep that behavior if `bash` is available on PATH, for example via Git for Windows.

If `bash` is not available, the script stops and tells you exactly why.
