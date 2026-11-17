// Unita' di misura: tabella unica e conversioni (vedi units.hpp).
//
// Costanti:
//  - esatte per definizione: SI 2019 (BIPM, 9a ed. della brochure SI) per
//    c, h, e, k, N_A; NIST SP 811 (2008) App. B e NIST HB 44 per le unita'
//    anglosassoni (yard 1959: 0,9144 m; libbra 0,45359237 kg; gallone US
//    231 in³; gallone UK 4,54609 L; Btu IT 1055,05585262 J; caloria IT 4,1868 J);
//  - misurate: CODATA 2018 (NIST, maggio 2019), come la fx-CG50 per u;
//  - astronomiche: IAU 2012 B2 (unita' astronomica), IAU 2015 B2/B3 (parsec,
//    valori nominali di Terra e Sole).
// Le unita' fx-CG50 (le prime di ogni categoria LENGTH...POWER, flag FX) hanno
// esattamente i fattori della v1.13.1: Run-Matrix resta identico.
#include "units.hpp"
#include "expression.hpp"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fx::units {
namespace {
constexpr double kPi = 3.14159265358979323846;
// SI 2019, esatte.
constexpr double kC = 299792458.0;            // velocita' della luce, m/s
constexpr double kH = 6.62607015e-34;         // costante di Planck, J·s
constexpr double kE = 1.602176634e-19;        // carica elementare, C
constexpr double kK = 1.380649e-23;           // costante di Boltzmann, J/K
constexpr double kNA = 6.02214076e23;         // costante di Avogadro, 1/mol
constexpr double kG0 = 9.80665;               // gravita' standard (CGPM 1901), m/s²
// CODATA 2018.
constexpr double kU = 1.66053906660e-27;      // unita' di massa atomica, kg (come la fx-CG50)
constexpr double kMe = 9.1093837015e-31;      // massa dell'elettrone, kg
constexpr double kMp = 1.67262192369e-27;     // massa del protone, kg
constexpr double kA0 = 5.29177210903e-11;     // raggio di Bohr, m
constexpr double kLP = 1.616255e-35;          // lunghezza di Planck, m
constexpr double kTP = 5.391247e-44;          // tempo di Planck, s
constexpr double kMP = 2.176434e-8;           // massa di Planck, kg
constexpr double kEh = 4.3597447222071e-18;   // energia di Hartree, J
constexpr double kRy = 2.1798723611035e-18;   // energia di Rydberg h·c·R∞, J
// Anglosassoni (esatte, NIST SP 811 B.8).
constexpr double kLb = 0.45359237, kLbf = 4.4482216152605, kIn = 0.0254, kFt = 0.3048;
constexpr double kIn2 = 6.4516e-4, kFt2 = 0.09290304, kIn3 = 1.6387064e-5, kFt3 = 0.028316846592;
constexpr double kGalUS = 3.785411784e-3, kGalUK = 4.54609e-3, kMile = 1609.344, kBtu = 1055.05585262;
constexpr double kOz = 0.028349523125, kBbl = 0.158987294928;
constexpr double kCmil = kPi * 2.54e-5 * 2.54e-5 / 4;    // circular mil, m²

enum Cat : std::uint8_t {
    LEN, AREA, VOL, TIME, TMPR, VEL, MASS, FORCE, PRES, ENER, POW,      // fx-CG50, nell'ordine del menu CONVERT
    DTMP, ACC, TORQ, ANG, SANG, FREQ, DENS, VFLOW, MFLOW, FUEL, DATA, RATE,
    CHG, CUR, VOLT, RES, COND, CAP, IND, RESV, CONDV, MFLUX, MDENS, HFIELD,
    LINT, LFLUX, ILLUM, LUM, ACT, DOSE, DEQ, EXPO, AMT, MOLC, MASC, RATIO,
    VISD, VISK, SURF, TCOND, HTC, CP, HFLUX, ENTR, TRES, RVAL, MOM, ANGM, INER, STIFF, ANGV,
    PRAT, ARAT, SPL, VLEV, FRAT, PH, AWG, SEIS, STEL, PHOT, MEQ, KT, MOLM, WGT, SHOE, CAT_COUNT
};
using G = Group;
const Category kCategories[CAT_COUNT] = {
    {"Lunghezza", "LENGTH", "m", "px: punti per pollice impostabili (PARAM)", G::BaseSI, Param::Dpi},
    {"Area", "AREA", "m\xC2\xB2", nullptr, G::BaseSI, Param::None},
    {"Volume", "VOLUME", "m\xC2\xB3", nullptr, G::BaseSI, Param::None},
    {"Tempo", "TIME", "s", "anno gregoriano 365,2425 d; mese medio 30,436875 d", G::BaseSI, Param::None},
    {"Temperatura", "TMPR", "K", "Temperature assolute (per le differenze: Intervallo di temperatura)", G::Thermal, Param::None},
    {"Velocita'", "VELOCITY", "m/s", "mach: ISA, 15 \xC2\xB0" "C al livello del mare (340,294 m/s); min/km: ritmo (inverso)", G::Mechanics, Param::None},
    {"Massa", "MASS", "kg", nullptr, G::BaseSI, Param::None},
    {"Forza", "FORCE", "N", nullptr, G::Mechanics, Param::None},
    {"Pressione", "PRESSURE", "Pa", "colonne d'acqua e di mercurio convenzionali (g\xE2\x82\x80)", G::Mechanics, Param::None},
    {"Energia", "ENERGY", "J", "E = mc\xC2\xB2: vedi Massa \xE2\x86\x94 energia", G::Mechanics, Param::None},
    {"Potenza", "POWER", "W", "dBm = 10\xC2\xB7log\xE2\x82\x81\xE2\x82\x80(P/1 mW), dBW = 10\xC2\xB7log\xE2\x82\x81\xE2\x82\x80(P/1 W)", G::Mechanics, Param::None},
    {"Intervallo di temperatura", nullptr, "K", "Differenze di temperatura (nessun offset)", G::Thermal, Param::None},
    {"Accelerazione", nullptr, "m/s\xC2\xB2", "g\xE2\x82\x80 = 9,80665 m/s\xC2\xB2 (esatta)", G::Mechanics, Param::None},
    {"Coppia / momento", nullptr, "N\xE2\x80\xA2m", nullptr, G::Mechanics, Param::None},
    {"Angolo piano", nullptr, "rad", nullptr, G::BaseSI, Param::None},
    {"Angolo solido", nullptr, "sr", nullptr, G::BaseSI, Param::None},
    {"Frequenza", nullptr, "Hz", "rad/s: pulsazione \xCF\x89 = 2\xCF\x80" "f", G::BaseSI, Param::None},
    {"Densita'", nullptr, "kg/m\xC2\xB3", nullptr, G::Mechanics, Param::None},
    {"Portata volumetrica", nullptr, "m\xC2\xB3/s", nullptr, G::Mechanics, Param::None},
    {"Portata massica", nullptr, "kg/s", nullptr, G::Mechanics, Param::None},
    {"Consumo di carburante", nullptr, "m/m\xC2\xB3", "L/100 km e gal/100 mi sono inversi di km/L e mpg", G::Mechanics, Param::None},
    {"Dati digitali", nullptr, "bit", "k = 1000, Ki = 1024 (IEC 80000-13)", G::Data, Param::None},
    {"Velocita' di trasmissione", nullptr, "bit/s", "k = 1000, Ki = 1024 (IEC 80000-13)", G::Data, Param::None},
    {"Carica elettrica", nullptr, "C", "e = 1,602176634E-19 C (esatta)", G::Electricity, Param::None},
    {"Corrente elettrica", nullptr, "A", nullptr, G::Electricity, Param::None},
    {"Tensione elettrica", nullptr, "V", nullptr, G::Electricity, Param::None},
    {"Resistenza", nullptr, "\xCE\xA9", nullptr, G::Electricity, Param::None},
    {"Conduttanza", nullptr, "S", nullptr, G::Electricity, Param::None},
    {"Capacita'", nullptr, "F", nullptr, G::Electricity, Param::None},
    {"Induttanza", nullptr, "H", nullptr, G::Electricity, Param::None},
    {"Resistivita'", nullptr, "\xCE\xA9\xE2\x80\xA2m", nullptr, G::Electricity, Param::None},
    {"Conducibilita' elettrica", nullptr, "S/m", nullptr, G::Electricity, Param::None},
    {"Flusso magnetico", nullptr, "Wb", nullptr, G::Electricity, Param::None},
    {"Induzione magnetica", nullptr, "T", nullptr, G::Electricity, Param::None},
    {"Campo magnetico H", nullptr, "A/m", "1 Oe = 1000/(4\xCF\x80) A/m", G::Electricity, Param::None},
    {"Intensita' luminosa", nullptr, "cd", nullptr, G::LightRadiation, Param::None},
    {"Flusso luminoso", nullptr, "lm", nullptr, G::LightRadiation, Param::None},
    {"Illuminamento", nullptr, "lx", nullptr, G::LightRadiation, Param::None},
    {"Luminanza", nullptr, "cd/m\xC2\xB2", nullptr, G::LightRadiation, Param::None},
    {"Attivita' radioattiva", nullptr, "Bq", nullptr, G::LightRadiation, Param::None},
    {"Dose assorbita", nullptr, "Gy", nullptr, G::LightRadiation, Param::None},
    {"Dose equivalente", nullptr, "Sv", nullptr, G::LightRadiation, Param::None},
    {"Esposizione", nullptr, "C/kg", "1 R = 2,58E-4 C/kg (esatto)", G::LightRadiation, Param::None},
    {"Quantita' di sostanza", nullptr, "mol", "N_A = 6,02214076E23 /mol (esatta)", G::Chemistry, Param::None},
    {"Concentrazione molare", nullptr, "mol/m\xC2\xB3", "1 M = 1 mol/L", G::Chemistry, Param::None},
    {"Concentrazione in massa", nullptr, "kg/m\xC2\xB3", nullptr, G::Chemistry, Param::None},
    {"Rapporti adimensionali", nullptr, "1", "% \xE2\x80\xB0 punti base, ppm, ppb...", G::Chemistry, Param::None},
    {"Viscosita' dinamica", nullptr, "Pa\xE2\x80\xA2s", nullptr, G::Mechanics, Param::None},
    {"Viscosita' cinematica", nullptr, "m\xC2\xB2/s", nullptr, G::Mechanics, Param::None},
    {"Tensione superficiale", nullptr, "N/m", nullptr, G::Mechanics, Param::None},
    {"Conducibilita' termica", nullptr, "W/(m\xE2\x80\xA2K)", "cal e Btu IT", G::Thermal, Param::None},
    {"Coeff. di scambio termico", nullptr, "W/(m\xC2\xB2\xE2\x80\xA2K)", "cal e Btu IT", G::Thermal, Param::None},
    {"Calore specifico", nullptr, "J/(kg\xE2\x80\xA2K)", "cal e Btu IT", G::Thermal, Param::None},
    {"Flusso termico", nullptr, "W/m\xC2\xB2", "cal e Btu IT", G::Thermal, Param::None},
    {"Entropia / capacita' termica", nullptr, "J/K", "k = costante di Boltzmann", G::Thermal, Param::None},
    {"Resistenza termica", nullptr, "K/W", nullptr, G::Thermal, Param::None},
    {"Isolamento termico (R)", nullptr, "m\xC2\xB2\xE2\x80\xA2K/W", "1 clo = 0,155 m\xC2\xB2\xE2\x80\xA2K/W (ISO 9920)", G::Thermal, Param::None},
    {"Quantita' di moto / impulso", nullptr, "kg\xE2\x80\xA2m/s", nullptr, G::Mechanics, Param::None},
    {"Momento angolare", nullptr, "kg\xE2\x80\xA2m\xC2\xB2/s", "\xC4\xA7 = h/(2\xCF\x80)", G::Mechanics, Param::None},
    {"Momento d'inerzia", nullptr, "kg\xE2\x80\xA2m\xC2\xB2", nullptr, G::Mechanics, Param::None},
    {"Rigidezza", nullptr, "N/m", nullptr, G::Mechanics, Param::None},
    {"Velocita' angolare", nullptr, "rad/s", nullptr, G::Mechanics, Param::None},
    {"Rapporto di potenza", nullptr, "P/P0", "dB = 10\xC2\xB7log\xE2\x82\x81\xE2\x82\x80(P/P0), Np = \xC2\xBD\xC2\xB7ln(P/P0)", G::Logarithmic, Param::None},
    {"Rapporto di ampiezza", nullptr, "A/A0", "dB = 20\xC2\xB7log\xE2\x82\x81\xE2\x82\x80(A/A0), Np = ln(A/A0)", G::Logarithmic, Param::None},
    {"Pressione sonora", nullptr, "Pa", "dB SPL = 20\xC2\xB7log\xE2\x82\x81\xE2\x82\x80(p/20 \xC2\xB5Pa)", G::Logarithmic, Param::None},
    {"Livello di tensione", nullptr, "V", "dBV rif. 1 V, dBu rif. \xE2\x88\x9A" "0,6 V (0,775 V)", G::Logarithmic, Param::None},
    {"Rapporto di frequenza", nullptr, "f2/f1", "ottava = 2\xC2\xB7, semitono = 2^(1/12), cent = 2^(1/1200)", G::Logarithmic, Param::None},
    {"Acidita' (pH)", nullptr, "mol/L", "pH = -log\xE2\x82\x81\xE2\x82\x80[H\xE2\x81\xBA]; pH+pOH = 14 (Kw = 1E-14, 25 \xC2\xB0" "C)", G::Logarithmic, Param::None},
    {"Calibro dei fili (AWG)", nullptr, "m", "d = 0,127 mm\xC2\xB7" "92^((36-n)/39); 0000 = -3; sezione = \xCF\x80" "d\xC2\xB2/4", G::Electricity, Param::None},
    {"Magnitudo sismica", nullptr, "J", "log\xE2\x82\x81\xE2\x82\x80" "E[J] = 1,5\xC2\xB7M + 4,8 (Gutenberg-Richter)", G::Logarithmic, Param::None},
    {"Magnitudo stellare", nullptr, "F/F0", "F/F0 = 10^(-0,4\xC2\xB7\xCE\x94m) (Pogson)", G::Logarithmic, Param::None},
    {"Fotone: \xCE\xBB \xE2\x86\x94 \xCE\xBD \xE2\x86\x94 E \xE2\x86\x94 \xCF\x83", nullptr, "Hz", "\xCE\xBD = c/\xCE\xBB = E/h = c\xC2\xB7\xCF\x83; c = 299792458 m/s, h = 6,62607015E-34 J\xE2\x80\xA2s (esatte)", G::Derived, Param::None},
    {"Massa \xE2\x86\x94 energia (E = mc\xC2\xB2)", nullptr, "J", "c\xC2\xB2 = 89875517873681764 m\xC2\xB2/s\xC2\xB2 (esatta)", G::Derived, Param::None},
    {"Temperatura \xE2\x86\x94 energia (kT)", nullptr, "J", "E = k\xC2\xB7T; k = 1,380649E-23 J/K, N_A = 6,02214076E23 /mol (esatte)", G::Derived, Param::None},
    {"Moli \xE2\x86\x94 massa \xE2\x86\x94 particelle", nullptr, "mol", "n = m/M = N/N_A; M massa molare (PARAM); gas ideale a 0 \xC2\xB0" "C, 101,325 kPa", G::Derived, Param::MolarMass},
    {"Peso \xE2\x86\x94 massa (g\xE2\x82\x80)", nullptr, "N", "P = m\xC2\xB7g\xE2\x82\x80, g\xE2\x82\x80 = 9,80665 m/s\xC2\xB2 (esatta)", G::Derived, Param::None},
    {"Scarpe (adulti)", nullptr, "m", "APPROSSIMATO: piede L cm; EU = 1,5(L+1,5); UK = 3(L+1,5)/2,54-25; US = UK+1 (uomo), +2 (donna)", G::Sizes, Param::None},
};
const char* const kFxNames[] = {"LENGTH", "AREA", "VOLUME", "TIME", "TMPR", "VELOCITY", "MASS", "FORCE", "PRESSURE", "ENERGY", "POWER", nullptr};

// Fonti dei fattori (Unit::source()).
enum Src : std::uint8_t { DEF = 0, FXV = 1, NIST = 2, CODATA = 3, IAU = 4, ASTRO = 5, ISA = 6, HIST = 7, APPROXF = 8, CONV = 9 };
const char* const kSources[] = {
    "esatto per definizione (SI 2019, NIST SP 811 App. B)",
    "valore della fx-CG50 (invariato)",
    "NIST SP 811 (2008) App. B.8-B.9, arrotondato",
    "CODATA 2018",
    "IAU 2012 B2 / IAU 2015 B2-B3 (valore nominale)",
    "valore astronomico medio (IERS / Explanatory Supplement)",
    "atmosfera standard ISO 2533 (15 gradi C, livello del mare)",
    "valore storico convenzionale (approssimato)",
    "formula o tabella convenzionale approssimata",
    "convenzione internazionale (esatta per accordo)",
};

// Offset delle unita' affini: s = (v + offset) × factor.
struct Offset { double v; const char* exact; };
const Offset kOffsets[] = {
    {273.15, "273.15"},                    // 0 °C
    {459.67, "459.67"},                    // 1 °F
    {218.52, "218.52"},                    // 2 °Ré: K = v·5/4 + 273,15
    {135.90375, "135.90375"},              // 3 °Rø: °C = (v - 7,5)·40/21
    {-559.725, "-559.725"},                // 4 °De: °C = 100 - v·2/3
    {90.1395, "90.1395"},                  // 5 °N: °C = v·100/33
    {-2.25, nullptr},                      // 6 scarpe EU: L = EU/1,5 - 1,5 cm
    {25 - 4.5 / 2.54, nullptr},            // 7 scarpe UK: L = (UK+25)·2,54/3 - 1,5 cm
    {24 - 4.5 / 2.54, nullptr},            // 8 scarpe US uomo = UK + 1
    {23 - 4.5 / 2.54, nullptr},            // 9 scarpe US donna = UK + 2
};

// Funzioni non lineari (Unit::aux), factor = k.
enum Fn : std::uint8_t { F_LEV10 = 1, F_LEV20, F_POW10, F_EXP, F_POW2, F_AWG, F_AREA, F_SEIS, F_POH };
const char* const kFormulas[] = {
    "", "s = k\xC2\xB7" "10^(v/10)", "s = k\xC2\xB7" "10^(v/20)", "s = 10^(k\xC2\xB7v)", "s = e^(k\xC2\xB7v)", "s = 2^(k\xC2\xB7v)",
    "d = 0,127 mm\xC2\xB7" "92^((36-n)/39)", "d = \xE2\x88\x9A(4\xC2\xB7k\xC2\xB7v/\xCF\x80)", "E = 10^(1,5\xC2\xB7M+4,8) J", "[H\xE2\x81\xBA] = 10^(pOH-14)",
};

// Flag e tipi abbreviati per la tabella.
constexpr std::uint8_t FX = flag::FxCG50, PSI = flag::PrefixSI, PIEC = flag::PrefixIEC, APX = flag::Approx,
                       PRM = flag::Param, HIS = flag::Historic, XD = flag::ExactDecimal;
constexpr std::uint8_t ks(Kind k, Src s) { return static_cast<std::uint8_t>(static_cast<unsigned>(k) | (static_cast<unsigned>(s) << 4)); }
constexpr Kind LI = Kind::Linear, AF = Kind::Affine, IV = Kind::Inverse, FN = Kind::Function;
// E: lineare esatta (decimale scritto); X: lineare esatta con testo; M: lineare
// misurata/convenzionale con fonte; A: affine; I: inversa; F: funzione.
#define E(f, n, l, c, fl) {f, n, l, nullptr, c, static_cast<std::uint8_t>((fl) | XD), ks(LI, DEF), 0}
#define X(f, n, l, ex, c, fl) {f, n, l, ex, c, fl, ks(LI, DEF), 0}
#define M(f, n, l, c, fl, src) {f, n, l, nullptr, c, fl, ks(LI, src), 0}
#define A(f, n, l, ex, c, fl, off, src) {f, n, l, ex, c, fl, ks(AF, src), off}
#define I(f, n, l, c, fl, src) {f, n, l, nullptr, c, fl, ks(IV, src), 0}
#define F(k, n, l, c, fl, fn, src) {k, n, l, nullptr, c, fl, ks(FN, src), fn}

#define SQ "\xC2\xB2"
#define CU "\xC2\xB3"
#define MU "\xC2\xB5"
#define DOT "\xE2\x80\xA2"
#define DEG "\xC2\xB0"
#define OHM "\xCE\xA9"
#define DELTA "\xCE\x94"

const Unit kUnits[] = {
    // ------------------------------------------------------------ LENGTH (fx: fm Å µm mm cm m km AU l.y. pc Mil in ft yd fath rd mile n mile)
    E(1e-15, "fm", "femtometro", LEN, FX),
    E(1e-12, "pm", "picometro", LEN, 0),
    E(1e-10, "\xC3\x85", "angstrom", LEN, FX),
    M(kA0, "a0", "raggio di Bohr", LEN, 0, CODATA),
    E(1e-9, "nm", "nanometro", LEN, 0),
    E(1e-6, MU "m", "micrometro", LEN, FX),
    E(1e-3, "mm", "millimetro", LEN, FX),
    E(1e-2, "cm", "centimetro", LEN, FX),
    E(0.1, "dm", "decimetro", LEN, 0),
    E(1, "m", "metro", LEN, FX | PSI),
    E(10, "dam", "decametro", LEN, 0),
    E(100, "hm", "ettometro", LEN, 0),
    E(1e3, "km", "chilometro", LEN, FX),
    E(1e6, "Mm", "megametro", LEN, 0),
    E(1e9, "Gm", "gigametro", LEN, 0),
    M(6.3781e6, "R_T", "raggio terrestre equatoriale", LEN, 0, IAU),          // IAU 2015 B3, R_E nominale
    M(3.84399e8, "d_TL", "distanza media Terra-Luna", LEN, 0, ASTRO),         // semiasse maggiore dell'orbita lunare
    E(1.495978707e11, "AU", "unita' astronomica", LEN, FX),                   // IAU 2012 B2, esatta
    E(9.4607304725808e15, "l.y.", "anno luce", LEN, FX),                      // c × anno giuliano, esatto
    M(3.0856775814913673e16, "pc", "parsec", LEN, FX, IAU),                   // IAU 2015 B2: 648000/π au
    M(3.0856775814913673e22, "Mpc", "megaparsec", LEN, 0, IAU),
    M(3.0856775814913673e25, "Gpc", "gigaparsec", LEN, 0, IAU),
    E(2.54e-5, "Mil", "mil (millesimo di pollice)", LEN, FX),
    E(2.54e-5, "thou", "thou (= mil)", LEN, 0),
    E(0.0254, "in", "pollice", LEN, FX),
    E(0.1016, "hand", "palmo (hand, 4 in)", LEN, 0),
    E(0.3048, "ft", "piede", LEN, FX),
    X(1200.0 / 3937.0, "ft(US)", "piede US survey", "1200/3937", LEN, 0),
    E(0.9144, "yd", "iarda", LEN, FX),
    E(1.8288, "fath", "braccio (fathom)", LEN, FX),
    E(0.201168, "link", "link (1/100 chain)", LEN, 0),
    E(5.0292, "rd", "pertica (rod)", LEN, FX),
    E(20.1168, "chain", "catena (chain, 66 ft)", LEN, 0),
    E(201.168, "furlong", "furlong (1/8 mile)", LEN, 0),
    E(1609.344, "mile", "miglio", LEN, FX),
    E(4828.032, "league", "lega (3 miglia)", LEN, 0),
    E(1852, "n mile", "miglio nautico", LEN, FX),
    E(185.2, "cable", "cavo (1/10 miglio nautico)", LEN, 0),
    E(5556, "nleague", "lega nautica (3 miglia nautiche)", LEN, 0),
    X(0.0254 / 72, "pt", "punto tipografico (DTP, 1/72 in)", "0.0254/72", LEN, 0),
    X(0.0254 / 6, "pica", "pica (12 pt)", "0.0254/6", LEN, 0),
    X(0.0254 / 1440, "twip", "twip (1/20 pt)", "0.0254/1440", LEN, 0),
    {0.0254, "px", "pixel (1 in / DPI)", nullptr, LEN, PRM, ks(LI, DEF), 0},
    M(kLP, "l_P", "lunghezza di Planck", LEN, 0, CODATA),
    M(0.296, "pes", "piede romano", LEN, HIS | APX, HIST),
    M(1.48, "passus", "passo romano (5 piedi)", LEN, HIS | APX, HIST),
    M(1480, "m.p.", "miglio romano (mille passus)", LEN, HIS | APX, HIST),
    // ------------------------------------------------------------ AREA (fx: cm² m² ha km² in² ft² yd² acre mile²)
    E(1e-28, "b", "barn", AREA, 0),
    M(kCmil, "cmil", "circular mil", AREA, 0, DEF),
    E(1e-6, "mm" SQ, "millimetro quadrato", AREA, 0),
    E(1e-4, "cm" SQ, "centimetro quadrato", AREA, FX),
    E(1e-2, "dm" SQ, "decimetro quadrato", AREA, 0),
    E(1, "m" SQ, "metro quadrato", AREA, FX),
    E(100, "a", "ara", AREA, 0),
    E(100, "dam" SQ, "decametro quadrato", AREA, 0),
    E(1e4, "hm" SQ, "ettometro quadrato", AREA, 0),
    E(1e4, "ha", "ettaro", AREA, FX),
    E(1e6, "km" SQ, "chilometro quadrato", AREA, FX),
    E(6.4516e-4, "in" SQ, "pollice quadrato", AREA, FX),
    E(0.09290304, "ft" SQ, "piede quadrato", AREA, FX),
    E(0.83612736, "yd" SQ, "iarda quadrata", AREA, FX),
    E(1011.7141056, "rood", "rood (1/4 acro)", AREA, 0),
    E(4046.8564224, "acre", "acro", AREA, FX),
    E(2589988.110336, "mile" SQ, "miglio quadrato", AREA, FX),
    X((6336000.0 / 3937) * (6336000.0 / 3937), "section", "sezione (1 miglio\xC2\xB2 US survey)", "6336000/3937*6336000/3937", AREA, 0),
    X(36 * (6336000.0 / 3937) * (6336000.0 / 3937), "township", "township (36 sezioni)", "36*6336000/3937*6336000/3937", AREA, 0),
    M(654.5179, "pert.MI", "pertica milanese", AREA, HIS | APX, HIST),
    M(2080.44, "torn.BO", "tornatura bolognese", AREA, HIS | APX, HIST),
    M(3364.86, "mogg.NA", "moggio napoletano", AREA, HIS | APX, HIST),
    M(3809.96, "giorn.PI", "giornata piemontese", AREA, HIS | APX, HIST),
    // ------------------------------------------------------------ VOLUME (fx: cm³ mL L m³ in³ ft³ fl_oz(UK) fl_oz(US) gal(US) gal(UK) pt qt tsp tbsp cup)
    E(6.1611519921875e-8, "minim", "minim US", VOL, 0),
    E(1e-9, "mm" CU, "millimetro cubo", VOL, 0),
    E(1e-6, "cm" CU, "centimetro cubo", VOL, FX),
    E(1e-6, "mL", "millilitro", VOL, FX),
    E(1e-5, "cL", "centilitro", VOL, 0),
    E(1e-4, "dL", "decilitro", VOL, 0),
    E(1e-3, "L", "litro", VOL, FX | PSI),
    E(1e-3, "dm" CU, "decimetro cubo", VOL, 0),
    E(0.1, "hL", "ettolitro", VOL, 0),
    E(1, "kL", "chilolitro", VOL, 0),
    E(1, "m" CU, "metro cubo", VOL, FX),
    E(1, "st", "stero (legna)", VOL, 0),
    E(1e3, "ML", "megalitro", VOL, 0),
    E(1e9, "km" CU, "chilometro cubo", VOL, 0),
    E(1.6387064e-5, "in" CU, "pollice cubo", VOL, FX),
    E(0.028316846592, "ft" CU, "piede cubo", VOL, FX),
    E(0.764554857984, "yd" CU, "iarda cuba", VOL, 0),
    E(3.624556363776, "cord", "cord (128 ft\xC2\xB3)", VOL, 0),
    E(2.8316846592, "RT", "tonnellata di stazza (100 ft\xC2\xB3)", VOL, 0),
    E(1233.48183754752, "ac" DOT "ft", "acro-piede", VOL, 0),
    E(2.84130625e-5, "fl_oz(UK)", "oncia fluida UK", VOL, FX),
    E(2.95735295625e-5, "fl_oz(US)", "oncia fluida US", VOL, FX),
    E(3.6966911953125e-6, "fl_dr", "dramma fluida US", VOL, 0),
    E(1.1829411825e-4, "gill(US)", "gill US", VOL, 0),
    E(1.420653125e-4, "gill(UK)", "gill UK", VOL, 0),
    E(3.785411784e-3, "gal(US)", "gallone US", VOL, FX),
    E(4.54609e-3, "gal(UK)", "gallone UK", VOL, FX),
    E(0.158987294928, "bbl", "barile di petrolio (42 gal US)", VOL, 0),
    E(0.119240471196, "bbl(US)", "barile US liquidi (31,5 gal)", VOL, 0),
    E(0.03523907016688, "bu(US)", "bushel US", VOL, 0),
    E(0.00880976754172, "pk(US)", "peck US", VOL, 0),
    E(0.03636872, "bu(UK)", "bushel UK", VOL, 0),
    E(0.00909218, "pk(UK)", "peck UK", VOL, 0),
    E(4.73176473e-4, "pt", "pinta US", VOL, FX),
    E(5.6826125e-4, "pt(UK)", "pinta UK", VOL, 0),
    E(9.46352946e-4, "qt", "quarto US", VOL, FX),
    E(1.1365225e-3, "qt(UK)", "quarto UK", VOL, 0),
    E(4.92892159375e-6, "tsp", "cucchiaino US", VOL, FX),
    E(1.478676478125e-5, "tbsp", "cucchiaio US", VOL, FX),
    E(2.365882365e-4, "cup", "tazza US", VOL, FX),
    E(5e-6, "tsp(m)", "cucchiaino metrico", VOL, 0),
    E(1.5e-5, "tbsp(m)", "cucchiaio metrico", VOL, 0),
    E(2.5e-4, "cup(m)", "tazza metrica", VOL, 0),
    // ------------------------------------------------------------ TIME (fx: ns µs ms s min h day week yr s-yr t-yr)
    M(kTP, "t_P", "tempo di Planck", TIME, 0, CODATA),
    E(1e-15, "fs", "femtosecondo", TIME, 0),
    E(1e-12, "ps", "picosecondo", TIME, 0),
    E(1e-9, "ns", "nanosecondo", TIME, FX),
    E(1e-8, "shake", "shake", TIME, 0),
    E(1e-6, MU "s", "microsecondo", TIME, FX),
    E(1e-3, "ms", "millisecondo", TIME, FX),
    E(1, "s", "secondo", TIME, FX | PSI),
    E(60, "min", "minuto", TIME, FX),
    E(3600, "h", "ora", TIME, FX),
    M(86164.0905, "day(sid)", "giorno sidereo", TIME, 0, ASTRO),             // IERS: 23 h 56 min 4,0905 s
    E(86400, "day", "giorno", TIME, FX),
    E(604800, "week", "settimana", TIME, FX),
    E(1209600, "fortnight", "quindicina (14 giorni)", TIME, 0),
    E(2629746, "mese", "mese medio (30,436875 d)", TIME, 0),
    M(2551442.8768992, "mese(sin)", "mese sinodico (29,530588853 d)", TIME, 0, ASTRO),
    E(31536000, "yr", "anno (365 d)", TIME, FX),
    M(31556925.9747, "s-yr", "anno solare (tropico 1900)", TIME, FX, FXV),
    M(31558149.76, "t-yr", "anno siderale (fx-CG50)", TIME, FX, FXV),
    E(31557600, "yr(jul)", "anno giuliano (365,25 d)", TIME, 0),
    E(31556952, "yr(greg)", "anno gregoriano (365,2425 d)", TIME, 0),
    M(31558149.7635456, "yr(sid)", "anno sidereo J2000 (365,256363004 d)", TIME, 0, ASTRO),
    E(315569520, "decennio", "decennio (gregoriano)", TIME, 0),
    E(3155695200, "secolo", "secolo (gregoriano)", TIME, 0),
    E(31556952000, "millennio", "millennio (gregoriano)", TIME, 0),
    // ------------------------------------------------------------ TMPR (fx: °C K °F °R)
    A(1, DEG "C", "grado Celsius", "1", TMPR, FX, 0, DEF),
    E(1, "K", "kelvin", TMPR, FX),
    A(5.0 / 9.0, DEG "F", "grado Fahrenheit", "5/9", TMPR, FX, 1, DEF),
    X(5.0 / 9.0, DEG "R", "grado Rankine", "5/9", TMPR, FX),
    A(5.0 / 4.0, DEG "R\xC3\xA9", "grado Reaumur", "5/4", TMPR, 0, 2, DEF),
    A(40.0 / 21.0, DEG "R\xC3\xB8", "grado Romer", "40/21", TMPR, 0, 3, DEF),
    A(-2.0 / 3.0, DEG "De", "grado Delisle", "-2/3", TMPR, 0, 4, DEF),
    A(100.0 / 33.0, DEG "N", "grado Newton", "100/33", TMPR, 0, 5, DEF),
    // ------------------------------------------------------------ VELOCITY (fx: m/s km/h knot ft/s mile/h)
    E(1e-3, "mm/s", "millimetri al secondo", VEL, 0),
    E(1e-2, "cm/s", "centimetri al secondo", VEL, 0),
    E(0.0254, "in/s", "pollici al secondo", VEL, 0),
    X(0.3048 / 60, "ft/min", "piedi al minuto", "0.3048/60", VEL, 0),
    X(1.0 / 60, "m/min", "metri al minuto", "1/60", VEL, 0),
    E(1, "m/s", "metri al secondo", VEL, FX),
    X(1 / 3.6, "km/h", "chilometri orari", "1/3.6", VEL, FX),
    X(1852.0 / 3600.0, "knot", "nodo", "1852/3600", VEL, FX),
    E(0.3048, "ft/s", "piedi al secondo", VEL, FX),
    E(0.9144, "yd/s", "iarde al secondo", VEL, 0),
    E(0.44704, "mile/h", "miglia orarie", VEL, FX),
    E(1e3, "km/s", "chilometri al secondo", VEL, 0),
    M(340.294, "mach", "mach (ISA, 15 \xC2\xB0" "C, livello del mare)", VEL, 0, ISA),   // ISO 2533: a0 = 340,294 m/s
    M(343.21, "v_s", "velocita' del suono (aria secca, 20 \xC2\xB0" "C)", VEL, APX, APPROXF),  // 331,3·√(293,15/273,15)
    E(kC, "c", "velocita' della luce", VEL, 0),
    I(1000.0 / 60, "min/km", "ritmo (minuti al km)", VEL, 0, DEF),
    I(1609.344 / 60, "min/mile", "ritmo (minuti al miglio)", VEL, 0, DEF),
    I(1000, "s/km", "secondi al km", VEL, 0, DEF),
    // ------------------------------------------------------------ MASS (fx: u mg g kg mton oz lb slug ton(short) ton(long))
    M(kMe, "m_e", "massa dell'elettrone", MASS, 0, CODATA),
    M(kU, "u", "unita' di massa atomica", MASS, FX, CODATA),
    M(kU, "Da", "dalton (= u)", MASS, 0, CODATA),
    M(kMp, "m_p", "massa del protone", MASS, 0, CODATA),
    E(1e-12, "ng", "nanogrammo", MASS, 0),
    E(1e-9, MU "g", "microgrammo", MASS, 0),
    E(1e-6, "mg", "milligrammo", MASS, FX),
    E(2e-4, "ct", "carato metrico", MASS, 0),
    E(6.479891e-5, "gr", "grano", MASS, 0),
    E(1e-3, "g", "grammo", MASS, FX | PSI),
    E(1.55517384e-3, "dwt", "pennyweight", MASS, 0),
    E(1.7718451953125e-3, "dr", "dramma (avoirdupois)", MASS, 0),
    E(1, "kg", "chilogrammo", MASS, FX),
    E(100, "q", "quintale", MASS, 0),
    E(1000, "mton", "tonnellata metrica", MASS, FX),
    E(1000, "t", "tonnellata", MASS, PSI),
    E(0.028349523125, "oz", "oncia", MASS, FX),
    E(0.0311034768, "oz t", "oncia troy", MASS, 0),
    E(0.3732417216, "lb t", "libbra troy", MASS, 0),
    E(0.45359237, "lb", "libbra", MASS, FX),
    E(6.35029318, "stone", "stone (14 lb)", MASS, 0),
    M(14.593902937206, "slug", "slug", MASS, FX, FXV),                        // NIST: 1,459 390 E+01
    E(907.18474, "ton(short)", "tonnellata corta", MASS, FX),
    E(1016.0469088, "ton(long)", "tonnellata lunga", MASS, FX),
    M(kMP, "m_P", "massa di Planck", MASS, 0, CODATA),
    M(5.9722e24, "M_T", "massa della Terra", MASS, 0, IAU),                  // IAU 2015 GM_E / G (CODATA 2018)
    M(1.98847e30, "M_S", "massa del Sole", MASS, 0, IAU),                    // IAU 2015 GM_S / G (CODATA 2018)
    // ------------------------------------------------------------ FORCE (fx: N lbf tonf dyne kgf)
    E(1e-6, MU "N", "micronewton", FORCE, 0),
    E(1e-3, "mN", "millinewton", FORCE, 0),
    E(1, "N", "newton", FORCE, FX | PSI),
    E(1e3, "kN", "chilonewton", FORCE, 0),
    E(1e6, "MN", "meganewton", FORCE, 0),
    E(4.4482216152605, "lbf", "libbra-forza", FORCE, FX),
    E(4448.2216152605, "kip", "kip (1000 lbf)", FORCE, 0),
    E(9964.01641818352, "tonf", "tonnellata-forza lunga", FORCE, FX),
    E(1e-5, "dyne", "dina", FORCE, FX),
    E(9.80665e-3, "gf", "grammo-forza", FORCE, 0),
    E(9.80665, "kgf", "chilogrammo-forza", FORCE, FX),
    X(kLbf / 16, "ozf", "oncia-forza", "4.4482216152605/16", FORCE, 0),
    E(0.138254954376, "pdl", "poundal", FORCE, 0),
    E(1000, "sn", "sthene", FORCE, 0),
    // ------------------------------------------------------------ PRESSURE (fx: Pa kPa mmH2O mmHg atm inH2O inHg lbf/in² bar kgf/cm²)
    E(0.1, "Ba", "barye", PRES, 0),
    E(0.1, MU "bar", "microbar", PRES, 0),
    E(1, "Pa", "pascal", PRES, FX | PSI),
    E(100, "hPa", "ettopascal", PRES, 0),
    E(100, "mbar", "millibar", PRES, 0),
    E(1e3, "kPa", "chilopascal", PRES, FX),
    E(1e6, "MPa", "megapascal", PRES, 0),
    E(1e9, "GPa", "gigapascal", PRES, 0),
    E(9.80665, "mmH2O", "millimetro d'acqua", PRES, FX),
    E(98.0665, "cmH2O", "centimetro d'acqua", PRES, 0),
    X(101325.0 / 760.0, "mmHg", "millimetro di mercurio", "101325/760", PRES, FX),
    X(101325.0 / 760.0, "Torr", "torr", "101325/760", PRES, 0),
    E(1333.22387415, "cmHg", "centimetro di mercurio (conv.)", PRES, 0),
    E(101325, "atm", "atmosfera", PRES, FX),
    E(98066.5, "at", "atmosfera tecnica", PRES, 0),
    E(249.08891, "inH2O", "pollice d'acqua", PRES, FX),
    E(2989.06692, "ftH2O", "piede d'acqua", PRES, 0),
    E(3386.388640341, "inHg", "pollice di mercurio", PRES, FX),
    X(kLbf / kFt2, "psf", "libbra-forza al piede\xC2\xB2", "4.4482216152605/0.09290304", PRES, 0),
    M(6894.757293168, "lbf/in" SQ, "libbra-forza al pollice\xC2\xB2", PRES, FX, FXV),
    X(kLbf / kIn2, "psi", "psi (lbf/in\xC2\xB2)", "4.4482216152605/0.00064516", PRES, 0),
    X(1000 * kLbf / kIn2, "ksi", "ksi (1000 psi)", "4448.2216152605/0.00064516", PRES, 0),
    E(1e5, "bar", "bar", PRES, FX),
    E(98066.5, "kgf/cm" SQ, "chilogrammo-forza al cm\xC2\xB2", PRES, FX),
    // ------------------------------------------------------------ ENERGY (fx: eV J calth cal15 calIT kcalth kcal15 kcalIT l-atm kW•h ft•lbf Btu erg kgf•m)
    M(kRy, "Ry", "rydberg", ENER, 0, CODATA),
    M(kEh, "Eh", "hartree", ENER, 0, CODATA),
    E(1.602176634e-19, "eV", "elettronvolt", ENER, FX | PSI),
    E(1.602176634e-16, "keV", "chiloelettronvolt", ENER, 0),
    E(1.602176634e-13, "MeV", "megaelettronvolt", ENER, 0),
    E(1.602176634e-10, "GeV", "gigaelettronvolt", ENER, 0),
    E(1.602176634e-7, "TeV", "teraelettronvolt", ENER, 0),
    E(1, "J", "joule", ENER, FX | PSI),
    E(1e3, "kJ", "chilojoule", ENER, 0),
    E(1e6, "MJ", "megajoule", ENER, 0),
    E(1e9, "GJ", "gigajoule", ENER, 0),
    E(4.184, "calth", "caloria termochimica", ENER, FX),
    M(4.1855, "cal15", "caloria a 15 \xC2\xB0" "C", ENER, FX, NIST),          // NIST SP 811 B.8: 4,185 80
    E(4.1868, "calIT", "caloria internazionale", ENER, FX),
    E(4184, "kcalth", "chilocaloria termochimica", ENER, FX),
    M(4185.5, "kcal15", "chilocaloria a 15 \xC2\xB0" "C", ENER, FX, NIST),
    E(4186.8, "kcalIT", "chilocaloria internazionale", ENER, FX),
    E(4184, "Cal", "caloria alimentare (kcal)", ENER, 0),
    E(101.325, "l-atm", "litro-atmosfera", ENER, FX),
    E(3600, "Wh", "wattora", ENER, PSI),
    E(3.6e6, "kW" DOT "h", "chilowattora", ENER, FX),
    E(3.6e6, "kWh", "chilowattora", ENER, 0),
    E(3.6e9, "MWh", "megawattora", ENER, 0),
    E(3.6e12, "GWh", "gigawattora", ENER, 0),
    M(1.3558179483314, "ft" DOT "lbf", "piede-libbra forza", ENER, FX, FXV),  // esatto: 1,3558179483314004
    E(1055.05585262, "Btu", "Btu (IT)", ENER, FX),
    X(4.184 * 453.59237 * 5 / 9, "Btu(th)", "Btu termochimico", "4.184*453.59237*5/9", ENER, 0),
    E(105480400, "thm(US)", "therm US", ENER, 0),
    E(105506000, "thm(EC)", "therm EC", ENER, 0),
    E(1.05505585262e18, "quad", "quad (1E15 Btu)", ENER, 0),
    E(1e-7, "erg", "erg", ENER, FX),
    E(9.80665, "kgf" DOT "m", "chilogrammetro", ENER, FX),
    M(4.184e9, "tTNT", "tonnellata di TNT", ENER, 0, CONV),
    M(4.184e12, "ktTNT", "chilotone di TNT", ENER, 0, CONV),
    M(4.1868e10, "tep", "tonnellata equivalente di petrolio", ENER, 0, CONV),   // IEA: 41,868 GJ
    // ------------------------------------------------------------ POWER (fx: W calth/s hp ft•lbf/s Btu/min)
    E(1e-7, "erg/s", "erg al secondo", POW, 0),
    E(1e-3, "mW", "milliwatt", POW, 0),
    E(1, "W", "watt", POW, FX | PSI),
    E(1e3, "kW", "chilowatt", POW, 0),
    E(1e6, "MW", "megawatt", POW, 0),
    E(1e9, "GW", "gigawatt", POW, 0),
    E(4.184, "calth/s", "caloria termochimica al secondo", POW, FX),
    E(1.163, "kcal/h", "chilocaloria IT all'ora", POW, 0),
    M(745.69987158227, "hp", "cavallo vapore britannico", POW, FX, FXV),     // esatto: 745,69987158227022
    E(735.49875, "CV", "cavallo vapore metrico (CV, PS)", POW, 0),
    E(746, "hp(E)", "cavallo elettrico", POW, 0),
    M(9809.5, "hp(S)", "cavallo da caldaia", POW, 0, NIST),                   // NIST SP 811 B.8: 9,809 50 E+03
    M(1.3558179483314, "ft" DOT "lbf/s", "piede-libbra forza al secondo", POW, FX, FXV),
    X(kBtu / 3600, "Btu/h", "Btu all'ora", "1055.05585262/3600", POW, 0),
    M(17.584264210, "Btu/min", "Btu al minuto", POW, FX, FXV),
    X(12000 * kBtu / 3600, "TR", "tonnellata di refrigerazione", "12000*1055.05585262/3600", POW, 0),
    F(1e-3, "dBm", "dBm (rif. 1 mW)", POW, 0, F_LEV10, DEF),
    F(1, "dBW", "dBW (rif. 1 W)", POW, 0, F_LEV10, DEF),
    // ------------------------------------------------------------ intervalli di temperatura
    E(1, DELTA "K", "intervallo in kelvin", DTMP, 0),
    E(1, DELTA DEG "C", "intervallo in gradi Celsius", DTMP, 0),
    X(5.0 / 9.0, DELTA DEG "F", "intervallo in gradi Fahrenheit", "5/9", DTMP, 0),
    X(5.0 / 9.0, DELTA DEG "R", "intervallo in gradi Rankine", "5/9", DTMP, 0),
    X(5.0 / 4.0, DELTA DEG "R\xC3\xA9", "intervallo in gradi Reaumur", "5/4", DTMP, 0),
    X(40.0 / 21.0, DELTA DEG "R\xC3\xB8", "intervallo in gradi Romer", "40/21", DTMP, 0),
    X(100.0 / 33.0, DELTA DEG "N", "intervallo in gradi Newton", "100/33", DTMP, 0),
    // ------------------------------------------------------------ accelerazione
    E(1, "m/s" SQ, "metri al secondo quadrato", ACC, 0),
    E(1e-2, "Gal", "gal (cm/s\xC2\xB2)", ACC, 0),
    E(1e-5, "mGal", "milligal", ACC, 0),
    E(0.3048, "ft/s" SQ, "piedi al secondo quadrato", ACC, 0),
    E(0.0254, "in/s" SQ, "pollici al secondo quadrato", ACC, 0),
    E(kG0, "g0", "gravita' standard", ACC, 0),
    X(1 / 3.6, "km/h/s", "km/h al secondo", "1/3.6", ACC, 0),
    E(0.44704, "mph/s", "miglia orarie al secondo", ACC, 0),
    X(1852.0 / 3600, "knot/s", "nodi al secondo", "1852/3600", ACC, 0),
    // ------------------------------------------------------------ coppia
    E(1e-7, "dyn" DOT "cm", "dina-centimetro", TORQ, 0),
    E(1e-3, "N" DOT "mm", "newton-millimetro", TORQ, 0),
    E(1e-2, "N" DOT "cm", "newton-centimetro", TORQ, 0),
    E(1, "N" DOT "m", "newton-metro", TORQ, 0),
    E(1e3, "kN" DOT "m", "chilonewton-metro", TORQ, 0),
    E(0.0980665, "kgf" DOT "cm", "chilogrammo-forza-centimetro", TORQ, 0),
    E(9.80665, "kgf" DOT "m", "chilogrammo-forza-metro", TORQ, 0),
    X(kLbf * kIn / 16, "ozf" DOT "in", "oncia-forza-pollice", "4.4482216152605*0.0254/16", TORQ, 0),
    X(kLbf * kIn, "lbf" DOT "in", "libbra-forza-pollice", "4.4482216152605*0.0254", TORQ, 0),
    X(kLbf * kFt, "lbf" DOT "ft", "libbra-forza-piede", "4.4482216152605*0.3048", TORQ, 0),
    // ------------------------------------------------------------ angolo piano (SI: rad)
    X(kPi / 648000000, "mas", "millisecondo d'arco", "pi/648000000", ANG, 0),
    X(kPi / 648000, "arcsec", "secondo d'arco", "pi/648000", ANG, 0),
    X(kPi / 10800, "arcmin", "minuto d'arco", "pi/10800", ANG, 0),
    E(1e-3, "mrad", "milliradiante", ANG, 0),
    X(kPi / 3200, "mil(NATO)", "mil NATO (1/6400 di giro)", "pi/3200", ANG, 0),
    X(kPi / 3000, "mil(URSS)", "mil sovietico (1/6000 di giro)", "pi/3000", ANG, 0),
    X(kPi / 200, "gon", "gradiante (gon)", "pi/200", ANG, 0),
    X(kPi / 180, "deg", "grado sessagesimale", "pi/180", ANG, 0),
    X(kPi / 16, "point", "punto della bussola (1/32 di giro)", "pi/16", ANG, 0),
    X(kPi / 12, "h(ang)", "ora angolare (15\xC2\xB0)", "pi/12", ANG, 0),
    E(1, "rad", "radiante", ANG, PSI),
    X(kPi / 3, "sestante", "sestante (60\xC2\xB0)", "pi/3", ANG, 0),
    X(kPi / 2, "quadrante", "quadrante (90\xC2\xB0)", "pi/2", ANG, 0),
    X(2 * kPi, "giro", "giro completo", "2*pi", ANG, 0),
    // ------------------------------------------------------------ angolo solido
    X((kPi / 180) * (kPi / 180), "deg" SQ, "grado quadrato", "pi*pi/32400", SANG, 0),
    E(1, "sr", "steradiante", SANG, PSI),
    X(2 * kPi, "emisfero", "emisfero (2\xCF\x80 sr)", "2*pi", SANG, 0),
    X(4 * kPi, "sfera", "sfera (4\xCF\x80 sr)", "4*pi", SANG, 0),
    // ------------------------------------------------------------ frequenza
    X(1.0 / 60, "bpm", "battiti al minuto", "1/60", FREQ, 0),
    X(1.0 / 60, "rpm", "giri al minuto", "1/60", FREQ, 0),
    X(1.0 / 360, "deg/s", "gradi al secondo", "1/360", FREQ, 0),
    X(1 / (2 * kPi), "rad/s", "radianti al secondo (\xCF\x89)", "1/2/pi", FREQ, 0),
    E(1, "Hz", "hertz", FREQ, PSI),
    E(1, "rps", "giri al secondo", FREQ, 0),
    E(1e3, "kHz", "chilohertz", FREQ, 0),
    E(1e6, "MHz", "megahertz", FREQ, 0),
    E(1e9, "GHz", "gigahertz", FREQ, 0),
    E(1e12, "THz", "terahertz", FREQ, 0),
    // ------------------------------------------------------------ densita'
    E(1, "kg/m" CU, "chilogrammi al m\xC2\xB3", DENS, 0),
    E(1, "g/L", "grammi al litro", DENS, 0),
    E(1e3, "g/cm" CU, "grammi al cm\xC2\xB3", DENS, 0),
    E(1e3, "g/mL", "grammi al millilitro", DENS, 0),
    E(1e3, "kg/L", "chilogrammi al litro", DENS, 0),
    E(1e3, "t/m" CU, "tonnellate al m\xC2\xB3", DENS, 0),
    X(kLb / kFt3, "lb/ft" CU, "libbre al piede\xC2\xB3", "0.45359237/0.028316846592", DENS, 0),
    X(kLb / kIn3, "lb/in" CU, "libbre al pollice\xC2\xB3", "0.45359237/0.000016387064", DENS, 0),
    X(kLb / kGalUS, "lb/gal(US)", "libbre al gallone US", "0.45359237/0.003785411784", DENS, 0),
    X(kLb / kGalUK, "lb/gal(UK)", "libbre al gallone UK", "0.45359237/0.00454609", DENS, 0),
    X(kOz / kIn3, "oz/in" CU, "once al pollice\xC2\xB3", "0.028349523125/0.000016387064", DENS, 0),
    X(kLb * kG0 / kFt / kFt3, "slug/ft" CU, "slug al piede\xC2\xB3", "0.45359237*9.80665/0.3048/0.028316846592", DENS, 0),
    // ------------------------------------------------------------ portata volumetrica
    X(1e-6 / 60, "mL/min", "millilitri al minuto", "0.000001/60", VFLOW, 0),
    X(1e-3 / 3600, "L/h", "litri all'ora", "0.001/3600", VFLOW, 0),
    X(1e-3 / 60, "L/min", "litri al minuto", "0.001/60", VFLOW, 0),
    E(1e-3, "L/s", "litri al secondo", VFLOW, 0),
    X(1.0 / 86400, "m" CU "/d", "m\xC2\xB3 al giorno", "1/86400", VFLOW, 0),
    X(1.0 / 3600, "m" CU "/h", "m\xC2\xB3 all'ora", "1/3600", VFLOW, 0),
    E(1, "m" CU "/s", "m\xC2\xB3 al secondo", VFLOW, 0),
    X(kFt3 / 60, "cfm", "piedi\xC2\xB3 al minuto", "0.028316846592/60", VFLOW, 0),
    E(0.028316846592, "cfs", "piedi\xC2\xB3 al secondo", VFLOW, 0),
    X(kGalUS / 3600, "gph(US)", "galloni US all'ora", "0.003785411784/3600", VFLOW, 0),
    X(kGalUS / 60, "gpm(US)", "galloni US al minuto", "0.003785411784/60", VFLOW, 0),
    X(kGalUK / 60, "gpm(UK)", "galloni UK al minuto", "0.00454609/60", VFLOW, 0),
    X(kBbl / 86400, "bbl/d", "barili al giorno", "0.158987294928/86400", VFLOW, 0),
    // ------------------------------------------------------------ portata massica
    E(1e-3, "g/s", "grammi al secondo", MFLOW, 0),
    X(1.0 / 3600, "kg/h", "chilogrammi all'ora", "1/3600", MFLOW, 0),
    X(1.0 / 60, "kg/min", "chilogrammi al minuto", "1/60", MFLOW, 0),
    E(1, "kg/s", "chilogrammi al secondo", MFLOW, 0),
    X(1000.0 / 3600, "t/h", "tonnellate all'ora", "1000/3600", MFLOW, 0),
    X(kLb / 3600, "lb/h", "libbre all'ora", "0.45359237/3600", MFLOW, 0),
    E(0.45359237, "lb/s", "libbre al secondo", MFLOW, 0),
    // ------------------------------------------------------------ consumo di carburante (SI: m/m³)
    I(1e8, "L/100km", "litri per 100 km", FUEL, 0, DEF),
    E(1e6, "km/L", "km al litro", FUEL, 0),
    X(kMile / kGalUS, "mpg(US)", "miglia per gallone US", "1609.344/0.003785411784", FUEL, 0),
    X(kMile / kGalUK, "mpg(UK)", "miglia per gallone UK", "1609.344/0.00454609", FUEL, 0),
    X(kMile / 1e-3, "mi/L", "miglia al litro", "1609344", FUEL, 0),
    I(100 * kMile / kGalUS, "gal/100mi(US)", "galloni US per 100 miglia", FUEL, 0, DEF),
    I(100 * kMile / kGalUK, "gal/100mi(UK)", "galloni UK per 100 miglia", FUEL, 0, DEF),
    // ------------------------------------------------------------ dati digitali (bit)
    E(1, "bit", "bit", DATA, PSI | PIEC),
    E(4, "nibble", "nibble (4 bit)", DATA, 0),
    E(8, "B", "byte (8 bit)", DATA, PSI | PIEC),
    E(1e3, "kbit", "kilobit", DATA, 0),
    E(1024, "Kibit", "kibibit", DATA, 0),
    E(8e3, "kB", "kilobyte", DATA, 0),
    E(8192, "KiB", "kibibyte", DATA, 0),
    E(1e6, "Mbit", "megabit", DATA, 0),
    E(1048576, "Mibit", "mebibit", DATA, 0),
    E(8e6, "MB", "megabyte", DATA, 0),
    E(8388608, "MiB", "mebibyte", DATA, 0),
    E(1e9, "Gbit", "gigabit", DATA, 0),
    E(1073741824, "Gibit", "gibibit", DATA, 0),
    E(8e9, "GB", "gigabyte", DATA, 0),
    E(8589934592, "GiB", "gibibyte", DATA, 0),
    E(1e12, "Tbit", "terabit", DATA, 0),
    E(8e12, "TB", "terabyte", DATA, 0),
    E(8796093022208, "TiB", "tebibyte", DATA, 0),
    E(8e15, "PB", "petabyte", DATA, 0),
    X(9007199254740992.0, "PiB", "pebibyte", "9007199254740992", DATA, 0),
    E(8e18, "EB", "exabyte", DATA, 0),
    M(9223372036854775808.0, "EiB", "exbibyte (2^63 bit)", DATA, 0, DEF),
    // ------------------------------------------------------------ velocita' di trasmissione (bit/s)
    E(1, "bit/s", "bit al secondo", RATE, PSI | PIEC),
    E(1e3, "kbit/s", "kilobit al secondo", RATE, 0),
    E(1e6, "Mbit/s", "megabit al secondo", RATE, 0),
    E(1e9, "Gbit/s", "gigabit al secondo", RATE, 0),
    E(1e12, "Tbit/s", "terabit al secondo", RATE, 0),
    E(8, "B/s", "byte al secondo", RATE, PSI | PIEC),
    E(8e3, "kB/s", "kilobyte al secondo", RATE, 0),
    E(8192, "KiB/s", "kibibyte al secondo", RATE, 0),
    E(8e6, "MB/s", "megabyte al secondo", RATE, 0),
    E(8388608, "MiB/s", "mebibyte al secondo", RATE, 0),
    E(8e9, "GB/s", "gigabyte al secondo", RATE, 0),
    E(8589934592, "GiB/s", "gibibyte al secondo", RATE, 0),
    // ------------------------------------------------------------ carica
    E(1.602176634e-19, "e", "carica elementare", CHG, 0),
    X(1 / 2997924580.0, "statC", "statcoulomb (franklin)", "1/2997924580", CHG, 0),
    E(1e-12, "pC", "picocoulomb", CHG, 0),
    E(1e-9, "nC", "nanocoulomb", CHG, 0),
    E(1e-6, MU "C", "microcoulomb", CHG, 0),
    E(1e-3, "mC", "millicoulomb", CHG, 0),
    E(1, "C", "coulomb", CHG, PSI),
    E(3.6, "mAh", "milliamperora", CHG, 0),
    E(10, "abC", "abcoulomb", CHG, 0),
    E(3600, "Ah", "amperora", CHG, 0),
    M(kE * kNA, "faraday", "faraday (e\xC2\xB7N_A)", CHG, 0, DEF),
    // ------------------------------------------------------------ corrente
    X(1 / 2997924580.0, "statA", "statampere", "1/2997924580", CUR, 0),
    E(1e-9, "nA", "nanoampere", CUR, 0),
    E(1e-6, MU "A", "microampere", CUR, 0),
    E(1e-3, "mA", "milliampere", CUR, 0),
    E(1, "A", "ampere", CUR, PSI),
    E(10, "abA", "abampere (biot)", CUR, 0),
    E(1e3, "kA", "chiloampere", CUR, 0),
    // ------------------------------------------------------------ tensione
    E(1e-8, "abV", "abvolt", VOLT, 0),
    E(1e-6, MU "V", "microvolt", VOLT, 0),
    E(1e-3, "mV", "millivolt", VOLT, 0),
    E(1, "V", "volt", VOLT, PSI),
    E(299.792458, "statV", "statvolt", VOLT, 0),
    E(1e3, "kV", "chilovolt", VOLT, 0),
    E(1e6, "MV", "megavolt", VOLT, 0),
    // ------------------------------------------------------------ resistenza
    E(1e-9, "ab" OHM, "abohm", RES, 0),
    E(1e-3, "m" OHM, "milliohm", RES, 0),
    E(1, OHM, "ohm", RES, PSI),
    E(1e3, "k" OHM, "chiloohm", RES, 0),
    E(1e6, "M" OHM, "megaohm", RES, 0),
    E(1e9, "G" OHM, "gigaohm", RES, 0),
    X(898755178736.8176, "stat" OHM, "statohm", "898755178736.8176", RES, 0),
    // ------------------------------------------------------------ conduttanza
    E(1e-9, "nS", "nanosiemens", COND, 0),
    E(1e-6, MU "S", "microsiemens", COND, 0),
    E(1e-3, "mS", "millisiemens", COND, 0),
    E(1, "S", "siemens", COND, PSI),
    E(1, "mho", "mho", COND, 0),
    // ------------------------------------------------------------ capacita'
    X(1 / 898755178736.8176, "statF", "statfarad", "1/898755178736.8176", CAP, 0),
    E(1e-12, "pF", "picofarad", CAP, 0),
    E(1e-9, "nF", "nanofarad", CAP, 0),
    E(1e-6, MU "F", "microfarad", CAP, 0),
    E(1e-3, "mF", "millifarad", CAP, 0),
    E(1, "F", "farad", CAP, PSI),
    E(1e9, "abF", "abfarad", CAP, 0),
    // ------------------------------------------------------------ induttanza
    E(1e-9, "abH", "abhenry", IND, 0),
    E(1e-9, "nH", "nanohenry", IND, 0),
    E(1e-6, MU "H", "microhenry", IND, 0),
    E(1e-3, "mH", "millihenry", IND, 0),
    E(1, "H", "henry", IND, PSI),
    X(898755178736.8176, "statH", "stathenry", "898755178736.8176", IND, 0),
    // ------------------------------------------------------------ resistivita'
    X(kCmil / kFt, OHM DOT "cmil/ft", "ohm-circular mil al piede", "pi*0.0000254*0.0000254/4/0.3048", RESV, 0),
    E(1e-8, MU OHM DOT "cm", "microohm-centimetro", RESV, 0),
    E(1e-6, OHM DOT "mm" SQ "/m", "ohm\xC2\xB7mm\xC2\xB2/m", RESV, 0),
    E(1e-2, OHM DOT "cm", "ohm-centimetro", RESV, 0),
    E(1, OHM DOT "m", "ohm-metro", RESV, 0),
    // ------------------------------------------------------------ conducibilita'
    E(1e-4, MU "S/cm", "microsiemens al cm", CONDV, 0),
    E(1e-3, "mS/m", "millisiemens al metro", CONDV, 0),
    E(0.1, "mS/cm", "millisiemens al cm", CONDV, 0),
    E(1, "S/m", "siemens al metro", CONDV, 0),
    E(100, "S/cm", "siemens al cm", CONDV, 0),
    // ------------------------------------------------------------ flusso magnetico
    E(1e-8, "Mx", "maxwell", MFLUX, 0),
    E(1e-6, MU "Wb", "microweber", MFLUX, 0),
    E(1e-3, "mWb", "milliweber", MFLUX, 0),
    E(1, "Wb", "weber", MFLUX, PSI),
    E(1, "V" DOT "s", "volt-secondo", MFLUX, 0),
    // ------------------------------------------------------------ induzione magnetica
    E(1e-9, "\xCE\xB3", "gamma", MDENS, 0),
    E(1e-9, "nT", "nanotesla", MDENS, 0),
    E(1e-7, "mG", "milligauss", MDENS, 0),
    E(1e-6, MU "T", "microtesla", MDENS, 0),
    E(1e-4, "G", "gauss", MDENS, 0),
    E(1e-3, "mT", "millitesla", MDENS, 0),
    E(1, "T", "tesla", MDENS, PSI),
    // ------------------------------------------------------------ campo H
    E(1, "A/m", "ampere al metro", HFIELD, 0),
    X(1000 / (4 * kPi), "Oe", "oersted", "1000/4/pi", HFIELD, 0),
    E(1e3, "kA/m", "chiloampere al metro", HFIELD, 0),
    X(1e6 / (4 * kPi), "kOe", "chilooersted", "1000000/4/pi", HFIELD, 0),
    // ------------------------------------------------------------ fotometria
    E(1e-3, "mcd", "millicandela", LINT, 0),
    E(1, "cd", "candela", LINT, PSI),
    E(1e3, "kcd", "chilocandela", LINT, 0),
    E(1e-3, "mlm", "millilumen", LFLUX, 0),
    E(1, "lm", "lumen", LFLUX, PSI),
    E(1e3, "klm", "chilolumen", LFLUX, 0),
    E(1e-3, "nox", "nox", ILLUM, 0),
    E(1, "lx", "lux", ILLUM, PSI),
    X(1 / kFt2, "fc", "foot-candle (lm/ft\xC2\xB2)", "1/0.09290304", ILLUM, 0),
    E(1e3, "klx", "chilolux", ILLUM, 0),
    E(1e4, "ph", "phot", ILLUM, 0),
    X(1 / kPi, "asb", "apostilb", "1/pi", LUM, 0),
    X(1 / kFt2 / kPi, "fL", "foot-lambert", "1/0.09290304/pi", LUM, 0),
    E(1, "cd/m" SQ, "candele al m\xC2\xB2", LUM, 0),
    E(1, "nit", "nit", LUM, 0),
    E(1e3, "kcd/m" SQ, "chilocandele al m\xC2\xB2", LUM, 0),
    X(1e4 / kPi, "lambert", "lambert", "10000/pi", LUM, 0),
    E(1e4, "sb", "stilb", LUM, 0),
    // ------------------------------------------------------------ radiazioni
    X(1.0 / 60, "dpm", "disintegrazioni al minuto", "1/60", ACT, 0),
    E(1, "Bq", "becquerel", ACT, PSI),
    E(1e3, "kBq", "chilobecquerel", ACT, 0),
    E(1e6, "MBq", "megabecquerel", ACT, 0),
    E(1e6, "Rd", "rutherford", ACT, 0),
    E(1e9, "GBq", "gigabecquerel", ACT, 0),
    E(1e12, "TBq", "terabecquerel", ACT, 0),
    E(37, "nCi", "nanocurie", ACT, 0),
    E(3.7e4, MU "Ci", "microcurie", ACT, 0),
    E(3.7e7, "mCi", "millicurie", ACT, 0),
    E(3.7e10, "Ci", "curie", ACT, 0),
    E(1e-6, MU "Gy", "microgray", DOSE, 0),
    E(1e-4, "erg/g", "erg al grammo", DOSE, 0),
    E(1e-3, "mGy", "milligray", DOSE, 0),
    E(1e-2, "rad", "rad", DOSE, 0),
    E(1, "Gy", "gray", DOSE, PSI),
    E(1e-6, MU "Sv", "microsievert", DEQ, 0),
    E(1e-5, "mrem", "millirem", DEQ, 0),
    E(1e-3, "mSv", "millisievert", DEQ, 0),
    E(1e-2, "rem", "rem", DEQ, 0),
    E(1, "Sv", "sievert", DEQ, PSI),
    E(2.58e-7, "mR", "milliroentgen", EXPO, 0),
    E(2.58e-4, "R", "roentgen", EXPO, 0),
    E(1e-3, "mC/kg", "millicoulomb al kg", EXPO, 0),
    E(1, "C/kg", "coulomb al kg", EXPO, 0),
    // ------------------------------------------------------------ chimica
    M(1 / kNA, "part.", "numero di particelle (N/N_A)", AMT, 0, DEF),
    E(1e-9, "nmol", "nanomole", AMT, 0),
    E(1e-6, MU "mol", "micromole", AMT, 0),
    E(1e-3, "mmol", "millimole", AMT, 0),
    E(1, "mol", "mole", AMT, PSI),
    E(1e3, "kmol", "chilomole", AMT, 0),
    E(1e-6, "nM", "nanomolare (nmol/L)", MOLC, 0),
    E(1e-3, MU "M", "micromolare (\xC2\xB5mol/L)", MOLC, 0),
    E(1e-3, MU "mol/L", "micromoli al litro", MOLC, 0),
    E(1, "mol/m" CU, "moli al m\xC2\xB3", MOLC, 0),
    E(1, "mM", "millimolare (mmol/L)", MOLC, 0),
    E(1, "mmol/L", "millimoli al litro", MOLC, 0),
    E(1e3, "M", "molare (mol/L)", MOLC, 0),
    E(1e3, "mol/L", "moli al litro", MOLC, 0),
    E(1e-6, "ng/mL", "nanogrammi al mL", MASC, 0),
    E(1e-6, MU "g/L", "microgrammi al litro", MASC, 0),
    E(1e-3, "mg/L", "milligrammi al litro", MASC, 0),
    E(1e-3, MU "g/mL", "microgrammi al mL", MASC, 0),
    E(1e-2, "mg/dL", "milligrammi al decilitro", MASC, 0),
    E(1, "g/L", "grammi al litro", MASC, 0),
    E(1, "mg/mL", "milligrammi al mL", MASC, 0),
    E(1, "kg/m" CU, "chilogrammi al m\xC2\xB3", MASC, 0),
    E(10, "g/dL", "grammi al decilitro", MASC, 0),
    E(1e-15, "ppq", "parti per biliardo", RATIO, 0),
    E(1e-12, "ppt", "parti per trilione", RATIO, 0),
    E(1e-9, "ppb", "parti per miliardo", RATIO, 0),
    E(1e-6, "ppm", "parti per milione", RATIO, 0),
    E(1e-5, "pcm", "per centomila", RATIO, 0),
    E(1e-4, "bp", "punto base (\xE2\x80\xB1)", RATIO, 0),
    E(1e-3, "\xE2\x80\xB0", "per mille", RATIO, 0),
    E(1e-2, "%", "percento", RATIO, 0),
    E(1, "x", "rapporto (1)", RATIO, 0),
    // ------------------------------------------------------------ viscosita'
    E(1e-3, "cP", "centipoise", VISD, 0),
    E(1e-3, "mPa" DOT "s", "millipascal-secondo", VISD, 0),
    E(0.1, "P", "poise", VISD, 0),
    E(1, "Pa" DOT "s", "pascal-secondo", VISD, 0),
    X(kLb / kFt, "lb/(ft" DOT "s)", "libbre al piede-secondo", "0.45359237/0.3048", VISD, 0),
    E(9.80665, "kgf" DOT "s/m" SQ, "kgf\xC2\xB7s/m\xC2\xB2", VISD, 0),
    X(kLbf / kFt2, "lbf" DOT "s/ft" SQ, "lbf\xC2\xB7s/ft\xC2\xB2", "4.4482216152605/0.09290304", VISD, 0),
    X(kLbf / kIn2, "reyn", "reyn (lbf\xC2\xB7s/in\xC2\xB2)", "4.4482216152605/0.00064516", VISD, 0),
    E(1e-6, "cSt", "centistokes", VISK, 0),
    E(1e-6, "mm" SQ "/s", "mm\xC2\xB2 al secondo", VISK, 0),
    E(1e-4, "St", "stokes", VISK, 0),
    E(1e-4, "cm" SQ "/s", "cm\xC2\xB2 al secondo", VISK, 0),
    X(1.0 / 3600, "m" SQ "/h", "m\xC2\xB2 all'ora", "1/3600", VISK, 0),
    E(1, "m" SQ "/s", "m\xC2\xB2 al secondo", VISK, 0),
    E(6.4516e-4, "in" SQ "/s", "pollici\xC2\xB2 al secondo", VISK, 0),
    E(0.09290304, "ft" SQ "/s", "piedi\xC2\xB2 al secondo", VISK, 0),
    E(1e-3, "dyn/cm", "dine al centimetro", SURF, 0),
    E(1e-3, "erg/cm" SQ, "erg al cm\xC2\xB2", SURF, 0),
    E(1e-3, "mN/m", "millinewton al metro", SURF, 0),
    E(1, "N/m", "newton al metro", SURF, 0),
    X(kLbf / kFt, "lbf/ft", "libbre-forza al piede", "4.4482216152605/0.3048", SURF, 0),
    X(kLbf / kIn, "lbf/in", "libbre-forza al pollice", "4.4482216152605/0.0254", SURF, 0),
    // ------------------------------------------------------------ termica
    E(1e-3, "mW/(m" DOT "K)", "milliwatt al metro-kelvin", TCOND, 0),
    E(1.163, "kcal/(h" DOT "m" DOT DEG "C)", "kcal IT/(h\xC2\xB7m\xC2\xB7\xC2\xB0" "C)", TCOND, 0),
    X(kBtu / 3600 / kFt * 9 / 5, "Btu/(h" DOT "ft" DOT DEG "F)", "Btu IT/(h\xC2\xB7" "ft\xC2\xB7\xC2\xB0" "F)", "1055.05585262/3600/0.3048*9/5", TCOND, 0),
    E(1, "W/(m" DOT "K)", "watt al metro-kelvin", TCOND, 0),
    E(100, "W/(cm" DOT "K)", "watt al cm-kelvin", TCOND, 0),
    E(418.68, "cal/(s" DOT "cm" DOT DEG "C)", "cal IT/(s\xC2\xB7" "cm\xC2\xB7\xC2\xB0" "C)", TCOND, 0),
    E(1.163, "kcal/(h" DOT "m" SQ DOT DEG "C)", "kcal IT/(h\xC2\xB7m\xC2\xB2\xC2\xB7\xC2\xB0" "C)", HTC, 0),
    E(1, "W/(m" SQ DOT "K)", "watt al m\xC2\xB2-kelvin", HTC, 0),
    X(kBtu / 3600 / kFt2 * 9 / 5, "Btu/(h" DOT "ft" SQ DOT DEG "F)", "Btu IT/(h\xC2\xB7" "ft\xC2\xB2\xC2\xB7\xC2\xB0" "F)", "1055.05585262/3600/0.09290304*9/5", HTC, 0),
    E(1e3, "kW/(m" SQ DOT "K)", "chilowatt al m\xC2\xB2-kelvin", HTC, 0),
    E(41868, "cal/(s" DOT "cm" SQ DOT DEG "C)", "cal IT/(s\xC2\xB7" "cm\xC2\xB2\xC2\xB7\xC2\xB0" "C)", HTC, 0),
    E(1, "J/(kg" DOT "K)", "joule al kg-kelvin", CP, 0),
    E(1e3, "J/(g" DOT "K)", "joule al grammo-kelvin", CP, 0),
    E(1e3, "kJ/(kg" DOT "K)", "chilojoule al kg-kelvin", CP, 0),
    E(4186.8, "cal/(g" DOT DEG "C)", "cal IT/(g\xC2\xB7\xC2\xB0" "C)", CP, 0),
    E(4186.8, "kcal/(kg" DOT DEG "C)", "kcal IT/(kg\xC2\xB7\xC2\xB0" "C)", CP, 0),
    E(4186.8, "Btu/(lb" DOT DEG "F)", "Btu IT/(lb\xC2\xB7\xC2\xB0" "F)", CP, 0),
    E(1.163, "kcal/(h" DOT "m" SQ ")", "kcal IT/(h\xC2\xB7m\xC2\xB2)", HFLUX, 0),
    E(1, "W/m" SQ, "watt al m\xC2\xB2", HFLUX, 0),
    X(kBtu / 3600 / kFt2, "Btu/(h" DOT "ft" SQ ")", "Btu IT/(h\xC2\xB7" "ft\xC2\xB2)", "1055.05585262/3600/0.09290304", HFLUX, 0),
    E(10, "mW/cm" SQ, "milliwatt al cm\xC2\xB2", HFLUX, 0),
    E(1e3, "kW/m" SQ, "chilowatt al m\xC2\xB2", HFLUX, 0),
    E(1e4, "W/cm" SQ, "watt al cm\xC2\xB2", HFLUX, 0),
    E(41868, "cal/(s" DOT "cm" SQ ")", "cal IT/(s\xC2\xB7" "cm\xC2\xB2)", HFLUX, 0),
    E(1.380649e-23, "k", "costante di Boltzmann k", ENTR, 0),
    E(1, "J/K", "joule al kelvin", ENTR, 0),
    E(4.1868, "cal/K", "caloria IT al kelvin", ENTR, 0),
    E(1e3, "kJ/K", "chilojoule al kelvin", ENTR, 0),
    X(kBtu * 9 / 5, "Btu/" DEG "F", "Btu IT al grado F", "1055.05585262*9/5", ENTR, 0),
    E(4186.8, "kcal/K", "chilocaloria IT al kelvin", ENTR, 0),
    E(1, "K/W", "kelvin al watt", TRES, 0),
    E(1, DEG "C/W", "grado C al watt", TRES, 0),
    X(5.0 / 9 * 3600 / kBtu, DEG "F" DOT "h/Btu", "\xC2\xB0" "F\xC2\xB7h/Btu IT", "5/9*3600/1055.05585262", TRES, 0),
    E(0.1, "tog", "tog", RVAL, 0),
    M(0.155, "clo", "clo (vestiario)", RVAL, 0, CONV),                        // ISO 9920: 1 clo = 0,155 m²·K/W
    X(kFt2 * 5 / 9 * 3600 / kBtu, "ft" SQ DOT DEG "F" DOT "h/Btu", "valore R US", "0.09290304*5/9*3600/1055.05585262", RVAL, 0),
    E(1, "m" SQ DOT "K/W", "valore R SI (RSI)", RVAL, 0),
    // ------------------------------------------------------------ meccanica
    E(1e-5, "g" DOT "cm/s", "grammo-centimetro al secondo", MOM, 0),
    E(1e-5, "dyn" DOT "s", "dina-secondo", MOM, 0),
    X(kLb * kFt, "lb" DOT "ft/s", "libbra-piede al secondo", "0.45359237*0.3048", MOM, 0),
    E(1, "kg" DOT "m/s", "chilogrammo-metro al secondo", MOM, 0),
    E(1, "N" DOT "s", "newton-secondo (impulso)", MOM, 0),
    E(4.4482216152605, "lbf" DOT "s", "libbra-forza-secondo", MOM, 0),
    M(kH / (2 * kPi), "\xC4\xA7", "costante di Planck ridotta", ANGM, 0, DEF),
    E(1e-7, "g" DOT "cm" SQ "/s", "g\xC2\xB7" "cm\xC2\xB2/s", ANGM, 0),
    X(kLb * kFt2, "lb" DOT "ft" SQ "/s", "lb\xC2\xB7" "ft\xC2\xB2/s", "0.45359237*0.09290304", ANGM, 0),
    E(1, "kg" DOT "m" SQ "/s", "kg\xC2\xB7m\xC2\xB2/s", ANGM, 0),
    E(1, "J" DOT "s", "joule-secondo", ANGM, 0),
    E(1e-7, "g" DOT "cm" SQ, "grammo-cm\xC2\xB2", INER, 0),
    X(kLb * kIn2, "lb" DOT "in" SQ, "libbra-pollice\xC2\xB2", "0.45359237*0.00064516", INER, 0),
    X(kLb * kFt2, "lb" DOT "ft" SQ, "libbra-piede\xC2\xB2", "0.45359237*0.09290304", INER, 0),
    E(1, "kg" DOT "m" SQ, "chilogrammo-metro\xC2\xB2", INER, 0),
    X(kLbf * kFt, "slug" DOT "ft" SQ, "slug-piede\xC2\xB2", "4.4482216152605*0.3048", INER, 0),
    X(kLbf / kFt, "lbf/ft", "libbre-forza al piede", "4.4482216152605/0.3048", STIFF, 0),
    E(1, "N/m", "newton al metro", STIFF, 0),
    X(kLbf / kIn, "lbf/in", "libbre-forza al pollice", "4.4482216152605/0.0254", STIFF, 0),
    E(980.665, "kgf/cm", "kgf al centimetro", STIFF, 0),
    E(1e3, "N/mm", "newton al millimetro", STIFF, 0),
    E(1e3, "kN/m", "chilonewton al metro", STIFF, 0),
    E(9806.65, "kgf/mm", "kgf al millimetro", STIFF, 0),
    X(kPi / 10800, "deg/min", "gradi al minuto", "pi/10800", ANGV, 0),
    X(1.0 / 60, "rad/min", "radianti al minuto", "1/60", ANGV, 0),
    X(kPi / 180, "deg/s", "gradi al secondo", "pi/180", ANGV, 0),
    X(kPi / 30, "rpm", "giri al minuto", "pi/30", ANGV, 0),
    E(1, "rad/s", "radianti al secondo", ANGV, 0),
    X(2 * kPi, "rps", "giri al secondo", "2*pi", ANGV, 0),
    // ------------------------------------------------------------ logaritmiche
    E(1, "P/P0", "rapporto di potenza", PRAT, 0),
    F(0.1, "dB", "decibel (potenza)", PRAT, 0, F_POW10, DEF),
    F(1, "B", "bel", PRAT, 0, F_POW10, DEF),
    F(2, "Np", "neper (potenza)", PRAT, 0, F_EXP, DEF),
    E(1, "A/A0", "rapporto di ampiezza", ARAT, 0),
    F(0.05, "dB", "decibel (ampiezza)", ARAT, 0, F_POW10, DEF),
    F(1, "Np", "neper", ARAT, 0, F_EXP, DEF),
    E(1e-6, MU "Pa", "micropascal", SPL, 0),
    E(1e-3, "mPa", "millipascal", SPL, 0),
    E(1, "Pa", "pascal", SPL, 0),
    F(2e-5, "dB SPL", "livello di pressione sonora (rif. 20 \xC2\xB5Pa)", SPL, 0, F_LEV20, DEF),
    E(1e-6, MU "V", "microvolt", VLEV, 0),
    E(1e-3, "mV", "millivolt", VLEV, 0),
    E(1, "V", "volt", VLEV, 0),
    F(1, "dBV", "dBV (rif. 1 V)", VLEV, 0, F_LEV20, DEF),
    F(0.7745966692414834, "dBu", "dBu (rif. \xE2\x88\x9A" "0,6 V)", VLEV, 0, F_LEV20, DEF),
    F(1e-3, "dBmV", "dBmV (rif. 1 mV)", VLEV, 0, F_LEV20, DEF),
    F(1e-6, "dB" MU "V", "dB\xC2\xB5V (rif. 1 \xC2\xB5V)", VLEV, 0, F_LEV20, DEF),
    E(1, "f2/f1", "rapporto di frequenza", FRAT, 0),
    F(1, "ottava", "ottave", FRAT, 0, F_POW2, DEF),
    F(1.0 / 12, "semitono", "semitoni temperati", FRAT, 0, F_POW2, DEF),
    F(1.0 / 1200, "cent", "cent", FRAT, 0, F_POW2, DEF),
    F(1, "decade", "decadi", FRAT, 0, F_POW10, DEF),
    F(1e-3, "savart", "savart", FRAT, 0, F_POW10, DEF),
    E(1e-9, "nmol/L", "[H\xE2\x81\xBA] nmol/L", PH, 0),
    E(1e-6, MU "mol/L", "[H\xE2\x81\xBA] \xC2\xB5mol/L", PH, 0),
    E(1e-3, "mmol/L", "[H\xE2\x81\xBA] mmol/L", PH, 0),
    E(1, "mol/L", "[H\xE2\x81\xBA] mol/L", PH, 0),
    F(-1, "pH", "pH", PH, 0, F_POW10, DEF),
    F(0, "pOH", "pOH (25 \xC2\xB0" "C)", PH, 0, F_POH, DEF),
    I(1e-14, "[OH-]", "[OH\xE2\x81\xBB] mol/L (25 \xC2\xB0" "C)", PH, 0, DEF),
    E(2.54e-5, "mil", "diametro in mil", AWG, 0),
    E(1e-6, MU "m", "diametro in \xC2\xB5m", AWG, 0),
    E(1e-3, "mm", "diametro in mm", AWG, 0),
    E(0.0254, "in", "diametro in pollici", AWG, 0),
    E(1, "m", "diametro in metri", AWG, 0),
    F(0, "AWG", "calibro AWG (0000 = -3)", AWG, 0, F_AWG, DEF),
    F(1e-6, "mm" SQ, "sezione in mm\xC2\xB2", AWG, 0, F_AREA, DEF),
    F(kCmil, "cmil", "sezione in circular mil", AWG, 0, F_AREA, DEF),
    F(1000 * kCmil, "kcmil", "sezione in kcmil (MCM)", AWG, 0, F_AREA, DEF),
    E(1, "J", "joule", SEIS, 0),
    E(1e9, "GJ", "gigajoule", SEIS, 0),
    E(1e12, "TJ", "terajoule", SEIS, 0),
    E(1e15, "PJ", "petajoule", SEIS, 0),
    M(4.184e9, "tTNT", "tonnellate di TNT", SEIS, 0, CONV),
    M(4.184e12, "ktTNT", "chilotoni di TNT", SEIS, 0, CONV),
    M(4.184e15, "MtTNT", "megatoni di TNT", SEIS, 0, CONV),
    F(0, "Mw", "magnitudo (Richter / momento)", SEIS, APX, F_SEIS, APPROXF),
    E(1, "F/F0", "rapporto di flusso", STEL, 0),
    F(-0.4, "mag", "differenza di magnitudo \xCE\x94m", STEL, 0, F_POW10, DEF),
    // ------------------------------------------------------------ derivate: fotone (SI: Hz)
    E(1, "Hz", "frequenza (Hz)", PHOT, PSI),
    E(1e3, "kHz", "frequenza (kHz)", PHOT, 0),
    E(1e6, "MHz", "frequenza (MHz)", PHOT, 0),
    E(1e9, "GHz", "frequenza (GHz)", PHOT, 0),
    E(1e12, "THz", "frequenza (THz)", PHOT, 0),
    E(1e15, "PHz", "frequenza (PHz)", PHOT, 0),
    I(kC, "m", "lunghezza d'onda (m)", PHOT, 0, DEF),
    I(kC / 1e-6, MU "m", "lunghezza d'onda (\xC2\xB5m)", PHOT, 0, DEF),
    I(kC / 1e-9, "nm", "lunghezza d'onda (nm)", PHOT, 0, DEF),
    I(kC / 1e-10, "\xC3\x85", "lunghezza d'onda (\xC3\x85)", PHOT, 0, DEF),
    I(kC / 1e-12, "pm", "lunghezza d'onda (pm)", PHOT, 0, DEF),
    M(1 / kH, "J", "energia del fotone (J)", PHOT, 0, DEF),
    M(1e-3 * kE / kH, "meV", "energia del fotone (meV)", PHOT, 0, DEF),
    M(kE / kH, "eV", "energia del fotone (eV)", PHOT, 0, DEF),
    M(1e3 * kE / kH, "keV", "energia del fotone (keV)", PHOT, 0, DEF),
    M(1e3 / (kNA * kH), "kJ/mol", "energia molare (kJ/mol)", PHOT, 0, DEF),
    M(4184 / (kNA * kH), "kcal/mol", "energia molare (kcal/mol)", PHOT, 0, DEF),
    E(kC, "m\xE2\x81\xBB\xC2\xB9", "numero d'onda (m\xE2\x81\xBB\xC2\xB9)", PHOT, 0),
    E(100 * kC, "cm\xE2\x81\xBB\xC2\xB9", "numero d'onda (cm\xE2\x81\xBB\xC2\xB9)", PHOT, 0),
    // ------------------------------------------------------------ massa <-> energia (SI: J)
    E(1.602176634e-19, "eV", "elettronvolt", MEQ, 0),
    E(1.602176634e-13, "MeV", "megaelettronvolt", MEQ, 0),
    E(1.602176634e-10, "GeV", "gigaelettronvolt", MEQ, 0),
    E(1.602176634e-7, "TeV", "teraelettronvolt", MEQ, 0),
    E(1, "J", "joule", MEQ, 0),
    E(1e6, "MJ", "megajoule", MEQ, 0),
    E(3.6e6, "kWh", "chilowattora", MEQ, 0),
    M(4.184e12, "ktTNT", "chilotoni di TNT", MEQ, 0, CONV),
    M(4.184e15, "MtTNT", "megatoni di TNT", MEQ, 0, CONV),
    M(kU * kC * kC, "u", "massa: unita' atomica (u\xC2\xB7" "c\xC2\xB2)", MEQ, 0, CODATA),
    E(1.602176634e-13, "MeV/c" SQ, "massa: MeV/c\xC2\xB2", MEQ, 0),
    E(1.602176634e-10, "GeV/c" SQ, "massa: GeV/c\xC2\xB2", MEQ, 0),
    X(89875517.873681764, MU "g", "massa: microgrammi", "89875517.873681764", MEQ, 0),
    X(89875517873.681764, "mg", "massa: milligrammi", "89875517873.681764", MEQ, 0),
    X(89875517873681.764, "g", "massa: grammi", "89875517873681.764", MEQ, 0),
    X(89875517873681764.0, "kg", "massa: chilogrammi", "89875517873681764", MEQ, 0),
    // ------------------------------------------------------------ temperatura <-> energia (SI: J)
    M(kK, "K", "temperatura (K)", KT, 0, DEF),
    A(kK, DEG "C", "temperatura (\xC2\xB0" "C)", nullptr, KT, 0, 0, DEF),
    A(kK * 5 / 9, DEG "F", "temperatura (\xC2\xB0" "F)", nullptr, KT, 0, 1, DEF),
    E(1, "J", "energia (J)", KT, 0),
    E(1.602176634e-22, "meV", "energia (meV)", KT, 0),
    E(1.602176634e-19, "eV", "energia (eV)", KT, 0),
    M(1 / kNA, "J/mol", "energia molare (J/mol)", KT, 0, DEF),
    M(1e3 / kNA, "kJ/mol", "energia molare (kJ/mol)", KT, 0, DEF),
    M(4184 / kNA, "kcal/mol", "energia molare (kcal/mol)", KT, 0, DEF),
    M(kH * kC * 100, "cm\xE2\x81\xBB\xC2\xB9", "numero d'onda (cm\xE2\x81\xBB\xC2\xB9)", KT, 0, DEF),
    // ------------------------------------------------------------ moli <-> massa (SI: mol)
    M(1 / kNA, "part.", "numero di particelle", MOLM, 0, DEF),
    E(1e-6, MU "mol", "micromoli", MOLM, 0),
    E(1e-3, "mmol", "millimoli", MOLM, 0),
    E(1, "mol", "moli", MOLM, PSI),
    E(1e3, "kmol", "chilomoli", MOLM, 0),
    {1e-6, MU "g", "massa in \xC2\xB5g (con M)", nullptr, MOLM, PRM, ks(LI, DEF), 0},
    {1e-3, "mg", "massa in mg (con M)", nullptr, MOLM, PRM, ks(LI, DEF), 0},
    {1, "g", "massa in g (con M)", nullptr, MOLM, PRM, ks(LI, DEF), 0},
    {1e3, "kg", "massa in kg (con M)", nullptr, MOLM, PRM, ks(LI, DEF), 0},
    M(1e-3 * 101325 / (kK * kNA * 273.15), "L(STP)", "litri di gas ideale (0 \xC2\xB0" "C, 1 atm)", MOLM, 0, DEF),
    // ------------------------------------------------------------ peso <-> massa (SI: N)
    E(1e-5, "dyn", "dine", WGT, 0),
    E(1, "N", "newton", WGT, 0),
    E(1e3, "kN", "chilonewton", WGT, 0),
    E(9.80665, "kgf", "chilogrammo-forza", WGT, 0),
    E(4.4482216152605, "lbf", "libbra-forza", WGT, 0),
    E(9.80665e-3, "g", "massa: grammi", WGT, 0),
    E(9.80665, "kg", "massa: chilogrammi", WGT, 0),
    E(9806.65, "t", "massa: tonnellate", WGT, 0),
    X(kOz * kG0, "oz", "massa: once", "0.028349523125*9.80665", WGT, 0),
    E(4.4482216152605, "lb", "massa: libbre", WGT, 0),
    // ------------------------------------------------------------ scarpe (SI: lunghezza del piede, m)
    M(1e-2, "cm", "lunghezza del piede (cm)", SHOE, APX, APPROXF),
    M(1e-3, "MP", "Mondopoint (mm)", SHOE, APX, APPROXF),
    M(1e-2, "JP", "Giappone (cm)", SHOE, APX, APPROXF),
    M(0.0254, "in", "lunghezza del piede (in)", SHOE, APX, APPROXF),
    A(1.0 / 150, "EU", "taglia EU (punto di Parigi)", nullptr, SHOE, APX, 6, APPROXF),
    A(0.0254 / 3, "UK", "taglia UK", nullptr, SHOE, APX, 7, APPROXF),
    A(0.0254 / 3, "US(M)", "taglia US uomo", nullptr, SHOE, APX, 8, APPROXF),
    A(0.0254 / 3, "US(W)", "taglia US donna", nullptr, SHOE, APX, 9, APPROXF),
};
#undef E
#undef X
#undef M
#undef A
#undef I
#undef F
constexpr std::size_t kUnitCount = sizeof(kUnits) / sizeof(kUnits[0]);

const Prefix kPrefixes[] = {
    {"q", "quecto", 1e-30, -30, false}, {"r", "ronto", 1e-27, -27, false}, {"y", "yocto", 1e-24, -24, false},
    {"z", "zepto", 1e-21, -21, false}, {"a", "atto", 1e-18, -18, false}, {"f", "femto", 1e-15, -15, false},
    {"p", "pico", 1e-12, -12, false}, {"n", "nano", 1e-9, -9, false}, {MU, "micro", 1e-6, -6, false},
    {"m", "milli", 1e-3, -3, false}, {"c", "centi", 1e-2, -2, false}, {"d", "deci", 1e-1, -1, false},
    {"da", "deca", 1e1, 1, false}, {"h", "etto", 1e2, 2, false}, {"k", "chilo", 1e3, 3, false},
    {"M", "mega", 1e6, 6, false}, {"G", "giga", 1e9, 9, false}, {"T", "tera", 1e12, 12, false},
    {"P", "peta", 1e15, 15, false}, {"E", "exa", 1e18, 18, false}, {"Z", "zetta", 1e21, 21, false},
    {"Y", "yotta", 1e24, 24, false}, {"R", "ronna", 1e27, 27, false}, {"Q", "quetta", 1e30, 30, false},
    {"Ki", "kibi", 1024.0, 10, true}, {"Mi", "mebi", 1048576.0, 20, true}, {"Gi", "gibi", 1073741824.0, 30, true},
    {"Ti", "tebi", 1099511627776.0, 40, true}, {"Pi", "pebi", 1125899906842624.0, 50, true},
    {"Ei", "exbi", 1152921504606846976.0, 60, true}, {"Zi", "zebi", 1180591620717411303424.0, 70, true},
    {"Yi", "yobi", 1208925819614629174706176.0, 80, true},
};
constexpr std::size_t kPrefixCount = sizeof(kPrefixes) / sizeof(kPrefixes[0]);

[[noreturn]] void mathError(const std::string& what) { throw MathError("Math ERROR: " + what); }
[[noreturn]] void argError(const std::string& what) { throw MathError("Argument ERROR: " + what); }
double finite(double v) { if (!std::isfinite(v)) mathError("risultato fuori intervallo"); return v; }
double prefixFactor(const Ref& r) { return r.prefix >= 0 && static_cast<std::size_t>(r.prefix) < kPrefixCount ? kPrefixes[r.prefix].factor : 1.0; }
double paramOf(const Unit& u, const Params& p) {
    const double v = kCategories[u.cat].param == Param::Dpi ? p.dpi : p.molarMass;
    if (!(v > 0) || !std::isfinite(v)) argError(kCategories[u.cat].param == Param::Dpi ? "DPI > 0" : "massa molare > 0");
    return v;
}
double offsetOf(const Unit& u) { return u.kind() == Kind::Affine ? kOffsets[u.aux].v : 0.0; }
bool lowerEq(char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; }
} // namespace

const char* groupName(Group g) noexcept {
    static const char* const names[] = {"Base SI", "Meccanica", "Elettricita'", "Termica", "Luce e radiazioni", "Chimica",
                                        "Dati", "Logaritmiche", "Derivate", "Formati", "Taglie", "Valute"};
    const auto i = static_cast<std::size_t>(g);
    return i < static_cast<std::size_t>(Group::Count) ? names[i] : "";
}
std::size_t categoryCount() noexcept { return CAT_COUNT; }
const Category& category(std::size_t i) noexcept { return kCategories[i < CAT_COUNT ? i : 0]; }
std::size_t unitCount() noexcept { return kUnitCount; }
const Unit& unit(std::size_t i) noexcept { return kUnits[i < kUnitCount ? i : 0]; }
std::size_t indexOf(const Unit& u) noexcept { return static_cast<std::size_t>(&u - kUnits); }
void unitsOf(std::size_t cat, std::size_t& first, std::size_t& count) noexcept {
    first = 0; count = 0;
    while (first < kUnitCount && kUnits[first].cat != cat) ++first;
    while (first + count < kUnitCount && kUnits[first + count].cat == cat) ++count;
}
const char* const* fxCategoryNames() noexcept { return kFxNames; }
std::size_t prefixCount() noexcept { return kPrefixCount; }
const Prefix& prefix(std::size_t i) noexcept { return kPrefixes[i < kPrefixCount ? i : 0]; }
bool prefixAllowed(const Unit& u, std::size_t i) noexcept {
    if (i >= kPrefixCount || u.kind() != Kind::Linear) return false;
    return kPrefixes[i].iec ? u.has(flag::PrefixIEC) : u.has(flag::PrefixSI);
}
double affineOffset(const Unit& u) noexcept { return offsetOf(u); }
const char* affineOffsetText(const Unit& u) noexcept { return u.kind() == Kind::Affine ? kOffsets[u.aux].exact : "0"; }
const char* sourceName(unsigned s) noexcept { return s < sizeof(kSources) / sizeof(kSources[0]) ? kSources[s] : ""; }
std::size_t sourceCount() noexcept { return sizeof(kSources) / sizeof(kSources[0]); }
const char* functionFormula(const Unit& u) noexcept {
    return u.kind() == Kind::Function && u.aux < sizeof(kFormulas) / sizeof(kFormulas[0]) ? kFormulas[u.aux] : "";
}

std::string symbol(const Ref& r) {
    if (!r.unit) return {};
    std::string s = r.prefix >= 0 ? kPrefixes[r.prefix].sym : "";
    return s + r.unit->name;
}

Ref find(std::size_t cat, const std::string& sym) noexcept {
    std::size_t first = 0, count = 0;
    unitsOf(cat, first, count);
    for (std::size_t i = first; i < first + count; ++i) if (sym == kUnits[i].name) return {&kUnits[i], -1};
    // Prefisso + unita' che lo ammette ("kPa" e' gia' in tabella; "nPa", "TiB"...).
    for (std::size_t p = 0; p < kPrefixCount; ++p) {
        const char* ps = kPrefixes[p].sym;
        const std::size_t n = std::strlen(ps);
        const bool micro = p == 8 && sym.size() > 1 && sym[0] == 'u';    // 'u' per µ
        if (!micro && (sym.size() <= n || sym.compare(0, n, ps) != 0)) continue;
        const std::string rest = sym.substr(micro ? 1 : n);
        for (std::size_t i = first; i < first + count; ++i)
            if (rest == kUnits[i].name && prefixAllowed(kUnits[i], p)) return {&kUnits[i], static_cast<int>(p)};
    }
    return {};
}

namespace {
bool usable(const Unit& u, bool fxOnly, bool runMatrix) {
    if (fxOnly && !u.has(flag::FxCG50)) return false;
    if (runMatrix && u.has(flag::Param)) return false;
    return true;
}
}

int resolve(const std::string& from, const std::string& to, Ref& a, Ref& b, bool fxOnly, bool runMatrix) noexcept {
    bool fromKnown = false, toKnown = false;
    for (std::size_t c = 0; c < CAT_COUNT; ++c) {
        if (fxOnly && !kCategories[c].fxName) continue;
        const Ref ra = find(c, from), rb = find(c, to);
        const bool okA = ra && usable(*ra.unit, fxOnly, runMatrix), okB = rb && usable(*rb.unit, fxOnly, runMatrix);
        fromKnown = fromKnown || okA; toKnown = toKnown || okB;
        if (okA && okB) { a = ra; b = rb; return 0; }
    }
    return fromKnown && toKnown ? 1 : 2;
}

bool known(const std::string& sym, bool fxOnly, bool runMatrix) noexcept {
    for (std::size_t c = 0; c < CAT_COUNT; ++c) {
        const Ref r = find(c, sym);
        if (r && usable(*r.unit, fxOnly, runMatrix)) return true;
    }
    return false;
}

double toSI(double v, const Ref& r, const Params& p) {
    if (!r.unit) argError("unita' mancante");
    if (!std::isfinite(v)) mathError("valore non finito");
    const Unit& u = *r.unit;
    const double k = u.factor;
    switch (u.kind()) {
    case Kind::Linear: {
        double s = v * prefixFactor(r) * k;
        if (u.has(flag::Param)) s /= paramOf(u, p);
        return finite(s);
    }
    case Kind::Affine: return finite((v + kOffsets[u.aux].v) * k);
    case Kind::Inverse:
        if (v == 0) mathError("divisione per zero (" + std::string(u.name) + " = 0)");
        return finite(k / v);
    case Kind::Function:
        switch (u.aux) {
        case F_LEV10: return finite(k * std::pow(10.0, v / 10));
        case F_LEV20: return finite(k * std::pow(10.0, v / 20));
        case F_POW10: return finite(std::pow(10.0, k * v));
        case F_EXP: return finite(std::exp(k * v));
        case F_POW2: return finite(std::exp2(k * v));
        case F_AWG:
            if (v < -3 || v > 60) argError("AWG da -3 (0000) a 60");
            return 0.000127 * std::pow(92.0, (36 - v) / 39);
        case F_AREA:
            if (v < 0) argError("sezione negativa");
            return finite(std::sqrt(4 * k * v / kPi));
        case F_SEIS: return finite(std::pow(10.0, 1.5 * v + 4.8));
        case F_POH: return finite(std::pow(10.0, v - 14));
        default: break;
        }
        break;
    }
    argError("unita' non valida");
}

double fromSI(double s, const Ref& r, const Params& p) {
    if (!r.unit) argError("unita' mancante");
    if (!std::isfinite(s)) mathError("valore non finito");
    const Unit& u = *r.unit;
    const double k = u.factor;
    auto logOf = [&](double x) { if (!(x > 0)) mathError("logaritmo di un valore <= 0 (" + std::string(u.name) + ")"); return x; };
    switch (u.kind()) {
    case Kind::Linear: {
        double v = s / (prefixFactor(r) * k);
        if (u.has(flag::Param)) v *= paramOf(u, p);
        return finite(v);
    }
    case Kind::Affine: return finite(s / k - kOffsets[u.aux].v);
    case Kind::Inverse:
        if (s == 0) mathError("divisione per zero (" + std::string(u.name) + ")");
        return finite(k / s);
    case Kind::Function:
        switch (u.aux) {
        case F_LEV10: return finite(10 * std::log10(logOf(s / k)));
        case F_LEV20: return finite(20 * std::log10(logOf(s / k)));
        case F_POW10: return finite(std::log10(logOf(s)) / k);
        case F_EXP: return finite(std::log(logOf(s)) / k);
        case F_POW2: return finite(std::log2(logOf(s)) / k);
        case F_AWG: {
            const double n = 36 - 39 * std::log(logOf(s) / 0.000127) / std::log(92.0);
            if (n < -3 || n > 60) argError("diametro fuori dalla scala AWG (-3..60)");
            return n;
        }
        case F_AREA: if (s < 0) argError("diametro negativo"); return finite(kPi * s * s / (4 * k));
        case F_SEIS: return finite((std::log10(logOf(s)) - 4.8) / 1.5);
        case F_POH: return finite(std::log10(logOf(s)) + 14);
        default: break;
        }
        break;
    }
    argError("unita' non valida");
}

double convertLinear(double v, const Unit& from, const Unit& to) {
    if (from.cat != to.cat) argError("[" + std::string(from.name) + "] e [" + to.name + "] misurano grandezze diverse");
    return (v + offsetOf(from)) * from.factor / to.factor - offsetOf(to);
}

double convert(double v, const Ref& from, const Ref& to, const Params& p) {
    if (!from.unit || !to.unit) argError("unita' mancante");
    if (from.unit->cat != to.unit->cat) argError("[" + symbol(from) + "] e [" + symbol(to) + "] misurano grandezze diverse");
    if (!std::isfinite(v)) mathError("valore non finito");
    const auto simple = [](const Ref& r) {
        const Kind k = r.unit->kind();
        return r.prefix < 0 && !r.unit->has(flag::Param) && (k == Kind::Linear || k == Kind::Affine);
    };
    if (simple(from) && simple(to)) return finite(convertLinear(v, *from.unit, *to.unit));
    return fromSI(toSI(v, from, p), to, p);
}

// ---------------------------------------------------------------- esatto
bool parseExactText(const char* text, ExactFactor& out) noexcept {
    if (!text || !*text) return false;
    out = {exact::Rational::integer(1), 0};
    char op = '*';
    const char* p = text;
    char buf[48];
    while (*p) {
        std::size_t n = 0;
        while (p[n] && p[n] != '*' && (p[n] != '/' || n == 0)) ++n;
        if (n == 0 || n >= sizeof(buf)) return false;
        std::memcpy(buf, p, n); buf[n] = 0;
        if (std::strcmp(buf, "pi") == 0) out.pi += op == '*' ? 1 : -1;
        else {
            exact::Rational r;
            if (!exact::parseDecimal(buf[0] == '-' ? std::string(buf + 1) : std::string(buf), r)) return false;
            if (buf[0] == '-' && !exact::mul(r, exact::Rational::integer(-1), r)) return false;
            if (op == '*' ? !exact::mul(out.r, r, out.r) : !exact::div(out.r, r, out.r)) return false;
        }
        p += n;
        if (*p) { op = *p; ++p; if (!*p) return false; }
    }
    return true;
}

namespace {
bool decimalOf(double f, ExactFactor& out) noexcept {
    char buf[40];
    for (int digits = 15; digits <= 17; ++digits) {
        std::snprintf(buf, sizeof buf, "%.*g", digits, f);
        if (std::strtod(buf, nullptr) == f) break;
    }
    return parseExactText(buf, out);
}
bool scale(exact::Value& v, const ExactFactor& f, bool divide) noexcept {
    exact::Value r = exact::Value::fromRational(f.r);
    if (!(divide ? exact::div(v, r, v) : exact::mul(v, r, v))) return false;
    const exact::Value pi = exact::Value::piTimes(exact::Rational::integer(1));
    for (int k = 0; k < (f.pi < 0 ? -f.pi : f.pi); ++k) {
        const bool mul = (f.pi > 0) != divide;
        if (!(mul ? exact::mul(v, pi, v) : exact::div(v, pi, v))) return false;
    }
    return true;
}
} // namespace

bool exactFactor(const Ref& r, ExactFactor& out) noexcept {
    if (!r.unit) return false;
    const Unit& u = *r.unit;
    if (u.has(flag::Param) || (u.kind() != Kind::Linear && u.kind() != Kind::Affine)) return false;
    if (u.source() != DEF) return false;
    bool ok = u.exact ? parseExactText(u.exact, out) : (u.has(flag::ExactDecimal) && decimalOf(u.factor, out));
    if (!ok) return false;
    if (r.prefix >= 0) {
        const Prefix& p = kPrefixes[r.prefix];
        if (p.iec ? p.exp > 62 : (p.exp > 18 || p.exp < -18)) return false;
        std::int64_t m = 1;
        for (int k = 0; k < (p.exp < 0 ? -p.exp : p.exp); ++k) m *= p.iec ? 2 : 10;
        if (!(p.exp < 0 ? exact::div(out.r, exact::Rational::integer(m), out.r) : exact::mul(out.r, exact::Rational::integer(m), out.r))) return false;
    }
    return true;
}

bool convertExact(const exact::Value& v, const Ref& from, const Ref& to, exact::Value& out) noexcept {
    if (!from.unit || !to.unit || from.unit->cat != to.unit->cat) return false;
    ExactFactor fa, fb;
    if (!exactFactor(from, fa) || !exactFactor(to, fb)) return false;
    exact::Rational oa = exact::Rational::integer(0), ob = oa;
    auto offset = [](const Unit& u, exact::Rational& o) {
        if (u.kind() != Kind::Affine) return true;
        const char* t = affineOffsetText(u);
        if (!t) return false;
        ExactFactor f;
        if (!parseExactText(t, f) || f.pi != 0) return false;
        o = f.r;
        return true;
    };
    if (!offset(*from.unit, oa) || !offset(*to.unit, ob)) return false;
    exact::Value s = v;
    if (!exact::add(s, exact::Value::fromRational(oa), s) || !scale(s, fa, false) || !scale(s, fb, true)) return false;
    return exact::sub(s, exact::Value::fromRational(ob), out);
}

// ---------------------------------------------------------------- ricerca
bool matchesText(const char* text, const std::string& q) noexcept {
    if (q.empty()) return true;
    if (!text) return false;
    const std::size_t n = std::strlen(text);
    for (std::size_t i = 0; i + q.size() <= n; ++i) {
        std::size_t k = 0;
        while (k < q.size() && lowerEq(text[i + k], q[k])) ++k;
        if (k == q.size()) return true;
    }
    return false;
}
bool matches(const Unit& u, const std::string& q) noexcept { return matchesText(u.name, q) || matchesText(u.label, q); }

} // namespace fx::units
