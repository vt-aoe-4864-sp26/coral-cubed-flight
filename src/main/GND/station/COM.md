# Coral Cubed COM Stack

## Overview
This repository contains the Zephyr RTOS-based firmware for the nRF52833 Communications (COM) board. The architecture is designed to be hardware-agnostic regarding its physical placement: the exact same codebase operates as both the Ground Station USB transceiver and the Satellite flight computer bridge, differentiated only by a compile-time role toggle.

The COM board uses the custom TAB serial protocol (derived from openLST) to package commands. It bridges physical UART connections and an IEEE 802.15.4 wireless link using Zephyr's raw DGRAM sockets, allowing for hardware-filtered addressing and automatic CSMA/CA RF collision avoidance.

## Expected Behavior
The board operates three primary threads:
1.  **Command Processor (`cmd_processor_tid`)**: Listens to hardware UARTs (USB-C or the CDH payload line). When a complete TAB frame is parsed, it evaluates the `dest_id`. If the destination is local, it executes the command. If the destination requires a hop (e.g., Ground to Satellite), it pushes the frame to the Radio TX queue.
2.  **Radio TX Thread (`radio_tid`)**: Monitors the outgoing transmission queue. It handles pushing raw TAB frames into the 802.15.4 MAC layer. It includes an automatic retry state-machine that escalates the nRF52833's TX power (-12dBm up to +8dBm) if an ACK is not received within 250ms.
3.  **Radio RX Thread (`radio_rx_tid`)**: A blocking listener on the 802.15.4 socket. Because the hardware is configured with strict PAN ID and Short Address filtering, this thread only wakes up when a packet specifically addressed to this board arrives. It parses the packet, clears pending ACKs, and passes new commands to the Command Processor.

## Available openLST / TAB Commands
Commands are dispatched using the `COMMON_DATA_OPCODE` (0x16). The payload's first byte indicates the specific Variable Code (`VAR_CODE`).

### System Diagnostics
* `0x00` **Alive**: Triggers a local LED state to confirm the board is receiving commands.
* `0x07` **Blink COM**: Executes a 5-second LED toggle sequence on the local COM board.
* `0x06` **Blink CDH**: Routes an LED toggle command specifically to the CDH target.
* `0x0B` **Run Demo**: Triggers a pre-programmed RF and Payload timing sequence (simulated pass/operations).

### RF Hardware Controls
* `0x03` **RF Enable**: Toggles the Front-End Module (FEM) Power Down pin.
* `0x04` **RF TX**: Manually forces the FEM into TX mode.
* `0x05` **RF RX**: Manually forces the FEM into RX mode.

### CDH & Payload Operations (Coral Micro / NXP RT1176)
* `0x02` **Payload Enable**: Commands the CDH board to enable/disable power to the payload PCB.
* `0x08` **Coral Wake**: Signals the NXP MCU to wake the Google Coral Edge TPU.
* `0x09` **Coral Cam On**: Initializes the payload image sensor.
* `0x0A` **Coral Infer**: Triggers the neural network inference loop on the TPU.

### Bootloader Commands
Standard openLST bootloader opcodes are supported for remote firmware updates:
* `0x00` Ping
* `0x0c` Erase Flash
* `0x02` Write Page
* `0x20` Write Page (32-bit Address)
* `0x0b` Jump to Application