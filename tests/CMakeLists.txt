# Host build: portable core library, unit/integration tests and the screenshot simulator.
#   FXAI_TEST_BOARD=PICO_PLUS_2W (default, main target) or PICO2W selects the
#   board profile (persistent limits, flash layout, PSRAM capability); the
#   memory pools are simulated by the tests that need them.
cmake_minimum_required(VERSION 3.16)
project(PicoCalcFXAI_Host LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(FXAI_SANITIZE "Build host tests with AddressSanitizer and UBSan" OFF)
option(FXAI_TSAN "Build the multi-threaded allocator tests with ThreadSanitizer" OFF)
set(FXAI_TEST_BOARD "PICO_PLUS_2W" CACHE STRING "Board profile of the host build: PICO_PLUS_2W or PICO2W")
set_property(CACHE FXAI_TEST_BOARD PROPERTY STRINGS PICO_PLUS_2W PICO2W)
if(FXAI_TEST_BOARD STREQUAL "PICO_PLUS_2W")
  set(FXAI_BOARD_DEFINE FXAI_BOARD_PICO_PLUS_2W=1)
elseif(FXAI_TEST_BOARD STREQUAL "PICO2W")
  set(FXAI_BOARD_DEFINE FXAI_BOARD_PICO2W=1)
else()
  message(FATAL_ERROR "FXAI_TEST_BOARD must be PICO_PLUS_2W or PICO2W")
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/fxai_sources.cmake)

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wall> $<$<COMPILE_LANGUAGE:CXX>:-Wextra> $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
if(FXAI_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
find_package(Threads REQUIRED)

add_library(fxai_core STATIC ${FXAI_CORE_SOURCES})
target_include_directories(fxai_core PUBLIC ${FXAI_INCLUDE_DIR})
target_compile_definitions(fxai_core PUBLIC FXAI_VERSION_STRING="${FXAI_VERSION_STRING}" FXAI_FATFS_MKFS=1
  ${FXAI_BOARD_DEFINE} LFS_NO_DEBUG=1 LFS_NO_WARN=1 LFS_NO_ERROR=1 LFS_NO_ASSERT=1)
target_link_libraries(fxai_core PUBLIC Threads::Threads)

add_library(fxai_host_board STATIC ${FXAI_ROOT}/src/platform/host/board_host.cpp ${FXAI_ROOT}/src/platform/host/file_disk.cpp)
target_link_libraries(fxai_host_board PUBLIC fxai_core)

enable_testing()
set(FXAI_TESTS
  expression_test exact_test run_matrix_test architecture_test editor_test math_engine_test storage_test app_test apps_flow_test solvers_test statistics_test ai_test
  graph_engine_test graph_solve_test graph_file_test graph_app_test sd_fatfs_test graph_verify_test table_test
  graphics_smoke font_smoke animation_smoke cheatsheet_smoke vpam_ast_smoke
  # v1.9.0: memory architecture, PSRAM, internal flash, 3D, board profiles
  mem_pools_smoke psram_selftest_smoke limits_smoke internal_fs_test graphics_memory_test graph3d_test
  board_profile_test system_diag_test)
foreach(t ${FXAI_TESTS})
  add_executable(${t} ${CMAKE_CURRENT_LIST_DIR}/${t}.cpp)
  target_link_libraries(${t} PRIVATE fxai_core fxai_host_board)
  target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  add_test(NAME ${t} COMMAND ${t})
endforeach()
# Tests that replace the global operator new with the pool router (src/mem_new.cpp),
# as the firmware does: every C++ allocation goes through the simulated pools.
foreach(t mem_new_smoke oom_test)
  add_executable(${t} ${CMAKE_CURRENT_LIST_DIR}/${t}.cpp ${FXAI_DEVICE_SOURCES})
  target_link_libraries(${t} PRIVATE fxai_core fxai_host_board)
  target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  add_test(NAME ${t} COMMAND ${t})
endforeach()

# The architecture test reads the sources it checks.
target_compile_definitions(architecture_test PRIVATE FXAI_SOURCE_ROOT="${FXAI_ROOT}")

# ThreadSanitizer build of the allocator stress (two threads on the same pools).
# TSan cannot be combined with ASan: run_host_tests.sh --tsan uses its own build.
if(FXAI_TSAN AND FXAI_SANITIZE)
  message(FATAL_ERROR "FXAI_TSAN and FXAI_SANITIZE cannot be used in the same build directory")
endif()
if(FXAI_TSAN)
  add_executable(mem_pools_tsan ${CMAKE_CURRENT_LIST_DIR}/mem_pools_smoke.cpp ${FXAI_ROOT}/src/mem_pools.cpp ${FXAI_ROOT}/src/tlsf.cpp)
  target_include_directories(mem_pools_tsan PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${FXAI_INCLUDE_DIR})
  target_compile_definitions(mem_pools_tsan PRIVATE ${FXAI_BOARD_DEFINE})
  target_compile_options(mem_pools_tsan PRIVATE -fsanitize=thread -O1 -g)
  target_link_options(mem_pools_tsan PRIVATE -fsanitize=thread)
  target_link_libraries(mem_pools_tsan PRIVATE Threads::Threads)
  add_test(NAME mem_pools_tsan COMMAND mem_pools_tsan)
endif()

add_executable(picocalc_sim ${FXAI_ROOT}/src/platform/host/simulator.cpp)
target_link_libraries(picocalc_sim PRIVATE fxai_core fxai_host_board)
target_include_directories(picocalc_sim PRIVATE ${FXAI_ROOT}/src/platform/host)

# microSD image tool and the interoperability test with independent FAT/exFAT
# tools (dosfstools, exfatprogs, mtools); skipped when they are not installed.
add_executable(sd_image_tool ${FXAI_ROOT}/tools/sd_image_tool.cpp)
target_link_libraries(sd_image_tool PRIVATE fxai_core fxai_host_board)
target_include_directories(sd_image_tool PRIVATE ${FXAI_ROOT}/src/platform/host)
find_program(FXAI_MKFS_FAT mkfs.fat PATHS /usr/sbin /sbin)
find_program(FXAI_MCOPY mcopy)
if(FXAI_MKFS_FAT AND FXAI_MCOPY)
  add_test(NAME sd_interop_test COMMAND bash ${CMAKE_CURRENT_LIST_DIR}/sd_interop_test.sh $<TARGET_FILE:sd_image_tool> ${CMAKE_CURRENT_BINARY_DIR}/sd_interop)
endif()
