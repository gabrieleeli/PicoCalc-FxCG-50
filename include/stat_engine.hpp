#pragma once
// Nucleo di Statistics alla parita' con il capitolo 6 del manuale fx-CG50
// (v1.18.0, solo Pimoroni Pico Plus 2 W). Solo calcolo e modelli: nessuna
// interfaccia, nessun accesso al workspace. Le funzioni della v1.16.0
// (statistics.hpp) restano invariate per il Pico 2 W congelato.
//
//  * dati con frequenze intere (0..100000, regola della v1.13.1) e quartili
//    Std (mediane delle meta') oppure OnData (SET UP Q1Q3 Type);
//  * 12 regressioni (ax+b, a+bx, Med-Med, X^2..X^4, Log, ae^bx, ab^x, Power,
//    Sin, Logistic) con r, r^2, MSe del manuale, residui, punti Med-Med,
//    stima x^ / y^; Sin e Logistic con un metodo iterativo a passi (FitTask);
//  * test Z, t, chi^2, F, ANOVA a una e due vie (AnovaTask) e intervalli;
//  * modelli dei grafici statistici (classi, baffi e outlier, NPP, torta, barre);
//  * stato persistente della modalita' (sezione 0x0101 del record v12).
#include "statistics.hpp"
#include "step_task.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace fx::stats {

// ---------------------------------------------------------------- dati
// Frequenze: nullptr = tutte 1; altrimenti stessa lunghezza dei dati, intere 0..100000.
void checkFrequencies(std::size_t size, const std::vector<double>* freq);
enum class QuartileRule : std::uint8_t { Std = 0, OnData = 1 };
// 1-Variable completo (Mod:n = modes.size()).
OneVar oneVarEx(const std::vector<double>& x, const std::vector<double>* freq, QuartileRule rule);
// 2-Variable con le frequenze (n = somma delle frequenze).
TwoVar twoVarEx(const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>* freq);

// ---------------------------------------------------------------- code delle distribuzioni
// Code calcolate direttamente (senza 1 - cdf), per i p-value dei test.
double normalLower(double z);
double normalUpper(double z);
double tLowerTail(double t, double df);
double tUpperTail(double t, double df);
double chiUpperTail(double x, double df);
double fLowerTail(double x, double d1, double d2);
double fUpperTail(double x, double d1, double d2);
double gammaQ(double a, double x);          // Gamma incompleta superiore regolarizzata
// Verso dell'ipotesi: 0 "≠", -1 "<", +1 ">".
double pValue(double lower, double upper, int tail);
// Quantili (motore unico di calc_library): normale standard e t con area superiore.
double zCritical(double level);              // z con area centrale = level
double tCritical(double level, double df);   // t con area centrale = level

// ---------------------------------------------------------------- regressioni
// I valori 0..8 coincidono con RegType (Settings::statRegType della v1.16.0).
enum class Reg : std::uint8_t { LinearAx = 0, MedMed = 1, Quad = 2, Cubic = 3, Quart = 4, Log = 5, ExpE = 6, ExpAb = 7, Power = 8,
                                Sin = 9, Logistic = 10, LinearA = 11, Count = 12 };
const char* regTitle(Reg t) noexcept;                 // "LinearReg(ax+b)", "SinReg"...
const char* regModel(Reg t) noexcept;                 // "y=a\xC2\xB7x+b"...
std::size_t regCoefficientCount(Reg t) noexcept;
struct Fit {
    Reg type{Reg::LinearAx};
    std::array<double, 5> coef{};                     // a, b, c, d, e nell'ordine del manuale
    std::size_t ncoef{0};
    double r{0}, r2{0}, mse{0}, n{0};
    bool hasR{false}, hasR2{false};
    std::array<double, 6> pts{};                      // Med-Med: x1, y1, x2, y2, x3, y3
    bool hasPts{false};
    double predict(double x) const;
    // Testo canonico per GRAPH (COPY): coefficienti con 15 cifre, Sin in radianti (suffisso ʳ).
    std::string expression() const;
};
// Righe della schermata dei risultati (etichetta, valore) nell'ordine del manuale.
std::vector<std::pair<std::string, double>> fitRows(const Fit& f);
// Residui yi - f(xi) di ogni coppia (le frequenze non contano).
std::vector<double> residuals(const Fit& f, const std::vector<double>& x, const std::vector<double>& y);
// x^ (una o piu' soluzioni reali ordinate); MathError se non esiste o il tipo non lo permette (Sin).
std::vector<double> estimateX(const Fit& f, double y);

// Regressione a passi: le forme chiuse finiscono al primo passo; Sin e
// Logistic consumano un'unita' per candidato della stima iniziale e per
// iterazione di Levenberg-Marquardt.
class FitTask final : public step::Task {
public:
    FitTask(Reg type, std::vector<double> x, std::vector<double> y, std::vector<double> freq);
    bool step(step::Budget& budget) override;
    std::size_t total() const override { return total_; }
    std::size_t done() const override { return done_; }
    const Fit& result() const noexcept { return fit_; }
private:
    void prepare();
    void finishClosed();
    Reg type_;
    std::vector<double> x_, y_, w_;
    Fit fit_{};
    int stage_{0};
    std::size_t total_{1}, done_{0}, cand_{0};
    std::array<double, 4> p_{}, best_{};
    double bestSs_{0}, lambda_{1e-3};
    int iter_{0};
    std::vector<double> grid_;
};
// Esegue FitTask fino alla fine (test, VARS).
Fit fit(Reg type, const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>* freq = nullptr);

// ---------------------------------------------------------------- TEST e INTR
// Una schermata di risultati: titolo (ipotesi alternativa), righe, variabili
// di VARS › STAT e il modello del grafico (DRAW).
struct Row { std::string label; double value; };
enum class Curve : std::uint8_t { None = 0, Normal, T, Chi, F };
struct Outcome {
    std::string title;                                // "\xCE\xBC\xE2\x89\xA0" "0" ...
    std::vector<Row> rows;
    std::vector<std::pair<std::uint8_t, double>> vars;   // (ResultVar, valore)
    Curve curve{Curve::None};
    double df1{0}, df2{0};
    double stat{0};                                   // statistica sul grafico
    int tail{0};                                      // area: 0 due code, -1 sinistra, +1 destra
    std::vector<double> listOut;                      // CNTRB (chi^2 GOF)
};
// Dati di un campione: lista (con frequenze) o valori riassuntivi (Var).
struct Sample {
    bool list{false};
    std::vector<double> x, f;                         // List: dati e frequenze (vuote = 1)
    double mean{0}, sd{0}, n{0};                      // Var (sd = sx dove serve)
};
Outcome zTest1S(const Sample& s, double mu0, double sigma, int tail);
Outcome zTest2S(const Sample& a, const Sample& b, double sigma1, double sigma2, int tail);
Outcome zTest1P(double p0, double x, double n, int tail);
Outcome zTest2P(double x1, double n1, double x2, double n2, int tail);
Outcome tTest1S(const Sample& s, double mu0, int tail);
Outcome tTest2S(const Sample& a, const Sample& b, bool pooled, int tail);
Outcome tTestReg(const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>* freq, int tail);
Outcome chiGof(const std::vector<double>& observed, const std::vector<double>& expected, double df);
// Matrice osservata per righe (rows x cols); expected esce con la stessa forma.
Outcome chi2Way(const std::vector<double>& observed, std::size_t rows, std::size_t cols, std::vector<double>& expected);
Outcome fTest2S(const Sample& a, const Sample& b, int tail);
Outcome zInt1S(const Sample& s, double level, double sigma);
Outcome zInt2S(const Sample& a, const Sample& b, double level, double sigma1, double sigma2);
Outcome zInt1P(double level, double x, double n);
Outcome zInt2P(double level, double x1, double n1, double x2, double n2);
Outcome tInt1S(const Sample& s, double level);
Outcome tInt2S(const Sample& a, const Sample& b, double level, bool pooled);

// ANOVA: una via (factorB vuota) o due vie (disegno bilanciato). Un'unita' per dato.
struct AnovaTable {
    bool twoWay{false}, interaction{false};
    double dfA{0}, ssA{0}, msA{0}, fA{0}, pA{0};
    double dfB{0}, ssB{0}, msB{0}, fB{0}, pB{0};
    double dfAB{0}, ssAB{0}, msAB{0}, fAB{0}, pAB{0};
    double dfE{0}, ssE{0}, msE{0};
    std::vector<double> levelsA, levelsB;             // livelli ordinati
    std::vector<double> cellMeans;                    // due vie: medie per (A, B), A per righe
};
class AnovaTask final : public step::Task {
public:
    AnovaTask(std::vector<double> factorA, std::vector<double> factorB, std::vector<double> dependent);
    bool step(step::Budget& budget) override;
    std::size_t total() const override { return y_.size() + 1; }
    std::size_t done() const override { return next_; }
    const AnovaTable& table() const noexcept { return table_; }
    Outcome outcome() const;
private:
    void finish();
    std::vector<double> a_, b_, y_;
    std::size_t next_{0};
    std::vector<double> sum_, cnt_;                   // per cella (due vie) o per livello (una via)
    double total_{0}, totalSq_{0};
    AnovaTable table_;
};
Outcome anova(const std::vector<double>& factorA, const std::vector<double>& factorB, const std::vector<double>& dependent);

// ---------------------------------------------------------------- grafici (modelli puri)
enum class GraphType : std::uint8_t { Scatter = 0, XYLine, NPPlot, Pie, Hist, MedBox, NDist, Broken, Linear, MedMed, Quad, Cubic, Quart,
                                      Log, ExpE, ExpAb, Power, Sin, Logistic, Bar, Count };
const char* graphTypeName(GraphType t) noexcept;      // "Scatter", "xyLine", "NPPlot", "Pie", "Hist", ...
bool graphNeedsY(GraphType t) noexcept;               // XList e YList
bool graphIsRegression(GraphType t) noexcept;
Reg regressionOf(GraphType t) noexcept;
struct Histogram { double start{0}, width{1}; std::vector<double> counts; };
Histogram histogram(const std::vector<double>& x, const std::vector<double>* freq, double start, double width);
struct BoxModel { double min{0}, q1{0}, med{0}, q3{0}, max{0}, lowWhisker{0}, highWhisker{0}; std::vector<double> outliers; };
BoxModel boxModel(const std::vector<double>& x, const std::vector<double>* freq, QuartileRule rule, bool outliers);
// NPPlot: dati ordinati (ripetuti secondo le frequenze) e quantili normali di (i - 0.5)/n.
void normalProbability(const std::vector<double>& x, const std::vector<double>* freq, std::vector<double>& xs, std::vector<double>& zs);
// Torta: percentuali (dati >= 0, somma > 0).
std::vector<double> piePercent(const std::vector<double>& data);

// ---------------------------------------------------------------- variabili di VARS › STAT
// Valori dell'ultimo calcolo (salvati nella sezione): nome canonico e nome mostrato.
enum ResultVar : std::uint8_t {
    VN = 0, VMeanX, VSumX, VSumX2, VPopSdX, VSdX, VMinX, VMaxX,
    VMeanY, VSumY, VSumY2, VSumXY, VPopSdY, VSdY, VMinY, VMaxY,
    VRegA, VRegB, VRegC, VRegD, VRegE, VRegR, VRegR2, VRegMse, VQ1, VQ3, VMed, VMod, VHStart, VHPitch,
    VX1, VY1, VX2, VY2, VX3, VY3,
    VInN, VInMean, VInSx, VInN1, VInN2, VInMean1, VInMean2, VInSx1, VInSx2, VInSp,
    VP, VZ, VT, VChi, VF, VPHat, VPHat1, VPHat2, VDf, VSe, VR, VR2, VPa, VFa, VAdf, VSSa, VMSa, VPb, VFb, VBdf, VSSb, VMSb,
    VPab, VFab, VABdf, VSSab, VMSab, VEdf, VSSe, VMSe,
    VLeft, VRight, VIPHat, VIPHat1, VIPHat2, VIDf,
    VDistP, VXInv, VX1Inv, VX2Inv, VZLow, VZUp, VTLow, VTUp,
    ResultVarCount
};
const char* resultVarName(std::uint8_t v) noexcept;   // "stat_n", "reg_a", "stat_res_p"...
int resultVarByName(const std::string& lowerName) noexcept;   // -1 se sconosciuto
// Variabili di un 1-Var / 2-Var / regressione.
void oneVarVars(const OneVar& o, std::vector<std::pair<std::uint8_t, double>>& out);
void twoVarVars(const TwoVar& t, std::vector<std::pair<std::uint8_t, double>>& out);
void fitVars(const Fit& f, std::vector<std::pair<std::uint8_t, double>>& out);

// ---------------------------------------------------------------- stato persistente (sezione 0x0101)
struct StatGraph {
    GraphType type{GraphType::Scatter};
    std::uint8_t xList{0}, yList{1};
    std::uint8_t freq{0};                             // 0 = 1, k = List k
    std::uint8_t mark{0};                             // 0 quadrato, 1 croce, 2 punto, 3 cerchio
    std::uint8_t color{0};                            // indice della tavolozza (7 colori)
    std::uint8_t colorLink{0};                        // 0 Off, 1 X&Freq, 2 OnData
    bool on{false};
    bool outliers{false};
    std::uint8_t pieData{0};                          // indice di lista
    std::uint8_t pieDisplay{0};                       // 0 %, 1 Data, 2 None
    std::uint8_t pieStore{0};                         // 0 None, k = List k
    std::array<std::uint8_t, 3> bar{{1, 0, 0}};       // Data1..Data3: 0 None, k = List k
    std::uint8_t stick{0};                            // 0 Length, 1 Horiz
    double hStart{0}, hWidth{1};
    bool hSet{false};                                 // Start/Width gia' scelti
    bool operator==(const StatGraph& o) const noexcept;
};
StatGraph defaultGraph(std::size_t index) noexcept;  // StatGraph1 acceso, colori Blue, Red, Green
constexpr std::size_t kPaletteColors = 7;
const char* paletteName(std::uint8_t c) noexcept;    // "Blue", "Red", "Magenta", "Green", "Cyan", "Yellow", "Black"
struct PlusState {
    std::array<StatGraph, 3> graphs{defaultGraph(0), defaultGraph(1), defaultGraph(2)};
    std::uint8_t var1X{0}, var1Freq{0}, var2Freq{0};  // SET di CALC (2Var XList/YList in Settings)
    bool statWindManual{false};
    std::uint8_t residList{0};                        // 0 None, k = List k
    QuartileRule quartiles{QuartileRule::Std};
    std::array<std::uint8_t, 26> listColor{};
    std::array<std::string, 26> listFormula{};
    std::array<double, ResultVarCount> results{};
    std::array<std::uint8_t, ResultVarCount> valid{};
    void store(const std::vector<std::pair<std::uint8_t, double>>& vars);
    bool value(std::uint8_t v, double& out) const noexcept;
    bool operator==(const PlusState& o) const noexcept;
};
void reset(PlusState& s);
void encode(const PlusState& s, std::vector<std::uint8_t>& out);
// Solo dati validi (numeri finiti, indici nei limiti); s invariato se false.
bool decode(PlusState& s, const std::uint8_t* data, std::size_t size);
constexpr std::size_t kFormulaMax = 120;              // caratteri di una formula nella testa di una lista
} // namespace fx::stats
