# WSL Flash Demo Steps

## 1. Attach the ST-Link to WSL from Windows
Open **PowerShell** on Windows and list USB devices:

```powershell
usbipd list
```

Attach the ST-Link to WSL using its BUSID:

```powershell
usbipd attach --wsl --busid <BUSID>
```

## 2. Verify the ST-Link inside WSL
In Ubuntu / WSL:

```bash
lsusb
st-info --probe
```

## 3. Copy the demo app into the repo
Copy the `CDH_flash_demo` folder into:

```text
src/main/CDH_flash_demo
```

## 4. Flash the demo
From the project root in WSL:

```bash
chmod +x flash_cdh_flash_demo_wsl.sh
./flash_cdh_flash_demo_wsl.sh
```

## 5. Open the USB console
Use your usual serial terminal to watch boot output. The demo should print:
- USB console live
- JEDEC ID
- flash ready
- read back = 0xDEADBEEF
- PASS

## Optional flags
Skip build:

```bash
./flash_cdh_flash_demo_wsl.sh --no-build
```

Skip flash:

```bash
./flash_cdh_flash_demo_wsl.sh --no-flash
```
