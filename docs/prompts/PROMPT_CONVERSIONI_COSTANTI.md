# Prompt: conversioni complete + costanti fisiche (stile fx-CG50 CONVERT / CONST)

> Copia tutto da "## Compito" in giù e dallo a un agente di coding sul repo `PicoCalc-FxCG-50`.

## Compito

Estendi il firmware PicoCalc-FxCG-50 perché abbia **un catalogo completo di conversioni di unità** e **un catalogo completo di costanti fisiche** (CODATA 2022), almeno alla pari del menu CONVERT / CONST del Casio fx-CG50 e possibilmente oltre. Tutto deve restare utilizzabile da tastiera sul PicoCalc (320×320) e girare sia sul simulatore host sia sul Pico 2 W.

### Stato attuale (da leggere prima di toccare qualcosa)

- `include/point_a.hpp` – `enum class Unit` con solo ~40 unità; `src/point_a.cpp` – `convert()`, `dim()`, `scaleOf()`, `toK()/fromK()`, `physicalConstant()` (9 costanti hard-coded con if a catena).
- `src/apps/conversion_app.cpp` – app CONVERSION con 10 categorie (Lunghezza, Area, Volume, Tempo, Velocità, Massa, Pressione, Energia, Potenza, Temperatura) definite in una tabella locale.
- `src/apps/run_matrix_app.cpp` – sottomenu `sub:CONV` / `sub:CONVk` (sintassi `[cm]▶[in]`, token `unit:U`) e `sub:CONST` (π, e, c, G, h, …).
- `src/expression.cpp` (~riga 100) – `kConstants[]`: nomi riconosciuti dal parser (`physc`, `physg`, `physh`, `na`, `kb`, `qe`, `g0`, `speedoflight`, …). Usato da `isConstantName()` e dalla valutazione.
- `src/memory.cpp` (~riga 35) – tabella `constants[]` nome → glifo da mostrare (`physc` → `c`, `NA` → `Nₐ`, …).
- `include/cheatsheet.hpp` + `docs/SYMBOL_CHEATSHEET.md` – pagina "COSTANTI MATEMATICHE" dell'app SYMBOLS (solo costanti matematiche).
- Test: `tests/math_engine_test.cpp`, `tests/run_matrix_*_test.cpp`, `tests/cheatsheet_smoke.cpp`, runner `tests/run_host_tests.sh`.

Oggi ci sono quindi **solo 8 costanti fisiche** (c, G, h, Nₐ, k_B, qₑ, g₀ + matematiche) e **~40 unità in 10 categorie**, duplicate in più tabelle.

### Obiettivo architetturale

1. **Un'unica fonte di verità** per unità e costanti, data-driven (tabelle `constexpr` in flash, niente `std::vector` statici costruiti a runtime, niente catene di `if`):
   - nuovo `include/units_catalog.hpp` + `src/units_catalog.cpp`: struct `UnitDef { id, symbol (UTF-8), asciiName, nameIT, category, factor, offset }` con conversione lineare `SI = v*factor + offset` (offset ≠ 0 solo per temperature); categorie con nome IT e unità SI di riferimento.
   - nuovo `include/physical_constants.hpp` + `src/physical_constants.cpp`: struct `ConstDef { parserName, glyph (UTF-8), nameIT, value, unit (stringa), relUncertainty, exact (bool), group }`.
   - `point_a::convert`, `point_a::physicalConstant`, `kConstants[]` in `expression.cpp`, la tabella glifi in `memory.cpp`, la `ConversionApp`, i menu `sub:CONV*`/`sub:CONST` in RUN-MATRIX e il cheatsheet devono **tutti leggere da queste tabelle**. Rimuovi le copie.
   - Mantieni la compatibilità all'indietro: `enum class Unit` può restare come alias/indice, e **tutti i nomi parser esistenti** (`physc`, `physg`, `physh`, `na`, `kb`, `qe`, `g0`, `speedoflight`, `planckconstant`, `avogadro`, `boltzmann`, `elementarycharge`, `standardgravity`, `gravitationalconstant`) devono continuare a funzionare. Attenzione alle collisioni con variabili A–Z e nomi di funzioni: usa il prefisso `phys…`/nomi lunghi come oggi, verificando `isReservedWord`.
2. **Memoria**: le tabelle devono stare in `.rodata` (flash). Misura con `cmake/check_firmware_size.cmake` che il firmware resti nei limiti; riporta la differenza di dimensione.
3. **Precisione**: valori esatti dove definiti dall'SI 2019 (c, h, e, k_B, Nₐ, ΔνCs, Kcd, e derivate esatte come R = Nₐ·k_B, F = Nₐ·e, σ, K_J, R_K), altrimenti CODATA 2022 con incertezza relativa. Fattori di conversione esatti per definizione (es. 1 in = 0.0254 m, 1 lb = 0.45359237 kg, 1 cal_th = 4.184 J, 1 atm = 101325 Pa, 1 mmHg = 133.322387415 Pa, 1 eV = qₑ J).

### Catalogo conversioni richiesto (minimo)

Per ogni categoria includi prefissi SI sensati, non solo l'unità base.

- **Lunghezza**: pm, nm, µm, mm, cm, dm, m, km, Å, in, ft, yd, mi (statute), mi nautico (NM), lega, fathom, chain, furlong, mil/thou, punto tipografico (pt), pica, unità astronomica (au), anno luce (ly), parsec (pc), raggio di Bohr, lunghezza di Planck.
- **Area**: mm², cm², m², km², ha, ara, in², ft², yd², mi², acro, barn.
- **Volume**: mm³, cm³, mL, cL, dL, L, m³, in³, ft³, yd³, gal US, gal UK, qt US, pt US, pt UK, cup US, fl oz US, fl oz UK, cucchiaio (tbsp), cucchiaino (tsp), barile petrolio (bbl).
- **Tempo**: ps, ns, µs, ms, s, min, h, giorno, settimana, mese (30.4375 d), anno (365.25 d, giuliano), anno siderale, secolo.
- **Velocità**: m/s, km/h, mi/h, ft/s, nodo, Mach (al livello del mare 340.3 m/s, documentato come convenzionale), c.
- **Accelerazione**: m/s², g₀, ft/s², Gal.
- **Massa**: µg, mg, g, kg, t (tonnellata), ct (carato), gr (grain), oz, oz troy, lb, stone, short ton, long ton, u (Dalton), massa elettrone, massa protone.
- **Forza**: N, kN, dyn, kgf, lbf, poundal.
- **Pressione**: Pa, hPa, kPa, MPa, bar, mbar, atm, at (kgf/cm²), Torr, mmHg, inHg, psi, mmH₂O.
- **Energia**: J, kJ, MJ, cal, kcal, Wh, kWh, eV, keV, MeV, erg, BTU (IT), ft·lbf, tep (toe), hartree, Rydberg.
- **Potenza**: W, mW, kW, MW, hp (meccanico), CV (metrico), BTU/h, erg/s, ft·lbf/s.
- **Temperatura** (affine, non lineare): K, °C, °F, °R (Rankine), °Ré (Réaumur). Gestisci separatamente le **differenze di temperatura** (ΔK = Δ°C, Δ°F = 5/9 ΔK) come categoria distinta.
- **Angolo**: rad, °, ', ", grad (gon), giro, mrad.
- **Angolo solido**: sr, deg², sfera.
- **Frequenza / velocità angolare**: Hz, kHz, MHz, GHz, rpm, rad/s.
- **Dati digitali**: bit, byte, kB/MB/GB/TB (10³) e KiB/MiB/GiB/TiB (2¹⁰).
- **Velocità dati**: bit/s, kbit/s, Mbit/s, Gbit/s, B/s, MB/s.
- **Carica elettrica**: C, mAh, Ah, statC, e.
- **Corrente / tensione / resistenza / capacità / induttanza**: A, mA, µA; V, mV, kV; Ω, kΩ, MΩ; F, µF, nF, pF; H, mH, µH.
- **Campo magnetico**: T, mT, G (gauss); **flusso**: Wb, Mx.
- **Radioattività / dose**: Bq, Ci, dps; Gy, rad; Sv, rem.
- **Luce**: lux, foot-candle, phot; cd/m², nit, lambert.
- **Viscosità dinamica**: Pa·s, P, cP; **cinematica**: m²/s, St, cSt.
- **Consumo carburante**: L/100 km ↔ km/L ↔ mpg US ↔ mpg UK (relazione **inversa**, non lineare: supporta il tipo "reciproco" in `UnitDef`).
- **Densità**: kg/m³, g/cm³, g/L, lb/ft³, lb/gal.
- **Coppia**: N·m, kgf·m, lbf·ft, lbf·in.
- **Portata**: m³/s, L/s, L/min, m³/h, gal/min (US).
- **Concentrazione/quantità**: mol, mmol, µmol, kmol, molecole (×Nₐ).

### Catalogo costanti richiesto (CODATA 2022, minimo)

Raggruppate come nel CONST del Casio (Universali, Elettromagnetiche, Atomiche e nucleari, Fisico-chimiche, Valori adottati, Altro) e aggiungi:

- **Universali**: c, μ₀, ε₀, Z₀, G, h, ħ, massa/lunghezza/tempo/temperatura di Planck.
- **Elettromagnetiche**: e, Φ₀ (quanto di flusso), G₀ (conduttanza quantica), K_J (Josephson), R_K (von Klitzing), μ_B (magnetone di Bohr), μ_N (magnetone nucleare).
- **Atomiche e nucleari**: mₑ, mₚ, mₙ, m_μ, m_d, m_α, α (struttura fine), 1/α, R∞ (Rydberg), a₀ (Bohr), rₑ (raggio classico elettrone), λ_C (Compton), σₑ (Thomson), Eₕ (Hartree), μₑ, μₚ, μₙ, γₚ (rapporto giromagnetico protone), mₚ/mₑ.
- **Fisico-chimiche**: Nₐ, k_B, R (costante dei gas), F (Faraday), u (unità di massa atomica), σ (Stefan-Boltzmann), b (costante di Wien), c₁, c₂ (costanti di radiazione), Vₘ (volume molare gas ideale a 273.15 K, 101.325 kPa e a 100 kPa), n₀ (Loschmidt).
- **Valori adottati**: g₀, atm, ΔνCs, K_cd, M_u.
- **Astronomiche (extra, IAU)**: au, ly, pc, M☉, R☉, L☉, M⊕, R⊕ equatoriale, GM⊕, GM☉.
- **Altro**: T₀ = 273.15 K, zero assoluto in °C, velocità del suono in aria a 20 °C (convenzionale, segnalata come tale), densità acqua a 4 °C.

Ogni costante: nome parser ASCII univoco (es. `physme`, `physmp`, `physalpha`, `physR`, `physF`…; scegli uno schema coerente `phys<sigla>` e documentalo), glifo UTF-8 per la visualizzazione (verifica che i caratteri siano presenti in `src/font_data.cpp`; se manca un glifo, aggiungilo o usa un fallback leggibile), nome italiano, unità SI, flag esatta/incertezza.

### Interfaccia utente

1. **App CONVERSION** (`conversion_app.cpp`):
   - lista categorie scorrevole (molte più di oggi), poi Da/A con liste lunghe scorrevoli e ricerca per iniziale/testo (tasto per filtrare), tasto SWAP Da↔A, risultato con `formatEngineering` e cifre significative secondo il SETUP, copia in Ans come oggi.
   - mostra anche il fattore "1 Da = x A".
   - errori in italiano per dimensioni incompatibili / valori fuori dominio (es. temperatura sotto lo zero assoluto, consumo = 0).
2. **Nuova app o pagina COSTANTI** (registrala in `app_registry.cpp` / launcher, oppure come sezione dell'app SYMBOLS se più coerente con l'architettura): elenco per gruppo, dettaglio con valore completo, unità, incertezza, nome parser; tasto per inserire la costante in RUN-MATRIX / copiarla in Ans.
3. **RUN-MATRIX**: `sub:CONST` diventa un menu a pagine per gruppo con tutte le costanti; `sub:CONV*` usa il nuovo catalogo con tutte le categorie. La sintassi `valore[unità]▶[unità]` deve funzionare per ogni unità del catalogo e dare errore pulito se le dimensioni non combaciano.
4. **SYMBOLS / cheatsheet**: aggiungi la pagina "COSTANTI FISICHE" e rigenera/aggiorna `docs/SYMBOL_CHEATSHEET.md` di conseguenza.
5. Tutti i testi UI in italiano, coerenti con lo stile delle app esistenti (`ui_kit`, soft key F1–F6, EXIT torna alla home).

### Test (obbligatori)

Aggiungi `tests/units_constants_test.cpp` e registralo in `tests/CMakeLists.txt`:
- per **ogni** unità: round-trip `convert(convert(v,a,b),b,a) ≈ v` (tolleranza relativa 1e-12) contro l'unità SI della categoria;
- valori di riferimento noti (almeno 2–3 per categoria): 1 mi = 1609.344 m, 1 ly = 9.4607304725808e15 m, 1 au = 149597870700 m, 1 kWh = 3.6e6 J, 1 eV = 1.602176634e-19 J, 100 °C = 212 °F = 373.15 K, 0 °R = 0 K, 1 psi = 6894.757293168 Pa, 1 mmHg = 133.322387415 Pa, 1 hp = 745.69987158227 W, 1 CV = 735.49875 W, 1 GiB = 1073741824 B, 5 L/100km ≈ 47.0429 mpg US, ecc.;
- dimensioni incompatibili → eccezione;
- ogni costante: valore esatto dove è esatta; costanti derivate coerenti (ħ = h/2π, R = Nₐk_B, F = Nₐe, μ₀ ≈ 2αh/(e²c), ε₀ = 1/(μ₀c²), σ = 2π⁵k⁴/(15h³c²), Φ₀ = h/2e, K_J = 2e/h, R_K = h/e²) entro l'incertezza;
- ogni nome parser valuta tramite `fx::evaluate` (es. `physme*physc^2`), nessun nome collide con funzioni o con le parole riservate esistenti, tutti i vecchi nomi funzionano ancora;
- nessun duplicato di simbolo/nome nelle tabelle (test statico);
- aggiorna i test RUN-MATRIX/apps_flow esistenti che toccano CONV/CONST.

Esegui `tests/run_host_tests.sh` (e la build host/simulatore + `scripts/build_pico2w.sh` se l'ambiente lo permette) e riporta i risultati reali, compresa la differenza di dimensione del firmware.

### Documentazione

- Nuovo `docs/UNITS_AND_CONSTANTS.md`: tabella di tutte le categorie/unità con fattori e di tutte le costanti con valore, unità, incertezza, fonte (CODATA 2022 / SI 2019 / IAU 2015 / NIST SP 811) e nome parser.
- Aggiorna `docs/FEATURE_MATRIX.md`, `docs/MATH_ENGINE.md`, `docs/CHANGELOG.md`, `docs/SYMBOL_CHEATSHEET.md`.

### Vincoli

- C++ come il resto del progetto, nessuna nuova dipendenza esterna.
- Nessuna allocazione dinamica per i cataloghi; tabelle `constexpr` in flash.
- Non rompere i file `.pcgraph`/workspace esistenti né i nomi già salvati.
- Commit piccoli e descrittivi sul branch di lavoro; non aprire PR se non richiesto.
