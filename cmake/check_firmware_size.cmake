# Post-build firmware checks (invoked with cmake -P).
#   -DFXAI_BIN=<image.bin> -DFXAI_ELF=<image.elf> -DFXAI_SIZE_TOOL=<arm-none-eabi-size>
#   -DFXAI_LIMIT=<firmware partition bytes> -DFXAI_FLASH=<total flash bytes>
#   -DFXAI_BOARD=<board name>
#   -DFXAI_MARGIN=<bytes of the partition that must stay free>          (v1.15.0)
#   -DFXAI_MAP=<linker map>                                             (v1.15.0)
#   -DFXAI_EXCLUDED_OBJECTS=<a.cpp.o|b.cpp.o>  -DFXAI_EXCLUDED_SYMBOLS=<Name|Name>
#
# The .bin image is exactly what occupies flash from offset 0, so its size is
# compared with the firmware partition that ends where the diagnostic sector
# and LittleFS begin. Since v1.15.0 every check is an ERROR, not a warning:
#  * the image exceeds the physical flash or the firmware partition (it would
#    overwrite the file system area when flashed);
#  * the image leaves less than FXAI_MARGIN bytes of the partition free
#    (Pico 2 W: 32 KiB kept for future fixes; Pico Plus 2 W: 10 %);
#  * the linker map of the image contains an object file or a symbol of a mode
#    or a service that is excluded on this board (cmake/fxai_sources.cmake).

if(NOT EXISTS "${FXAI_BIN}")
    message(FATAL_ERROR "Firmware image not found: ${FXAI_BIN}")
endif()
file(SIZE "${FXAI_BIN}" _size)
math(EXPR _pct "(${_size} * 100) / ${FXAI_LIMIT}")
math(EXPR _kib "${_size} / 1024")
math(EXPR _limit_kib "${FXAI_LIMIT} / 1024")
if(NOT DEFINED FXAI_MARGIN OR FXAI_MARGIN STREQUAL "")
    set(FXAI_MARGIN 0)
endif()
math(EXPR _budget "${FXAI_LIMIT} - ${FXAI_MARGIN}")

if(FXAI_SIZE_TOOL AND EXISTS "${FXAI_ELF}")
    execute_process(COMMAND "${FXAI_SIZE_TOOL}" -A "${FXAI_ELF}" OUTPUT_VARIABLE _sections ERROR_QUIET)
    # Sum the SRAM-resident sections (.data copied from flash + zero-initialised .bss + scratch).
    set(_sram 0)
    string(REPLACE "\n" ";" _lines "${_sections}")
    foreach(_line IN LISTS _lines)
        if(_line MATCHES "^(\\.[A-Za-z0-9_.]+)[ \t]+([0-9]+)[ \t]+([0-9]+)")
            set(_name "${CMAKE_MATCH_1}")
            set(_bytes "${CMAKE_MATCH_2}")
            set(_addr "${CMAKE_MATCH_3}")
            # SRAM: 0x20000000 .. 0x20082000 (536870912 .. 537403392)
            if(_addr GREATER_EQUAL 536870912 AND _addr LESS 537403392 AND NOT _name MATCHES "dummy")
                math(EXPR _sram "${_sram} + ${_bytes}")
            endif()
        endif()
    endforeach()
    math(EXPR _sram_kib "${_sram} / 1024")
    message(STATUS "[${FXAI_BOARD}] static SRAM (.data/.bss/.ram sections): ${_sram} bytes (${_sram_kib} KiB) of 520 KiB")
endif()

message(STATUS "[${FXAI_BOARD}] firmware image ${_size} bytes (${_kib} KiB) = ${_pct}% of the ${_limit_kib} KiB firmware partition"
               " (budget ${_budget} B, margin ${FXAI_MARGIN} B)")
if(_size GREATER FXAI_FLASH)
    message(FATAL_ERROR "[${FXAI_BOARD}] firmware (${_size} B) exceeds the physical flash (${FXAI_FLASH} B)")
elseif(_size GREATER FXAI_LIMIT)
    message(FATAL_ERROR "[${FXAI_BOARD}] firmware (${_size} B) exceeds the firmware partition (${FXAI_LIMIT} B): "
                        "it would overlap the diagnostic sector/LittleFS.")
elseif(_size GREATER _budget)
    message(FATAL_ERROR "[${FXAI_BOARD}] firmware (${_size} B) leaves less than ${FXAI_MARGIN} B of the "
                        "${FXAI_LIMIT} B partition free (budget ${_budget} B)")
endif()

# Excluded modes and services must not reach the image (v1.15.0).
if(FXAI_MAP AND EXISTS "${FXAI_MAP}")
    file(READ "${FXAI_MAP}" _map)
    string(FIND "${_map}" "Linker script and memory map" _start)
    if(_start GREATER -1)
        string(SUBSTRING "${_map}" ${_start} -1 _map)
    endif()
    string(REPLACE "|" ";" _objects "${FXAI_EXCLUDED_OBJECTS}")
    string(REPLACE "|" ";" _symbols "${FXAI_EXCLUDED_SYMBOLS}")
    set(_checked 0)
    foreach(_name IN LISTS _objects _symbols)
        if(_name STREQUAL "")
            continue()
        endif()
        math(EXPR _checked "${_checked} + 1")
        string(FIND "${_map}" "${_name}" _hit)
        if(_hit GREATER -1)
            message(FATAL_ERROR "[${FXAI_BOARD}] excluded mode/service found in the image: ${_name} (${FXAI_MAP})")
        endif()
    endforeach()
    message(STATUS "[${FXAI_BOARD}] linker map: ${_checked} excluded objects/symbols, none in the image")
elseif(FXAI_MAP)
    message(FATAL_ERROR "[${FXAI_BOARD}] linker map not found: ${FXAI_MAP}")
endif()
