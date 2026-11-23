**English** | [Tiếng Việt](vi/porting.md) | [中文](zh/porting.md)

# Quick Porting Guide

0. Copy the `keyboard/template` folder and rename it to your keyboard's name.
1. Open `Makefile` and update the configuration options for your target MCU.
2. Prepare your keyboard schematic for reference in the next steps.
3. Open `config.h` and configure the matrix row/column counts and pin lists, LED pins, battery sensing pins, and UART pins.
4. Adjust any remaining configuration options in `config.h` as needed.
5. Open `keymap_common.h` and define the mapping between keys and the switch matrix. Many existing templates under `tmk/keyboard` can be copied as a starting point.
6. Open `keymap_plain.c` and edit the default keymap.
7. Run `make` in the terminal to test compilation.

## Advanced Features

See [hook.md](hook.md) for available keyboard hooks and their behavior.
