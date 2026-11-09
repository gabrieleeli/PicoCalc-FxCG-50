# Post-build firmware size check (invoked with cmake -P).
#   -DFXAI_BIN=<image.bin> -DFXAI_ELF=<image.elf> -DFXAI_SIZE_TOOL=<arm-none-eabi-size>
#   -DFXAI_LIMIT=<firmware partition bytes> -DFXAI_FLASH=<total flash bytes>
#   -DFXAI_BOARD=<board name>
#
# The .bin image is exactly what occupies flash from offset 0, so its size is
# compared with the firmware partition that ends where the diagnostic sector
# and LittleFS begin. Exceeding the partition is reported as a warning (the
# image would overwrite the file system area when flashed); exceeding the
# physical flash is a hard error.

if(NOT EXISTS "${FXAI_BIN}")
    message(FATAL_ERROR "Firmware image not found: ${FXAI_BIN}")
endif()
file(SIZE "${FXAI_BIN}" _size)
math(EXPR _pct "(${_size} * 100) / ${FXAI_LIMIT}")
math(EXPR _kib "${_size} / 1024")
math(EXPR _limit_kib "${FXAI_LIMIT} / 1024")
math(EXPR _soft "(${FXAI_LIMIT} * 90) / 100")

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

message(STATUS "[${FXAI_BOARD}] firmware image ${_size} bytes (${_kib} KiB) = ${_pct}% of the ${_limit_kib} KiB firmware partition")
if(_size GREATER FXAI_FLASH)
    message(FATAL_ERROR "[${FXAI_BOARD}] firmware (${_size} B) exceeds the physical flash (${FXAI_FLASH} B)")
elseif(_size GREATER FXAI_LIMIT)
    message(WARNING "[${FXAI_BOARD}] firmware (${_size} B) exceeds the firmware partition (${FXAI_LIMIT} B): "
                    "it overlaps the diagnostic sector/LittleFS. Increase FXAI_FIRMWARE_PARTITION_BYTES.")
elseif(_size GREATER _soft)
    message(WARNING "[${FXAI_BOARD}] firmware uses more than 90% of its partition (${_size}/${FXAI_LIMIT} B)")
endif()
