# Communications Software

## Directory Structure

### Primary App

* [com_main.c](com_main.c) : Main application to flash to the COM board.

### Supporting Libraries

* [com.c](libs/com.c) / [com.h](libs/com.h) : High-level board application, routing logic, and command handlers.
* [radio.c](libs/radio.c) / [radio.h](libs/radio.h) : nRF52833 Radio FEM controls, retry logic, and IEEE 802.15.4 stack interface.
* [uart.c](libs/uart.c) / [uart.h](libs/uart.h) : Hardware UART pipelines and interrupt handling.
* [tab.c](libs/tab.c) / [tab.h](libs/tab.h) : TAB Serial Communications Protocol implementation.

## Flashtool (`flashtool.sh`)

A flashtool script has been included for your convenience. It automatically activates the `coraldev` Python virtual environment, builds the application using `west` for the `coral_nrf52833` board, and flashes it via an ST-Link SWD using OpenOCD.

**Basic Usage (Native x64 Linux):**

```bash
chmod +x flashtool.sh
./flashtool.sh
```

**Via WSL (after mounting the ST-Link SWD to your WSL):**

```
chmod +x flashtool.sh
dos2unix flashtool.sh
./flashtool.sh
```

**Script Arguments**
ou can modify the behavior of the flashtool using the following arguments:

`--no-build` or `-nb` : Skips the west build step. Useful if you just want to re-flash an existing build.

`--no-flash` or `-nf` : Skips the OpenOCD flashing step. Useful if you just want to verify the code compiles.

`--reset` or `-rst` : Performs an nrf52_recover sequence before proceeding.

## Troubleshooting

Did your board decide to kill itself or stop responding after a bad flash? Don't panic. Nuke the bootloader and recover the nRF52 by using the reset flag:

```bash
./flashtool.sh --reset
```

Alternatively, you can run the OpenOCD recovery command manually from within the `tools` directory:

```bash
openocd -f stlink.cfg -f nrf52.cfg -c "init; nrf52_recover; exit"
```
