#!/usr/bin/env bash
# Builds the alternative Raspberry Pi Pico 2 W profile (v1.8.0 command, kept for
# compatibility). Equivalent to: ./scripts/build.sh --board pico2w [options]
set -euo pipefail
exec "$(cd "$(dirname "$0")" && pwd)/build.sh" --board pico2w "$@"
