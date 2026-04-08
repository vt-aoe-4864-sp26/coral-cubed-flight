# Ground Station (GND)

**Path:** `src/main/GND/station/`
**Primary File:** `gnd_main.c`

## Primary Functionality
The Ground Station application runs on an nRF52833 board (`coral_nrf52833`) using the Zephyr RTOS. It acts as the Host-to-Air bridge, intercepting packets from a connected PC (via USB UART interface) and routing them out over Radio to the satellite/COM interface.

It safely manages USB interruptions and only enumerates USB when properly detected via UART line properties (`UART_LINE_CTRL_DTR`).

## Flashing & Building
We use `flashtool.sh` to compile with `west build` alongside OpenOCD matching the configuration of the COM nRF52 module.

### Flashtool Usage
Run the flashtool from this directory:
```bash
./flashtool.sh [FLAGS]
```

### Flags
- `--no-build` or `-nb` : Skip the Zephyr build step.
- `--no-flash` or `-nf` : Skip the OpenOCD flashing/deploy step.
- `--reset` or `-rst` : Recovers the nRF52 utilizing an SWD ST-Link OpenOCD interface and exits.
