#!/usr/bin/env bash
# Backward-compatible alias: builds the Raspberry Pi Pico 2 W firmware.
# Equivalent to: ./scripts/build.sh --board pico2w [extra options]
set -euo pipefail
exec "$(cd "$(dirname "$0")" && pwd)/build.sh" --board pico2w "$@"
