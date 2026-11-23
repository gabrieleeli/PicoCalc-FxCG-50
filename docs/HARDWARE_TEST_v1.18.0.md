# Collaudo su PicoCalc fisica — v1.18.0 (Graph e Table alla parità completa)

Questa procedura verifica sul dispositivo la v1.18.0
([reports/SVILUPPO_v1.18.0.md](reports/SVILUPPO_v1.18.0.md)): Graph e Table
alla parità completa con la fx-CG50 su **entrambe le schede** (Pimoroni Pico
Plus 2 W e Raspberry Pi Pico 2 W), con il Dual Screen G+G, GtoT e T+G, le
immagini a due livelli, le cartelle delle immagini e le liste da 999 elementi
anche sul Pico 2 W. **Nessuna voce è stata provata su hardware**: fino a quando
i risultati non arrivano dal dispositivo nessuna voce è dichiarata provata.
Riportare per ogni prova l'esito (OK / KO / non eseguita), le righe di log
richieste e, dove indicato, una foto o un tempo.

Le prove **HW-01…HW-245** delle versioni precedenti
([v1.8.0](HARDWARE_TEST_v1.8.0.md) … [v1.17.0](HARDWARE_TEST_v1.17.0.md))
restano valide e vanno eseguite con il firmware 1.18.0. Prove aggiuntive:
**HW-246…HW-265**, su **entrambe le schede** salvo dove indicato.

## 0. Preparazione

1. UF2 della release `v1.18.0` (`picocalc_fxai_pico_plus_2w-v1.18.0.uf2`,
   `picocalc_fxai_pico2w-v1.18.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Log USB dall'accensione (righe `[mem]`, `[psram]`, `[flash]`, `[DIAG]`, `[fault]`).
3. Un dispositivo per scheda con la **v1.17.0** installata e dati in Graph
   (funzioni, V-Window, SKETCH), Table (tabella generata) ed eActivity
   (un documento con una striscia Graph).
4. microSD FAT32 con `PICOCALC/PICT/PICT01.pcpict` salvato dalla v1.17.0.

## 1. Prove

| Codice | Prova | Atteso | Da riportare |
|---|---|---|---|
| HW-246 | Aggiornamento dalla v1.17.0 alla v1.18.0 su entrambe le schede, spegnimento e riaccensione | tutti i dati restano; Graph e Table si aprono come prima; il Pico 2 W ha le stesse modalità della v1.17.0 | foto dei menu |
| HW-247 | Pico 2 W: SYSTEM → INFO e DIAG dopo HW-246; Graph e Table aperte e chiuse | immagine 1 972 608 B (≤ 2 015 232 B, [BUILD_STATUS.md](BUILD_STATUS.md)); heap libero come la v1.17.0 ± 2 KB; record v11 | foto, righe `[DIAG]`, `[mem]` |
| HW-248 | Graph, SET UP › Dual Screen = G+G; V-Window del principale (−2…2) e, con F6 SOTTO, del sotto-schermo (−4…4); Y1 = x(x+1)(x−1); DRAW, OPTN › COPY | due riquadri 320×140 separati da una linea; la curva nel principale, poi in entrambi con le due scale; nessun pixel del principale nel riquadro sotto | foto |
| HW-249 | G+G: SWAP, SELECT su una riga «E» o «S», ZOOM › BOX nel principale | indicatori S/E nella lista; SELECT li toglie; lo zoom cambia solo il riquadro sotto | foto |
| HW-250 | G+G: PAN, PICTURE › STORE, incolla (Ctrl+V), MODIFY | finestra d'errore «non disponibile con il Dual Screen» per ognuna; nessun blocco | foto |
| HW-251 | Tempo del primo disegno di due grafici densi in G+G (`sin(40x)` in entrambi i riquadri) con SYSTEM → DIAG › tempi di GRAPH | tempo totale e reattività della tastiera durante il disegno (AC, EXIT); confronto con lo schermo pieno | tempi |
| HW-252 | Dual Screen = GtoT: TRACE su Y1 = x²−3, EXE in tre punti, ↓ su Y2 = −x+1, EXE; OPTN › CHANGE; R•DEL; LISTMEM in List 3; DEL•ALL | righe nella tabella sotto con le celle di 6 caratteri; punti con etichetta; List 3 in Statistics | foto |
| HW-253 | GtoT con Derivative On | colonne Y1′ nella tabella del sotto-schermo | foto |
| HW-254 | TABLE, SET UP › Dual Screen = T+G; TABLE, GPH-CON, TRACE ←/→; OPTN › CHANGE; AC | tabella sotto e grafico sopra; la riga della tabella segue il puntatore; il grafico resta sopra la tabella attiva | foto |
| HW-255 | TRACE a schermo pieno su Y = x³−4x | coordinate in basso e accanto al puntatore; SHIFT ↓ nasconde le linee ausiliarie, SHIFT ↑ le mostra; il tasto x apre la finestra del valore | foto |
| HW-256 | SKETCH › Tangent, OPTN (FORMAT) Thick e Red, più tangenti con EXE; SKETCH › PLOT › Plot con FORMAT Green | le tangenti seguono il FORMAT; un'operazione nuova riparte da Sketch Line e Plot/LineCol | foto |
| HW-257 | Editor di Graph e di Table: OPTN › ANGLE (°, ʳ, ᵍ) con Angle = Deg; Y1 = sin xʳ; G-SOLVE ROOT tra 2 e 4 | il simbolo appare nell'espressione; la radice è π | foto |
| HW-258 | PICTURE › SAVE•AS: NEW F «LAB», apri la cartella, SAVE•AS «WAVE»; OPEN dalla cartella; Background › OPEN nella cartella | `PICOCALC/PICT/LAB/WAVE.pcpict` sul PC; il richiamo e lo sfondo funzionano | foto, `ls` sul PC |
| HW-259 | Immagine a due livelli: Grid Line, STORE Pict 2, poi Background = PICT 2 e Fade I/O 100 % con il cursore (←/→ a passi di 5) | la griglia salvata scompare, la curva salvata resta nitida; il valore di Fade è nel file (riaperto dopo il riavvio) | foto |
| HW-260 | Un `PICT01.pcpict` della v1.17.0 come sfondo e in eActivity; un'immagine nuova (v2) in eActivity e in Picture Plot (Plus) | le immagini v1 e v2 si vedono uguali allo schermo che le ha salvate | foto |
| HW-261 | LISTMEM di una tabella da 999 righe (Start 1, End 999) in List 1 su entrambe le schede; poi una seconda lista da 999 sul Pico 2 W | List 1 con 999 valori; sul Pico 2 W la seconda dà «Memory ERROR» (totale 1 530) | foto |
| HW-262 | Run-Matrix in Line: `Graph Y=sin`, `Graph r=cos` | i grafici di sin x e cos θ | foto |
| HW-263 | Spegnimento e riaccensione con G+G, GtoT (tabella con righe) e T+G attivi | Dual Screen, V-Window del sotto-schermo, indicatori e tabella del GtoT tornano | foto |
| HW-264 | Progetto `.pcgraph` con Dual Screen salvato e riaperto; lo stesso file aperto con la v1.17.0 | la v1.18.0 ritrova il Dual Screen; la v1.17.0 apre il progetto (le chiavi nuove sono ignorate) | esiti |
| HW-265 | Ritorno alla v1.17.0 dopo la v1.18.0 (Dual Screen Off e On, una lista da 999 sul Pico 2 W) | con Dual Screen Off e liste ≤ 255 il record si legge; la sezione del Dual Screen viene conservata dalla v1.17.0; sul Pico 2 W un record con una lista > 255 non si legge (limite noto, [STORAGE.md](STORAGE.md)) | esito |
