# PicoCalc FX-CG50 AI - RP2350 firmware.
#   Main target:          Pimoroni Pico Plus 2 W (RP2350B, 8 MB PSRAM, 16 MB flash)
#   Alternative profile:  Raspberry Pi Pico 2 W (RP2350A, 4 MB flash)
# Host tests and the simulator live in tests/CMakeLists.txt.
cmake_minimum_required(VERSION 3.13)

# Board selection: FXAI_BOARD=PICO_PLUS_2W (default) | PICO2W. Must run before
# the Pico SDK import, which derives platform, board header and flash size
# from PICO_BOARD.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/fxai_board.cmake)

# The math engine reports Syntax/Math errors (and std::bad_alloc -> Memory
# ERROR) with C++ exceptions: the SDK default (-fno-exceptions) cannot build it.
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
  set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "Raspberry Pi Pico SDK")
endif()
if(NOT PICO_SDK_PATH OR NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  message(FATAL_ERROR "Set PICO_SDK_PATH (environment or -DPICO_SDK_PATH=...) to a Pico SDK >= 2.1 checkout")
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
project(PicoCalcFXAI C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
pico_sdk_init()
if(PICO_SDK_VERSION_STRING VERSION_LESS "2.1.0")
  message(FATAL_ERROR "Pico SDK >= 2.1.0 is required (found ${PICO_SDK_VERSION_STRING}): "
                      "the pimoroni_pico_plus2_w_rp2350 board header first appears in 2.1.0")
endif()

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
set(FXAI_TARGET picocalc_fxai)
add_executable(${FXAI_TARGET}
  ${FXAI_CORE_SOURCES}
  ${FXAI_DEVICE_SOURCES}
  src/platform/pico/board_pico.cpp
  src/platform/pico/net_pico.cpp
  src/platform/pico/sd_spi_pico.cpp
  src/platform/pico/main_pico.cpp)
# Distinct image names per board: UF2 files are not interchangeable.
set_target_properties(${FXAI_TARGET} PROPERTIES OUTPUT_NAME "picocalc_fxai_${FXAI_BOARD_SUFFIX}")
target_include_directories(${FXAI_TARGET} PRIVATE ${FXAI_INCLUDE_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/src/platform/pico/config ${CMAKE_CURRENT_BINARY_DIR}/generated)
fxai_apply_board_definitions(${FXAI_TARGET})
target_compile_definitions(${FXAI_TARGET} PRIVATE
  FXAI_VERSION="${FXAI_VERSION}"
  FXAI_VERSION_STRING="${FXAI_VERSION}"
  FXAI_LCD_SPI_HZ=${FXAI_LCD_SPI_HZ}
  FXAI_LCD_RGB565=$<BOOL:${FXAI_LCD_RGB565}>
  FXAI_DISPLAY_CALIBRATION_SCREEN=$<BOOL:${FXAI_DISPLAY_CALIBRATION_SCREEN}>
  # Core 0 stack: 32 KB STACK region of memmap_picocalc.ld, hardware-guarded
  # by MSPLIM (PICO_USE_STACK_GUARDS installs it at boot). Core 1 (diagnostics
  # only) uses the whole 4 KB SCRATCH_X bank, also guarded.
  PICO_STACK_SIZE=0x8000
  PICO_CORE1_STACK_SIZE=0x1000
  PICO_USE_STACK_GUARDS=1
  # malloc failure returns NULL -> std::bad_alloc -> "Memory ERROR", never panic().
  PICO_MALLOC_PANIC=0
  # Flash writes use the project's safety helper (src/system_runtime.cpp).
  PICO_FLASH_ASSERT_ON_UNSAFE=0
  # LittleFS errors are returned as files::Status codes (shown by GRAPH and DIAG);
  # its own printf diagnostics are disabled.
  LFS_NO_DEBUG=1
  LFS_NO_WARN=1
  LFS_NO_ERROR=1
  LFS_NO_ASSERT=1
  # Stack bytes one evaluation may use (of the 32 KB stack); see expression.cpp.
  FXAI_EVAL_STACK_BUDGET=16384)
# The optimisation level comes last on the command line, so it overrides the
# SDK's Release -O3. -fstack-usage keeps the stack budget measurable.
target_compile_options(${FXAI_TARGET} PRIVATE -${FXAI_OPT_LEVEL} -Wall -Wextra
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-psabi> -fstack-usage)
target_link_options(${FXAI_TARGET} PRIVATE "LINKER:--print-memory-usage")
if(FXAI_ENABLE_LTO)
  # Whole-target LTO breaks the SDK's -Wl,--wrap mechanism (pico_stdio,
  # pico_float/double): only the application sources are LTO-compiled.
  foreach(_src ${FXAI_CORE_SOURCES} ${FXAI_DEVICE_SOURCES})
    if(_src MATCHES "\\.cpp$")
      set_property(SOURCE ${_src} APPEND PROPERTY COMPILE_OPTIONS -flto -ffat-lto-objects)
    endif()
  endforeach()
  target_link_options(${FXAI_TARGET} PRIVATE -flto=auto)
  message(WARNING "FXAI_ENABLE_LTO: experimental (application sources only), not hardware-validated")
endif()
pico_set_linker_script(${FXAI_TARGET} ${CMAKE_CURRENT_LIST_DIR}/src/platform/pico/memmap_picocalc.ld)
target_link_libraries(${FXAI_TARGET}
  pico_stdlib pico_multicore pico_flash
  hardware_spi hardware_i2c hardware_flash hardware_xip_cache hardware_watchdog hardware_sync hardware_clocks
  pico_cyw43_arch_lwip_poll pico_lwip_mbedtls pico_mbedtls)
if(FXAI_ENABLE_BT)
  # Bluetooth LE is optional and never required to build the firmware.
  target_link_libraries(${FXAI_TARGET} pico_btstack_ble pico_btstack_cyw43)
endif()
pico_set_program_name(${FXAI_TARGET} "PicoCalc FX-CG50 AI")
pico_set_program_version(${FXAI_TARGET} "${FXAI_VERSION} (${FXAI_BOARD_SUFFIX})")
pico_set_program_description(${FXAI_TARGET} "PicoCalc FX-CG50 AI firmware for ${FXAI_BOARD_DISPLAY_NAME}")
pico_enable_stdio_usb(${FXAI_TARGET} 1)
pico_enable_stdio_uart(${FXAI_TARGET} 0)
pico_add_extra_outputs(${FXAI_TARGET})

# Post-build check: warning above 90% of the firmware partition and above the
# partition, error above the physical flash; static SRAM report.
get_filename_component(_fxai_toolchain_dir "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(FXAI_SIZE_TOOL NAMES arm-none-eabi-size HINTS "${_fxai_toolchain_dir}")
add_custom_command(TARGET ${FXAI_TARGET} POST_BUILD
  COMMAND ${CMAKE_COMMAND}
    -DFXAI_BIN=$<TARGET_FILE_DIR:${FXAI_TARGET}>/picocalc_fxai_${FXAI_BOARD_SUFFIX}.bin
    -DFXAI_ELF=$<TARGET_FILE:${FXAI_TARGET}>
    -DFXAI_SIZE_TOOL=${FXAI_SIZE_TOOL}
    -DFXAI_LIMIT=${FXAI_FIRMWARE_PARTITION_BYTES}
    -DFXAI_FLASH=${FXAI_FLASH_SIZE_BYTES}
    -DFXAI_BOARD=${FXAI_BOARD}
    -P ${CMAKE_CURRENT_LIST_DIR}/cmake/check_firmware_size.cmake
  VERBATIM)
