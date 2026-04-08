# Command and Data Handling (CDH)

**Path:** `src/main/CDH/`
**Primary File:** `cdh_main.c`

## Primary Functionality
The CDH module runs on the Custom STM32 Board (`coral_stm32`) using the Zephyr RTOS. It acts as the central orchestrator for the flight stack, handling the power sequencing for the COM module to prevent USB brownouts and initializing hardware peripherals like UARTs and LEDs. 

In its continuous operation, it expects a handshake from the COM module, runs command/packet routing, and processes command queues (`rx_cmd_queue` and `tx_cmd_buff_t`), communicating over hardware UART.

## Flashing & Building
We use `flashtool.sh` to wrap `west build` and `st-flash` for the STM32 processing unit.

### Flashtool Usage
Run the flashtool from this directory:
```bash
./flashtool.sh [FLAGS]
```

### Flags
- `--no-build` or `-nb` : Skip the Zephyr build step.
- `--no-flash` or `-nf` : Skip the st-flash flashing step.
