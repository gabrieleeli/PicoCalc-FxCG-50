# Display driver — v0.9

## Implemented in source
- 320x320 RGB565 framebuffer (single buffer, 200 KiB).
- PIO-based SPI transmitter on GP10 (SCK) and GP11 (MOSI).
- LCD control pins: GP13 CS, GP14 D/C, GP15 reset.
- ST7365P / ILI9488-compatible startup sequence, 16-bit pixel mode, address window and RAM write.
- Basic clipped pixel, filled/outline rectangle and 5x7 ASCII text primitives.
- UI pages call `board::present()` once after drawing, rather than flushing each primitive.
- Keyboard I2C pins remain GP6/GP7.

## Important validation status
This is an initial hardware driver, not yet validated on a physical PicoCalc. The panel's exact controller initialization, color order, rotation and visible-area offsets may vary by panel revision. `LCD_Y_OFFSET`, MADCTL and the initialization command table are deliberately isolated for calibration.

The PIO transfer currently prioritizes a simple, conservative implementation over DMA throughput. A later pass should add DMA and dirty rectangles after the first successful physical-panel test.

## Build requirements
- Raspberry Pi Pico SDK with PIO support.
- `pico_generate_pio_header()` generates `lcd_spi.pio.h` from `src/lcd_spi.pio`.
- Link `hardware_pio`; no hardware SPI peripheral is used for the LCD path.

## Next display tasks
1. Build the UF2 with the Pico 2 W SDK toolchain.
2. Verify panel init and orientation on the actual PicoCalc revision.
3. Tune gamma, color order and offsets using a color-bar/test-grid screen.
4. Replace the basic font with a compact readable bitmap font and add clipping/UTF-8 glyphs.
5. Add a 2D math layout renderer for stacked fractions, radicals, superscripts/subscripts, matrices and cursor positioning. The current `formatNatural()` is only a text formatter, not full Natural Textbook Display.
6. Add DMA/partial-screen flush after correctness is established.


## Schermata di calibrazione v1.0

È disponibile `ui::drawDisplayCalibration()` con barre RGB565, griglia a 16 pixel e
marcatori dei quattro angoli. Per avviarla temporaneamente, configurare CMake con
`-DFXAI_DISPLAY_CALIBRATION_SCREEN=ON`.
La schermata serve a rilevare rotazione, clipping, offset, ordine dei colori e
problemi di dimensione visibile. Non dimostra da sola che la sequenza di init sia
corretta su ogni revisione: la verifica finale richiede il PicoCalc fisico.

## Graphics engine fase 2 (2026-11-07)

Le primitive `board::clear/pixel/rect` ora delegano a `gfx::Canvas`, implementato in
`graphics_engine.cpp`. Il canvas riutilizza il framebuffer esistente e non crea un
secondo buffer. È accessibile tramite `board::graphics()` per linee, cerchi, archi,
poligoni, rettangoli arrotondati, bitmap/livelli, clipping e blending. Vedi
`GRAPHICS_ENGINE.md` per limiti e API. Il flush completo e la validazione fisica
rimangono invariati.

Il font ASCII 5×7 è stato estratto dal driver nel modulo condiviso
`font_renderer.cpp`; `board::text()` ne usa ora le API. La scala bitmap 1×–3× e
alcuni glifi matematici sono coperti da test host, senza implicare una verifica
visiva sul pannello reale.
