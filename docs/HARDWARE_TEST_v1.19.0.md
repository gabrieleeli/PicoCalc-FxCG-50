# Collaudo su PicoCalc fisica — v1.19.0 (Geometry alla parità)

Questa procedura verifica sul dispositivo la v1.19.0
([reports/SVILUPPO_v1.19.0.md](reports/SVILUPPO_v1.19.0.md)): sulla **Pimoroni
Pico Plus 2 W** Geometry alla parità con il capitolo 14 ([GEOMETRY.md](GEOMETRY.md)),
con la sezione `0x0110` versione 2 del record v12 e i file `.pcgeo` versione 2.
Il **Raspberry Pi Pico 2 W è congelato**: si controlla solo che sia rimasto
quello della v1.16.0. **Nessuna voce è stata provata su hardware**: fino a
quando i risultati non arrivano dal dispositivo nessuna voce è dichiarata
provata. Riportare per ogni prova l'esito (OK / KO / non eseguita), le righe di
log richieste e, dove indicato, una foto o un tempo.

Le prove **HW-01…HW-271** delle versioni precedenti
([v1.8.0](HARDWARE_TEST_v1.8.0.md) … [v1.18.0](HARDWARE_TEST_v1.18.0.md))
restano valide e vanno eseguite con il firmware 1.19.0. Prove aggiuntive:
**HW-272…HW-295**, sulla Plus salvo dove indicato.

## 0. Preparazione

1. UF2 della release `v1.19.0` (`picocalc_fxai_pico_plus_2w-v1.19.0.uf2`,
   `picocalc_fxai_pico2w-v1.19.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Log USB dall'accensione (righe `[mem]`, `[psram]`, `[flash]`, `[DIAG]`, `[fault]`).
3. Un dispositivo con la **v1.18.0** installata e una costruzione di Geometry
   (punti, un segmento, una circonferenza, un poligono) aperta e salvata in un
   file `.pcgeo` sulla microSD (formato 1 della v1.17.0).
4. microSD FAT32; un cronometro per i tempi.

## 1. Prove

| # | Prova | Atteso | Da riportare |
|---|---|---|---|
| HW-272 | Aggiornamento dalla v1.18.0 alla v1.19.0 sulla Plus; Geometry | la costruzione del record si apre come punti liberi e figure legate; nessun avviso di sezione danneggiata; icona senza il triangolo Base | foto, log `[flash]` |
| HW-273 | **Pico 2 W**: aggiornamento alla v1.19.0 | stesse modalità e schermate della v1.18.0 (23 icone, Geometry «non disponibile»), immagine di 1 944 228 B | foto del menu |
| HW-274 | File › Open del `.pcgeo` della v1.17.0 dalla microSD; Save as; Open del file nuovo; DELETE (F1 nell'elenco) | file migrato; il file nuovo si rilegge uguale; cancellazione con conferma | foto, file |
| HW-275 | Menu F1–F6 con le schede (◀ ▶), scelta con le cifre e con EXE, EXIT; Key Help (tre pagine) | menu leggibili, nessun ritardo percepibile | foto |
| HW-276 | Puntatore: frecce tenute premute (ripetizione), SHIFT ×4, salti 1–9; vista che segue il puntatore | movimento fluido; nessun tasto perso | tempo di attraversamento dello schermo |
| HW-277 | Draw: Point (anche su un segmento e su una circonferenza), Line Segment, Infinite Line, Ray, Vector, Circle, Arc, SemiCirc, 3-Point Arc | figure corrette; punti legati che restano sulla curva | foto |
| HW-278 | Draw Spec: Triangle, Isosc Triangle, Rectangle, Square, Polygon (chiuso e aperto), Regular n-gon (3 e 12), Function f(x); spostamento dei vertici vincolati | le figure restano isoscele, rettangoli, quadrati, regolari | foto |
| HW-279 | Construct: le otto voci su un triangolo; spostamento di un vertice | tutte le costruzioni seguono il vertice; segni di parallelismo | foto |
| HW-280 | Casi degeneri: due rette rese parallele, due cerchi resi concentrici, due punti sovrapposti | gli oggetti dipendenti spariscono e tornano; nessun blocco | foto |
| HW-281 | Transform: le sei voci (esempi di GEOMETRY.md §10); spostamento dell'originale, dell'asse, del centro, del vettore | immagini legate; etichette con l'apice | foto |
| HW-282 | Casella di misura: OPTN/EXIT, tipi per ogni selezione della tabella di GEOMETRY.md §6, Sì/No di incidenza, tangenza, congruenza | valori come sul PC (`geometry_test`); unità di lunghezza | foto |
| HW-283 | Valore scritto (lunghezza 5, angolo 90, coordinate, raggio) e spostamento dei punti con le misure bloccate | misure esatte; il punto trascinato si muove quasi dove indicato | foto |
| HW-284 | **Risolutore con molti blocchi**: 20 segmenti con la lunghezza bloccata, poi uno spostamento e un valore scritto; AC durante il calcolo | tempo di risposta; AC = Break e figura com'era | tempo, foto |
| HW-285 | Blocchi incompatibili (due punti fissi e una lunghezza diversa) | «Math ERROR: vincoli incompatibili», nulla cambia | foto |
| HW-286 | PASTE, Label, Expression con @n, Number Format, Area Calc | testi aggiornati mentre si spostano i punti | foto |
| HW-287 | FORMAT (SHIFT 5): i cinque tratti, i sette colori, aree Normal e Lighter, colori del testo | leggibili sul pannello reale (RGB666) | foto |
| HW-288 | View: Zoom Box, Pan, Scroll, Zoom In/Out, Zoom to Fit, V-Window; Axes On/Scale/Off; Grid On/Line/Off | finestra corretta; griglia e numeri leggibili | foto |
| HW-289 | Undo/Redo dopo un disegno, una cancellazione, uno spostamento; Clear All non annullabile | stato ripristinato | foto |
| HW-290 | Store Picture in Pict 3, Recall Picture come sfondo, Fade I/O 60 %, Clear Picture; Pict 3 aperta in Graph | immagine identica; sfondo schiarito | foto |
| HW-291 | **Animazione**: 3 punti animati con 100 passi, Trace, Go (once) e Go (repeat), AC | fluidità (fotogrammi al secondo), tracce visibili, punti a posto dopo AC | tempo di un giro, foto |
| HW-292 | Tabella: Add Table (lunghezza e coordinate), Display Table, STORE › LIST 3, STORE › STAT | List 3 e List 1… in Statistics con i valori della tabella | foto |
| HW-293 | **1 000 oggetti** (n-goni e costruzioni): ricalcolo dopo uno spostamento, disegno, Memory ERROR al 1 001° | tempo di ridisegno; nessuna perdita | tempo, log `[mem]` |
| HW-294 | Batteria tolta durante un'animazione e durante un valore scritto; riaccensione | record v12 leggibile (slot A/B); costruzione com'era prima | log `[flash]` |
| HW-295 | Ritorno alla v1.18.0 dopo la v1.19.0 sulla Plus | la v1.18.0 isola la sezione `0x0110` versione 2 («Data ERROR» di Geometry) senza perdere il resto; tornando alla v1.19.0 la costruzione si rilegge dal file `.pcgeo` salvato | esito |
