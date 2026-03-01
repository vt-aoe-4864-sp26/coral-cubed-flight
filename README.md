# coral-cubed-flight

Flight Software Stack for the Coral Cube

## Contributors

Coral Cubed Team, Avionics Lab PicoSat DBF Spring '26

Software Lead: Jack Rathert

Examples and boilerplate board files written by Bradley Denby, Chad Taylor, and others!

## Software

The source code for this repository is organized by hardware target:

* [CDH](src/main/CDH/cdh_main.c)
* [COM](src/main/COM/com_main.c)
* [CoralPico](src/main/TPU/com_main.c)

This is accompanied by supporting python scripts for a host PC to drive UART control:

* [GND](src/main/GND/README.md)

## Dependencies

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
