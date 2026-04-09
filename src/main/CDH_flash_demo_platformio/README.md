# CDH Flash Demo (PlatformIO)

This is a **separate PlatformIO Zephyr project** for a standalone CDH flash-memory demo.

It is designed to keep the original `src/main/CDH` app intact.

## What it does

- boots the board
- brings up the USB console
- initializes the external `is25lp128` QSPI flash
- checks the JEDEC ID
- erases one 4 KB test sector
- writes `0xDEADBEEF`
- reads it back and prints the result
- blinks LED1 forever

## Where to place it

Put this folder at:

```text
src/main/CDH_flash_demo_platformio
```

It assumes your original board files stay in:

```text
src/main/CDH
```

and uses them through:

```ini
-DBOARD_ROOT="${PROJECT_DIR}/../CDH"
```

## Project structure

```text
CDH_flash_demo_platformio/
├── boards/
│   └── coral_stm32.json
├── src/
│   └── main.c
├── zephyr/
│   ├── CMakeLists.txt
│   └── prj.conf
├── platformio.ini
└── README.md
```

## How to use in VS Code

1. Open this folder itself in VS Code.
2. Let the PlatformIO extension detect the project.
3. In the PlatformIO sidebar, use the `coral_stm32_flash_demo` environment.
4. Click **Build**.
5. Connect the board through ST-LINK.
6. Click **Upload**.
7. Open a serial monitor on the USB CDC port to see the flash test output.

## Notes

- This is a best-effort PlatformIO rebuild based on your existing custom Zephyr board setup.
- The project expects the original `coral_stm32` board definition and DTS files to remain under `src/main/CDH`.
- If PlatformIO reports a custom-board issue, the first thing to check is that the original board directory still exists under `../CDH/boards`.
