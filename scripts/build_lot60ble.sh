#!/usr/bin/env bash
# =============================================================================
#  build_lot60ble.sh — Lot60-BLE Firmware Build Script
#  Separated from flash_lot60ble.sh for clean build/flash workflow.
#
#  Repo: https://github.com/trinhquocviet/lotlab-nrf52-keyboard
#  Wiki: https://wiki.lotlab.org/ble/lot60-ble.html
#
#  Builds firmware for all Lot60-BLE PCB revisions:
#    rev_c, rev_e, rev_e_3led, rev_f, rev_f_3led,
#    rev_g, rev_g_3led, rev_g_832
#
#  Output files per revision:
#    - lot60-ble-{rev}-nrf52-{date}-{version}.hex  (nRF52, merged with SoftDevice)
#    - lot60-ble-{rev}-nrf52-{date}-{version}.zip  (nRF52, DFU package)
#    - lot60-ble-{rev}-ch554-{date}-{version}.bin  (CH554 USB firmware)
# =============================================================================

set -euo pipefail

# ─── Colour helpers ──────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'

info()    { echo -e "${CYAN}[INFO]${RESET}  $*"; }
ok()      { echo -e "${GREEN}[OK]${RESET}    $*"; }
warn()    { echo -e "${YELLOW}[WARN]${RESET}  $*"; }
error()   { echo -e "${RED}[ERROR]${RESET} $*" >&2; }
die()     { error "$*"; exit 1; }
banner()  { echo -e "\n${BOLD}${CYAN}=== $* ===${RESET}\n"; }

# ─── Usage ───────────────────────────────────────────────────────────────────
usage() {
  cat <<EOF
${BOLD}Usage:${RESET}
  $0 [OPTIONS]

${BOLD}Description:${RESET}
  Build Lot60-BLE firmware from source. Requires the ARM GCC toolchain,
  SDCC, nRF5 SDK, and nrfutil to be set up (see the repo README).

${BOLD}Options:${RESET}
  -k, --keyboard <name>    Keyboard target dir (default: lot60-ble)
  -r, --revision <rev>     Build only a specific revision subdirectory
                            e.g. rev_g, rev_g_3led, rev_g_832, rev_c, ...
                            (default: build ALL revisions)
  -j, --jobs <N>            Parallel make jobs (default: auto-detect)
  -o, --output <dir>        Output directory (default: _build)
  -D, --docker              Build using Docker image (lotlab/nrf52-keyboard)
  -h, --help                Show this help

${BOLD}Examples:${RESET}
  # Build all revisions
  $0

  # Build only Rev.G single-LED (for Rev.GX PCB with 1 RGB LED)
  $0 --revision rev_g

  # Build only Rev.G 3-LED variant
  $0 --revision rev_g_3led

  # Build using Docker
  $0 --docker

EOF
  exit 0
}

# ─── Defaults ────────────────────────────────────────────────────────────────
KEYBOARD="lot60-ble"
REVISION=""
JOBS=""
OUTPUT_DIR=""
USE_DOCKER=false
REPO_ROOT=""

# ─── Parse args ──────────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
  case $1 in
    -k|--keyboard)  KEYBOARD="$2";    shift 2 ;;
    -r|--revision)  REVISION="$2";    shift 2 ;;
    -j|--jobs)      JOBS="$2";        shift 2 ;;
    -o|--output)    OUTPUT_DIR="$2";  shift 2 ;;
    -D|--docker)    USE_DOCKER=true;  shift ;;
    -h|--help)      usage ;;
    *) die "Unknown argument: $1. Run '$0 --help' for usage." ;;
  esac
done

# ─── Detect OS ───────────────────────────────────────────────────────────────
OS="$(uname -s)"
case "$OS" in
  Darwin) PLATFORM="macOS" ;;
  Linux)  PLATFORM="Linux" ;;
  *) die "Unsupported platform: $OS. This script supports macOS and Linux." ;;
esac
info "Platform: $PLATFORM"

# ─── Auto-detect parallel jobs ───────────────────────────────────────────────
if [[ -z "$JOBS" ]]; then
  JOBS="$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
fi

# ─── Find repo root ─────────────────────────────────────────────────────────
find_repo_root() {
  local dir="${PWD}"
  while [[ "$dir" != "/" ]]; do
    if [[ -d "$dir/keyboard/${KEYBOARD}" ]] || [[ -f "$dir/Dockerfile" && -d "$dir/application" ]]; then
      echo "$dir"
      return 0
    fi
    dir="$(dirname "$dir")"
  done
  return 1
}

if REPO_ROOT="$(find_repo_root 2>/dev/null)"; then
  info "Repo root detected: $REPO_ROOT"
else
  warn "Could not auto-detect repo root. Assuming current directory."
  REPO_ROOT="${PWD}"
fi

# ─── Docker build ────────────────────────────────────────────────────────────
if [[ "$USE_DOCKER" == true ]]; then
  banner "Building with Docker"

  command -v docker &>/dev/null || die "'docker' not found in PATH."

  info "Building Docker image..."
  docker build -t lotlab/nrf52-keyboard "$REPO_ROOT"

  info "Running build inside Docker container..."
  if [[ -n "$REVISION" ]]; then
    docker run --rm -v "${REPO_ROOT}/keyboard/${KEYBOARD}/_build:/work/keyboard/${KEYBOARD}/_build" \
      lotlab/nrf52-keyboard \
      -c "cd keyboard/${KEYBOARD} && make ${REVISION} -j${JOBS}"
  else
    docker run --rm -v "${REPO_ROOT}/keyboard/${KEYBOARD}/_build:/work/keyboard/${KEYBOARD}/_build" \
      lotlab/nrf52-keyboard \
      -c "cd keyboard/${KEYBOARD} && make -j${JOBS}"
  fi

  ok "Docker build complete."
  info "Output in: ${REPO_ROOT}/keyboard/${KEYBOARD}/_build/"
  exit 0
fi

# ─── Prerequisites check ────────────────────────────────────────────────────
banner "Build Prerequisites Check"

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

# Check make
check_tool make || die "'make' is required."

# Check ARM GCC
if ! check_tool arm-none-eabi-gcc; then
  warn "arm-none-eabi-gcc not found. Firmware build will likely fail."
  warn "Install gcc-arm-none-eabi-7-2018-q2-update and set GNU_INSTALL_ROOT"
  echo ""
  if [[ "$PLATFORM" == "macOS" ]]; then
    echo "  brew install --cask gcc-arm-embedded"
  else
    echo "  sudo apt install gcc-arm-none-eabi  # or download from ARM"
  fi
  echo ""
  read -rp "Continue anyway? [y/N]: " cont
  [[ "$cont" =~ ^[Yy]$ ]] || exit 1
fi

# Check SDCC (for CH554 USB firmware)
if ! check_tool sdcc; then
  warn "sdcc not found. CH554 USB firmware will not be built."
  warn "Install SDCC 4.0.0 for CH554 compilation."
  echo ""
  if [[ "$PLATFORM" == "macOS" ]]; then
    echo "  brew install sdcc"
  else
    echo "  # Download from https://sourceforge.net/projects/sdcc/"
  fi
  echo ""
fi

# Check nrfutil (for DFU package generation)
if ! check_tool nrfutil; then
  warn "nrfutil not found. DFU packages (.zip) will not be generated."
  echo "  pip3 install nrfutil"
  echo ""
fi

# Check mergehex (for merged SoftDevice+app hex)
if ! check_tool mergehex; then
  warn "mergehex not found. Merged SoftDevice+application .hex files will not be generated."
  echo "  mergehex is part of Nordic nRF Command Line Tools"
  echo "  https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools"
  echo ""
fi

# ─── Build firmware ─────────────────────────────────────────────────────────
banner "Building Firmware"

KBD_DIR="${REPO_ROOT}/keyboard/${KEYBOARD}"
[[ -d "$KBD_DIR" ]] || die "Keyboard directory not found: $KBD_DIR"

info "Keyboard directory: $KBD_DIR"
cd "$KBD_DIR"

# Pass output directory if specified
MAKE_ARGS=""
if [[ -n "$OUTPUT_DIR" ]]; then
  MAKE_ARGS="OUTPUT_DIRECTORY=${OUTPUT_DIR}"
fi

if [[ -n "$REVISION" ]]; then
  # Validate revision exists
  if [[ ! -d "${KBD_DIR}/${REVISION}" ]]; then
    die "Revision directory not found: ${KBD_DIR}/${REVISION}"
    echo ""
    echo "Available revisions:"
    for d in "${KBD_DIR}"/rev_*; do
      [[ -d "$d" ]] && echo "  $(basename "$d")"
    done
  fi

  info "Building single revision: ${REVISION}"
  info "Running: make ${REVISION} -j${JOBS} ${MAKE_ARGS}"
  # shellcheck disable=SC2086
  make "${REVISION}" -j"${JOBS}" ${MAKE_ARGS}
else
  info "Building ALL revisions..."
  info "Running: make -j${JOBS} ${MAKE_ARGS}"
  # shellcheck disable=SC2086
  make -j"${JOBS}" ${MAKE_ARGS}
fi

ok "Build complete!"

# ─── List output ─────────────────────────────────────────────────────────────
banner "Build Output"

BUILD_DIR="${KBD_DIR}/_build"
if [[ -n "$OUTPUT_DIR" ]]; then
  BUILD_DIR="${OUTPUT_DIR}"
fi

if [[ -d "$BUILD_DIR" ]]; then
  info "Files in ${BUILD_DIR}:"
  echo ""

  # List hex files
  for f in "${BUILD_DIR}"/*.hex "${BUILD_DIR}"/**/*.hex; do
    [[ -f "$f" ]] && echo -e "  ${GREEN}HEX${RESET}  $f"
  done 2>/dev/null

  # List zip files
  for f in "${BUILD_DIR}"/*.zip "${BUILD_DIR}"/**/*.zip; do
    [[ -f "$f" ]] && echo -e "  ${CYAN}ZIP${RESET}  $f"
  done 2>/dev/null

  # List bin files
  for f in "${BUILD_DIR}"/*.bin "${BUILD_DIR}"/**/*.bin; do
    [[ -f "$f" ]] && echo -e "  ${YELLOW}BIN${RESET}  $f"
  done 2>/dev/null

  echo ""
else
  warn "Build directory not found: $BUILD_DIR"
fi

# ─── Revision reference ─────────────────────────────────────────────────────
banner "Revision Reference"
echo -e "  ${BOLD}rev_c${RESET}       → Rev.C PCB, nRF52810, CMSIS-DAP, single RGB LED"
echo -e "  ${BOLD}rev_e${RESET}       → Rev.E PCB, nRF52810, single RGB LED"
echo -e "  ${BOLD}rev_e_3led${RESET}  → Rev.E PCB, nRF52810, 3 status LEDs"
echo -e "  ${BOLD}rev_f${RESET}       → Rev.F PCB, nRF52810, single RGB LED"
echo -e "  ${BOLD}rev_f_3led${RESET}  → Rev.F PCB, nRF52810, 3 status LEDs"
echo -e "  ${BOLD}rev_g${RESET}       → Rev.G/GX PCB, nRF52810, CMSIS-DAP, single RGB LED"
echo -e "  ${BOLD}rev_g_3led${RESET}  → Rev.G/GX PCB, nRF52810, CMSIS-DAP, 3 status LEDs"
echo -e "  ${BOLD}rev_g_832${RESET}   → Rev.G/GX PCB, nRF52832 variant, CMSIS-DAP, single RGB LED"
echo ""
echo -e "  ${YELLOW}Your Rev.GX with single LED → use ${BOLD}rev_g${RESET}"
echo ""

ok "Done. Use flash_lot60ble.sh to flash the built firmware."
