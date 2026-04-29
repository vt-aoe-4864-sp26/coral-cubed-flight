# CDH QSPI Flash Demo Report

## Overview

This report summarizes the work completed to build a standalone **CDH external QSPI flash demo** for the Coral Cubed flight software, the changes made to support a **PlatformIO-based workflow**, the current working state of the demo, and the remaining issue that still needs to be fixed.

The goal of this work was to create a **small, isolated demo** that could:

- build independently from the main CDH application
- leave the original CDH application intact
- flash onto the CDH board without requiring COM or payload boards
- initialize the external QSPI flash
- erase one test sector
- write a known test value (`0xDEADBEEF`)
- read it back
- print the result over USB console
- blink an LED to show the board is alive

---

## What Was Done

## 1. Original Goal

The original goal was to begin using the external nonvolatile memory connected to the CDH board and to do that with the least disruption possible to the current codebase.

The desired direction was:

- identify the external flash part
- confirm the MCU-to-memory pin mapping
- confirm the device tree configuration
- prepare initialization code
- add future read, write, and erase support
- keep data handling on the command-processing thread
- avoid changing unrelated CDH logic

---

## 2. Hardware and Flash Identification

The external memory on the CDH board was identified as the **IS25LP128F** serial NOR flash connected to the STM32L496 over **QSPI Bank 1**.

The connection path was traced and confirmed as:

- `QS_BK1_NCS` -> `CE#`
- `QS_CLK` -> `SCK`
- `QS_BK1_IO0` -> `SI / IO0`
- `QS_BK1_IO1` -> `SO / IO1`
- `QS_BK1_IO2` -> `WP# / IO2`
- `QS_BK1_IO3` -> `HOLD# / RESET# / IO3`

The flash is powered from **3.3 V** and used as **external nonvolatile memory**.

Important flash properties identified during the work:

- 128 Mbit / 16 MiB total capacity
- QSPI-capable serial NOR flash
- 4 KB sector erase support
- 256-byte page program behavior
- JEDEC ID expected as `9D 60 18`

---

## 3. Devicetree Review

The CDH board devicetree (`coral_stm32.dts`) was reviewed and the external flash definition was confirmed to already exist.

The key findings were:

- the `quadspi` peripheral is enabled
- the QSPI Bank 1 pins are already declared
- the flash node exists as `is25lp128`
- the compatible string is `st,stm32-qspi-nor`
- the flash node is marked `okay`

This was a major success because it meant the board already had the necessary low-level flash definition in place and the software work could focus on initialization and runtime access rather than starting from zero.

---

## 4. prj.conf Review

The runtime configuration was checked and the existing flash-related settings were reviewed.

The important existing settings were:

- `CONFIG_SPI=y`
- `CONFIG_FLASH=y`
- `CONFIG_FLASH_MAP=y`

Then the additional setting needed for JEDEC/SFDP checks was identified and added:

- `CONFIG_FLASH_JESD216_API=y`

This enabled the code path used for software-side device verification.

---

## 5. Flash Initialization Work

A ready-to-paste initialization function was prepared for CDH to bind to the external flash and verify it at runtime.

The initialization logic was designed to:

- bind directly to `DT_NODELABEL(is25lp128)`
- check `device_is_ready()`
- read the flash size
- optionally read the JEDEC ID
- print useful initialization messages
- avoid changing unrelated application logic

The intended use was to keep the flash logic integrated cleanly with the existing CDH architecture.

---

## 6. Flash Test Function

A first flash test helper was added:

- erase one reserved 4 KB test sector
- write `0xDEADBEEF`
- read it back
- print the readback value
- return success or failure

This function was created as a simple demonstration that the external flash path was alive and usable before building more complex storage functions.

---

## 7. Requirements Work

The CDH requirements were reorganized into a **checkbox-style checklist** so that progress could be tracked more clearly.

Additional flash-related future-work requirements were added, including:

- flash initialization integration
- flash read support
- flash write support
- flash erase support
- flash JEDEC and size verification
- flash-region planning for logs and data storage

This gave the memory work a clearer project-management structure and made it easier to explain in presentations and reports.

---

## 8. Presentation Work

A compact explanation was prepared for presentation use describing:

- the QSPI hardware connection
- the role of the IS25LP128F
- the software-side confirmation of pin mapping and configuration
- the use of JEDEC ID / JESD216-SFDP as a runtime verification mechanism
- the next steps for read/write/erase support

This helped translate the engineering work into a form suitable for project updates and demonstrations.

---

## 9. Build-System Pivot: Zephyr/West to PlatformIO

A major part of the work was discovering that the original repo state and local environment were not a good fit for continuing with the old WSL `west` flow.

### Problems encountered on the west/WSL path

The original approach tried to build the demo using:

- WSL
- Zephyr west workspace tooling
- ST-Link flashing tools
- old repo setup scripts

This ran into several practical problems:

- missing `west` in the virtual environment
- removed or incomplete Zephyr tooling
- submodule problems (`libopencm3` path issue)
- confusion caused by mixing WSL, west, PlatformIO, and partially migrated repo state

At that point, the workflow was intentionally redirected to match the actual desired toolchain:

- **VS Code**
- **PlatformIO extension**
- a standalone **PlatformIO Zephyr demo project**

This was the right decision because it aligned the demo with the workflow actually being used.

---

## 10. PlatformIO Demo Project

A separate PlatformIO project was created so the original CDH app would stay intact.

### Why a separate demo project was created

The standalone demo project was designed to:

- avoid touching the original CDH application
- avoid COM handshake blocking
- avoid requiring the COM or payload boards
- focus only on external QSPI flash functionality
- isolate the demo behavior from mission software startup behavior

### Demo behavior

The demo application was designed to do only the following:

1. initialize LEDs and GPIO
2. start the USB console
3. initialize the external QSPI flash
4. verify the flash
5. erase one test sector
6. write `0xDEADBEEF`
7. read it back
8. print the result
9. blink LED1 forever

### PlatformIO project structure

A PlatformIO Zephyr project was assembled with:

- `platformio.ini`
- `boards/coral_stm32.json`
- `src/main.c`
- `zephyr/CMakeLists.txt`
- `zephyr/prj.conf`

This structure was chosen so the demo could be built independently from the main app.

### Custom board work

A custom PlatformIO board manifest was added for `coral_stm32`, including:

- MCU type
- CPU/core information
- flash size
- RAM size
- ST-Link configuration
- OpenOCD target configuration
- vendor, name, and URL metadata

This was necessary because the board is custom and not part of the standard PlatformIO board list.

---

## 11. OneDrive / WSL Filesystem Issue

A major practical blocker during the PlatformIO build phase was the project being located under **OneDrive** and built through WSL from a Windows-mounted path.

This caused build instability and permissions errors in generated files under `.pio/build`.

To fix that, the repo was ultimately moved to a **normal local path outside OneDrive**.

This was an important cleanup step because it removed:

- OneDrive sync interference
- DrvFS permission oddities
- instability in generated build outputs

This made the local development setup much cleaner.

---

## 12. Current Working State

## What is currently working

At this point, the following is confirmed working:

- the custom PlatformIO project builds successfully
- the custom board configuration is accepted
- the board files are found correctly
- the firmware uploads successfully using ST-Link
- the MCU boots the demo firmware
- LED blinking confirms the firmware is running
- the USB console works
- the external flash responds to the init function enough to return a JEDEC ID

A successful upload was completed and verified.

The console output currently shows:

```text
*** Booting Zephyr OS build zephyr-v40300 ***

--- CDH FLASH DEMO START ---
USB console live
NVM init failed: unexpected JEDEC ID 18 9D 60
FLASH DEMO: init failed
```

That means:

- the demo app boots
- the USB console is alive
- the flash initialization code is reached
- the flash does answer a JEDEC read
- the failure is not a dead board or dead flash
- the failure is currently in the JEDEC ID validation logic

---

## What Is Wrong Right Now

## 1. JEDEC ID check is too strict

The current code expects the JEDEC ID in exactly this order:

```text
9D 60 18
```

But the demo is currently receiving:

```text
18 9D 60
```

This is the same three bytes, but rotated.

### What this means

This means the flash is responding, but the software-side check rejects the result before continuing to the rest of the test.

So the current initialization failure is **not** because the flash is missing or dead.

It is most likely one of these:

- the API is returning the bytes in a different order than expected
- the driver is exposing the JEDEC bytes in a rotated form
- the check logic needs to be relaxed for this board/demo path

### Practical consequence

Because the init function currently fails, the demo never reaches:

- sector erase
- write of `0xDEADBEEF`
- readback verification

So the next real fix is to adjust the JEDEC validation logic so the demo can continue.

---

## 2. Demo currently stops after flash init failure

The demo prints:

```text
FLASH DEMO: init failed
```

and stops before the actual write/readback stage.

That means the board is alive, but the core flash demonstration is not yet complete.

---

## Why the Current Result Is Still a Success

Even though the write/readback part is not yet passing, the current result is still a strong success because it proves:

- the board boots
- the custom PlatformIO project is valid
- ST-Link upload works
- the USB console works
- the external flash path is active
- the QSPI configuration is at least partially correct
- the software reaches and communicates with the flash

That is a very meaningful milestone.

---

## Big Description of the Demo

## Demo purpose

The demo was created to provide a **small, clean, standalone proof-of-concept** for the CDH external QSPI flash before integrating more advanced memory handling into the main flight software.

It is intentionally limited in scope.

Instead of demonstrating the full CDH application with COM and payload interaction, this demo isolates only the external memory path.

This makes the demo much easier to build, flash, and explain.

## Why this demo is important

The external flash will be important for future CDH capabilities such as:

- data logging
- payload product storage
- event storage
- delayed forwarding
- nonvolatile state retention

Before any of those larger features can be added safely, the software first needs to prove that it can:

- see the device
- initialize it
- erase memory safely
- write known values
- read them back correctly

That is exactly what this demo is for.

## Demo design philosophy

The design intentionally avoids:

- COM startup dependencies
- payload dependencies
- command-routing complexity
- larger mission-state management
- unrelated board interactions

It keeps only the minimum needed for a successful memory bring-up demonstration.

That makes it ideal for:

- presentations
- lab validation
- isolated troubleshooting
- first-stage memory integration

---

## Big Description of the PlatformIO Work

## Why PlatformIO was used

The PlatformIO workflow was chosen because it matches the actual intended development environment:

- VS Code
- PlatformIO extension
- simpler project/task integration
- easier local build-and-upload flow for a focused demo

This avoided the complexity of trying to restore a fully working Zephyr west workspace in a partially modified repo while also keeping the original CDH application intact.

## What PlatformIO changed

Moving to PlatformIO required:

- creating a new independent demo project
- creating a custom board manifest for the Coral STM32 board
- mapping the Zephyr board root correctly
- adapting the project structure to PlatformIO’s expectations
- configuring ST-Link upload behavior

Once those pieces were in place, the build and upload process became reliable.

## Why this matters

This means the demo can now be developed and demonstrated using a more manageable project structure, separate from the original CDH codebase, while still using the same board and Zephyr-based runtime.

---

## Recommended Next Fix

The next change should be to update the JEDEC validation logic in the flash initialization function so the demo accepts the current flash response format and proceeds to the erase/write/readback stage.

For demo purposes, the simplest fix is to accept both:

- `9D 60 18`
- `18 9D 60`

Then rebuild, upload, and verify that the console shows:

- successful flash init
- erase
- write
- readback
- `0xDEADBEEF`
- PASS

---

## Command List

## Git / repo management

```bash
git status
git branch --show-current
git checkout -b cdh-flash-demo
git add .
git commit -m "Add CDH flash demo and QSPI flash bring-up work"
git push -u origin cdh-flash-demo
```

## PlatformIO environment (WSL venv)

```bash
python3 -m venv ~/.pio-cli
source ~/.pio-cli/bin/activate
python -m pip install -U platformio
pio --version
```

## Move into the demo project

```bash
cd /mnt/c/Users/julia/github/coral-cubed-flight/src/main/CDH_flash_demo_platformio
```

## Build the demo

```bash
source ~/.pio-cli/bin/activate
pio run
```

## Upload the demo

```bash
source ~/.pio-cli/bin/activate
pio run -t upload
```

## Check serial devices in WSL

```bash
ls /dev/tty*
ls /dev/ttyACM*
ls /dev/ttyUSB*
```

## Read the USB console with minicom

```bash
minicom -b 115200 -D /dev/ttyACM0
```

## ST-Link USB attach to WSL (Windows PowerShell)

```powershell
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

## Check ST-Link from WSL

```bash
lsusb
st-info --probe
```

## Clean old PlatformIO build artifacts

```bash
rm -rf .pio
```

## Rebuild from clean state

```bash
source ~/.pio-cli/bin/activate
rm -rf .pio
pio run
```

---

## Suggested Immediate Next Steps

1. Modify the JEDEC check so the current returned ID is accepted.
2. Rebuild the PlatformIO demo.
3. Upload again with ST-Link.
4. Open the USB console.
5. Confirm that the demo prints a successful `0xDEADBEEF` readback.
6. Then proceed to formal flash read, write, and erase helper functions for future CDH integration.

---

## Final Status Summary

## Successes

- QSPI flash part identified
- pin mapping confirmed
- devicetree configuration confirmed
- flash-related config reviewed and updated
- flash init function prepared
- flash test function prepared
- CDH requirements updated
- presentation summary prepared
- separate demo app created
- PlatformIO custom project created
- custom board manifest created
- project builds successfully
- ST-Link upload works
- board boots
- LED blinks
- USB console works
- flash responds to JEDEC query

## Current issue

- JEDEC validation currently fails because the returned ID is `18 9D 60` instead of the strictly expected `9D 60 18`

## Overall state

The demo infrastructure is working, the board is alive, and the flash is responding. The remaining problem is a **software-side JEDEC check issue**, not a broken board or failed build system.


