#!/usr/bin/env bash
# Builds the portable core once and runs every host test with CTest.
# Usage: tests/run_host_tests.sh [--sanitize]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SAN=OFF
BUILD="${HOST_BUILD_DIR:-$ROOT/build/host}"
if [[ "${1:-}" == "--sanitize" ]]; then SAN=ON; BUILD="${BUILD}-asan"; fi
GEN=()
command -v ninja >/dev/null 2>&1 && GEN=(-G Ninja)
cmake -S "$ROOT/tests" -B "$BUILD" "${GEN[@]}" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DFXAI_SANITIZE=$SAN >/dev/null
cmake --build "$BUILD" --parallel "${JOBS:-4}"
ctest --test-dir "$BUILD" --output-on-failure
echo "PicoCalc FX host test suite: PASS (sanitizers: $SAN)"
