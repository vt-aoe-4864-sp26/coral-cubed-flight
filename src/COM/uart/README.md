# COM UART Example

A UART TAB example for the COM board

Compile the program:

```bash
cd ../scripts/
source sourcefile.txt
cd ../uart/
make
```

Run openocd in one terminal:

```bash
cd ../scripts/
openocd -f stlink.cfg -f nrf52.cfg
```

Use telnet to program the MCU in a second terminal:

```bash
telnet 127.0.0.1 4444
init
halt
program path/to/uart.bin
reset
```
