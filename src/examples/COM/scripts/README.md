# COM Software Scripts

Scripts and other files supporting COM software

**Dependencies**

```bash
sudo apt install python3-venv
sudo usermod -aG dialout $USER
sudo shutdown --reboot now
cd /path/to/sb-com/software/scripts/
./setup_dependencies.sh
```

## Directory Contents

* [py-tab](py-tab/README.md): Python implementation of TAB
* [rx](rx/README.md): A Python receiver example
* [tx](tx/README.md): A Python transmitter example
* [setup_dependencies.sh](setup_dependencies.sh): Run this script to setup the
  Python 3 virtual environment
* [sourcefile.txt](sourcefile.txt): `source sourcefile.txt` to place the MCU
  cross-compiler in your `$PATH`
* [README.md](README.md): This document
