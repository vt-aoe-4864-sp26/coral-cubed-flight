# Communications Software

## Directory Structure

### Primary App

[com_main.c](com_main.c) : Main application to flash to the COM board

### Supporting Apps

* [cdh.c](libs/com.c) : Board Support Implementation
* [cdh.h](libs/com.h) : Board Support Header

* [tab.c](libs/tab.c) : Communications Protocol Implementation
* [tab.h](libs/tab.h) : Communications Protocol Header

### Flashtool

A flashtool has been included for your convenience which will build the apps and flash it to the CDH board via SWD

Via Native x64 Linux:

```bash
chmod +x setup.sh
./flashtool.sh
```

Via WSL (after mounting the ST-Link SWD to your WSL)

```bash
chmod +x setup.sh
dos2unix setup.sh
./flashtool.sh
```
