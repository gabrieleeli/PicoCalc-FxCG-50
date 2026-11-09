# Vendored LittleFS

- Upstream: https://github.com/littlefs-project/littlefs
- Version: **v2.11.3** (tag commit `6cb4e86540eca0d9ba62500a298385c9d863c8be`)
- Files copied unmodified: `lfs.c`, `lfs.h`, `lfs_util.c`, `lfs_util.h`, `LICENSE.md`
- License: BSD-3-Clause (see `LICENSE.md`)

The block-device glue for the RP2350 internal flash lives in `src/flash_fs.cpp`
(`flash_safe_execute()` + SRAM bounce buffer). The same glue is exercised on the
host by `tests/flash_fs_smoke.cpp` with an emulated NOR flash.
