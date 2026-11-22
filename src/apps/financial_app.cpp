// FINANCIAL (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Funzioni minime del capitolo 7 del manuale fx-CG50 (descrizione del
// comportamento, nessun codice copiato): interesse semplice, interesse composto
// TVM che risolve n, I%, PV, PMT o FV con il pagamento BGN o END, flussi di
// cassa (NPV, IRR, PBP, NFV), ammortamento (BAL, INT, PRN, ΣINT, ΣPRN),
// conversione dei tassi (APR <-> EFF) e giorni tra due date. Ogni risultato si
// copia in Ans (F6). I calcoli sono quelli di OPTN › TVM (calc_library) e le
// variabili TVM sono quelle di VARS › TVM.
#include "apps.hpp"
#include "financial.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fx::app {
using namespace ui::theme;
namespace {
enum Page : int { Menu = 0, Simple, Compound, Cash, Amort, Convert, Days };
const char* const kPageNames[] = {"Financial", "Simple Interest", "Compound Interest", "Cash Flow", "Amortization", "Interest Conversion", "Day Calculation"};
enum Act : int { ActSI = 0, ActSFV, ActN, ActI, ActPV, ActPMT, ActFV, ActNPV, ActIRR, ActPBP, ActNFV, ActBAL, ActINT, ActPRN, ActSINT, ActSPRN,
                 ActEFF, ActAPR, ActPRD, ActAns, ActOpen, ActSaveAs };
const FKeyPage kFilePage{{{"OPEN", ActOpen}, {"SAVE\xC2\xB7" "AS", ActSaveAs}, {}, {}, {}, {}}};
const FKeyMenu kFileMenu{&kFilePage, 1, nullptr};
const FKeyPage kMenuPage{{{}, {}, {}, {}, {"FILE", -1, &kFileMenu}, {}}};
const FKeyMenu kMenuMenu{&kMenuPage, 1, nullptr};
const FKeyPage kSimplePage{{{"SI", ActSI}, {"SFV", ActSFV}, {}, {}, {}, {"\xE2\x86\x92" "Ans", ActAns}}};
const FKeyPage kCompoundPage{{{"n", ActN}, {"I%", ActI}, {"PV", ActPV}, {"PMT", ActPMT}, {"FV", ActFV}, {"\xE2\x86\x92" "Ans", ActAns}}};
const FKeyPage kCashPage{{{"NPV", ActNPV}, {"IRR", ActIRR}, {"PBP", ActPBP}, {"NFV", ActNFV}, {}, {"\xE2\x86\x92" "Ans", ActAns}}};
const FKeyPage kAmortPage1{{{"BAL", ActBAL}, {"INT", ActINT}, {"PRN", ActPRN}, {"\xCE\xA3" "INT", ActSINT}, {"\xCE\xA3" "PRN", ActSPRN}, {"\xE2\x86\x92" "Ans", ActAns}}};
const FKeyPage kConvertPage{{{"\xE2\x96\xB6" "EFF", ActEFF}, {"\xE2\x96\xB6" "APR", ActAPR}, {}, {}, {}, {"\xE2\x86\x92" "Ans", ActAns}}};
const FKeyPage kDaysPage{{{"PRD", ActPRD}, {}, {}, {}, {}, {"\xE2\x86\x92" "Ans", ActAns}}};
const FKeyMenu kSimpleMenu{&kSimplePage, 1, nullptr}, kCompoundMenu{&kCompoundPage, 1, nullptr}, kCashMenu{&kCashPage, 1, nullptr},
               kAmortMenu{&kAmortPage1, 1, nullptr}, kConvertMenu{&kConvertPage, 1, nullptr}, kDaysMenu{&kDaysPage, 1, nullptr};
const FKeyMenu* menuOf(int page) {
    switch (page) {
    case Simple: return &kSimpleMenu;
    case Compound: return &kCompoundMenu;
    case Cash: return &kCashMenu;
    case Amort: return &kAmortMenu;
    case Convert: return &kConvertMenu;
    case Days: return &kDaysMenu;
    default: return &kMenuMenu;
    }
}
} // namespace

struct FinancialApp::View {
    ModeShell shell;
    TypedFileMenu files{files::FileType::Financial};
    SelectList pages;
    Form form;
    int page{Menu};
    std::string resultLabel;
    double result{0};
    bool haveResult{false};
};

FinancialApp::FinancialApp() = default;
FinancialApp::~FinancialApp() = default;

namespace {
fin::Tvm tvmOf(Context& ctx) {
    const ModeVariables& m = ctx.ws.modeVars;
    fin::Tvm t;
    t.n = m.tvmN; t.i = m.tvmI; t.pv = m.tvmPV; t.pmt = m.tvmPMT; t.fv = m.tvmFV; t.py = m.tvmPY; t.cy = m.tvmCY;
    return t;
}
void storeTvm(Context& ctx, const fin::Tvm& t) {
    ModeVariables& m = ctx.ws.modeVars;
    m.tvmN = t.n; m.tvmI = t.i; m.tvmPV = t.pv; m.tvmPMT = t.pmt; m.tvmFV = t.fv; m.tvmPY = t.py; m.tvmCY = t.cy;
    ctx.markDirty();
}
std::string cashText(const std::vector<double>& cash) {
    std::string s = "{";
    for (std::size_t i = 0; i < cash.size(); ++i) s += (i ? "," : "") + formNumber(cash[i]);
    return s + "}";
}
void fillPage(Context& ctx, FinancialApp::View& v) {
    const fin::State& s = ctx.ws.financial;
    v.form.fields.clear();
    v.form.selected = 0;
    switch (v.page) {
    case Simple:
        v.form.fields = {numberField("n (giorni)", formNumber(s.smplN)), numberField("I%", formNumber(s.smplI)), numberField("PV", formNumber(s.smplPV))};
        break;
    case Compound: case Amort: {
        const fin::Tvm t = tvmOf(ctx);
        v.form.fields = {numberField("n", formNumber(t.n)), numberField("I%", formNumber(t.i)), numberField("PV", formNumber(t.pv)),
                         numberField("PMT", formNumber(t.pmt)), numberField("FV", formNumber(t.fv)), numberField("P/Y", formNumber(t.py)),
                         numberField("C/Y", formNumber(t.cy))};
        if (v.page == Amort) {
            v.form.fields.insert(v.form.fields.begin(), {numberField("PM1", formNumber(s.pm1)), numberField("PM2", formNumber(s.pm2))});
        }
        break;
    }
    case Cash: v.form.fields = {numberField("I%", formNumber(s.cashI)), textField("Csh", cashText(s.cash))}; break;
    case Convert: v.form.fields = {numberField("n", formNumber(s.cnvN)), numberField("I%", formNumber(s.cnvI))}; break;
    case Days:
        v.form.fields = {numberField("m1", formNumber(s.dates[0])), numberField("d1", formNumber(s.dates[1])), numberField("y1", formNumber(s.dates[2])),
                         numberField("m2", formNumber(s.dates[3])), numberField("d2", formNumber(s.dates[4])), numberField("y2", formNumber(s.dates[5]))};
        break;
    default: break;
    }
}
// Il modulo nel workspace (prima di ogni calcolo): un valore non valido lascia tutto com'era.
void applyPage(Context& ctx, FinancialApp::View& v) {
    fin::State s = ctx.ws.financial;
    auto n = [&](std::size_t i) { return v.form.number(ctx, i); };
    switch (v.page) {
    case Simple: s.smplN = n(0); s.smplI = n(1); s.smplPV = n(2); break;
    case Compound: case Amort: {
        const std::size_t o = v.page == Amort ? 2 : 0;
        if (v.page == Amort) { s.pm1 = n(0); s.pm2 = n(1); }
        fin::Tvm t;
        t.n = n(o); t.i = n(o + 1); t.pv = n(o + 2); t.pmt = n(o + 3); t.fv = n(o + 4); t.py = n(o + 5); t.cy = n(o + 6);
        storeTvm(ctx, t);
        break;
    }
    case Cash: {
        s.cashI = n(0);
        // La lista dei flussi letta dal motore unico ({...} o List n).
        const CalcValue cv = evaluateAny(*parseExpression(v.form.fields[1].text), ctx.ws.evalContext());
        if (cv.type != ValueType::List || cv.re.empty()) throw MathError("Argument ERROR: Csh deve essere una lista");
        s.cash = cv.re;
        break;
    }
    case Convert: s.cnvN = n(0); s.cnvI = n(1); break;
    case Days: for (std::size_t i = 0; i < 6; ++i) s.dates[i] = n(i); break;
    default: break;
    }
    ctx.ws.financial = s;
    ctx.markDirty();
}
void show(FinancialApp::View& v, const std::string& label, double value) {
    v.resultLabel = label;
    v.result = value;
    v.haveResult = true;
}
void calculate(Context& ctx, FinancialApp::View& v, int act) {
    applyPage(ctx, v);
    const fin::State& s = ctx.ws.financial;
    switch (act) {
    case ActSI: show(v, "SI", fin::simple(s).si); return;
    case ActSFV: show(v, "SFV", fin::simple(s).sfv); return;
    case ActN: case ActI: case ActPV: case ActPMT: case ActFV: {
        const auto var = static_cast<fin::TvmVar>(act - ActN);
        fin::Tvm t = tvmOf(ctx);
        const double r = fin::solve(var, t, s.begin);
        t.at(var) = r;
        storeTvm(ctx, t);
        fillPage(ctx, v);
        v.form.selected = act - ActN;
        show(v, fin::tvmName(var), r);
        return;
    }
    case ActNPV: case ActIRR: case ActPBP: case ActNFV: {
        const fin::CashResult r = fin::cashFlow(s);
        if (act == ActNPV) show(v, "NPV", r.npv);
        else if (act == ActNFV) show(v, "NFV", r.nfv);
        else if (act == ActIRR) { if (!r.irrOk) throw MathError("Iteration ERROR: IRR non trovato"); show(v, "IRR", r.irr); }
        else { if (!r.pbpOk) throw MathError("Math ERROR: il periodo di recupero supera i flussi"); show(v, "PBP", r.pbp); }
        return;
    }
    case ActBAL: case ActINT: case ActPRN: case ActSINT: case ActSPRN: {
        const fin::AmortResult r = fin::amortization(s, tvmOf(ctx));
        const double vals[] = {r.bal, r.interest, r.principal, r.sumInterest, r.sumPrincipal};
        const char* names[] = {"BAL", "INT", "PRN", "\xCE\xA3" "INT", "\xCE\xA3" "PRN"};
        show(v, names[act - ActBAL], vals[act - ActBAL]);
        return;
    }
    case ActEFF: show(v, "EFF", fin::toEff(s)); return;
    case ActAPR: show(v, "APR", fin::toApr(s)); return;
    case ActPRD: show(v, "PRD", fin::days(s)); return;
    default: return;
    }
}
} // namespace

void FinancialApp::enter(Context& ctx) {
    v_ = std::make_unique<View>();
    for (int p = Simple; p <= Days; ++p) v_->pages.items.push_back(kPageNames[p]);
    v_->shell.fkeys.set(&kMenuMenu);
    if (ctx.inStrip(AppId::Financial)) { v_->page = Compound; fillPage(ctx, *v_); v_->shell.fkeys.set(&kCompoundMenu); }
}
void FinancialApp::leave(Context&) { v_.reset(); }
std::string FinancialApp::screenName() const {
    if (!v_) return "closed";
    static const char* const n[] = {"menu", "simple", "compound", "cash", "amort", "convert", "days"};
    return n[v_->page];
}
bool FinancialApp::tick(Context&) { return false; }

void FinancialApp::key(Context& ctx, const KeyEvent& ev) {
    if (!v_) enter(ctx);
    View& v = *v_;
    if (v.files.active()) {
        if (v.files.key(ev) != 1) return;
        if (v.files.purpose() == TypedFileMenu::Purpose::Open) {
            const auto data = v.files.load(ctx, v.files.file());
            fin::FileData f;
            if (!fin::decodeFile(data.data(), data.size(), f)) throw MathError("Data ERROR: file Financial danneggiato");
            ctx.ws.financial = f.state;
            storeTvm(ctx, f.tvm);
        } else {
            v.files.save(ctx, v.files.file(), fin::encodeFile({ctx.ws.financial, tvmOf(ctx)}));
            ctx.toast("Salvato " + v.files.file());
        }
        return;
    }
    if (ev.is(Key::Setup)) {
        // SET UP: Payment BGN / END (manuale 7-2).
        ctx.ws.financial.begin = !ctx.ws.financial.begin;
        ctx.markDirty();
        ctx.toast(std::string("Payment: ") + (ctx.ws.financial.begin ? "BGN" : "END"));
        return;
    }
    if (v.page == Menu) {
        const int a = v.shell.fkeys.key(ev, ctx.shift());
        if (a == FKeyBar::Handled) return;
        if (a == ActOpen) { v.shell.fkeys.set(&kMenuMenu); v.files.openList(ctx); return; }
        if (a == ActSaveAs) { v.shell.fkeys.set(&kMenuMenu); v.files.openSaveAs(ctx, ""); return; }
        if (ev.is(Key::Exe) || (ev.key == Key::Char && ev.ch >= '1' && ev.ch <= '6')) {
            v.page = ev.key == Key::Char ? ev.ch - '0' : v.pages.selected + 1;
            v.haveResult = false;
            fillPage(ctx, v);
            v.shell.fkeys.set(menuOf(v.page));
            return;
        }
        if (ev.is(Key::Exit)) { if (v.shell.fkeys.inSubmenu()) v.shell.fkeys.back(); else ctx.home(); return; }
        (void)v.pages.key(ev);
        return;
    }
    const bool editing = v.form.editing();
    const int a = editing ? FKeyBar::None : v.shell.fkeys.key(ev, false);
    if (a == ActAns) {
        if (!v.haveResult) return;
        ctx.ws.memory.setAnswer(v.result);
        ctx.markDirty();
        ctx.toast(v.resultLabel + " \xE2\x86\x92 Ans");
        return;
    }
    if (a >= 0) { calculate(ctx, v, a); return; }
    if (!editing && ev.is(Key::Exit)) { v.page = Menu; v.haveResult = false; v.shell.fkeys.set(&kMenuMenu); return; }
    (void)v.form.key(ctx, ev);
}

void FinancialApp::draw(Context& ctx, gfx::Canvas& c) {
    if (!v_) enter(ctx);
    View& v = *v_;
    ui::clear(c);
    ctx.drawStatus(c, "FINANCIAL");
    const int top = ui::ContentTop;
    const std::string head = std::string(kPageNames[v.page]) + "   Payment: " + (ctx.ws.financial.begin ? "BGN" : "END");
    drawModeHeader(c, head);
    if (v.page == Menu) {
        v.pages.draw(c, gfx::Rect{0, top + 18, ui::ScreenW, ui::ContentBottom - top - 18}, 24);
    } else {
        v.form.draw(c, top + 18, ui::ContentBottom - (v.haveResult ? 22 : 0));
        if (v.haveResult) {
            const int y = ui::ContentBottom - 22;
            c.rect(0, y, ui::ScreenW, 22, SelectSoft, true);
            const std::string t = v.resultLabel + " = " + fmt(ctx, v.result);
            ui::text(c, 6, y + 3, t.c_str(), Ink);
        }
    }
    v.shell.fkeys.draw(c, ctx.shift());
    v.shell.drawModals(c);
    v.files.draw(c);
}
} // namespace fx::app
