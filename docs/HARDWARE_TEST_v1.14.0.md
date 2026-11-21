# Collaudo su PicoCalc fisica — v1.14.0 (completamento di 3D Graph)

Questa procedura verifica sul dispositivo il 3D Graph della v1.14.0
([GRAPH3D.md](GRAPH3D.md), [reports/SVILUPPO_v1.14.0.md](reports/SVILUPPO_v1.14.0.md)).
**Nessuna voce è stata provata su hardware**: fino a quando i risultati non
arrivano dal dispositivo nessuna voce è dichiarata provata. Ogni prova ha un
codice: riportare l'esito (OK / KO / non eseguita), le righe di log richieste
e, dove indicato, una foto o il tempo (cronometro o log `[DIAG]`).

Le prove **HW-01…HW-36** ([v1.8.0](HARDWARE_TEST_v1.8.0.md)), **HW-40…HW-82**
([v1.9.0](HARDWARE_TEST_v1.9.0.md)), **HW-90…HW-99** ([v1.10.0](HARDWARE_TEST_v1.10.0.md)),
**HW-100…HW-115** ([v1.11.0](HARDWARE_TEST_v1.11.0.md)), **HW-120…HW-134**
([v1.12.0](HARDWARE_TEST_v1.12.0.md)), **HW-140…HW-159**
([v1.13.0](HARDWARE_TEST_v1.13.0.md)) e **HW-160…HW-169**
([v1.13.1](HARDWARE_TEST_v1.13.1.md)) restano valide e vanno eseguite con il
firmware 1.14.0 (le prove del 3D della v1.9.0, HW-6x, sono sostituite da
quelle qui sotto). Prove aggiuntive: **HW-170…HW-189**, da eseguire sul
**Pimoroni Pico Plus 2 W** e, dove indicato, sul **Raspberry Pi Pico 2 W**.

## 0. Preparazione

1. UF2 della release `v1.14.0` (`picocalc_fxai_pico_plus_2w-v1.14.0.uf2`,
   `picocalc_fxai_pico2w-v1.14.0.uf2`), controllati con `sha256sum -c SHA256SUMS`,
   oppure compilati con `scripts/build.sh --board …`.
2. Log USB dall'accensione (righe `[mem]`, `[psram]`, `[flash]`, `[DIAG]`, `[fault]`).
3. microSD FAT32 (HW-185, HW-186).
4. Un dispositivo con dati della v1.13.1 nel 3D Graph (HW-170).

## 1. Prove

| Codice | Prova | Atteso | Da riportare |
|---|---|---|---|
| HW-170 | Aggiornamento dalla v1.13.1 con funzioni Z1–Z3, finestra e vista non predefinite; spegnimento e riaccensione | le funzioni diventano grafici Z= con la stessa selezione, la stessa finestra, griglia celle + 1, stessa orientazione della vista; nessun errore | foto prima/dopo, log `[flash]` |
| HW-171 | Esempio 1 del manuale (16-2): Sphere FACTOR 0,0,0,2, DRAW | sfera blu con il reticolo nero nella scatola, etichette X/Y/Z e valori, indicatore degli assi | foto, tempo dal DRAW al disegno completo |
| HW-172 | Esempio 2 (16-3): Z=X²+Y²−3, griglia 25 e poi 50 (Plus) | paraboloide tagliato esattamente a Zmax | tempi di calcolo (25 e 50) |
| HW-173 | Tempi di disegno per tipo con griglia 25 e 50: Line, Plane, Sphere, Cylinder, Cone, Z=, Param, Rot X, Rot Y; tre grafici insieme | Plus: z-buffer, ridisegno a ogni rotazione; Pico 2 W: pittore, griglia ≤ 25 | tempo del calcolo e di una rotazione (→) per ogni tipo |
| HW-174 | Rotazione con ← → ↑ ↓ (tenendo premuto), giro completo di φ | rotazione fluida, θ e φ della V-Window seguono, nessun riavvio | foto della V-Window, sensazione di fluidità |
| HW-175 | OPTN › ROTATE › L→R e B→T; AC durante la rotazione | due giri e arresto; AC ferma subito | tempo di due giri |
| HW-176 | AC durante il calcolo di tre grafici Z= con griglia 50 | il calcolo si ferma subito, disegno parziale, tastiera sempre reattiva | foto |
| HW-177 | Z-buffer in PSRAM (Plus): sfera, piano e retta che si intersecano | intersezioni corrette, parti nascoste della retta non disegnate; DIAG MEM mostra la PSRAM usata | foto, log `[mem]` |
| HW-178 | Pittore sul Pico 2 W (o Plus con PSRAM disabilitata): la stessa scena di HW-177 | disegno corretto a meno di piccoli errori di ordine; griglia ridotta a 25 | foto, tempo |
| HW-179 | Memory ERROR: 3 grafici Param con griglia 50 sul Pico 2 W, poi molte liste piene e DRAW | griglia ridotta o «Memory ERROR»; nessun riavvio; dopo EXIT la memoria libera in DIAG torna come prima | foto, log `[mem]` |
| HW-180 | TRACE su Z=X×Y: frecce, Next/Back, tasto x con X=1, Y=−2 | X=0, Y=0, Z=0 all'inizio; il puntatore salta in (1,−2,−2); tempo di un passo con la cache (Plus) | foto, tempo di un passo |
| HW-181 | G-Solve INTSECT con le due rette di 16-18 | X=2, Y=2, Z=2 e il puntatore sul punto | foto |
| HW-182 | G-Solve RELATION con la retta e il piano di 16-19; piano–piano | PARALLEL; la retta d'intersezione disegnata e le sue equazioni | foto |
| HW-183 | G-Solve CROSS X/Y/Z su sfera, cilindro e cono, UP/DOWN, valore digitato | sezioni nel colore opposto, in primo piano | foto |
| HW-184 | SET UP: Axes/Box/Label Off, tasti • e , sul grafico; FORMAT con Area Clear | elementi nascosti/mostrati come nel manuale; reticolo senza superficie | foto |
| HW-185 | OPTN › PICTURE › STORE 3, poi GRAPH con Background PICT 3 | l'immagine del grafico 3D come sfondo di GRAPH | foto, file `PICOCALC/PICT/PICT03.pcpict` |
| HW-186 | CAPTURE (SHIFT 7) dal grafico 3D | file `PICOCALC/CAPT/CAPTnn.pcpict` | nome del file |
| HW-187 | 3D-GMEM STORE 1…20 fino a «Memory ERROR» (Pico 2 W), RECALL, riavvio | dati ripristinati dopo il riavvio; budget 12 KB (Pico 2 W) / 64 KB (Plus) | foto, log `[flash]` |
| HW-188 | 3D-VMEM STORE/RECALL; MEMORY › 3D G-Mem / 3D V-Win / 3D Graph | cancellazioni separate | foto |
| HW-189 | Stack: tutte le schermate del 3D con tre grafici, poi DIAG | nessun `[fault]`, stack massimo sotto il limite | log `[DIAG]`, `[fault]` |

## 2. Esiti

Riportare per ogni codice: data, scheda (Pico Plus 2 W / Pico 2 W,
revisione), esito, tempi, osservazioni. Ogni KO va riprodotto con un test
host prima della correzione; solo le voci provate passano a TESTED ON
HARDWARE in [FEATURE_MATRIX.md](FEATURE_MATRIX.md).
