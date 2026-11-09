# FatFs R0.16 (third-party)

Generic FAT/exFAT file system module by ChaN (<http://elm-chan.org/fsw/ff/>),
used by the firmware to read and write the PicoCalc microSD card.

- Version: R0.16 with patch 2 (header of `ff.c`).
- Source: the mirror of the official releases at
  <https://github.com/abbrev/fatfs>, commit `ab992fcf2926b8a1ee3f0a8f9cefb6584174100d`
  (tag `R0.16`), recorded in `UPSTREAM_COMMIT`.
- License: the FatFs license (BSD-style, one clause), see `LICENSE.txt`.
- `ff.c`, `ff.h`, `diskio.h`, `ffunicode.c`, `00readme.txt`, `00history.txt`
  and `LICENSE.txt` are unmodified.
- `ffconf.h` is the project configuration (long file names, exFAT, 64-bit LBA,
  tiny buffers, no RTC, `f_mkfs` only in the host build).
- `diskio.c` and `ffsystem.c` (examples) are not included: the disk I/O
  functions are in `src/fatfs_store.cpp`, on top of `fx::sd::BlockDevice`
  (`src/sd_card.cpp`, SPI mode).

The sources are compiled with warnings disabled (`cmake/fxai_sources.cmake`),
so that the project's `-Werror` does not apply to third-party code.
