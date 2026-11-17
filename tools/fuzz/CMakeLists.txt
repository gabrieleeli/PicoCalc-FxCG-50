# Fuzzing leggero sul PC (v1.13.1): libFuzzer con AddressSanitizer e UBSan bloccante.
# Richiede clang con il runtime di libFuzzer (Ubuntu: libclang-rt-18-dev).
#   CC=clang CXX=clang++ cmake -S tools/fuzz -B build/fuzz -G Ninja -DFXAI_TEST_BOARD=PICO_PLUS_2W
#   cmake --build build/fuzz
#   tools/fuzz/run_fuzz.sh build/fuzz 300      # 300 s per harness
cmake_minimum_required(VERSION 3.16)
project(PicoCalcFXAI_Fuzz LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(FXAI_TEST_BOARD "PICO_PLUS_2W" CACHE STRING "PICO_PLUS_2W o PICO2W")
if(FXAI_TEST_BOARD STREQUAL "PICO_PLUS_2W")
  set(FXAI_BOARD_DEFINE FXAI_BOARD_PICO_PLUS_2W=1)
else()
  set(FXAI_BOARD_DEFINE FXAI_BOARD_PICO2W=1)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/../../cmake/fxai_sources.cmake)
add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -g -O1)
add_link_options(-fsanitize=address,undefined)
add_library(fxai_fuzz_core STATIC ${FXAI_CORE_SOURCES} ${FXAI_ROOT}/src/platform/host/board_host.cpp ${FXAI_ROOT}/src/platform/host/file_disk.cpp)
target_include_directories(fxai_fuzz_core PUBLIC ${FXAI_INCLUDE_DIR} ${FXAI_ROOT}/tests)
target_compile_definitions(fxai_fuzz_core PUBLIC FXAI_VERSION_STRING="${FXAI_VERSION_STRING}" FXAI_FATFS_MKFS=1 ${FXAI_BOARD_DEFINE}
  LFS_NO_DEBUG=1 LFS_NO_WARN=1 LFS_NO_ERROR=1 LFS_NO_ASSERT=1)
foreach(t fuzz_expr fuzz_exactdiff fuzz_workspace fuzz_csv fuzz_pcgraph fuzz_equation fuzz_ui)
  add_executable(${t} ${CMAKE_CURRENT_LIST_DIR}/${t}.cpp)
  target_link_libraries(${t} PRIVATE fxai_fuzz_core)
  target_link_options(${t} PRIVATE -fsanitize=fuzzer)
endforeach()
add_executable(gen_seeds ${CMAKE_CURRENT_LIST_DIR}/gen_seeds.cpp)
target_link_libraries(gen_seeds PRIVATE fxai_fuzz_core)
