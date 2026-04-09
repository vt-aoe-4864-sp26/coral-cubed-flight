# PlatformIO Setup for the CDH Flash Demo

This replaces the west/WSL workspace flow with a **PlatformIO-only** flow.

## Files provided

- `CDH_flash_demo_platformio/` → the demo project
- `PLATFORMIO_CDH_FLASH_DEMO_SETUP.md` → this guide

## Where to put the demo project

Place the folder here in your repo:

```text
src/main/CDH_flash_demo_platformio
```

Keep your original app here:

```text
src/main/CDH
```

## Open the correct folder in VS Code

Open **this project folder itself** in VS Code:

```text
src/main/CDH_flash_demo_platformio
```

Do **not** open the old `CDH` app if you want PlatformIO to detect the new demo project.

## What PlatformIO needs

This project includes:

- `platformio.ini`
- a local custom board manifest in `boards/coral_stm32.json`
- Zephyr project files in `zephyr/`
- the standalone demo app in `src/main.c`

PlatformIO’s Zephyr docs require this split layout with `platformio.ini` at the root and `zephyr/CMakeLists.txt` + `zephyr/prj.conf` in a separate `zephyr/` folder. citeturn286419view0

## Build and upload in VS Code

1. Open `src/main/CDH_flash_demo_platformio` in VS Code.
2. Wait for PlatformIO to finish indexing the project.
3. In the PlatformIO sidebar, open **Project Tasks**.
4. Under `coral_stm32_flash_demo`, click **Build**.
5. Connect the ST-LINK to the board.
6. Click **Upload**.

For ST-LINK uploads in PlatformIO, the relevant project settings are `debug_tool = stlink` and `upload_protocol = stlink`. citeturn286419view1

## What this demo does

- initializes the USB console
- initializes the external QSPI flash
- checks JEDEC ID `9D 60 18`
- erases one test sector
- writes `0xDEADBEEF`
- reads it back
- prints the result
- blinks LED1 forever

## Important note

This rebuild avoids:

- `west init`
- `west update`
- WSL USB passthrough
- broken repo submodule recursion

because your current goal is to use the **PlatformIO VS Code extension**, not a restored Zephyr workspace.
