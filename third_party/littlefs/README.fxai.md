# Vendored LittleFS

- Upstream: https://github.com/littlefs-project/littlefs
- Version: **v2.11.3** (tag commit `6cb4e86540eca0d9ba62500a298385c9d863c8be`)
- Files copied unmodified: `lfs.c`, `lfs.h`, `lfs_util.c`, `lfs_util.h`, `LICENSE.md`
- License: BSD-3-Clause (see `LICENSE.md`)

The file-system adapter (`fx::files::LittleFsFileSystem`) lives in
`src/flash_fs.cpp` and works on any `storage::FlashDevice`. On the RP2350 the
device is the internal-flash region of `src/platform/pico/board_pico.cpp`
(`flash_safe_execute()` + SRAM bounce buffer, bounded to the LittleFS
partition and to the real flash chip). On the host the same adapter is
exercised by `tests/internal_fs_test.cpp` on an emulated NOR flash
(`storage::RamFlash`, with power-loss injection).
