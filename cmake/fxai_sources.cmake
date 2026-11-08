# Portable sources shared by the RP2350 firmware and the host build (tests + simulator).
set(FXAI_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(FXAI_CORE_SOURCES
  ${FXAI_ROOT}/src/math_engine.cpp
  ${FXAI_ROOT}/src/expression.cpp
  ${FXAI_ROOT}/src/exact.cpp
  ${FXAI_ROOT}/src/memory.cpp
  ${FXAI_ROOT}/src/number_format.cpp
  ${FXAI_ROOT}/src/point_a.cpp
  ${FXAI_ROOT}/src/editors.cpp
  ${FXAI_ROOT}/src/graph.cpp
  ${FXAI_ROOT}/src/graph_model.cpp
  ${FXAI_ROOT}/src/graph_engine.cpp
  ${FXAI_ROOT}/src/graph_solve.cpp
  ${FXAI_ROOT}/src/file_store.cpp
  ${FXAI_ROOT}/src/graph_file.cpp
  ${FXAI_ROOT}/src/pict.cpp
  ${FXAI_ROOT}/src/sd_card.cpp
  ${FXAI_ROOT}/src/fatfs_store.cpp
  ${FXAI_ROOT}/third_party/fatfs/ff.c
  ${FXAI_ROOT}/third_party/fatfs/ffunicode.c
  ${FXAI_ROOT}/src/natural_editor.cpp
  ${FXAI_ROOT}/src/math_renderer.cpp
  ${FXAI_ROOT}/src/graphics_engine.cpp
  ${FXAI_ROOT}/src/font_renderer.cpp
  ${FXAI_ROOT}/src/font_data.cpp
  ${FXAI_ROOT}/src/animation_system.cpp
  ${FXAI_ROOT}/src/keyboard_input.cpp
  ${FXAI_ROOT}/src/ui_kit.cpp
  ${FXAI_ROOT}/src/line_editor.cpp
  ${FXAI_ROOT}/src/workspace.cpp
  ${FXAI_ROOT}/src/storage.cpp
  ${FXAI_ROOT}/src/run_matrix.cpp
  ${FXAI_ROOT}/src/solvers.cpp
  ${FXAI_ROOT}/src/statistics.cpp
  ${FXAI_ROOT}/src/app_registry.cpp
  ${FXAI_ROOT}/src/app_models.cpp
  ${FXAI_ROOT}/src/app_manager.cpp
  ${FXAI_ROOT}/src/firmware.cpp
  ${FXAI_ROOT}/src/apps/launcher.cpp
  ${FXAI_ROOT}/src/apps/symbols_app.cpp
  ${FXAI_ROOT}/src/apps/run_matrix_app.cpp
  ${FXAI_ROOT}/src/apps/widgets.cpp
  ${FXAI_ROOT}/src/apps/equation_app.cpp
  ${FXAI_ROOT}/src/apps/statistics_app.cpp
  ${FXAI_ROOT}/src/apps/graph_app.cpp
  ${FXAI_ROOT}/src/apps/graph_plot.cpp
  ${FXAI_ROOT}/src/apps/graph_files.cpp
  ${FXAI_ROOT}/src/apps/table_app.cpp
  ${FXAI_ROOT}/src/apps/system_app.cpp
  ${FXAI_ROOT}/src/apps/memory_app.cpp
  ${FXAI_ROOT}/src/apps/conversion_app.cpp
  ${FXAI_ROOT}/src/apps/ai_chat_app.cpp
  ${FXAI_ROOT}/src/ai_protocol.cpp
)
set(FXAI_INCLUDE_DIR ${FXAI_ROOT}/include ${FXAI_ROOT}/third_party/fatfs)
# FatFs is vendored unmodified (third_party/fatfs, 1-clause BSD): its own warnings are not ours.
set_source_files_properties(${FXAI_ROOT}/third_party/fatfs/ff.c ${FXAI_ROOT}/third_party/fatfs/ffunicode.c PROPERTIES COMPILE_OPTIONS "-w")
file(READ ${FXAI_ROOT}/VERSION FXAI_VERSION_RAW)
string(STRIP "${FXAI_VERSION_RAW}" FXAI_VERSION_STRING)
