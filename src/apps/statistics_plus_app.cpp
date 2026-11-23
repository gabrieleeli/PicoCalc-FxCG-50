// STATISTICS della Pimoroni Pico Plus 2 W (v1.18.0): parita' con il capitolo 6
// del manuale fx-CG50 (descrizione del comportamento, nessun codice o testo
// copiato). Tabella delle voci e decisioni: docs/reports/ANALISI_FONTI_v1.18.0.md.
//
//  * editor di List 1-26 su tre pagine di tasti: GRAPH CALC TEST INTR DIST /
//    SRT-A SRT-D DEL DEL-ALL INS / ADD JUMP COLOR FILE CSV; riga SUB, testa
//    della lista con la formula (ricalcolo automatico, Circular ERROR);
//  * StatGraph1-3 (SET, SELECT, DefG), 20 tipi di grafico, Stat Wind, TRACE,
//    V-Window, CALC dal grafico (stat_plot.cpp per il disegno);
//  * CALC 1-VAR, 2-VAR, REG (12 regressioni) con SET, residui (Resid List),
//    COPY nell'elenco delle relazioni di GRAPH e DRAW;
//  * TEST, INTR, DIST con Data List/Variable, Save Res, CALC e DRAW;
//  * SET UP della modalita'; i lavori lunghi in step::Task con AC ("Break").
#include "apps.hpp"
#include "csv_io.hpp"
#include "dist_core.hpp"
#include "stat_engine.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <map>
#include <memory>

namespace fx::app {
using namespace ui::theme;
using stats::GraphType;
using stats::Reg;
using Screen = StatisticsApp::Screen;

namespace {
// ---------------------------------------------------------------- azioni dei tasti
enum Act : int {
    ActSrtA = 1, ActSrtD, ActDel, ActDelAll, ActIns, ActAdd, ActColor, ActFile, ActJumpTop, ActJumpBottom, ActJumpRow,
    ActCsvLoad, ActCsvSave, ActCsvSet,
    ActGraph1, ActGraph2, ActGraph3, ActSelect, ActGraphSet,
    Act1Var, Act2Var, ActCalcSet,
    ActCopy, ActDraw, ActTrace, ActVWin, ActCalcRun,
    ActGph1, ActGph2, ActGph3, ActDefG,
    ActOn, ActOff,
    ActRegBase = 100,       // + Reg
    ActTestBase = 200,      // + TestKind
    ActDistBase = 300,      // + indice di dist::Kind
};
enum TestKind : int { TZ1S = 0, TZ2S, TZ1P, TZ2P, TT1S, TT2S, TTReg, TChiGof, TChi2W, TF2S, TAnova, IZ1S, IZ2S, IZ1P, IZ2P, IT1S, IT2S, TestKindCount };
const char* const kTestTitles[TestKindCount] = {
    "1-Sample ZTest", "2-Sample ZTest", "1-Prop ZTest", "2-Prop ZTest", "1-Sample tTest", "2-Sample tTest", "LinearReg tTest",
    "\xCF\x87\xC2\xB2 GOF Test", "\xCF\x87\xC2\xB2 2-way Test", "2-Sample FTest", "ANOVA",
    "1-Sample ZInterval", "2-Sample ZInterval", "1-Prop ZInterval", "2-Prop ZInterval", "1-Sample tInterval", "2-Sample tInterval"};
constexpr int reg(Reg r) { return ActRegBase + static_cast<int>(r); }
constexpr int test(TestKind t) { return ActTestBase + static_cast<int>(t); }
constexpr int distAct(dist::Family f, dist::Func fn) { return ActDistBase + static_cast<int>(f) * 3 + static_cast<int>(fn); }

// ---------------------------------------------------------------- barre F1-F6
// Regressioni (CALC › REG e CALC dal grafico): X › ax+b / a+bx, Exp › ae^bx / ab^x.
const FKeyPage kXPage{{{"ax+b", reg(Reg::LinearAx)}, {"a+bx", reg(Reg::LinearA)}, {}, {}, {}, {}}};
const FKeyMenu kXMenu{&kXPage, 1, nullptr};
const FKeyPage kExpPage{{{"ae^bx", reg(Reg::ExpE)}, {"ab^x", reg(Reg::ExpAb)}, {}, {}, {}, {}}};
const FKeyMenu kExpMenu{&kExpPage, 1, nullptr};
const FKeyPage kRegPages[] = {
    {{{"X", -1, &kXMenu}, {"Med", reg(Reg::MedMed)}, {"X\xC2\xB2", reg(Reg::Quad)}, {"X\xC2\xB3", reg(Reg::Cubic)}, {"X\xE2\x81\xB4", reg(Reg::Quart)}, {}}},
    {{{"Log", reg(Reg::Log)}, {"Exp", -1, &kExpMenu}, {"Pwr", reg(Reg::Power)}, {"Sin", reg(Reg::Sin)}, {"Lgst", reg(Reg::Logistic)}, {}}}};
const FKeyMenu kRegMenu{kRegPages, 2, nullptr};
const FKeyPage kCalcPage{{{"1-VAR", Act1Var}, {"2-VAR", Act2Var}, {"REG", -1, &kRegMenu}, {}, {}, {"SET", ActCalcSet}}};
const FKeyMenu kCalcMenu{&kCalcPage, 1, nullptr};
const FKeyPage kGraphPage{{{"GRAPH1", ActGraph1}, {"GRAPH2", ActGraph2}, {"GRAPH3", ActGraph3}, {"SELECT", ActSelect}, {}, {"SET", ActGraphSet}}};
const FKeyMenu kGraphMenu{&kGraphPage, 1, nullptr};
const FKeyPage kZTestPage{{{"1-SMPL", test(TZ1S)}, {"2-SMPL", test(TZ2S)}, {"1-PROP", test(TZ1P)}, {"2-PROP", test(TZ2P)}, {}, {}}};
const FKeyMenu kZTestMenu{&kZTestPage, 1, nullptr};
const FKeyPage kTTestPage{{{"1-SMPL", test(TT1S)}, {"2-SMPL", test(TT2S)}, {"REG", test(TTReg)}, {}, {}, {}}};
const FKeyMenu kTTestMenu{&kTTestPage, 1, nullptr};
const FKeyPage kChiPage{{{"GOF", test(TChiGof)}, {"2WAY", test(TChi2W)}, {}, {}, {}, {}}};
const FKeyMenu kChiMenu{&kChiPage, 1, nullptr};
const FKeyPage kTestPage{{{"Z", -1, &kZTestMenu}, {"t", -1, &kTTestMenu}, {"CHI", -1, &kChiMenu}, {"F", test(TF2S)}, {"ANOVA", test(TAnova)}, {}}};
const FKeyMenu kTestMenu{&kTestPage, 1, nullptr};
const FKeyPage kZIntPage{{{"1-SMPL", test(IZ1S)}, {"2-SMPL", test(IZ2S)}, {"1-PROP", test(IZ1P)}, {"2-PROP", test(IZ2P)}, {}, {}}};
const FKeyMenu kZIntMenu{&kZIntPage, 1, nullptr};
const FKeyPage kTIntPage{{{"1-SMPL", test(IT1S)}, {"2-SMPL", test(IT2S)}, {}, {}, {}, {}}};
const FKeyMenu kTIntMenu{&kTIntPage, 1, nullptr};
const FKeyPage kIntrPage{{{"Z", -1, &kZIntMenu}, {"t", -1, &kTIntMenu}, {}, {}, {}, {}}};
const FKeyMenu kIntrMenu{&kIntrPage, 1, nullptr};
using dist::Family;
using dist::Func;
#define FXAI_DIST_MENU(NAME, FAM, L1, L2, L3)                                                                       \
    const FKeyPage kD##NAME##Page{{{L1, distAct(Family::FAM, Func::PD)}, {L2, distAct(Family::FAM, Func::CD)},      \
                                   {L3, distAct(Family::FAM, Func::Inv)}, {}, {}, {}}};                            \
    const FKeyMenu kD##NAME##Menu{&kD##NAME##Page, 1, nullptr};
FXAI_DIST_MENU(Norm, Normal, "Npd", "Ncd", "InvN")
FXAI_DIST_MENU(T, T, "tpd", "tcd", "Invt")
FXAI_DIST_MENU(Chi, Chi, "Cpd", "Ccd", "InvC")
FXAI_DIST_MENU(F, F, "Fpd", "Fcd", "InvF")
FXAI_DIST_MENU(Bin, Binomial, "Bpd", "Bcd", "InvB")
FXAI_DIST_MENU(Poi, Poisson, "Ppd", "Pcd", "InvP")
FXAI_DIST_MENU(Geo, Geometric, "Gpd", "Gcd", "InvG")
FXAI_DIST_MENU(Hyp, Hypergeometric, "Hpd", "Hcd", "InvH")
#undef FXAI_DIST_MENU
const FKeyPage kDistPages[] = {
    {{{"NORM", -1, &kDNormMenu}, {"t", -1, &kDTMenu}, {"CHI", -1, &kDChiMenu}, {"F", -1, &kDFMenu}, {"BINOMIAL", -1, &kDBinMenu}, {}}},
    {{{"POISSON", -1, &kDPoiMenu}, {"GEO", -1, &kDGeoMenu}, {"H.GEO", -1, &kDHypMenu}, {}, {}, {}}}};
const FKeyMenu kDistMenu{kDistPages, 2, nullptr};
const FKeyPage kJumpPage{{{"TOP", ActJumpTop}, {"BOTTOM", ActJumpBottom}, {"ROW", ActJumpRow}, {}, {}, {}}};
const FKeyMenu kJumpMenu{&kJumpPage, 1, nullptr};
const FKeyPage kCsvPage{{{"LOAD", ActCsvLoad}, {"SAVE\xC2\xB7" "AS", ActCsvSave}, {"SET", ActCsvSet}, {}, {}, {}}};
const FKeyMenu kCsvMenu{&kCsvPage, 1, nullptr};
// E01: tre pagine dell'editor delle liste.
const FKeyPage kListPages[] = {
    {{{"GRAPH", -1, &kGraphMenu}, {"CALC", -1, &kCalcMenu}, {"TEST", -1, &kTestMenu}, {"INTR", -1, &kIntrMenu}, {"DIST", -1, &kDistMenu}, {}}},
    {{{"SRT-A", ActSrtA}, {"SRT-D", ActSrtD}, {"DEL", ActDel}, {"DEL-ALL", ActDelAll}, {"INS", ActIns}, {}}},
    {{{"ADD", ActAdd}, {"JUMP", -1, &kJumpMenu}, {"COLOR", ActColor}, {"FILE", ActFile}, {"CSV", -1, &kCsvMenu}, {}}}};
const FKeyMenu kListMenu{kListPages, 3, nullptr};
// Schermata del grafico: CALC (1-VAR, 2-VAR e regressioni sulle liste del grafico), TRACE, V-WIN.
const FKeyPage kGraphCalcPages[] = {
    {{{"1-VAR", Act1Var}, {"2-VAR", Act2Var}, {"X", -1, &kXMenu}, {"Med", reg(Reg::MedMed)}, {"X\xC2\xB2", reg(Reg::Quad)}, {}}},
    {{{"X\xC2\xB3", reg(Reg::Cubic)}, {"X\xE2\x81\xB4", reg(Reg::Quart)}, {"Log", reg(Reg::Log)}, {"Exp", -1, &kExpMenu}, {"Pwr", reg(Reg::Power)}, {}}},
    {{{"Sin", reg(Reg::Sin)}, {"Lgst", reg(Reg::Logistic)}, {}, {}, {}, {}}}};
const FKeyMenu kGraphCalcMenu{kGraphCalcPages, 3, nullptr};
const FKeyPage kPlotPage{{{"CALC", -1, &kGraphCalcMenu}, {"TRACE", ActTrace}, {"V-WIN", ActVWin}, {}, {}, {}}};
const FKeyMenu kPlotMenu{&kPlotPage, 1, nullptr};
const FKeyPage kRegResultPage{{{}, {}, {}, {}, {"COPY", ActCopy}, {"DRAW", ActDraw}}};
const FKeyMenu kRegResultMenu{&kRegResultPage, 1, nullptr};
const FKeyPage kDrawPage{{{}, {}, {}, {}, {}, {"DRAW", ActDraw}}};
const FKeyMenu kDrawMenu{&kDrawPage, 1, nullptr};
const FKeyPage kEmptyPage{{{}, {}, {}, {}, {}, {}}};
const FKeyMenu kEmptyMenu{&kEmptyPage, 1, nullptr};
const FKeyPage kFormPage{{{"CALC", ActCalcRun}, {}, {}, {}, {}, {"DRAW", ActDraw}}};
const FKeyMenu kFormMenu{&kFormPage, 1, nullptr};
const FKeyPage kFormCalcPage{{{"CALC", ActCalcRun}, {}, {}, {}, {}, {}}};
const FKeyMenu kFormCalcMenu{&kFormCalcPage, 1, nullptr};
const FKeyPage kSetPage{{{"GPH1", ActGph1}, {"GPH2", ActGph2}, {"GPH3", ActGph3}, {}, {"DefG", ActDefG}, {}}};
const FKeyMenu kSetMenu{&kSetPage, 1, nullptr};
const FKeyPage kSelectPage{{{"On", ActOn}, {"Off", ActOff}, {}, {}, {}, {"DRAW", ActDraw}}};
const FKeyMenu kSelectMenu{&kSelectPage, 1, nullptr};

// ---------------------------------------------------------------- scelte dei moduli
std::vector<std::string> listNames(const char* none) {
    std::vector<std::string> v;
    if (none) v.emplace_back(none);
    for (int i = 1; i <= 26; ++i) v.push_back("List " + std::to_string(i));
    return v;
}
std::vector<std::string> matNames() {
    std::vector<std::string> v;
    for (char c = 'A'; c <= 'Z'; ++c) v.push_back(std::string("Mat ") + c);
    return v;
}
std::vector<std::string> paletteNames() {
    std::vector<std::string> v;
    for (std::uint8_t i = 0; i < stats::kPaletteColors; ++i) v.push_back(stats::paletteName(i));
    return v;
}
const char* const kOrdinals[] = {"Base", "Second", "Third", "Fourth", "Fifth", "Sixth"};
std::string tailChoice(const char* lhs, int tail, const char* rhs) {
    return std::string(lhs) + (tail == 1 ? "<" : tail == 2 ? ">" : "\xE2\x89\xA0") + rhs;
}
FormField tailField(const char* label, const char* lhs, const char* rhs, int choice) {
    return choiceField(label, {tailChoice(lhs, 0, rhs), tailChoice(lhs, 1, rhs), tailChoice(lhs, 2, rhs)}, choice);
}
// Verso del test dalla scelta (0 ≠, 1 <, 2 >) al segno del nucleo (0, -1, +1).
int tailOf(int choice) { return choice == 1 ? -1 : choice == 2 ? 1 : 0; }

// Lavoro di un solo passo (calcolo singolo della DIST, test): stesso percorso dei lavori lunghi.
class FnTask final : public step::Task {
public:
    explicit FnTask(std::function<void()> fn) : fn_(std::move(fn)) {}
    bool step(step::Budget&) override { fn_(); done_ = 1; return true; }
    std::size_t total() const override { return 1; }
    std::size_t done() const override { return done_; }
private:
    std::function<void()> fn_;
    std::size_t done_{0};
};
} // namespace

// ---------------------------------------------------------------- vista
struct StatisticsApp::View {
    ModeShell shell;
    FileDialog dialog;
    bool saving{false};
    Screen screen{Screen::Lists};
    std::vector<Screen> back;                   // schermate da cui si e' arrivati (EXIT)
    // editor delle liste
    int row{0}, col{0}, firstRow{0}, firstCol{0};
    bool editing{false};
    LineEditor editor{240};
    // SRT-A / SRT-D: domande in sequenza
    bool sortDesc{false};
    int sortCount{0};
    std::vector<int> sortLists;
    // risultati
    ResultList results;
    enum class Kind { None, OneVar, TwoVar, Reg, Outcome, Dist, DistList, Anova } kind{Kind::None};
    stats::Fit fit;
    std::vector<double> fitX, fitY, fitF;
    std::size_t fitXList{0}, fitYList{1};
    stats::Outcome outcome;
    stats::AnovaTable anova;
    dist::GraphModel distGraph;
    std::vector<std::string> graphLines;
    // grafici
    std::vector<statplot::Plot> plots;
    statplot::Window window;
    bool overlay{false};
    bool tracing{false};
    std::size_t tracePlot{0}, traceIndex{0};
    std::string traceLabel;
    std::vector<std::size_t> pendingDraw;       // grafici da disegnare dopo Start/Width
    std::size_t histGraph{0};
    std::uint8_t histAsked{0};                  // Start/Width gia' chiesti in questo DRAW (bit per StatGraph)
    // moduli
    Form form;
    int formKind{-1};                           // TestKind o ActDistBase + indice
    int setGraph{0};
    std::map<std::string, std::pair<std::string, int>> remembered;   // valori dei moduli per etichetta
    SelectList copyList;
    // lavori lunghi
    step::Runner runner;
    std::unique_ptr<step::Task> task;
    std::function<void(Context&)> onDone;
    int askTag{0};
};

StatisticsApp::StatisticsApp() = default;
StatisticsApp::~StatisticsApp() = default;

namespace {
using View = StatisticsApp::View;
const std::vector<double>* freqList(Context& ctx, std::uint8_t f) { return f == 0 ? nullptr : &ctx.ws.lists[f - 1]; }
void go(View& v, Screen s) { v.back.push_back(v.screen); v.screen = s; }
void barFor(View& v);
void goBack(View& v) {
    if (v.back.empty()) v.screen = Screen::Lists;
    else { v.screen = v.back.back(); v.back.pop_back(); }
    if (v.screen == Screen::Lists) v.back.clear();
    barFor(v);
}
void barFor(View& v) {
    switch (v.screen) {
    case Screen::Lists: v.shell.fkeys.set(&kListMenu); break;
    case Screen::Graph: v.shell.fkeys.set(&kPlotMenu); break;
    case Screen::Results:
        if (v.kind == View::Kind::Reg) v.shell.fkeys.set(&kRegResultMenu);
        else if ((v.kind == View::Kind::Outcome && v.outcome.curve != stats::Curve::None) || (v.kind == View::Kind::Dist && v.distGraph.available) ||
                 (v.kind == View::Kind::Anova && v.anova.twoWay)) v.shell.fkeys.set(&kDrawMenu);
        else v.shell.fkeys.set(&kEmptyMenu);
        break;
    case Screen::Form: v.shell.fkeys.set(&kFormMenu); break;
    case Screen::Set: v.shell.fkeys.set(&kSetMenu); break;
    case Screen::Select: v.shell.fkeys.set(&kSelectMenu); break;
    default: v.shell.fkeys.set(&kEmptyMenu); break;
    }
}

// ---------------------------------------------------------------- formule nella testa delle liste (E11)
// Liste citate da una formula ("List 3", "list12").
std::vector<int> referencedLists(const std::string& f) {
    std::vector<int> out;
    for (std::size_t i = 0; i + 4 <= f.size(); ++i) {
        if ((f[i] != 'L' && f[i] != 'l') || f.compare(i + 1, 3, "ist") != 0) continue;
        if (i > 0 && (std::isalnum(static_cast<unsigned char>(f[i - 1])) || f[i - 1] == '_')) continue;
        std::size_t j = i + 4;
        while (j < f.size() && f[j] == ' ') ++j;
        int n = 0, digits = 0;
        while (j < f.size() && std::isdigit(static_cast<unsigned char>(f[j])) && digits < 3) { n = n * 10 + (f[j] - '0'); ++j; ++digits; }
        if (digits && n >= 1 && n <= 26) out.push_back(n - 1);
    }
    return out;
}
// Ordine di calcolo delle liste con formula; false se c'e' un ciclo.
bool formulaOrder(const std::array<std::string, 26>& formulas, std::vector<int>& order) {
    std::array<int, 26> state{};                // 0 da visitare, 1 in corso, 2 fatto
    order.clear();
    std::function<bool(int)> visit = [&](int i) {
        if (state[static_cast<std::size_t>(i)] == 2) return true;
        if (state[static_cast<std::size_t>(i)] == 1) return false;
        state[static_cast<std::size_t>(i)] = 1;
        for (int d : referencedLists(formulas[static_cast<std::size_t>(i)]))
            if (!formulas[static_cast<std::size_t>(d)].empty() && !visit(d)) return false;
            else if (d == i) return false;
        state[static_cast<std::size_t>(i)] = 2;
        order.push_back(i);
        return true;
    };
    for (int i = 0; i < 26; ++i) if (!formulas[static_cast<std::size_t>(i)].empty() && !visit(i)) return false;
    return true;
}
std::vector<double> evaluateFormula(Context& ctx, const std::string& f) {
    const CalcValue cv = evaluateAny(*parseExpression(f), ctx.ws.evalContext());
    if (cv.type != ValueType::List) throw MathError("Argument ERROR: la formula della lista deve dare una lista");
    if (cv.complex()) throw MathError("Non-Real ERROR: lista con valori complessi");
    return cv.re;
}
// Ricalcola le liste con formula (dopo ogni modifica delle liste). strict: errori visibili.
void recalcFormulas(Context& ctx, bool strict) {
    auto& formulas = ctx.ws.statistics.listFormula;
    std::vector<int> order;
    if (!formulaOrder(formulas, order)) { if (strict) throw MathError("Circular ERROR: formula delle liste circolare"); return; }
    for (int i : order) {
        try {
            std::vector<double> v = evaluateFormula(ctx, formulas[static_cast<std::size_t>(i)]);
            if (ctx.ws.lists[static_cast<std::size_t>(i)] != v) { ctx.ws.setList(static_cast<std::size_t>(i), std::move(v)); ctx.markDirty(); }
        } catch (const MathError&) {
            if (strict) throw;
        }
    }
}
void setFormula(Context& ctx, int list, const std::string& text) {
    auto& f = ctx.ws.statistics.listFormula;
    if (text.empty()) { f[static_cast<std::size_t>(list)].clear(); ctx.markDirty(); return; }
    if (text.size() > stats::kFormulaMax) throw MathError("Argument ERROR: formula troppo lunga");
    std::array<std::string, 26> trial = f;
    trial[static_cast<std::size_t>(list)] = text;
    std::vector<int> order;
    if (!formulaOrder(trial, order)) throw MathError("Circular ERROR: la formula cita la propria lista");
    std::vector<double> v = evaluateFormula(ctx, text);              // errori della formula visibili subito
    ctx.ws.setList(static_cast<std::size_t>(list), std::move(v));
    f[static_cast<std::size_t>(list)] = text;
    ctx.markDirty();
    recalcFormulas(ctx, false);
}
// Una lista modificata a mano perde la formula (Q8).
void touchList(Context& ctx, int list) {
    auto& f = ctx.ws.statistics.listFormula[static_cast<std::size_t>(list)];
    if (!f.empty()) f.clear();
    ctx.markDirty();
    recalcFormulas(ctx, false);
}

// ---------------------------------------------------------------- editor (E02-E08)
std::size_t lengthOf(Context& ctx, int c) { return ctx.ws.lists[static_cast<std::size_t>(c)].size(); }
void clampRow(Context& ctx, View& v) {
    if (v.row >= 0) v.row = std::min(v.row, static_cast<int>(lengthOf(ctx, v.col)));
}
void setCell(Context& ctx, View& v, double value) {
    auto& l = ctx.ws.lists[static_cast<std::size_t>(v.col)];
    const std::size_t r = static_cast<std::size_t>(v.row);
    if (r >= l.size()) ctx.ws.checkListLength(static_cast<std::size_t>(v.col), r + 1);
    if (r >= l.size()) l.resize(r + 1, 0.0);
    l[r] = value;
    touchList(ctx, v.col);
    ++v.row;
}
void deleteCell(Context& ctx, View& v) {
    auto& l = ctx.ws.lists[static_cast<std::size_t>(v.col)];
    if (v.row < 0 || static_cast<std::size_t>(v.row) >= l.size()) return;
    l.erase(l.begin() + v.row);
    touchList(ctx, v.col);
    clampRow(ctx, v);
}
void insertCell(Context& ctx, View& v, bool after) {
    auto& l = ctx.ws.lists[static_cast<std::size_t>(v.col)];
    ctx.ws.checkListLength(static_cast<std::size_t>(v.col), l.size() + 1);
    const std::size_t at = std::min(l.size(), static_cast<std::size_t>(std::max(0, v.row)) + (after && !l.empty() ? 1 : 0));
    l.insert(l.begin() + static_cast<long>(at), 0.0);
    touchList(ctx, v.col);
    v.row = static_cast<int>(at);
}
// SRT-A / SRT-D: la lista base ordinata, le altre riordinate come lei (stabile).
void sortLists(Context& ctx, const std::vector<int>& lists, bool desc) {
    auto& L = ctx.ws.lists;
    const auto& base = L[static_cast<std::size_t>(lists.front())];
    for (int i : lists)
        if (L[static_cast<std::size_t>(i)].size() != base.size()) throw DimensionError("Dimension ERROR: liste da ordinare di lunghezza diversa");
    std::vector<std::size_t> idx(base.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) { return desc ? base[a] > base[b] : base[a] < base[b]; });
    std::vector<std::vector<double>> sorted;
    for (int i : lists) {
        std::vector<double> s;
        for (std::size_t k : idx) s.push_back(L[static_cast<std::size_t>(i)][k]);
        sorted.push_back(std::move(s));
    }
    for (std::size_t k = 0; k < lists.size(); ++k) { L[static_cast<std::size_t>(lists[k])] = std::move(sorted[k]); ctx.ws.statistics.listFormula[static_cast<std::size_t>(lists[k])].clear(); }
    ctx.markDirty();
    recalcFormulas(ctx, false);
}
enum AskTag : int { AskSortCount = 1, AskSortList, AskJumpRow, AskFormula };
void askSortNext(View& v) {
    const std::size_t k = v.sortLists.size();
    const std::string q = v.sortCount == 1 ? "Select List  List No:" : std::string("Select ") + kOrdinals[k] + " List  List No:";
    v.shell.ask.open(q, AskSortList);
}
} // namespace

namespace {
// ---------------------------------------------------------------- lavori lunghi
void startTask(View& v, std::unique_ptr<step::Task> t, std::function<void(Context&)> done) {
    v.task = std::move(t);
    v.onDone = std::move(done);
    v.runner.reset();
    v.runner.start(*v.task, step::Runner::Config{200, 25});
    go(v, Screen::Running);
    barFor(v);
}
// Dalla schermata "in corso" a quella del risultato.
void leaveRunning(View& v) { if (v.screen == Screen::Running) { v.screen = v.back.empty() ? Screen::Lists : v.back.back(); if (!v.back.empty()) v.back.pop_back(); } }
void showResults(View& v) { leaveRunning(v); go(v, Screen::Results); barFor(v); }

// Save Res (Q12): i valori in colonna nella lista scelta (0 = None).
void saveRes(Context& ctx, int choice, const std::vector<double>& values) {
    if (choice <= 0 || values.empty()) return;
    ctx.ws.setList(static_cast<std::size_t>(choice - 1), values);
    ctx.ws.statistics.listFormula[static_cast<std::size_t>(choice - 1)].clear();
    ctx.markDirty();
}
void storeVars(Context& ctx, const std::vector<std::pair<std::uint8_t, double>>& vars) { ctx.ws.statistics.store(vars); ctx.markDirty(); }

// ---------------------------------------------------------------- CALC (C01-C05, R01-R18)
void showOneVar(Context& ctx, View& v, std::size_t xList, std::uint8_t freq) {
    const stats::OneVar o = stats::oneVarEx(ctx.ws.lists[xList], freqList(ctx, freq), ctx.ws.statistics.quartiles);
    v.results.clear();
    v.results.title = "1-Variable  (List " + std::to_string(xList + 1) + ")";
    auto add = [&](const char* l, double x) { v.results.add(l, fmt(ctx, x)); };
    add("\xEE\x80\x81", o.mean); add("\xCE\xA3x", o.sum); add("\xCE\xA3x\xC2\xB2", o.sumSq); add("\xCF\x83x", o.popSd); add("sx", o.sampleSd); add("n", o.n);
    add("minX", o.min); add("Q1", o.q1); add("Med", o.median); add("Q3", o.q3); add("maxX", o.max);
    std::string modes;
    for (std::size_t i = 0; i < o.modes.size() && i < 4; ++i) { if (i) modes += ", "; modes += fmt(ctx, o.modes[i]); }
    if (o.modes.size() > 4) modes += ", ...";
    v.results.add("Mod", modes); add("Mod:n", static_cast<double>(o.modes.size())); add("Mod:F", o.modeCount);
    std::vector<std::pair<std::uint8_t, double>> vars;
    stats::oneVarVars(o, vars);
    storeVars(ctx, vars);
    v.kind = View::Kind::OneVar;
    showResults(v);
}
void showTwoVar(Context& ctx, View& v, std::size_t xl, std::size_t yl, std::uint8_t freq) {
    const stats::TwoVar t = stats::twoVarEx(ctx.ws.lists[xl], ctx.ws.lists[yl], freqList(ctx, freq));
    v.results.clear();
    v.results.title = "2-Variable  (List " + std::to_string(xl + 1) + ", List " + std::to_string(yl + 1) + ")";
    auto add = [&](const char* l, double x) { v.results.add(l, fmt(ctx, x)); };
    add("\xEE\x80\x81", t.meanX); add("\xCE\xA3x", t.sumX); add("\xCE\xA3x\xC2\xB2", t.sumX2); add("\xCF\x83x", t.popSdX); add("sx", t.sampleSdX); add("n", t.n);
    add("\xEE\x80\x82", t.meanY); add("\xCE\xA3y", t.sumY); add("\xCE\xA3y\xC2\xB2", t.sumY2); add("\xCF\x83y", t.popSdY); add("sy", t.sampleSdY);
    add("\xCE\xA3xy", t.sumXY); add("minX", t.minX); add("maxX", t.maxX); add("minY", t.minY); add("maxY", t.maxY);
    std::vector<std::pair<std::uint8_t, double>> vars;
    stats::twoVarVars(t, vars);
    storeVars(ctx, vars);
    v.kind = View::Kind::TwoVar;
    showResults(v);
}
void startRegression(Context& ctx, View& v, Reg type, std::size_t xl, std::size_t yl, std::uint8_t freq) {
    v.fitX = ctx.ws.lists[xl]; v.fitY = ctx.ws.lists[yl];
    v.fitF = freq ? ctx.ws.lists[freq - 1] : std::vector<double>{};
    v.fitXList = xl; v.fitYList = yl;
    auto* task = new stats::FitTask(type, v.fitX, v.fitY, v.fitF);
    View* self = &v;
    startTask(v, std::unique_ptr<step::Task>(task), nullptr);
    v.onDone = [self, task, type](Context& c) {
        View& w = *self;
        w.fit = task->result();
        c.ws.settings.statRegType = static_cast<std::uint8_t>(type);
        std::vector<std::pair<std::uint8_t, double>> vars;
        stats::fitVars(w.fit, vars);
        storeVars(c, vars);
        // R14: residui nella lista del SET UP Resid List.
        if (c.ws.statistics.residList > 0) saveRes(c, c.ws.statistics.residList, stats::residuals(w.fit, w.fitX, w.fitY));
        w.results.clear();
        w.results.title = std::string(stats::regTitle(type)) + "  " + stats::regModel(type);
        for (const auto& [label, value] : stats::fitRows(w.fit)) w.results.add(label, fmt(c, value));
        w.kind = View::Kind::Reg;
        showResults(w);
    };
}

// ---------------------------------------------------------------- moduli di TEST, INTR, DIST
bool isDistForm(int kind) { return kind >= ActDistBase; }
dist::Kind distKindOf(int kind) { return dist::kindAt(static_cast<std::size_t>(kind - ActDistBase)); }
std::string formKey(int kind) { return std::to_string(kind) + ":"; }
int fieldIndex(const Form& f, const std::string& label) {
    for (std::size_t i = 0; i < f.fields.size(); ++i) if (f.fields[i].label == label) return static_cast<int>(i);
    return -1;
}
int choiceOf(const Form& f, const std::string& label, int def = 0) { const int i = fieldIndex(f, label); return i < 0 ? def : f.fields[static_cast<std::size_t>(i)].choice; }
double numberOf(Context& ctx, const Form& f, const std::string& label) {
    const int i = fieldIndex(f, label);
    if (i < 0) throw MathError("Argument ERROR: campo " + label);
    return evalNumber(ctx, f.fields[static_cast<std::size_t>(i)].text);
}
void remember(View& v) {
    for (const FormField& f : v.form.fields) v.remembered[formKey(v.formKind) + f.label] = {f.text, f.choice};
}
void restore(View& v) {
    for (FormField& f : v.form.fields) {
        const auto it = v.remembered.find(formKey(v.formKind) + f.label);
        if (it == v.remembered.end()) continue;
        if (f.type == FormField::Type::Choice) {
            if (it->second.second >= 0 && static_cast<std::size_t>(it->second.second) < f.choices.size()) { f.choice = it->second.second; f.text = f.choices[static_cast<std::size_t>(f.choice)]; }
        } else f.text = it->second.first;
    }
}
int rememberedChoice(View& v, const std::string& label, int def) {
    const auto it = v.remembered.find(formKey(v.formKind) + label);
    return it == v.remembered.end() ? def : it->second.second;
}
void buildForm(View& v) {
    const int k = v.formKind;
    std::vector<FormField> f;
    auto num = [&](const char* label, const char* def) { f.push_back(numberField(label, def)); };
    auto lst = [&](const char* label, int def) { f.push_back(choiceField(label, listNames(nullptr), def)); };
    auto frq = [&](const char* label) { f.push_back(choiceField(label, listNames("1"), 0)); };
    auto save = [&]() { f.push_back(choiceField("Save Res", listNames("None"), 0)); };
    auto data = [&](int def) { const int c = rememberedChoice(v, "Data", def); f.push_back(choiceField("Data", {"List", "Variable"}, c)); return c == 0; };
    if (isDistForm(k)) {
        const dist::Kind dk = distKindOf(k);
        const bool list = data(1);
        if (dist::hasTail(dk)) f.push_back(choiceField("Tail", {"Left", "Right", "Central"}, 0));
        const auto spec = dist::paramSpecs(dk);
        const std::size_t args = dist::argCount(dk);
        if (list) { int d = 0; for (const char* l : dist::listLabels(dk)) lst(l, d++); }
        for (std::size_t i = list ? args : 0; i < spec.size(); ++i) num(spec[i].label, formNumber(spec[i].def).c_str());
        save();
    } else {
        switch (k) {
        case TZ1S: { const bool l = data(0); f.push_back(tailField("\xCE\xBC", "", "\xCE\xBC" "0", 0)); num("\xCE\xBC" "0", "0"); num("\xCF\x83", "1");
                     if (l) { lst("List", 0); frq("Freq"); } else { num("\xEE\x80\x81", "0"); num("n", "10"); } save(); break; }
        case TZ2S: { const bool l = data(0); f.push_back(tailField("\xCE\xBC" "1", "", "\xCE\xBC" "2", 0)); num("\xCF\x83" "1", "1"); num("\xCF\x83" "2", "1");
                     if (l) { lst("List(1)", 0); lst("List(2)", 1); frq("Freq(1)"); frq("Freq(2)"); }
                     else { num("\xEE\x80\x81" "1", "0"); num("n1", "10"); num("\xEE\x80\x81" "2", "0"); num("n2", "10"); }
                     save(); break; }
        case TZ1P: f.push_back(tailField("Prop", "", "p0", 0)); num("p0", "0.5"); num("x", "5"); num("n", "10"); save(); break;
        case TZ2P: f.push_back(tailField("p1", "", "p2", 0)); num("x1", "5"); num("n1", "10"); num("x2", "5"); num("n2", "10"); save(); break;
        case TT1S: { const bool l = data(0); f.push_back(tailField("\xCE\xBC", "", "\xCE\xBC" "0", 0)); num("\xCE\xBC" "0", "0");
                     if (l) { lst("List", 0); frq("Freq"); } else { num("\xEE\x80\x81", "0"); num("sx", "1"); num("n", "10"); } save(); break; }
        case TT2S: case IT2S: {
            const bool l = data(0);
            if (k == TT2S) f.push_back(tailField("\xCE\xBC" "1", "", "\xCE\xBC" "2", 0)); else num("C-Level", "0.95");
            if (l) { lst("List(1)", 0); lst("List(2)", 1); frq("Freq(1)"); frq("Freq(2)"); }
            else { num("\xEE\x80\x81" "1", "0"); num("sx1", "1"); num("n1", "10"); num("\xEE\x80\x81" "2", "0"); num("sx2", "1"); num("n2", "10"); }
            f.push_back(choiceField("Pooled", {"Off", "On"}, 0)); save(); break;
        }
        case TTReg: lst("XList", 0); lst("YList", 1); frq("Freq"); f.push_back(tailField("\xCE\xB2 & \xCF\x81", "", "0", 0)); save(); break;
        case TChiGof: lst("Observed", 0); lst("Expected", 1); num("df", "1"); f.push_back(choiceField("CNTRB", listNames("None"), 3)); save(); break;
        case TChi2W: f.push_back(choiceField("Observed", matNames(), 0)); f.push_back(choiceField("Expected", matNames(), 1)); save(); break;
        case TF2S: { const bool l = data(0); f.push_back(tailField("\xCF\x83" "1", "", "\xCF\x83" "2", 0));
                     if (l) { lst("List(1)", 0); lst("List(2)", 1); frq("Freq(1)"); frq("Freq(2)"); }
                     else { num("sx1", "1"); num("n1", "10"); num("sx2", "1"); num("n2", "10"); }
                     save(); break; }
        case TAnova: { const int many = rememberedChoice(v, "How Many", 0); f.push_back(choiceField("How Many", {"1", "2"}, many));
                       lst("Factor A", 0); if (many == 1) lst("Factor B", 1); lst("Dependent", many == 1 ? 2 : 1); save(); break; }
        case IZ1S: { const bool l = data(0); num("C-Level", "0.95"); num("\xCF\x83", "1");
                     if (l) { lst("List", 0); frq("Freq"); } else { num("\xEE\x80\x81", "0"); num("n", "10"); } save(); break; }
        case IZ2S: { const bool l = data(0); num("C-Level", "0.95"); num("\xCF\x83" "1", "1"); num("\xCF\x83" "2", "1");
                     if (l) { lst("List(1)", 0); lst("List(2)", 1); frq("Freq(1)"); frq("Freq(2)"); }
                     else { num("\xEE\x80\x81" "1", "0"); num("n1", "10"); num("\xEE\x80\x81" "2", "0"); num("n2", "10"); }
                     save(); break; }
        case IZ1P: num("C-Level", "0.95"); num("x", "5"); num("n", "10"); save(); break;
        case IZ2P: num("C-Level", "0.95"); num("x1", "5"); num("n1", "10"); num("x2", "5"); num("n2", "10"); save(); break;
        case IT1S: { const bool l = data(0); num("C-Level", "0.95");
                     if (l) { lst("List", 0); frq("Freq"); } else { num("\xEE\x80\x81", "0"); num("sx", "1"); num("n", "10"); } save(); break; }
        default: break;
        }
    }
    const int selected = v.form.selected;
    v.form.fields = std::move(f);
    restore(v);
    v.form.selected = std::min(std::max(0, selected), static_cast<int>(v.form.fields.size()) - 1);
}
void openForm(View& v, int kind) {
    if (v.formKind >= 0 && !v.form.fields.empty()) remember(v);
    v.formKind = kind;
    v.form.selected = 0;
    buildForm(v);
    go(v, Screen::Form);
    v.shell.fkeys.set(isDistForm(kind) && !dist::continuous(distKindOf(kind).family) ? &kFormCalcMenu
                      : (kind == TAnova || kind == TTReg || kind >= IZ1S) && !isDistForm(kind) ? &kFormCalcMenu : &kFormMenu);
}
// Il cambio di Data o di How Many ricostruisce il modulo (campi diversi).
void formChanged(View& v) {
    const int data = fieldIndex(v.form, "Data"), many = fieldIndex(v.form, "How Many");
    const int oldData = rememberedChoice(v, "Data", -1), oldMany = rememberedChoice(v, "How Many", -1);
    remember(v);
    if ((data >= 0 && v.form.fields[static_cast<std::size_t>(data)].choice != oldData && oldData >= 0) ||
        (many >= 0 && v.form.fields[static_cast<std::size_t>(many)].choice != oldMany && oldMany >= 0)) buildForm(v);
}
stats::Sample sampleOf(Context& ctx, const Form& f, const std::string& sfx, bool withSd) {
    stats::Sample s;
    s.list = choiceOf(f, "Data") == 0;
    if (s.list) {
        s.x = ctx.ws.lists[static_cast<std::size_t>(choiceOf(f, "List" + (sfx.empty() ? std::string() : "(" + sfx + ")")))];
        const int fr = choiceOf(f, "Freq" + (sfx.empty() ? std::string() : "(" + sfx + ")"));
        if (fr > 0) s.f = ctx.ws.lists[static_cast<std::size_t>(fr - 1)];
    } else {
        if (fieldIndex(f, "\xEE\x80\x81" + sfx) >= 0) s.mean = numberOf(ctx, f, "\xEE\x80\x81" + sfx);
        if (withSd) s.sd = numberOf(ctx, f, "sx" + sfx);
        s.n = numberOf(ctx, f, "n" + sfx);
    }
    return s;
}
void showOutcome(Context& ctx, View& v, const stats::Outcome& o, int save) {
    v.outcome = o;
    v.results.clear();
    v.results.title = o.title;
    std::vector<double> values;
    for (const stats::Row& r : o.rows) { v.results.add(r.label, fmt(ctx, r.value)); values.push_back(r.value); }
    storeVars(ctx, o.vars);
    saveRes(ctx, save, values);
    v.kind = View::Kind::Outcome;
    showResults(v);
}
void runTestForm(Context& ctx, View& v) {
    remember(v);
    const Form& f = v.form;
    const int k = v.formKind;
    const int save = choiceOf(f, "Save Res");
    auto tail = [&](const char* label) { return tailOf(choiceOf(f, label)); };
    auto n = [&](const char* label) { return numberOf(ctx, f, label); };
    View* self = &v;
    if (k == TAnova) {
        const bool two = choiceOf(f, "How Many") == 1;
        const auto& A = ctx.ws.lists[static_cast<std::size_t>(choiceOf(f, "Factor A"))];
        const std::vector<double> B = two ? ctx.ws.lists[static_cast<std::size_t>(choiceOf(f, "Factor B"))] : std::vector<double>{};
        const auto& Y = ctx.ws.lists[static_cast<std::size_t>(choiceOf(f, "Dependent"))];
        auto* task = new stats::AnovaTask(A, B, Y);
        startTask(v, std::unique_ptr<step::Task>(task), [self, task, save](Context& c) {
            self->anova = task->table();
            showOutcome(c, *self, task->outcome(), save);
            self->kind = View::Kind::Anova;
            barFor(*self);
        });
        return;
    }
    // Gli altri test sono brevi (una formula chiusa): calcolo diretto.
    stats::Outcome out;
    std::vector<double> expected;
    std::size_t rows = 0, cols = 0;
    std::function<void()> fn;
    switch (k) {
    case TZ1S: { const auto s = sampleOf(ctx, f, "", false); const double mu = n("\xCE\xBC" "0"), sg = n("\xCF\x83"); const int t = tail("\xCE\xBC");
                 fn = [&out, s, mu, sg, t] { out = stats::zTest1S(s, mu, sg, t); }; break; }
    case TZ2S: { const auto a = sampleOf(ctx, f, "1", false), b = sampleOf(ctx, f, "2", false); const double s1 = n("\xCF\x83" "1"), s2 = n("\xCF\x83" "2"); const int t = tail("\xCE\xBC" "1");
                 fn = [&out, a, b, s1, s2, t] { out = stats::zTest2S(a, b, s1, s2, t); }; break; }
    case TZ1P: { const double p0 = n("p0"), x = n("x"), nn = n("n"); const int t = tail("Prop"); fn = [&out, p0, x, nn, t] { out = stats::zTest1P(p0, x, nn, t); }; break; }
    case TZ2P: { const double x1 = n("x1"), n1 = n("n1"), x2 = n("x2"), n2 = n("n2"); const int t = tail("p1"); fn = [&out, x1, n1, x2, n2, t] { out = stats::zTest2P(x1, n1, x2, n2, t); }; break; }
    case TT1S: { const auto s = sampleOf(ctx, f, "", true); const double mu = n("\xCE\xBC" "0"); const int t = tail("\xCE\xBC"); fn = [&out, s, mu, t] { out = stats::tTest1S(s, mu, t); }; break; }
    case TT2S: { const auto a = sampleOf(ctx, f, "1", true), b = sampleOf(ctx, f, "2", true); const bool pooled = choiceOf(f, "Pooled") == 1; const int t = tail("\xCE\xBC" "1");
                 fn = [&out, a, b, pooled, t] { out = stats::tTest2S(a, b, pooled, t); }; break; }
    case TTReg: { const auto x = ctx.ws.lists[static_cast<std::size_t>(choiceOf(f, "XList"))], y = ctx.ws.lists[static_cast<std::size_t>(choiceOf(f, "YList"))];
                  const int fr = choiceOf(f, "Freq"); const std::vector<double> fq = fr ? ctx.ws.lists[static_cast<std::size_t>(fr - 1)] : std::vector<double>{};
                  const int t = tail("\xCE\xB2 & \xCF\x81");
                  fn = [&out, x, y, fq, t] { out = stats::tTestReg(x, y, fq.empty() ? nullptr : &fq, t); }; break; }
    case TChiGof: { const auto o = ctx.ws.lists[static_cast<std::size_t>(choiceOf(f, "Observed"))], e = ctx.ws.lists[static_cast<std::size_t>(choiceOf(f, "Expected"))];
                    const double df = n("df"); fn = [&out, o, e, df] { out = stats::chiGof(o, e, df); }; break; }
    case TChi2W: {
        CalcValue m;
        if (!ctx.ws.collection(CollectionResolver::Memory::Mat, choiceOf(f, "Observed"), m) || m.type != ValueType::Matrix)
            throw MathError(std::string("Dimension ERROR: ") + "Mat " + static_cast<char>('A' + choiceOf(f, "Observed")) + " non definita");
        if (m.complex()) throw MathError("Non-Real ERROR: matrice complessa");
        const std::vector<double> obs = m.re; rows = m.rows; cols = m.cols;
        fn = [&out, &expected, obs, rows, cols] { out = stats::chi2Way(obs, rows, cols, expected); };
        break;
    }
    case TF2S: { const auto a = sampleOf(ctx, f, "1", true), b = sampleOf(ctx, f, "2", true); const int t = tail("\xCF\x83" "1"); fn = [&out, a, b, t] { out = stats::fTest2S(a, b, t); }; break; }
    case IZ1S: { const auto s = sampleOf(ctx, f, "", false); const double c = n("C-Level"), sg = n("\xCF\x83"); fn = [&out, s, c, sg] { out = stats::zInt1S(s, c, sg); }; break; }
    case IZ2S: { const auto a = sampleOf(ctx, f, "1", false), b = sampleOf(ctx, f, "2", false); const double c = n("C-Level"), s1 = n("\xCF\x83" "1"), s2 = n("\xCF\x83" "2");
                 fn = [&out, a, b, c, s1, s2] { out = stats::zInt2S(a, b, c, s1, s2); }; break; }
    case IZ1P: { const double c = n("C-Level"), x = n("x"), nn = n("n"); fn = [&out, c, x, nn] { out = stats::zInt1P(c, x, nn); }; break; }
    case IZ2P: { const double c = n("C-Level"), x1 = n("x1"), n1 = n("n1"), x2 = n("x2"), n2 = n("n2"); fn = [&out, c, x1, n1, x2, n2] { out = stats::zInt2P(c, x1, n1, x2, n2); }; break; }
    case IT1S: { const auto s = sampleOf(ctx, f, "", true); const double c = n("C-Level"); fn = [&out, s, c] { out = stats::tInt1S(s, c); }; break; }
    case IT2S: { const auto a = sampleOf(ctx, f, "1", true), b = sampleOf(ctx, f, "2", true); const double c = n("C-Level"); const bool pooled = choiceOf(f, "Pooled") == 1;
                 fn = [&out, a, b, c, pooled] { out = stats::tInt2S(a, b, c, pooled); }; break; }
    default: return;
    }
    fn();
    if (k == TChiGof) {
        const int cntrb = choiceOf(f, "CNTRB");
        if (cntrb > 0) saveRes(ctx, cntrb, out.listOut);
    }
    if (k == TChi2W) {
        CalcValue e = CalcValue::matrix(rows, cols);
        e.re = expected;
        ctx.ws.storeMatrix(static_cast<std::size_t>(choiceOf(f, "Expected")), e);
        ctx.markDirty();
    }
    showOutcome(ctx, v, out, save);
}
void runDistForm(Context& ctx, View& v) {
    remember(v);
    const Form& f = v.form;
    const dist::Kind dk = distKindOf(v.formKind);
    const auto spec = dist::paramSpecs(dk);
    const std::size_t args = dist::argCount(dk);
    const bool list = choiceOf(f, "Data") == 0;
    const int save = choiceOf(f, "Save Res");
    dist::Input in;
    in.tail = static_cast<dist::Tail>(choiceOf(f, "Tail"));
    for (std::size_t i = 0; i < spec.size(); ++i) in.values.push_back(list && i < args ? 0.0 : numberOf(ctx, f, spec[i].label));
    View* self = &v;
    if (list) {
        const auto labels = dist::listLabels(dk);
        std::vector<double> first = ctx.ws.lists[static_cast<std::size_t>(choiceOf(f, labels[0]))];
        std::vector<double> second = labels.size() > 1 ? ctx.ws.lists[static_cast<std::size_t>(choiceOf(f, labels[1]))] : std::vector<double>{};
        auto* task = new dist::ListTask(dk, in, std::move(first), std::move(second));
        startTask(v, std::unique_ptr<step::Task>(task), [self, task, dk, save](Context& c) {
            View& w = *self;
            const std::vector<double>& out = task->results();
            CalcValue lv = CalcValue::list(out.size());
            lv.re = out;
            c.ws.storeCollectionAnswer(lv);                              // ListAns (D09)
            saveRes(c, save, out);
            w.results.clear();
            w.results.title = std::string(dist::kindName(dk)) + (dk.func == Func::Inv ? "  xInv" : "  p");
            for (std::size_t i = 0; i < out.size(); ++i) w.results.add(std::to_string(i + 1), fmt(c, out[i]));
            w.kind = View::Kind::DistList;
            w.distGraph = {};
            c.markDirty();
            showResults(w);
        });
        return;
    }
    auto result = std::make_shared<dist::Result>();
    startTask(v, std::make_unique<FnTask>([result, dk, in] { *result = dist::compute(dk, in); }), [self, result, dk, in, save](Context& c) {
        View& w = *self;
        w.results.clear();
        w.results.title = dist::kindName(dk);
        std::vector<double> values;
        w.graphLines.clear();
        for (const auto& [label, value] : result->rows) { w.results.add(label, fmt(c, value)); values.push_back(value); w.graphLines.push_back(label + "=" + fmt(c, value)); }
        storeVars(c, result->vars);
        c.ws.memory.setAnswer(result->main);                         // D12 / Q11
        saveRes(c, save, values);
        w.distGraph = dist::graphModel(dk, in, *result);
        w.kind = View::Kind::Dist;
        c.markDirty();
        showResults(w);
    });
}
} // namespace

namespace {
// ---------------------------------------------------------------- grafici (G01-G19)
statplot::Window viewWindow(const ViewWindow& w) {
    statplot::Window o;
    o.xmin = w.xmin; o.xmax = w.xmax; o.xscl = w.xscl; o.ymin = w.ymin; o.ymax = w.ymax; o.yscl = w.yscl;
    return o;
}
void continueDraw(Context& ctx, View& v);
void openHistAsk(Context& ctx, View& v, std::size_t gi) {
    const stats::StatGraph& g = ctx.ws.statistics.graphs[gi];
    const auto& x = ctx.ws.lists[g.xList];
    if (x.empty()) throw DimensionError("Dimension ERROR: List " + std::to_string(g.xList + 1) + " vuota");
    double start = g.hStart, width = g.hWidth;
    if (!g.hSet) {
        const auto mm = std::minmax_element(x.begin(), x.end());
        start = *mm.first;
        width = *mm.second > *mm.first ? (*mm.second - *mm.first) / 10 : 1;
    }
    v.form.fields = {numberField("Start", formNumber(start)), numberField("Width", formNumber(width))};
    v.form.selected = 0;
    v.histGraph = gi;
    go(v, Screen::HistAsk);
    barFor(v);
}
void startDraw(Context& ctx, View& v, std::vector<std::size_t> idx) {
    if (idx.empty()) throw MathError("Argument ERROR: nessuno StatGraph in DrawOn (SELECT)");
    v.pendingDraw = std::move(idx);
    v.histAsked = 0;
    v.overlay = false;
    continueDraw(ctx, v);
}
void continueDraw(Context& ctx, View& v) {
    auto& graphs = ctx.ws.statistics.graphs;
    for (std::size_t gi : v.pendingDraw) {
        const GraphType t = graphs[gi].type;
        if ((t == GraphType::Hist || t == GraphType::Broken) && !(v.histAsked & (1u << gi))) { openHistAsk(ctx, v, gi); return; }
    }
    std::vector<statplot::Plot> plots;
    for (std::size_t gi : v.pendingDraw) plots.push_back(statplot::build(ctx.ws, gi, graphs[gi], ctx.ws.statistics.quartiles));
    std::vector<std::pair<std::uint8_t, double>> vars;
    for (const auto& p : plots)
        if (p.g.type == GraphType::Hist || p.g.type == GraphType::Broken) { vars.emplace_back(stats::VHStart, p.hist.start); vars.emplace_back(stats::VHPitch, p.hist.width); }
    if (!vars.empty()) storeVars(ctx, vars);
    v.plots = std::move(plots);
    v.window = ctx.ws.statistics.statWindManual ? viewWindow(ctx.ws.view) : statplot::autoWindow(v.plots);
    v.tracing = false;
    v.tracePlot = v.traceIndex = 0;
    go(v, Screen::Graph);
    barFor(v);
}
// DRAW della regressione: punti delle liste usate e curva sopra (R13, G17).
void drawRegression(Context& ctx, View& v) {
    stats::StatGraph g;
    g.type = GraphType::Scatter;
    g.xList = static_cast<std::uint8_t>(v.fitXList); g.yList = static_cast<std::uint8_t>(v.fitYList);
    g.color = 0;
    statplot::Plot p = statplot::build(ctx.ws, 0, g, ctx.ws.statistics.quartiles);
    p.f = v.fitF;
    v.plots = {std::move(p)};
    v.window = ctx.ws.statistics.statWindManual ? viewWindow(ctx.ws.view) : statplot::autoWindow(v.plots);
    v.overlay = true;
    v.tracing = false;
    go(v, Screen::Graph);
    barFor(v);
}
void drawResult(View& v) {
    if (v.kind == View::Kind::Reg) return;
    if (v.kind == View::Kind::Anova && v.anova.twoWay) { go(v, Screen::Interaction); barFor(v); return; }
    if (v.kind == View::Kind::Outcome && v.outcome.curve != stats::Curve::None) {
        v.distGraph = dist::testGraph(v.outcome);
        v.graphLines.clear();
        for (std::size_t i = 0; i < v.outcome.rows.size() && i < 2; ++i) {
            char b[64];
            std::snprintf(b, sizeof b, "%s=%s", v.outcome.rows[i].label.c_str(), formatReal(v.outcome.rows[i].value, {DisplayMode::Sci, 6}).c_str());
            v.graphLines.push_back(b);
        }
    }
    if (!v.distGraph.available) return;
    go(v, Screen::DistGraph);
    barFor(v);
}
void traceUpdate(View& v) {
    if (v.plots.empty()) { v.tracing = false; return; }
    v.tracePlot = std::min(v.tracePlot, v.plots.size() - 1);
    const std::size_t n = statplot::traceCount(v.plots[v.tracePlot]);
    if (n == 0) { v.traceLabel = "TRACE: nessun punto"; return; }
    v.traceIndex = std::min(v.traceIndex, n - 1);
    double x = 0, y = 0;
    (void)statplot::tracePoint(v.plots[v.tracePlot], v.traceIndex, x, y, v.traceLabel);
}

// ---------------------------------------------------------------- SET di StatGraph (G02, R16), SELECT (G03), SET di CALC, SET UP
std::vector<std::string> graphTypeNames() {
    std::vector<std::string> v;
    for (int i = 0; i < static_cast<int>(GraphType::Count); ++i) v.push_back(stats::graphTypeName(static_cast<GraphType>(i)));
    return v;
}
void buildSetForm(Context& ctx, View& v) {
    const stats::StatGraph& g = ctx.ws.statistics.graphs[static_cast<std::size_t>(v.setGraph)];
    const GraphType t = g.type;
    std::vector<FormField> f;
    f.push_back(choiceField("Graph Type", graphTypeNames(), static_cast<int>(t)));
    if (t == GraphType::Pie) {
        f.push_back(choiceField("Data", listNames(nullptr), g.pieData));
        f.push_back(choiceField("Display", {"%", "Data", "None"}, g.pieDisplay));
        f.push_back(choiceField("% Sto List", listNames("None"), g.pieStore));
    } else if (t == GraphType::Bar) {
        f.push_back(choiceField("Data1", listNames(nullptr), std::max(0, g.bar[0] - 1)));
        f.push_back(choiceField("Data2", listNames("None"), g.bar[1]));
        f.push_back(choiceField("Data3", listNames("None"), g.bar[2]));
        f.push_back(choiceField("Stick Style", {"Length", "Horiz"}, g.stick));
        f.push_back(choiceField("Graph Color", paletteNames(), g.color));
    } else {
        f.push_back(choiceField("XList", listNames(nullptr), g.xList));
        if (stats::graphNeedsY(t)) f.push_back(choiceField("YList", listNames(nullptr), g.yList));
        f.push_back(choiceField("Frequency", listNames("1"), g.freq));
        if (t == GraphType::Scatter || t == GraphType::XYLine || t == GraphType::NPPlot || stats::graphIsRegression(t))
            f.push_back(choiceField("Mark Type", {"Square", "Cross", "Dot", "Circle"}, g.mark));
        if (t == GraphType::Scatter || t == GraphType::XYLine) f.push_back(choiceField("Color Link", {"Off", "X&Freq", "OnData"}, g.colorLink));
        f.push_back(choiceField("Graph Color", paletteNames(), g.color));
        if (t == GraphType::MedBox) f.push_back(choiceField("Outliers", {"Off", "On"}, g.outliers ? 1 : 0));
    }
    const int sel = v.form.selected;
    v.form.fields = std::move(f);
    v.form.selected = std::min(std::max(0, sel), static_cast<int>(v.form.fields.size()) - 1);
}
void applySetForm(Context& ctx, View& v) {
    stats::StatGraph& g = ctx.ws.statistics.graphs[static_cast<std::size_t>(v.setGraph)];
    const stats::StatGraph before = g;
    const Form& f = v.form;
    g.type = static_cast<GraphType>(choiceOf(f, "Graph Type"));
    auto c8 = [&](const char* label, std::uint8_t& out, int add = 0) { const int i = fieldIndex(f, label); if (i >= 0) out = static_cast<std::uint8_t>(f.fields[static_cast<std::size_t>(i)].choice + add); };
    c8("XList", g.xList); c8("YList", g.yList); c8("Frequency", g.freq); c8("Mark Type", g.mark); c8("Color Link", g.colorLink); c8("Graph Color", g.color);
    c8("Data", g.pieData); c8("Display", g.pieDisplay); c8("% Sto List", g.pieStore);
    c8("Data1", g.bar[0], 1); c8("Data2", g.bar[1]); c8("Data3", g.bar[2]); c8("Stick Style", g.stick);
    if (fieldIndex(f, "Outliers") >= 0) g.outliers = choiceOf(f, "Outliers") == 1;
    if (!(g == before)) ctx.markDirty();
    if (g.type != before.type) buildSetForm(ctx, v);          // voci diverse per il tipo nuovo
}
void openSet(Context& ctx, View& v, int graph) {
    v.setGraph = graph;
    v.form.selected = 0;
    buildSetForm(ctx, v);
    if (v.screen != Screen::Set) go(v, Screen::Set);
    barFor(v);
}
void buildSelectForm(Context& ctx, View& v) {
    v.form.fields.clear();
    for (std::size_t i = 0; i < 3; ++i)
        v.form.fields.push_back(choiceField("StatGraph" + std::to_string(i + 1), {"DrawOn", "DrawOff"}, ctx.ws.statistics.graphs[i].on ? 0 : 1));
}
void applySelectForm(Context& ctx, View& v) {
    for (std::size_t i = 0; i < 3 && i < v.form.fields.size(); ++i) {
        const bool on = v.form.fields[i].choice == 0;
        if (ctx.ws.statistics.graphs[i].on != on) { ctx.ws.statistics.graphs[i].on = on; ctx.markDirty(); }
    }
}
void openCalcSet(Context& ctx, View& v) {
    const stats::PlusState& s = ctx.ws.statistics;
    v.form.fields = {choiceField("1Var XList", listNames(nullptr), s.var1X), choiceField("1Var Freq", listNames("1"), s.var1Freq),
                     choiceField("2Var XList", listNames(nullptr), ctx.ws.settings.statXList), choiceField("2Var YList", listNames(nullptr), ctx.ws.settings.statYList),
                     choiceField("2Var Freq", listNames("1"), s.var2Freq)};
    v.form.selected = 0;
    go(v, Screen::CalcSet);
    barFor(v);
}
void applyCalcSet(Context& ctx, View& v) {
    stats::PlusState& s = ctx.ws.statistics;
    s.var1X = static_cast<std::uint8_t>(choiceOf(v.form, "1Var XList"));
    s.var1Freq = static_cast<std::uint8_t>(choiceOf(v.form, "1Var Freq"));
    ctx.ws.settings.statXList = static_cast<std::uint8_t>(choiceOf(v.form, "2Var XList"));
    ctx.ws.settings.statYList = static_cast<std::uint8_t>(choiceOf(v.form, "2Var YList"));
    s.var2Freq = static_cast<std::uint8_t>(choiceOf(v.form, "2Var Freq"));
    ctx.markDirty();
}
void openSetup(Context& ctx, View& v) {
    const stats::PlusState& s = ctx.ws.statistics;
    const Settings& st = ctx.ws.settings;
    std::vector<std::string> files;
    for (int i = 1; i <= 6; ++i) files.push_back("File " + std::to_string(i));
    const int angle = st.angle == AngleMode::Degrees ? 0 : st.angle == AngleMode::Radians ? 1 : 2;
    v.form.fields = {choiceField("Stat Wind", {"Auto", "Manual"}, s.statWindManual ? 1 : 0), choiceField("Resid List", listNames("None"), s.residList),
                     choiceField("Q1Q3 Type", {"Std", "OnData"}, static_cast<int>(s.quartiles)), choiceField("List File", files, st.listFile),
                     choiceField("Sub Name", {"On", "Off"}, st.subName ? 0 : 1), choiceField("Angle", {"Deg", "Rad", "Gra"}, angle),
                     choiceField("Display", {"Norm1", "Norm2", "Fix", "Sci", "Eng"}, static_cast<int>(st.display.mode))};
    v.form.selected = 0;
    go(v, Screen::Setup);
    barFor(v);
}
void applySetup(Context& ctx, View& v) {
    stats::PlusState& s = ctx.ws.statistics;
    Settings& st = ctx.ws.settings;
    s.statWindManual = choiceOf(v.form, "Stat Wind") == 1;
    s.residList = static_cast<std::uint8_t>(choiceOf(v.form, "Resid List"));
    s.quartiles = static_cast<stats::QuartileRule>(choiceOf(v.form, "Q1Q3 Type"));
    const int file = choiceOf(v.form, "List File");
    if (file != st.listFile) ctx.ws.selectListFile(static_cast<std::size_t>(file));
    st.subName = choiceOf(v.form, "Sub Name") == 0;
    const int a = choiceOf(v.form, "Angle");
    st.angle = a == 0 ? AngleMode::Degrees : a == 1 ? AngleMode::Radians : AngleMode::Gradians;
    st.display.mode = static_cast<DisplayMode>(choiceOf(v.form, "Display"));
    ctx.ws.apply();
    ctx.markDirty();
}
void openVWindow(Context&, View& v) {
    const statplot::Window& w = v.window;
    v.form.fields = {numberField("Xmin", formNumber(w.xmin)), numberField("Xmax", formNumber(w.xmax)), numberField("Xscale", formNumber(w.xscl)),
                     numberField("Ymin", formNumber(w.ymin)), numberField("Ymax", formNumber(w.ymax)), numberField("Yscale", formNumber(w.yscl))};
    v.form.selected = 0;
    go(v, Screen::VWindow);
    barFor(v);
}
void applyVWindow(Context& ctx, View& v) {
    statplot::Window w;
    w.xmin = numberOf(ctx, v.form, "Xmin"); w.xmax = numberOf(ctx, v.form, "Xmax"); w.xscl = numberOf(ctx, v.form, "Xscale");
    w.ymin = numberOf(ctx, v.form, "Ymin"); w.ymax = numberOf(ctx, v.form, "Ymax"); w.yscl = numberOf(ctx, v.form, "Yscale");
    if (!(w.xmin < w.xmax) || !(w.ymin < w.ymax)) throw MathError("Range ERROR: Xmin < Xmax e Ymin < Ymax");
    v.window = w;
    ViewWindow& vw = ctx.ws.view;
    vw.xmin = w.xmin; vw.xmax = w.xmax; vw.xscl = w.xscl; vw.ymin = w.ymin; vw.ymax = w.ymax; vw.yscl = w.yscl;
    ctx.markDirty();
}

// ---------------------------------------------------------------- COPY (R15)
void openCopy(Context& ctx, View& v) {
    v.copyList.items.clear();
    for (std::size_t i = 0; i < ctx.ws.functions.size(); ++i) {
        const std::string& e = ctx.ws.functions[i].expr;
        v.copyList.items.push_back("Y" + std::to_string(i + 1) + ": " + (e.empty() ? std::string() : e));
    }
    v.copyList.selected = 0;
    for (std::size_t i = 0; i < ctx.ws.functions.size(); ++i) if (ctx.ws.functions[i].expr.empty()) { v.copyList.selected = static_cast<int>(i); break; }
    go(v, Screen::Copy);
    barFor(v);
}
void copyTo(Context& ctx, View& v) {
    const std::size_t slot = static_cast<std::size_t>(v.copyList.selected);
    if (slot >= ctx.ws.functions.size()) return;
    ctx.ws.functions[slot] = emptyGraphFunction(slot);
    ctx.ws.functions[slot].expr = v.fit.expression();
    ctx.markDirty();
    ctx.toast("Copiato in Y" + std::to_string(slot + 1));
    goBack(v);
}

// ---------------------------------------------------------------- CSV (E13)
void csvList(Context& ctx, View& v, bool save) {
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) throw MathError("Storage ERROR: archivio file non disponibile in questa build");
    v.saving = save;
    if (save) { v.dialog.openName("CSV SAVE\xC2\xB7" "AS  liste 1-26", ""); return; }
    std::vector<std::string> names;
    const files::Status st = csv::list(*fs, names);
    if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
    v.dialog.openList("CSV LOAD  da List " + std::to_string(v.col + 1), std::move(names));
}
void csvChosen(Context& ctx, View& v) {
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) throw MathError("Storage ERROR: archivio file non disponibile in questa build");
    const std::string name = v.dialog.name();
    if (v.saving) {
        v.saving = false;
        const files::Status st = csv::save(*fs, name, csv::encodeLists(ctx.ws));
        if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
        ctx.toast("Salvato " + name + ".csv");
        return;
    }
    std::vector<std::uint8_t> bytes;
    const files::Status st = csv::load(*fs, name, bytes);
    if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
    const std::size_t first = static_cast<std::size_t>(v.col);
    const csv::Table t = csv::decode(bytes.data(), bytes.size(), csv::formatOf(ctx.ws), Workspace::MaxListLength, Workspace::ListCount - first);
    const std::size_t n = csv::loadLists(ctx.ws, first, t);
    for (std::size_t i = first; i < first + n && i < 26; ++i) ctx.ws.statistics.listFormula[i].clear();
    ctx.markDirty();
    recalcFormulas(ctx, false);
    ctx.toast(std::to_string(n) + (n == 1 ? " lista caricata da " : " liste caricate da ") + name + ".csv");
}
void openCsvSet(Context& ctx, View& v) {
    const Settings& st = ctx.ws.settings;
    v.form.fields = {choiceField("CSV Separator", {",", ";"}, st.csvSeparator == ';' ? 1 : 0), choiceField("CSV Decimal", {".", ","}, st.csvDecimal == ',' ? 1 : 0)};
    v.form.selected = 0;
    v.formKind = -2;                              // modulo del formato CSV (schermata Setup)
    go(v, Screen::Setup);
    barFor(v);
}
void applyCsvSet(Context& ctx, View& v) {
    const csv::Format f{choiceOf(v.form, "CSV Separator") == 1 ? ';' : ',', choiceOf(v.form, "CSV Decimal") == 1 ? ',' : '.'};
    const std::string bad = csv::validate(f);
    if (!bad.empty()) throw MathError("Argument ERROR: " + bad);
    ctx.ws.settings.csvSeparator = f.separator; ctx.ws.settings.csvDecimal = f.decimal;
    ctx.markDirty();
}

// ---------------------------------------------------------------- azioni dell'editor
bool memoryListsStrip(Context& ctx) { return ctx.inStrip(AppId::Statistics) && ctx.strip().entry == stripentry::MemoryLists; }
void calcAction(Context& ctx, View& v, int a, bool fromGraph) {
    std::size_t x1 = ctx.ws.statistics.var1X, x2 = ctx.ws.settings.statXList, y2 = ctx.ws.settings.statYList;
    std::uint8_t f1 = ctx.ws.statistics.var1Freq, f2 = ctx.ws.statistics.var2Freq;
    if (fromGraph && !v.plots.empty()) {
        const stats::StatGraph& g = v.plots.front().g;
        x1 = x2 = g.xList; y2 = g.yList; f1 = f2 = g.freq;
    }
    if (a == Act1Var) showOneVar(ctx, v, x1, f1);
    else if (a == Act2Var) showTwoVar(ctx, v, x2, y2, f2);
    else if (a >= ActRegBase && a < ActRegBase + static_cast<int>(Reg::Count)) startRegression(ctx, v, static_cast<Reg>(a - ActRegBase), x2, y2, f2);
}
void listAction(Context& ctx, View& v, int a) {
    switch (a) {
    case ActSrtA: case ActSrtD:
        v.sortDesc = a == ActSrtD; v.sortLists.clear();
        v.shell.ask.open("How Many Lists?  (1-6)", AskSortCount);
        return;
    case ActDel: deleteCell(ctx, v); return;
    case ActDelAll: v.shell.confirm.open("Delete All Data?  (List " + std::to_string(v.col + 1) + ")", 1); return;
    case ActIns: insertCell(ctx, v, false); return;
    case ActAdd: insertCell(ctx, v, true); return;
    case ActColor: v.shell.showMenu("COLOR  List", paletteNames(), 1); return;
    case ActFile: {
        std::vector<std::string> files;
        for (int i = 1; i <= 6; ++i) files.push_back("List File " + std::to_string(i));
        v.shell.showMenu("FILE", std::move(files), 2);
        return;
    }
    case ActJumpTop: v.row = 0; return;
    case ActJumpBottom: v.row = std::max(0, static_cast<int>(lengthOf(ctx, v.col)) - 1); return;
    case ActJumpRow: v.shell.ask.open("JUMP  riga", AskJumpRow); return;
    case ActCsvLoad: csvList(ctx, v, false); return;
    case ActCsvSave: csvList(ctx, v, true); return;
    case ActCsvSet: openCsvSet(ctx, v); return;
    case ActGraph1: case ActGraph2: case ActGraph3: startDraw(ctx, v, {static_cast<std::size_t>(a - ActGraph1)}); return;
    case ActSelect: buildSelectForm(ctx, v); v.form.selected = 0; go(v, Screen::Select); barFor(v); return;
    case ActGraphSet: openSet(ctx, v, 0); return;
    case ActCalcSet: openCalcSet(ctx, v); return;
    default: break;
    }
    if (a == Act1Var || a == Act2Var || (a >= ActRegBase && a < ActTestBase)) { calcAction(ctx, v, a, false); return; }
    if (a >= ActTestBase && a < ActTestBase + static_cast<int>(TestKindCount)) { openForm(v, a - ActTestBase); return; }
    if (a >= ActDistBase && a < ActDistBase + static_cast<int>(dist::KindCount)) { openForm(v, a); return; }
}
std::vector<std::size_t> selectedGraphs(Context& ctx) {
    std::vector<std::size_t> idx;
    for (std::size_t i = 0; i < 3; ++i) if (ctx.ws.statistics.graphs[i].on) idx.push_back(i);
    return idx;
}
void commitEdit(Context& ctx, View& v) {
    const std::string t = v.editor.text();
    v.editing = false;
    if (v.row == StatisticsApp::SubRow) { ctx.ws.setListName(static_cast<std::size_t>(v.col), t); ctx.markDirty(); v.row = 0; return; }
    if (v.row == StatisticsApp::HeadRow) { setFormula(ctx, v.col, t); return; }
    if (t.empty()) return;
    setCell(ctx, v, evalNumber(ctx, t));
}
void startEdit(Context& ctx, View& v, const KeyEvent* first) {
    v.editor.clear();
    if (first) v.editor.insert(std::string(1, first->ch));
    else if (v.row == StatisticsApp::SubRow) v.editor.set(ctx.ws.listName(static_cast<std::size_t>(v.col)));
    else if (v.row == StatisticsApp::HeadRow) v.editor.set(ctx.ws.statistics.listFormula[static_cast<std::size_t>(v.col)]);
    else if (static_cast<std::size_t>(v.row) < lengthOf(ctx, v.col)) v.editor.set(formatReal(ctx.ws.lists[static_cast<std::size_t>(v.col)][static_cast<std::size_t>(v.row)], {DisplayMode::Norm2, 0}));
    v.editing = true;
}
void listsKey(Context& ctx, View& v, const KeyEvent& ev) {
    if (v.editing) {
        if (ev.is(Key::Exe) || ev.is(Key::Down)) { commitEdit(ctx, v); return; }
        if (ev.is(Key::Exit)) { v.editing = false; return; }
        (void)v.editor.key(ev);
        return;
    }
    // 'LIST di eActivity: solo la modifica delle liste (la prima pagina non c'e').
    if (memoryListsStrip(ctx) && !v.shell.fkeys.inSubmenu() && v.shell.fkeys.page() == 0 &&
        (ev.is(Key::F1) || ev.is(Key::F2) || ev.is(Key::F3) || ev.is(Key::F4) || ev.is(Key::F5))) return;
    const int a = v.shell.fkeys.key(ev, ctx.shift());
    if (a == FKeyBar::Handled) return;
    if (a >= 0) { listAction(ctx, v, a); return; }
    const bool sub = ctx.ws.settings.subName;
    const int len = static_cast<int>(lengthOf(ctx, v.col));
    switch (ev.key) {
    case Key::Up:
        if (v.row > 0) --v.row;
        else if (v.row == 0) v.row = sub ? StatisticsApp::SubRow : StatisticsApp::HeadRow;
        else if (v.row == StatisticsApp::SubRow) v.row = StatisticsApp::HeadRow;
        return;
    case Key::Down:
        if (v.row == StatisticsApp::HeadRow) v.row = sub ? StatisticsApp::SubRow : 0;
        else if (v.row == StatisticsApp::SubRow) v.row = 0;
        else v.row = std::min(v.row + 1, len);
        return;
    case Key::Left: v.col = std::max(0, v.col - 1); clampRow(ctx, v); return;
    case Key::Right: v.col = std::min(25, v.col + 1); clampRow(ctx, v); return;
    case Key::PageUp: if (v.row >= 0) v.row = std::max(0, v.row - 10); return;
    case Key::PageDown: if (v.row >= 0) v.row = std::min(len, v.row + 10); return;
    case Key::Home: v.row = 0; return;
    case Key::End: v.row = std::max(0, len - 1); return;
    case Key::Exe: startEdit(ctx, v, nullptr); return;
    case Key::Char: if (!ev.ctrl) startEdit(ctx, v, &ev); return;
    case Key::Del: case Key::DelForward:
        if (v.row == StatisticsApp::SubRow) { ctx.ws.setListName(static_cast<std::size_t>(v.col), std::string()); ctx.markDirty(); }
        else if (v.row == StatisticsApp::HeadRow) setFormula(ctx, v.col, std::string());
        else deleteCell(ctx, v);
        return;
    case Key::Optn:
        v.shell.showMenu("LIST", {"CSV LOAD  (da List " + std::to_string(v.col + 1) + ")", "CSV SAVE\xC2\xB7" "AS", "CSV SET  formato", "DEL-ALL  tutte le liste"}, 3);
        return;
    case Key::Exit: ctx.home(); return;
    default: return;
    }
}
void onConfirm(Context& ctx, View& v, int tag, bool yes) {
    if (!yes) return;
    if (tag == 1) {
        ctx.ws.lists[static_cast<std::size_t>(v.col)].clear();
        ctx.ws.statistics.listFormula[static_cast<std::size_t>(v.col)].clear();
        v.row = 0;
        ctx.markDirty();
        recalcFormulas(ctx, false);
    } else if (tag == 2) {
        for (auto& l : ctx.ws.lists) l.clear();
        for (auto& f : ctx.ws.statistics.listFormula) f.clear();
        v.row = 0;
        ctx.markDirty();
        ctx.toast("Liste 1-26 cancellate");
    }
}
void onAnswer(Context& ctx, View& v, int tag) {
    const std::string t = v.shell.ask.text();
    if (t.empty()) return;
    const double n = evalNumber(ctx, t);
    if (tag == AskSortCount) {
        if (n != std::floor(n) || n < 1 || n > 6) throw MathError("Argument ERROR: da 1 a 6 liste");
        v.sortCount = static_cast<int>(n);
        askSortNext(v);
    } else if (tag == AskSortList) {
        if (n != std::floor(n) || n < 1 || n > 26) throw MathError("Argument ERROR: List da 1 a 26");
        const int idx = static_cast<int>(n) - 1;
        if (std::find(v.sortLists.begin(), v.sortLists.end(), idx) != v.sortLists.end()) throw MathError("Argument ERROR: lista gia' scelta");
        v.sortLists.push_back(idx);
        if (static_cast<int>(v.sortLists.size()) < v.sortCount) { askSortNext(v); return; }
        sortLists(ctx, v.sortLists, v.sortDesc);
    } else if (tag == AskJumpRow) {
        if (n != std::floor(n) || n < 1) throw MathError("Argument ERROR: riga >= 1");
        v.row = std::min(static_cast<int>(n) - 1, static_cast<int>(lengthOf(ctx, v.col)));
    }
}
void onMenu(Context& ctx, View& v, int tag, int choice) {
    if (tag == 1) { ctx.ws.statistics.listColor[static_cast<std::size_t>(v.col)] = static_cast<std::uint8_t>(choice); ctx.markDirty(); }
    else if (tag == 2) { ctx.ws.selectListFile(static_cast<std::size_t>(choice)); ctx.markDirty(); recalcFormulas(ctx, false); }
    else if (tag == 3) {
        if (choice == 0) csvList(ctx, v, false);
        else if (choice == 1) csvList(ctx, v, true);
        else if (choice == 2) openCsvSet(ctx, v);
        else v.shell.confirm.open("Delete All Lists?  (List 1-26)", 2);
    }
}
// Moduli con applicazione all'uscita (SET UP, SET di CALC, V-Window, SELECT, Start/Width).
bool formScreenKey(Context& ctx, View& v, const KeyEvent& ev, const std::function<void()>& apply) {
    if (v.form.editing()) { (void)v.form.key(ctx, ev); return true; }
    if (ev.is(Key::Exit) || ev.is(Key::Exe)) { apply(); return false; }
    (void)v.form.key(ctx, ev);
    return true;
}
} // namespace

// ---------------------------------------------------------------- App
void StatisticsApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    View& v = *v_;
    barFor(v);
    recalcFormulas(ctx, false);
    // v1.16.0: striscia Stat Graph di eActivity: gli StatGraph accesi (StatGraph1 se nessuno).
    if (ctx.inStrip(AppId::Statistics) && ctx.strip().entry == stripentry::StatGraph) {
        try {
            std::vector<std::size_t> idx = selectedGraphs(ctx);
            if (idx.empty()) idx.push_back(0);
            bool hist = false;
            for (std::size_t i : idx) hist = hist || ctx.ws.statistics.graphs[i].type == GraphType::Hist || ctx.ws.statistics.graphs[i].type == GraphType::Broken;
            if (!hist) startDraw(ctx, v, idx);
        } catch (const MathError&) {
            v.screen = Screen::Lists; v.back.clear(); barFor(v);
        }
    }
}
void StatisticsApp::leave(Context&) { v_.reset(); }
bool StatisticsApp::tick(Context& ctx) {
    if (!v_ || !v_->task) return false;
    View& v = *v_;
    if (v.runner.running()) {
        try { v.runner.tick([&ctx] { return ctx.now(); }); }
        catch (...) { v.task.reset(); v.onDone = nullptr; leaveRunning(v); barFor(v); throw; }
    }
    if (v.runner.state() == step::State::Done) {
        auto done = std::move(v.onDone);
        v.onDone = nullptr;
        try { if (done) done(ctx); } catch (...) { v.task.reset(); leaveRunning(v); barFor(v); throw; }
        v.task.reset();
        v.runner.reset();
    }
    return true;
}

void StatisticsApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    if (v.runner.running()) {
        if (ev.is(Key::AC)) {
            v.runner.interrupt(); v.task.reset(); v.onDone = nullptr; v.runner.reset();
            leaveRunning(v); barFor(v);
            throw MathError("Break: calcolo interrotto (AC)");
        }
        return;
    }
    if (v.dialog.active()) { if (v.dialog.key(ev) == 1) csvChosen(ctx, v); return; }
    if (v.shell.modalOpen()) {
        (void)v.shell.modalKey(ev, [&](int tag, bool yes) { onConfirm(ctx, v, tag, yes); }, [&](int tag) { onAnswer(ctx, v, tag); },
                               [&](int tag, int choice) { onMenu(ctx, v, tag, choice); });
        return;
    }
    if (ev.is(Key::Setup) && !v.editing && v.screen != Screen::Setup) { openSetup(ctx, v); return; }
    switch (v.screen) {
    case Screen::Lists: listsKey(ctx, v, ev); return;
    case Screen::Results: {
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActCopy && v.kind == View::Kind::Reg) { openCopy(ctx, v); return; }
        if (a == ActDraw) { if (v.kind == View::Kind::Reg) drawRegression(ctx, v); else drawResult(v); return; }
        if (ev.is(Key::Exit)) { goBack(v); return; }
        (void)v.results.key(ev);
        return;
    }
    case Screen::Graph: {
        const int a = v.shell.fkeys.key(ev, ctx.shift());
        if (a == FKeyBar::Handled) return;
        if (a == ActTrace) { v.tracing = !v.tracing; if (v.tracing) traceUpdate(v); return; }
        if (a == ActVWin) { openVWindow(ctx, v); return; }
        if (a >= 0) { calcAction(ctx, v, a, true); return; }
        if (v.tracing) {
            if (ev.is(Key::Right)) { ++v.traceIndex; traceUpdate(v); return; }
            if (ev.is(Key::Left)) { if (v.traceIndex) --v.traceIndex; traceUpdate(v); return; }
            if (ev.is(Key::Up) || ev.is(Key::Down)) { v.tracePlot = (v.tracePlot + (ev.is(Key::Up) ? v.plots.size() - 1 : 1)) % std::max<std::size_t>(1, v.plots.size()); v.traceIndex = 0; traceUpdate(v); return; }
            if (ev.is(Key::Exit)) { v.tracing = false; return; }
        }
        if (ev.is(Key::Exit) || ev.is(Key::Exe)) { goBack(v); return; }
        return;
    }
    case Screen::Form: {
        if (v.form.editing()) { (void)v.form.key(ctx, ev); return; }
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActCalcRun || a == ActDraw) {
            const bool draw = a == ActDraw;
            if (isDistForm(v.formKind)) {
                runDistForm(ctx, v);
                if (draw) { View* self = &v; auto prev = std::move(v.onDone); v.onDone = [self, prev](Context& c) { if (prev) prev(c); drawResult(*self); }; }
            } else {
                runTestForm(ctx, v);
                if (draw) {
                    if (v.task) { View* self = &v; auto prev = std::move(v.onDone); v.onDone = [self, prev](Context& c) { if (prev) prev(c); drawResult(*self); }; }
                    else drawResult(v);
                }
            }
            return;
        }
        if (ev.is(Key::Exit)) { remember(v); goBack(v); return; }
        (void)v.form.key(ctx, ev);
        formChanged(v);
        return;
    }
    case Screen::Set: {
        if (v.form.editing()) { (void)v.form.key(ctx, ev); return; }
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActGph1 || a == ActGph2 || a == ActGph3) { openSet(ctx, v, a - ActGph1); return; }
        if (a == ActDefG) {
            ctx.ws.statistics.graphs[static_cast<std::size_t>(v.setGraph)] = stats::defaultGraph(static_cast<std::size_t>(v.setGraph));
            ctx.markDirty(); buildSetForm(ctx, v); return;
        }
        if (ev.is(Key::Exit) || ev.is(Key::Exe)) { goBack(v); return; }
        (void)v.form.key(ctx, ev);
        applySetForm(ctx, v);
        return;
    }
    case Screen::Select: {
        const int a = v.shell.fkeys.key(ev, false);
        if (a == ActOn || a == ActOff) { v.form.fields[static_cast<std::size_t>(v.form.selected)].choice = a == ActOn ? 0 : 1; applySelectForm(ctx, v); buildSelectForm(ctx, v); return; }
        if (a == ActDraw) { applySelectForm(ctx, v); goBack(v); startDraw(ctx, v, selectedGraphs(ctx)); return; }
        if (ev.is(Key::Exit) || ev.is(Key::Exe)) { applySelectForm(ctx, v); goBack(v); return; }
        (void)v.form.key(ctx, ev);
        applySelectForm(ctx, v);
        return;
    }
    case Screen::CalcSet: if (!formScreenKey(ctx, v, ev, [&] { applyCalcSet(ctx, v); })) goBack(v); return;
    case Screen::Setup:
        if (!formScreenKey(ctx, v, ev, [&] { if (v.formKind == -2) { applyCsvSet(ctx, v); v.formKind = -1; } else applySetup(ctx, v); })) goBack(v);
        return;
    case Screen::VWindow:
        if (!formScreenKey(ctx, v, ev, [&] { applyVWindow(ctx, v); })) goBack(v);
        return;
    case Screen::HistAsk: {
        if (v.form.editing()) { (void)v.form.key(ctx, ev); return; }
        if (ev.is(Key::Exit)) { goBack(v); return; }
        if (ev.is(Key::Exe) && v.form.selected == 1) {
            stats::StatGraph& g = ctx.ws.statistics.graphs[v.histGraph];
            const double start = numberOf(ctx, v.form, "Start"), width = numberOf(ctx, v.form, "Width");
            if (!(width > 0) || !std::isfinite(start)) throw MathError("Argument ERROR: Width > 0");
            g.hStart = start; g.hWidth = width; g.hSet = true;
            ctx.markDirty();
            v.histAsked = static_cast<std::uint8_t>(v.histAsked | (1u << v.histGraph));
            goBack(v);
            continueDraw(ctx, v);
            return;
        }
        (void)v.form.key(ctx, ev);
        return;
    }
    case Screen::Copy:
        if (ev.is(Key::Exe)) { copyTo(ctx, v); return; }
        if (ev.is(Key::Exit)) { goBack(v); return; }
        (void)v.copyList.key(ev);
        return;
    case Screen::DistGraph: case Screen::Interaction:
        if (ev.is(Key::Exit) || ev.is(Key::Exe)) goBack(v);
        return;
    case Screen::Running: return;
    }
}

namespace {
// ---------------------------------------------------------------- disegno
void drawEditor(Context& ctx, View& v, gfx::Canvas& c) {
    const int top = ui::ContentTop + 1, bottom = ui::ContentBottom;
    constexpr int labelW = 26, cols = 4, headH = 16, rowH = 18, editH = 22;
    const bool sub = ctx.ws.settings.subName;
    const int subH = sub ? 14 : 0;
    const int colW = (ui::ScreenW - labelW - 4) / cols;
    const int visibleRows = std::max(1, (bottom - top - headH - subH - editH) / rowH);
    if (v.col < v.firstCol) v.firstCol = v.col;
    if (v.col >= v.firstCol + cols) v.firstCol = v.col - cols + 1;
    const int focus = std::max(0, v.row);
    if (focus < v.firstRow) v.firstRow = focus;
    if (focus >= v.firstRow + visibleRows) v.firstRow = focus - visibleRows + 1;
    c.rect(0, top, ui::ScreenW, headH + subH, StatusBg, true);
    const int file = ctx.ws.settings.listFile;
    for (int k = 0; k < cols; ++k) {
        const int cc = v.firstCol + k, x = labelW + k * colW;
        const std::size_t ci = static_cast<std::size_t>(cc);
        const bool formula = !ctx.ws.statistics.listFormula[ci].empty();
        const bool headSel = v.row == StatisticsApp::HeadRow && v.col == cc;
        if (headSel) c.rect(x + 1, top, colW - 1, headH, v.editing ? SelectSoft : Select, true);
        const std::string name = (file ? "F" + std::to_string(file + 1) + " " : std::string()) + "List " + std::to_string(cc + 1);
        ui::textCenter(c, x + colW / 2, top + 2, name.c_str(), headSel && !v.editing ? SelectInk : formula ? Accent : Ink, font::Face::Small);
        if (formula) c.hLine(x + 6, top + headH - 2, colW - 12, headSel ? SelectInk : Accent);
        if (sub) {
            const bool sel = v.row == StatisticsApp::SubRow && v.col == cc;
            if (sel) c.rect(x + 1, top + headH, colW - 1, subH, v.editing ? SelectSoft : Select, true);
            ui::textCenter(c, x + colW / 2, top + headH + 1, ctx.ws.listName(ci).c_str(), sel && !v.editing ? SelectInk : Accent, font::Face::Small);
        }
        c.vLine(x, top, headH + subH + visibleRows * rowH, Faint);
    }
    if (sub) ui::textRight(c, labelW - 3, top + headH + 1, "SUB", Muted, font::Face::Small);
    if (v.firstCol > 0) ui::text(c, 1, top + 2, "\xE2\x97\x80", Accent, font::Face::Small);
    if (v.firstCol + cols < 26) ui::textRight(c, ui::ScreenW - 1, top + 2, "\xE2\x96\xB6", Accent, font::Face::Small);
    const int gridTop = top + headH + subH;
    for (int r = 0; r < visibleRows; ++r) {
        const int rr = v.firstRow + r, y = gridTop + r * rowH;
        char lbl[16]; std::snprintf(lbl, sizeof lbl, "%d", rr + 1);
        ui::textRight(c, labelW - 3, y + 3, lbl, Muted, font::Face::Small);
        for (int k = 0; k < cols; ++k) {
            const int cc = v.firstCol + k, x = labelW + k * colW;
            const auto& l = ctx.ws.lists[static_cast<std::size_t>(cc)];
            const bool sel = rr == v.row && cc == v.col;
            if (sel) c.rect(x + 1, y, colW - 1, rowH, v.editing ? SelectSoft : Select, true);
            if (static_cast<std::size_t>(rr) < l.size()) {
                const double val = l[static_cast<std::size_t>(rr)];
                std::string s = formatReal(val, ctx.ws.settings.display);
                const int maxGlyphs = (colW - 4) / 6;
                if (static_cast<int>(font::glyphCount(s.c_str())) > maxGlyphs) s = formatReal(val, {DisplayMode::Sci, static_cast<std::uint8_t>(std::max(1, maxGlyphs - 5))});
                // E10: i dati nel colore della lista.
                const std::uint16_t ink = sel && !v.editing ? SelectInk : statplot::paletteColor(ctx.ws.statistics.listColor[static_cast<std::size_t>(cc)]);
                ui::textRight(c, x + colW - 3, y + 3, s.c_str(), ink, font::Face::Small);
            }
        }
        c.hLine(labelW, y + rowH - 1, ui::ScreenW - labelW, Grid);
    }
    const int ey = bottom - editH + 3;
    c.hLine(0, bottom - editH, ui::ScreenW, Faint);
    const std::size_t ci = static_cast<std::size_t>(v.col);
    if (v.editing) {
        const char* prefix = v.row == StatisticsApp::HeadRow ? "=" : nullptr;
        int x = 6;
        if (prefix) x += ui::text(c, 6, ey, ("List " + std::to_string(v.col + 1) + " " + prefix).c_str(), Accent) + 4;
        v.editor.draw(c, x, ey, ui::ScreenW - x - 6, Ink);
    } else if (v.row == StatisticsApp::HeadRow) {
        const std::string& f = ctx.ws.statistics.listFormula[ci];
        ui::textFit(c, 6, ey, "List " + std::to_string(v.col + 1) + (f.empty() ? std::string("  (EXE: formula)") : " = " + f), ui::ScreenW - 12, Muted);
    } else if (v.row == StatisticsApp::SubRow) {
        ui::text(c, 6, ey, ("SUB: " + ctx.ws.listName(ci)).c_str(), Muted);
    } else if (static_cast<std::size_t>(v.row) < ctx.ws.lists[ci].size()) {
        ui::text(c, 6, ey, formatReal(ctx.ws.lists[ci][static_cast<std::size_t>(v.row)], ctx.ws.settings.display).c_str(), Muted);
    }
}
void drawTitle(gfx::Canvas& c, const std::string& title) {
    const int top = ui::ContentTop + 1;
    c.rect(0, top, ui::ScreenW, 20, SelectSoft, true);
    ui::textFit(c, 6, top + 2, title, ui::ScreenW - 12, Ink);
}
std::string formTitle(const View& v) {
    if (isDistForm(v.formKind)) return dist::kindName(distKindOf(v.formKind));
    return v.formKind >= 0 && v.formKind < TestKindCount ? kTestTitles[v.formKind] : "";
}
} // namespace

void StatisticsApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    ui::clear(c);
    ctx.drawStatus(c, "STATISTICS");
    const int top = ui::ContentTop + 1, bottom = ui::ContentBottom;
    switch (v.screen) {
    case Screen::Lists: drawEditor(ctx, v, c); break;
    case Screen::Results: v.results.draw(c, top, bottom); break;
    case Screen::Graph:
        statplot::draw(c, v.window, v.plots, v.overlay ? &v.fit : nullptr, ctx.ws);
        if (v.tracing && !v.plots.empty()) {
            double x = 0, y = 0;
            std::string label;
            if (statplot::tracePoint(v.plots[v.tracePlot], v.traceIndex, x, y, label)) statplot::drawTraceCursor(c, v.window, x, y);
            c.rect(0, bottom - 16, ui::ScreenW, 16, Bg, true);
            ui::textFit(c, 4, bottom - 15, v.traceLabel, ui::ScreenW - 8, Ink, font::Face::Small);
        }
        break;
    case Screen::Form: case Screen::Set: case Screen::Select: case Screen::CalcSet: case Screen::Setup: case Screen::VWindow: case Screen::HistAsk: {
        std::string title;
        switch (v.screen) {
        case Screen::Form: title = formTitle(v); break;
        case Screen::Set: title = "StatGraph" + std::to_string(v.setGraph + 1); break;
        case Screen::Select: title = "SELECT  DrawOn / DrawOff"; break;
        case Screen::CalcSet: title = "SET  1-VAR / 2-VAR"; break;
        case Screen::Setup: title = v.formKind == -2 ? "CSV SET  formato" : "SET UP  Statistics"; break;
        case Screen::VWindow: title = "V-Window"; break;
        default: title = "Histogram  Start / Width"; break;
        }
        drawTitle(c, title);
        v.form.draw(c, top + 24, bottom);
        break;
    }
    case Screen::Copy:
        drawTitle(c, "COPY  " + std::string(stats::regModel(v.fit.type)) + "  \xE2\x86\x92 Yn");
        v.copyList.draw(c, gfx::Rect{0, top + 22, ui::ScreenW, bottom - top - 22}, 18);
        break;
    case Screen::Running: {
        drawTitle(c, "Calcolo in corso");
        const int pct = v.runner.percent();
        const std::string t = pct >= 0 ? std::to_string(pct) + " %" : "...";
        ui::textCenter(c, ui::ScreenW / 2, top + 100, t.c_str(), Ink);
        c.rect(40, top + 124, ui::ScreenW - 80, 10, Faint, false);
        if (pct > 0) c.rect(41, top + 125, (ui::ScreenW - 82) * pct / 100, 8, Accent, true);
        ui::textCenter(c, ui::ScreenW / 2, top + 150, "AC: Break", Muted, font::Face::Small);
        break;
    }
    case Screen::DistGraph:
        drawDistributionGraph(c, statplot::plotArea(), v.distGraph, v.graphLines);
        break;
    case Screen::Interaction:
        statplot::drawInteraction(c, v.anova);
        break;
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.dialog.draw(c);
}

// ---------------------------------------------------------------- accessori (test e simulatore)
StatisticsApp::Screen StatisticsApp::screen() const noexcept { return v_ ? v_->screen : Screen::Lists; }
std::string StatisticsApp::screenName() const {
    if (!v_) return "closed";
    static const char* const n[] = {"lists", "results", "graph", "form", "set", "select", "calcset", "setup", "vwindow", "histask", "copy", "running", "distgraph", "interaction"};
    return n[static_cast<int>(v_->screen)];
}
const ResultList& StatisticsApp::results() const noexcept { static const ResultList empty; return v_ ? v_->results : empty; }
int StatisticsApp::cursorRow() const noexcept { return v_ ? v_->row : 0; }
int StatisticsApp::cursorColumn() const noexcept { return v_ ? v_->col : 0; }
int StatisticsApp::firstColumn() const noexcept { return v_ ? v_->firstCol : 0; }
const Form& StatisticsApp::form() const noexcept { static const Form empty; return v_ ? v_->form : empty; }
const FileDialog& StatisticsApp::fileDialog() const noexcept { static const FileDialog empty; return v_ ? v_->dialog : empty; }
const char* const* StatisticsApp::softKeyLabels(Context& ctx) const {
    static const char* labels[6] = {};
    for (auto& l : labels) l = nullptr;
    if (v_) v_->shell.fkeys.labels(ctx.shift(), labels);
    return labels;
}
std::size_t StatisticsApp::drawnPlots() const noexcept { return v_ ? v_->plots.size() : 0; }
std::string StatisticsApp::traceText() const { return v_ && v_->tracing ? v_->traceLabel : std::string(); }
} // namespace fx::app
