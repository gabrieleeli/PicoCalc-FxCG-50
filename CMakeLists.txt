# PicoCalc FX-CG50 AI - RP2350 (Raspberry Pi Pico 2 W) firmware.
# Host tests and the simulator live in tests/CMakeLists.txt.
cmake_minimum_required(VERSION 3.13)
if(NOT DEFINED PICO_BOARD)
  set(PICO_BOARD pico2_w CACHE STRING "Target board")
endif()
# The math engine reports Syntax/Math errors with C++ exceptions.
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
  set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "Raspberry Pi Pico SDK")
endif()
if(NOT PICO_SDK_PATH)
  message(FATAL_ERROR "Set PICO_SDK_PATH (environment or -DPICO_SDK_PATH=...) to a Pico SDK >= 2.0 checkout")
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(PicoCalcFXAI C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
pico_sdk_init()

file(READ ${CMAKE_CURRENT_LIST_DIR}/VERSION FXAI_VERSION)
string(STRIP "${FXAI_VERSION}" FXAI_VERSION)

option(FXAI_DISPLAY_CALIBRATION_SCREEN "Show the LCD colour/orientation test pattern at boot" OFF)
option(FXAI_LCD_RGB565 "Drive the LCD in 16-bit RGB565 instead of the official 18-bit mode" OFF)
set(FXAI_LCD_SPI_HZ 25000000 CACHE STRING "LCD SPI clock (official driver: 25 MHz)")
# CA certificate(s) used to verify the AI relay (default: Let's Encrypt ISRG Root X1 + X2).
set(FXAI_RELAY_CA_FILE ${CMAKE_CURRENT_LIST_DIR}/certs/relay_ca.pem CACHE FILEPATH "PEM bundle trusted for the AI relay")
file(READ ${FXAI_RELAY_CA_FILE} FXAI_RELAY_CA_PEM)
if(NOT FXAI_RELAY_CA_PEM MATCHES "BEGIN CERTIFICATE")
  message(FATAL_ERROR "FXAI_RELAY_CA_FILE does not contain a PEM certificate: ${FXAI_RELAY_CA_FILE}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${FXAI_RELAY_CA_FILE})
configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/relay_ca.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/relay_ca.h @ONLY)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/fxai_sources.cmake)
add_executable(picocalc_fxai
  ${FXAI_CORE_SOURCES}
  src/platform/pico/board_pico.cpp
  src/platform/pico/net_pico.cpp
  src/platform/pico/sd_spi_pico.cpp
  src/platform/pico/main_pico.cpp)
target_include_directories(picocalc_fxai PRIVATE ${FXAI_INCLUDE_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/src/platform/pico/config ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_options(picocalc_fxai PRIVATE -Wall -Wextra)
target_compile_definitions(picocalc_fxai PRIVATE
  FXAI_VERSION="${FXAI_VERSION}"
  FXAI_VERSION_STRING="${FXAI_VERSION}"
  FXAI_LCD_SPI_HZ=${FXAI_LCD_SPI_HZ}
  FXAI_LCD_RGB565=$<BOOL:${FXAI_LCD_RGB565}>
  FXAI_DISPLAY_CALIBRATION_SCREEN=$<BOOL:${FXAI_DISPLAY_CALIBRATION_SCREEN}>
  PICO_STACK_SIZE=0x8000
  # Stack bytes one evaluation may use (of the 32 KB stack); see expression.cpp.
  FXAI_EVAL_STACK_BUDGET=16384)
pico_set_linker_script(picocalc_fxai ${CMAKE_CURRENT_LIST_DIR}/src/platform/pico/memmap_picocalc.ld)
target_link_libraries(picocalc_fxai
  pico_stdlib hardware_spi hardware_i2c hardware_flash pico_flash
  pico_cyw43_arch_lwip_poll pico_lwip_mbedtls pico_mbedtls)
pico_set_program_name(picocalc_fxai "PicoCalc FX-CG50 AI")
pico_set_program_version(picocalc_fxai "${FXAI_VERSION}")
pico_enable_stdio_usb(picocalc_fxai 1)
pico_enable_stdio_uart(picocalc_fxai 0)
pico_add_extra_outputs(picocalc_fxai)
