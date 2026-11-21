#!/usr/bin/env -S uv run
# /// script
# requires-python = ">=3.9"
# dependencies = [
#     "hidapi",
# ]
# ///
"""
Lot60-BLE: Enter CH554 USB ISP Mode via software.
This script sends a magic HID packet to the CH554 to force it into the bootloader
without needing to hold the physical K1 button.
"""
import hid
import sys
import time

VID = 0x1209
# You may need to verify your specific PID using lsusb or system info
PID = 0x7110 

def enter_isp():
    print("Searching for Lot60-BLE keyboard...")
    devices = hid.enumerate(VID)
    if not devices:
        print("Error: Keyboard not found! Make sure it is plugged in via USB.")
        sys.exit(1)
    
    # We need to find the configuration interface (usually interface 2 or 3)
    # The CH554 composite device exposes a keyboard, mouse, system control, and config interface.
    # macOS blocks opening keyboard/mouse interfaces, so we must target the vendor-defined interface.
    target_path = None
    for d in devices:
        if d.get('usage_page') == 0xFF00 and d.get('interface_number') == 3:
            target_path = d['path']
            break
    
    if not target_path:
        for d in devices:
            if d.get('usage_page') == 0xFF00:
                target_path = d['path']
                break

    if not target_path:
        target_path = devices[-1]['path'] # Fallback

    
    try:
        print(f"Opening device path: {target_path}")
        h = hid.device()
        h.open_path(target_path)
        h.set_nonblocking(1)
        
        # Magic packet: Report ID (0x00) + 0xF0 + 0x00
        magic_packet = [0x00, 0xF0, 0x00]
        
        print("Sending ISP jump command to Endpoint 3...")
        h.write(magic_packet)
        time.sleep(0.5)
        h.close()
        print("\nSuccess! The keyboard should disconnect and reappear as a CH554 ISP device.")
        print("You can now use `usbisp` to flash the CH554 firmware.")
    except Exception as e:
        print(f"\nError: {e}")
        print("Hint: On Linux, you may need to run this script with `sudo` or setup udev rules.")

if __name__ == "__main__":
    enter_isp()
