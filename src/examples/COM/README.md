# Communication Module (COM)

**Path:** `src/main/COM/`
**Primary File:** `com_main.c`

## Primary Functionality
The COM module runs on the customized nRF52833 board (`coral_nrf52833`) utilizing the Zephyr RTOS. It serves as the primary router and communication bridge between the ground station and the CDH board.

It boots its hardware connections to CDH via UART, initializes its internal radio subsystem, and routes command packets back and forth between the local interfaces (`rx_cmd_queue` and `tx_cmd_buff_t`). It detects UART DTR to verify USB activity securely.

## Flashing & Building
We use `flashtool.sh` to wrapper `west build` alongside OpenOCD (`openocd`) targeted to perform SWD connection over ST-Link.

### Flashtool Usage
Run the flashtool from this directory:
```bash
./flashtool.sh [FLAGS]
```

### Flags
- `--no-build` or `-nb` : Skip the Zephyr build step.
- `--no-flash` or `-nf` : Skip the openocd flashing step.
- `--reset` or `-rst` : Connects to OpenOCD and issues `nrf52_recover` to recover a locked/bricked nRF52 board. Skips building and flashing following a reset unless explicitly configured otherwise.
