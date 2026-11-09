cmake_minimum_required(VERSION 3.13)

# ---------------------------------------------------------------------------
# Board selection: FXAI_BOARD=PICO2W (default) | PICO_PLUS_2W.
# Must run before the Pico SDK import, which derives platform, board header and
# flash size from PICO_BOARD.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/fxai_board.cmake)

if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT PICO_SDK_PATH OR NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "Set PICO_SDK_PATH (environment or -DPICO_SDK_PATH) to a Pico SDK >= 2.1 checkout")
endif()

# The math engine reports errors (and std::bad_alloc -> "Memoria insufficiente")
# through C++ exceptions: the SDK default (-fno-exceptions) cannot build it.
set(PICO_CXX_ENABLE_EXCEPTIONS 1)

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(PicoCalcFXAI C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

if(PICO_SDK_VERSION_STRING VERSION_LESS "2.1.0")
    message(FATAL_ERROR "Pico SDK >= 2.1.0 is required (found ${PICO_SDK_VERSION_STRING}): "
                        "the pimoroni_pico_plus2_w_rp2350 board header first appears in 2.1.0")
endif()

set(FXAI_TARGET picocalc_fxai)
add_executable(${FXAI_TARGET}
    src/main.cpp src/natural_editor.cpp src/keyboard_input.cpp src/run_matrix.cpp src/point_a.cpp
    src/math_engine.cpp src/expression.cpp src/graph.cpp
    src/ui.cpp src/window_manager.cpp src/graphics_engine.cpp src/font_renderer.cpp src/math_renderer.cpp
    src/graph_renderer.cpp src/widget_system.cpp src/animation_system.cpp src/board_port.cpp src/ai_client.cpp
    src/memory.cpp src/editors.cpp
    # v1.9.0: memory architecture, PSRAM, storage, diagnostics
    src/tlsf.cpp src/mem_pools.cpp src/mem_new.cpp src/psram.cpp src/limits.cpp src/system_runtime.cpp
    src/flash_fs.cpp src/graph3d.cpp src/system_info.cpp src/diagnostics.cpp src/sd_card.cpp src/bt_support.cpp
    third_party/littlefs/lfs.c third_party/littlefs/lfs_util.c
)
# Distinct image names per board: UF2 files are not interchangeable.
set_target_properties(${FXAI_TARGET} PROPERTIES OUTPUT_NAME "picocalc_fxai_${FXAI_BOARD_SUFFIX}")

pico_generate_pio_header(${FXAI_TARGET} ${CMAKE_CURRENT_LIST_DIR}/src/lcd_spi.pio)
target_include_directories(${FXAI_TARGET} PRIVATE include third_party/littlefs)

fxai_apply_board_definitions(${FXAI_TARGET})
target_compile_definitions(${FXAI_TARGET} PRIVATE
    PICO_USE_STACK_GUARDS=1        # MSPLIM stack limit on core 0 boot stack and core 1
    PICO_CORE1_STACK_SIZE=0        # core 1 stack is provided by system_runtime.cpp
    PICO_MALLOC_PANIC=0            # malloc failure returns NULL -> std::bad_alloc, never panic
    PICO_FLASH_ASSERT_ON_UNSAFE=0
    LFS_NO_DEBUG=1
    LFS_NO_ASSERT=1
)

# Optimisation: -Os / -O2 selectable; section GC is already enabled by the SDK
# (-ffunction-sections -fdata-sections + --gc-sections), repeated here to make
# it explicit. -fstack-usage feeds the structural recursion limits.
target_compile_options(${FXAI_TARGET} PRIVATE
    -${FXAI_OPT_LEVEL}
    -ffunction-sections -fdata-sections
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wno-psabi>
    -fstack-usage
)
target_link_options(${FXAI_TARGET} PRIVATE "LINKER:--gc-sections" "LINKER:--print-memory-usage")
if(FXAI_ENABLE_LTO)
    # Whole-target LTO breaks the SDK's -Wl,--wrap mechanism (pico_stdio,
    # pico_float/double: unresolved __wrap_printf/__wrap_pow). LTO is therefore
    # applied to the application sources only; SDK objects stay regular.
    get_target_property(_fxai_sources ${FXAI_TARGET} SOURCES)
    foreach(_src ${_fxai_sources})
        if(_src MATCHES "^src/.*\\.cpp$")
            set_source_files_properties(${_src} PROPERTIES COMPILE_OPTIONS "-flto;-ffat-lto-objects")
        endif()
    endforeach()
    target_link_options(${FXAI_TARGET} PRIVATE -flto=auto)
    message(WARNING "FXAI_ENABLE_LTO: experimental (application sources only), not hardware-validated")
endif()

target_link_libraries(${FXAI_TARGET}
    pico_stdlib pico_multicore pico_flash
    hardware_spi hardware_i2c hardware_pwm hardware_pio hardware_dma hardware_flash
    hardware_xip_cache hardware_watchdog hardware_sync
    pico_cyw43_arch_lwip_threadsafe_background
)
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

# Post-build firmware size check (warning above the firmware partition, error
# above the physical flash) + static SRAM report.
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
