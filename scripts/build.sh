#!/usr/bin/env bash
# Build the PicoCalc FX-CG50 AI firmware for one board.
#
#   ./scripts/build.sh --board pico_plus_2w    # Pimoroni Pico Plus 2 W (PSRAM 8 MB, flash 16 MB; default)
#   ./scripts/build.sh --board pico2w          # Raspberry Pi Pico 2 W  (alternative profile)
#
# Options:
#   --board <pico2w|pico_plus_2w>   target module
#   --opt <O2|O3|Os>                optimisation level (default: O2)
#   --lto                           enable link-time optimisation (experimental)
#   --fb-psram                      framebuffer in PSRAM (Pico Plus 2 W only)
#   --bt                            enable Bluetooth (pico_btstack)
#   --clean                         remove the build directory first
#   -D<VAR>=<VALUE>                 forwarded to CMake
#
# Each board has its own build directory (build/<board>) and its own UF2 name
# (picocalc_fxai_<board>.uf2): UF2 images are NOT interchangeable between boards.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

BOARD="pico_plus_2w"
CLEAN=0
EXTRA=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --board) BOARD="${2:-}"; shift 2 ;;
    --board=*) BOARD="${1#*=}"; shift ;;
    --opt) EXTRA+=("-DFXAI_OPT_LEVEL=${2:-}"); shift 2 ;;
    --lto) EXTRA+=("-DFXAI_ENABLE_LTO=ON"); shift ;;
    --fb-psram) EXTRA+=("-DFXAI_FRAMEBUFFER_IN_PSRAM=ON"); shift ;;
    --bt) EXTRA+=("-DFXAI_ENABLE_BT=ON"); shift ;;
    --clean) CLEAN=1; shift ;;
    -D*) EXTRA+=("$1"); shift ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "ERROR: unknown argument '$1' (see --help)" >&2; exit 2 ;;
  esac
done

case "$BOARD" in
  pico2w|pico2_w|PICO2W) BOARD="pico2w"; FXAI_BOARD="PICO2W" ;;
  pico_plus_2w|pico-plus-2w|picoplus2w|PICO_PLUS_2W) BOARD="pico_plus_2w"; FXAI_BOARD="PICO_PLUS_2W" ;;
  *) echo "ERROR: --board must be pico2w or pico_plus_2w (got '$BOARD')" >&2; exit 2 ;;
esac

: "${PICO_SDK_PATH:?Set PICO_SDK_PATH to a Raspberry Pi Pico SDK (>= 2.1) checkout}"
if [[ ! -f "$PICO_SDK_PATH/external/pico_sdk_import.cmake" ]]; then
  echo "ERROR: PICO_SDK_PATH does not point to a Pico SDK checkout." >&2
  exit 2
fi
for tool in cmake arm-none-eabi-gcc arm-none-eabi-g++; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: $tool is not installed or not on PATH." >&2
    exit 2
  fi
done
if [[ ! -f "$PICO_SDK_PATH/src/boards/include/boards/pimoroni_pico_plus2_w_rp2350.h" ]]; then
  echo "ERROR: this Pico SDK has no pimoroni_pico_plus2_w_rp2350 board header (need SDK >= 2.1)." >&2
  exit 2
fi

SDK_REV="unknown"
if git -C "$PICO_SDK_PATH" rev-parse --short HEAD >/dev/null 2>&1; then
  SDK_REV="$(git -C "$PICO_SDK_PATH" describe --tags --always --dirty 2>/dev/null || echo unknown)"
fi
BUILD_DIR="${PICO_BUILD_DIR:-$ROOT/build/$BOARD}"
GENERATOR=()
if command -v ninja >/dev/null 2>&1 && [[ ! -f "$BUILD_DIR/Makefile" ]]; then
  GENERATOR=(-G Ninja)
fi
if [[ "$CLEAN" == "1" ]]; then rm -rf "$BUILD_DIR"; fi

printf 'PicoCalc FX-CG50 AI build\n  Board: %s (FXAI_BOARD=%s)\n  SDK: %s (%s)\n  Build dir: %s\n' \
  "$BOARD" "$FXAI_BOARD" "$PICO_SDK_PATH" "$SDK_REV" "$BUILD_DIR"

cmake -S "$ROOT" -B "$BUILD_DIR" "${GENERATOR[@]}" \
  -DPICO_SDK_PATH="$PICO_SDK_PATH" \
  -DFXAI_BOARD="$FXAI_BOARD" \
  -DCMAKE_BUILD_TYPE=Release \
  "${EXTRA[@]}"
cmake --build "$BUILD_DIR" --parallel "${JOBS:-$(nproc 2>/dev/null || echo 2)}"

UF2="$BUILD_DIR/picocalc_fxai_${BOARD}.uf2"
ELF="$BUILD_DIR/picocalc_fxai_${BOARD}.elf"
if [[ ! -s "$UF2" ]]; then
  echo "ERROR: build completed but UF2 is missing or empty: $UF2" >&2
  exit 1
fi
if command -v arm-none-eabi-size >/dev/null 2>&1; then
  arm-none-eabi-size "$ELF"
fi
# Record the sources this UF2 was built from, for scripts/make_release.sh: the
# commit when the firmware sources match it, "dirty" otherwise, and the hash of
# the UF2 (a later rebuild without this script invalidates the record). File
# dates cannot be used: the Pico SDK touches CMakeLists.txt on every build when
# it compiles picotool from source (Findpicotool.cmake).
STATE="$BUILD_DIR/fxai_source_state"
SOURCES=(src include cmake CMakeLists.txt certs VERSION third_party)
if git -C "$ROOT" rev-parse --verify -q HEAD >/dev/null 2>&1; then
  if git -C "$ROOT" diff --quiet HEAD -- "${SOURCES[@]}" &&
     [[ -z "$(git -C "$ROOT" ls-files --others --exclude-standard -- "${SOURCES[@]}")" ]]; then
    COMMIT="$(git -C "$ROOT" rev-parse HEAD)"
  else
    COMMIT="dirty"
  fi
  printf '%s %s\n' "$COMMIT" "$(sha256sum "$UF2" | cut -d' ' -f1)" > "$STATE"
else
  rm -f "$STATE"
fi
echo "SUCCESS: $UF2"
echo "Flash: hold BOOTSEL while connecting USB, then copy the UF2 to the RP2350 drive."
