#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.9"
# dependencies = [
#     "pyocd",
# ]
# ///
"""
Lot60-BLE: Enter nRF52 DFU Mode via software.
This script uses the always-active CMSIS-DAP debugger on the CH554
to write the DFU magic value to the nRF52's GPREGRET register and reboot it.
"""
import sys
try:
    from pyocd.core.helpers import ConnectHelper
    from pyocd.core.memory_map import MemoryType
    from pyocd.probe.pydapaccess.interface import common
    
    # Dynamically register the Lot60.G VID/PID to PyOCD's internal whitelist
    # This ensures it works even inside isolated 'uv' environments.
    if (0x1209, 0x0514) not in common.KNOWN_CMSIS_DAP_IDS:
        common.KNOWN_CMSIS_DAP_IDS = tuple(list(common.KNOWN_CMSIS_DAP_IDS) + [(0x1209, 0x0514)])
    if "Lot60.G" not in common.KNOWN_DEVICE_STRINGS:
        common.KNOWN_DEVICE_STRINGS = tuple(list(common.KNOWN_DEVICE_STRINGS) + ["Lot60.G"])
except ImportError:
    print("Error: pyocd is not installed. Please run: pip install pyocd")
    sys.exit(1)

# nRF52 Register Definitions
GPREGRET_ADDR = 0x4000051C
BOOTLOADER_DFU_START = 0xB1
AIRCR_ADDR = 0xE000ED0C
AIRCR_SYSRESETREQ = 0x05FA0004

def force_dfu():
    print("Connecting to CMSIS-DAP debugger...")
    try:
        # Automatically connect to the first available probe (the CH554)
        with ConnectHelper.session_with_chosen_probe(target_override="nrf52") as session:
            board = session.board
            target = board.target
            
            print("Connected to nRF52.")
            
            # 1. Write the magic value to GPREGRET
            print(f"Writing 0xB1 to GPREGRET (0x{GPREGRET_ADDR:08X})...")
            target.write32(GPREGRET_ADDR, BOOTLOADER_DFU_START)
            
            # 2. Trigger a software reset via AIRCR
            print("Triggering system soft-reset...")
            target.write32(AIRCR_ADDR, AIRCR_SYSRESETREQ)
            
            print("\nSuccess! The nRF52 is rebooting into DFU mode.")
            print("Look for 'DFUTarg' in your BLE scanner or use nrfutil over BLE.")
            
    except Exception as e:
        print(f"\nFailed to enter DFU mode: {e}")
        print("Make sure the keyboard is plugged in and no other PyOCD instances are running.")

if __name__ == "__main__":
    force_dfu()
