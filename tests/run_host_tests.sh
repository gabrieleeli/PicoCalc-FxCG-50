#!/usr/bin/env bash
# Builds the portable core and runs every host test with CTest, for both board
# profiles (Pimoroni Pico Plus 2 W, the main target, and Raspberry Pi Pico 2 W).
# Usage: tests/run_host_tests.sh [--sanitize] [--tsan] [--board PICO_PLUS_2W|PICO2W]
#   --sanitize  AddressSanitizer + UndefinedBehaviorSanitizer
#   --tsan      adds the ThreadSanitizer build of the two-thread allocator test
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SAN=OFF
TSAN=OFF
BOARDS=(PICO_PLUS_2W PICO2W)
while [[ $# -gt 0 ]]; do
  case "$1" in
    --sanitize) SAN=ON; shift ;;
    --tsan) TSAN=ON; shift ;;
    --board) BOARDS=("${2:?--board needs PICO_PLUS_2W or PICO2W}"); shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done
GEN=()
command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)
for board in "${BOARDS[@]}"; do
  BUILD="${HOST_BUILD_DIR:-$ROOT/build/host}-$(echo "$board" | tr '[:upper:]' '[:lower:]')"
  [[ "$SAN" == "ON" ]] && BUILD="${BUILD}-asan"
  cmake -S "$ROOT/tests" -B "$BUILD" "${GEN[@]}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFXAI_SANITIZE=$SAN \
        -DFXAI_TSAN=OFF -DFXAI_TEST_BOARD="$board" >/dev/null
  cmake --build "$BUILD" --parallel "${JOBS:-4}"
  ctest --test-dir "$BUILD" --output-on-failure
  echo "PicoCalc FX host test suite ($board): PASS (sanitizers: $SAN)"
  if [[ "$TSAN" == "ON" ]]; then
    # ThreadSanitizer cannot share a build with ASan: dedicated directory.
    TBUILD="${HOST_BUILD_DIR:-$ROOT/build/host}-$(echo "$board" | tr '[:upper:]' '[:lower:]')-tsan"
    cmake -S "$ROOT/tests" -B "$TBUILD" "${GEN[@]}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFXAI_SANITIZE=OFF \
          -DFXAI_TSAN=ON -DFXAI_TEST_BOARD="$board" >/dev/null
    cmake --build "$TBUILD" --parallel "${JOBS:-4}" --target mem_pools_tsan
    ctest --test-dir "$TBUILD" --output-on-failure -R mem_pools_tsan
    echo "PicoCalc FX ThreadSanitizer allocator test ($board): PASS"
  fi
done
