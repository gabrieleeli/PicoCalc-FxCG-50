# Host build: portable core library, unit/integration tests and the screenshot simulator.
cmake_minimum_required(VERSION 3.16)
project(PicoCalcFXAI_Host LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(FXAI_SANITIZE "Build host tests with AddressSanitizer and UBSan" OFF)
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/fxai_sources.cmake)

add_compile_options(-Wall -Wextra -Werror)
if(FXAI_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(fxai_core STATIC ${FXAI_CORE_SOURCES})
target_include_directories(fxai_core PUBLIC ${FXAI_INCLUDE_DIR})
target_compile_definitions(fxai_core PUBLIC FXAI_VERSION_STRING="${FXAI_VERSION_STRING}" FXAI_FATFS_MKFS=1)

add_library(fxai_host_board STATIC ${FXAI_ROOT}/src/platform/host/board_host.cpp)
target_link_libraries(fxai_host_board PUBLIC fxai_core)

enable_testing()
set(FXAI_TESTS
  expression_test exact_test run_matrix_test architecture_test editor_test math_engine_test storage_test app_test apps_flow_test solvers_test statistics_test ai_test
  graph_engine_test graph_solve_test graph_file_test graph_app_test sd_fatfs_test
  graphics_smoke font_smoke animation_smoke cheatsheet_smoke vpam_ast_smoke)
foreach(t ${FXAI_TESTS})
  add_executable(${t} ${CMAKE_CURRENT_LIST_DIR}/${t}.cpp)
  target_link_libraries(${t} PRIVATE fxai_core fxai_host_board)
  target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  add_test(NAME ${t} COMMAND ${t})
endforeach()

# The architecture test reads the sources it checks.
target_compile_definitions(architecture_test PRIVATE FXAI_SOURCE_ROOT="${FXAI_ROOT}")

add_executable(picocalc_sim ${FXAI_ROOT}/src/platform/host/simulator.cpp)
target_link_libraries(picocalc_sim PRIVATE fxai_core fxai_host_board)
