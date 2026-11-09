cmake_minimum_required(VERSION 3.16)
project(PicoCalcFXAI_Desktop LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Host build: portable core library + test suite (CTest) + optional SDL2
# desktop emulator. No Pico SDK required.
#
#   cmake -S desktop -B build-desktop [-DFXAI_TEST_BOARD=PICO_PLUS_2W]
#   cmake --build build-desktop && ctest --test-dir build-desktop --output-on-failure
set(FXAI_TEST_BOARD "PICO2W" CACHE STRING "Board profile for the host build: PICO2W or PICO_PLUS_2W")
set_property(CACHE FXAI_TEST_BOARD PROPERTY STRINGS PICO2W PICO_PLUS_2W)
if(FXAI_TEST_BOARD STREQUAL "PICO_PLUS_2W")
    set(_fxai_board_def FXAI_BOARD_PICO_PLUS_2W=1)
elseif(FXAI_TEST_BOARD STREQUAL "PICO2W")
    set(_fxai_board_def FXAI_BOARD_PICO2W=1)
else()
    message(FATAL_ERROR "FXAI_TEST_BOARD must be PICO2W or PICO_PLUS_2W")
endif()

set(FXAI_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(FXAI_CORE_SOURCES
    natural_editor keyboard_input run_matrix point_a math_engine expression graph graph_renderer
    graphics_engine memory editors font_renderer math_renderer widget_system animation_system
    window_manager tlsf mem_pools limits psram system_runtime flash_fs graph3d system_info
    diagnostics sd_card ai_client bt_support)
list(TRANSFORM FXAI_CORE_SOURCES PREPEND ${FXAI_ROOT}/src/)
list(TRANSFORM FXAI_CORE_SOURCES APPEND .cpp)

add_library(fxcore STATIC ${FXAI_CORE_SOURCES}
    ${FXAI_ROOT}/third_party/littlefs/lfs.c ${FXAI_ROOT}/third_party/littlefs/lfs_util.c)
target_include_directories(fxcore PUBLIC ${FXAI_ROOT}/include ${FXAI_ROOT}/third_party/littlefs ${FXAI_ROOT}/tests)
target_compile_definitions(fxcore PUBLIC ${_fxai_board_def} LFS_NO_DEBUG=1 LFS_NO_ASSERT=1)
target_compile_options(fxcore PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
find_package(Threads REQUIRED)
target_link_libraries(fxcore PUBLIC Threads::Threads)

enable_testing()
set(FXAI_TESTS
    math_smoke graphics_smoke font_smoke math_renderer_smoke graph_renderer_smoke launcher_smoke
    widget_smoke animation_smoke cheatsheet_smoke window_manager_smoke vpam_ast_smoke
    mem_pools_smoke psram_selftest_smoke limits_smoke flash_fs_smoke graphics_memory_smoke
    system_app_smoke)
foreach(test ${FXAI_TESTS})
    add_executable(${test} ${FXAI_ROOT}/tests/${test}.cpp)
    target_link_libraries(${test} PRIVATE fxcore)
    add_test(NAME ${test} COMMAND ${test})
endforeach()
# Tests that replace the global operator new with the pool router.
foreach(test mem_new_smoke oom_smoke)
    add_executable(${test} ${FXAI_ROOT}/tests/${test}.cpp ${FXAI_ROOT}/src/mem_new.cpp)
    target_link_libraries(${test} PRIVATE fxcore)
    add_test(NAME ${test} COMMAND ${test})
endforeach()

# Optional SDL2 desktop emulator.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    pkg_check_modules(SDL2_TTF QUIET IMPORTED_TARGET SDL2_ttf)
endif()
if(SDL2_FOUND AND SDL2_TTF_FOUND)
    add_executable(picocalc_cg50 main.cpp)
    target_link_libraries(picocalc_cg50 PRIVATE fxcore PkgConfig::SDL2 PkgConfig::SDL2_TTF)
    target_compile_options(picocalc_cg50 PRIVATE -Wall -Wextra -Wpedantic)
else()
    message(STATUS "SDL2/SDL2_ttf not found: desktop emulator skipped (tests are still built)")
endif()
