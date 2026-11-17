# App SYMBOLS (guida ai simboli)

L'app **SYMBOLS** (tasto `N` del menu principale) è una guida in sola lettura
ai simboli, alle funzioni e alle costanti accettati dall'editor e dal parser.
Il contenuto è in `include/cheatsheet.hpp`; le pagine delle costanti sono
generate dalla tabella unica `include/physical_constants.hpp`
(vedi [PHYSICAL_CONSTANTS.md](PHYSICAL_CONSTANTS.md)).

## Tasti

| Tasto | Azione |
|---|---|
| **F1 ◀PAG** / **←** | pagina precedente (gira dall'ultima alla prima) |
| **F2 PAG▶** / **→** | pagina successiva |
| **↑ / ↓** | voce precedente / successiva della pagina; le pagine con più di 6 voci scorrono (barra a destra) |
| **EXIT** / **F6** | torna al menu principale |

Il riquadro in basso mostra l'esempio della voce selezionata.

## Pagine (23)

| # | Titolo | Voci |
|---|---|---|
| 1 | ARITMETICA | 6 |
| 2 | POTENZE E RADICI | 6 |
| 3 | LETTERE GRECHE | 6 |
| 4 | COSTANTI MATEMATICHE | 8 (π, e, τ, φ, γ, √2, ln 2, ln 10) |
| 5 | COSTANTI FISICHE: UNIVERSALI | 11 |
| 6 | COSTANTI FISICHE: ELETTROMAGNETICHE | 7 |
| 7 | COSTANTI FISICHE: ATOMICHE E NUCLEARI | 19 |
| 8 | COSTANTI FISICHE: FISICO-CHIMICHE | 13 |
| 9 | COSTANTI FISICHE: VALORI ADOTTATI | 5 |
| 10 | COSTANTI ASTRONOMICHE (IAU) | 10 |
| 11 | FUNZIONI | 6 |
| 12 | CALCOLO | 6 |
| 13 | INSIEMI E LOGICA | 6 |
| 14 | STATISTICA | 6 |
| 15 | GRAFICI | 6 |
| 16 | COMPLESSI / MATRICI | 6 |
| 17 | ALTRE FUNZIONI | 6 |
| 18 | COMBINATORIA | 6 |
| 19 | UNITA' DI MISURA | 6 |
| 20 | NOTAZIONE | 6 |
| 21 | GRECHE MAIUSCOLE | 6 |
| 22 | FORMATI RISULTATO | 6 |
| 23 | CONFRONTI E FRAZIONI | 6 |

## Pagine delle costanti (v1.14.0)

Le pagine 4–10 non contengono dati propri: ogni pagina è un gruppo della
tabella delle costanti (`constGroup` di `help::Page`), quindi simboli, nomi e
valori sono sempre quelli del parser.

- **Simbolo**: il glifo mostrato anche nell'editor e nella cronologia (`mₑ`, `ħ`, `M☉`).
- **Significato**: il nome italiano.
- **Riquadro**:
  - riga piccola: il nome da scrivere nelle espressioni e gli alias (`Nome: physme   alias: electronmass`);
  - `= valore unità` con 15 cifre significative (`= 9.1093837139e-31 kg`);
  - `Esatta (per definizione)` oppure `Incertezza relativa 3.1e-10` (CODATA 2022), `Costante matematica` per il gruppo Matematiche;
  - l'esempio del gruppo, per esempio `Es.: physme*physc^2 ≈ 8.187e-14 J`.

Gli esempi dei gruppi sono verificati da `tests/physical_constants_test.cpp`
(l'espressione a sinistra, valutata dal parser, deve dare il valore a destra).

## Supporto

Le pagine di simboli sono un riferimento di notazione: non ogni voce è
un'espressione accettata dal parser (insiemi, limiti e alcune notazioni sono
solo spiegazioni). Le pagine delle costanti, invece, elencano solo nomi
accettati dal parser e inseribili anche da RUN-MATRIX › OPTN › CONST.
L'app non fa calcoli, non usa la rete e non alloca le tabelle: sono dati
costanti in flash; il riquadro compone tre righe di testo al momento del
disegno.
