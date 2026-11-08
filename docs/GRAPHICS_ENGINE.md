# Graphics engine — v1.6.0

Le note delle fasi precedenti (font 5×7, flush completo PIO-SPI) sono superate;
restano consultabili nella cronologia Git e nei documenti storici elencati in
[README.md](README.md).

## Componenti

| Modulo | Ruolo |
|---|---|
| `graphics_engine.hpp/.cpp` (`gfx::Canvas`) | primitive RGB565 su un buffer posseduto dal chiamante: pixel, linee (anche tratteggiate), rettangoli pieni/arrotondati, cerchi, ellissi, archi, poligoni (≤32 vertici), polilinee, Bézier, bitmap/livelli con colore trasparente, alpha blending, gradiente verticale, rettangolo punteggiato, clipping |
| `gfx::TileDiff` | rileva i tile 16×16 cambiati con hash FNV-1a e li unisce in rettangoli |
| `font_renderer.hpp/.cpp` + `font_data.cpp` | font bitmap UTF-8 a cella fissa con ricerca binaria dei code point |
| `math_renderer.hpp/.cpp` | layout a box del Natural Display (frazioni, radici, potenze, ∫, Σ, …) |
| `ui_kit.hpp/.cpp` | tema e widget CG50: barra di stato, tasti funzione, righe di lista, popup, finestre di errore, toast, testo a capo |
| `animation_system.hpp` | primitiva `ui::Animation` (inizio/durata/progresso); non ancora usata nelle schermate |

Nessun modulo dipende dal Pico SDK: lo stesso codice disegna sul framebuffer del
firmware e su quello del simulatore host (`picocalc_sim`, screenshot PNG).

## Framebuffer e aggiornamento del display

- Un solo framebuffer statico 320×320 RGB565 (204 800 byte), posseduto dalla porta
  board e pubblicato con `board::graphics()`.
- Ogni frame l'app attiva ridisegna tutto lo schermo; `board::present()` chiama
  `TileDiff::diff()` (400 hash, 1,6 KB) e invia solo i rettangoli cambiati.
  Se i rettangoli sono troppi viene inviato il loro rettangolo contenitore.
- Sul Pico ogni riga è convertita in RGB666 (`gfx::rgb565ToRgb666`) perché il
  driver ufficiale configura il pannello con `COLMOD 0x66`; con
  `-DFXAI_LCD_RGB565=ON` i pixel vengono inviati a 16 bit.
- `board::invalidate()` forza un invio completo (dopo l'init o la schermata di
  calibrazione).

## Font

| Face | Font | Cella | Uso |
|---|---|---|---|
| `Medium` | GNU Unifont 8×16 (baseline 14) | 8×16 | testo principale, espressioni |
| `Small` | Spleen 6×12 (baseline 9) + simboli Unifont ridotti | 6×12 | barra di stato, tasti funzione, esponenti, indici |
| `Tiny` | font storico 5×7 (ASCII + simboli matematici) | 6×9 | compatibilità con le API precedenti |

Se `Small` non ha un glifo, usa il simbolo 5×7 allineato alla propria baseline;
il fallback finale è `?`.

I set includono ASCII, Latin-1, greco maiuscolo/minuscolo e i simboli matematici
usati dalle modalità (√ ∫ Σ Π ∞ ≤ ≥ ≠ → ⇔ ∠ ° ′ ″ × ÷ ±, apici e pedici). I dati
sono generati da `tools/gen_fonts.py`; `font_smoke` verifica che ogni carattere
usato nei testi dell'interfaccia abbia un glifo.

## Math renderer (Natural Display)

`fx::math::layout()` riceve l'albero dell'editor (`NaturalNode`) e produce una
lista di operazioni (`NaturalDrawOp`: testo, linea di frazione, segno di radice,
parentesi estensibili, segnaposto, cursore) con:

- frazioni con linea centrata sull'asse matematico;
- radici quadrate e n-esime con indice in Small;
- potenze e indici in Small, con innalzamento proporzionale alla base;
- modelli ∫ab, d/dx, d²/dx², Σ, Π con limiti sopra/sotto;
- parentesi e |x| che crescono con il contenuto;
- segnaposto (quadratini) per i campi vuoti;
- posizione del cursore (`CursorTarget`) anche fuori dall'area visibile, usata
  dall'editor per lo scorrimento orizzontale.

Lo stesso layout disegna l'input dell'editor, i risultati di Run-Matrix (frazioni
e radici esatte) e le formule delle altre modalità; le metriche sono un'unica
fonte, quindi cursore e testo non possono disallinearsi.

## UI kit

Costanti di schermo: barra di stato 18 px, tasti funzione 20 px, riga 16 px.
Il tema (`ui::theme`) usa sfondo bianco, selezione blu, errori rossi e
indicatore SHIFT arancione, nello stile della fx-CG50 ma con grafica propria.
`ui::errorBox` mostra il titolo (Math ERROR, Syntax ERROR, …), il dettaglio e
"Press:[EXIT]".

## Test

`graphics_smoke` (primitive, clipping, blending, TileDiff, conversione RGB666),
`font_smoke` (copertura glifi, misure, UTF-8 non valido), `vpam_ast_smoke`,
`editor_test` (layout e cursore), `app_test` (rendering delle schermate su
framebuffer host). Gli screenshot in `docs/screenshots/` sono prodotti dal
simulatore con lo stesso codice del firmware.

## Limiti noti

- Nessun anti-aliasing né font proporzionali; scale intere soltanto.
- `arc` usa campionamento trigonometrico: adatto a elementi UI, non a grafici.
- Poligoni pieni oltre 32 vertici ignorati per non allocare memoria.
- Animazioni di transizione non ancora applicate alle schermate.
- Resa reale dei colori sul pannello ST7365P non ancora verificata.
