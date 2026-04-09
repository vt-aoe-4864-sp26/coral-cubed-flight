# WSL All-in-One Setup for CDH Flash Demo

This guide installs everything needed to build and flash the **CDH flash demo** from **WSL** in one pass.

It assumes:

- your repo root is `coral-cubed-flight`
- your demo app is at `src/main/CDH_flash_demo`
- your flash script is at `src/main/CDH/flash_cdh_flash_demo_wsl.sh`
- your board files are at `src/main/CDH/boards`

---

## 1. Open WSL and go to the repo root

```bash
cd /mnt/c/Users/<YOUR_USER>/OneDrive/Documentos/GitHub/coral-cubed-flight
```

---

## 2. Install Linux dependencies

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  python3-venv dos2unix wget git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler python3-dev python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 \
  stlink-tools openocd usbutils
```

---

## 3. Create or refresh the virtual environment

If `coraldev` does not exist yet:

```bash
python3 -m venv coraldev
```

Activate it:

```bash
source coraldev/bin/activate
```

Upgrade pip and install west:

```bash
python -m pip install --upgrade pip
python -m pip install west
west --version
```

---

## 4. Restore the west workspace and Python packages

Initialize submodules:

```bash
git submodule update --init --recursive
```

If `.west` does not exist and your repo has `west.yml`, run:

```bash
west init -l .
```

Then run:

```bash
west update
west zephyr-export
west packages pip --install
```

---

## 5. Make sure the demo layout is correct

Your tree should look like this:

```text
src/main/
├── CDH/
│   ├── boards/
│   ├── flash_cdh_flash_demo_wsl.sh
│   └── ...
└── CDH_flash_demo/
    ├── CMakeLists.txt
    ├── prj.conf
    ├── README.md
    └── src/
        └── main.c
```

---

## 6. Fix the flash script permissions

```bash
chmod +x src/main/CDH/flash_cdh_flash_demo_wsl.sh
```

---

## 7. Attach the ST-Link from Windows to WSL

Open **PowerShell as Administrator** in Windows and run:

```powershell
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

Notes:

- only `bind` once if needed
- repeat `attach` each time you reconnect the probe

---

## 8. Verify the ST-Link inside WSL

Back in WSL:

```bash
lsusb
st-info --probe
```

If `st-info --probe` works, WSL can see the probe.

---

## 9. Build only first

From the repo root:

```bash
src/main/CDH/flash_cdh_flash_demo_wsl.sh --no-flash
```

If that succeeds, flash:

```bash
src/main/CDH/flash_cdh_flash_demo_wsl.sh --no-build
```

Or do both at once:

```bash
src/main/CDH/flash_cdh_flash_demo_wsl.sh
```

---

## 10. What the demo should do

After flashing, the board should:

- boot the demo firmware
- bring up the USB console
- initialize the external QSPI flash
- erase one test sector
- write `0xDEADBEEF`
- read it back
- print the result
- blink LED1 forever

Expected console output will look similar to:

```text
--- CDH FLASH DEMO START ---
USB console live
NVM JEDEC ID OK: 9D 60 18
NVM ready: ...
FLASH TEST: read back = 0xDEADBEEF
FLASH TEST: PASS
```

---

## 11. If something fails

### `west: command not found`

Run:

```bash
source coraldev/bin/activate
python -m pip install --upgrade pip
python -m pip install west
west --version
```

### build fails because Python packages are missing

Run:

```bash
source coraldev/bin/activate
west packages pip --install
```

### ST-Link is not detected

In Windows PowerShell:

```powershell
usbipd list
usbipd attach --wsl --busid <BUSID>
```

In WSL:

```bash
lsusb
st-info --probe
```

### flash script fails to find `coraldev`

Make sure you run it from the **repo root**:

```bash
cd /mnt/c/Users/<YOUR_USER>/OneDrive/Documentos/GitHub/coral-cubed-flight
src/main/CDH/flash_cdh_flash_demo_wsl.sh --no-flash
```

---

## 12. One-shot command block

If you want the shortest path from WSL, use this from the repo root:

```bash
sudo apt-get update && sudo apt-get install -y --no-install-recommends \
  python3-venv dos2unix wget git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler python3-dev python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 \
  stlink-tools openocd usbutils && \
python3 -m venv coraldev && \
source coraldev/bin/activate && \
python -m pip install --upgrade pip && \
python -m pip install west && \
git submodule update --init --recursive && \
(if [ ! -d .west ] && [ -f west.yml ]; then west init -l .; fi) && \
west update && \
west zephyr-export && \
west packages pip --install && \
chmod +x src/main/CDH/flash_cdh_flash_demo_wsl.sh
```

Then attach the ST-Link from Windows and run:

```bash
src/main/CDH/flash_cdh_flash_demo_wsl.sh
```
