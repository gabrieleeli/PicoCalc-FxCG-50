#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${PICO_SDK_PATH:?Set PICO_SDK_PATH to the Raspberry Pi Pico SDK checkout}"

if [[ ! -f "$PICO_SDK_PATH/external/pico_sdk_import.cmake" ]]; then
  echo "ERROR: PICO_SDK_PATH does not point to a Pico SDK checkout." >&2
  exit 2
fi
if ! command -v cmake >/dev/null 2>&1; then
  echo "ERROR: cmake is not installed." >&2
  exit 2
fi
if ! command -v arm-none-eabi-g++ >/dev/null 2>&1; then
  echo "ERROR: arm-none-eabi-g++ is not installed or not on PATH." >&2
  exit 2
fi

SDK_REV="unknown"
if git -C "$PICO_SDK_PATH" rev-parse --short HEAD >/dev/null 2>&1; then
  SDK_REV="$(git -C "$PICO_SDK_PATH" describe --tags --always --dirty)"
fi
BUILD_DIR="${PICO_BUILD_DIR:-$ROOT/build/pico2w}"
printf 'PicoCalc FX-CG50 AI build\n  SDK: %s\n  SDK revision: %s\n  Build dir: %s\n' \
  "$PICO_SDK_PATH" "$SDK_REV" "$BUILD_DIR"

cmake -S "$ROOT" -B "$BUILD_DIR" \
  -DPICO_SDK_PATH="$PICO_SDK_PATH" \
  -DPICO_BOARD=pico2_w \
  -DCMAKE_BUILD_TYPE=Release \
  -DFXAI_DISPLAY_CALIBRATION_SCREEN=OFF
cmake --build "$BUILD_DIR" --parallel "${JOBS:-2}"

UF2="$BUILD_DIR/picocalc_fxai.uf2"
if [[ ! -s "$UF2" ]]; then
  echo "ERROR: build completed but UF2 is missing or empty: $UF2" >&2
  exit 1
fi
echo "SUCCESS: $UF2"
