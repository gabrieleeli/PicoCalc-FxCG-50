# PicoCalc FX-CG50 AI — board selection.
#
# This file MUST be included before the Pico SDK import: the SDK derives the
# platform (rp2350-arm-s), the board header and PICO_FLASH_SIZE_BYTES from
# PICO_BOARD while pico_sdk_import.cmake / pico_sdk_init() run.
#
#   FXAI_BOARD=PICO2W        -> PICO_BOARD=pico2_w                       (default)
#   FXAI_BOARD=PICO_PLUS_2W  -> PICO_BOARD=pimoroni_pico_plus2_w_rp2350
#
# Use one build directory per board (scripts/build.sh does this): switching
# FXAI_BOARD inside an existing build directory is rejected because the SDK
# caches platform/toolchain decisions derived from the first board.

set(_fxai_known_boards PICO2W PICO_PLUS_2W)

# Backward compatibility: `cmake -DPICO_BOARD=pico2_w` (README of v1.5) or
# `-DPICO_BOARD=pimoroni_pico_plus2_w_rp2350` without FXAI_BOARD selects the
# matching FXAI board on the first configure.
set(_fxai_default_board PICO2W)
if(NOT DEFINED CACHE{FXAI_BOARD} AND NOT DEFINED FXAI_BOARD AND DEFINED PICO_BOARD)
    if(PICO_BOARD STREQUAL "pico2_w")
        set(_fxai_default_board PICO2W)
    elseif(PICO_BOARD STREQUAL "pimoroni_pico_plus2_w_rp2350")
        set(_fxai_default_board PICO_PLUS_2W)
    else()
        message(FATAL_ERROR "PICO_BOARD=${PICO_BOARD} is not supported by this firmware. "
                            "Use -DFXAI_BOARD=PICO2W or -DFXAI_BOARD=PICO_PLUS_2W.")
    endif()
endif()

# The cache default carries the inferred board (a plain variable would be
# shadowed by set(CACHE) under policy CMP0126 OLD).
set(FXAI_BOARD "${_fxai_default_board}" CACHE STRING "Target board: PICO2W (Raspberry Pi Pico 2 W) or PICO_PLUS_2W (Pimoroni Pico Plus 2 W)")
set_property(CACHE FXAI_BOARD PROPERTY STRINGS ${_fxai_known_boards})
string(TOUPPER "${FXAI_BOARD}" _fxai_board_upper)
string(REPLACE "-" "_" _fxai_board_upper "${_fxai_board_upper}")
if(_fxai_board_upper STREQUAL "PICO2_W" OR _fxai_board_upper STREQUAL "PICO_2W")
    set(_fxai_board_upper PICO2W)
endif()
if(_fxai_board_upper STREQUAL "PICOPLUS2W" OR _fxai_board_upper STREQUAL "PICO_PLUS2_W")
    set(_fxai_board_upper PICO_PLUS_2W)
endif()

if(_fxai_board_upper STREQUAL "PICO2W")
    set(_fxai_pico_board pico2_w)
    set(FXAI_BOARD_SUFFIX pico2w)
    set(FXAI_BOARD_DISPLAY_NAME "Raspberry Pi Pico 2 W")
    set(FXAI_HAS_PSRAM 0)
    set(FXAI_FLASH_SIZE_BYTES 4194304)          # 4 MiB (pico2_w.h)
    set(_fxai_default_fw_partition 2097152)      # 2 MiB firmware partition
    set(_fxai_default_opt "Os")
    set(_fxai_default_3d ON)                     # reduced 3D profile selected at runtime
    set(_fxai_default_cas OFF)
    set(_fxai_default_sd ON)
elseif(_fxai_board_upper STREQUAL "PICO_PLUS_2W")
    set(_fxai_pico_board pimoroni_pico_plus2_w_rp2350)
    set(FXAI_BOARD_SUFFIX pico_plus_2w)
    set(FXAI_BOARD_DISPLAY_NAME "Pimoroni Pico Plus 2 W")
    set(FXAI_HAS_PSRAM 1)
    set(FXAI_FLASH_SIZE_BYTES 16777216)         # 16 MiB (pimoroni_pico_plus2_w_rp2350.h)
    set(_fxai_default_fw_partition 4194304)      # 4 MiB firmware partition
    set(_fxai_default_opt "O2")
    set(_fxai_default_3d ON)
    set(_fxai_default_cas ON)
    set(_fxai_default_sd ON)
else()
    message(FATAL_ERROR "Unknown FXAI_BOARD='${FXAI_BOARD}'. Valid values: ${_fxai_known_boards}")
endif()
set(FXAI_BOARD "${_fxai_board_upper}" CACHE STRING "Target board" FORCE)

if(DEFINED CACHE{FXAI_CONFIGURED_BOARD} AND NOT FXAI_CONFIGURED_BOARD STREQUAL FXAI_BOARD)
    message(FATAL_ERROR "This build directory was configured for ${FXAI_CONFIGURED_BOARD}; "
                        "FXAI_BOARD=${FXAI_BOARD} needs a separate build directory "
                        "(e.g. ./scripts/build.sh --board ${FXAI_BOARD_SUFFIX}).")
endif()
if(NOT DEFINED CACHE{FXAI_CONFIGURED_BOARD} AND DEFINED PICO_BOARD AND NOT PICO_BOARD STREQUAL _fxai_pico_board)
    message(FATAL_ERROR "PICO_BOARD=${PICO_BOARD} contradicts FXAI_BOARD=${FXAI_BOARD} "
                        "(which requires PICO_BOARD=${_fxai_pico_board}). Pass only -DFXAI_BOARD.")
endif()
set(FXAI_CONFIGURED_BOARD "${FXAI_BOARD}" CACHE INTERNAL "Board this build directory was configured for")
set(PICO_BOARD "${_fxai_pico_board}" CACHE STRING "Pico SDK board (derived from FXAI_BOARD)" FORCE)

# ---------------------------------------------------------------------------
# Feature flags (defaults depend on the board; every flag can be overridden).
option(FXAI_ENABLE_3D  "3D graph engine (z-buffer/mesh; reduced profile without PSRAM)" ${_fxai_default_3d})
option(FXAI_ENABLE_CAS "Reserve CAS budgets/arena (no symbolic CAS module exists yet)"   ${_fxai_default_cas})
option(FXAI_ENABLE_BT  "Bluetooth LE through pico_btstack (never required to build)"     OFF)
option(FXAI_ENABLE_SD  "PicoCalc SD-card probe on SPI0 (GP16-19, detect GP22)"           ${_fxai_default_sd})
option(FXAI_FRAMEBUFFER_IN_PSRAM "Place the 320x320 RGB565 framebuffer in PSRAM (default: SRAM)" OFF)
option(FXAI_ENABLE_LTO "Link-time optimisation (experimental with the Pico SDK)" OFF)
option(FXAI_DISPLAY_CALIBRATION_SCREEN "Boot into LCD color/orientation calibration screen" OFF)

set(FXAI_OPT_LEVEL "${_fxai_default_opt}" CACHE STRING "Firmware optimisation level: Os or O2")
set_property(CACHE FXAI_OPT_LEVEL PROPERTY STRINGS Os O2)
if(NOT FXAI_OPT_LEVEL MATCHES "^(Os|O2)$")
    message(FATAL_ERROR "FXAI_OPT_LEVEL must be Os or O2 (got '${FXAI_OPT_LEVEL}')")
endif()

# Allocations at least this large go to PSRAM first (PREFER_PSRAM); smaller
# ones prefer SRAM. Ignored on boards without PSRAM.
set(FXAI_PSRAM_NEW_THRESHOLD 512 CACHE STRING "operator new size threshold (bytes) for PSRAM placement")

# Application stack (core 0) — lives in SRAM, guarded by MSPLIM.
set(FXAI_APP_STACK_BYTES 32768 CACHE STRING "Core 0 application stack size in bytes (SRAM)")
set(FXAI_CORE1_STACK_BYTES 8192 CACHE STRING "Core 1 worker stack size in bytes (SRAM)")

if(FXAI_FRAMEBUFFER_IN_PSRAM AND NOT FXAI_HAS_PSRAM)
    message(WARNING "FXAI_FRAMEBUFFER_IN_PSRAM ignored: ${FXAI_BOARD_DISPLAY_NAME} has no PSRAM")
    set(FXAI_FRAMEBUFFER_IN_PSRAM OFF CACHE BOOL "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Flash layout (computed from the board flash size):
#   [0, FW)            firmware image (size-checked after every build)
#   [FW, FW+4K)        diagnostic scratch sector (PSRAM-after-flash-write test)
#   [FW+4K, END-8K)    LittleFS internal file system
#   [END-8K, END)      reserved for BTstack TLV storage (kept even if BT is off,
#                      so toggling FXAI_ENABLE_BT never moves the file system)
set(FXAI_FIRMWARE_PARTITION_BYTES ${_fxai_default_fw_partition} CACHE STRING "Firmware partition size in bytes")
math(EXPR FXAI_FLASH_DIAG_OFFSET "${FXAI_FIRMWARE_PARTITION_BYTES}")
math(EXPR FXAI_FLASH_FS_OFFSET   "${FXAI_FIRMWARE_PARTITION_BYTES} + 4096")
math(EXPR FXAI_FLASH_BT_RESERVED "8192")
math(EXPR FXAI_FLASH_FS_SIZE     "${FXAI_FLASH_SIZE_BYTES} - ${FXAI_FLASH_FS_OFFSET} - ${FXAI_FLASH_BT_RESERVED}")
if(FXAI_FLASH_FS_SIZE LESS 65536)
    message(FATAL_ERROR "Flash layout leaves only ${FXAI_FLASH_FS_SIZE} bytes for LittleFS")
endif()
math(EXPR _fxai_fw_rem "${FXAI_FIRMWARE_PARTITION_BYTES} % 4096")
if(NOT _fxai_fw_rem EQUAL 0)
    message(FATAL_ERROR "FXAI_FIRMWARE_PARTITION_BYTES must be a multiple of 4096")
endif()

file(READ "${CMAKE_CURRENT_LIST_DIR}/../VERSION" FXAI_VERSION)
string(STRIP "${FXAI_VERSION}" FXAI_VERSION)

message(STATUS "PicoCalc FX AI ${FXAI_VERSION}: FXAI_BOARD=${FXAI_BOARD} -> PICO_BOARD=${PICO_BOARD}, "
               "PSRAM=${FXAI_HAS_PSRAM}, flash=${FXAI_FLASH_SIZE_BYTES} B, "
               "firmware partition=${FXAI_FIRMWARE_PARTITION_BYTES} B, LittleFS=${FXAI_FLASH_FS_SIZE} B @ ${FXAI_FLASH_FS_OFFSET}")
message(STATUS "Features: 3D=${FXAI_ENABLE_3D} CAS=${FXAI_ENABLE_CAS} BT=${FXAI_ENABLE_BT} SD=${FXAI_ENABLE_SD} "
               "FB_IN_PSRAM=${FXAI_FRAMEBUFFER_IN_PSRAM} OPT=-${FXAI_OPT_LEVEL} LTO=${FXAI_ENABLE_LTO}")

# Compile definitions shared by the firmware target.
function(fxai_apply_board_definitions target)
    if(FXAI_BOARD STREQUAL "PICO2W")
        set(_board_def FXAI_BOARD_PICO2W=1)
    else()
        set(_board_def FXAI_BOARD_PICO_PLUS_2W=1)
    endif()
    foreach(flag FXAI_ENABLE_3D FXAI_ENABLE_CAS FXAI_ENABLE_BT FXAI_ENABLE_SD FXAI_FRAMEBUFFER_IN_PSRAM FXAI_DISPLAY_CALIBRATION_SCREEN)
        if(${flag})
            list(APPEND _flag_defs ${flag}=1)
        else()
            list(APPEND _flag_defs ${flag}=0)
        endif()
    endforeach()
    target_compile_definitions(${target} PRIVATE
        ${_board_def}
        ${_flag_defs}
        FXAI_BOARD_NAME="${FXAI_BOARD_DISPLAY_NAME}"
        FXAI_BOARD_ID="${FXAI_BOARD_SUFFIX}"
        FXAI_VERSION="${FXAI_VERSION}"
        FXAI_HAS_PSRAM=${FXAI_HAS_PSRAM}
        FXAI_FLASH_SIZE=${FXAI_FLASH_SIZE_BYTES}
        FXAI_FIRMWARE_PARTITION_BYTES=${FXAI_FIRMWARE_PARTITION_BYTES}
        FXAI_FLASH_DIAG_OFFSET=${FXAI_FLASH_DIAG_OFFSET}
        FXAI_FLASH_FS_OFFSET=${FXAI_FLASH_FS_OFFSET}
        FXAI_FLASH_FS_SIZE=${FXAI_FLASH_FS_SIZE}
        FXAI_PSRAM_NEW_THRESHOLD=${FXAI_PSRAM_NEW_THRESHOLD}
        FXAI_APP_STACK_BYTES=${FXAI_APP_STACK_BYTES}
        FXAI_CORE1_STACK_BYTES=${FXAI_CORE1_STACK_BYTES}
    )
endfunction()
