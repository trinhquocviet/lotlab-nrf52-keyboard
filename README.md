# nrf52-keyboard

**English** | [Tiếng Việt](README.vi.md) | [中文](README.zh.md)

![Build Status](https://api.travis-ci.org/Lotlab/nrf52-keyboard.svg?branch=master)

## Overview

This is the custom version mainly focus on lot60BLE by Lotlab, a TMK-based keyboard firmware for the Nordic nRF52 series (supporting both **nRF52810** and **nRF52832**), using nRF5 SDK 15.3 as the low-level hardware driver and TMK as the upper-layer keyboard logic.

The original and official firmware look at here [Lotlab/nrf52-keyboard](https://github.com/Lotlab/nrf52-keyboard).

## Highlights

- **BLE / USB dual mode** with USB NKRO, mouse keys, media keys (including macOS F3–F6 consumer usages), and macros
- **Multi-device BLE switching** — switch between up to 3 paired BLE hosts
- **Custom UTF-8 BLE device name & per-channel profiles** — The custom name with the affix `_<number>`
- **Live battery sync** — real-time SAADC sampling with automatic BLE BAS notifications (macOS battery widget sync) and HID battery query (`0x16`)
- **Customizable ESC RGB status indicators** — per-channel and connection/charging status colours
- **Dynamic keymap / macro configuration** with auto-loaded factory defaults and FDS timeouts on fresh flash
- **Low power & nRF52810 optimizations** — ~200 uA idle draw (LEDs off on Lot60-BLE), `DYNAMIC_TX_POWER`, single-register `NRF_P0->IN` matrix scan, and 2 KB reclaimed RAM
- **Hardened storage & UART** — resilient FDS flash queue with RAM-only fallback and `NRF_UARTE0` EasyDMA + PPI transport
- **No-jumper flashing & build tooling** — automated build/flash scripts with 5-layer compatibility checks, GCC 12+ support, and software DFU/ISP mode switching
- **Extensible driver & event system** — rotary encoder, WS2812, OLED, and modular hooks (see `application/main/src/driver/`)

> Download this custom firmware from the **[Releases](https://github.com/trinhquocviet/lotlab-nrf52-keyboard/releases)** page.

## Supported Hardware

Currently supports both **nRF52810** and **nRF52832**. See the `keyboard/` directory for supported keyboard PCBs and revisions.

## Directory Structure

```text
.
├── application              # Bootloader and main application firmware for nRF52
│   ├── main                 # Main nRF52 application
│   │   ├── src              # Source code
│   │   │   ├── ble          # Bluetooth Low Energy (BLE) stack and services
│   │   │   ├── config       # SDK and hardware configuration
│   │   │   ├── driver       # Peripheral device drivers
│   │   │   ├── keyboard     # Keyboard matrix, power, storage, and event logic
│   │   │   ├── protocol     # UART / HID communication protocol
│   │   │   └── tmk          # TMK keyboard core adapter
│   │   └── project          # Build projects and linker scripts
│   └── bootloader           # Secure DFU bootloader for nRF52
├── docs                     # Project documentation and guides (EN / VI / ZH)
├── keyboard                 # Keyboard-specific hardware configs and keymaps (e.g. lot60-ble)
├── scripts                  # Build, flashing, and software mode-switching helper scripts
├── SDK                      # Nordic nRF5 SDK 15.3 directory
├── template                 # nRF SDK Makefile templates
├── tmk                      # TMK core source code
├── usb                      # CH554 USB bridge & CMSIS-DAP firmware
├── Dockerfile
├── README.md
└── ...
```

## Documentation

- [Development Guide](docs/development.md) — Branches, toolchain setup, compiling, and flashing
- [Lot60-BLE Hardware, Mode Switching & Flashing Guide](docs/lot60-ble-guide.md)
- [Communication Protocol](docs/communicate.md)
- [External Drivers & Modular Hooks](docs/driver.md)
- [Keyboard Hooks Reference](docs/hook.md)
- [Hardware Porting Guide](docs/porting.md)

