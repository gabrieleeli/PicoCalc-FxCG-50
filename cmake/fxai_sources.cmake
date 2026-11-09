# Portable sources shared by the RP2350 firmware and the host build (tests + simulator).
set(FXAI_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(FXAI_CORE_SOURCES
  ${FXAI_ROOT}/src/math_engine.cpp
  # Memory architecture (v1.9.0): pools, TLSF, PSRAM driver (host: stub),
  # board/memory-derived limits, runtime services (host: stubs), system report.
  ${FXAI_ROOT}/src/tlsf.cpp
  ${FXAI_ROOT}/src/mem_pools.cpp
  ${FXAI_ROOT}/src/limits.cpp
  ${FXAI_ROOT}/src/psram.cpp
  ${FXAI_ROOT}/src/system_runtime.cpp
  ${FXAI_ROOT}/src/system_info.cpp
  ${FXAI_ROOT}/src/bt_support.cpp
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
  ${FXAI_ROOT}/src/graph3d.cpp
  ${FXAI_ROOT}/src/table_model.cpp
  ${FXAI_ROOT}/src/diagnostics.cpp
  ${FXAI_ROOT}/src/file_store.cpp
  ${FXAI_ROOT}/src/graph_file.cpp
  ${FXAI_ROOT}/src/pict.cpp
  ${FXAI_ROOT}/src/sd_card.cpp
  ${FXAI_ROOT}/src/fatfs_store.cpp
  ${FXAI_ROOT}/third_party/fatfs/ff.c
  ${FXAI_ROOT}/third_party/fatfs/ffunicode.c
  ${FXAI_ROOT}/src/flash_fs.cpp
  ${FXAI_ROOT}/third_party/littlefs/lfs.c
  ${FXAI_ROOT}/third_party/littlefs/lfs_util.c
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
  ${FXAI_ROOT}/src/apps/graph3d_app.cpp
  ${FXAI_ROOT}/src/ai_protocol.cpp
)
# Device-only sources: the global operator new that routes every C++
# allocation through the memory pools (host tests link it explicitly where
# they exercise it, see tests/CMakeLists.txt).
set(FXAI_DEVICE_SOURCES
  ${FXAI_ROOT}/src/mem_new.cpp
)
set(FXAI_INCLUDE_DIR ${FXAI_ROOT}/include ${FXAI_ROOT}/third_party/fatfs ${FXAI_ROOT}/third_party/littlefs)
# FatFs (1-clause BSD) and LittleFS (BSD-3-Clause) are vendored unmodified: their own warnings are not ours.
set_source_files_properties(${FXAI_ROOT}/third_party/fatfs/ff.c ${FXAI_ROOT}/third_party/fatfs/ffunicode.c
  ${FXAI_ROOT}/third_party/littlefs/lfs.c ${FXAI_ROOT}/third_party/littlefs/lfs_util.c PROPERTIES COMPILE_OPTIONS "-w")
file(READ ${FXAI_ROOT}/VERSION FXAI_VERSION_RAW)
string(STRIP "${FXAI_VERSION_RAW}" FXAI_VERSION_STRING)
