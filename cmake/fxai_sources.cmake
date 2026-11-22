# Portable sources shared by the RP2350 firmware and the host build (tests + simulator).
get_filename_component(FXAI_ROOT ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)
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
  ${FXAI_ROOT}/src/value.cpp
  ${FXAI_ROOT}/src/expression.cpp
  ${FXAI_ROOT}/src/calc_library.cpp
  ${FXAI_ROOT}/src/exact.cpp
  ${FXAI_ROOT}/src/memory.cpp
  ${FXAI_ROOT}/src/number_format.cpp
  ${FXAI_ROOT}/src/point_a.cpp
  ${FXAI_ROOT}/src/editors.cpp
  ${FXAI_ROOT}/src/graph.cpp
  ${FXAI_ROOT}/src/graph_model.cpp
  ${FXAI_ROOT}/src/graph_engine.cpp
  ${FXAI_ROOT}/src/graph_solve.cpp
  ${FXAI_ROOT}/src/graph3d_model.cpp
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
  ${FXAI_ROOT}/src/workspace_sections.cpp
  ${FXAI_ROOT}/src/storage.cpp
  ${FXAI_ROOT}/src/run_matrix.cpp
  ${FXAI_ROOT}/src/csv_io.cpp
  ${FXAI_ROOT}/src/graph_commands.cpp
  ${FXAI_ROOT}/src/solvers.cpp
  ${FXAI_ROOT}/src/equation.cpp
  ${FXAI_ROOT}/src/statistics.cpp
  ${FXAI_ROOT}/src/app_registry.cpp
  ${FXAI_ROOT}/src/app_models.cpp
  ${FXAI_ROOT}/src/app_manager.cpp
  ${FXAI_ROOT}/src/firmware.cpp
  ${FXAI_ROOT}/src/apps/launcher.cpp
  ${FXAI_ROOT}/src/apps/symbols_app.cpp
  ${FXAI_ROOT}/src/apps/run_matrix_app.cpp
  ${FXAI_ROOT}/src/apps/matrix_editor.cpp
  ${FXAI_ROOT}/src/apps/answer_view.cpp
  ${FXAI_ROOT}/src/apps/widgets.cpp
  ${FXAI_ROOT}/src/apps/math_field.cpp
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
  ${FXAI_ROOT}/src/apps/graph3d_plot.cpp
  ${FXAI_ROOT}/src/ai_protocol.cpp
)
# ---------------------------------------------------------------------------
# v1.15.0 (F1): modalita' e servizi per scheda.
#
# La scheda viene da FXAI_BOARD (firmware, cmake/fxai_board.cmake) o da
# FXAI_TEST_BOARD (test host, fuzzing). Ogni modalita' e ogni servizio nuovi
# hanno un'opzione FXAI_MODE_<NOME> / FXAI_SERVICE_<NOME> con il valore
# predefinito della scheda; accenderli su una scheda non prevista e' un errore.
# Il codice riceve FXAI_MODE_<NOME>=0/1 e FXAI_SERVICE_<NOME>=0/1
# (FXAI_FEATURE_DEFINITIONS); il registro (src/app_registry.cpp) rende
# Available una modalita' solo quando la sua definizione e' 1, e
# tests/architecture_test.cpp confronta registro, definizioni e sorgenti
# compilati (FXAI_COMPILED_SOURCES). FXAI_EXCLUDED_OBJECTS e
# FXAI_EXCLUDED_SYMBOLS sono i nomi che non devono comparire nella mappa del
# linker (controllo in cmake/check_firmware_size.cmake).
if(DEFINED FXAI_BOARD)
  set(_fxai_feature_board "${FXAI_BOARD}")
elseif(DEFINED FXAI_TEST_BOARD)
  set(_fxai_feature_board "${FXAI_TEST_BOARD}")
else()
  set(_fxai_feature_board PICO_PLUS_2W)
endif()
set(FXAI_FEATURE_DEFINITIONS)
set(FXAI_FEATURE_SOURCES)
set(FXAI_EXCLUDED_OBJECTS)
set(FXAI_EXCLUDED_SYMBOLS)
set(FXAI_FEATURES_ON)
# fxai_feature(MODE|SERVICE <NOME> BOARDS <PICO_PLUS_2W PICO2W...> [SYMBOLS <nomi>] [SOURCES <file>...])
function(fxai_feature kind name)
  cmake_parse_arguments(F "" "" "BOARDS;SOURCES;SYMBOLS" ${ARGN})
  list(FIND F_BOARDS "${_fxai_feature_board}" _on_board)
  if(_on_board GREATER -1)
    set(_default ON)
  else()
    set(_default OFF)
  endif()
  set(_var "FXAI_${kind}_${name}")
  set(${_var} ${_default} CACHE BOOL "v1.15.0: ${kind} ${name} (schede: ${F_BOARDS})")
  if(${_var} AND _on_board EQUAL -1)
    message(FATAL_ERROR "${_var}=ON non e' previsto per ${_fxai_feature_board} (schede: ${F_BOARDS})")
  endif()
  set(_srcs)
  foreach(_s ${F_SOURCES})
    list(APPEND _srcs ${FXAI_ROOT}/${_s})
  endforeach()
  if(${_var})
    set(FXAI_FEATURE_DEFINITIONS ${FXAI_FEATURE_DEFINITIONS} ${_var}=1 PARENT_SCOPE)
    set(FXAI_FEATURE_SOURCES ${FXAI_FEATURE_SOURCES} ${_srcs} PARENT_SCOPE)
    set(FXAI_FEATURES_ON ${FXAI_FEATURES_ON} ${_var} PARENT_SCOPE)
  else()
    set(FXAI_FEATURE_DEFINITIONS ${FXAI_FEATURE_DEFINITIONS} ${_var}=0 PARENT_SCOPE)
    set(_objs ${FXAI_EXCLUDED_OBJECTS})
    foreach(_s ${F_SOURCES})
      list(APPEND _objs "${_s}.o")
    endforeach()
    set(FXAI_EXCLUDED_OBJECTS ${_objs} PARENT_SCOPE)
    set(FXAI_EXCLUDED_SYMBOLS ${FXAI_EXCLUDED_SYMBOLS} ${F_SYMBOLS} PARENT_SCOPE)
  endif()
endfunction()

# Servizi comuni (v1.15.0, M1). Sul Pico 2 W solo quelli che servono a
# eActivity e alle modalita' della v1.14.0.
fxai_feature(SERVICE TYPEDFILES BOARDS PICO_PLUS_2W PICO2W SOURCES src/typed_files.cpp)
fxai_feature(SERVICE MODESHELL  BOARDS PICO_PLUS_2W PICO2W SOURCES src/apps/mode_shell.cpp)
fxai_feature(SERVICE TEXTEDIT   BOARDS PICO_PLUS_2W PICO2W SOURCES src/text_editor.cpp)
fxai_feature(SERVICE STEPTASK   BOARDS PICO_PLUS_2W PICO2W SOURCES src/step_task.cpp)
fxai_feature(SERVICE GRAPH2D    BOARDS PICO_PLUS_2W SYMBOLS 6graph2d SOURCES src/graph2d.cpp)
fxai_feature(SERVICE BINDING    BOARDS PICO_PLUS_2W SYMBOLS BindingResolver DependencyGraph SOURCES src/binding.cpp)
fxai_feature(SERVICE INTERP     BOARDS PICO_PLUS_2W SYMBOLS 4prog SOURCES src/program_lang.cpp src/program_vm.cpp)
# Modalita' non ancora realizzate (nessun sorgente fino alla loro milestone):
# l'opzione esiste gia' perche' il registro dichiara su quali schede possono esistere.
fxai_feature(MODE EACTIVITY    BOARDS PICO_PLUS_2W PICO2W)
fxai_feature(MODE SPREADSHEET  BOARDS PICO_PLUS_2W SYMBOLS SpreadsheetApp)
fxai_feature(MODE DYNAGRAPH    BOARDS PICO_PLUS_2W SYMBOLS DynaGraphApp)
fxai_feature(MODE RECURSION    BOARDS PICO_PLUS_2W SYMBOLS RecursionApp)
fxai_feature(MODE CONICGRAPHS  BOARDS PICO_PLUS_2W SYMBOLS ConicGraphsApp)
fxai_feature(MODE PROGRAM      BOARDS PICO_PLUS_2W SYMBOLS ProgramApp)
fxai_feature(MODE FINANCIAL    BOARDS PICO_PLUS_2W SYMBOLS FinancialApp)
fxai_feature(MODE LINK         BOARDS PICO_PLUS_2W SYMBOLS LinkApp)
fxai_feature(MODE GEOMETRY     BOARDS PICO_PLUS_2W SYMBOLS GeometryApp)
fxai_feature(MODE PICTUREPLOT  BOARDS PICO_PLUS_2W SYMBOLS PicturePlotApp)
fxai_feature(MODE PYTHON       BOARDS PICO_PLUS_2W SYMBOLS PythonApp)
list(APPEND FXAI_CORE_SOURCES ${FXAI_FEATURE_SOURCES})
# Sorgenti compilati (relativi al repository), per architecture_test.
set(FXAI_COMPILED_SOURCES)
foreach(_s ${FXAI_CORE_SOURCES})
  file(RELATIVE_PATH _rel ${FXAI_ROOT} ${_s})
  list(APPEND FXAI_COMPILED_SOURCES ${_rel})
endforeach()

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
