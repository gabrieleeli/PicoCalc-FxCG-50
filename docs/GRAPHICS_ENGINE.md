# Graphics engine — fase 2

## Architettura

`gfx::Canvas` è un renderer RGB565 indipendente dall'hardware. Riceve un puntatore al buffer posseduto dal chiamante e non alloca memoria, non copia il framebuffer e non dipende da Pico SDK, SDL o filesystem. Il driver PicoCalc possiede ancora il framebuffer statico 320×320 e pubblica il canvas con `board::graphics()`.

Le API legacy `board::clear`, `board::pixel` e `board::rect` restano disponibili e delegano al canvas: le schermate esistenti non devono essere riscritte per adottare il nuovo motore.

## Funzionalità implementate

- Pixel e lettura pixel entro clipping.
- Linee Bresenham, linee orizzontali e verticali.
- Rettangoli pieni e vuoti, rettangoli arrotondati.
- Cerchi pieni e vuoti, archi.
- Poligoni pieni e vuoti (massimo 32 vertici, buffer temporaneo sullo stack).
- Blit RGB565 e livelli bitmap statici con chiave trasparente opzionale.
- Alpha blending RGB565 a 8 bit.
- Clip rettangolare impostabile e ripristinabile.
- Palette base condivisa.

## Vincoli di memoria e prestazioni

Il canvas non possiede il framebuffer: sul target viene riutilizzato quello già presente in `board_port.cpp` (320×320×2 = 204.800 byte). Nessuna seconda copia del framebuffer viene creata. Le primitive lavorano direttamente sul buffer; il clipping viene applicato durante il disegno. Il riempimento dei poligoni usa un array locale fisso da 32 intersezioni.

Il flush LCD continua a inviare l'intero framebuffer tramite PIO-SPI. Dirty rectangles, DMA, font scalabili e rendering matematico 2D sono attività successive e non sono dichiarate implementate in questa fase.

## API essenziale

```cpp
auto& canvas = board::graphics();
canvas.setClip({8, 32, 304, 280});
canvas.roundedRect(12, 40, 100, 48, 6, gfx::palette::Panel, true);
canvas.line(12, 100, 300, 100, gfx::palette::Cyan);
canvas.resetClip();
board::present();
```

`gfx::Canvas` è anche testabile su host con un array RGB565 locale. I test sono in `tests/graphics_smoke.cpp`.

## Limiti noti

- La primitiva `arc` usa campionamento trigonometrico e va usata per elementi UI non critici, non per ogni frame di animazione.
- I poligoni pieni oltre 32 vertici vengono ignorati intenzionalmente per evitare allocazioni dinamiche.
- Il blending è simulato e scrive il colore risultante nel framebuffer; non esiste alpha hardware.
- I livelli sono viste bitmap non proprietarie: l'ordinamento e la vita dei buffer restano responsabilità dell'applicazione.
- Non è ancora disponibile un renderer testuale condiviso: il font ASCII 5×7 resta nel driver ed è oggetto della fase font.

## Font renderer condiviso — primo incremento fase 3

`font_renderer.hpp/.cpp` separa il font bitmap dal driver LCD. `font::drawText`
renderizza il set ASCII 5×7 a scala intera 1×–3×, gestisce newline e larghezza
massima e usa il clipping del canvas. `font::measure` restituisce misure coerenti
con la spaziatura del font. Sono presenti glifi dedicati per π, √, ×, ÷, Σ, θ,
∫, ∞, ≤ e ≥; i caratteri non presenti hanno fallback `?`.

`board::text()` delega ora a questo modulo, quindi le schermate correnti usano il
renderer condiviso senza modifiche alle loro chiamate. Il font resta volutamente
compatto: il ridimensionamento è nearest-neighbour, non una famiglia di font
proporzionale. Il Math Layout Engine e il set greco esteso sono ancora da fare.

## Incremento v1.4.2 — layout, launcher, graph e dirty regions

- `math_renderer.hpp/.cpp`: layout ricorsivo separato dall'editor; misurazione
  e disegno di righe, frazioni, radicali, potenze, pedici, gruppi e contenitori
  matrice (per le matrici, il nodo atteso contiene righe `Row`). Limiti di
  profondità e dimensione per evitare layout patologici. Il rendering naturale
  delle schermate RUN-MATRIX e Natural Editor usa ora questo renderer; la
  geometria del cursore continua a provenire dall'editor.
- `run_matrix.cpp`: il launcher ora mantiene una selezione 3×3, navigabile con
  frecce; EXE apre RUN-MATRIX o GRAPH. ESC riporta al launcher. Le altre tile
  sono ancora non collegate a un controller applicativo.
- `graph_renderer.hpp/.cpp`: disegna griglia, assi, funzione cartesiana e
  cursore; usa `fx::evaluate` per campionare l'espressione, zoom centrato e pan
  disponibile nell'API. L'interfaccia attuale usa frecce per trace e zoom.
- `widget_system.hpp/.cpp`: primitive condivise per pannelli, title bar, popup
  e selezione.
- `animation_system.hpp/.cpp`: timeline non bloccante basata su timestamp;
  il componente è testato ma non ancora collegato alle transizioni delle app.
- `Canvas` accumula il bounding box delle scritture. `board::present()` invia
  soltanto l'area modificata; per regioni parziali riemette RAMWR per riga per
  non dipendere dalla persistenza del comando durante il toggle CS.
- `KEY_TAB` è trattato come alias logico di MENU/indietro. Il firmware tastiera
  standard espone ESC ma non un codice dedicato MENU nel set di codici
  documentato; non viene inventato un codice hardware.

### Test aggiunti

`math_renderer_smoke`, `graph_renderer_smoke`, `launcher_smoke`,
`widget_smoke`, `animation_smoke` e verifica dirty rectangle sono inclusi in
`tests/run_host_tests.sh`.

### Limiti residui

- Il nodo Matrix è predisposto per la visualizzazione ma non esiste ancora un
  comando editor per creare/modificare matrici naturali e il parser non valuta
  il nodo Matrix.
- I tile STAT, EQUATION, MATRIX, VECTOR, BASE-N, AI e 3D non hanno ancora un
  controller interattivo collegato. Le API degli editor restano presenti.
- Il cursore del layout resta gestito dall'editor precedente; la selezione del
  cursore dentro strutture annidate richiede un successivo consolidamento.
- Popup/widget e timeline animazioni sono primitive condivise; non tutte le
  schermate li utilizzano e non è ancora implementata una transizione animata.
- Dirty rectangles sono implementati nel driver ma il percorso UI corrente
  chiama spesso `clear()`, che correttamente marca l'intero display come dirty.
- Nessuna validazione del pannello fisico, orientamento o colori su hardware.

## Phase 2 — 2D engine consolidation (incremental)

The shared `gfx::Canvas` now additionally provides:

- `dashedLine()` with deterministic dash/gap lengths.
- `ellipse()` for outline and filled ellipses using a midpoint rasterizer.
- `polyline()` and bounded-degree Bézier curves (2–8 control points).
- `Transform2D` affine translation, scaling, rotation and transformed polylines.
- RGB888/RGB565 conversion helpers and RGB565 byte swapping.
- A fixed-capacity dirty-region list (`consumeDirtyRegions()`). Touching/overlapping
  regions are merged; if the list fills, it safely collapses to one bounding region.
- Cohen–Sutherland line clipping before Bresenham rasterization, avoiding work
  proportional to very distant off-screen endpoints.

The legacy `dirtyRect()` / `consumeDirty()` union API remains available for existing
callers. The dirty-region API is allocation-free and has a capacity of 16 rectangles.
