# COM GPIO Program

COM board GPIO program

Compile the program:

```bash
cd ../scripts/
source sourcefile.txt
cd ../gpio/
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
program path/to/gpio.bin
reset
```
## How to enable Rx and Tx on the COM Board
