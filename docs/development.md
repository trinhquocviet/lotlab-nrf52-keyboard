**English** | [Tiếng Việt](vi/development.md) | [中文](zh/development.md)

# Development Guide

This guide covers setting up the build toolchain, compiling firmware and bootloaders, flashing target hardware, and navigating the codebase.

---

## Code Branches

- `master`: Main development branch; may contain active changes.
- `develop`: Experimental branch; may contain unfinished features or build failures.

For daily use, download pre-built stable firmware from the **[Releases](https://github.com/trinhquocviet/lotlab-nrf52-keyboard/releases)** page.

---

## Toolchain Setup

1. **nRF5 SDK 15.3**
   Download [nRF5 SDK 15.3](https://www.nordicsemi.com/Software-and-Tools/Software/nRF5-SDK/Download#infotabs), extract it, and place its contents into the `SDK/` directory (so `SDK/components` exists).

2. **ARM GCC Toolchain**
   Install `gcc-arm-none-eabi` (`7-2018-q2-update` or GCC 12+, compatible with GCC 15.x via `-Wno-array-bounds` in `application/main/project/app.mk`).
   Copy `template/Makefile.posix.template` or `template/Makefile.windows.template` to `Makefile.posix` or `Makefile.windows` depending on your OS, and update the toolchain path inside.

3. **SDCC (for CH554 USB firmware)**
   Install [SDCC](http://sdcc.sourceforge.net/) (`>= 4.0`). Older SDCC packages (e.g., Ubuntu 18.04 / Debian 10) will not compile the CH554 firmware.

4. **uECC (only required for Bootloader)**
   To compile the Secure DFU bootloader, build the `uECC` library first. See [Getting Started with Nordic's Secure DFU Bootloader](https://devzone.nordicsemi.com/b/blog/posts/getting-started-with-nordics-secure-dfu-bootloader).

---

## Compiling

### Local Build

```bash
cd keyboard/lot60-ble
make            # Compile main nRF52 application and CH554 USB firmware
make bootloader # Compile nRF52 Secure DFU bootloader
```

### Helper Script & Docker

You can also build using the helper script or the pre-configured Docker image (includes toolchain and SDK):

```bash
# Build a specific revision (e.g., Lot60-BLE Rev.G/GX)
./scripts/build_lot60ble.sh --revision rev_g

# Build all revisions
./scripts/build_lot60ble.sh

# Build via Docker
./scripts/build_lot60ble.sh --docker
# or directly:
docker run lotlab/nrf52-keyboard
```

---

## Flashing

- **nRF52 (DAP-Link / SWD):** Recommended. Install [pyOCD](https://github.com/mbedmicro/pyOCD) (or run via `uvx pyocd`).
- **nRF52 (BLE DFU):** Install [nrfutil](https://github.com/NordicSemiconductor/pc-nrfutil/) to generate `.zip` DFU packages, or flash wirelessly with the nRF Connect mobile app.
- **CH554 (USB ISP):** Use the official [WCH ISP Tool](http://www.wch.cn/downloads/WCHISPTool_Setup_exe.html) on Windows, or [librech551 (`usbisp`)](https://github.com/rgwan/librech551) on Linux.

Run `make help` inside any keyboard directory to list all flashing and packaging targets.

For step-by-step flashing, software mode switching (DFU / ISP without physical buttons), and compatibility checks, see the [Lot60-BLE Hardware, Mode Switching & Flashing Guide](lot60-ble-guide.md).

---

## Further Reading

- [Hardware Porting Guide](porting.md) — Add support for a new keyboard PCB
- [External Drivers & Modular Hooks](driver.md) — Write custom peripheral drivers, Fn handlers, ADC channels, and HID configs
- [Keyboard Hooks Reference](hook.md) — `hook_bootmagic()` and `action_function()`
- [Communication Protocol](communicate.md) — UART and HID configuration protocol specification
