# COM Examples

**Path:** `src/examples/COM/`

## Directory Structure

```
src/examples/COM/
├── apps/                  ← Example applications (one is built at a time)
│   ├── blink/
│   │   ├── blink.c        ← Alternates LED1 and LED2
│   │   └── CMakeLists.txt
│   ├── gpio/
│   │   ├── gpio.c         ← Controls RF frontend, cycles TX/RX modes
│   │   └── CMakeLists.txt
│   └── uart/
│       ├── uart.c         ← Responds to TAB packets via USB-C console
│       └── CMakeLists.txt
├── libs/                  ← Shared board support library (com.c, com.h, radio.c, radio.h, uart.c, uart.h, tab.c, tab.h)
├── boards/                ← Custom board definitions (coral_nrf52833)
├── tools/                 ← OpenOCD configs and tooling
├── prj.conf               ← Shared Zephyr Kconfig (all apps use this)
├── flashtool.sh           ← Build & flash script with app selection
└── README.md              ← This file
```

## Example Applications

### Blink (`--blink`, default)
Toggles LED1 and LED2 in an alternating pattern at 500ms intervals. Radio and UART subsystems are not initialized in the main loop. Good for verifying the board boots and GPIOs work.

### GPIO (`--gpio`)
Demonstrates control of the COM board's RF Front End Module (FEM). Powers on the RF frontend via `enable_rf()`, then enters a continuous cycle between TX and RX modes using `enable_tx()` / `enable_rx()` with proper guard-band transitions. LED1 indicates TX mode, LED2 indicates RX mode. Status is output via `printk`.

### UART (`--uart`)
Full USB-C console example. Boots the USB CDC device with DTR detection, enables interrupt-driven UART RX, and processes incoming TAB-protocol packets through a dedicated command processor thread. Also initializes the radio subsystem and hardware UART connection to CDH.

## Building & Flashing

Use `flashtool.sh` to build and flash examples to the COM board (nRF52833 via OpenOCD/ST-Link).

### Usage
```bash
./flashtool.sh [APP_FLAG] [OPTIONS]
```

### App Flags (choose one)
| Flag       | Description                         |
|------------|-------------------------------------|
| `--blink`  | Build the blink example (default)   |
| `--gpio`   | Build the GPIO example              |
| `--uart`   | Build the UART example              |

> **Note:** If no app flag is specified, `--blink` is used by default. Specifying more than one app flag will produce an error.

### Options
| Flag              | Short | Description                                              |
|-------------------|-------|----------------------------------------------------------|
| `--no-build`      | `-nb` | Skip the west build step                                 |
| `--no-flash`      | `-nf` | Skip the OpenOCD flashing step                           |
| `--reset`         | `-rst`| Recover a locked/bricked nRF52 via `nrf52_recover`       |

### Examples
```bash
# Build and flash the default blink example
./flashtool.sh

# Build the GPIO example without flashing (build validation only)
./flashtool.sh --gpio -nf

# Build and flash the UART example
./flashtool.sh --uart

# Recover a bricked board, then build and flash blink
./flashtool.sh --blink --reset

# Flash a previously built example (skip rebuild)
./flashtool.sh --uart -nb
```

## How It Works

Each app under `apps/` has its own `CMakeLists.txt` that:
1. Declares the app source file (e.g., `blink.c`)
2. Links the shared `libs/` sources (`com.c`, `tab.c`, `uart.c`, `radio.c`) via relative paths
3. Adds the `libs/` directory to the include path

The shared `prj.conf` and `boards/` overlay at the top level are automatically picked up by Zephyr's build system for all apps.

> **Note:** All COM examples link `radio.c` because `com.c` depends on radio functions (`enable_rf()`, etc.). In the blink example, the radio threads will start but idle harmlessly since no sockets are opened from main.
