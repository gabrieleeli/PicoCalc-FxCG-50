# Collaudo su PicoCalc fisica — v1.8.0 (GRAPH, microSD, TABLE)

> Con la v1.9.0 queste prove restano valide e vanno ripetute con l'UF2 della
> propria scheda (`picocalc_fxai_pico_plus_2w.uf2` o `picocalc_fxai_pico2w.uf2`,
> preparazione in [HARDWARE_TEST_v1.9.0.md](HARDWARE_TEST_v1.9.0.md)); sulla
> Plus i limiti di memoria sono più alti (HW-17 non deve più dare "disegno
> parziale", vedi HW-50).

Questa procedura serve a portare le voci da *TESTED ON HOST* a *TESTED ON
HARDWARE*. **Fino a quando i risultati non arrivano dal dispositivo, nessuna
voce è dichiarata provata su hardware.** Ogni prova ha un codice (HW-xx):
riportare per ciascuna l'esito (OK / KO / non eseguita), le righe di log
richieste e, dove indicato, una foto dello schermo.

## 0. Preparazione

1. Firmware: `build/pico2w/picocalc_fxai.uf2` (oppure `scripts/build_pico2w.sh`).
   Tenere premuto BOOTSEL sul Pico 2 W, collegarlo via USB e copiare l'UF2
   nell'unità `RP2350`.
2. Log USB seriale (115200 baud, il valore non conta per USB CDC):
   - Linux/macOS: `screen /dev/ttyACM0 115200` (o `/dev/tty.usbmodem*`);
   - Windows: PuTTY sulla porta COM del Pico.
   Salvare il log completo: contiene le righe `[DIAG] ...`.
3. Schede microSD da provare (quelle disponibili):
   - **A**: SDHC 4–32 GB formattata FAT32 dal computer;
   - **B**: SDXC 64 GB o più formattata exFAT (se disponibile);
   - **C** (facoltativa): una scheda senza file system (es. dopo
     `wipefs`/"pulizia" del disco) per il messaggio "non formattata".
   **Usare schede senza dati importanti**: le prove scrivono file di prova
   (cancellati alla fine) e la prova facoltativa HW-28 estrae la scheda durante una scrittura.
4. Indicare nel rapporto: revisione della mainboard (v2.0), marca/capacità
   delle schede, versione mostrata da SYSTEM > INFO.

## 1. Avvio e diagnostica automatica

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-01 | Accendere. MENU → `G` (SYSTEM) → F1 INFO | "Firmware PicoCalc FX 1.8.0", RAM libera ≈ 180–195 KB | foto |
| HW-02 | Senza scheda: SYSTEM → F2 DIAG | prima riga "SD scheda: scheda microSD assente" (in rosso); righe GPH tutte "+" | log `[DIAG]` |
| HW-03 | Inserire la scheda A, SYSTEM → F2 DIAG | intestazione "DIAGNOSTICA: tutto OK"; "SDHC/SDXC xx GB, FAT32"; scrittura e lettura 64 KB con velocità; "dati identici"; "progetto .pcgraph: salvato e riletto"; "file di prova eliminati" | log `[DIAG]` + foto |
| HW-04 | Ripetere HW-03 con la scheda B (exFAT) | come HW-03 con "exFAT" | log `[DIAG]` |
| HW-05 | Scheda C: SYSTEM → F2 DIAG | "scheda non formattata o file system non supportato: formattarla in FAT32 o exFAT dal computer" | foto |
| HW-06 | Righe GPH della diagnostica (con qualsiasi scheda) | "campionamento: N ms, … valutazioni"; "G-Solve ROOT … 3 radici"; "integrale … = 4" | log `[DIAG]` (servono i tempi reali) |

## 2. GRAPH

Confrontare con gli screenshot del simulatore in `docs/screenshots/`.

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-10 | GRAPH: Y1 = x³−4x, Y2 = 2sin(x), r3 = 2+cos(3θ), Xt4/Yt4 = 3cos(t)/1.5sin(2t), Y5 ≥ x²−5; DRAW | come `22_graph_multi.png` (colori, campitura, curve continue, nessuna linea verticale spuria) | foto, tempo approssimato del disegno completo (s) |
| HW-11 | Durante il disegno di HW-10 premere subito F1 (TRACE), poi EXIT | il TRACE risponde senza attendere la fine del disegno | OK/KO |
| HW-12 | Y1 = tan(x), DRAW; Y1 = 1/x, DRAW; Y1 = ln(x), DRAW | nessun segmento che attraversa gli asintoti; ln parte dal bordo inferiore vicino a x = 0 | foto |
| HW-13 | Y1 = x³−4x, DRAW, F5 G-SOLV → ROOT (←/→), MAX, MIN, ∫dx da −2 a 0 | radici −2, 0, 2; MAX x = −1.154700538, y = 3.079201436; ∫dx = 4 | foto del ∫dx |
| HW-14 | Y1 = 1/(x−1): G-SOLVE ∫dx da 0 a 2 | "Math ERROR: integrale divergente" | foto |
| HW-15 | Sul grafico di x³−4x: F4 SKETCH → Tangent, digitare 1, EXE | retta y = −x − 2 (passa per (0,−2) e (−2,0)) | foto |
| HW-16 | ZOOM BOX, IN, OUT, AUTO, SQUARE; tasti cursore (scorrimento) | ridisegno corretto dopo ogni operazione | OK/KO |
| HW-17 | Disegnare 20 funzioni parametriche dense (es. Xt = 6sin(37t), Yt = 3cos(41t), Tpitch 0.0005), poi SYSTEM → INFO | il disegno termina (eventuale "Memory ERROR: … disegno parziale" una volta), nessun blocco; RAM libera indicata | foto + RAM libera |

## 3. microSD con GRAPH

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-20 | Scheda A. Progetto di HW-10: TOOL → FILE → SAVE AS "PROVA1" | toast "Progetto salvato"; FILE → OPEN mostra PROVA1 con dimensione e spazio libero | foto della lista |
| HW-21 | Spegnere e riaccendere; GRAPH → FILE → NEW → OPEN PROVA1 | funzioni, colori, stili, V-Window, Setup, SKETCH come prima | OK/KO |
| HW-22 | Modificare Y2, FILE → SAVE; riaprire | la modifica è presente | OK/KO |
| HW-23 | Estrarre la scheda, reinserirla (senza altre operazioni), FILE → OPEN | la lista compare (la scheda viene reinizializzata) | OK/KO |
| HW-24 | Con la scheda estratta: SAVE AS | "Storage ERROR: scheda microSD assente" | foto |
| HW-25 | PICTURE → STORE 1, RECALL 1; PICTURE → Esporta BMP "IMG1" | sul PC `PICOCALC/PICT/IMG1.bmp` si apre ed è l'area del grafico 320×282 | file BMP |
| HW-26 | Sul PC: copiare la scheda o almeno `PICOCALC/GRAPH/*.pcgraph`; eseguire `fsck.fat -n` (Linux) o "Controlla errori" (Windows) | nessun errore del file system; i `.pcgraph` sono testo leggibile (formato in `docs/PCGRAPH_FORMAT.md`) | esito del controllo, file `.pcgraph` |
| HW-27 | Sul PC: `build/host/sd_image_tool - sample SAMPLE.pcgraph`, copiarlo in `PICOCALC/GRAPH/` sulla scheda; sul dispositivo FILE → OPEN SAMPLE | Y1 = x³−4x, Y2 = 2sin(x) spesso, parametrica tratteggiata, Y4 ≥ x²−5, griglia a linee, Derivative On | foto |
| HW-28 (facoltativa) | Durante FILE → SAVE AS di un progetto, estrarre la scheda | errore "Storage ERROR"; reinserita la scheda, il file precedente (se esisteva) si apre integro; nessun `.tmp` resta dopo l'apertura della lista | OK/KO |

## 4. TABLE

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-30 | MENU → 7 TABLE. Y1 = x²−3x (dopo x² premere →), F5 SET: −2, 2, 0.5; F6 TABLE | 9 righe; x = −2 → 10; x = −0.5 → 1.75 | foto |
| HW-31 | F4 EDIT, `pi/4` | x = 0.7854, Y1 aggiornata; il valore completo in basso | foto |
| HW-32 | F3 ROW → INSERT, ADD, DELETE | righe duplicate/eliminate come descritto in `docs/TABLE.md` | OK/KO |
| HW-33 | OPTN → LISTMEM, List 1; poi STAT | List 1 contiene la colonna | OK/KO |
| HW-34 | SET 0, 998, 1; TABLE; tasto END e ↑/↓ veloci | 999 righe, scorrimento fluido (annotare eventuale lentezza) | tempo percepito |
| HW-35 | F5 GPH-CON, poi F6 (G⇔T) o EXIT | il grafico per punti collegati; ritorno alla tabella | foto |
| HW-36 | Spegnere e riaccendere, MENU → 7 | la tabella (anche con le righe modificate) è ancora presente | OK/KO |

## 5. Rapporto da inviare

```
Mainboard: v2.0   Pico 2 W   Firmware (SYSTEM > INFO): 1.8.0
Schede: A = ...GB FAT32 (marca ...), B = ...GB exFAT, C = ...
HW-01: OK/KO  note...
...
Log USB seriale: allegato (righe [DIAG])
Foto: elenco
```

Dopo l'invio: ogni KO viene analizzato e corretto; le voci OK passano a
TESTED ON HARDWARE in `docs/FEATURE_MATRIX.md` e `docs/GRAPH_PARITY.md`, con
data e revisione della scheda.
