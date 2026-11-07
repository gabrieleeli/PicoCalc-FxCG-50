#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/picocalc_fxai_math_tests"
CXX="${CXX:-g++}"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror \
  -I"$ROOT/include" \
  "$ROOT/src/natural_editor.cpp" "$ROOT/src/keyboard_input.cpp" "$ROOT/src/run_matrix.cpp" "$ROOT/src/point_a.cpp" "$ROOT/src/math_engine.cpp" "$ROOT/src/expression.cpp" \
  "$ROOT/src/graph.cpp" "$ROOT/src/graph_renderer.cpp" "$ROOT/src/graphics_engine.cpp" "$ROOT/src/memory.cpp" "$ROOT/src/editors.cpp" \
  "$ROOT/tests/math_smoke.cpp" -o "$OUT"
"$OUT"
GFX_OUT="${TMPDIR:-/tmp}/picocalc_fxai_graphics_tests"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/graphics_engine.cpp" "$ROOT/tests/graphics_smoke.cpp" -o "$GFX_OUT"
"$GFX_OUT"
FONT_OUT="${TMPDIR:-/tmp}/picocalc_fxai_font_tests"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/graphics_engine.cpp" "$ROOT/src/font_renderer.cpp" "$ROOT/tests/font_smoke.cpp" -o "$FONT_OUT"
"$FONT_OUT"
MATH_LAYOUT_OUT="${TMPDIR:-/tmp}/picocalc_fxai_math_layout_tests"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/natural_editor.cpp" "$ROOT/src/expression.cpp" "$ROOT/src/math_engine.cpp" "$ROOT/src/memory.cpp" "$ROOT/src/math_renderer.cpp" "$ROOT/tests/math_renderer_smoke.cpp" -o "$MATH_LAYOUT_OUT"
"$MATH_LAYOUT_OUT"
GRAPH_OUT="${TMPDIR:-/tmp}/picocalc_fxai_graph_renderer_tests"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/graphics_engine.cpp" "$ROOT/src/math_engine.cpp" "$ROOT/src/memory.cpp" "$ROOT/src/expression.cpp" "$ROOT/src/graph_renderer.cpp" "$ROOT/tests/graph_renderer_smoke.cpp" -o "$GRAPH_OUT"
"$GRAPH_OUT"
LAUNCH_OUT="${TMPDIR:-/tmp}/picocalc_fxai_launcher_tests"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/natural_editor.cpp" "$ROOT/src/keyboard_input.cpp" "$ROOT/src/run_matrix.cpp" "$ROOT/src/point_a.cpp" "$ROOT/src/math_engine.cpp" "$ROOT/src/expression.cpp" "$ROOT/src/graph.cpp" "$ROOT/src/graph_renderer.cpp" "$ROOT/src/graphics_engine.cpp" "$ROOT/src/memory.cpp" "$ROOT/tests/launcher_smoke.cpp" -o "$LAUNCH_OUT"
"$LAUNCH_OUT"
WIDGET_OUT="${TMPDIR:-/tmp}/picocalc_fxai_widget_tests"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/graphics_engine.cpp" "$ROOT/src/font_renderer.cpp" "$ROOT/src/widget_system.cpp" "$ROOT/tests/widget_smoke.cpp" -o "$WIDGET_OUT"
"$WIDGET_OUT"
ANIMATION_OUT="${TMPDIR:-/tmp}/picocalc_fxai_animation_tests"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/animation_system.cpp" "$ROOT/tests/animation_smoke.cpp" -o "$ANIMATION_OUT"
"$ANIMATION_OUT"
CHEATSHEET_OUT="${TMPDIR:-/tmp}/picocalc_fxai_cheatsheet_tests"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/tests/cheatsheet_smoke.cpp" -o "$CHEATSHEET_OUT"
"$CHEATSHEET_OUT"
WINDOW_OUT="${TMPDIR:-/tmp}/picocalc_fxai_window_manager_tests"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/window_manager.cpp" "$ROOT/tests/window_manager_smoke.cpp" -o "$WINDOW_OUT"
"$WINDOW_OUT"
VPAM_AST_OUT="${TMPDIR:-/tmp}/picocalc_fxai_vpam_ast_tests"
"$CXX" -std=c++20 -O2 -Wall -Wextra -Werror -I"$ROOT/include" \
  "$ROOT/src/natural_editor.cpp" "$ROOT/src/math_engine.cpp" "$ROOT/src/expression.cpp" "$ROOT/src/memory.cpp" \
  "$ROOT/src/math_renderer.cpp" \
  "$ROOT/tests/vpam_ast_smoke.cpp" -o "$VPAM_AST_OUT"
"$VPAM_AST_OUT"

echo "PicoCalc FX AI host suite + VPAM shared AST tests: PASS"
