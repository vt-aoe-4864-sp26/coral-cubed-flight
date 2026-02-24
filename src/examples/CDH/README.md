# CDH Software

Command and data handling (CDH) board software.

**Dependencies**

* A cross-compiler for the CDH micocontroller unit (MCU) (i.e., AArch32
  bare-metal target)
* A library for abstracting the MCU registers (i.e., libopencm3)
* Tools for transferring compiled programs from the host computer to the MCU
  (i.e., stlink tools)

**Setup dependencies**

Bash commands assume the working directory (`pwd`) is
`/path/to/vt-cdh-sp25/software/`.

```bash
# Download the cross-compiler appropriate for your system
# See https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

## If your computer is x86_64 Linux:
wget https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
tar -xf arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
rm arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz

# Download the abstraction library
git clone https://github.com/libopencm3/libopencm3.git
cd ./scripts/
source sourcefile.txt
cd ../libopencm3/
make

# Install stlink tools
sudo apt install stlink-tools
```

## Quickstart with Blink

The `blink` directory contains a [README](blink/README.md) for compiling and
loading a "blink" example onto the CDH board.

## Directory Contents

* [blink](blink/README.md): Example program for blinking LEDs
* [documentation](documentation/README.md): Useful documentation
* [gpio](gpio/README.md): Example program for toggling GPIOs
* [scripts](scripts/README.md): Scripts and other files supporting CDH software
* [uart](uart/README.md): Example program for using UART
* [rules.mk](rules.mk): Used by Makefiles
* [README.md](README.md): This document
