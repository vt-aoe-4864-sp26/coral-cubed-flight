# coral-cubed-flight

Flight Software Stack for the Coral Cube

## Contributors

Coral Cubed Team, Avionics Lab PicoSat DBF Spring '26

Software Lead: Jack Rathert

Other Contributors: Julia Alcantera, Nico DeMarinis

Bare-Metal LibOpenCM3 Examples and boilerplate board files written by Bradley Denby, Chad Taylor, and others!

## Software

The source code for this repository is organized by hardware target:

* [CDH](src/main/CDH/README.md)
* [COM](src/main/COM/README.md)
* [CoralPico](src/main/TPU/README.md)

This is accompanied by supporting python scripts and firmware for a host PC to send commands via S-band radio utilizing a second COM board.
* [GND](src/main/GND/README.md)

### Architecture

This flight software repository utilizes the [Zephyr Project](https://zephyrproject.org/), a Real-Time Operating System (RTOS) for the CDH, COM, and GND embedded boards. 

This RTOS approach enables the usage of much existing infrastructure, greatly accelerating development during this time-constrained course, at the cost of run-time overhead. 

Zephyr does handle this better than many other RTOS projects, and is relatively low-overhead, allowing the user to trim down the includes by quite a bit, but the kernal will always require additional headroom.

Some primary features of Zephyr that resulted in its adoption for team Coral Cubed:

* Concurrency: Gone are the days of bare-metal superloops. Zephyr allows true threading, interrupt service routines, and concurrency primitives, opening up the system state machine significantly.
* Native Nordic nrf52833 Hardware Abstraction Layer (HAL): The HAL greatly simplified control of the MCU over the baseline, bare-metal libopencm3 this course initally provided. 
* Streamlined IEE/IPV6 networking management: No need to reinvent the wheel, Zephyr built-ins provide our network layer.
  

## Dependencies

It is highly reccommended to install this repository into a workspace directory.
The setup script will install dependencies for Zephyr alongside this repository, and not inside it.

```tree
REPOS
|Workspace
├── coral-cubed-flight
├── modules
├── .west
└── zephyr
```

If you are installing on Native x64 Linux (recommended):

```bash
chmod +x setup.sh
./setup.sh
```

If you are installing via WSL :

```bash
sudo apt-get install dos2unix
chmod +x setup.sh
dos2unix setup.sh
./setup.sh
```

If you are on WSL, you will also be required to route your USB device for flashing to your WSL, using [usbipd](https://learn.microsoft.com/en-us/windows/wsl/connect-usb) on your Windows partition.
