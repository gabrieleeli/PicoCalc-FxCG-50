# v1.5 — build and hardware validation checklist

## Reproducible RP2350 build

Prerequisites: Raspberry Pi Pico SDK checkout, ARM GNU Embedded toolchain,
CMake and Ninja or Make. Use the same SDK revision and compiler version for
repeatable artifacts. The build script prints the SDK revision and uses the
explicit `pico2_w` board target.

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
./scripts/build_pico2w.sh
```

Expected artifact: `build/pico2w/picocalc_fxai.uf2`.

The script has not yet been run in this environment because a Pico SDK checkout
and ARM cross-toolchain are not configured here. A successful host test is not
an RP2350 firmware build.

## Hardware acceptance

- [ ] Flash the UF2 to Raspberry Pi Pico 2 W on the target PicoCalc revision.
- [ ] Confirm LCD startup, full visible area, rotation and RGB565 color order.
- [ ] Run `FXAI_DISPLAY_CALIBRATION_SCREEN=ON` build and verify all four corners,
      color bars and 16-pixel grid.
- [ ] Verify keyboard I2C address, FIFO reads and key state/code mapping.
- [ ] Exercise arrows, Enter, Escape and F1–F6; check for duplicate/repeated input.
- [ ] Confirm repeated screen changes do not corrupt the display or lock input.
- [ ] Record board revision, SDK revision, compiler version and test results.

Do not mark hardware validation complete based only on a successful compile or
host-side simulation.


## v1.5.1-dev — input simbolico automatico

- Verificato su host: conversione `^` in nodo Power, `sqrt`/`cbrt` in nodi Radical e `rootN` per indici 3–99.
- Verificato su host: alias greci minuscoli/maiuscoli e rendering bitmap compatto.
- Verificato su host: alias numerici per tutte le costanti fisiche già presenti in `physicalConstant()`, più costanti matematiche aggiuntive.
- Suite `tests/run_host_tests.sh`: PASS.
- Non ancora verificati: build UF2 RP2350, resa visiva sul pannello fisico e digitazione sulla tastiera fisica.
