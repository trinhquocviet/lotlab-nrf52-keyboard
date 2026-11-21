#!/usr/bin/env bash
# =============================================================================
#  flash_lot60ble.sh — Lot60-BLE Firmware Flash Script (v2)
#  Separated from the build logic. Flashes pre-built firmware to the PCB.
#
#  Repo: https://github.com/trinhquocviet/lotlab-nrf52-keyboard
#  Wiki: https://wiki.lotlab.org/ble/lot60-ble.html
#
#  Firmware compatibility checking:
#    Parses the firmware filename to verify it matches your PCB revision,
#    chip type, and LED configuration before flashing.
#
#  Three flashing methods:
#    1. pyocd   — DAP-Link / on-board debugger (HEX, wired via USB)
#    2. nrfutil — Bluetooth DFU (ZIP package, over BLE)
#    3. usbisp  — CH554 USB firmware (Linux only via librech551/usbisp)
# =============================================================================

set -euo pipefail

# ─── Colour helpers ──────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; DIM='\033[2m'; RESET='\033[0m'

info()    { echo -e "${CYAN}[INFO]${RESET}  $*"; }
ok()      { echo -e "${GREEN}[OK]${RESET}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
error()   { echo -e "${RED}[ERROR]${RESET} $*" >&2; }
die()     { error "$*"; exit 1; }
banner()  { echo -e "\n${BOLD}${CYAN}=== $* ===${RESET}\n"; }

# ─── Version ─────────────────────────────────────────────────────────────────
SCRIPT_VERSION="2.0.0"

# ─── Usage ───────────────────────────────────────────────────────────────────
usage() {
  cat <<EOF
${BOLD}Lot60-BLE Firmware Flash Tool v${SCRIPT_VERSION}${RESET}

${BOLD}Usage:${RESET}
  $0 [OPTIONS]

${BOLD}Options:${RESET}
  -m, --method  <pyocd|dfu|usb>   Flash method (default: interactive prompt)
  -f, --file    <path>            Firmware file path (.hex / .zip / .bin)
  -p, --pcb     <revision>        Your PCB revision for compatibility check
                                    rev_g      = Rev.G/GX, single RGB LED (default)
                                    rev_g_3led = Rev.G/GX, 3 status LEDs
                                    rev_g_832  = Rev.G/GX, nRF52832 chip
                                    rev_c      = Rev.C
                                    rev_e      = Rev.E, single RGB LED
                                    rev_e_3led = Rev.E, 3 status LEDs
                                    rev_f      = Rev.F, single RGB LED
                                    rev_f_3led = Rev.F, 3 status LEDs
  -e, --erase                     Erase chip before flashing (pyocd only)
  -d, --dfu-device <addr>         BLE address / device name for DFU
      --skip-compat               Skip firmware compatibility checks
  -h, --help                      Show this help

${BOLD}Examples:${RESET}
  # Flash nRF52 via on-board DAP-Link debugger (Rev.GX)
  $0 --method pyocd --file lot60-ble-rev_g-nrf52-2024_01_01-v1.2.0.0.hex

  # Flash with explicit PCB revision check
  $0 -m pyocd -f firmware.hex -p rev_g

  # Flash nRF52 with erase (fresh install / downgrade)
  $0 --method pyocd --file firmware.hex --erase

  # Update BLE firmware via Bluetooth DFU
  $0 --method dfu --file firmware.zip

  # Update CH554 USB firmware (Linux)
  $0 --method usb --file ch554.bin

EOF
  exit 0
}

# ─── Defaults ────────────────────────────────────────────────────────────────
METHOD=""
FIRMWARE=""
PCB_REVISION="rev_g"
ERASE=false
DFU_DEVICE=""
SKIP_COMPAT=false

# ─── Parse args ──────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case $1 in
    -m|--method)     METHOD="$2";       shift 2 ;;
    -f|--file)       FIRMWARE="$2";     shift 2 ;;
    -p|--pcb)        PCB_REVISION="$2"; shift 2 ;;
    -e|--erase)      ERASE=true;        shift ;;
    -d|--dfu-device) DFU_DEVICE="$2";   shift 2 ;;
    --skip-compat)   SKIP_COMPAT=true;  shift ;;
    -h|--help)       usage ;;
    *) die "Unknown argument: $1. Run '$0 --help' for usage." ;;
  esac
done

# ─── Detect OS ───────────────────────────────────────────────────────────────
OS="$(uname -s)"
case "$OS" in
  Darwin) PLATFORM="macOS" ;;
  Linux)  PLATFORM="Linux" ;;
  *) die "Unsupported platform: $OS." ;;
esac

# ─── Check tool availability ─────────────────────────────────────────────────
check_tool() {
  local tool="$1"
  if command -v "$tool" &>/dev/null; then
    ok "$tool found at $(command -v "$tool")"
    return 0
  else
    warn "$tool not found in PATH"
    return 1
  fi
}

# ─── Install hints ───────────────────────────────────────────────────────────
hint_pyocd() {
  echo -e "${YELLOW}Install pyocd:${RESET}"
  echo "  pip3 install pyocd"
  echo "  # macOS: brew install python3 && pip3 install pyocd"
  echo "  # Linux: sudo pip3 install pyocd"
  echo "  Verify: pyocd list"
}

hint_nrfutil() {
  echo -e "${YELLOW}Install nrfutil:${RESET}"
  echo "  pip3 install nrfutil"
  echo "  # or download binary: https://github.com/NordicSemiconductor/pc-nrfutil/releases"
}

hint_usbisp() {
  echo -e "${YELLOW}Install usbisp (Linux only):${RESET}"
  echo "  git clone https://github.com/rgwan/librech551"
  echo "  cd librech551 && make && sudo make install"
}

# ─── Interactive method picker ───────────────────────────────────────────────
pick_method() {
  banner "Select Flashing Method"
  echo "  1) pyocd   — On-board DAP-Link debugger (Rev.G/GX, wired USB, .hex)"
  echo "  2) dfu     — Bluetooth DFU over BLE (.zip package)"
  echo "  3) usb     — CH554 USB firmware update (.bin via usbisp, Linux)"
  echo ""
  read -rp "Enter choice [1-3]: " choice
  case "$choice" in
    1) METHOD="pyocd" ;;
    2) METHOD="dfu" ;;
    3) METHOD="usb" ;;
    *) die "Invalid choice." ;;
  esac
}

# =============================================================================
# ─── COMPATIBILITY CHECK ENGINE ─────────────────────────────────────────────
# =============================================================================
#
# Firmware files from the build system follow this naming pattern:
#   lot60-ble-{revision}-nrf52-{date}-{git_version}.hex
#   lot60-ble-{revision}-nrf52-{date}-{git_version}.zip
#   lot60-ble-{revision}-ch554-{date}-{git_version}.bin
#
# Compatibility rules:
#
# ┌──────────────┬───────────┬────────────┬───────────────┬───────────────────┐
# │ Firmware Rev │ PCB Board │ Chip       │ LED Config    │ Has CMSIS-DAP     │
# ├──────────────┼───────────┼────────────┼───────────────┼───────────────────┤
# │ rev_c        │ Rev.C     │ nRF52810   │ 1 RGB LED     │ Yes               │
# │ rev_e        │ Rev.E     │ nRF52810   │ 1 RGB LED     │ No                │
# │ rev_e_3led   │ Rev.E     │ nRF52810   │ 3 status LEDs │ No                │
# │ rev_f        │ Rev.F     │ nRF52810   │ 1 RGB LED     │ No                │
# │ rev_f_3led   │ Rev.F     │ nRF52810   │ 3 status LEDs │ No                │
# │ rev_g        │ Rev.G/GX  │ nRF52810   │ 1 RGB LED     │ Yes               │
# │ rev_g_3led   │ Rev.G/GX  │ nRF52810   │ 3 status LEDs │ Yes               │
# │ rev_g_832    │ Rev.G/GX  │ nRF52832   │ 1 RGB LED     │ Yes               │
# └──────────────┴───────────┴────────────┴───────────────┴───────────────────┘
#
# CRITICAL: Flashing wrong firmware can:
#   - Wrong revision → wrong GPIO pin mapping → dead keyboard or damaged LED
#   - Wrong chip (810 vs 832) → firmware won't boot, possible brick
#   - Wrong LED config → status LED will not function correctly
# =============================================================================

# Map PCB revision to its expected properties
get_pcb_board_family() {
  case "$1" in
    rev_c)       echo "rev_c" ;;
    rev_e|rev_e_3led) echo "rev_e" ;;
    rev_f|rev_f_3led) echo "rev_f" ;;
    rev_g|rev_g_3led|rev_g_832) echo "rev_g" ;;
    *) echo "unknown" ;;
  esac
}

get_pcb_chip() {
  case "$1" in
    rev_g_832) echo "nrf52832" ;;
    *)         echo "nrf52810" ;;
  esac
}

get_pcb_led() {
  case "$1" in
    rev_e_3led|rev_f_3led|rev_g_3led) echo "3led" ;;
    *) echo "1rgb" ;;
  esac
}

get_pcb_has_dap() {
  case "$1" in
    rev_c|rev_g|rev_g_3led|rev_g_832) echo "yes" ;;
    *) echo "no" ;;
  esac
}

get_pcb_label() {
  case "$1" in
    rev_c)       echo "Rev.C, nRF52810, single RGB LED, CMSIS-DAP" ;;
    rev_e)       echo "Rev.E, nRF52810, single RGB LED" ;;
    rev_e_3led)  echo "Rev.E, nRF52810, 3 status LEDs" ;;
    rev_f)       echo "Rev.F, nRF52810, single RGB LED" ;;
    rev_f_3led)  echo "Rev.F, nRF52810, 3 status LEDs" ;;
    rev_g)       echo "Rev.G/GX, nRF52810, single RGB LED, CMSIS-DAP" ;;
    rev_g_3led)  echo "Rev.G/GX, nRF52810, 3 status LEDs, CMSIS-DAP" ;;
    rev_g_832)   echo "Rev.G/GX, nRF52832, single RGB LED, CMSIS-DAP" ;;
    *)           echo "Unknown revision" ;;
  esac
}

# Extract revision tag from firmware filename
# Supports patterns like:
#   lot60-ble-rev_g-nrf52-2024_01_01-v1.2.0.0.hex
#   lot60-ble-rev_g_3led-ch554-2024_01_01-v1.2.0.0.bin
#   nrf52_kbd_lot60ble.hex  (generic/unrecognised → skip check)
extract_firmware_revision() {
  local filename
  filename="$(basename "$1")"

  # Pattern: lot60-ble-{revision}-{chip_type}-{rest}
  if [[ "$filename" =~ ^lot60-ble-([a-z0-9_]+)-(nrf52|ch554)- ]]; then
    echo "${BASH_REMATCH[1]}"
    return 0
  fi

  # Pattern: {prefix}-{revision}-nrf52-{rest} (looser match)
  if [[ "$filename" =~ -(rev_[a-z0-9_]+)- ]]; then
    echo "${BASH_REMATCH[1]}"
    return 0
  fi

  # Cannot determine revision from filename
  return 1
}

# Extract chip type from firmware filename
extract_firmware_chip() {
  local filename
  filename="$(basename "$1")"

  if [[ "$filename" =~ -(nrf52|ch554)- ]]; then
    echo "${BASH_REMATCH[1]}"
    return 0
  fi

  # Infer from extension
  case "$filename" in
    *.hex) echo "nrf52" ;;
    *.zip) echo "nrf52" ;;
    *.bin) echo "ch554" ;;
    *)     return 1 ;;
  esac
}

# ─── Binary-level HEX file verification ─────────────────────────────────────
# Intel HEX files contain address records that reveal the target chip's memory
# layout. nRF52810 has 192KB flash (0x00000–0x30000), while nRF52832 has 512KB
# flash (0x00000–0x80000). If a HEX file contains addresses beyond 0x30000
# for code data, it's likely a 52832 firmware.
verify_hex_chip_compat() {
  local hex_file="$1"
  local expected_chip="$2"

  if [[ ! -f "$hex_file" ]]; then
    return 0  # Skip if file doesn't exist
  fi

  # Only check .hex files
  if [[ "$hex_file" != *.hex ]]; then
    return 0
  fi

  info "Inspecting HEX file memory layout..."

  # Parse Extended Linear Address records (type 04) and data records (type 00)
  # to find the highest address used in the firmware.
  local max_addr=0
  local ext_addr=0

  while IFS= read -r line; do
    # Remove carriage return if present
    line="${line%$'\r'}"

    # Intel HEX format: :LLAAAATT[DD...]CC
    # LL = byte count, AAAA = address, TT = record type, DD = data, CC = checksum
    if [[ "$line" =~ ^:([0-9A-Fa-f]{2})([0-9A-Fa-f]{4})([0-9A-Fa-f]{2}) ]]; then
      local byte_count_hex="${BASH_REMATCH[1]}"
      local addr_hex="${BASH_REMATCH[2]}"
      local record_type="${BASH_REMATCH[3]}"

      case "$record_type" in
        04)
          # Extended Linear Address record — sets upper 16 bits
          local data_hex="${line:9:4}"
          ext_addr=$(( 16#${data_hex} << 16 ))
          ;;
        00)
          # Data record — compute full address
          local byte_count=$(( 16#${byte_count_hex} ))
          local addr=$(( 16#${addr_hex} ))
          local full_addr=$(( ext_addr + addr + byte_count ))
          if (( full_addr > max_addr )); then
            max_addr=$full_addr
          fi
          ;;
      esac
    fi
  done < "$hex_file"

  # nRF52810 flash: 192KB = 0x30000
  # nRF52832 flash: 512KB = 0x80000
  # SoftDevice S112 (for 810) ends at ~0x19000
  # SoftDevice S132 (for 832) ends at ~0x26000
  local NRF52810_MAX=196608   # 0x30000
  local NRF52832_MAX=524288   # 0x80000

  local max_addr_hex
  max_addr_hex=$(printf "0x%X" "$max_addr")

  if [[ "$expected_chip" == "nrf52810" ]]; then
    if (( max_addr > NRF52810_MAX )); then
      echo ""
      error "═══════════════════════════════════════════════════════════════"
      error "  CHIP MISMATCH DETECTED!"
      error ""
      error "  Firmware addresses up to ${max_addr_hex} — exceeds nRF52810"
      error "  flash limit (0x30000 = 192KB)."
      error ""
      error "  This firmware appears to be built for nRF52832 (512KB flash)."
      error "  Your PCB (${PCB_REVISION}) uses nRF52810."
      error ""
      error "  Flashing this will FAIL or BRICK your keyboard!"
      error ""
      error "  Correct firmware for your PCB: rev_g (not rev_g_832)"
      error "═══════════════════════════════════════════════════════════════"
      echo ""
      return 1
    fi
    ok "HEX memory layout OK: max address ${max_addr_hex} within nRF52810 range"
  elif [[ "$expected_chip" == "nrf52832" ]]; then
    if (( max_addr <= NRF52810_MAX )); then
      warn "Firmware max address ${max_addr_hex} is within nRF52810 range."
      warn "This may be a 52810 firmware. Your PCB uses nRF52832."
      echo ""
      read -rp "  Continue anyway? [y/N]: " cont
      [[ "$cont" =~ ^[Yy]$ ]] || return 1
    else
      ok "HEX memory layout OK: max address ${max_addr_hex} matches nRF52832 range"
    fi
  fi

  return 0
}

# ─── Binary-level BIN file verification ──────────────────────────────────────
# CH554 has 14KB code flash. If a .bin file exceeds this, it's not valid.
verify_bin_size() {
  local bin_file="$1"

  if [[ ! -f "$bin_file" || "$bin_file" != *.bin ]]; then
    return 0
  fi

  local file_size
  if [[ "$PLATFORM" == "macOS" ]]; then
    file_size=$(stat -f%z "$bin_file")
  else
    file_size=$(stat -c%s "$bin_file")
  fi

  local CH554_MAX=14336  # 14KB = 0x3800

  if (( file_size > CH554_MAX )); then
    error "═══════════════════════════════════════════════════════════════"
    error "  BIN FILE TOO LARGE!"
    error ""
    error "  File size: ${file_size} bytes (max: ${CH554_MAX} bytes / 14KB)"
    error "  CH554 code flash is limited to 14KB."
    error ""
    error "  This .bin file is NOT a valid CH554 firmware."
    error "═══════════════════════════════════════════════════════════════"
    return 1
  fi

  ok "BIN size OK: ${file_size} bytes (limit: ${CH554_MAX} bytes)"
  return 0
}

# ─── Main compatibility check orchestrator ───────────────────────────────────
check_firmware_compatibility() {
  local firmware_file="$1"
  local pcb_rev="$2"
  local method="$3"

  if [[ "$SKIP_COMPAT" == true ]]; then
    warn "Skipping compatibility checks (--skip-compat)"
    return 0
  fi

  banner "Firmware Compatibility Check"

  local pcb_label
  pcb_label="$(get_pcb_label "$pcb_rev")"
  info "Your PCB: ${BOLD}${pcb_rev}${RESET} → ${pcb_label}"

  local firmware_name
  firmware_name="$(basename "$firmware_file")"
  info "Firmware file: ${BOLD}${firmware_name}${RESET}"
  echo ""

  local errors=0

  # ── Step 1: File extension vs method check ─────────────────────────────
  info "Check 1: File extension vs flash method..."
  case "$method" in
    pyocd)
      if [[ "$firmware_file" != *.hex ]]; then
        error "  pyocd method requires a .hex file (got: ${firmware_name})"
        (( errors++ ))
      else
        ok "  Extension .hex matches pyocd method"
      fi
      ;;
    dfu)
      if [[ "$firmware_file" != *.zip ]]; then
        error "  DFU method requires a .zip DFU package (got: ${firmware_name})"
        (( errors++ ))
      else
        ok "  Extension .zip matches DFU method"
      fi
      ;;
    usb)
      if [[ "$firmware_file" != *.bin && "$firmware_file" != *.hex ]]; then
        error "  USB method requires a .bin or .hex file (got: ${firmware_name})"
        (( errors++ ))
      else
        ok "  Extension matches USB method"
      fi
      ;;
  esac

  # ── Step 2: Firmware revision vs PCB revision ──────────────────────────
  info "Check 2: Firmware revision vs PCB revision..."
  local fw_rev
  if fw_rev="$(extract_firmware_revision "$firmware_file")"; then
    info "  Detected firmware revision: ${BOLD}${fw_rev}${RESET}"

    if [[ "$fw_rev" == "$pcb_rev" ]]; then
      ok "  Firmware revision matches PCB revision ✓"
    else
      # Check if they're at least the same board family
      local fw_family pcb_family
      fw_family="$(get_pcb_board_family "$fw_rev")"
      pcb_family="$(get_pcb_board_family "$pcb_rev")"

      if [[ "$fw_family" != "$pcb_family" ]]; then
        error "══════════════════════════════════════════════════════════"
        error "  BOARD FAMILY MISMATCH!"
        error ""
        error "  Firmware: ${fw_rev} ($(get_pcb_label "$fw_rev"))"
        error "  Your PCB: ${pcb_rev} (${pcb_label})"
        error ""
        error "  These are DIFFERENT PCB boards with different GPIO pin"
        error "  assignments. Flashing will cause hardware malfunction!"
        error "══════════════════════════════════════════════════════════"
        (( errors++ ))
      else
        # Same board family but different variant — detailed warnings
        local fw_chip pcb_chip fw_led pcb_led
        fw_chip="$(get_pcb_chip "$fw_rev")"
        pcb_chip="$(get_pcb_chip "$pcb_rev")"
        fw_led="$(get_pcb_led "$fw_rev")"
        pcb_led="$(get_pcb_led "$pcb_rev")"

        if [[ "$fw_chip" != "$pcb_chip" ]]; then
          error "══════════════════════════════════════════════════════════"
          error "  CHIP TYPE MISMATCH!"
          error ""
          error "  Firmware: ${fw_rev} → built for ${fw_chip}"
          error "  Your PCB: ${pcb_rev} → has ${pcb_chip}"
          error ""
          if [[ "$pcb_chip" == "nrf52810" && "$fw_chip" == "nrf52832" ]]; then
            error "  nRF52832 firmware is LARGER and uses features not"
            error "  available on nRF52810. This will NOT boot!"
          else
            error "  nRF52810 firmware may run on 52832 but with reduced"
            error "  functionality and wasted flash space."
          fi
          error "══════════════════════════════════════════════════════════"
          (( errors++ ))
        fi

        if [[ "$fw_led" != "$pcb_led" ]]; then
          warn "═══════════════════════════════════════════════════════"
          warn "  LED CONFIGURATION MISMATCH!"
          warn ""
          if [[ "$fw_led" == "3led" ]]; then
            warn "  Firmware: ${fw_rev} → 3 separate status LEDs"
            warn "  Your PCB: ${pcb_rev} → single RGB LED"
          else
            warn "  Firmware: ${fw_rev} → single RGB LED"
            warn "  Your PCB: ${pcb_rev} → 3 separate status LEDs"
          fi
          warn ""
          warn "  The keyboard WILL work, but status LED indication"
          warn "  will be incorrect (wrong colors or only one LED lit)."
          warn "═══════════════════════════════════════════════════════"
          echo ""
          read -rp "  Continue despite LED mismatch? [y/N]: " cont
          [[ "$cont" =~ ^[Yy]$ ]] || (( errors++ ))
        fi
      fi
    fi
  else
    warn "  Could not detect revision from filename."
    warn "  Filename doesn't match pattern: lot60-ble-{rev}-{chip}-{date}-{version}.{ext}"
    warn "  Skipping revision check. Make sure you have the correct firmware!"
    echo ""
  fi

  # ── Step 3: Chip type vs flash method compatibility ────────────────────
  info "Check 3: Firmware chip type vs flash method..."
  local fw_chip_type
  if fw_chip_type="$(extract_firmware_chip "$firmware_file")"; then
    case "$method" in
      pyocd|dfu)
        if [[ "$fw_chip_type" == "ch554" ]]; then
          error "  Cannot flash CH554 firmware via ${method}."
          error "  CH554 is the USB controller chip — use --method usb instead."
          (( errors++ ))
        else
          ok "  nRF52 firmware matches ${method} method"
        fi
        ;;
      usb)
        if [[ "$fw_chip_type" == "nrf52" ]]; then
          error "  Cannot flash nRF52 firmware via USB/ISP method."
          error "  Use --method pyocd or --method dfu for nRF52 firmware."
          (( errors++ ))
        else
          ok "  CH554 firmware matches USB method"
        fi
        ;;
    esac
  fi

  # ── Step 4: pyocd DAP-Link availability ────────────────────────────────
  if [[ "$method" == "pyocd" ]]; then
    info "Check 4: CMSIS-DAP debugger support..."
    local has_dap
    has_dap="$(get_pcb_has_dap "$pcb_rev")"
    if [[ "$has_dap" == "yes" ]]; then
      ok "  PCB revision ${pcb_rev} has on-board CMSIS-DAP debugger"
    else
      warn "  PCB revision ${pcb_rev} does NOT have an on-board DAP debugger."
      warn "  You need an external J-Link or DAP-Link probe for pyocd."
      echo ""
      read -rp "  Do you have an external debugger? [y/N]: " cont
      [[ "$cont" =~ ^[Yy]$ ]] || (( errors++ ))
    fi
  fi

  # ── Step 5: Binary-level verification ──────────────────────────────────
  if [[ "$method" == "pyocd" && "$firmware_file" == *.hex ]]; then
    info "Check 5: HEX file binary verification..."
    local expected_chip
    expected_chip="$(get_pcb_chip "$pcb_rev")"
    if ! verify_hex_chip_compat "$firmware_file" "$expected_chip"; then
      (( errors++ ))
    fi
  fi

  if [[ "$method" == "usb" && "$firmware_file" == *.bin ]]; then
    info "Check 5: BIN file size verification..."
    if ! verify_bin_size "$firmware_file"; then
      (( errors++ ))
    fi
  fi

  # ── Final verdict ──────────────────────────────────────────────────────
  echo ""
  if (( errors > 0 )); then
    error "═══════════════════════════════════════════════════════════════"
    error "  COMPATIBILITY CHECK FAILED: ${errors} issue(s) found."
    error ""
    error "  Firmware: ${firmware_name}"
    error "  Target:   ${pcb_rev} (${pcb_label})"
    error ""
    error "  Flashing incompatible firmware can damage your keyboard."
    error "  Use --skip-compat to override (AT YOUR OWN RISK)."
    error "═══════════════════════════════════════════════════════════════"
    echo ""
    read -rp "  Override and flash anyway? (type 'YES' to confirm): " override
    if [[ "$override" != "YES" ]]; then
      die "Aborted. No changes were made."
    fi
    warn "Override accepted. Proceeding with flash..."
  else
    ok "All compatibility checks passed ✓"
  fi
  echo ""
}

# ─── Prerequisites check ─────────────────────────────────────────────────────
check_prerequisites() {
  banner "Prerequisites Check"

  local PYTHON_OK=false
  if check_tool python3; then
    PYTHON_OK=true
  elif check_tool python; then
    PYTHON_OK=true
  fi

  $PYTHON_OK || warn "Python3 not found — pyocd and nrfutil require Python."

  # macOS: check libusb
  if [[ "$PLATFORM" == "macOS" ]]; then
    if command -v brew &>/dev/null; then
      if brew list libusb &>/dev/null 2>&1; then
        ok "libusb found (Homebrew)"
      else
        warn "libusb may be needed by pyocd: brew install libusb"
      fi
    fi
  fi

  # Linux: check udev rules
  if [[ "$PLATFORM" == "Linux" ]]; then
    if [[ ! -f /etc/udev/rules.d/50-cmsis-dap.rules ]] && \
       [[ ! -f /lib/udev/rules.d/50-cmsis-dap.rules ]]; then
      warn "pyocd udev rules may not be installed."
      echo "  Install with: pyocd install-pack --list  (then install nrf52 pack)"
      echo "  Or manually: sudo python3 -m pyocd.udev"
    fi
  fi
}

# ─── Flash via pyocd (DAP-Link / on-board debugger) ──────────────────────────
flash_pyocd() {
  banner "Flash via pyocd (DAP-Link / On-board Debugger)"

  if ! check_tool pyocd; then
    hint_pyocd
    die "pyocd is required for this method."
  fi

  [[ -z "$FIRMWARE" ]] && die "No firmware file specified. Use --file <path.hex>"
  [[ -f "$FIRMWARE" ]] || die "Firmware file not found: $FIRMWARE"

  # Run compatibility check
  check_firmware_compatibility "$FIRMWARE" "$PCB_REVISION" "pyocd"

  info "Detecting connected DAP-Link devices..."
  pyocd list || die "pyocd list failed — is the keyboard plugged in and the debugger jumper connected?"

  echo ""
  info "Firmware: $FIRMWARE"

  local ERASE_FLAG=""
  if [[ "$ERASE" == true ]]; then
    warn "Erase flag set — all configuration data will be wiped!"
    ERASE_FLAG="--erase chip"
  fi

  local TARGET_CHIP
  TARGET_CHIP="$(get_pcb_chip "$PCB_REVISION")"
  local PYOCD_TARGET="nrf52"
  if [[ "$TARGET_CHIP" == "nrf52832" ]]; then
    PYOCD_TARGET="nrf52832"
  fi

  info "Flashing ${TARGET_CHIP} with pyocd (target: ${PYOCD_TARGET})..."
  # shellcheck disable=SC2086
  pyocd flash -t "${PYOCD_TARGET}" $ERASE_FLAG "$FIRMWARE"

  ok "Flash complete!"
  info "Disconnect and reconnect battery + USB to reboot the keyboard."

  echo ""
  echo -e "${YELLOW}Note (Multi-device support):${RESET}"
  echo "  After flashing, switch BLE devices with: Space + U + Q / W / E"
  echo "  (up to 3 paired devices)"
}

# ─── Flash via Bluetooth DFU (nrfutil) ───────────────────────────────────────
flash_dfu() {
  banner "Flash via Bluetooth DFU"

  echo "DFU can be done in two ways:"
  echo "  A) nrfutil (command-line, BLE required) — requires DfuTarg device visible"
  echo "  B) nRF Connect mobile app (iOS/Android)  — manual, no extra tools"
  echo ""
  read -rp "Use nrfutil CLI [A] or get app instructions [B]? [A/b]: " dfu_choice

  if [[ "$dfu_choice" =~ ^[Bb]$ ]]; then
    echo ""
    echo -e "${YELLOW}Manual DFU via nRF Connect app:${RESET}"
    echo "  1. Put keyboard into DFU mode:"
    echo "       • Sleep the keyboard (Fn0 + CapsLock)"
    echo "       • Wake with Space + B  →  BLE shows 'DFUTarg'"
    echo "       • Rev.G: press RESET button (enters DFU automatically)"
    echo "  2. Install 'nRF Connect' on iOS or Android"
    echo "     https://www.nordicsemi.com/Software-and-tools/Development-Tools/nRF-Connect-for-mobile"
    echo "  3. Scan → connect to 'DFUTarg' → tap DFU button"
    echo "  4. Choose 'Distribution packet (ZIP)' and select your .zip firmware"
    echo "  5. Wait for progress bar to reach 100%"
    echo ""
    echo -e "${YELLOW}Important:${RESET} After DFU, delete the pairing on your host and re-pair."
    return 0
  fi

  # CLI nrfutil path
  if ! check_tool nrfutil; then
    hint_nrfutil
    die "nrfutil is required for CLI DFU."
  fi

  [[ -z "$FIRMWARE" ]] && die "No firmware file specified. Use --file <path.zip>"
  [[ -f "$FIRMWARE" ]] || die "Firmware file not found: $FIRMWARE"

  # Run compatibility check
  check_firmware_compatibility "$FIRMWARE" "$PCB_REVISION" "dfu"

  echo ""
  echo -e "${YELLOW}Before proceeding — put the keyboard into DFU mode:${RESET}"
  echo "  Method 1: Sleep keyboard → wake with Space + B"
  echo "  Method 2: Use GPIO0 + GND jumper + RESET (hardware recovery)"
  echo "  The device 'DFUTarg' should appear in BLE scan."
  echo ""
  read -rp "Press Enter when DFUTarg is visible in BLE scan..." _

  if [[ -n "$DFU_DEVICE" ]]; then
    info "Using specified device: $DFU_DEVICE"
    nrfutil dfu ble -pkg "$FIRMWARE" -a "$DFU_DEVICE"
  else
    info "Scanning for DFUTarg..."
    nrfutil dfu ble -pkg "$FIRMWARE" -n "DFUTarg"
  fi

  ok "DFU complete!"
  echo ""
  echo -e "${YELLOW}Note:${RESET} Delete and re-pair BLE devices after firmware update."
}

# ─── Flash CH554 USB firmware (Linux via usbisp) ─────────────────────────────
flash_usb() {
  banner "Flash CH554 USB Firmware"

  [[ "$PLATFORM" == "macOS" ]] && {
    warn "CH554 USB firmware flashing via usbisp is Linux-only."
    echo ""
    echo "On macOS options:"
    echo "  1. Use the official WCH flash tool on a Windows machine"
    echo "     http://www.wch.cn/downloads/WCHISPTool_Setup_exe.html"
    echo "  2. Use a Linux VM or dual-boot"
    echo "  3. Use the Lotlab Flash Toolbox (Windows only)"
    echo "     https://dl.lotlab.org/keyboard/flash-toolbox/"
    echo ""
    return 0
  }

  [[ -z "$FIRMWARE" ]] && die "No firmware file specified. Use --file <path.bin>"
  [[ -f "$FIRMWARE" ]] || die "Firmware file not found: $FIRMWARE"

  # Run compatibility check
  check_firmware_compatibility "$FIRMWARE" "$PCB_REVISION" "usb"

  if ! check_tool usbisp; then
    hint_usbisp
    die "usbisp not found. Install librech551 first."
  fi

  echo ""
  echo -e "${YELLOW}Enter USB ISP mode on the keyboard:${RESET}"
  echo "  1. Disconnect the USB cable from the keyboard"
  echo "  2. Hold the ISP button (K1, on the PCB back)"
  echo "  3. Reconnect the USB cable while holding ISP"
  echo "  4. Release the button after the device is detected"
  echo ""
  read -rp "Press Enter once the keyboard is in ISP mode..." _

  info "Flashing CH554 USB firmware: $FIRMWARE"
  usbisp "$FIRMWARE"

  ok "USB firmware flash complete!"
  info "Disconnect and reconnect USB to exit ISP mode."
}

# ─── Main ────────────────────────────────────────────────────────────────────
banner "Lot60-BLE Firmware Flash Tool v${SCRIPT_VERSION}"
echo -e "  PCB      : ${BOLD}${PCB_REVISION}${RESET} ($(get_pcb_label "$PCB_REVISION"))"
echo -e "  Platform : ${BOLD}${PLATFORM}${RESET}"
echo -e "  Fork     : https://github.com/trinhquocviet/lotlab-nrf52-keyboard"
echo -e "  Feature  : Multi-device switch (Space+U+Q/W/E, up to 3 devices)"
echo ""

check_prerequisites

# Pick method if not specified
[[ -z "$METHOD" ]] && pick_method

# Prompt for firmware file if not given
if [[ -z "$FIRMWARE" ]]; then
  echo ""
  case "$METHOD" in
    pyocd) read -rp "Enter path to .hex firmware file: " FIRMWARE ;;
    dfu)   read -rp "Enter path to .zip DFU package: "   FIRMWARE ;;
    usb)   read -rp "Enter path to .bin USB firmware: "  FIRMWARE ;;
  esac
fi

# Execute
case "$METHOD" in
  pyocd) flash_pyocd ;;
  dfu)   flash_dfu ;;
  usb)   flash_usb ;;
  *) die "Unknown method: $METHOD. Use pyocd, dfu, or usb." ;;
esac

echo ""
ok "Done."
