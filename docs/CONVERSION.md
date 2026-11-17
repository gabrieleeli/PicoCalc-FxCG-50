# CONVERSION — categorie, unita', fattori e fonti (v1.14.0)

<!-- Generato da tests/conversion_v114_test.cpp dalla tabella di src/units.cpp e src/unit_formats.cpp:
     il test fallisce se questo file non coincide. Per rigenerarlo: FXAI_UPDATE_DOCS=1 build/host-pico_plus_2w/conversion_v114_test -->

CONVERSION e Run-Matrix (OPTN › CONVERT e la sintassi `valore[unita']▶[unita']`) usano **una sola tabella**
(`include/units.hpp`, `src/units.cpp`), in flash. Totale: **77 categorie numeriche, 771 unita'** (di cui 106 della fx-CG50, le sole del menu CONVERT di Run-Matrix), 32 prefissi, 19 conversioni di formato e le valute.

Modello: lineare `s = v × fattore`; affine `s = (v + offset) × fattore`; inverso `s = fattore / v`;
funzione `s = f(v)` con inversa. `s` e' il valore nell'unita' di riferimento della categoria (SI o coerente).
Costanti: SI 2019 esatte (c, h, e, k, N_A), NIST SP 811 (2008) per le unita' anglosassoni, **CODATA 2018** per le
costanti misurate (la stessa base della fx-CG50 per u), IAU 2012/2015 per le grandezze astronomiche.
Colonna *Esatto*: testo razionale usato per i risultati esatti (`pi` = π), `decimale` se il fattore scritto e' esatto.

## Fonti

| # | Fonte |
|---|---|
| 0 | esatto per definizione (SI 2019, NIST SP 811 App. B) |
| 1 | valore della fx-CG50 (invariato) |
| 2 | NIST SP 811 (2008) App. B.8-B.9, arrotondato |
| 3 | CODATA 2018 |
| 4 | IAU 2012 B2 / IAU 2015 B2-B3 (valore nominale) |
| 5 | valore astronomico medio (IERS / Explanatory Supplement) |
| 6 | atmosfera standard ISO 2533 (15 gradi C, livello del mare) |
| 7 | valore storico convenzionale (approssimato) |
| 8 | formula o tabella convenzionale approssimata |
| 9 | convenzione internazionale (esatta per accordo) |

## Prefissi

| Simbolo | Nome | Fattore |
|---|---|---|
| q | quecto | 10^-30 |
| r | ronto | 10^-27 |
| y | yocto | 10^-24 |
| z | zepto | 10^-21 |
| a | atto | 10^-18 |
| f | femto | 10^-15 |
| p | pico | 10^-12 |
| n | nano | 10^-9 |
| µ | micro | 10^-6 |
| m | milli | 10^-3 |
| c | centi | 10^-2 |
| d | deci | 10^-1 |
| da | deca | 10^1 |
| h | etto | 10^2 |
| k | chilo | 10^3 |
| M | mega | 10^6 |
| G | giga | 10^9 |
| T | tera | 10^12 |
| P | peta | 10^15 |
| E | exa | 10^18 |
| Z | zetta | 10^21 |
| Y | yotta | 10^24 |
| R | ronna | 10^27 |
| Q | quetta | 10^30 |
| Ki | kibi | 2^10 |
| Mi | mebi | 2^20 |
| Gi | gibi | 2^30 |
| Ti | tebi | 2^40 |
| Pi | pebi | 2^50 |
| Ei | exbi | 2^60 |
| Zi | zebi | 2^70 |
| Yi | yobi | 2^80 |

I prefissi si applicano (F4 PREFIX, o scritti nel simbolo in Run-Matrix: `[nPa]`, `[TiB]`) alle unita' marcate
*SI* o *IEC* nella colonna Note.

## Base SI

### Lunghezza (fx-CG50: LENGTH)

Riferimento: `m`, 47 unita'. px: punti per pollice impostabili (PARAM).

| Simbolo | Nome | Conversione in m | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `fm` | femtometro | v × 1e-15 | decimale | 0 | fx-CG50 |
| `pm` | picometro | v × 1e-12 | decimale | 0 |  |
| `Å` | angstrom | v × 1e-10 | decimale | 0 | fx-CG50 |
| `a0` | raggio di Bohr | v × 5.29177210903e-11 | — | 3 |  |
| `nm` | nanometro | v × 1e-09 | decimale | 0 |  |
| `µm` | micrometro | v × 1e-06 | decimale | 0 | fx-CG50 |
| `mm` | millimetro | v × 0.001 | decimale | 0 | fx-CG50 |
| `cm` | centimetro | v × 0.01 | decimale | 0 | fx-CG50 |
| `dm` | decimetro | v × 0.1 | decimale | 0 |  |
| `m` | metro | v × 1 | decimale | 0 | fx-CG50, SI |
| `dam` | decametro | v × 1e+01 | decimale | 0 |  |
| `hm` | ettometro | v × 1e+02 | decimale | 0 |  |
| `km` | chilometro | v × 1e+03 | decimale | 0 | fx-CG50 |
| `Mm` | megametro | v × 1e+06 | decimale | 0 |  |
| `Gm` | gigametro | v × 1e+09 | decimale | 0 |  |
| `R_T` | raggio terrestre equatoriale | v × 6.3781e+06 | — | 4 |  |
| `d_TL` | distanza media Terra-Luna | v × 3.84399e+08 | — | 5 |  |
| `AU` | unita' astronomica | v × 1.495978707e+11 | decimale | 0 | fx-CG50 |
| `l.y.` | anno luce | v × 9.4607304725808e+15 | decimale | 0 | fx-CG50 |
| `pc` | parsec | v × 3.085677581491367e+16 | — | 4 | fx-CG50 |
| `Mpc` | megaparsec | v × 3.085677581491367e+22 | — | 4 |  |
| `Gpc` | gigaparsec | v × 3.0856775814913673e+25 | — | 4 |  |
| `Mil` | mil (millesimo di pollice) | v × 2.54e-05 | decimale | 0 | fx-CG50 |
| `thou` | thou (= mil) | v × 2.54e-05 | decimale | 0 |  |
| `in` | pollice | v × 0.0254 | decimale | 0 | fx-CG50 |
| `hand` | palmo (hand, 4 in) | v × 0.1016 | decimale | 0 |  |
| `ft` | piede | v × 0.3048 | decimale | 0 | fx-CG50 |
| `ft(US)` | piede US survey | v × 0.3048006096012192 | 1200/3937 | 0 |  |
| `yd` | iarda | v × 0.9144 | decimale | 0 | fx-CG50 |
| `fath` | braccio (fathom) | v × 1.8288 | decimale | 0 | fx-CG50 |
| `link` | link (1/100 chain) | v × 0.201168 | decimale | 0 |  |
| `rd` | pertica (rod) | v × 5.0292 | decimale | 0 | fx-CG50 |
| `chain` | catena (chain, 66 ft) | v × 20.1168 | decimale | 0 |  |
| `furlong` | furlong (1/8 mile) | v × 201.168 | decimale | 0 |  |
| `mile` | miglio | v × 1609.344 | decimale | 0 | fx-CG50 |
| `league` | lega (3 miglia) | v × 4828.032 | decimale | 0 |  |
| `n mile` | miglio nautico | v × 1852 | decimale | 0 | fx-CG50 |
| `cable` | cavo (1/10 miglio nautico) | v × 185.2 | decimale | 0 |  |
| `nleague` | lega nautica (3 miglia nautiche) | v × 5556 | decimale | 0 |  |
| `pt` | punto tipografico (DTP, 1/72 in) | v × 0.00035277777777777776 | 0.0254/72 | 0 |  |
| `pica` | pica (12 pt) | v × 0.004233333333333333 | 0.0254/6 | 0 |  |
| `twip` | twip (1/20 pt) | v × 1.763888888888889e-05 | 0.0254/1440 | 0 |  |
| `px` | pixel (1 in / DPI) | v × 0.0254 / DPI | — | 0 |  |
| `l_P` | lunghezza di Planck | v × 1.616255e-35 | — | 3 |  |
| `pes` | piede romano | v × 0.296 | — | 7 | approssimata, storica |
| `passus` | passo romano (5 piedi) | v × 1.48 | — | 7 | approssimata, storica |
| `m.p.` | miglio romano (mille passus) | v × 1.48e+03 | — | 7 | approssimata, storica |

### Area (fx-CG50: AREA)

Riferimento: `m²`, 23 unita'.

| Simbolo | Nome | Conversione in m² | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `b` | barn | v × 1e-28 | decimale | 0 |  |
| `cmil` | circular mil | v × 5.067074790974977e-10 | — | 0 |  |
| `mm²` | millimetro quadrato | v × 1e-06 | decimale | 0 |  |
| `cm²` | centimetro quadrato | v × 0.0001 | decimale | 0 | fx-CG50 |
| `dm²` | decimetro quadrato | v × 0.01 | decimale | 0 |  |
| `m²` | metro quadrato | v × 1 | decimale | 0 | fx-CG50 |
| `a` | ara | v × 1e+02 | decimale | 0 |  |
| `dam²` | decametro quadrato | v × 1e+02 | decimale | 0 |  |
| `hm²` | ettometro quadrato | v × 1e+04 | decimale | 0 |  |
| `ha` | ettaro | v × 1e+04 | decimale | 0 | fx-CG50 |
| `km²` | chilometro quadrato | v × 1e+06 | decimale | 0 | fx-CG50 |
| `in²` | pollice quadrato | v × 0.00064516 | decimale | 0 | fx-CG50 |
| `ft²` | piede quadrato | v × 0.09290304 | decimale | 0 | fx-CG50 |
| `yd²` | iarda quadrata | v × 0.83612736 | decimale | 0 | fx-CG50 |
| `rood` | rood (1/4 acro) | v × 1011.7141056 | decimale | 0 |  |
| `acre` | acro | v × 4046.8564224 | decimale | 0 | fx-CG50 |
| `mile²` | miglio quadrato | v × 2589988.110336 | decimale | 0 | fx-CG50 |
| `section` | sezione (1 miglio² US survey) | v × 2589998.470319521 | 6336000/3937*6336000/3937 | 0 |  |
| `township` | township (36 sezioni) | v × 93239944.93150276 | 36*6336000/3937*6336000/3937 | 0 |  |
| `pert.MI` | pertica milanese | v × 654.5179 | — | 7 | approssimata, storica |
| `torn.BO` | tornatura bolognese | v × 2080.44 | — | 7 | approssimata, storica |
| `mogg.NA` | moggio napoletano | v × 3364.86 | — | 7 | approssimata, storica |
| `giorn.PI` | giornata piemontese | v × 3809.96 | — | 7 | approssimata, storica |

### Volume (fx-CG50: VOLUME)

Riferimento: `m³`, 43 unita'.

| Simbolo | Nome | Conversione in m³ | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `minim` | minim US | v × 6.1611519921875e-08 | decimale | 0 |  |
| `mm³` | millimetro cubo | v × 1e-09 | decimale | 0 |  |
| `cm³` | centimetro cubo | v × 1e-06 | decimale | 0 | fx-CG50 |
| `mL` | millilitro | v × 1e-06 | decimale | 0 | fx-CG50 |
| `cL` | centilitro | v × 1e-05 | decimale | 0 |  |
| `dL` | decilitro | v × 0.0001 | decimale | 0 |  |
| `L` | litro | v × 0.001 | decimale | 0 | fx-CG50, SI |
| `dm³` | decimetro cubo | v × 0.001 | decimale | 0 |  |
| `hL` | ettolitro | v × 0.1 | decimale | 0 |  |
| `kL` | chilolitro | v × 1 | decimale | 0 |  |
| `m³` | metro cubo | v × 1 | decimale | 0 | fx-CG50 |
| `st` | stero (legna) | v × 1 | decimale | 0 |  |
| `ML` | megalitro | v × 1e+03 | decimale | 0 |  |
| `km³` | chilometro cubo | v × 1e+09 | decimale | 0 |  |
| `in³` | pollice cubo | v × 1.6387064e-05 | decimale | 0 | fx-CG50 |
| `ft³` | piede cubo | v × 0.028316846592 | decimale | 0 | fx-CG50 |
| `yd³` | iarda cuba | v × 0.764554857984 | decimale | 0 |  |
| `cord` | cord (128 ft³) | v × 3.624556363776 | decimale | 0 |  |
| `RT` | tonnellata di stazza (100 ft³) | v × 2.8316846592 | decimale | 0 |  |
| `ac•ft` | acro-piede | v × 1233.48183754752 | decimale | 0 |  |
| `fl_oz(UK)` | oncia fluida UK | v × 2.84130625e-05 | decimale | 0 | fx-CG50 |
| `fl_oz(US)` | oncia fluida US | v × 2.95735295625e-05 | decimale | 0 | fx-CG50 |
| `fl_dr` | dramma fluida US | v × 3.6966911953125e-06 | decimale | 0 |  |
| `gill(US)` | gill US | v × 0.00011829411825 | decimale | 0 |  |
| `gill(UK)` | gill UK | v × 0.0001420653125 | decimale | 0 |  |
| `gal(US)` | gallone US | v × 0.003785411784 | decimale | 0 | fx-CG50 |
| `gal(UK)` | gallone UK | v × 0.00454609 | decimale | 0 | fx-CG50 |
| `bbl` | barile di petrolio (42 gal US) | v × 0.158987294928 | decimale | 0 |  |
| `bbl(US)` | barile US liquidi (31,5 gal) | v × 0.119240471196 | decimale | 0 |  |
| `bu(US)` | bushel US | v × 0.03523907016688 | decimale | 0 |  |
| `pk(US)` | peck US | v × 0.00880976754172 | decimale | 0 |  |
| `bu(UK)` | bushel UK | v × 0.03636872 | decimale | 0 |  |
| `pk(UK)` | peck UK | v × 0.00909218 | decimale | 0 |  |
| `pt` | pinta US | v × 0.000473176473 | decimale | 0 | fx-CG50 |
| `pt(UK)` | pinta UK | v × 0.00056826125 | decimale | 0 |  |
| `qt` | quarto US | v × 0.000946352946 | decimale | 0 | fx-CG50 |
| `qt(UK)` | quarto UK | v × 0.0011365225 | decimale | 0 |  |
| `tsp` | cucchiaino US | v × 4.92892159375e-06 | decimale | 0 | fx-CG50 |
| `tbsp` | cucchiaio US | v × 1.478676478125e-05 | decimale | 0 | fx-CG50 |
| `cup` | tazza US | v × 0.0002365882365 | decimale | 0 | fx-CG50 |
| `tsp(m)` | cucchiaino metrico | v × 5e-06 | decimale | 0 |  |
| `tbsp(m)` | cucchiaio metrico | v × 1.5e-05 | decimale | 0 |  |
| `cup(m)` | tazza metrica | v × 0.00025 | decimale | 0 |  |

### Tempo (fx-CG50: TIME)

Riferimento: `s`, 25 unita'. anno gregoriano 365,2425 d; mese medio 30,436875 d.

| Simbolo | Nome | Conversione in s | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `t_P` | tempo di Planck | v × 5.391247e-44 | — | 3 |  |
| `fs` | femtosecondo | v × 1e-15 | decimale | 0 |  |
| `ps` | picosecondo | v × 1e-12 | decimale | 0 |  |
| `ns` | nanosecondo | v × 1e-09 | decimale | 0 | fx-CG50 |
| `shake` | shake | v × 1e-08 | decimale | 0 |  |
| `µs` | microsecondo | v × 1e-06 | decimale | 0 | fx-CG50 |
| `ms` | millisecondo | v × 0.001 | decimale | 0 | fx-CG50 |
| `s` | secondo | v × 1 | decimale | 0 | fx-CG50, SI |
| `min` | minuto | v × 6e+01 | decimale | 0 | fx-CG50 |
| `h` | ora | v × 3.6e+03 | decimale | 0 | fx-CG50 |
| `day(sid)` | giorno sidereo | v × 86164.0905 | — | 5 |  |
| `day` | giorno | v × 8.64e+04 | decimale | 0 | fx-CG50 |
| `week` | settimana | v × 6.048e+05 | decimale | 0 | fx-CG50 |
| `fortnight` | quindicina (14 giorni) | v × 1.2096e+06 | decimale | 0 |  |
| `mese` | mese medio (30,436875 d) | v × 2629746 | decimale | 0 |  |
| `mese(sin)` | mese sinodico (29,530588853 d) | v × 2551442.8768992 | — | 5 |  |
| `yr` | anno (365 d) | v × 3.1536e+07 | decimale | 0 | fx-CG50 |
| `s-yr` | anno solare (tropico 1900) | v × 31556925.9747 | — | 1 | fx-CG50 |
| `t-yr` | anno siderale (fx-CG50) | v × 31558149.76 | — | 1 | fx-CG50 |
| `yr(jul)` | anno giuliano (365,25 d) | v × 3.15576e+07 | decimale | 0 |  |
| `yr(greg)` | anno gregoriano (365,2425 d) | v × 31556952 | decimale | 0 |  |
| `yr(sid)` | anno sidereo J2000 (365,256363004 d) | v × 31558149.7635456 | — | 5 |  |
| `decennio` | decennio (gregoriano) | v × 3.1556952e+08 | decimale | 0 |  |
| `secolo` | secolo (gregoriano) | v × 3.1556952e+09 | decimale | 0 |  |
| `millennio` | millennio (gregoriano) | v × 3.1556952e+10 | decimale | 0 |  |

### Massa (fx-CG50: MASS)

Riferimento: `kg`, 27 unita'.

| Simbolo | Nome | Conversione in kg | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `m_e` | massa dell'elettrone | v × 9.1093837015e-31 | — | 3 |  |
| `u` | unita' di massa atomica | v × 1.6605390666e-27 | — | 3 | fx-CG50 |
| `Da` | dalton (= u) | v × 1.6605390666e-27 | — | 3 |  |
| `m_p` | massa del protone | v × 1.67262192369e-27 | — | 3 |  |
| `ng` | nanogrammo | v × 1e-12 | decimale | 0 |  |
| `µg` | microgrammo | v × 1e-09 | decimale | 0 |  |
| `mg` | milligrammo | v × 1e-06 | decimale | 0 | fx-CG50 |
| `ct` | carato metrico | v × 0.0002 | decimale | 0 |  |
| `gr` | grano | v × 6.479891e-05 | decimale | 0 |  |
| `g` | grammo | v × 0.001 | decimale | 0 | fx-CG50, SI |
| `dwt` | pennyweight | v × 0.00155517384 | decimale | 0 |  |
| `dr` | dramma (avoirdupois) | v × 0.0017718451953125 | decimale | 0 |  |
| `kg` | chilogrammo | v × 1 | decimale | 0 | fx-CG50 |
| `q` | quintale | v × 1e+02 | decimale | 0 |  |
| `mton` | tonnellata metrica | v × 1e+03 | decimale | 0 | fx-CG50 |
| `t` | tonnellata | v × 1e+03 | decimale | 0 | SI |
| `oz` | oncia | v × 0.028349523125 | decimale | 0 | fx-CG50 |
| `oz t` | oncia troy | v × 0.0311034768 | decimale | 0 |  |
| `lb t` | libbra troy | v × 0.3732417216 | decimale | 0 |  |
| `lb` | libbra | v × 0.45359237 | decimale | 0 | fx-CG50 |
| `stone` | stone (14 lb) | v × 6.35029318 | decimale | 0 |  |
| `slug` | slug | v × 14.593902937206 | — | 1 | fx-CG50 |
| `ton(short)` | tonnellata corta | v × 907.18474 | decimale | 0 | fx-CG50 |
| `ton(long)` | tonnellata lunga | v × 1016.0469088 | decimale | 0 | fx-CG50 |
| `m_P` | massa di Planck | v × 2.176434e-08 | — | 3 |  |
| `M_T` | massa della Terra | v × 5.9722e+24 | — | 4 |  |
| `M_S` | massa del Sole | v × 1.98847e+30 | — | 4 |  |

### Angolo piano

Riferimento: `rad`, 14 unita'.

| Simbolo | Nome | Conversione in rad | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `mas` | millisecondo d'arco | v × 4.8481368110953594e-09 | pi/648000000 | 0 |  |
| `arcsec` | secondo d'arco | v × 4.84813681109536e-06 | pi/648000 | 0 |  |
| `arcmin` | minuto d'arco | v × 0.0002908882086657216 | pi/10800 | 0 |  |
| `mrad` | milliradiante | v × 0.001 | decimale | 0 |  |
| `mil(NATO)` | mil NATO (1/6400 di giro) | v × 0.0009817477042468104 | pi/3200 | 0 |  |
| `mil(URSS)` | mil sovietico (1/6000 di giro) | v × 0.0010471975511965976 | pi/3000 | 0 |  |
| `gon` | gradiante (gon) | v × 0.015707963267948967 | pi/200 | 0 |  |
| `deg` | grado sessagesimale | v × 0.017453292519943295 | pi/180 | 0 |  |
| `point` | punto della bussola (1/32 di giro) | v × 0.19634954084936207 | pi/16 | 0 |  |
| `h(ang)` | ora angolare (15°) | v × 0.2617993877991494 | pi/12 | 0 |  |
| `rad` | radiante | v × 1 | decimale | 0 | SI |
| `sestante` | sestante (60°) | v × 1.0471975511965976 | pi/3 | 0 |  |
| `quadrante` | quadrante (90°) | v × 1.5707963267948966 | pi/2 | 0 |  |
| `giro` | giro completo | v × 6.283185307179586 | 2*pi | 0 |  |

### Angolo solido

Riferimento: `sr`, 4 unita'.

| Simbolo | Nome | Conversione in sr | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `deg²` | grado quadrato | v × 0.00030461741978670857 | pi*pi/32400 | 0 |  |
| `sr` | steradiante | v × 1 | decimale | 0 | SI |
| `emisfero` | emisfero (2π sr) | v × 6.283185307179586 | 2*pi | 0 |  |
| `sfera` | sfera (4π sr) | v × 12.566370614359172 | 4*pi | 0 |  |

### Frequenza

Riferimento: `Hz`, 10 unita'. rad/s: pulsazione ω = 2πf.

| Simbolo | Nome | Conversione in Hz | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `bpm` | battiti al minuto | v × 0.016666666666666666 | 1/60 | 0 |  |
| `rpm` | giri al minuto | v × 0.016666666666666666 | 1/60 | 0 |  |
| `deg/s` | gradi al secondo | v × 0.002777777777777778 | 1/360 | 0 |  |
| `rad/s` | radianti al secondo (ω) | v × 0.15915494309189535 | 1/2/pi | 0 |  |
| `Hz` | hertz | v × 1 | decimale | 0 | SI |
| `rps` | giri al secondo | v × 1 | decimale | 0 |  |
| `kHz` | chilohertz | v × 1e+03 | decimale | 0 |  |
| `MHz` | megahertz | v × 1e+06 | decimale | 0 |  |
| `GHz` | gigahertz | v × 1e+09 | decimale | 0 |  |
| `THz` | terahertz | v × 1e+12 | decimale | 0 |  |

## Meccanica

### Velocita' (fx-CG50: VELOCITY)

Riferimento: `m/s`, 18 unita'. mach: ISA, 15 °C al livello del mare (340,294 m/s); min/km: ritmo (inverso).

| Simbolo | Nome | Conversione in m/s | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `mm/s` | millimetri al secondo | v × 0.001 | decimale | 0 |  |
| `cm/s` | centimetri al secondo | v × 0.01 | decimale | 0 |  |
| `in/s` | pollici al secondo | v × 0.0254 | decimale | 0 |  |
| `ft/min` | piedi al minuto | v × 0.00508 | 0.3048/60 | 0 |  |
| `m/min` | metri al minuto | v × 0.016666666666666666 | 1/60 | 0 |  |
| `m/s` | metri al secondo | v × 1 | decimale | 0 | fx-CG50 |
| `km/h` | chilometri orari | v × 0.2777777777777778 | 1/3.6 | 0 | fx-CG50 |
| `knot` | nodo | v × 0.5144444444444445 | 1852/3600 | 0 | fx-CG50 |
| `ft/s` | piedi al secondo | v × 0.3048 | decimale | 0 | fx-CG50 |
| `yd/s` | iarde al secondo | v × 0.9144 | decimale | 0 |  |
| `mile/h` | miglia orarie | v × 0.44704 | decimale | 0 | fx-CG50 |
| `km/s` | chilometri al secondo | v × 1e+03 | decimale | 0 |  |
| `mach` | mach (ISA, 15 °C, livello del mare) | v × 340.294 | — | 6 |  |
| `v_s` | velocita' del suono (aria secca, 20 °C) | v × 343.21 | — | 8 | approssimata |
| `c` | velocita' della luce | v × 299792458 | decimale | 0 |  |
| `min/km` | ritmo (minuti al km) | 16.666666666666668 / v | — | 0 |  |
| `min/mile` | ritmo (minuti al miglio) | 26.822400000000002 / v | — | 0 |  |
| `s/km` | secondi al km | 1e+03 / v | — | 0 |  |

### Forza (fx-CG50: FORCE)

Riferimento: `N`, 14 unita'.

| Simbolo | Nome | Conversione in N | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `µN` | micronewton | v × 1e-06 | decimale | 0 |  |
| `mN` | millinewton | v × 0.001 | decimale | 0 |  |
| `N` | newton | v × 1 | decimale | 0 | fx-CG50, SI |
| `kN` | chilonewton | v × 1e+03 | decimale | 0 |  |
| `MN` | meganewton | v × 1e+06 | decimale | 0 |  |
| `lbf` | libbra-forza | v × 4.4482216152605 | decimale | 0 | fx-CG50 |
| `kip` | kip (1000 lbf) | v × 4448.2216152605 | decimale | 0 |  |
| `tonf` | tonnellata-forza lunga | v × 9964.01641818352 | decimale | 0 | fx-CG50 |
| `dyne` | dina | v × 1e-05 | decimale | 0 | fx-CG50 |
| `gf` | grammo-forza | v × 0.00980665 | decimale | 0 |  |
| `kgf` | chilogrammo-forza | v × 9.80665 | decimale | 0 | fx-CG50 |
| `ozf` | oncia-forza | v × 0.2780138509537812 | 4.4482216152605/16 | 0 |  |
| `pdl` | poundal | v × 0.138254954376 | decimale | 0 |  |
| `sn` | sthene | v × 1e+03 | decimale | 0 |  |

### Pressione (fx-CG50: PRESSURE)

Riferimento: `Pa`, 24 unita'. colonne d'acqua e di mercurio convenzionali (g₀).

| Simbolo | Nome | Conversione in Pa | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `Ba` | barye | v × 0.1 | decimale | 0 |  |
| `µbar` | microbar | v × 0.1 | decimale | 0 |  |
| `Pa` | pascal | v × 1 | decimale | 0 | fx-CG50, SI |
| `hPa` | ettopascal | v × 1e+02 | decimale | 0 |  |
| `mbar` | millibar | v × 1e+02 | decimale | 0 |  |
| `kPa` | chilopascal | v × 1e+03 | decimale | 0 | fx-CG50 |
| `MPa` | megapascal | v × 1e+06 | decimale | 0 |  |
| `GPa` | gigapascal | v × 1e+09 | decimale | 0 |  |
| `mmH2O` | millimetro d'acqua | v × 9.80665 | decimale | 0 | fx-CG50 |
| `cmH2O` | centimetro d'acqua | v × 98.0665 | decimale | 0 |  |
| `mmHg` | millimetro di mercurio | v × 133.32236842105263 | 101325/760 | 0 | fx-CG50 |
| `Torr` | torr | v × 133.32236842105263 | 101325/760 | 0 |  |
| `cmHg` | centimetro di mercurio (conv.) | v × 1333.22387415 | decimale | 0 |  |
| `atm` | atmosfera | v × 101325 | decimale | 0 | fx-CG50 |
| `at` | atmosfera tecnica | v × 98066.5 | decimale | 0 |  |
| `inH2O` | pollice d'acqua | v × 249.08891 | decimale | 0 | fx-CG50 |
| `ftH2O` | piede d'acqua | v × 2989.06692 | decimale | 0 |  |
| `inHg` | pollice di mercurio | v × 3386.388640341 | decimale | 0 | fx-CG50 |
| `psf` | libbra-forza al piede² | v × 47.88025898033584 | 4.4482216152605/0.09290304 | 0 |  |
| `lbf/in²` | libbra-forza al pollice² | v × 6894.757293168 | — | 1 | fx-CG50 |
| `psi` | psi (lbf/in²) | v × 6894.757293168361 | 4.4482216152605/0.00064516 | 0 |  |
| `ksi` | ksi (1000 psi) | v × 6894757.293168361 | 4448.2216152605/0.00064516 | 0 |  |
| `bar` | bar | v × 1e+05 | decimale | 0 | fx-CG50 |
| `kgf/cm²` | chilogrammo-forza al cm² | v × 98066.5 | decimale | 0 | fx-CG50 |

### Energia (fx-CG50: ENERGY)

Riferimento: `J`, 35 unita'. E = mc²: vedi Massa ↔ energia.

| Simbolo | Nome | Conversione in J | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `Ry` | rydberg | v × 2.1798723611035e-18 | — | 3 |  |
| `Eh` | hartree | v × 4.3597447222071e-18 | — | 3 |  |
| `eV` | elettronvolt | v × 1.602176634e-19 | decimale | 0 | fx-CG50, SI |
| `keV` | chiloelettronvolt | v × 1.602176634e-16 | decimale | 0 |  |
| `MeV` | megaelettronvolt | v × 1.602176634e-13 | decimale | 0 |  |
| `GeV` | gigaelettronvolt | v × 1.602176634e-10 | decimale | 0 |  |
| `TeV` | teraelettronvolt | v × 1.602176634e-07 | decimale | 0 |  |
| `J` | joule | v × 1 | decimale | 0 | fx-CG50, SI |
| `kJ` | chilojoule | v × 1e+03 | decimale | 0 |  |
| `MJ` | megajoule | v × 1e+06 | decimale | 0 |  |
| `GJ` | gigajoule | v × 1e+09 | decimale | 0 |  |
| `calth` | caloria termochimica | v × 4.184 | decimale | 0 | fx-CG50 |
| `cal15` | caloria a 15 °C | v × 4.1855 | — | 2 | fx-CG50 |
| `calIT` | caloria internazionale | v × 4.1868 | decimale | 0 | fx-CG50 |
| `kcalth` | chilocaloria termochimica | v × 4184 | decimale | 0 | fx-CG50 |
| `kcal15` | chilocaloria a 15 °C | v × 4185.5 | — | 2 | fx-CG50 |
| `kcalIT` | chilocaloria internazionale | v × 4186.8 | decimale | 0 | fx-CG50 |
| `Cal` | caloria alimentare (kcal) | v × 4184 | decimale | 0 |  |
| `l-atm` | litro-atmosfera | v × 101.325 | decimale | 0 | fx-CG50 |
| `Wh` | wattora | v × 3.6e+03 | decimale | 0 | SI |
| `kW•h` | chilowattora | v × 3.6e+06 | decimale | 0 | fx-CG50 |
| `kWh` | chilowattora | v × 3.6e+06 | decimale | 0 |  |
| `MWh` | megawattora | v × 3.6e+09 | decimale | 0 |  |
| `GWh` | gigawattora | v × 3.6e+12 | decimale | 0 |  |
| `ft•lbf` | piede-libbra forza | v × 1.3558179483314 | — | 1 | fx-CG50 |
| `Btu` | Btu (IT) | v × 1055.05585262 | decimale | 0 | fx-CG50 |
| `Btu(th)` | Btu termochimico | v × 1054.350264488889 | 4.184*453.59237*5/9 | 0 |  |
| `thm(US)` | therm US | v × 1.054804e+08 | decimale | 0 |  |
| `thm(EC)` | therm EC | v × 1.05506e+08 | decimale | 0 |  |
| `quad` | quad (1E15 Btu) | v × 1.05505585262e+18 | decimale | 0 |  |
| `erg` | erg | v × 1e-07 | decimale | 0 | fx-CG50 |
| `kgf•m` | chilogrammetro | v × 9.80665 | decimale | 0 | fx-CG50 |
| `tTNT` | tonnellata di TNT | v × 4.184e+09 | — | 9 |  |
| `ktTNT` | chilotone di TNT | v × 4.184e+12 | — | 9 |  |
| `tep` | tonnellata equivalente di petrolio | v × 4.1868e+10 | — | 9 |  |

### Potenza (fx-CG50: POWER)

Riferimento: `W`, 18 unita'. dBm = 10·log₁₀(P/1 mW), dBW = 10·log₁₀(P/1 W).

| Simbolo | Nome | Conversione in W | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `erg/s` | erg al secondo | v × 1e-07 | decimale | 0 |  |
| `mW` | milliwatt | v × 0.001 | decimale | 0 |  |
| `W` | watt | v × 1 | decimale | 0 | fx-CG50, SI |
| `kW` | chilowatt | v × 1e+03 | decimale | 0 |  |
| `MW` | megawatt | v × 1e+06 | decimale | 0 |  |
| `GW` | gigawatt | v × 1e+09 | decimale | 0 |  |
| `calth/s` | caloria termochimica al secondo | v × 4.184 | decimale | 0 | fx-CG50 |
| `kcal/h` | chilocaloria IT all'ora | v × 1.163 | decimale | 0 |  |
| `hp` | cavallo vapore britannico | v × 745.69987158227 | — | 1 | fx-CG50 |
| `CV` | cavallo vapore metrico (CV, PS) | v × 735.49875 | decimale | 0 |  |
| `hp(E)` | cavallo elettrico | v × 746 | decimale | 0 |  |
| `hp(S)` | cavallo da caldaia | v × 9809.5 | — | 2 |  |
| `ft•lbf/s` | piede-libbra forza al secondo | v × 1.3558179483314 | — | 1 | fx-CG50 |
| `Btu/h` | Btu all'ora | v × 0.2930710701722222 | 1055.05585262/3600 | 0 |  |
| `Btu/min` | Btu al minuto | v × 17.58426421 | — | 1 | fx-CG50 |
| `TR` | tonnellata di refrigerazione | v × 3516.8528420666667 | 12000*1055.05585262/3600 | 0 |  |
| `dBm` | dBm (rif. 1 mW) | s = k·10^(v/10), k = 0.001 | — | 0 |  |
| `dBW` | dBW (rif. 1 W) | s = k·10^(v/10), k = 1 | — | 0 |  |

### Accelerazione

Riferimento: `m/s²`, 9 unita'. g₀ = 9,80665 m/s² (esatta).

| Simbolo | Nome | Conversione in m/s² | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `m/s²` | metri al secondo quadrato | v × 1 | decimale | 0 |  |
| `Gal` | gal (cm/s²) | v × 0.01 | decimale | 0 |  |
| `mGal` | milligal | v × 1e-05 | decimale | 0 |  |
| `ft/s²` | piedi al secondo quadrato | v × 0.3048 | decimale | 0 |  |
| `in/s²` | pollici al secondo quadrato | v × 0.0254 | decimale | 0 |  |
| `g0` | gravita' standard | v × 9.80665 | decimale | 0 |  |
| `km/h/s` | km/h al secondo | v × 0.2777777777777778 | 1/3.6 | 0 |  |
| `mph/s` | miglia orarie al secondo | v × 0.44704 | decimale | 0 |  |
| `knot/s` | nodi al secondo | v × 0.5144444444444445 | 1852/3600 | 0 |  |

### Coppia / momento

Riferimento: `N•m`, 10 unita'.

| Simbolo | Nome | Conversione in N•m | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `dyn•cm` | dina-centimetro | v × 1e-07 | decimale | 0 |  |
| `N•mm` | newton-millimetro | v × 0.001 | decimale | 0 |  |
| `N•cm` | newton-centimetro | v × 0.01 | decimale | 0 |  |
| `N•m` | newton-metro | v × 1 | decimale | 0 |  |
| `kN•m` | chilonewton-metro | v × 1e+03 | decimale | 0 |  |
| `kgf•cm` | chilogrammo-forza-centimetro | v × 0.0980665 | decimale | 0 |  |
| `kgf•m` | chilogrammo-forza-metro | v × 9.80665 | decimale | 0 |  |
| `ozf•in` | oncia-forza-pollice | v × 0.007061551814226043 | 4.4482216152605*0.0254/16 | 0 |  |
| `lbf•in` | libbra-forza-pollice | v × 0.11298482902761668 | 4.4482216152605*0.0254 | 0 |  |
| `lbf•ft` | libbra-forza-piede | v × 1.3558179483314003 | 4.4482216152605*0.3048 | 0 |  |

### Densita'

Riferimento: `kg/m³`, 12 unita'.

| Simbolo | Nome | Conversione in kg/m³ | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `kg/m³` | chilogrammi al m³ | v × 1 | decimale | 0 |  |
| `g/L` | grammi al litro | v × 1 | decimale | 0 |  |
| `g/cm³` | grammi al cm³ | v × 1e+03 | decimale | 0 |  |
| `g/mL` | grammi al millilitro | v × 1e+03 | decimale | 0 |  |
| `kg/L` | chilogrammi al litro | v × 1e+03 | decimale | 0 |  |
| `t/m³` | tonnellate al m³ | v × 1e+03 | decimale | 0 |  |
| `lb/ft³` | libbre al piede³ | v × 16.01846337396014 | 0.45359237/0.028316846592 | 0 |  |
| `lb/in³` | libbre al pollice³ | v × 27679.904710203125 | 0.45359237/0.000016387064 | 0 |  |
| `lb/gal(US)` | libbre al gallone US | v × 119.82642731689663 | 0.45359237/0.003785411784 | 0 |  |
| `lb/gal(UK)` | libbre al gallone UK | v × 99.7763726631017 | 0.45359237/0.00454609 | 0 |  |
| `oz/in³` | once al pollice³ | v × 1729.9940443876953 | 0.028349523125/0.000016387064 | 0 |  |
| `slug/ft³` | slug al piede³ | v × 515.3788183931961 | 0.45359237*9.80665/0.3048/0.028316846592 | 0 |  |

### Portata volumetrica

Riferimento: `m³/s`, 13 unita'.

| Simbolo | Nome | Conversione in m³/s | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `mL/min` | millilitri al minuto | v × 1.6666666666666667e-08 | 0.000001/60 | 0 |  |
| `L/h` | litri all'ora | v × 2.7777777777777776e-07 | 0.001/3600 | 0 |  |
| `L/min` | litri al minuto | v × 1.6666666666666667e-05 | 0.001/60 | 0 |  |
| `L/s` | litri al secondo | v × 0.001 | decimale | 0 |  |
| `m³/d` | m³ al giorno | v × 1.1574074074074073e-05 | 1/86400 | 0 |  |
| `m³/h` | m³ all'ora | v × 0.0002777777777777778 | 1/3600 | 0 |  |
| `m³/s` | m³ al secondo | v × 1 | decimale | 0 |  |
| `cfm` | piedi³ al minuto | v × 0.0004719474432 | 0.028316846592/60 | 0 |  |
| `cfs` | piedi³ al secondo | v × 0.028316846592 | decimale | 0 |  |
| `gph(US)` | galloni US all'ora | v × 1.0515032733333334e-06 | 0.003785411784/3600 | 0 |  |
| `gpm(US)` | galloni US al minuto | v × 6.30901964e-05 | 0.003785411784/60 | 0 |  |
| `gpm(UK)` | galloni UK al minuto | v × 7.576816666666667e-05 | 0.00454609/60 | 0 |  |
| `bbl/d` | barili al giorno | v × 1.8401307283333336e-06 | 0.158987294928/86400 | 0 |  |

### Portata massica

Riferimento: `kg/s`, 7 unita'.

| Simbolo | Nome | Conversione in kg/s | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `g/s` | grammi al secondo | v × 0.001 | decimale | 0 |  |
| `kg/h` | chilogrammi all'ora | v × 0.0002777777777777778 | 1/3600 | 0 |  |
| `kg/min` | chilogrammi al minuto | v × 0.016666666666666666 | 1/60 | 0 |  |
| `kg/s` | chilogrammi al secondo | v × 1 | decimale | 0 |  |
| `t/h` | tonnellate all'ora | v × 0.2777777777777778 | 1000/3600 | 0 |  |
| `lb/h` | libbre all'ora | v × 0.00012599788055555556 | 0.45359237/3600 | 0 |  |
| `lb/s` | libbre al secondo | v × 0.45359237 | decimale | 0 |  |

### Consumo di carburante

Riferimento: `m/m³`, 7 unita'. L/100 km e gal/100 mi sono inversi di km/L e mpg.

| Simbolo | Nome | Conversione in m/m³ | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `L/100km` | litri per 100 km | 1e+08 / v | — | 0 |  |
| `km/L` | km al litro | v × 1e+06 | decimale | 0 |  |
| `mpg(US)` | miglia per gallone US | v × 425143.707430272 | 1609.344/0.003785411784 | 0 |  |
| `mpg(UK)` | miglia per gallone UK | v × 354006.1899346471 | 1609.344/0.00454609 | 0 |  |
| `mi/L` | miglia al litro | v × 1609344 | 1609344 | 0 |  |
| `gal/100mi(US)` | galloni US per 100 miglia | 42514370.743027195 / v | — | 0 |  |
| `gal/100mi(UK)` | galloni UK per 100 miglia | 35400618.993464716 / v | — | 0 |  |

### Viscosita' dinamica

Riferimento: `Pa•s`, 8 unita'.

| Simbolo | Nome | Conversione in Pa•s | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `cP` | centipoise | v × 0.001 | decimale | 0 |  |
| `mPa•s` | millipascal-secondo | v × 0.001 | decimale | 0 |  |
| `P` | poise | v × 0.1 | decimale | 0 |  |
| `Pa•s` | pascal-secondo | v × 1 | decimale | 0 |  |
| `lb/(ft•s)` | libbre al piede-secondo | v × 1.4881639435695537 | 0.45359237/0.3048 | 0 |  |
| `kgf•s/m²` | kgf·s/m² | v × 9.80665 | decimale | 0 |  |
| `lbf•s/ft²` | lbf·s/ft² | v × 47.88025898033584 | 4.4482216152605/0.09290304 | 0 |  |
| `reyn` | reyn (lbf·s/in²) | v × 6894.757293168361 | 4.4482216152605/0.00064516 | 0 |  |

### Viscosita' cinematica

Riferimento: `m²/s`, 8 unita'.

| Simbolo | Nome | Conversione in m²/s | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `cSt` | centistokes | v × 1e-06 | decimale | 0 |  |
| `mm²/s` | mm² al secondo | v × 1e-06 | decimale | 0 |  |
| `St` | stokes | v × 0.0001 | decimale | 0 |  |
| `cm²/s` | cm² al secondo | v × 0.0001 | decimale | 0 |  |
| `m²/h` | m² all'ora | v × 0.0002777777777777778 | 1/3600 | 0 |  |
| `m²/s` | m² al secondo | v × 1 | decimale | 0 |  |
| `in²/s` | pollici² al secondo | v × 0.00064516 | decimale | 0 |  |
| `ft²/s` | piedi² al secondo | v × 0.09290304 | decimale | 0 |  |

### Tensione superficiale

Riferimento: `N/m`, 6 unita'.

| Simbolo | Nome | Conversione in N/m | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `dyn/cm` | dine al centimetro | v × 0.001 | decimale | 0 |  |
| `erg/cm²` | erg al cm² | v × 0.001 | decimale | 0 |  |
| `mN/m` | millinewton al metro | v × 0.001 | decimale | 0 |  |
| `N/m` | newton al metro | v × 1 | decimale | 0 |  |
| `lbf/ft` | libbre-forza al piede | v × 14.593902937206362 | 4.4482216152605/0.3048 | 0 |  |
| `lbf/in` | libbre-forza al pollice | v × 175.12683524647636 | 4.4482216152605/0.0254 | 0 |  |

### Quantita' di moto / impulso

Riferimento: `kg•m/s`, 6 unita'.

| Simbolo | Nome | Conversione in kg•m/s | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `g•cm/s` | grammo-centimetro al secondo | v × 1e-05 | decimale | 0 |  |
| `dyn•s` | dina-secondo | v × 1e-05 | decimale | 0 |  |
| `lb•ft/s` | libbra-piede al secondo | v × 0.13825495437600002 | 0.45359237*0.3048 | 0 |  |
| `kg•m/s` | chilogrammo-metro al secondo | v × 1 | decimale | 0 |  |
| `N•s` | newton-secondo (impulso) | v × 1 | decimale | 0 |  |
| `lbf•s` | libbra-forza-secondo | v × 4.4482216152605 | decimale | 0 |  |

### Momento angolare

Riferimento: `kg•m²/s`, 5 unita'. ħ = h/(2π).

| Simbolo | Nome | Conversione in kg•m²/s | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `ħ` | costante di Planck ridotta | v × 1.0545718176461565e-34 | — | 0 |  |
| `g•cm²/s` | g·cm²/s | v × 1e-07 | decimale | 0 |  |
| `lb•ft²/s` | lb·ft²/s | v × 0.042140110093804806 | 0.45359237*0.09290304 | 0 |  |
| `kg•m²/s` | kg·m²/s | v × 1 | decimale | 0 |  |
| `J•s` | joule-secondo | v × 1 | decimale | 0 |  |

### Momento d'inerzia

Riferimento: `kg•m²`, 5 unita'.

| Simbolo | Nome | Conversione in kg•m² | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `g•cm²` | grammo-cm² | v × 1e-07 | decimale | 0 |  |
| `lb•in²` | libbra-pollice² | v × 0.0002926396534292 | 0.45359237*0.00064516 | 0 |  |
| `lb•ft²` | libbra-piede² | v × 0.042140110093804806 | 0.45359237*0.09290304 | 0 |  |
| `kg•m²` | chilogrammo-metro² | v × 1 | decimale | 0 |  |
| `slug•ft²` | slug-piede² | v × 1.3558179483314003 | 4.4482216152605*0.3048 | 0 |  |

### Rigidezza

Riferimento: `N/m`, 7 unita'.

| Simbolo | Nome | Conversione in N/m | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `lbf/ft` | libbre-forza al piede | v × 14.593902937206362 | 4.4482216152605/0.3048 | 0 |  |
| `N/m` | newton al metro | v × 1 | decimale | 0 |  |
| `lbf/in` | libbre-forza al pollice | v × 175.12683524647636 | 4.4482216152605/0.0254 | 0 |  |
| `kgf/cm` | kgf al centimetro | v × 980.665 | decimale | 0 |  |
| `N/mm` | newton al millimetro | v × 1e+03 | decimale | 0 |  |
| `kN/m` | chilonewton al metro | v × 1e+03 | decimale | 0 |  |
| `kgf/mm` | kgf al millimetro | v × 9806.65 | decimale | 0 |  |

### Velocita' angolare

Riferimento: `rad/s`, 6 unita'.

| Simbolo | Nome | Conversione in rad/s | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `deg/min` | gradi al minuto | v × 0.0002908882086657216 | pi/10800 | 0 |  |
| `rad/min` | radianti al minuto | v × 0.016666666666666666 | 1/60 | 0 |  |
| `deg/s` | gradi al secondo | v × 0.017453292519943295 | pi/180 | 0 |  |
| `rpm` | giri al minuto | v × 0.10471975511965977 | pi/30 | 0 |  |
| `rad/s` | radianti al secondo | v × 1 | decimale | 0 |  |
| `rps` | giri al secondo | v × 6.283185307179586 | 2*pi | 0 |  |

## Elettricita'

### Carica elettrica

Riferimento: `C`, 11 unita'. e = 1,602176634E-19 C (esatta).

| Simbolo | Nome | Conversione in C | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `e` | carica elementare | v × 1.602176634e-19 | decimale | 0 |  |
| `statC` | statcoulomb (franklin) | v × 3.3356409519815207e-10 | 1/2997924580 | 0 |  |
| `pC` | picocoulomb | v × 1e-12 | decimale | 0 |  |
| `nC` | nanocoulomb | v × 1e-09 | decimale | 0 |  |
| `µC` | microcoulomb | v × 1e-06 | decimale | 0 |  |
| `mC` | millicoulomb | v × 0.001 | decimale | 0 |  |
| `C` | coulomb | v × 1 | decimale | 0 | SI |
| `mAh` | milliamperora | v × 3.6 | decimale | 0 |  |
| `abC` | abcoulomb | v × 1e+01 | decimale | 0 |  |
| `Ah` | amperora | v × 3.6e+03 | decimale | 0 |  |
| `faraday` | faraday (e·N_A) | v × 96485.33212331001 | — | 0 |  |

### Corrente elettrica

Riferimento: `A`, 7 unita'.

| Simbolo | Nome | Conversione in A | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `statA` | statampere | v × 3.3356409519815207e-10 | 1/2997924580 | 0 |  |
| `nA` | nanoampere | v × 1e-09 | decimale | 0 |  |
| `µA` | microampere | v × 1e-06 | decimale | 0 |  |
| `mA` | milliampere | v × 0.001 | decimale | 0 |  |
| `A` | ampere | v × 1 | decimale | 0 | SI |
| `abA` | abampere (biot) | v × 1e+01 | decimale | 0 |  |
| `kA` | chiloampere | v × 1e+03 | decimale | 0 |  |

### Tensione elettrica

Riferimento: `V`, 7 unita'.

| Simbolo | Nome | Conversione in V | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `abV` | abvolt | v × 1e-08 | decimale | 0 |  |
| `µV` | microvolt | v × 1e-06 | decimale | 0 |  |
| `mV` | millivolt | v × 0.001 | decimale | 0 |  |
| `V` | volt | v × 1 | decimale | 0 | SI |
| `statV` | statvolt | v × 299.792458 | decimale | 0 |  |
| `kV` | chilovolt | v × 1e+03 | decimale | 0 |  |
| `MV` | megavolt | v × 1e+06 | decimale | 0 |  |

### Resistenza

Riferimento: `Ω`, 7 unita'.

| Simbolo | Nome | Conversione in Ω | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `abΩ` | abohm | v × 1e-09 | decimale | 0 |  |
| `mΩ` | milliohm | v × 0.001 | decimale | 0 |  |
| `Ω` | ohm | v × 1 | decimale | 0 | SI |
| `kΩ` | chiloohm | v × 1e+03 | decimale | 0 |  |
| `MΩ` | megaohm | v × 1e+06 | decimale | 0 |  |
| `GΩ` | gigaohm | v × 1e+09 | decimale | 0 |  |
| `statΩ` | statohm | v × 898755178736.8176 | 898755178736.8176 | 0 |  |

### Conduttanza

Riferimento: `S`, 5 unita'.

| Simbolo | Nome | Conversione in S | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `nS` | nanosiemens | v × 1e-09 | decimale | 0 |  |
| `µS` | microsiemens | v × 1e-06 | decimale | 0 |  |
| `mS` | millisiemens | v × 0.001 | decimale | 0 |  |
| `S` | siemens | v × 1 | decimale | 0 | SI |
| `mho` | mho | v × 1 | decimale | 0 |  |

### Capacita'

Riferimento: `F`, 7 unita'.

| Simbolo | Nome | Conversione in F | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `statF` | statfarad | v × 1.1126500560536185e-12 | 1/898755178736.8176 | 0 |  |
| `pF` | picofarad | v × 1e-12 | decimale | 0 |  |
| `nF` | nanofarad | v × 1e-09 | decimale | 0 |  |
| `µF` | microfarad | v × 1e-06 | decimale | 0 |  |
| `mF` | millifarad | v × 0.001 | decimale | 0 |  |
| `F` | farad | v × 1 | decimale | 0 | SI |
| `abF` | abfarad | v × 1e+09 | decimale | 0 |  |

### Induttanza

Riferimento: `H`, 6 unita'.

| Simbolo | Nome | Conversione in H | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `abH` | abhenry | v × 1e-09 | decimale | 0 |  |
| `nH` | nanohenry | v × 1e-09 | decimale | 0 |  |
| `µH` | microhenry | v × 1e-06 | decimale | 0 |  |
| `mH` | millihenry | v × 0.001 | decimale | 0 |  |
| `H` | henry | v × 1 | decimale | 0 | SI |
| `statH` | stathenry | v × 898755178736.8176 | 898755178736.8176 | 0 |  |

### Resistivita'

Riferimento: `Ω•m`, 5 unita'.

| Simbolo | Nome | Conversione in Ω•m | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `Ω•cmil/ft` | ohm-circular mil al piede | v × 1.6624261125245987e-09 | pi*0.0000254*0.0000254/4/0.3048 | 0 |  |
| `µΩ•cm` | microohm-centimetro | v × 1e-08 | decimale | 0 |  |
| `Ω•mm²/m` | ohm·mm²/m | v × 1e-06 | decimale | 0 |  |
| `Ω•cm` | ohm-centimetro | v × 0.01 | decimale | 0 |  |
| `Ω•m` | ohm-metro | v × 1 | decimale | 0 |  |

### Conducibilita' elettrica

Riferimento: `S/m`, 5 unita'.

| Simbolo | Nome | Conversione in S/m | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `µS/cm` | microsiemens al cm | v × 0.0001 | decimale | 0 |  |
| `mS/m` | millisiemens al metro | v × 0.001 | decimale | 0 |  |
| `mS/cm` | millisiemens al cm | v × 0.1 | decimale | 0 |  |
| `S/m` | siemens al metro | v × 1 | decimale | 0 |  |
| `S/cm` | siemens al cm | v × 1e+02 | decimale | 0 |  |

### Flusso magnetico

Riferimento: `Wb`, 5 unita'.

| Simbolo | Nome | Conversione in Wb | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `Mx` | maxwell | v × 1e-08 | decimale | 0 |  |
| `µWb` | microweber | v × 1e-06 | decimale | 0 |  |
| `mWb` | milliweber | v × 0.001 | decimale | 0 |  |
| `Wb` | weber | v × 1 | decimale | 0 | SI |
| `V•s` | volt-secondo | v × 1 | decimale | 0 |  |

### Induzione magnetica

Riferimento: `T`, 7 unita'.

| Simbolo | Nome | Conversione in T | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `γ` | gamma | v × 1e-09 | decimale | 0 |  |
| `nT` | nanotesla | v × 1e-09 | decimale | 0 |  |
| `mG` | milligauss | v × 1e-07 | decimale | 0 |  |
| `µT` | microtesla | v × 1e-06 | decimale | 0 |  |
| `G` | gauss | v × 0.0001 | decimale | 0 |  |
| `mT` | millitesla | v × 0.001 | decimale | 0 |  |
| `T` | tesla | v × 1 | decimale | 0 | SI |

### Campo magnetico H

Riferimento: `A/m`, 4 unita'. 1 Oe = 1000/(4π) A/m.

| Simbolo | Nome | Conversione in A/m | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `A/m` | ampere al metro | v × 1 | decimale | 0 |  |
| `Oe` | oersted | v × 79.57747154594767 | 1000/4/pi | 0 |  |
| `kA/m` | chiloampere al metro | v × 1e+03 | decimale | 0 |  |
| `kOe` | chilooersted | v × 79577.47154594767 | 1000000/4/pi | 0 |  |

### Calibro dei fili (AWG)

Riferimento: `m`, 9 unita'. d = 0,127 mm·92^((36-n)/39); 0000 = -3; sezione = πd²/4.

| Simbolo | Nome | Conversione in m | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `mil` | diametro in mil | v × 2.54e-05 | decimale | 0 |  |
| `µm` | diametro in µm | v × 1e-06 | decimale | 0 |  |
| `mm` | diametro in mm | v × 0.001 | decimale | 0 |  |
| `in` | diametro in pollici | v × 0.0254 | decimale | 0 |  |
| `m` | diametro in metri | v × 1 | decimale | 0 |  |
| `AWG` | calibro AWG (0000 = -3) | d = 0,127 mm·92^((36-n)/39) | — | 0 |  |
| `mm²` | sezione in mm² | d = √(4·k·v/π), k = 1e-06 | — | 0 |  |
| `cmil` | sezione in circular mil | d = √(4·k·v/π), k = 5.067074790974977e-10 | — | 0 |  |
| `kcmil` | sezione in kcmil (MCM) | d = √(4·k·v/π), k = 5.067074790974977e-07 | — | 0 |  |

## Termica

### Temperatura (fx-CG50: TMPR)

Riferimento: `K`, 8 unita'. Temperature assolute (per le differenze: Intervallo di temperatura).

| Simbolo | Nome | Conversione in K | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `°C` | grado Celsius | (v + 273.15) × 1 | 1 | 0 | fx-CG50 |
| `K` | kelvin | v × 1 | decimale | 0 | fx-CG50 |
| `°F` | grado Fahrenheit | (v + 459.67) × 0.5555555555555556 | 5/9 | 0 | fx-CG50 |
| `°R` | grado Rankine | v × 0.5555555555555556 | 5/9 | 0 | fx-CG50 |
| `°Ré` | grado Reaumur | (v + 218.52) × 1.25 | 5/4 | 0 |  |
| `°Rø` | grado Romer | (v + 135.90375) × 1.9047619047619047 | 40/21 | 0 |  |
| `°De` | grado Delisle | (v + -559.725) × -0.6666666666666666 | -2/3 | 0 |  |
| `°N` | grado Newton | (v + 90.1395) × 3.0303030303030303 | 100/33 | 0 |  |

### Intervallo di temperatura

Riferimento: `ΔK`, 7 unita'. Differenze di temperatura (nessun offset).

| Simbolo | Nome | Conversione in ΔK | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `ΔK` | intervallo in kelvin | v × 1 | decimale | 0 |  |
| `Δ°C` | intervallo in gradi Celsius | v × 1 | decimale | 0 |  |
| `Δ°F` | intervallo in gradi Fahrenheit | v × 0.5555555555555556 | 5/9 | 0 |  |
| `Δ°R` | intervallo in gradi Rankine | v × 0.5555555555555556 | 5/9 | 0 |  |
| `Δ°Ré` | intervallo in gradi Reaumur | v × 1.25 | 5/4 | 0 |  |
| `Δ°Rø` | intervallo in gradi Romer | v × 1.9047619047619047 | 40/21 | 0 |  |
| `Δ°N` | intervallo in gradi Newton | v × 3.0303030303030303 | 100/33 | 0 |  |

### Conducibilita' termica

Riferimento: `W/(m•K)`, 6 unita'. cal e Btu IT.

| Simbolo | Nome | Conversione in W/(m•K) | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `mW/(m•K)` | milliwatt al metro-kelvin | v × 0.001 | decimale | 0 |  |
| `kcal/(h•m•°C)` | kcal IT/(h·m·°C) | v × 1.163 | decimale | 0 |  |
| `Btu/(h•ft•°F)` | Btu IT/(h·ft·°F) | v × 1.7307346663713907 | 1055.05585262/3600/0.3048*9/5 | 0 |  |
| `W/(m•K)` | watt al metro-kelvin | v × 1 | decimale | 0 |  |
| `W/(cm•K)` | watt al cm-kelvin | v × 1e+02 | decimale | 0 |  |
| `cal/(s•cm•°C)` | cal IT/(s·cm·°C) | v × 418.68 | decimale | 0 |  |

### Coeff. di scambio termico

Riferimento: `W/(m²•K)`, 5 unita'. cal e Btu IT.

| Simbolo | Nome | Conversione in W/(m²•K) | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `kcal/(h•m²•°C)` | kcal IT/(h·m²·°C) | v × 1.163 | decimale | 0 |  |
| `W/(m²•K)` | watt al m²-kelvin | v × 1 | decimale | 0 |  |
| `Btu/(h•ft²•°F)` | Btu IT/(h·ft²·°F) | v × 5.678263341113487 | 1055.05585262/3600/0.09290304*9/5 | 0 |  |
| `kW/(m²•K)` | chilowatt al m²-kelvin | v × 1e+03 | decimale | 0 |  |
| `cal/(s•cm²•°C)` | cal IT/(s·cm²·°C) | v × 41868 | decimale | 0 |  |

### Calore specifico

Riferimento: `J/(kg•K)`, 6 unita'. cal e Btu IT.

| Simbolo | Nome | Conversione in J/(kg•K) | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `J/(kg•K)` | joule al kg-kelvin | v × 1 | decimale | 0 |  |
| `J/(g•K)` | joule al grammo-kelvin | v × 1e+03 | decimale | 0 |  |
| `kJ/(kg•K)` | chilojoule al kg-kelvin | v × 1e+03 | decimale | 0 |  |
| `cal/(g•°C)` | cal IT/(g·°C) | v × 4186.8 | decimale | 0 |  |
| `kcal/(kg•°C)` | kcal IT/(kg·°C) | v × 4186.8 | decimale | 0 |  |
| `Btu/(lb•°F)` | Btu IT/(lb·°F) | v × 4186.8 | decimale | 0 |  |

### Flusso termico

Riferimento: `W/m²`, 7 unita'. cal e Btu IT.

| Simbolo | Nome | Conversione in W/m² | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `kcal/(h•m²)` | kcal IT/(h·m²) | v × 1.163 | decimale | 0 |  |
| `W/m²` | watt al m² | v × 1 | decimale | 0 |  |
| `Btu/(h•ft²)` | Btu IT/(h·ft²) | v × 3.1545907450630484 | 1055.05585262/3600/0.09290304 | 0 |  |
| `mW/cm²` | milliwatt al cm² | v × 1e+01 | decimale | 0 |  |
| `kW/m²` | chilowatt al m² | v × 1e+03 | decimale | 0 |  |
| `W/cm²` | watt al cm² | v × 1e+04 | decimale | 0 |  |
| `cal/(s•cm²)` | cal IT/(s·cm²) | v × 41868 | decimale | 0 |  |

### Entropia / capacita' termica

Riferimento: `J/K`, 6 unita'. k = costante di Boltzmann.

| Simbolo | Nome | Conversione in J/K | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `k` | costante di Boltzmann k | v × 1.380649e-23 | decimale | 0 |  |
| `J/K` | joule al kelvin | v × 1 | decimale | 0 |  |
| `cal/K` | caloria IT al kelvin | v × 4.1868 | decimale | 0 |  |
| `kJ/K` | chilojoule al kelvin | v × 1e+03 | decimale | 0 |  |
| `Btu/°F` | Btu IT al grado F | v × 1899.100534716 | 1055.05585262*9/5 | 0 |  |
| `kcal/K` | chilocaloria IT al kelvin | v × 4186.8 | decimale | 0 |  |

### Resistenza termica

Riferimento: `K/W`, 3 unita'.

| Simbolo | Nome | Conversione in K/W | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `K/W` | kelvin al watt | v × 1 | decimale | 0 |  |
| `°C/W` | grado C al watt | v × 1 | decimale | 0 |  |
| `°F•h/Btu` | °F·h/Btu IT | v × 1.8956342406266344 | 5/9*3600/1055.05585262 | 0 |  |

### Isolamento termico (R)

Riferimento: `m²•K/W`, 4 unita'. 1 clo = 0,155 m²•K/W (ISO 9920).

| Simbolo | Nome | Conversione in m²•K/W | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `tog` | tog | v × 0.1 | decimale | 0 |  |
| `clo` | clo (vestiario) | v × 0.155 | — | 9 |  |
| `ft²•°F•h/Btu` | valore R US | v × 0.17611018368230585 | 0.09290304*5/9*3600/1055.05585262 | 0 |  |
| `m²•K/W` | valore R SI (RSI) | v × 1 | decimale | 0 |  |

## Luce e radiazioni

### Intensita' luminosa

Riferimento: `cd`, 3 unita'.

| Simbolo | Nome | Conversione in cd | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `mcd` | millicandela | v × 0.001 | decimale | 0 |  |
| `cd` | candela | v × 1 | decimale | 0 | SI |
| `kcd` | chilocandela | v × 1e+03 | decimale | 0 |  |

### Flusso luminoso

Riferimento: `lm`, 3 unita'.

| Simbolo | Nome | Conversione in lm | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `mlm` | millilumen | v × 0.001 | decimale | 0 |  |
| `lm` | lumen | v × 1 | decimale | 0 | SI |
| `klm` | chilolumen | v × 1e+03 | decimale | 0 |  |

### Illuminamento

Riferimento: `lx`, 5 unita'.

| Simbolo | Nome | Conversione in lx | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `nox` | nox | v × 0.001 | decimale | 0 |  |
| `lx` | lux | v × 1 | decimale | 0 | SI |
| `fc` | foot-candle (lm/ft²) | v × 10.763910416709722 | 1/0.09290304 | 0 |  |
| `klx` | chilolux | v × 1e+03 | decimale | 0 |  |
| `ph` | phot | v × 1e+04 | decimale | 0 |  |

### Luminanza

Riferimento: `cd/m²`, 7 unita'.

| Simbolo | Nome | Conversione in cd/m² | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `asb` | apostilb | v × 0.3183098861837907 | 1/pi | 0 |  |
| `fL` | foot-lambert | v × 3.4262590996353905 | 1/0.09290304/pi | 0 |  |
| `cd/m²` | candele al m² | v × 1 | decimale | 0 |  |
| `nit` | nit | v × 1 | decimale | 0 |  |
| `kcd/m²` | chilocandele al m² | v × 1e+03 | decimale | 0 |  |
| `lambert` | lambert | v × 3183.098861837907 | 10000/pi | 0 |  |
| `sb` | stilb | v × 1e+04 | decimale | 0 |  |

### Attivita' radioattiva

Riferimento: `Bq`, 11 unita'.

| Simbolo | Nome | Conversione in Bq | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `dpm` | disintegrazioni al minuto | v × 0.016666666666666666 | 1/60 | 0 |  |
| `Bq` | becquerel | v × 1 | decimale | 0 | SI |
| `kBq` | chilobecquerel | v × 1e+03 | decimale | 0 |  |
| `MBq` | megabecquerel | v × 1e+06 | decimale | 0 |  |
| `Rd` | rutherford | v × 1e+06 | decimale | 0 |  |
| `GBq` | gigabecquerel | v × 1e+09 | decimale | 0 |  |
| `TBq` | terabecquerel | v × 1e+12 | decimale | 0 |  |
| `nCi` | nanocurie | v × 37 | decimale | 0 |  |
| `µCi` | microcurie | v × 3.7e+04 | decimale | 0 |  |
| `mCi` | millicurie | v × 3.7e+07 | decimale | 0 |  |
| `Ci` | curie | v × 3.7e+10 | decimale | 0 |  |

### Dose assorbita

Riferimento: `Gy`, 5 unita'.

| Simbolo | Nome | Conversione in Gy | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `µGy` | microgray | v × 1e-06 | decimale | 0 |  |
| `erg/g` | erg al grammo | v × 0.0001 | decimale | 0 |  |
| `mGy` | milligray | v × 0.001 | decimale | 0 |  |
| `rad` | rad | v × 0.01 | decimale | 0 |  |
| `Gy` | gray | v × 1 | decimale | 0 | SI |

### Dose equivalente

Riferimento: `Sv`, 5 unita'.

| Simbolo | Nome | Conversione in Sv | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `µSv` | microsievert | v × 1e-06 | decimale | 0 |  |
| `mrem` | millirem | v × 1e-05 | decimale | 0 |  |
| `mSv` | millisievert | v × 0.001 | decimale | 0 |  |
| `rem` | rem | v × 0.01 | decimale | 0 |  |
| `Sv` | sievert | v × 1 | decimale | 0 | SI |

### Esposizione

Riferimento: `C/kg`, 4 unita'. 1 R = 2,58E-4 C/kg (esatto).

| Simbolo | Nome | Conversione in C/kg | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `mR` | milliroentgen | v × 2.58e-07 | decimale | 0 |  |
| `R` | roentgen | v × 0.000258 | decimale | 0 |  |
| `mC/kg` | millicoulomb al kg | v × 0.001 | decimale | 0 |  |
| `C/kg` | coulomb al kg | v × 1 | decimale | 0 |  |

## Chimica

### Quantita' di sostanza

Riferimento: `mol`, 6 unita'. N_A = 6,02214076E23 /mol (esatta).

| Simbolo | Nome | Conversione in mol | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `part.` | numero di particelle (N/N_A) | v × 1.6605390671738466e-24 | — | 0 |  |
| `nmol` | nanomole | v × 1e-09 | decimale | 0 |  |
| `µmol` | micromole | v × 1e-06 | decimale | 0 |  |
| `mmol` | millimole | v × 0.001 | decimale | 0 |  |
| `mol` | mole | v × 1 | decimale | 0 | SI |
| `kmol` | chilomole | v × 1e+03 | decimale | 0 |  |

### Concentrazione molare

Riferimento: `mol/m³`, 8 unita'. 1 M = 1 mol/L.

| Simbolo | Nome | Conversione in mol/m³ | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `nM` | nanomolare (nmol/L) | v × 1e-06 | decimale | 0 |  |
| `µM` | micromolare (µmol/L) | v × 0.001 | decimale | 0 |  |
| `µmol/L` | micromoli al litro | v × 0.001 | decimale | 0 |  |
| `mol/m³` | moli al m³ | v × 1 | decimale | 0 |  |
| `mM` | millimolare (mmol/L) | v × 1 | decimale | 0 |  |
| `mmol/L` | millimoli al litro | v × 1 | decimale | 0 |  |
| `M` | molare (mol/L) | v × 1e+03 | decimale | 0 |  |
| `mol/L` | moli al litro | v × 1e+03 | decimale | 0 |  |

### Concentrazione in massa

Riferimento: `kg/m³`, 9 unita'.

| Simbolo | Nome | Conversione in kg/m³ | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `ng/mL` | nanogrammi al mL | v × 1e-06 | decimale | 0 |  |
| `µg/L` | microgrammi al litro | v × 1e-06 | decimale | 0 |  |
| `mg/L` | milligrammi al litro | v × 0.001 | decimale | 0 |  |
| `µg/mL` | microgrammi al mL | v × 0.001 | decimale | 0 |  |
| `mg/dL` | milligrammi al decilitro | v × 0.01 | decimale | 0 |  |
| `g/L` | grammi al litro | v × 1 | decimale | 0 |  |
| `mg/mL` | milligrammi al mL | v × 1 | decimale | 0 |  |
| `kg/m³` | chilogrammi al m³ | v × 1 | decimale | 0 |  |
| `g/dL` | grammi al decilitro | v × 1e+01 | decimale | 0 |  |

### Rapporti adimensionali

Riferimento: `x`, 9 unita'. % ‰ punti base, ppm, ppb....

| Simbolo | Nome | Conversione in x | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `ppq` | parti per biliardo | v × 1e-15 | decimale | 0 |  |
| `ppt` | parti per trilione | v × 1e-12 | decimale | 0 |  |
| `ppb` | parti per miliardo | v × 1e-09 | decimale | 0 |  |
| `ppm` | parti per milione | v × 1e-06 | decimale | 0 |  |
| `pcm` | per centomila | v × 1e-05 | decimale | 0 |  |
| `bp` | punto base (‱) | v × 0.0001 | decimale | 0 |  |
| `‰` | per mille | v × 0.001 | decimale | 0 |  |
| `%` | percento | v × 0.01 | decimale | 0 |  |
| `x` | rapporto (1) | v × 1 | decimale | 0 |  |

## Dati

### Dati digitali

Riferimento: `bit`, 22 unita'. k = 1000, Ki = 1024 (IEC 80000-13).

| Simbolo | Nome | Conversione in bit | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `bit` | bit | v × 1 | decimale | 0 | SI, IEC |
| `nibble` | nibble (4 bit) | v × 4 | decimale | 0 |  |
| `B` | byte (8 bit) | v × 8 | decimale | 0 | SI, IEC |
| `kbit` | kilobit | v × 1e+03 | decimale | 0 |  |
| `Kibit` | kibibit | v × 1024 | decimale | 0 |  |
| `kB` | kilobyte | v × 8e+03 | decimale | 0 |  |
| `KiB` | kibibyte | v × 8192 | decimale | 0 |  |
| `Mbit` | megabit | v × 1e+06 | decimale | 0 |  |
| `Mibit` | mebibit | v × 1048576 | decimale | 0 |  |
| `MB` | megabyte | v × 8e+06 | decimale | 0 |  |
| `MiB` | mebibyte | v × 8388608 | decimale | 0 |  |
| `Gbit` | gigabit | v × 1e+09 | decimale | 0 |  |
| `Gibit` | gibibit | v × 1073741824 | decimale | 0 |  |
| `GB` | gigabyte | v × 8e+09 | decimale | 0 |  |
| `GiB` | gibibyte | v × 8589934592 | decimale | 0 |  |
| `Tbit` | terabit | v × 1e+12 | decimale | 0 |  |
| `TB` | terabyte | v × 8e+12 | decimale | 0 |  |
| `TiB` | tebibyte | v × 8796093022208 | decimale | 0 |  |
| `PB` | petabyte | v × 8e+15 | decimale | 0 |  |
| `PiB` | pebibyte | v × 9007199254740992 | 9007199254740992 | 0 |  |
| `EB` | exabyte | v × 8e+18 | decimale | 0 |  |
| `EiB` | exbibyte (2^63 bit) | v × 9.223372036854776e+18 | — | 0 |  |

### Velocita' di trasmissione

Riferimento: `bit/s`, 12 unita'. k = 1000, Ki = 1024 (IEC 80000-13).

| Simbolo | Nome | Conversione in bit/s | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `bit/s` | bit al secondo | v × 1 | decimale | 0 | SI, IEC |
| `kbit/s` | kilobit al secondo | v × 1e+03 | decimale | 0 |  |
| `Mbit/s` | megabit al secondo | v × 1e+06 | decimale | 0 |  |
| `Gbit/s` | gigabit al secondo | v × 1e+09 | decimale | 0 |  |
| `Tbit/s` | terabit al secondo | v × 1e+12 | decimale | 0 |  |
| `B/s` | byte al secondo | v × 8 | decimale | 0 | SI, IEC |
| `kB/s` | kilobyte al secondo | v × 8e+03 | decimale | 0 |  |
| `KiB/s` | kibibyte al secondo | v × 8192 | decimale | 0 |  |
| `MB/s` | megabyte al secondo | v × 8e+06 | decimale | 0 |  |
| `MiB/s` | mebibyte al secondo | v × 8388608 | decimale | 0 |  |
| `GB/s` | gigabyte al secondo | v × 8e+09 | decimale | 0 |  |
| `GiB/s` | gibibyte al secondo | v × 8589934592 | decimale | 0 |  |

## Logaritmiche

### Rapporto di potenza

Riferimento: `P/P0`, 4 unita'. dB = 10·log₁₀(P/P0), Np = ½·ln(P/P0).

| Simbolo | Nome | Conversione in P/P0 | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `P/P0` | rapporto di potenza | v × 1 | decimale | 0 |  |
| `dB` | decibel (potenza) | s = 10^(k·v), k = 0.1 | — | 0 |  |
| `B` | bel | s = 10^(k·v), k = 1 | — | 0 |  |
| `Np` | neper (potenza) | s = e^(k·v), k = 2 | — | 0 |  |

### Rapporto di ampiezza

Riferimento: `A/A0`, 3 unita'. dB = 20·log₁₀(A/A0), Np = ln(A/A0).

| Simbolo | Nome | Conversione in A/A0 | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `A/A0` | rapporto di ampiezza | v × 1 | decimale | 0 |  |
| `dB` | decibel (ampiezza) | s = 10^(k·v), k = 0.05 | — | 0 |  |
| `Np` | neper | s = e^(k·v), k = 1 | — | 0 |  |

### Pressione sonora

Riferimento: `Pa`, 4 unita'. dB SPL = 20·log₁₀(p/20 µPa).

| Simbolo | Nome | Conversione in Pa | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `µPa` | micropascal | v × 1e-06 | decimale | 0 |  |
| `mPa` | millipascal | v × 0.001 | decimale | 0 |  |
| `Pa` | pascal | v × 1 | decimale | 0 |  |
| `dB SPL` | livello di pressione sonora (rif. 20 µPa) | s = k·10^(v/20), k = 2e-05 | — | 0 |  |

### Livello di tensione

Riferimento: `V`, 7 unita'. dBV rif. 1 V, dBu rif. √0,6 V (0,775 V).

| Simbolo | Nome | Conversione in V | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `µV` | microvolt | v × 1e-06 | decimale | 0 |  |
| `mV` | millivolt | v × 0.001 | decimale | 0 |  |
| `V` | volt | v × 1 | decimale | 0 |  |
| `dBV` | dBV (rif. 1 V) | s = k·10^(v/20), k = 1 | — | 0 |  |
| `dBu` | dBu (rif. √0,6 V) | s = k·10^(v/20), k = 0.7745966692414834 | — | 0 |  |
| `dBmV` | dBmV (rif. 1 mV) | s = k·10^(v/20), k = 0.001 | — | 0 |  |
| `dBµV` | dBµV (rif. 1 µV) | s = k·10^(v/20), k = 1e-06 | — | 0 |  |

### Rapporto di frequenza

Riferimento: `f2/f1`, 6 unita'. ottava = 2·, semitono = 2^(1/12), cent = 2^(1/1200).

| Simbolo | Nome | Conversione in f2/f1 | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `f2/f1` | rapporto di frequenza | v × 1 | decimale | 0 |  |
| `ottava` | ottave | s = 2^(k·v), k = 1 | — | 0 |  |
| `semitono` | semitoni temperati | s = 2^(k·v), k = 0.08333333333333333 | — | 0 |  |
| `cent` | cent | s = 2^(k·v), k = 0.0008333333333333334 | — | 0 |  |
| `decade` | decadi | s = 10^(k·v), k = 1 | — | 0 |  |
| `savart` | savart | s = 10^(k·v), k = 0.001 | — | 0 |  |

### Acidita' (pH)

Riferimento: `mol/L`, 7 unita'. pH = -log₁₀[H⁺]; pH+pOH = 14 (Kw = 1E-14, 25 °C).

| Simbolo | Nome | Conversione in mol/L | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `nmol/L` | [H⁺] nmol/L | v × 1e-09 | decimale | 0 |  |
| `µmol/L` | [H⁺] µmol/L | v × 1e-06 | decimale | 0 |  |
| `mmol/L` | [H⁺] mmol/L | v × 0.001 | decimale | 0 |  |
| `mol/L` | [H⁺] mol/L | v × 1 | decimale | 0 |  |
| `pH` | pH | s = 10^(k·v), k = -1 | — | 0 |  |
| `pOH` | pOH (25 °C) | [H⁺] = 10^(pOH-14) | — | 0 |  |
| `OH-` | [OH⁻] mol/L (25 °C) | 1e-14 / v | — | 0 |  |

### Magnitudo sismica

Riferimento: `J`, 8 unita'. log₁₀E[J] = 1,5·M + 4,8 (Gutenberg-Richter).

| Simbolo | Nome | Conversione in J | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `J` | joule | v × 1 | decimale | 0 |  |
| `GJ` | gigajoule | v × 1e+09 | decimale | 0 |  |
| `TJ` | terajoule | v × 1e+12 | decimale | 0 |  |
| `PJ` | petajoule | v × 1e+15 | decimale | 0 |  |
| `tTNT` | tonnellate di TNT | v × 4.184e+09 | — | 9 |  |
| `ktTNT` | chilotoni di TNT | v × 4.184e+12 | — | 9 |  |
| `MtTNT` | megatoni di TNT | v × 4.184e+15 | — | 9 |  |
| `Mw` | magnitudo (Richter / momento) | E = 10^(1,5·M+4,8) J | — | 8 | approssimata |

### Magnitudo stellare

Riferimento: `F/F0`, 2 unita'. F/F0 = 10^(-0,4·Δm) (Pogson).

| Simbolo | Nome | Conversione in F/F0 | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `F/F0` | rapporto di flusso | v × 1 | decimale | 0 |  |
| `mag` | differenza di magnitudo Δm | s = 10^(k·v), k = -0.4 | — | 0 |  |

## Derivate

### Fotone: λ ↔ ν ↔ E ↔ σ

Riferimento: `Hz`, 19 unita'. ν = c/λ = E/h = c·σ; c = 299792458 m/s, h = 6,62607015E-34 J•s (esatte).

| Simbolo | Nome | Conversione in Hz | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `Hz` | frequenza (Hz) | v × 1 | decimale | 0 | SI |
| `kHz` | frequenza (kHz) | v × 1e+03 | decimale | 0 |  |
| `MHz` | frequenza (MHz) | v × 1e+06 | decimale | 0 |  |
| `GHz` | frequenza (GHz) | v × 1e+09 | decimale | 0 |  |
| `THz` | frequenza (THz) | v × 1e+12 | decimale | 0 |  |
| `PHz` | frequenza (PHz) | v × 1e+15 | decimale | 0 |  |
| `m` | lunghezza d'onda (m) | 299792458 / v | — | 0 |  |
| `µm` | lunghezza d'onda (µm) | 2.99792458e+14 / v | — | 0 |  |
| `nm` | lunghezza d'onda (nm) | 2.99792458e+17 / v | — | 0 |  |
| `Å` | lunghezza d'onda (Å) | 2.99792458e+18 / v | — | 0 |  |
| `pm` | lunghezza d'onda (pm) | 2.99792458e+20 / v | — | 0 |  |
| `J` | energia del fotone (J) | v × 1.5091901796421518e+33 | — | 0 |  |
| `meV` | energia del fotone (meV) | v × 241798924208.4918 | — | 0 |  |
| `eV` | energia del fotone (eV) | v × 241798924208491.8 | — | 0 |  |
| `keV` | energia del fotone (keV) | v × 2.417989242084918e+17 | — | 0 |  |
| `kJ/mol` | energia molare (kJ/mol) | v × 2506069253090.909 | — | 0 |  |
| `kcal/mol` | energia molare (kcal/mol) | v × 10485393754932.363 | — | 0 |  |
| `m⁻¹` | numero d'onda (m⁻¹) | v × 299792458 | decimale | 0 |  |
| `cm⁻¹` | numero d'onda (cm⁻¹) | v × 2.99792458e+10 | decimale | 0 |  |

### Massa ↔ energia (E = mc²)

Riferimento: `J`, 16 unita'. c² = 89875517873681764 m²/s² (esatta).

| Simbolo | Nome | Conversione in J | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `eV` | elettronvolt | v × 1.602176634e-19 | decimale | 0 |  |
| `MeV` | megaelettronvolt | v × 1.602176634e-13 | decimale | 0 |  |
| `GeV` | gigaelettronvolt | v × 1.602176634e-10 | decimale | 0 |  |
| `TeV` | teraelettronvolt | v × 1.602176634e-07 | decimale | 0 |  |
| `J` | joule | v × 1 | decimale | 0 |  |
| `MJ` | megajoule | v × 1e+06 | decimale | 0 |  |
| `kWh` | chilowattora | v × 3.6e+06 | decimale | 0 |  |
| `ktTNT` | chilotoni di TNT | v × 4.184e+12 | — | 9 |  |
| `MtTNT` | megatoni di TNT | v × 4.184e+15 | — | 9 |  |
| `u` | massa: unita' atomica (u·c²) | v × 1.4924180856015513e-10 | — | 3 |  |
| `MeV/c²` | massa: MeV/c² | v × 1.602176634e-13 | decimale | 0 |  |
| `GeV/c²` | massa: GeV/c² | v × 1.602176634e-10 | decimale | 0 |  |
| `µg` | massa: microgrammi | v × 89875517.87368177 | 89875517.873681764 | 0 |  |
| `mg` | massa: milligrammi | v × 89875517873.68176 | 89875517873.681764 | 0 |  |
| `g` | massa: grammi | v × 89875517873681.77 | 89875517873681.764 | 0 |  |
| `kg` | massa: chilogrammi | v × 8.987551787368176e+16 | 89875517873681764 | 0 |  |

### Temperatura ↔ energia (kT)

Riferimento: `J`, 10 unita'. E = k·T; k = 1,380649E-23 J/K, N_A = 6,02214076E23 /mol (esatte).

| Simbolo | Nome | Conversione in J | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `K` | temperatura (K) | v × 1.380649e-23 | — | 0 |  |
| `°C` | temperatura (°C) | (v + 273.15) × 1.380649e-23 | — | 0 |  |
| `°F` | temperatura (°F) | (v + 459.67) × 7.670272222222223e-24 | — | 0 |  |
| `J` | energia (J) | v × 1 | decimale | 0 |  |
| `meV` | energia (meV) | v × 1.602176634e-22 | decimale | 0 |  |
| `eV` | energia (eV) | v × 1.602176634e-19 | decimale | 0 |  |
| `J/mol` | energia molare (J/mol) | v × 1.6605390671738466e-24 | — | 0 |  |
| `kJ/mol` | energia molare (kJ/mol) | v × 1.6605390671738467e-21 | — | 0 |  |
| `kcal/mol` | energia molare (kcal/mol) | v × 6.947695457055374e-21 | — | 0 |  |
| `cm⁻¹` | numero d'onda (cm⁻¹) | v × 1.9864458571489285e-23 | — | 0 |  |

### Moli ↔ massa ↔ particelle

Riferimento: `mol`, 10 unita'. n = m/M = N/N_A; M massa molare (PARAM); gas ideale a 0 °C, 101,325 kPa.

| Simbolo | Nome | Conversione in mol | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `part.` | numero di particelle | v × 1.6605390671738466e-24 | — | 0 |  |
| `µmol` | micromoli | v × 1e-06 | decimale | 0 |  |
| `mmol` | millimoli | v × 0.001 | decimale | 0 |  |
| `mol` | moli | v × 1 | decimale | 0 | SI |
| `kmol` | chilomoli | v × 1e+03 | decimale | 0 |  |
| `µg` | massa in µg (con M) | v × 1e-06 / M | — | 0 |  |
| `mg` | massa in mg (con M) | v × 0.001 / M | — | 0 |  |
| `g` | massa in g (con M) | v × 1 / M | — | 0 |  |
| `kg` | massa in kg (con M) | v × 1e+03 / M | — | 0 |  |
| `L(STP)` | litri di gas ideale (0 °C, 1 atm) | v × 0.044615033405470315 | — | 0 |  |

### Peso ↔ massa (g₀)

Riferimento: `N`, 10 unita'. P = m·g₀, g₀ = 9,80665 m/s² (esatta).

| Simbolo | Nome | Conversione in N | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `dyn` | dine | v × 1e-05 | decimale | 0 |  |
| `N` | newton | v × 1 | decimale | 0 |  |
| `kN` | chilonewton | v × 1e+03 | decimale | 0 |  |
| `kgf` | chilogrammo-forza | v × 9.80665 | decimale | 0 |  |
| `lbf` | libbra-forza | v × 4.4482216152605 | decimale | 0 |  |
| `g` | massa: grammi | v × 0.00980665 | decimale | 0 |  |
| `kg` | massa: chilogrammi | v × 9.80665 | decimale | 0 |  |
| `t` | massa: tonnellate | v × 9806.65 | decimale | 0 |  |
| `oz` | massa: once | v × 0.2780138509537812 | 0.028349523125*9.80665 | 0 |  |
| `lb` | massa: libbre | v × 4.4482216152605 | decimale | 0 |  |

## Formati

### Basi numeriche

Rappresentazioni (testo -> testo): decimale; esadecimale; binario; ottale; base N. Parametro: base N (2-36, predefinito 12). parte frazionaria con il punto; Base-N di Run-Matrix.

### Complemento a due

Rappresentazioni (testo -> testo): decimale con segno; esadecimale; binario. Parametro: bit (8-64, predefinito 16). 8, 16, 32 o 64 bit.

### BCD

Rappresentazioni (testo -> testo): decimale; BCD (binario). una cifra decimale per gruppo di 4 bit.

### Codice Gray

Rappresentazioni (testo -> testo): decimale; binario; codice Gray. Gray = n XOR (n >> 1).

### IEEE-754

Rappresentazioni (testo -> testo): valore; half (16 bit); single (32 bit); double (64 bit). esadecimale, segno s, esponente e, mantissa m.

### Numeri romani

Rappresentazioni (testo -> testo): decimale; romano (1-3999); romano con vinculum (x1000). vinculum: (IV) = 4000.

### Testo ↔ Unicode

Rappresentazioni (testo -> testo): testo; Unicode U+; codici decimali; byte UTF-8.

### Angoli (DMS, HMS)

Rappresentazioni (testo -> testo): gradi decimali; gradi ° ′ ″; ore angolari h m s; radianti; gradianti. 12°30′15″ anche come 12 30 15 o 12:30:15.

### Tempo (h:m:s)

Rappresentazioni (testo -> testo): ore decimali; h:m:s; secondi; minuti decimali; giorni d h:m:s.

### Date

Rappresentazioni (testo -> testo): data AAAA-MM-GG; giorno giuliano JD; MJD; giorno dell'anno; settimana ISO; timestamp Unix; giorno della settimana (solo uscita). calendario gregoriano prolettico, anni 1-9999; JD alle 0:00 TU.

### Differenza tra date

Rappresentazioni (testo -> testo): due date (A B); giorni (B-A) (solo uscita); settimane e giorni (solo uscita); anni, mesi, giorni (solo uscita). Days_Prd di Run-Matrix (1902-2097), giorno giuliano altrove.

### Coordinate 2D

Rappresentazioni (testo -> testo): rettangolari x, y; polari r, θ. angoli nell'unita' del SET UP.

### Coordinate 3D

Rappresentazioni (testo -> testo): cartesiane x, y, z; cilindriche ρ, φ, z; sferiche r, θ, φ. θ dall'asse z, φ azimut (ISO 80000-2); angoli del SET UP.

### Coordinate geografiche

Rappresentazioni (testo -> testo): gradi decimali lat, lon; DMS con N/S E/W. latitudine +-90, longitudine +-180.

### Numeri complessi

Rappresentazioni (testo -> testo): a+bi; r∠θ. angolo nell'unita' del SET UP.

### Notazioni numeriche

Rappresentazioni (testo -> testo): decimale; frazione n/d; frazione mista a b/c; periodico 0.1(6); scientifica; ingegneristica (simboli SI). S⇔D, ab/c, ENG come in Run-Matrix.

### Colori

Rappresentazioni (testo -> testo): HEX #RRGGBB; RGB; HSL; HSV; CMYK; RGB565 (display PicoCalc).

### Tassi APR ↔ EFF

Rappresentazioni (testo -> testo): nominale APR %; effettivo EFF %; per periodo %. Parametro: capitalizzazioni/anno (1-1e+06, predefinito 12). Cnvt_EFF / Cnvt_APR di Run-Matrix.

## Taglie

### Scarpe (adulti)

Riferimento: `m`, 8 unita'. APPROSSIMATO: piede L cm; EU = 1,5(L+1,5); UK = 3(L+1,5)/2,54-25; US = UK+1 (uomo), +2 (donna).

| Simbolo | Nome | Conversione in m | Esatto | Fonte | Note |
|---|---|---|---|---|---|
| `cm` | lunghezza del piede (cm) | v × 0.01 | — | 8 | approssimata |
| `MP` | Mondopoint (mm) | v × 0.001 | — | 8 | approssimata |
| `JP` | Giappone (cm) | v × 0.01 | — | 8 | approssimata |
| `in` | lunghezza del piede (in) | v × 0.0254 | — | 8 | approssimata |
| `EU` | taglia EU (punto di Parigi) | (v + -2.25) × 0.006666666666666667 | — | 8 | approssimata |
| `UK` | taglia UK | (v + 23.228346456692915) × 0.008466666666666666 | — | 8 | approssimata |
| `US(M)` | taglia US uomo | (v + 22.228346456692915) × 0.008466666666666666 | — | 8 | approssimata |
| `US(W)` | taglia US donna | (v + 21.228346456692915) × 0.008466666666666666 | — | 8 | approssimata |

### Anelli

Rappresentazioni (testo -> testo): diametro interno mm; circonferenza mm (EU/ISO); US/Canada; UK/Australia. APPROSSIMATO: UK A = 37,5 mm (+1,25 per lettera), US d = 11,63 + 0,8128·n mm.

## Valute

16 valute (EUR USD GBP CHF JPY CNY CAD AUD SEK NOK DKK PLN CZK HUF INR BRL) con i tassi inseriti
dall'utente (F4 TASSI: unita' per 1 EUR) e salvati nel workspace (record v10). Nessuna rete richiesta;
l'aggiornamento via Wi-Fi e' in docs/ROADMAP.md.
