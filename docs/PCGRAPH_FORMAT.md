# Formati dei file GRAPH (v1.8.0)

Formati originali del progetto, documentati qui e implementati in
`src/graph_file.cpp` (progetti e memorie grafiche) e `src/pict.cpp`
(immagini). Non riproducono formati Casio (`g3m`, `g3p`). Tutti i numeri
interi multi-byte dei formati binari sono **little-endian**; il CRC è il
CRC-32 IEEE 802.3 di `storage::crc32` (lo stesso del workspace in flash).

## Cartelle sulla microSD

| Contenuto | Percorso | Nome |
|---|---|---|
| Progetti GRAPH | `PICOCALC/GRAPH/` | `NOME.pcgraph`, 1–16 caratteri `A–Z a–z 0–9 _ -` |
| Immagini PICT 1–20 | `PICOCALC/PICT/` | `PICT01.pcpict` … `PICT20.pcpict` |
| Immagini con nome, sfondi | `PICOCALC/PICT/` | `NOME.pcpict`, 1–8 caratteri |
| Esportazioni BMP | `PICOCALC/PICT/` | `NOME.bmp`, 1–8 caratteri |

I nomi sono confrontati senza distinzione tra maiuscole e minuscole (FAT).
Le cartelle sono create alla prima scrittura (`makeDirs`).

## Scrittura atomica e recupero

Ogni file è scritto con `files::writeAtomic`:

1. scrittura completa in `NOME.ext.tmp`;
2. rilettura di `NOME.ext.tmp` e confronto del CRC con i dati in memoria;
3. il file esistente diventa `NOME.ext.bak`;
4. `NOME.ext.tmp` → `NOME.ext`;
5. eliminazione di `NOME.ext.bak`.

Un'interruzione (scheda estratta, alimentazione) lascia sempre o il file
vecchio o quello nuovo. `files::recover`, chiamata all'apertura del gestore
file, completa o annulla l'operazione interrotta:

- `.tmp` con il file principale presente: interruzione prima del passo 3, il
  `.tmp` è eliminato;
- `.tmp` senza file principale ma con `.bak`: il `.tmp` era già verificato
  (passo 2) e diventa il file principale, il `.bak` è eliminato;
- `.tmp` senza file principale né `.bak`: primo salvataggio non concluso, il
  `.tmp` è eliminato;
- `.bak` rimasto: eliminato se il file principale esiste, altrimenti ripristinato.

Prima di salvare un progetto si controlla lo spazio libero
(`FileSystem::freeSpace`); una scrittura parziale (`Status::Full`) non tocca il
file esistente. Test: `graph_file_test` (fault injection su `RamFileSystem`),
`sd_fatfs_test` (stessa sequenza su FatFs e scheda SD simulata).

## Progetto `.pcgraph` (testo, versione 1)

Testo ASCII/UTF-8, una voce `chiave=valore` per riga, terminatore di riga
`\n` (un eventuale `\r` finale è ignorato in lettura). Dimensione massima
64 KB (`graphfile::MaxProjectBytes`). Esempio reale prodotto da
`graphfile::encode` (Y1 = x³ − 4x, una parametrica, una memoria V-Window, una
tangente):

```
PCGRAPH 1
# PicoCalc FX-CG50 AI - progetto GRAPH (formato documentato in docs/PCGRAPH_FORMAT.md)
generator=PicoCalc FX-CG50 AI 1.8.0
angle=Rad
view=-6.2999999999999998;6.2999999999999998;1;-3.1000000000000001;3.1000000000000001;1;0;6.2831853071795862;0.052359877559829883
functype=Y=
draw=Connect
derivative=Off
coord=On
grid=Off
axes=On
label=Off
graphfunc=On
simul=Off
ineq=Union
sketchline=Normal
plotcolor=Blue
factor=2;2
background=0
fade=0
vmem1=-2;2;1;-1;1;1;0;6.2831853071795862;0.10471975511965977
table=1;5;1
f1=Y=|On|Blue|Normal|(x)^(3)-4x|
f3=Param|On|Green|Thick|3cos(t)|1.5sin(2t)
s=Tangent|1;-3;1;-1|Blue|Normal||
end
crc32=76C89E50
```

| Riga | Contenuto |
|---|---|
| `PCGRAPH <n>` | intestazione obbligatoria; `n` = versione del formato. Un file con `n` maggiore della versione supportata è rifiutato ("creato da una versione più recente") |
| `# ...` | commento, ignorato |
| `generator=` | versione del firmware che ha scritto il file (informativa) |
| `angle=` | `Deg`, `Rad`, `Gra` |
| `view=` | V-Window: `Xmin;Xmax;Xscale;Ymin;Ymax;Yscale;Tmin;Tmax;Tpitch` (9 numeri `%.17g`, round-trip esatto dei double); obbligatoria e validata (`validateView`) |
| `functype=` | tipo delle righe nuove: `Y=`, `r=`, `Param`, `X=`, `Y>`, `Y<`, `Y>=`, `Y<=`, `X>`, `X<`, `X>=`, `X<=` |
| `draw=` | `Connect` o `Plot` |
| `derivative=`, `coord=`, `label=`, `graphfunc=`, `simul=` | `On` / `Off` |
| `grid=` | `On`, `Off`, `Line` |
| `axes=` | `On`, `Off`, `Scale` |
| `ineq=` | `Union` o `Intsect` |
| `sketchline=` | stile: `Normal`, `Thick`, `Broken`, `Dot`, `Thin` |
| `plotcolor=` | colore: `Blue`, `Red`, `Green`, `Magenta`, `Black`, `Cyan`, `Yellow` |
| `factor=` | Xfact;Yfact dello ZOOM FACTOR (> 0) |
| `background=` | 0 = nessuno, 1–20 = Pict n, 21 = file (`bgfile`) |
| `bgfile=` | percorso dell'immagine di sfondo (≤ 64 caratteri, testo con escape) |
| `fade=` | 0–100 (%) |
| `vmem1=` … `vmem6=` | memorie V-Window usate (stesso formato di `view`) |
| `table=` | Start;End;Step di TABLE (Step > 0, Start ≤ End) |
| `overlay=` | percorso dell'immagine richiamata (RECALL) |
| `f<n>=` | funzione `n` (1–20): `tipo|selezione|colore|stile|espressione|Yt`; le righe vuote non sono scritte; `Yt` solo per `Param` |
| `s=` | elemento SKETCH (max 64): `tipo|x0;y0;x1;y1|colore|stile|testo|percorso` |
| `end` | fine dei dati (obbligatoria); testo dopo `end` è un errore |
| `crc32=` | CRC-32 di tutti i byte dal primo carattere di `PCGRAPH` fino al `\n` di `end` compreso, 8 cifre esadecimali maiuscole |

**Testo con escape** (espressioni, testi, percorsi): `%`, `|`, i caratteri di
controllo (< 0x20) e 0x7F sono scritti come `%XX` (esadecimale). I caratteri
UTF-8 (π, θ, √, ...) restano invariati. Le espressioni sono nel formato
lineare dell'editor (`(x)^(3)`, `@Y1` per i riferimenti ad altre funzioni,
`, [a,b]` per l'intervallo, `, [A=1,2,3]` per la sovrapposizione), al massimo
1000 caratteri (`MaxGraphExpression`).

**Elementi SKETCH** (`tipo` = `Plot`, `PlotOff`, `Line`, `Tangent`, `Normal`,
`Circle`, `Vertical`, `Horizontal`, `Text`, `Pen`, `Inverse`): coordinate
matematiche; per `Tangent`/`Normal` `x1;y1` è la direzione, per `Circle` `x1` è
il raggio; per `Text` il campo testo (≤ 32 caratteri) è la scritta, per
`Inverse` è l'espressione del grafico invertito (ridisegnato come X = f(y));
`percorso` (solo `Pen`) = punti `x,y` separati da `;`, max 400.

**Lettura**: CRC, intestazione e versione sono controllati prima di
interpretare i dati; ogni valore è validato (tipi, colori, stili, numeri finiti,
V-Window, limiti); qualsiasi errore rifiuta l'intero file con un messaggio
("Storage ERROR: NOME non valido (motivo)") e il progetto aperto non cambia.
Le chiavi sconosciute sono ignorate (compatibilità con revisioni minori future
dello stesso numero di versione). Le **memorie grafiche** (GPH-MEM) e i dati
delle altre modalità non fanno parte del progetto.

**Contenuto non salvato nel progetto**: le immagini (sono file `.pcpict`
separati, il progetto ne conserva il percorso), le memorie grafiche (sono della
calcolatrice), i grafici incollati e i punti marcati con EXE (temporanei, come
sulla CG50).

## Memoria grafica `PCGMEM` (testo, versione 1)

Le 20 memorie GPH-MEM sono salvate nel workspace in flash (record v3), non
sulla microSD. Ogni memoria è un testo con la stessa sintassi:

```
PCGMEM 1
view=...
f1=Y=|On|Blue|Normal|sin(x)|
end
crc32=XXXXXXXX
```

Spazio totale per le 20 memorie: 24 KB (`MaxGraphMemoryBytes`); oltre si ha
"Memory ERROR".

## Immagine `.pcpict` (binario, versione 1)

| Offset | Dimensione | Campo |
|---|---|---|
| 0 | 8 | magic `50 43 50 49 43 54 0D 0A` (`"PCPICT\r\n"`) |
| 8 | 2 | versione del formato (1) |
| 10 | 2 | larghezza in pixel (1–4096; GRAPH: 320) |
| 12 | 2 | altezza in pixel (1–4096; GRAPH: 282) |
| 14 | 1 | flag: bit 0 = V-Window presente (altri bit = 0) |
| 15 | 1 | Fade I/O dello sfondo (0–100) |
| 16 | 72 | V-Window: 9 double IEEE 754 (Xmin, Xmax, Xscale, Ymin, Ymax, Yscale, Tmin, Tmax, Tpitch) |
| 88 | 4 | `n` = lunghezza dei dati RLE (≤ 96 KB) |
| 92 | n | dati RLE |
| 92+n | 4 | CRC-32 di tutti i byte precedenti |

**Dati RLE**: per ogni riga, dall'alto in basso, sequenze di coppie
`conteggio (u16), colore (u16 RGB565)`; una sequenza non attraversa la fine
della riga e la somma dei conteggi di ogni riga è uguale alla larghezza.
Il bianco `0xFFFF` è trasparente quando l'immagine è richiamata sopra un
grafico (RECALL), ed è opaco quando l'immagine è usata come sfondo.

L'immagine contiene l'area del grafico (320×282, senza barra di stato né menu
funzione) come appare sullo schermo: sfondo, griglia, assi, grafici, sketch.

## Esportazione BMP

BMP Windows 3.x non compresso: intestazione di 54 byte (`BITMAPFILEHEADER` +
`BITMAPINFOHEADER`), 24 bit per pixel, altezza negativa (righe dall'alto in
basso), righe allineate a 4 byte, 2835 pixel/m. Per 320×282: 54 + 960 × 282 =
270 774 byte. I colori RGB565 sono espansi a 8 bit per canale replicando i
bit alti.

## Versionamento

- Un cambiamento incompatibile incrementa la versione (`PCGRAPH 2`,
  `PCGMEM 2`, versione binaria 2 di `.pcpict`); il firmware rifiuta le versioni
  più recenti di quella che conosce e legge tutte le precedenti.
- Un'aggiunta compatibile (nuova chiave) non cambia la versione: i firmware
  precedenti la ignorano.
