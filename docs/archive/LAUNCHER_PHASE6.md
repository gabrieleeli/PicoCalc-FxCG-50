# Phase 6 — Casio-style Launcher

## Implemented
- Replaced the generic 3x3 launcher with a 4x5 grid containing the 20 modes shown in the supplied Casio fx-CG50 reference image.
- Each tile has an individual Casio-inspired color, compact pictogram, caption and keyboard badge (1–9, A–K).
- Added separate project utility tiles for AI Chat and Symbols/Cheatsheet.
- Four-column keyboard navigation covers the 20-mode grid and the two utility tiles; `EXE` opens the selected item and `ESC` returns to the launcher.
- `F1` remains a direct shortcut to RUN-MATRIX.
- RUN-MATRIX and GRAPH open their existing controllers. Symbols/Cheatsheet opens the offline reference. Other modes and AI Chat show explicit placeholders until their controllers/API integration are implemented.

## Verification
- `tests/launcher_smoke.cpp` checks RUN-MATRIX and GRAPH transitions, selection, navigation into the utility dock, Cheatsheet opening, return navigation and the F1 shortcut.
- `tests/run_host_tests.sh` passes the full native suite.
- `src/ui.cpp` compiles independently with the host compiler using `-std=c++20 -Wall -Wextra -Werror`.

## Fidelity and hardware status
- The layout reproduces the reference's icon-tile structure, distinct color families, labels and shortcut badges, adapted to the PicoCalc's 320x320 panel.
- The pictograms are newly drawn compact approximations, not copied Casio bitmap assets; exact pixel-for-pixel reproduction would require original icon artwork.
- Rendering remains routed through `board_port`. Physical ST7365P display orientation/colors, key scan codes and the RP2350 UF2 build still require validation on the actual PicoCalc.
