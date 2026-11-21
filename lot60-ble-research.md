# Lot60-BLE Research & Flashing Guide

> **PCB:** Rev.GX — single RGB LED — nRF52810 — on-board CMSIS-DAP
> **Fork:** <https://github.com/trinhquocviet/lotlab-nrf52-keyboard>
> **Wiki:** <https://wiki.lotlab.org/ble/lot60-ble.html>
> **Last updated:** 2026-06-11

---

## 1. Hardware Overview

The Lot60-BLE is a 5×14 (60%) Bluetooth keyboard. Each PCB has **two chips**:

| Chip | Role | Flash tool | Firmware |
|------|------|-----------|----------|
| **nRF52810** (or nRF52832 variant) | Main controller — BLE, key matrix, LED | pyocd / nrfutil DFU | `.hex` or `.zip` |
| **CH554** | USB interface — HID, UART bridge, CMSIS-DAP debugger | usbisp (Linux) | `.bin` |

Both firmware files **must** always be updated together when upgrading.

### Default Keyboard Behaviour

- Boot: `Space + U`
- Clear paired devices: `Space + E`
- Sleep: `Fn + CapsLock`
- Switch USB/BLE: `Fn + Tab`
- Multi-device switch (v1.2.0.0): `Space + U + Q/W/E` (up to 3 devices)
- Idle → slow scan after 15s, auto-sleep after 600s

### Status LED Colours (Single RGB LED — `rev_g`)

| Colour | Meaning |
|--------|---------|
| White `#FFFFFF` | No connection (idle) |
| Cyan `#66FFFF` | BLE connected |
| Sky blue `#0099FF` | USB connected |
| Orange `#FF8000` | Charging |
| Green `#00FF00` | Fully charged |
| Yellow `#FFFF00` | Passkey input required |
| Pink `#FF0080` | Passkey sent |
| Purple `#FF00FF` | Sleeping |

---

## 2. PCB Revisions & Firmware Compatibility

### Revision Matrix

The build system produces firmware for **8 revision variants**. Each has different GPIO pin mappings, chip type, and LED configuration. **Flashing the wrong revision can brick the keyboard.**

| Directory | Board | Chip | LED Config | CMSIS-DAP | Has DFU Button |
|-----------|-------|------|-----------|-----------|----------------|
| `rev_c` | Rev.C | nRF52810 | 1 RGB LED | ✅ Yes | No (Buttonless DFU) |
| `rev_e` | Rev.E | nRF52810 | 1 RGB LED | ❌ No | No (Buttonless DFU) |
| `rev_e_3led` | Rev.E | nRF52810 | 3 status LEDs | ❌ No | No (Buttonless DFU) |
| `rev_f` | Rev.F | nRF52810 | 1 RGB LED | ❌ No | No (Buttonless DFU) |
| `rev_f_3led` | Rev.F | nRF52810 | 3 status LEDs | ❌ No | No (Buttonless DFU) |
| **`rev_g`** | **Rev.G/GX** | **nRF52810** | **1 RGB LED** | **✅ Yes** | **Yes (RESET pin)** |
| `rev_g_3led` | Rev.G/GX | nRF52810 | 3 status LEDs | ✅ Yes | Yes (RESET pin) |
| `rev_g_832` | Rev.G/GX | **nRF52832** | 1 RGB LED | ✅ Yes | Yes (RESET pin) |

> **My PCB → `rev_g`** (Rev.GX, single RGB LED, nRF52810, CMSIS-DAP)

### What Goes Wrong With Mismatched Firmware

| Mismatch | Consequence |
|----------|-------------|
| Wrong board family (e.g. `rev_e` on Rev.GX) | Wrong GPIO pins → dead keys, damaged LED, no USB |
| Wrong chip (`rev_g_832` on nRF52810) | Firmware exceeds 192KB flash → **won't boot, possible brick** |
| Wrong LED (`rev_g_3led` on single-LED board) | Keyboard works but status LED colours are incorrect |

### LED Configuration Details

From `common/common.mk`:
- If `THREE_LED_STATUS` is **not** set → builds with `ONE_RGB_LED = yes` and includes `user_evt.c` (single RGB LED driver)
- If `THREE_LED_STATUS = yes` → builds with `THREE_LED_STATUS` define and uses `3led_status.c` (3 discrete LED driver)

### Chip Memory Limits

| Chip | Flash Size | Max Address |
|------|-----------|-------------|
| nRF52810 | 192KB | `0x30000` |
| nRF52832 | 512KB | `0x80000` |
| CH554 | 14KB code | `0x3800` |

---

## 3. Firmware File Naming Convention

From `keyboard/lot60-ble/Makefile` (line 29), the build system generates files as:

```
lot60-ble-{revision}-{chip}-{date}-{git_version}.{ext}
```

| Component | Value | Example |
|-----------|-------|---------|
| `revision` | Subdirectory name | `rev_g`, `rev_g_3led`, `rev_g_832` |
| `chip` | `nrf52` or `ch554` | `nrf52` |
| `date` | Build date `YYYY_MM_DD` | `2026_06_11` |
| `git_version` | `git describe --always --dirty` | `v1.2.0.0` |
| `ext` | `.hex`, `.zip`, `.bin` | `.hex` |

### Example files for Rev.GX (single LED):

```
lot60-ble-rev_g-nrf52-2026_06_11-v1.2.0.0.hex   ← pyocd flash (merged with SoftDevice)
lot60-ble-rev_g-nrf52-2026_06_11-v1.2.0.0.zip   ← BLE DFU package
lot60-ble-rev_g-ch554-2026_06_11-v1.2.0.0.bin   ← CH554 USB firmware
```

### GitHub Release Workflow

- Tag format: `v*.*.*.*` (e.g. `v1.2.0.0`)
- Trigger: push tag → `.github/workflows/Release.yml`
- Builds **all 8 revisions** → uploads `*.hex`, `*.zip`, `*.bin` to GitHub Release
- Up to 24 firmware files per release (8 revisions × 3 file types)

---

## 4. Build System

### Toolchain Requirements

| Tool | Version | Purpose |
|------|---------|---------|
| `arm-none-eabi-gcc` | 7-2018-q2-update | nRF52 firmware compilation |
| `sdcc` | 4.0.0 | CH554 firmware compilation |
| `nrfutil` | Latest | DFU package generation (`.zip`) |
| `mergehex` | Nordic CLI tools | Merge SoftDevice + application into `.hex` |
| `make` | GNU Make | Build orchestration |
| nRF5 SDK | 15.3.0 | Nordic SDK (placed in `SDK/` directory) |

### Build Commands

```bash
# Build only your revision (Rev.GX single LED)
./scripts/build_lot60ble.sh --revision rev_g

# Build all revisions
./scripts/build_lot60ble.sh

# Build using Docker (no local toolchain needed)
./scripts/build_lot60ble.sh --docker
```

### Build Structure

```
keyboard/lot60-ble/
├── Makefile              ← Top-level: iterates all rev_* subdirs
├── common/
│   ├── common.mk         ← Hardware version logic, LED config, chip selection
│   ├── config.h           ← Shared config (USB IDs, key matrix, power, BLE)
│   ├── config.rev_g.h     ← Rev.G/GX GPIO pins, LED pins, matrix pins
│   ├── config.rev_c.h     ← Rev.C specific config
│   ├── config.rev_e.h     ← Rev.E specific config
│   ├── config.rev_f.h     ← Rev.F specific config
│   ├── config.rev_h.h     ← Rev.H specific config
│   ├── keymap_common.h    ← Keymap definitions
│   ├── keymap_plain.c     ← Default keymap
│   └── user_evt.c         ← Single RGB LED event handler (colour logic)
├── rev_g/Makefile         ← HARDWARE_VERSION = REV_G (our target)
├── rev_g_3led/Makefile    ← HARDWARE_VERSION = REV_G + THREE_LED_STATUS
├── rev_g_832/Makefile     ← HARDWARE_VERSION = REV_G_ALT (nRF52832)
├── rev_c/Makefile
├── rev_e/Makefile
├── rev_e_3led/Makefile
├── rev_f/Makefile
├── rev_f_3led/Makefile
└── _build/                ← Output directory
```

### Key Config Defines (Rev.G — `config.rev_g.h`)

```c
#define PRODUCT "Lot60.G"
#define DEVICE_VER 0x0003
#define LED_CAPS 19
#define LED_RGB_R 24
#define LED_RGB_G 23
#define LED_RGB_B 22
#define UART_RXD 17
#define UART_TXD 18
// Matrix: 5 rows × 14 cols
// row_pin_array = { 25, 28, 29, 30, 26 }
// column_pin_array = { 16, 15, 14, 13, 12, 11, 27, 3, 4, 5, 6, 7, 8, 9 }
```

---

## 5. Flashing Methods

### Method 1: pyocd (DAP-Link / On-board Debugger) — **Recommended for Rev.GX**

Best for Rev.G/GX which has the on-board CMSIS-DAP debugger. Wired via USB.

```bash
./scripts/flash_lot60ble.sh -m pyocd -f lot60-ble-rev_g-nrf52-*.hex

# With chip erase (for downgrades or fresh install)
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex --erase
```

**Prerequisites:**
- `pip3 install pyocd`
- macOS: `brew install libusb`
- Linux: udev rules (`sudo python3 -m pyocd.udev`)
- Debugger jumper on PCB must be connected

### Method 2: Bluetooth DFU (nrfutil / nRF Connect app)

Wireless update over BLE. Uses `.zip` DFU package.

```bash
./scripts/flash_lot60ble.sh -m dfu -f lot60-ble-rev_g-nrf52-*.zip
```

**Enter DFU mode:**
1. Sleep keyboard: `Fn + CapsLock`
2. Wake in DFU mode: `Space + B` → device shows as `DFUTarg`
3. Alternative: press RESET button on Rev.G/GX

**Manual DFU (mobile):**
1. Install nRF Connect app (iOS/Android)
2. Scan → connect to `DFUTarg` → DFU → select `.zip` file

### Method 3: USB ISP (CH554 firmware — Linux only)

For updating the CH554 USB controller chip.

```bash
./scripts/flash_lot60ble.sh -m usb -f lot60-ble-rev_g-ch554-*.bin
```

**Enter ISP mode:**
1. Disconnect USB
2. Hold ISP button (K1, on PCB back)
3. Reconnect USB while holding
4. Release after detection

**Tools:**
- Linux: `usbisp` from [librech551](https://github.com/rgwan/librech551)
- Windows: [WCH ISP Tool](http://www.wch.cn/downloads/WCHISPTool_Setup_exe.html)
- macOS: Not supported — use Linux VM

---

## 6. Firmware Compatibility Checker

The flash script (`flash_lot60ble.sh` v2) includes a **5-layer compatibility checker**:

1. **File extension vs method** — `.hex` for pyocd, `.zip` for DFU, `.bin` for USB
2. **Firmware revision vs PCB revision** — parses filename, checks board family/chip/LED match
3. **Chip type vs flash method** — prevents cross-chip firmware/method misuse
4. **CMSIS-DAP availability** — warns if PCB lacks on-board debugger
5. **Binary-level verification** — parses Intel HEX address records (detects nRF52832 firmware on nRF52810), validates CH554 `.bin` size (14KB limit)

```bash
# Default PCB is rev_g (Rev.GX single LED) — no extra flags needed
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex

# Explicit PCB override
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex -p rev_g_3led

# Skip all checks (dangerous)
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex --skip-compat
```

---

## 7. Scripts Reference

| Script | Purpose |
|--------|---------|
| `scripts/build_lot60ble.sh` | Build firmware from source (single revision or all) |
| `scripts/flash_lot60ble.sh` | Flash pre-built firmware with compatibility checking |

### Build script flags

| Flag | Description |
|------|-------------|
| `-r, --revision <rev>` | Build single revision (e.g. `rev_g`) |
| `-D, --docker` | Build using Docker image |
| `-j, --jobs <N>` | Parallel make jobs |
| `-o, --output <dir>` | Custom output directory |

### Flash script flags

| Flag | Description |
|------|-------------|
| `-m, --method <pyocd\|dfu\|usb>` | Flash method |
| `-f, --file <path>` | Firmware file |
| `-p, --pcb <revision>` | PCB revision (default: `rev_g`) |
| `-e, --erase` | Chip erase before flash (pyocd) |
| `-d, --dfu-device <addr>` | BLE device for DFU |
| `--skip-compat` | Skip compatibility checks |

---

## 8. Quick Reference for My Setup

```bash
# My PCB: Rev.GX, single RGB LED, nRF52810
# Correct firmware revision: rev_g

# Flash from GitHub release
./scripts/flash_lot60ble.sh -m pyocd -f lot60-ble-rev_g-nrf52-*.hex

# BLE wireless update
./scripts/flash_lot60ble.sh -m dfu -f lot60-ble-rev_g-nrf52-*.zip

# Build from source (my revision only)
./scripts/build_lot60ble.sh -r rev_g

# Full erase + flash (for downgrade or fresh install)
./scripts/flash_lot60ble.sh -m pyocd -f firmware.hex --erase
```

---

## 9. Version History

| Version | Key Feature |
|---------|-------------|
| v1.2.0.0 | `MULTI_DEVICE_SWITCH` — up to 3 paired BLE devices (`Space+U+Q/W/E`) |

## 10. Key Source Files

| File | Purpose |
|------|---------|
| `keyboard/lot60-ble/Makefile` | Build orchestrator, firmware naming pattern |
| `keyboard/lot60-ble/common/common.mk` | Hardware version logic, LED/chip selection |
| `keyboard/lot60-ble/common/config.h` | Shared config (USB IDs, matrix, BLE, power) |
| `keyboard/lot60-ble/common/config.rev_g.h` | Rev.G/GX GPIO pin assignments |
| `keyboard/lot60-ble/common/user_evt.c` | Single RGB LED colour logic |
| `keyboard/lot60-ble/rev_g/Makefile` | Rev.G build config (`HARDWARE_VERSION = REV_G`) |
| `usb/usb.mk` | CH554 build config |
| `.github/workflows/Release.yml` | Automated release build & upload |