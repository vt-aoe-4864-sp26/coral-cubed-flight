# CDH Examples

**Path:** `src/examples/CDH/`

## Directory Structure

```
src/examples/CDH/
├── apps/                  ← Example applications (one is built at a time)
│   ├── blink/
│   │   ├── blink.c        ← Alternates LED1 and LED2
│   │   └── CMakeLists.txt
│   ├── gpio/
│   │   ├── gpio.c         ← Enables COM and PAY power rails
│   │   └── CMakeLists.txt
│   └── uart/
│       ├── uart.c         ← Responds to TAB packets via USB-C console
│       └── CMakeLists.txt
├── libs/                  ← Shared board support library (cdh.c, cdh.h, tab.c, tab.h)
├── boards/                ← Custom board definitions (coral_stm32)
├── tools/                 ← Tooling and config files
├── prj.conf               ← Shared Zephyr Kconfig (all apps use this)
├── flashtool.sh           ← Build & flash script with app selection
└── README.md              ← This file
```

## Example Applications

### Blink (`--blink`, default)
Toggles LED1 and LED2 in an alternating pattern at 500ms intervals. No peripherals initialized beyond LEDs. Good for verifying the board boots and GPIOs work.

### GPIO (`--gpio`)
Initializes the CDH GPIO subsystem and enables the COM and PAY power rails using `power_on_com()` and `power_on_pay()`. Outputs status via USB console (`printk`). LEDs blink as a heartbeat.

### UART (`--uart`)
Full USB-C console example. Boots the USB CDC device, enables interrupt-driven UART RX, and processes incoming TAB-protocol packets through a dedicated command processor thread. Responds to commands and prints diagnostics to the console. Also initializes the hardware UARTs for board-to-board communication.

## Building & Flashing

Use `flashtool.sh` to build and flash examples to the CDH board (STM32 via `st-flash`).

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
| Flag              | Short | Description                      |
|-------------------|-------|----------------------------------|
| `--no-build`      | `-nb` | Skip the west build step         |
| `--no-flash`      | `-nf` | Skip the st-flash flashing step  |

### Examples
```bash
# Build and flash the default blink example
./flashtool.sh

# Build the GPIO example without flashing (build validation only)
./flashtool.sh --gpio -nf

# Build and flash the UART example
./flashtool.sh --uart

# Flash a previously built example (skip rebuild)
./flashtool.sh --blink -nb
```

## How It Works

Each app under `apps/` has its own `CMakeLists.txt` that:
1. Declares the app source file (e.g., `blink.c`)
2. Links the shared `libs/` sources (`cdh.c`, `tab.c`) via relative paths
3. Adds the `libs/` directory to the include path

The shared `prj.conf` and `boards/` overlay at the top level are automatically picked up by Zephyr's build system for all apps.
