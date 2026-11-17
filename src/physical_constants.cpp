// Catalogo delle costanti (v1.14.0): unica tabella constexpr (in flash) letta da
// parser, renderer, menu CONST, SYMBOLS e point_a. Valori verificati sulla
// tabella NIST "CODATA 2022 recommended values" (physics.nist.gov/cuu, file
// allascii.txt), sulla Brochure SI 2019 (le 7 costanti definenti) e sulle
// risoluzioni IAU 2012 B2, 2015 B2 e 2015 B3. Le costanti derivate esatte sono
// calcolate qui dalle costanti definenti (la tabella NIST le riporta troncate
// con "..."). Tabella completa e scelte: docs/PHYSICAL_CONSTANTS.md.
#include "physical_constants.hpp"
#include <array>

namespace fx::physconst {
namespace {
// ---------------------------------------------------------------- definenti (SI 2019, esatte)
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeltaNuCs = 9192631770.0;     // Hz
constexpr double kC = 299792458.0;              // m/s
constexpr double kH = 6.62607015e-34;           // J·s
constexpr double kQe = 1.602176634e-19;         // C
constexpr double kKB = 1.380649e-23;            // J/K
constexpr double kNA = 6.02214076e23;           // 1/mol
constexpr double kKcd = 683.0;                  // lm/W
// ---------------------------------------------------------------- derivate esatte
constexpr double kHbar = kH / (2.0 * kPi);
constexpr double kPhi0 = kH / (2.0 * kQe);
constexpr double kG0 = 2.0 * kQe * kQe / kH;
constexpr double kKJ = 2.0 * kQe / kH;
constexpr double kRK = kH / (kQe * kQe);
constexpr double kR = kNA * kKB;
constexpr double kF = kNA * kQe;
constexpr double kSigma = 2.0 * kPi * kPi * kPi * kPi * kPi * kKB * kKB * kKB * kKB / (15.0 * kH * kH * kH * kC * kC);
constexpr double kC1 = 2.0 * kPi * kH * kC * kC;
constexpr double kC2 = kH * kC / kKB;
// Legge di Wien: x soluzione di x = 5(1 - e^-x) (numero puro, CODATA: b = c2/x).
constexpr double kWienX = 4.965114231744276303698759;
constexpr double kWienB = kC2 / kWienX;
constexpr double kT0 = 273.15;                  // K (zero della scala Celsius)
constexpr double kAtm = 101325.0;               // Pa
constexpr double kBar = 100000.0;               // Pa
constexpr double kVmAtm = kR * kT0 / kAtm;
constexpr double kVmBar = kR * kT0 / kBar;
constexpr double kN0 = kAtm / (kKB * kT0);
// ---------------------------------------------------------------- misurate (CODATA 2022)
constexpr double kG = 6.67430e-11;
// ---------------------------------------------------------------- astronomiche (IAU)
constexpr double kAu = 149597870700.0;                  // IAU 2012 B2 (esatta)
constexpr double kLy = kC * 365.25 * 86400.0;           // anno giuliano (esatta)
constexpr double kPc = kAu * 648000.0 / kPi;            // IAU 2015 B2 (esatta)
constexpr double kGMSun = 1.3271244e20;                 // IAU 2015 B3, nominale
constexpr double kGMEarth = 3.986004e14;                // IAU 2015 B3, nominale

constexpr ConstGroup kGroups[] = {
    {"Matematiche", "COSTANTI MATEMATICHE", "tau/pi = 2"},
    {"Universali", "COSTANTI FISICHE: UNIVERSALI", "physh*physc/5e-7 \xE2\x89\x88 3.973e-19 J"},
    {"Elettromagnetiche", "COSTANTI FISICHE: ELETTROMAGNETICHE", "2*qe/physh \xE2\x89\x88 4.836e14 Hz/V"},
    {"Atomiche e nucleari", "COSTANTI FISICHE: ATOMICHE E NUCLEARI", "physme*physc^2 \xE2\x89\x88 8.187e-14 J"},
    {"Fisico-chimiche", "COSTANTI FISICHE: FISICO-CHIMICHE", "na*kb \xE2\x89\x88 8.314 J/(mol\xC2\xB7K)"},
    {"Valori adottati", "COSTANTI FISICHE: VALORI ADOTTATI", "physt0+25 = 298.15 K"},
    {"Astronomiche", "COSTANTI ASTRONOMICHE (IAU)", "physpc/physly \xE2\x89\x88 3.262"},
};

// Ordinata per gruppo (verificato da static_assert): constantsOf() restituisce un intervallo.
constexpr ConstDef kTable[] = {
    // ---- 0 Matematiche (nomi del parser invariati dalla v1.13)
    {"pi", "", "π", "Pi greco", kPi, "", 0, 0},
    {"e", "", "e", "Numero di Nepero", 2.71828182845904523536, "", 0, 0},
    {"tau", "", "τ", "Tau: giro completo, 2π", 2.0 * kPi, "", 0, 0},
    {"phi", "goldenratio", "φ", "Sezione aurea", 1.6180339887498948482, "", 0, 0},
    {"eulergamma", "", "γ", "Costante di Eulero-Mascheroni", 0.5772156649015328606, "", 0, 0},
    {"sqrt2", "", "√2", "Radice di 2", 1.4142135623730950488, "", 0, 0},
    {"ln2", "", "ln(2)", "Logaritmo naturale di 2", 0.69314718055994530942, "", 0, 0},
    {"ln10", "", "ln(10)", "Logaritmo naturale di 10", 2.30258509299404568402, "", 0, 0},
    // ---- 1 Universali
    {"physc", "speedoflight", "c", "Velocità della luce", kC, "m/s", 0, 1},
    {"physmu0", "magneticconstant", "μ₀", "Permeabilità del vuoto", 1.25663706127e-6, "N/A²", 1.6e-10, 1},
    {"physeps0", "electricconstant", "ε₀", "Permittività del vuoto", 8.8541878188e-12, "F/m", 1.6e-10, 1},
    {"physz0", "vacuumimpedance", "Z₀", "Impedenza del vuoto", 376.730313412, "Ω", 1.6e-10, 1},
    {"physg", "gravitationalconstant", "G", "Costante gravitazionale", kG, "m³/(kg·s²)", 2.2e-5, 1},
    {"physh", "planckconstant", "h", "Costante di Planck", kH, "J·s", 0, 1},
    {"physhbar", "reducedplanck", "ħ", "Costante di Planck ridotta", kHbar, "J·s", 0, 1},
    {"physplmass", "planckmass", "m_P", "Massa di Planck", 2.176434e-8, "kg", 1.1e-5, 1},
    {"physpllen", "plancklength", "l_P", "Lunghezza di Planck", 1.616255e-35, "m", 1.1e-5, 1},
    {"physpltime", "plancktime", "t_P", "Tempo di Planck", 5.391247e-44, "s", 1.1e-5, 1},
    {"physpltemp", "plancktemperature", "T_P", "Temperatura di Planck", 1.416784e32, "K", 1.1e-5, 1},
    // ---- 2 Elettromagnetiche
    {"qe", "elementarycharge", "qₑ", "Carica elementare", kQe, "C", 0, 2},
    {"physphi0", "fluxquantum", "Φ₀", "Quanto di flusso magnetico", kPhi0, "Wb", 0, 2},
    {"physgcond", "conductancequantum", "G₀", "Quanto di conduttanza", kG0, "S", 0, 2},
    {"physkj", "josephson", "K_J", "Costante di Josephson", kKJ, "Hz/V", 0, 2},
    {"physrk", "vonklitzing", "R_K", "Costante di von Klitzing", kRK, "Ω", 0, 2},
    {"physmub", "bohrmagneton", "μ_B", "Magnetone di Bohr", 9.2740100657e-24, "J/T", 3.1e-10, 2},
    {"physmunuc", "nuclearmagneton", "μ_N", "Magnetone nucleare", 5.0507837393e-27, "J/T", 3.2e-10, 2},
    // ---- 3 Atomiche e nucleari
    {"physalpha", "finestructure", "α", "Costante di struttura fine", 7.2973525643e-3, "", 1.5e-10, 3},
    {"physinvalpha", "", "1/α", "Inverso di α", 137.035999177, "", 1.5e-10, 3},
    {"physrinf", "rydberg", "R∞", "Costante di Rydberg", 10973731.568157, "m⁻¹", 1.1e-12, 3},
    {"physa0", "bohrradius", "a₀", "Raggio di Bohr", 5.29177210544e-11, "m", 1.5e-10, 3},
    {"physeh", "hartree", "Eₕ", "Energia di Hartree", 4.3597447222060e-18, "J", 1.1e-12, 3},
    {"physme", "electronmass", "mₑ", "Massa dell'elettrone", 9.1093837139e-31, "kg", 3.1e-10, 3},
    {"physmmu", "muonmass", "m_μ", "Massa del muone", 1.883531627e-28, "kg", 2.2e-8, 3},
    {"physmp", "protonmass", "mₚ", "Massa del protone", 1.67262192595e-27, "kg", 3.1e-10, 3},
    {"physmn", "neutronmass", "mₙ", "Massa del neutrone", 1.67492750056e-27, "kg", 5.1e-10, 3},
    {"physmd", "deuteronmass", "m_d", "Massa del deutone", 3.3435837768e-27, "kg", 3.0e-10, 3},
    {"physmalpha", "alphamass", "m_α", "Massa della particella α", 6.6446573450e-27, "kg", 3.2e-10, 3},
    {"physmratio", "protonelectronratio", "mₚ/mₑ", "Rapporto massa protone/elettrone", 1836.152673426, "", 1.7e-11, 3},
    {"physre", "electronradius", "rₑ", "Raggio classico dell'elettrone", 2.8179403205e-15, "m", 4.6e-10, 3},
    {"physlambdac", "comptonwavelength", "λ_C", "Lunghezza d'onda Compton", 2.42631023538e-12, "m", 3.1e-10, 3},
    {"physthomson", "", "σₑ", "Sezione d'urto Thomson", 6.6524587051e-29, "m²", 9.3e-10, 3},
    {"physmue", "electronmoment", "μₑ", "Momento magnetico dell'elettrone", -9.2847646917e-24, "J/T", 3.1e-10, 3},
    {"physmup", "protonmoment", "μₚ", "Momento magnetico del protone", 1.41060679545e-26, "J/T", 4.3e-10, 3},
    {"physmuneu", "neutronmoment", "μₙ", "Momento magnetico del neutrone", -9.6623653e-27, "J/T", 2.4e-7, 3},
    {"physgammap", "protongyromag", "γₚ", "Rapporto giromagnetico del protone", 2.6752218708e8, "s⁻¹·T⁻¹", 4.1e-10, 3},
    // ---- 4 Fisico-chimiche
    {"na", "avogadro", "Nₐ", "Costante di Avogadro", kNA, "mol⁻¹", 0, 4},
    {"kb", "boltzmann", "k_B", "Costante di Boltzmann", kKB, "J/K", 0, 4},
    {"physrgas", "gasconstant", "R", "Costante dei gas", kR, "J/(mol·K)", 0, 4},
    {"physf", "faraday", "F", "Costante di Faraday", kF, "C/mol", 0, 4},
    {"physu", "atomicmassunit", "u", "Unità di massa atomica", 1.66053906892e-27, "kg", 3.1e-10, 4},
    {"physmolu", "molarmassconstant", "M_u", "Costante di massa molare", 1.00000000105e-3, "kg/mol", 3.1e-10, 4},
    {"physsigma", "stefanboltzmann", "σ", "Costante di Stefan-Boltzmann", kSigma, "W/(m²·K⁴)", 0, 4},
    {"physb", "wien", "b", "Costante di Wien", kWienB, "m·K", 0, 4},
    {"physrad1", "", "c₁", "Prima costante di radiazione", kC1, "W·m²", 0, 4},
    {"physrad2", "", "c₂", "Seconda costante di radiazione", kC2, "m·K", 0, 4},
    {"physvmatm", "", "Vₘ", "Volume molare (0 °C, 1 atm)", kVmAtm, "m³/mol", 0, 4},
    {"physvmbar", "", "Vₘ°", "Volume molare (0 °C, 1 bar)", kVmBar, "m³/mol", 0, 4},
    {"physn0", "loschmidt", "n₀", "Costante di Loschmidt (1 atm)", kN0, "m⁻³", 0, 4},
    // ---- 5 Valori adottati
    {"g0", "standardgravity", "g₀", "Gravità standard", 9.80665, "m/s²", 0, 5},
    {"physatm", "", "atm", "Atmosfera standard", kAtm, "Pa", 0, 5},
    {"physdnucs", "cesiumfrequency", "Δν_Cs", "Frequenza del cesio 133", kDeltaNuCs, "Hz", 0, 5},
    {"physkcd", "luminousefficacy", "K_cd", "Efficacia luminosa", kKcd, "lm/W", 0, 5},
    {"physt0", "", "T₀", "Temperatura di 0 °C", kT0, "K", 0, 5},
    // ---- 6 Astronomiche (IAU)
    {"physau", "astronomicalunit", "au", "Unità astronomica", kAu, "m", 0, 6},
    {"physly", "lightyear", "ly", "Anno luce", kLy, "m", 0, 6},
    {"physpc", "parsec", "pc", "Parsec", kPc, "m", 0, 6},
    {"physgmsun", "", "GM☉", "Parametro GM del Sole", kGMSun, "m³/s²", 0, 6},
    {"physmsun", "solarmass", "M☉", "Massa del Sole", kGMSun / kG, "kg", 2.2e-5, 6},
    {"physrsun", "solarradius", "R☉", "Raggio del Sole", 6.957e8, "m", 0, 6},
    {"physlsun", "solarluminosity", "L☉", "Luminosità del Sole", 3.828e26, "W", 0, 6},
    {"physgmterra", "", "GM⊕", "Parametro GM della Terra", kGMEarth, "m³/s²", 0, 6},
    {"physmterra", "earthmass", "M⊕", "Massa della Terra", kGMEarth / kG, "kg", 2.2e-5, 6},
    {"physrterra", "earthradius", "R⊕", "Raggio equatoriale della Terra", 6.3781e6, "m", 0, 6},
};
constexpr std::size_t kGroupCount = sizeof(kGroups) / sizeof(kGroups[0]);
constexpr std::size_t kCount = sizeof(kTable) / sizeof(kTable[0]);

constexpr bool sortedByGroup() {
    for (std::size_t i = 0; i < kCount; ++i) {
        if (kTable[i].group >= kGroupCount) return false;
        if (i > 0 && kTable[i].group < kTable[i - 1].group) return false;
    }
    return true;
}
static_assert(sortedByGroup(), "kTable deve essere ordinata per gruppo");

// Primo indice di ogni gruppo (kGroupCount + 1 confini), calcolato a compile time.
constexpr std::array<std::size_t, kGroupCount + 1> groupBounds() {
    std::array<std::size_t, kGroupCount + 1> b{};
    for (std::size_t g = 0, i = 0; g <= kGroupCount; ++g) {
        while (i < kCount && kTable[i].group < g) ++i;
        b[g] = i;
    }
    b[kGroupCount] = kCount;
    return b;
}
constexpr auto kBounds = groupBounds();

char lowerAscii(char c) noexcept { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
bool equalNoCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) if (lowerAscii(a[i]) != lowerAscii(b[i])) return false;
    return true;
}
template <class Eq> const ConstDef* find(std::string_view name, Eq eq) noexcept {
    if (name.empty()) return nullptr;
    for (const ConstDef& k : kTable) {
        if (eq(name, k.parserName)) return &k;
        bool hit = false;
        forEachAlias(k, [&](std::string_view alias) { hit = hit || eq(name, alias); });
        if (hit) return &k;
    }
    return nullptr;
}
} // namespace

std::span<const ConstGroup> constGroups() noexcept { return {kGroups, kGroupCount}; }
std::span<const ConstDef> allConstants() noexcept { return {kTable, kCount}; }
std::span<const ConstDef> constantsOf(std::size_t group) noexcept {
    if (group >= kGroupCount) return {};
    return {kTable + kBounds[group], kBounds[group + 1] - kBounds[group]};
}
const ConstDef* findConstant(std::string_view lowerName) noexcept {
    return find(lowerName, [](std::string_view a, std::string_view b) { return a == b; });
}
const ConstDef* findConstantNoCase(std::string_view name) noexcept { return find(name, equalNoCase); }
const ConstDef* findByGlyph(std::string_view glyph) noexcept {
    if (glyph.empty()) return nullptr;
    for (const ConstDef& k : kTable) if (glyph == k.glyph) return &k;
    return nullptr;
}
} // namespace fx::physconst
