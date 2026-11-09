#!/usr/bin/env bash
# Native host test suite (no Pico SDK needed).
#
#   ./tests/run_host_tests.sh              # Pico 2 W profile (default)
#   FXAI_TEST_BOARD=PICO_PLUS_2W ./tests/run_host_tests.sh
#   FXAI_TEST_SANITIZE=1 ./tests/run_host_tests.sh   # ASan + UBSan
#
# Every portable source is compiled once into a static library; each smoke
# test links against it. The board profile (limits, PSRAM capability) follows
# FXAI_TEST_BOARD; the memory pools are simulated by the tests themselves.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BOARD="${FXAI_TEST_BOARD:-PICO2W}"
OUT="${TMPDIR:-/tmp}/picocalc_fxai_host_${BOARD}"
CXX="${CXX:-g++}"
CC="${CC:-gcc}"
mkdir -p "$OUT/obj"

case "$BOARD" in
  PICO2W) DEFS=(-DFXAI_BOARD_PICO2W=1) ;;
  PICO_PLUS_2W) DEFS=(-DFXAI_BOARD_PICO_PLUS_2W=1) ;;
  *) echo "FXAI_TEST_BOARD must be PICO2W or PICO_PLUS_2W" >&2; exit 2 ;;
esac
OPT=(-O2)
SAN=()
if [[ "${FXAI_TEST_SANITIZE:-0}" == "1" ]]; then
  OPT=(-O1 -g -fno-omit-frame-pointer)
  SAN=(-fsanitize=address,undefined -fno-sanitize-recover=undefined)
fi
CXXFLAGS=(-std=c++20 "${OPT[@]}" "${SAN[@]}" -Wall -Wextra -Werror "${DEFS[@]}" -I"$ROOT/include" -I"$ROOT/third_party/littlefs" -I"$ROOT/tests")
CFLAGS=(-std=c11 "${OPT[@]}" "${SAN[@]}" -DLFS_NO_DEBUG -DLFS_NO_ASSERT -I"$ROOT/third_party/littlefs")

CORE=(natural_editor keyboard_input run_matrix point_a math_engine expression graph graph_renderer
      graphics_engine memory editors font_renderer math_renderer widget_system animation_system
      window_manager tlsf mem_pools limits psram system_runtime flash_fs graph3d system_info
      diagnostics sd_card ai_client bt_support)
OBJS=()
for src in "${CORE[@]}"; do
  "$CXX" "${CXXFLAGS[@]}" -c "$ROOT/src/$src.cpp" -o "$OUT/obj/$src.o"
  OBJS+=("$OUT/obj/$src.o")
done
for src in lfs lfs_util; do
  "$CC" "${CFLAGS[@]}" -c "$ROOT/third_party/littlefs/$src.c" -o "$OUT/obj/$src.o"
  OBJS+=("$OUT/obj/$src.o")
done
rm -f "$OUT/libfxcore.a"
ar rcs "$OUT/libfxcore.a" "${OBJS[@]}"

# UI translation unit: compile-only check (it needs the device board layer to link).
"$CXX" "${CXXFLAGS[@]}" -c "$ROOT/src/ui.cpp" -o "$OUT/obj/ui_check.o"

run_test() {
  local name="$1"; shift
  "$CXX" "${CXXFLAGS[@]}" "$@" "$OUT/libfxcore.a" -pthread -o "$OUT/$name"
  "$OUT/$name"
}

run_test math_smoke "$ROOT/tests/math_smoke.cpp"
run_test graphics_smoke "$ROOT/tests/graphics_smoke.cpp"
run_test font_smoke "$ROOT/tests/font_smoke.cpp"
run_test math_renderer_smoke "$ROOT/tests/math_renderer_smoke.cpp"
run_test graph_renderer_smoke "$ROOT/tests/graph_renderer_smoke.cpp"
run_test launcher_smoke "$ROOT/tests/launcher_smoke.cpp"
run_test widget_smoke "$ROOT/tests/widget_smoke.cpp"
run_test animation_smoke "$ROOT/tests/animation_smoke.cpp"
run_test cheatsheet_smoke "$ROOT/tests/cheatsheet_smoke.cpp"
run_test window_manager_smoke "$ROOT/tests/window_manager_smoke.cpp"
run_test vpam_ast_smoke "$ROOT/tests/vpam_ast_smoke.cpp"
# v1.9.0 memory architecture
run_test mem_pools_smoke "$ROOT/tests/mem_pools_smoke.cpp"
run_test mem_new_smoke "$ROOT/tests/mem_new_smoke.cpp" "$ROOT/src/mem_new.cpp"
run_test psram_selftest_smoke "$ROOT/tests/psram_selftest_smoke.cpp"
run_test limits_smoke "$ROOT/tests/limits_smoke.cpp"
run_test oom_smoke "$ROOT/tests/oom_smoke.cpp" "$ROOT/src/mem_new.cpp"
run_test flash_fs_smoke "$ROOT/tests/flash_fs_smoke.cpp"
run_test graphics_memory_smoke "$ROOT/tests/graphics_memory_smoke.cpp"
run_test system_app_smoke "$ROOT/tests/system_app_smoke.cpp"

echo "PicoCalc FX AI host suite ($BOARD profile): PASS"
