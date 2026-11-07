cmake_minimum_required(VERSION 3.16)
project(PicoCalcFXAI_Desktop LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
add_executable(picocalc_cg50
  main.cpp
  ../src/natural_editor.cpp ../src/keyboard_input.cpp ../src/run_matrix.cpp ../src/point_a.cpp ../src/math_engine.cpp ../src/expression.cpp ../src/graph.cpp
  ../src/memory.cpp ../src/editors.cpp)
target_include_directories(picocalc_cg50 PRIVATE ../include)
target_link_libraries(picocalc_cg50 PRIVATE PkgConfig::SDL2 PkgConfig::SDL2_TTF)
target_compile_options(picocalc_cg50 PRIVATE -Wall -Wextra -Wpedantic)
