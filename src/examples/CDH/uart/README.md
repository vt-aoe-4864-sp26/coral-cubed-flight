# CDH UART Example

A UART TAB example for the CDH board.

Usage: Execute the following `bash` commands:

```bash
cd ../scripts/
source sourcefile.txt
cd ../uart/
make
st-flash write uart.bin 0x08000000
```
