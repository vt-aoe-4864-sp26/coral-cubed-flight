# CDH Flash Demo

This is a separate Zephyr application intended for a standalone CDH flash-memory demo.

## What it does
- boots the `coral_stm32` board
- brings up the USB console
- initializes the external IS25LP128F QSPI flash
- erases one reserved 4 KB test sector
- writes `0xDEADBEEF`
- reads the value back and prints it
- blinks LED1 forever

## Intended location
Copy this folder into:

`src/main/CDH_flash_demo`

This demo assumes your original out-of-tree board files stay in:

`src/main/CDH`

## Notes
- The demo does **not** power on COM or PAY.
- The demo does **not** wait for any COM handshake.
- The flash test uses offset `0x00100000` in the external QSPI flash as a reserved scratch area.
