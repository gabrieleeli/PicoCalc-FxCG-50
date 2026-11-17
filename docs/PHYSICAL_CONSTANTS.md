# Costanti matematiche e fisiche

Dalla v1.14.0 (in sviluppo) tutte le costanti con nome stanno in **una sola
tabella**, `kTable` di `src/physical_constants.cpp` (interfaccia
`include/physical_constants.hpp`): 73 costanti in 7 gruppi, valori in unità SI.
È una tabella `constexpr` di `ConstDef` (in flash sul firmware, nessuna
allocazione) e la leggono tutti i punti che prima avevano una copia propria:

| Chi | Cosa legge | Prima della v1.14.0 |
|---|---|---|
| parser (`expression.cpp`: `isConstantName`, `evalToken`, tokenizzatore, `isReservedWord`) | `findConstant()`: nome e valore | `kConstants[]` (22 nomi) |
| editor naturale e cronologia (`math_renderer` → `calclib::displayName`) | glifo | nessun glifo (si vedeva `physc`) |
| `formatNatural` (`memory.cpp`, etichette di GRAPH) | `findConstantNoCase()`: glifo | `constants[]` (20 nomi) |
| RUN-MATRIX › OPTN › CONST | gruppi e costanti | 10 voci fisse |
| SYMBOLS, pagine 4–10 | gruppi, glifi, nomi, valori, incertezze | pagina "COSTANTI MATEMATICHE" fissa |
| `pointa::physicalConstant()` | nome, alias o glifo | catena di 9 `if` |

```cpp
struct ConstDef {
  const char* parserName;   // minuscolo ASCII, es. "physme"
  const char* altNames;     // alias separati da '|', es. "electronmass" ("" se nessuno)
  const char* glyph;        // UTF-8 mostrato a video, es. "mₑ"
  const char* nameIT;       // "Massa dell'elettrone"
  double value;             // in unità SI
  const char* unit;         // "kg", "J·s", "m⁻¹", ...
  double relUncertainty;    // 0 se esatta
  std::uint8_t group;       // indice in constGroups()
};
std::span<const ConstGroup> constGroups();          // nome, titolo di SYMBOLS, esempio
std::span<const ConstDef> allConstants();
std::span<const ConstDef> constantsOf(std::size_t group);
const ConstDef* findConstant(std::string_view lowerName);   // parserName e altNames
const ConstDef* findConstantNoCase(std::string_view name);  // "NA", "kB", "speedOfLight"
const ConstDef* findByGlyph(std::string_view glyph);        // "c", "mₑ"
```

## Fonti e verifica

- **CODATA 2022**: tabella NIST "Fundamental Physical Constants — Complete
  Listing, 2022 CODATA adjustment" (`physics.nist.gov/cuu/Constants/Table/allascii.txt`),
  scaricata e confrontata riga per riga. `tests/physical_constants_test.cpp`
  contiene le righe originali (valore e incertezza come testo NIST) e controlla
  che ogni valore della tabella sia **identico** al valore NIST (entro 1e-9 per
  le derivate esatte, che NIST stampa troncate con "...") e che l'incertezza
  relativa sia u/|valore| della riga NIST (2 cifre significative).
- **SI 2019**: le 7 costanti definenti (Δν_Cs, c, h, e, k, Nₐ, K_cd) sono esatte.
  Le **derivate esatte** (ħ, Φ₀, G₀, K_J, R_K, R, F, σ, b, c₁, c₂, Vₘ, Vₘ°, n₀)
  sono calcolate `constexpr` da quelle definenti; il test le ricontrolla entro
  1e-12 relativo (b con x di Wien risolto per Newton da x = 5(1 − e⁻ˣ)).
- **IAU**: unità astronomica (2012 B2, esatta), anno luce (c × 365,25 d,
  esatto), parsec (2015 B2: 648000/π au, esatto), valori nominali della
  risoluzione **2015 B3** (verificati sul testo, arXiv:1510.07674):
  R☉ᴺ = 6,957 × 10⁸ m, L☉ᴺ = 3,828 × 10²⁶ W, (GM)☉ᴺ = 1,3271244 × 10²⁰ m³/s²,
  R⊕ᴺ (equatoriale) = 6,3781 × 10⁶ m, (GM)⊕ᴺ = 3,986004 × 10¹⁴ m³/s².
  I valori nominali sono **esatti per definizione** (fattori di conversione,
  non migliori stime): nella tabella hanno incertezza 0. Le masse seguono la
  raccomandazione 5 della risoluzione: M = (GM)ᴺ/G con la G CODATA 2022, quindi
  hanno l'incertezza di G (2,2 × 10⁻⁵); M⊕ = 5,97217 × 10²⁴ kg (5,9722 × 10²⁴
  arrotondato).
- Relazioni tra misurate controllate entro 1e-9: ε₀ = 1/(μ₀c²), Z₀ = μ₀c,
  α = e²/(4πε₀ħc), 1/α, mₚ/mₑ, Eₕ = 2R∞hc, a₀ = ħ/(mₑcα), rₑ = α²a₀, u = M_u/Nₐ.

## Nomi

- **Nomi della v1.13 invariati**, con lo stesso valore, anche con le maiuscole
  usate nei file e nei workspace salvati: `pi tau phi goldenratio eulergamma
  sqrt2 ln2 ln10 speedoflight physc gravitationalconstant physg planckconstant
  physh avogadro na boltzmann kb elementarycharge qe standardgravity g0`
  (e `NA`, `kB`, `speedOfLight`...: il parser ignora le maiuscole). Il loro
  `parserName` resta il nome corto di prima (`na`, `kb`, `qe`, `g0`), i nomi
  lunghi sono alias. `e` (una lettera) resta il numero di Nepero, `E` la variabile.
- **Nomi nuovi**: `phys` + sigla minuscola, solo lettere e cifre (`physme`,
  `physa0`, `physmu0`); alias in inglese per esteso (`electronmass`).
- Regole controllate dal test su tutta la tabella:
  1. nessun nome duplicato;
  2. nessun nome è una funzione, un operatore a parola, `ans`/`preans`/`theta`/`r_var`,
     una variabile di sistema, `List n`, `Str n`, `v_a…v_z` o un riferimento
     `y1…x20` (`isNonConstantName()` di `expression.hpp`), né una lettera singola
     (variabili A–Z, θ, r) tranne `e`;
  3. il tokenizzatore prende il pezzo più lungo: un nome nuovo non allunga un
     nome già riservato di **cifre o di una sola lettera**, perché quelle letture
     della v1.13 restano valide (`physc2` = c·2, `kbT` = k_B·T, `physhF` = h·F,
     `physg0` = G·0, `na2` = Nₐ·2). Per questo G₀ è `physgcond` e non `physg0`,
     c₁ e c₂ sono `physrad1`/`physrad2` e R è `physrgas`;
  4. nessun nome nuovo è l'inizio di un altro nome della tabella (`physme` e
     `physmearth` non possono coesistere: la Terra è `physmterra`, il rapporto
     mₚ/mₑ è `physmratio`, la sezione d'urto di Thomson `physthomson`).

  L'unica lettura della v1.13 che cambia è quella di una sequenza di lettere
  che ora è un nome intero: `physme` prima era il prodotto p·h·y·s·m·e,
  `physhbar` era h·B·A·R. Il confronto differenziale (stesse 6 034 espressioni
  con la libreria v1.13.1 e con quella nuova: tutti i nomi vecchi con
  suffissi di una lettera, una cifra, `*2`, `^2`, `(2)`, prefissi `2`, `x`, `a`)
  ha trovato differenze di valore solo in quei casi.

## Glifi

Il glifo compare nell'editor naturale (Math e Line), nella cronologia di
RUN-MATRIX, nel menu CONST, in SYMBOLS e in `formatNatural`. Scelte:

- **Font**: mancavano ħ, ₕ, ₖ, ₘ, ₙ, ₚ, ⊕ e ☉. Sono stati disegnati e
  aggiunti alle due facce (Small 6×12 e Medium 8×16) come `MANUAL_GLYPHS` di
  `tools/gen_fonts.py`: 8 glifi, 28 byte di bitmap ciascuno più l'indice
  (circa 290 byte di flash). Nessun ripiego ASCII per ☉/⊕ (`M☉`, `R⊕`).
- I pedici che Unicode non ha come lettere minuscole (B, N, J, K, P, C, μ, α,
  d, u, cd) si scrivono con il trattino basso, come il `k_B` già usato:
  `μ_B`, `μ_N`, `K_J`, `R_K`, `m_P`, `l_P`, `t_P`, `T_P`, `λ_C`, `m_μ`, `m_α`,
  `m_d`, `M_u`, `K_cd`, `Δν_Cs`.
- La carica elementare resta **qₑ** (glifo della v1.13; `e` sarebbe ambiguo con
  il numero di Nepero).
- I due volumi molari: **Vₘ** a 101,325 kPa e **Vₘ°** a 100 kPa (pressione
  standard, ° come in chimica).
- `ln2`, `ln10` e `sqrt2` digitati a mano restano `ln 2`, `√ 2` come prima; dal
  menu CONST sono un token con glifo `ln(2)`, `ln(10)`, `√2`.
- Nell'editor una costante è un solo token: le parole di sole lettere si
  uniscono già (`mergeWord`), quelle con cifre (`g0`, `physa0`, `physmu0`) si
  uniscono dalla v1.14.0 alla digitazione della cifra e alla rilettura dalla
  cronologia. Questo corregge anche il richiamo di `g0^2` dalla cronologia, che
  la v1.13.1 rileggeva come `g(0)^(2)` = 0.

## Interfaccia

- **RUN-MATRIX › OPTN › CONST** (ultima voce di OPTN): primo livello i 7
  gruppi (con i primi glifi), secondo livello le costanti `glifo  nome`; EXE
  inserisce il nome del parser come un token (`sym:physme`), EXIT chiude il menu.
- **SYMBOLS**: pagine 4–10, una per gruppo, con scorrimento (6 voci visibili):
  vedi [SYMBOL_CHEATSHEET.md](SYMBOL_CHEATSHEET.md).

## Memoria

Tutto è in flash: `kTable` occupa 3 504 byte (73 × 48), `kGroups` 84 byte,
più nomi, glifi, nomi italiani e unità (oggetto `physical_constants.cpp.o`:
8 904 byte con il codice di ricerca). La SRAM statica non cambia. Immagine
del firmware: +10 804 byte su entrambe le schede rispetto alla v1.13.1
(tabella, font, menu CONST, pagine di SYMBOLS, editor); il Pico 2 W passa dall'87%
all'88% della partizione da 2048 KiB. La ricerca per nome è lineare
(73 voci con gli alias, confronto con la lunghezza per prima): il parser la
fa solo per gli identificatori di almeno due lettere che non sono legati
(la `x` di un grafico non la percorre).

## Aggiungere una costante

1. Una riga in `kTable` (nel suo gruppo: la tabella deve restare ordinata per
   gruppo, lo verifica uno `static_assert`); per una costante CODATA misurata
   il valore copiato dalla tabella NIST e u/|valore| con 2 cifre; per una
   derivata esatta l'espressione `constexpr`.
2. Una riga in `kNist` di `tests/physical_constants_test.cpp` (testo NIST).
3. Se il glifo usa un carattere nuovo, aggiungerlo a `tools/gen_fonts.py` e a
   `src/font_data.cpp` (i test `cheatsheet_smoke` e `physical_constants_test`
   falliscono se un carattere non ha glifo).
4. `physical_constants_test` verifica da solo nome, collisioni, prefissi,
   parser, glifo, menu CONST e pagina di SYMBOLS.

## Tabella completa

### Matematiche

| Glifo | Nome nel parser (alias) | Nome | Valore | Unità | Incertezza relativa | Esatta | Fonte |
|---|---|---|---|---|---|---|---|
| π | `pi` | Pi greco | 3.14159265358979 | — | — | sì | matematica |
| e | `e` | Numero di Nepero | 2.71828182845905 | — | — | sì | matematica |
| τ | `tau` | Tau: giro completo, 2π | 6.28318530717959 | — | — | sì | matematica |
| φ | `phi`, `goldenratio` | Sezione aurea | 1.61803398874989 | — | — | sì | matematica |
| γ | `eulergamma` | Costante di Eulero-Mascheroni | 0.577215664901533 | — | — | sì | matematica |
| √2 | `sqrt2` | Radice di 2 | 1.4142135623731 | — | — | sì | matematica |
| ln(2) | `ln2` | Logaritmo naturale di 2 | 0.693147180559945 | — | — | sì | matematica |
| ln(10) | `ln10` | Logaritmo naturale di 10 | 2.30258509299405 | — | — | sì | matematica |

### Universali

| Glifo | Nome nel parser (alias) | Nome | Valore | Unità | Incertezza relativa | Esatta | Fonte |
|---|---|---|---|---|---|---|---|
| c | `physc`, `speedoflight` | Velocità della luce | 299792458 | m/s | — | sì | SI 2019, costante definente |
| μ₀ | `physmu0`, `magneticconstant` | Permeabilità del vuoto | 1.25663706127 × 10⁻⁶ | N/A² | 1.6 × 10⁻¹⁰ | no | CODATA 2022 |
| ε₀ | `physeps0`, `electricconstant` | Permittività del vuoto | 8.8541878188 × 10⁻¹² | F/m | 1.6 × 10⁻¹⁰ | no | CODATA 2022 |
| Z₀ | `physz0`, `vacuumimpedance` | Impedenza del vuoto | 376.730313412 | Ω | 1.6 × 10⁻¹⁰ | no | CODATA 2022 |
| G | `physg`, `gravitationalconstant` | Costante gravitazionale | 6.6743 × 10⁻¹¹ | m³/(kg·s²) | 2.2 × 10⁻⁵ | no | CODATA 2022 |
| h | `physh`, `planckconstant` | Costante di Planck | 6.62607015 × 10⁻³⁴ | J·s | — | sì | SI 2019, costante definente |
| ħ | `physhbar`, `reducedplanck` | Costante di Planck ridotta | 1.05457181764616 × 10⁻³⁴ | J·s | — | sì | CODATA 2022, derivata esatta (calcolata) |
| m_P | `physplmass`, `planckmass` | Massa di Planck | 2.176434 × 10⁻⁸ | kg | 1.1 × 10⁻⁵ | no | CODATA 2022 |
| l_P | `physpllen`, `plancklength` | Lunghezza di Planck | 1.616255 × 10⁻³⁵ | m | 1.1 × 10⁻⁵ | no | CODATA 2022 |
| t_P | `physpltime`, `plancktime` | Tempo di Planck | 5.391247 × 10⁻⁴⁴ | s | 1.1 × 10⁻⁵ | no | CODATA 2022 |
| T_P | `physpltemp`, `plancktemperature` | Temperatura di Planck | 1.416784 × 10³² | K | 1.1 × 10⁻⁵ | no | CODATA 2022 |

### Elettromagnetiche

| Glifo | Nome nel parser (alias) | Nome | Valore | Unità | Incertezza relativa | Esatta | Fonte |
|---|---|---|---|---|---|---|---|
| qₑ | `qe`, `elementarycharge` | Carica elementare | 1.602176634 × 10⁻¹⁹ | C | — | sì | SI 2019, costante definente |
| Φ₀ | `physphi0`, `fluxquantum` | Quanto di flusso magnetico | 2.06783384846193 × 10⁻¹⁵ | Wb | — | sì | CODATA 2022, derivata esatta (calcolata) |
| G₀ | `physgcond`, `conductancequantum` | Quanto di conduttanza | 7.74809172986365 × 10⁻⁵ | S | — | sì | CODATA 2022, derivata esatta (calcolata) |
| K_J | `physkj`, `josephson` | Costante di Josephson | 483597848416984 | Hz/V | — | sì | CODATA 2022, derivata esatta (calcolata) |
| R_K | `physrk`, `vonklitzing` | Costante di von Klitzing | 25812.8074593045 | Ω | — | sì | CODATA 2022, derivata esatta (calcolata) |
| μ_B | `physmub`, `bohrmagneton` | Magnetone di Bohr | 9.2740100657 × 10⁻²⁴ | J/T | 3.1 × 10⁻¹⁰ | no | CODATA 2022 |
| μ_N | `physmunuc`, `nuclearmagneton` | Magnetone nucleare | 5.0507837393 × 10⁻²⁷ | J/T | 3.2 × 10⁻¹⁰ | no | CODATA 2022 |

### Atomiche e nucleari

| Glifo | Nome nel parser (alias) | Nome | Valore | Unità | Incertezza relativa | Esatta | Fonte |
|---|---|---|---|---|---|---|---|
| α | `physalpha`, `finestructure` | Costante di struttura fine | 0.0072973525643 | — | 1.5 × 10⁻¹⁰ | no | CODATA 2022 |
| 1/α | `physinvalpha` | Inverso di α | 137.035999177 | — | 1.5 × 10⁻¹⁰ | no | CODATA 2022 |
| R∞ | `physrinf`, `rydberg` | Costante di Rydberg | 10973731.568157 | m⁻¹ | 1.1 × 10⁻¹² | no | CODATA 2022 |
| a₀ | `physa0`, `bohrradius` | Raggio di Bohr | 5.29177210544 × 10⁻¹¹ | m | 1.5 × 10⁻¹⁰ | no | CODATA 2022 |
| Eₕ | `physeh`, `hartree` | Energia di Hartree | 4.359744722206 × 10⁻¹⁸ | J | 1.1 × 10⁻¹² | no | CODATA 2022 |
| mₑ | `physme`, `electronmass` | Massa dell'elettrone | 9.1093837139 × 10⁻³¹ | kg | 3.1 × 10⁻¹⁰ | no | CODATA 2022 |
| m_μ | `physmmu`, `muonmass` | Massa del muone | 1.883531627 × 10⁻²⁸ | kg | 2.2 × 10⁻⁸ | no | CODATA 2022 |
| mₚ | `physmp`, `protonmass` | Massa del protone | 1.67262192595 × 10⁻²⁷ | kg | 3.1 × 10⁻¹⁰ | no | CODATA 2022 |
| mₙ | `physmn`, `neutronmass` | Massa del neutrone | 1.67492750056 × 10⁻²⁷ | kg | 5.1 × 10⁻¹⁰ | no | CODATA 2022 |
| m_d | `physmd`, `deuteronmass` | Massa del deutone | 3.3435837768 × 10⁻²⁷ | kg | 3 × 10⁻¹⁰ | no | CODATA 2022 |
| m_α | `physmalpha`, `alphamass` | Massa della particella α | 6.644657345 × 10⁻²⁷ | kg | 3.2 × 10⁻¹⁰ | no | CODATA 2022 |
| mₚ/mₑ | `physmratio`, `protonelectronratio` | Rapporto massa protone/elettrone | 1836.152673426 | — | 1.7 × 10⁻¹¹ | no | CODATA 2022 |
| rₑ | `physre`, `electronradius` | Raggio classico dell'elettrone | 2.8179403205 × 10⁻¹⁵ | m | 4.6 × 10⁻¹⁰ | no | CODATA 2022 |
| λ_C | `physlambdac`, `comptonwavelength` | Lunghezza d'onda Compton | 2.42631023538 × 10⁻¹² | m | 3.1 × 10⁻¹⁰ | no | CODATA 2022 |
| σₑ | `physthomson` | Sezione d'urto Thomson | 6.6524587051 × 10⁻²⁹ | m² | 9.3 × 10⁻¹⁰ | no | CODATA 2022 |
| μₑ | `physmue`, `electronmoment` | Momento magnetico dell'elettrone | -9.2847646917 × 10⁻²⁴ | J/T | 3.1 × 10⁻¹⁰ | no | CODATA 2022 |
| μₚ | `physmup`, `protonmoment` | Momento magnetico del protone | 1.41060679545 × 10⁻²⁶ | J/T | 4.3 × 10⁻¹⁰ | no | CODATA 2022 |
| μₙ | `physmuneu`, `neutronmoment` | Momento magnetico del neutrone | -9.6623653 × 10⁻²⁷ | J/T | 2.4 × 10⁻⁷ | no | CODATA 2022 |
| γₚ | `physgammap`, `protongyromag` | Rapporto giromagnetico del protone | 267522187.08 | s⁻¹·T⁻¹ | 4.1 × 10⁻¹⁰ | no | CODATA 2022 |

### Fisico-chimiche

| Glifo | Nome nel parser (alias) | Nome | Valore | Unità | Incertezza relativa | Esatta | Fonte |
|---|---|---|---|---|---|---|---|
| Nₐ | `na`, `avogadro` | Costante di Avogadro | 6.02214076 × 10²³ | mol⁻¹ | — | sì | SI 2019, costante definente |
| k_B | `kb`, `boltzmann` | Costante di Boltzmann | 1.380649 × 10⁻²³ | J/K | — | sì | SI 2019, costante definente |
| R | `physrgas`, `gasconstant` | Costante dei gas | 8.31446261815324 | J/(mol·K) | — | sì | CODATA 2022, derivata esatta (calcolata) |
| F | `physf`, `faraday` | Costante di Faraday | 96485.33212331 | C/mol | — | sì | CODATA 2022, derivata esatta (calcolata) |
| u | `physu`, `atomicmassunit` | Unità di massa atomica | 1.66053906892 × 10⁻²⁷ | kg | 3.1 × 10⁻¹⁰ | no | CODATA 2022 |
| M_u | `physmolu`, `molarmassconstant` | Costante di massa molare | 0.00100000000105 | kg/mol | 3.1 × 10⁻¹⁰ | no | CODATA 2022 |
| σ | `physsigma`, `stefanboltzmann` | Costante di Stefan-Boltzmann | 5.67037441918443 × 10⁻⁸ | W/(m²·K⁴) | — | sì | CODATA 2022, derivata esatta (calcolata) |
| b | `physb`, `wien` | Costante di Wien | 0.00289777195518517 | m·K | — | sì | CODATA 2022, derivata esatta (calcolata) |
| c₁ | `physrad1` | Prima costante di radiazione | 3.74177185219276 × 10⁻¹⁶ | W·m² | — | sì | CODATA 2022, derivata esatta (calcolata) |
| c₂ | `physrad2` | Seconda costante di radiazione | 0.0143877687750393 | m·K | — | sì | CODATA 2022, derivata esatta (calcolata) |
| Vₘ | `physvmatm` | Volume molare (0 °C, 1 atm) | 0.0224139695450141 | m³/mol | — | sì | CODATA 2022, derivata esatta (calcolata) |
| Vₘ° | `physvmbar` | Volume molare (0 °C, 1 bar) | 0.0227109546414856 | m³/mol | — | sì | CODATA 2022, derivata esatta (calcolata) |
| n₀ | `physn0`, `loschmidt` | Costante di Loschmidt (1 atm) | 2.68678011179844 × 10²⁵ | m⁻³ | — | sì | CODATA 2022, derivata esatta (calcolata) |

### Valori adottati

| Glifo | Nome nel parser (alias) | Nome | Valore | Unità | Incertezza relativa | Esatta | Fonte |
|---|---|---|---|---|---|---|---|
| g₀ | `g0`, `standardgravity` | Gravità standard | 9.80665 | m/s² | — | sì | CGPM 1901 (CODATA 2022) |
| atm | `physatm` | Atmosfera standard | 101325 | Pa | — | sì | CGPM 1954 (CODATA 2022) |
| Δν_Cs | `physdnucs`, `cesiumfrequency` | Frequenza del cesio 133 | 9192631770 | Hz | — | sì | SI 2019, costante definente |
| K_cd | `physkcd`, `luminousefficacy` | Efficacia luminosa | 683 | lm/W | — | sì | SI 2019, costante definente |
| T₀ | `physt0` | Temperatura di 0 °C | 273.15 | K | — | sì | SI, zero della scala Celsius |

### Astronomiche

| Glifo | Nome nel parser (alias) | Nome | Valore | Unità | Incertezza relativa | Esatta | Fonte |
|---|---|---|---|---|---|---|---|
| au | `physau`, `astronomicalunit` | Unità astronomica | 149597870700 | m | — | sì | IAU 2012 B2 |
| ly | `physly`, `lightyear` | Anno luce | 9.4607304725808 × 10¹⁵ | m | — | sì | IAU: c × 365,25 d (calcolata) |
| pc | `physpc`, `parsec` | Parsec | 3.08567758149137 × 10¹⁶ | m | — | sì | IAU 2015 B2: 648000/π au (calcolata) |
| GM☉ | `physgmsun` | Parametro GM del Sole | 1.3271244 × 10²⁰ | m³/s² | — | sì | IAU 2015 B3, valore nominale |
| M☉ | `physmsun`, `solarmass` | Massa del Sole | 1.98840987069805 × 10³⁰ | kg | 2.2 × 10⁻⁵ | no | IAU 2015 B3: GMᴺ/G, G CODATA 2022 (calcolata) |
| R☉ | `physrsun`, `solarradius` | Raggio del Sole | 695700000 | m | — | sì | IAU 2015 B3, valore nominale |
| L☉ | `physlsun`, `solarluminosity` | Luminosità del Sole | 3.828 × 10²⁶ | W | — | sì | IAU 2015 B3, valore nominale |
| GM⊕ | `physgmterra` | Parametro GM della Terra | 398600400000000 | m³/s² | — | sì | IAU 2015 B3, valore nominale |
| M⊕ | `physmterra`, `earthmass` | Massa della Terra | 5.97216786779138 × 10²⁴ | kg | 2.2 × 10⁻⁵ | no | IAU 2015 B3: GMᴺ/G, G CODATA 2022 (calcolata) |
| R⊕ | `physrterra`, `earthradius` | Raggio equatoriale della Terra | 6378100 | m | — | sì | IAU 2015 B3, valore nominale |
