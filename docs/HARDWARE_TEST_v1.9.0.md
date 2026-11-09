# Collaudo su PicoCalc fisica — v1.9.0 (Pimoroni Pico Plus 2 W)

Questa procedura porta le voci da *TESTED ON HOST* a *TESTED ON HARDWARE*.
**Fino a quando i risultati non arrivano dal dispositivo nessuna voce è
dichiarata provata su hardware.** Ogni prova ha un codice: riportare l'esito
(OK / KO / non eseguita), le righe di log richieste e, dove indicato, una foto.

La v1.9.0 contiene tutte le funzioni della v1.8.0: le prove **HW-01…HW-36** di
[HARDWARE_TEST_v1.8.0.md](HARDWARE_TEST_v1.8.0.md) restano valide e vanno
ripetute con il firmware 1.9.0 (sulla Plus e, se disponibile, sul Pico 2 W).
Qui sotto le prove aggiuntive della v1.9.0 (**HW-40 e seguenti**).

## 0. Preparazione

1. Firmware Plus: `scripts/build.sh --board pico_plus_2w` →
   `build/pico_plus_2w/picocalc_fxai_pico_plus_2w.uf2`. Tenere premuto **BOOT**
   sul Pico Plus 2 W collegando l'USB-C e copiare l'UF2 sull'unità `RP2350`.
   (Pico 2 W: `scripts/build.sh --board pico2w`, UF2 `picocalc_fxai_pico2w.uf2`.)
2. Log USB seriale (USB CDC): `screen /dev/ttyACM0 115200` o PuTTY. Salvare il
   log completo **dall'accensione**: contiene le righe `[psram]`, `[mem]`,
   `[fault]` dell'avvio e le righe `[DIAG]`.
3. Indicare: revisione della mainboard, modulo (Pico Plus 2 W / Pico 2 W),
   versione mostrata da SYSTEM → F1 INFO.

## 1. Avvio, PSRAM e memoria

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-40 | Accendere con il log USB aperto | `[psram] APS6404L MF=0x0D KGD=0x5D EID=0x.., 8192 KiB, SCK 75000000 Hz (div 2), self-test N us`; `[flash] JEDEC xx xx 18: chip 16384 KiB, build layout 16384 KiB`; righe `[mem] --- boot ---` con "PSRAM usata …/8.0M" | log di avvio |
| HW-41 | SYSTEM → F1 INFO | "PSRAM xxxx KB liberi di 8191 KB"; nessun "DISABILITATA" | foto |
| HW-42 | SYSTEM → F2 DIAG, righe **MEM** | "PSRAM: rilevata si, abilitata si, MF 0x0D KGD 0x5D …"; "clk_sys 150 MHz, div 2, SCK 75 MHz"; "self-test di avvio OK"; "Limiti RAM: SRAM 520 KB + PSRAM 8 MB"; "chip 16.0M (JEDEC …)"; "STACK core0 x/32K … guardia MSPLIM attiva"; "integrita' dei pool TLSF: OK" | log `[DIAG]` |
| HW-43 | Righe **PSR** | "PSRAM 256 KB: bus dati OK, pattern OK, cache/uncached OK" | log `[DIAG]` |
| HW-44 | Righe **ALC** | "0 corruzioni"; "2 core: core0 0 corruzioni, core1 completato con 0 corruzioni"; "integrita' dei pool OK" | log `[DIAG]` |
| HW-45 | Righe **FLS** | "flash: … verifica OK"; "PSRAM dopo la scrittura flash: con cache OK, diretta OK, ancora scrivibile si"; stato della configurazione M1 | log `[DIAG]` (servono i tempi) |
| HW-46 | Righe **LCD** | tempo di un aggiornamento completo del LCD (atteso ≈ 100–140 ms) e della copia del layer GRAPH dalla PSRAM (atteso pochi ms) | log `[DIAG]` |
| HW-47 | Righe **INT** (flash interna) | "flash interna: LittleFS 11.0 MB …"; scrittura/lettura 64 KB "dati identici"; ".pcgraph salvato e riletto" | log `[DIAG]` |
| HW-48 (solo Pico 2 W) | SYSTEM → F2 DIAG | "PSRAM: non presente su questa scheda"; nessun errore dovuto alla PSRAM; "LittleFS 1.9 MB"; "chip 4.0M (JEDEC …)" | log `[DIAG]` |
| HW-49 (facoltativa, Pico 2 W con dati salvati) | Copiare **di proposito** `picocalc_fxai_pico_plus_2w.uf2` sul Pico 2 W, accendere, poi reinstallare `picocalc_fxai_pico2w.uf2` | con l'UF2 sbagliato: "System ERROR: UF2 per Pimoroni Pico Plus 2 W su una flash da 4 MB…", nel log `[flash] … CHIP TOO SMALL`; nessun salvataggio. Dopo il ripristino: firmware e dati del Pico 2 W intatti | log + foto + OK/KO |

## 2. Prestazioni e limiti con la PSRAM

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-50 | Prova HW-17 della v1.8.0 (20 parametriche dense, Tpitch 0.0005) | il disegno termina **senza** "disegno parziale" (con la v1.8.0 / Pico 2 W compare) | tempo del disegno completo |
| HW-51 | Sul grafico di HW-50: F1 TRACE, tenere premuto → per 3 s | il cursore si muove fluido (cache dei layer in PSRAM); annotare la reattività | impressione + eventuale video |
| HW-52 | STAT: inserire 999 valori in List 1 (o TABLE 0..998 + LISTMEM) | nessun "Dimension ERROR" (liste da 999 sulla Plus) | OK/KO |
| HW-53 | Run-Matrix: 100 calcoli, spegnere e riaccendere | la cronologia c'è ancora (fino a 200 righe sulla Plus) | OK/KO |
| HW-54 | PICTURE → STORE di un grafico molto "rumoroso" (es. disequazioni sovrapposte + sfondo) | salvataggio senza "Memory ERROR: immagine troppo complessa" | OK/KO |

## 3. 3D Graph

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-60 | MENU → `J` (3D Graph), F6 DRAW con Z1 = sin(x)cos(y) | "Calcolo delle superfici… %" poi la superficie blu sfumata con l'altezza; intestazione "griglia 40 z-buffer" (come `docs/screenshots/41_graph3d_surfaces.png` del simulatore) | foto, tempo del calcolo |
| HW-61 | ← → ↑ ↓, F1/F2 zoom, F5 VIEW | rotazione/zoom senza ricalcolo (immediati); annotare il tempo di ridisegno | impressione |
| HW-62 | Z2 = x^2-y^2 (dopo x² premere →), V-WIN: Griglia 128, F6 | due superfici (Z1 blu, Z2 rossa) con intersezione corretta; tempo del calcolo | foto + tempo |
| HW-63 | F4 AUTO | la scatola si adatta all'intervallo z | foto |
| HW-64 (Pico 2 W) | come HW-60 | "griglia 24 pittore" | foto |

## 4. Flash, archivio e aggiornamenti

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-70 | SYSTEM: "Archivio file" = Flash interna; GRAPH → FILE → SAVE AS "INTERNO" senza microSD | salvataggio riuscito; FILE → OPEN lo mostra | foto |
| HW-71 | Spegnere/riaccendere; OPEN "INTERNO" | progetto intatto | OK/KO |
| HW-72 | Con dati nel workspace (variabili, liste, 3D), aggiornare il firmware copiando di nuovo l'UF2 | dopo il riavvio i dati ci sono ancora (il record non usa l'ultimo settore, erratum RP2350-E10) | OK/KO |
| HW-73 (Pico 2 W con dati della v1.8.0) | Installare la 1.9.0 sopra una 1.8.0 usata | variabili, storia, liste, GRAPH della v1.8.0 presenti | OK/KO |
| HW-74 (facoltativa) | Togliere l'alimentazione 1 s dopo una modifica (autosalvataggio in corso) | al riavvio il workspace precedente o quello nuovo, mai dati corrotti | OK/KO |

## 5. Guasti e stabilità

| Codice | Operazioni | Risultato atteso | Da inviare |
|---|---|---|---|
| HW-80 | Uso normale per 30 minuti (GRAPH, TABLE, 3D, AI Chat se configurata) | nessun blocco; nessuna riga `[fault]` al riavvio | log |
| HW-81 | Dopo un eventuale riavvio spontaneo | finestra "System ERROR: riavvio dopo …" e riga `[fault] … CFSR … PC …` nel log | log + foto (serve il PC per l'analisi) |
| HW-82 | Wi-Fi e AI Chat (con relay configurato) | risposta del relay; nessun errore TLS | foto |

## 6. Rapporto da inviare

```
Mainboard: v2.0   Modulo: Pimoroni Pico Plus 2 W   Firmware (SYSTEM > INFO): 1.9.0
HW-01..HW-36 (procedura v1.8.0): esiti...
HW-40: OK/KO  note...
...
Log USB seriale: allegato (dall'accensione, righe [psram] [mem] [DIAG] [fault])
Foto: elenco
```

Ogni KO viene analizzato e corretto con un test che lo riproduca; le voci OK
passano a TESTED ON HARDWARE in [FEATURE_MATRIX.md](FEATURE_MATRIX.md) con data,
modulo e revisione della scheda.
