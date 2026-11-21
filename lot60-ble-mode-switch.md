# Lot60-BLE Software Mode Switching Guide

> **Objective:** Enter DFU mode, CMSIS-DAP mode, and USB ISP mode entirely via software over USB, without using any physical buttons, keys, or jumpers. This is essential if the keyboard is sealed in its case or the buttons are inaccessible.

This document outlines the mechanisms and explains how to use the provided Python scripts in the `scripts/` directory to achieve software-based mode switching.

---

## 1. Entering CH554 USB ISP Mode

**The Problem:** Entering ISP mode normally requires holding the K1 button on the back of the PCB while plugging in the USB cable.
**The Solution:** The CH554 custom firmware explicitly listens for a magic packet on Endpoint 3 to jump into the bootloader via software.

### Mechanism
In `usb/main.c`, the firmware checks the incoming packet. Sending a packet where the second byte is `0xF0` and the third byte is `0x00` will disable the watchdog, detach the USB, and call `isp_enter()` (which jumps to `0x3800`, the CH554 built-in bootloader).

### How to Use (`scripts/enter_isp.py`)
1. Ensure your keyboard is plugged in via USB.
2. We use `uv` for seamless Python dependency management. If you don't have it, install it: `curl -LsSf https://astral.sh/uv/install.sh | sh`
3. The script has inline metadata (PEP 723), so you can just run it directly with `uv`:
   ```bash
   uv run scripts/enter_isp.py
   ```
4. The script will automatically download `hidapi` into an isolated environment and send the magic packet. The keyboard will disconnect and reappear to your OS as a CH554 ISP device. You can now use `usbisp` to flash the CH554 firmware (`.bin` file). Once flashed, it will automatically reboot back to standard mode.

---

## 2. Accessing On-board Debugger (CMSIS-DAP)

**The Problem:** Normally, debugging the nRF52 requires connecting a physical jumper to enable the debugger.
**The Solution:** You don't actually need to "enter" this mode. As long as the CH554 chip is running its standard firmware and the keyboard is plugged into USB, it acts as a composite USB device. One of those interfaces *is* the CMSIS-DAP debugger. 

### Mechanism
The CH554 runs DAP-Link compatible firmware alongside the HID keyboard driver. Tools like PyOCD communicate with it automatically via standard USB HID endpoints.

### How to Use
1. Ensure your keyboard is plugged in via USB.
2. We use `uv` for seamless Python dependency management.
3. You can run `pyocd` directly using `uvx` (which runs CLI tools without installing them globally):
   ```bash
   uvx pyocd list
   ```
   If your PCB is `Rev.GX`, you will see a probe named `CMSIS-DAP` listed.
4. You can interact with the nRF52 chip immediately, or flash firmware using the main flash script (the script will ask to install pyocd if missing, but you can also use `uvx pyocd flash` manually):
   ```bash
   ./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex
   ```

---

## 3. Entering nRF52 DFU Mode

**The Problem:** Entering DFU mode normally requires pressing `Space + B` (if the keyboard matrix is functional and mapped correctly) or triggering a physical reset pin.
**The Solution:** Since we have permanent access to the nRF52 via the CH554's CMSIS-DAP debugger, we can use the debugger to write the DFU magic value directly into the nRF52's memory and force a reboot.

### Mechanism
The nRF5 SDK bootloader checks the `GPREGRET` register (`0x4000051C`) upon boot. If it contains the magic value `0xB1` (`BOOTLOADER_DFU_START`), it stays in the bootloader and advertises DFU mode over BLE instead of booting the main application. We use PyOCD's Python API to force this state.

### How to Use (`scripts/enter_dfu.py`)
1. Ensure your keyboard is plugged in via USB.
2. We use `uv` for seamless Python dependency management.
3. The script has inline metadata (PEP 723), so you can just run it directly with `uv`:
   ```bash
   uv run scripts/enter_dfu.py
   ```
4. `uv` will automatically download `pyocd`, connect to the nRF52, write `0xB1` to `GPREGRET`, and trigger a system soft-reset. 
5. The nRF52 will reboot instantly into DFU mode. Look for `DFUTarg` in your BLE scanner on your phone (nRF Connect) or use `nrfutil` over BLE to flash a `.zip` package.

---

## 4. Alternative: Persistent Virtual Environment

If you prefer a traditional Python virtual environment instead of the on-the-fly `uv run` method, you can use the included setup script:

```bash
./scripts/setup_env.sh
```
This will use `uv` to rapidly create a `.venv` directory and install the requirements defined in `scripts/requirements.txt` (`hidapi` and `pyocd`). You can then `source scripts/.venv/bin/activate` to run the scripts.

---

## Summary of the "No-Jumper" Flashing Workflow

If your keyboard is fully assembled in a case and you cannot access the back PCB buttons/jumpers:

1. **Flash nRF52 Main Firmware (`.hex`):** Use `./scripts/flash_lot60ble.sh -m pyocd`. No mode switch required, as the CMSIS-DAP is always active.
2. **Flash nRF52 DFU Bootloader (`.zip`):** Run `./scripts/enter_dfu.py`, then flash via BLE using the nRF Connect app.
3. **Flash CH554 USB Firmware (`.bin`):** Run `./scripts/enter_isp.py`, then flash using `usbisp` (Linux). 
*(Note: After flashing the CH554, it will automatically reboot back into standard mode).*
