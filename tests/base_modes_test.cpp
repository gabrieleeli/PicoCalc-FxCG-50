// v1.17.0: nuclei delle modalita' Base (solo Pimoroni Pico Plus 2 W):
// formule del foglio, Dyna, successioni, coniche, TVM, file .pcprog ed
// evidenziazione, protocollo di Link, geometria, Picture Plot, MicroPython.
#include "board_host.hpp"
#include "conic.hpp"
#include "dyna.hpp"
#include "financial.hpp"
#include "geometry.hpp"
#include "link_protocol.hpp"
#include "mem_pools.hpp"
#include "picture_plot.hpp"
#include "program_mode.hpp"
#include "python_port.hpp"
#include "recursion.hpp"
#include "spreadsheet.hpp"
#include "statistics.hpp"
#include "calc_library.hpp"
#if FXAI_MODE_DISTRIBUTION
#include "dist_core.hpp"
#include "distribution.hpp"
#endif
#include "test_util.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace fx;

namespace {
double cellValue(const sheet::Sheet& s, const char* ref) {
    sheet::Ref r;
    if (!sheet::parseRef(ref, r)) return NAN;
    const sheet::Cell* c = s.cell(r);
    return c ? c->value : NAN;
}
std::string cellError(const sheet::Sheet& s, const char* ref) {
    sheet::Ref r;
    (void)sheet::parseRef(ref, r);
    const sheet::Cell* c = s.cell(r);
    return c ? c->error : "<vuota>";
}
sheet::Ref R(const char* t) { sheet::Ref r; (void)sheet::parseRef(t, r); return r; }

void spreadsheet() {
    Workspace ws;
    ws.memory.store('A', 10);
    const EvalContext ec = ws.evalContext();
    sheet::Ref r;
    CHECK(sheet::parseRef("B12", r) && r.col == 1 && r.row == 12 && sheet::refName(r) == "B12");
    CHECK(sheet::parseRef("$z$999", r) && r.col == 25 && r.row == 999);
    CHECK(!sheet::parseRef("A0", r) && !sheet::parseRef("A1000", r) && !sheet::parseRef("AA1", r) && !sheet::parseRef("1A", r));
    double n;
    CHECK(sheet::classify("12.5", n) == sheet::Kind::Number && n == 12.5);
    CHECK(sheet::classify("-3E2", n) == sheet::Kind::Number && n == -300);
    CHECK(sheet::classify("Ciao", n) == sheet::Kind::Text && sheet::classify("=A1", n) == sheet::Kind::Formula && sheet::classify("", n) == sheet::Kind::Empty);
    CHECK(sheet::classify("1-", n) == sheet::Kind::Text);
    sheet::Sheet s;
    s.set(R("A1"), "2");
    s.set(R("B1"), "=A1*3");
    s.set(R("C1"), "=B1+A");                                   // variabile A della memoria
    sheet::recalculate(s, ec);
    CHECK(cellValue(s, "B1") == 6 && cellValue(s, "C1") == 16);
    for (int i = 1; i <= 5; ++i) s.set(sheet::Ref{3, i}, std::to_string(i * i));   // D1..D5 = 1 4 9 16 25
    s.set(R("E1"), "=Sum(D1:D5)");
    s.set(R("E2"), "=Mean(D1:D5)");
    s.set(R("E3"), "=Min(D1:D5)");
    s.set(R("E4"), "=Max(D1:D5)");
    s.set(R("E5"), "=Median(D1:D5)");
    s.set(R("E6"), "=$A$1+A1");
    sheet::recalculate(s, ec);
    CHECK(cellValue(s, "E1") == 55 && cellValue(s, "E2") == 11 && cellValue(s, "E3") == 1 && cellValue(s, "E4") == 25 && cellValue(s, "E5") == 9);
    CHECK(cellValue(s, "E6") == 4);
    // ricalcolo automatico: la catena segue la cella cambiata
    s.set(R("A1"), "5");
    sheet::recalculate(s, ec);
    CHECK(cellValue(s, "B1") == 15 && cellValue(s, "C1") == 25);
    // riferimenti circolari: le celle del ciclo e quelle che ne dipendono
    s.set(R("F1"), "=G1+1");
    s.set(R("G1"), "=F1+1");
    s.set(R("H1"), "=F1*2");
    s.set(R("F2"), "=F2");
    sheet::recalculate(s, ec);
    CHECK(cellError(s, "F1") == sheet::CircularError && cellError(s, "G1") == sheet::CircularError && cellError(s, "H1") == sheet::CircularError);
    CHECK(cellError(s, "F2") == sheet::CircularError && cellError(s, "B1").empty());
    // errori di una formula: la cella mostra l'errore, il foglio resta
    s.set(R("I1"), "=1/0");
    s.set(R("I2"), "=Z999+1");                                  // cella vuota
    sheet::recalculate(s, ec);
    CHECK(cellError(s, "I1").find("Math ERROR") != std::string::npos && !cellError(s, "I2").empty());
    CHECK_THROWS(s.set(R("I3"), "=A1+"));                       // Syntax ERROR subito
    CHECK(s.cell(R("I3")) == nullptr);
    CHECK_THROWS(s.set(R("I3"), std::string(300, '1')));        // oltre 255 caratteri
    s.set(R("J1"), "Ciao");
    CHECK(s.cell(R("J1"))->kind == sheet::Kind::Text);
    s.set(R("J1"), "");
    CHECK(s.cell(R("J1")) == nullptr);
    // spostamento dei riferimenti relativi (COPY/PASTE, FILL)
    std::string out;
    CHECK(sheet::shiftFormula("=A1+$B$2+C$3+$D4+Sum(A1:A3)", 1, 2, out) && out == "=B3+$B$2+D$3+$D6+Sum(B3:B5)");
    CHECK(!sheet::shiftFormula("=A1", -1, 0, out));
    CHECK(sheet::shiftFormula("=Ans+A1", 0, 1, out) && out == "=Ans+A2");
    sheet::Clipboard clip;
    sheet::copyBlock(s, sheet::Block{R("A1"), R("B1")}, false, clip);
    sheet::paste(s, clip, R("A10"));
    sheet::recalculate(s, ec);
    CHECK(s.cell(R("B10"))->input == "=A10*3" && cellValue(s, "B10") == 15 && cellValue(s, "B1") == 15);
    sheet::copyBlock(s, sheet::Block{R("A10"), R("B10")}, true, clip);
    sheet::paste(s, clip, R("C20"));
    sheet::recalculate(s, ec);
    CHECK(s.cell(R("A10")) == nullptr && s.cell(R("D20"))->input == "=A10*3" && !clip.valid);
    sheet::fill(s, sheet::Block{R("K1"), R("K4")}, "=D1*2");
    sheet::recalculate(s, ec);
    CHECK(s.cell(R("K3"))->input == "=D3*2" && cellValue(s, "K4") == 32);
    CHECK_THROWS(sheet::paste(s, clip = sheet::Clipboard{true, false, sheet::Block{R("A1"), R("B2")}, {}}, R("Z999")));
    // colonna <-> lista
    CHECK(sheet::columnValues(s, 3, 1, 999) == std::vector<double>({1, 4, 9, 16, 25}));
    sheet::setColumn(s, 12, 2, {1.5, 2.5, 1e-20});
    CHECK(s.cell(R("M2"))->value == 1.5 && s.cell(R("M4"))->value == 1e-20 && s.cell(R("M1")) == nullptr);
    CHECK_THROWS(sheet::setColumn(s, 0, 998, {1, 2, 3}));
    // ricalcolo a passi: un'unita' = una formula; AC lascia le formule non valutate "stale"
    {
        sheet::Sheet big;
        for (int i = 1; i <= 50; ++i) big.set(sheet::Ref{0, i}, i == 1 ? "1" : "=A" + std::to_string(i - 1) + "+1");
        sheet::Recalc task(big, ec);
        step::Budget b(10, 0, nullptr);
        CHECK(!task.step(b) && task.done() == 10 && task.total() == 49);
        CHECK(big.cell(R("A50"))->stale);
        step::Budget all;
        CHECK(task.step(all));
        CHECK(cellValue(big, "A50") == 50 && !big.cell(R("A50"))->stale);
    }
    // codifica: stato e file
    sheet::State st;
    st.sheet = s;
    st.cursor = R("C7"); st.top = 3; st.left = 2; st.file = "CONTI.pcsheet"; st.autoCalc = false;
    std::vector<std::uint8_t> bytes;
    sheet::encode(st, bytes);
    sheet::State back;
    CHECK(sheet::decode(back, bytes.data(), bytes.size()) && back == st);
    bytes.back() = 9;
    CHECK(!sheet::decode(back, bytes.data(), bytes.size()) && back == st);
    const auto file = sheet::encodeFile(s);
    sheet::Sheet fs;
    CHECK(sheet::decodeFile(file.data(), file.size(), fs) && fs == s);
    for (std::size_t cut : {file.size() - 1, std::size_t{3}, std::size_t{0}}) CHECK(!sheet::decodeFile(file.data(), cut, fs));
}

void dynaGraph() {
    CHECK(dyna::frameValues(1, 5, 1) == std::vector<double>({1, 2, 3, 4, 5}));
    CHECK(dyna::frameValues(5, 1, -2) == std::vector<double>({5, 3, 1}));
    CHECK(dyna::frameValues(0, 1, 0.25).size() == 5);
    CHECK_THROWS(dyna::frameValues(1, 5, 0));
    CHECK_THROWS(dyna::frameValues(1, 5, -1));
    CHECK_THROWS(dyna::frameValues(0, 1000, 1));                // oltre 100 grafici
    CHECK(dyna::frameValues(0, 99, 1).size() == 100);
    dyna::State s;
    CHECK(dyna::validate(s).empty());
    s.function = "A(X-";
    CHECK(!dyna::validate(s).empty());
    s.function = "B sin(X)"; s.variable = 'B'; s.loop = false; s.speed = dyna::Speed::Fast;
    std::vector<std::uint8_t> b;
    dyna::encode(s, b);
    dyna::State t;
    CHECK(dyna::decode(t, b.data(), b.size()) && t == s);
    b[b.size() - 1] = 7;
    CHECK(!dyna::decode(t, b.data(), b.size()));
    // File .pcdyna: sezione + Start/End/Pitch; solo dati validi
    const dyna::FileData fd{s, -2, 2, 0.5};
    const auto fb = dyna::encodeFile(fd);
    dyna::FileData fr;
    CHECK(dyna::decodeFile(fb.data(), fb.size(), fr) && fr == fd);
    CHECK(!dyna::decodeFile(fb.data(), fb.size() - 1, fr));
    for (const dyna::FileData& bad : {dyna::FileData{s, 1, 5, 0}, dyna::FileData{s, 0, 1000, 1}, dyna::FileData{s, NAN, 5, 1}}) {
        const auto bb = dyna::encodeFile(bad);
        CHECK(!dyna::decodeFile(bb.data(), bb.size(), fr));
    }
    dyna::State badFn = s; badFn.function = "B(X-";
    const auto bf = dyna::encodeFile({badFn, 1, 5, 1});
    CHECK(!dyna::decodeFile(bf.data(), bf.size(), fr) && fr == fd);      // garanzia forte
    CHECK(dyna::frameDelayMs(dyna::Speed::Slow) > dyna::frameDelayMs(dyna::Speed::Normal) && dyna::frameDelayMs(dyna::Speed::Normal) > dyna::frameDelayMs(dyna::Speed::Fast));
}

void recursion() {
    Workspace ws;
    const EvalContext ec = ws.evalContext();
    rec::Setup s;
    s.type = rec::Type::An1;
    s.formula = "2rec_an+1";
    s.start = 0; s.end = 4; s.a0 = 1;
    auto rows = rec::table(s, ec);
    CHECK(rows.size() == 5 && rows[0].a == 1 && rows[1].a == 3 && rows[4].a == 31 && rows[4].sum == 1 + 3 + 7 + 15 + 31);
    s.formula = "rec_an+rec_n";                                 // dipende da n
    rows = rec::table(s, ec);
    CHECK(rows[1].a == 1 && rows[2].a == 2 && rows[3].a == 4 && rows[4].a == 7);
    // Fibonacci con an+2 = an+1 + an
    s.type = rec::Type::An2; s.formula = "rec_an1+rec_an"; s.a0 = 1; s.a1 = 1; s.end = 9;
    rows = rec::table(s, ec);
    CHECK(rows.size() == 10 && rows[2].a == 2 && rows[5].a == 8 && rows[9].a == 55);
    s.start = 5;
    rows = rec::table(s, ec);
    CHECK(rows.size() == 5 && rows[0].n == 5 && rows[0].a == 8 && rows[0].sum == 8);
    // A-Z restano quelli della memoria
    ws.memory.store('K', 3);
    s.type = rec::Type::An1; s.formula = "Krec_an"; s.start = 0; s.end = 3; s.a0 = 1;
    rows = rec::table(s, ws.evalContext());
    CHECK(rows[3].a == 27 && ws.memory.recall('K') == 3);
    // errori
    s.formula = "1/(rec_an-1)"; s.a0 = 1;
    CHECK_THROWS(rec::table(s, ec));
    s.formula = "rec_an"; s.start = 5; s.end = 2;
    CHECK(!rec::validate(s).empty());
    s.start = 0; s.end = 1000;
    CHECK(!rec::validate(s).empty());
    s.end = 999;
    CHECK(rec::validate(s).empty() && rec::table(s, ec).size() == 1000);
    // a passi
    s.end = 100;
    rec::Table t(s, ec);
    step::Budget b(10, 0, nullptr);
    CHECK(!t.step(b) && t.done() == 10);
    t.runAll();
    CHECK(t.rows().size() == 101);
    // ragnatela
    std::vector<rec::Row> r3 = {{0, 1, 1}, {1, 2, 3}, {2, 3, 6}};
    std::vector<double> xs, ys;
    rec::cobweb(r3, xs, ys);
    CHECK(xs.size() == ys.size() && xs.size() == 5 && xs[0] == 1 && ys[0] == 0 && ys[1] == 2 && xs[2] == 2 && ys[3] == 3);
    rec::State st;
    st.type = rec::Type::An2; st.an2 = "rec_an1+rec_an"; st.sigma = false;
    std::vector<std::uint8_t> bytes;
    rec::encode(st, bytes);
    rec::State back;
    CHECK(rec::decode(back, bytes.data(), bytes.size()) && back == st);
    // File .pcrecur: sezione, formula an+1, Start/End/a0/a1; solo dati validi
    const rec::FileData fd{st, "2rec_an+1", 1, 30, 1, 1};
    const auto fb = rec::encodeFile(fd);
    rec::FileData fr;
    CHECK(rec::decodeFile(fb.data(), fb.size(), fr) && fr == fd);
    CHECK(!rec::decodeFile(fb.data(), fb.size() - 1, fr));
    for (const rec::FileData& bad : {rec::FileData{st, "", 5, 2, 0, 0}, rec::FileData{st, "", 0, 1000, 0, 0},
                                     rec::FileData{st, "", 0.5, 3, 0, 0}, rec::FileData{st, "", 0, 3, INFINITY, 0}}) {
        const auto bb = rec::encodeFile(bad);
        CHECK(!rec::decodeFile(bb.data(), bb.size(), fr) && fr == fd);
    }
}

void conics() {
    using conic::Model;
    conic::State s;
    s.model = Model::Circle; s.coef = {0, 0, 1, 2, 3};          // (X-1)^2+(Y-2)^2=9
    auto r = conic::solve(s, conic::Solve::Center);
    CHECK(r.points.size() == 1 && r.points[0][0] == 1 && r.points[0][1] == 2);
    r = conic::solve(s, conic::Solve::Radius);
    CHECK(r.hasValue && r.value == 3 && r.text == "R=3");
    CHECK(conic::solve(s, conic::Solve::Eccentricity).value == 0);
    CHECK_THROWS(conic::solve(s, conic::Solve::Focus));
    s.model = Model::ParabolaY; s.coef = {0.25, 0, 1, -1, 0};    // Y=(X-1)^2/4-1: fuoco (1, 0)
    r = conic::solve(s, conic::Solve::Focus);
    CHECK(r.points[0][0] == 1 && r.points[0][1] == 0);
    r = conic::solve(s, conic::Solve::Vertex);
    CHECK(r.points[0][0] == 1 && r.points[0][1] == -1);
    CHECK(conic::solve(s, conic::Solve::Eccentricity).value == 1);
    s.model = Model::Ellipse; s.coef = {5, 3, 0, 0, 0};
    r = conic::solve(s, conic::Solve::Focus);
    CHECK(r.points.size() == 2 && r.points[1][0] == 4 && r.points[0][0] == -4);
    CHECK_NEAR(conic::solve(s, conic::Solve::Eccentricity).value, 0.8, 1e-15);
    s.model = Model::HyperbolaX; s.coef = {3, 4, 1, 1, 0};
    r = conic::solve(s, conic::Solve::Asymptote);
    CHECK(r.lines.size() == 2 && std::abs(r.lines[0][0] - 4.0 / 3) < 1e-15 && std::abs(r.lines[1][0] + 4.0 / 3) < 1e-15);
    CHECK_NEAR(conic::solve(s, conic::Solve::Eccentricity).value, 5.0 / 3, 1e-15);
    r = conic::solve(s, conic::Solve::Focus);
    CHECK(r.points[0][0] == -4 && r.points[1][0] == 6);
    s.model = Model::HyperbolaY;
    CHECK(conic::solve(s, conic::Solve::Vertex).points[1][1] == 5);
    // i punti delle curve soddisfano l'equazione del modello
    for (std::size_t m = 0; m < conic::ModelCount; ++m) {
        conic::State c;
        c.model = static_cast<Model>(m);
        c.coef = {2, 1.5, 0.5, -0.5, 2};
        CHECK(conic::validate(c).empty());
        const auto bs = conic::branches(c, -10, 10, -10, 10);
        CHECK(!bs.empty());
        for (std::size_t b = 0; b < bs.size(); ++b)
            for (double t = bs[b].t0; t <= bs[b].t1; t += (bs[b].t1 - bs[b].t0) / 7) {
                double x, y;
                CHECK(conic::point(c, b, t, x, y));
                const double X = x - 0.5, Y = y + 0.5;
                double e = 0;
                switch (c.model) {
                case Model::ParabolaX: e = x - (2 * Y * Y + 0.5); break;
                case Model::ParabolaY: e = y - (2 * X * X - 0.5); break;
                case Model::Circle: e = X * X + Y * Y - 4; break;
                case Model::Ellipse: e = X * X / 4 + Y * Y / 2.25 - 1; break;
                case Model::HyperbolaX: e = X * X / 4 - Y * Y / 2.25 - 1; break;
                default: e = Y * Y / 2.25 - X * X / 4 - 1; break;
                }
                CHECK(std::abs(e) < 1e-9 * (1 + std::abs(x) + std::abs(y)));
            }
    }
    s.model = Model::Circle; s.coef[4] = 0;
    CHECK(!conic::validate(s).empty() && conic::branches(s, -1, 1, -1, 1).empty());
    s.model = Model::ParabolaX; s.coef[0] = 0;
    CHECK(!conic::validate(s).empty());
    s.coef = {1, 2, 3, 4, 5}; s.model = Model::Ellipse;
    std::vector<std::uint8_t> b;
    conic::encode(s, b);
    conic::State t;
    CHECK(conic::decode(t, b.data(), b.size()) && t == s);
    b[1] = 9;
    CHECK(!conic::decode(t, b.data(), b.size()));
}

void financial() {
    fin::State s;
    s.smplN = 120; s.smplI = 7; s.smplPV = -1000;
    const auto si = fin::simple(s);
    CHECK_NEAR(si.si, 23.01369863, 1e-8);
    CHECK_NEAR(si.sfv, 1000 + 23.01369863, 1e-6);
    fin::Tvm t;
    t.n = 12; t.i = 5; t.pv = -1000; t.pmt = 0; t.py = 1; t.cy = 1;
    const double fv = fin::solve(fin::TvmVar::FV, t, false);
    CHECK_NEAR(fv, 1795.856326, 1e-6);
    t.fv = fv;
    CHECK_NEAR(fin::solve(fin::TvmVar::I, t, false), 5, 1e-9);
    CHECK_NEAR(fin::solve(fin::TvmVar::N, t, false), 12, 1e-9);
    CHECK_NEAR(fin::solve(fin::TvmVar::PV, t, false), -1000, 1e-9);
    // mutuo: 360 rate mensili, 6% annuo, 100000 ricevuti
    fin::Tvm m;
    m.n = 360; m.i = 6; m.pv = 100000; m.fv = 0; m.py = 12; m.cy = 12;
    const double pmtEnd = fin::solve(fin::TvmVar::PMT, m, false);
    CHECK_NEAR(pmtEnd, -599.5505252, 1e-6);
    const double pmtBgn = fin::solve(fin::TvmVar::PMT, m, true);
    CHECK_NEAR(pmtBgn, pmtEnd / 1.005, 1e-9);
    m.pmt = pmtBgn;
    CHECK_NEAR(fin::solve(fin::TvmVar::I, m, true), 6, 1e-8);
    CHECK_NEAR(fin::solve(fin::TvmVar::N, m, true), 360, 1e-6);
    CHECK_NEAR(fin::solve(fin::TvmVar::FV, m, true), 0, 1e-6);
    CHECK_THROWS(fin::solve(fin::TvmVar::PY, m, false));
    // flussi di cassa
    s.cash = {-1000, 300, 400, 500}; s.cashI = 10;
    const auto cf = fin::cashFlow(s);
    CHECK_NEAR(cf.npv, -1000 + 300 / 1.1 + 400 / 1.21 + 500 / 1.331, 1e-9);
    CHECK_NEAR(cf.nfv, cf.npv * 1.331, 1e-9);
    CHECK(cf.irrOk && std::abs(cf.irr - 8.896) < 1e-3);
    CHECK(!cf.pbpOk);                                           // a 10% non si recupera
    s.cashI = 5;
    CHECK(fin::cashFlow(s).pbpOk);
    // ammortamento: la somma dei capitali di tutte le rate e' il prestito
    m.pmt = pmtEnd;
    s.begin = false; s.pm1 = 1; s.pm2 = 360;
    auto am = fin::amortization(s, m);
    CHECK_NEAR(am.sumPrincipal, -100000, 1e-5);
    CHECK_NEAR(am.bal, 0, 1e-5);
    s.pm1 = s.pm2 = 1;
    am = fin::amortization(s, m);
    CHECK_NEAR(am.interest, -500, 1e-9);
    s.begin = true; m.pmt = pmtBgn; s.pm1 = 1; s.pm2 = 360;
    am = fin::amortization(s, m);
    CHECK_NEAR(am.sumPrincipal, -100000, 1e-5);
    s.pm2 = 1;
    CHECK(fin::amortization(s, m).interest == 0);               // BGN: la prima rata e' tutta capitale
    // tassi e giorni
    s.cnvN = 4; s.cnvI = 12;
    CHECK_NEAR(fin::toEff(s), 12.55088100, 1e-8);
    s.cnvI = fin::toEff(s);
    CHECK_NEAR(fin::toApr(s), 12, 1e-9);
    s.dates = {1, 1, 2026, 12, 31, 2026};
    CHECK(fin::days(s) == 364);
    s.dates = {2, 28, 2024, 3, 1, 2024};
    CHECK(fin::days(s) == 2);
    s.dates = {2, 30, 2026, 3, 1, 2026};
    CHECK_THROWS(fin::days(s));
    std::vector<std::uint8_t> b;
    fin::encode(s, b);
    fin::State back;
    CHECK(fin::decode(back, b.data(), b.size()) && back == s);
    b.resize(b.size() - 1);
    CHECK(!fin::decode(back, b.data(), b.size()));
    // fuzz_modes: un NaN nei dati non e' un valore valido (la sezione si isola)
    fin::State nan = s;
    nan.cash.push_back(NAN);
    std::vector<std::uint8_t> nb;
    fin::encode(nan, nb);
    CHECK(!fin::decode(back, nb.data(), nb.size()));
    // File .pcfin: sezione + variabili TVM; solo numeri finiti
    fin::Tvm tv; tv.n = 360; tv.i = 6; tv.pv = 100000; tv.pmt = -599.55; tv.fv = 0;
    const fin::FileData fd{s, tv};
    const auto fb = fin::encodeFile(fd);
    fin::FileData fr;
    CHECK(fin::decodeFile(fb.data(), fb.size(), fr) && fr == fd);
    CHECK(!fin::decodeFile(fb.data(), fb.size() - 1, fr));
    fin::FileData badTvm = fd; badTvm.tvm.fv = NAN;
    const auto bt = fin::encodeFile(badTvm);
    CHECK(!fin::decodeFile(bt.data(), bt.size(), fr) && fr == fd);
}

void program() {
    for (prog::Dialect d : {prog::Dialect::C, prog::Dialect::Cpp, prog::Dialect::CSharp}) {
        const std::string src = progmode::templateFor(d);
        const std::string file = progmode::encodeProgram(d, src);
        prog::Dialect got = prog::Dialect::C;
        CHECK(progmode::decodeProgram(file, prog::Dialect::Cpp, got) == src && got == d);
        Workspace ws;
        prog::WorkspaceHost host(ws);
        prog::Machine m(prog::compile(src, d), host);
        m.runToEnd();
        CHECK(m.state() == prog::Machine::State::Done && host.output.find("Ciao") != std::string::npos);
    }
    prog::Dialect d = prog::Dialect::C;
    CHECK(progmode::decodeProgram("int main(){}", prog::Dialect::CSharp, d) == "int main(){}" && d == prog::Dialect::CSharp);
    CHECK(progmode::decodeProgram("// pcprog: Rust\nx", prog::Dialect::C, d) == "// pcprog: Rust\nx" && d == prog::Dialect::C);
    bool inComment = false;
    const auto h = progmode::highlight("int x = 42; // ok", prog::Dialect::Cpp, inComment);
    CHECK(h[0] == progmode::Tok::Keyword && h[4] == progmode::Tok::Plain && h[8] == progmode::Tok::Number && h[12] == progmode::Tok::Comment);
    const auto h2 = progmode::highlight("a /* b", prog::Dialect::C, inComment);
    CHECK(inComment && h2[0] == progmode::Tok::Plain && h2[5] == progmode::Tok::Comment);
    const auto h3 = progmode::highlight("c */ \"s\"", prog::Dialect::C, inComment);
    CHECK(!inComment && h3[0] == progmode::Tok::Comment && h3[5] == progmode::Tok::String);
    CHECK(progmode::highlight("#include <x>", prog::Dialect::C, inComment)[3] == progmode::Tok::Preproc);
    // le liste del workspace dal programma (C++, C#, C)
    Workspace ws;
    ws.setList(0, {1, 2, 3});
    prog::WorkspaceHost host(ws);
    prog::Machine m(prog::compile("int main(){ fx::setList(2, 1, fx::getList(1, 3) * 10); fx::setList(2, 2, fx::listSize(1)); return 0; }", prog::Dialect::Cpp), host);
    m.runToEnd();
    CHECK(ws.lists[1] == std::vector<double>({30, 3}));
    prog::Machine bad(prog::compile("int main(){ return (int)fx::getList(1, 9); }", prog::Dialect::Cpp), host);
    CHECK_THROWS(bad.runToEnd());
    prog::Machine cs(prog::compile("class P { static void Main() { Fx.SetList(3, 1, Fx.ListSize(1)); } }", prog::Dialect::CSharp), host);
    cs.runToEnd();
    CHECK(ws.lists[2] == std::vector<double>({3}));
    progmode::State st;
    st.lastFile = "P.pcprog"; st.cursorLine = 4; st.cursorColumn = 7; st.newDialect = 2;
    std::vector<std::uint8_t> b;
    progmode::encode(st, b);
    progmode::State back;
    CHECK(progmode::decode(back, b.data(), b.size()) && back == st);
}

void linkProtocol() {
    using namespace link;
    Frame f;
    f.type = Type::Data; f.seq = 513; f.payload = {1, 2, 3, 0x7E, 'P'};
    const auto bytes = encodeFrame(f);
    CHECK(bytes.size() == FrameOverhead + 5 && bytes[0] == Sync0 && bytes[1] == Sync1);
    Decoder d;
    // rumore prima, a meta' e una trama con il CRC sbagliato in mezzo
    std::vector<std::uint8_t> stream = {'h', 'e', 'l', 'l', 'o', '\n', 0x7E, 0x7E};
    stream.insert(stream.end(), bytes.begin(), bytes.end());
    auto broken = bytes; broken[8] ^= 0xFF;
    stream.insert(stream.end(), broken.begin(), broken.end());
    stream.insert(stream.end(), bytes.begin(), bytes.end());
    for (std::uint8_t c : stream) d.push(&c, 1);           // un byte alla volta
    Frame g;
    int frames = 0;
    while (d.next(g)) { ++frames; CHECK(g.type == Type::Data && g.seq == 513 && g.payload == f.payload); }
    CHECK(frames == 2 && d.discarded() > 0);
    // pacchetto
    std::vector<Entry> es(2);
    es[0].kind = Entry::Kind::Workspace; es[0].data = {9, 9, 9};
    es[1].path = "PICOCALC/SHEET/CONTI.pcsheet"; es[1].data.assign(3000, 7);
    const auto pkg = encodePackage(es);
    std::vector<Entry> back;
    CHECK(decodePackage(pkg.data(), pkg.size(), back) && back == es);
    for (const char* bad : {"PICOCALC/../x", "/etc/passwd", "PICOCALC/", "OTHER/A.txt", "PICOCALC/A B.txt"}) CHECK(!validPath(bad));
    CHECK(validPath("PICOCALC/EACT/F{1}.pceact"));
    es[1].path = "../x";
    const auto evil = encodePackage(es);
    CHECK(!decodePackage(evil.data(), evil.size(), back));
    // trasferimento completo su due porte collegate, con rumore e trame perse
    for (int scenario = 0; scenario < 4; ++scenario) {
        LoopPort a, b;
        connect(a, b);
        std::uint32_t now = 0;
        int writes = 0;
        if (scenario == 1) a.corrupt = [&](std::vector<std::uint8_t>& v) { if (++writes == 3) v[v.size() / 2] ^= 0x55; };       // DATA rovinata
        if (scenario == 2) a.corrupt = [&](std::vector<std::uint8_t>& v) { if (++writes == 2) v.clear(); };                     // DATA persa
        if (scenario == 3) b.corrupt = [&](std::vector<std::uint8_t>& v) { if (++writes == 2) v.clear(); };                     // ACK perso
        std::vector<std::uint8_t> data(5000);
        for (std::size_t i = 0; i < data.size(); ++i) data[i] = static_cast<std::uint8_t>(i * 31);
        Sender tx(a, [&] { return now; }, data, "TEST");
        Receiver rx(b, [&] { return now; });
        for (int k = 0; k < 2000 && (tx.status() == Status::Running || rx.status() == Status::Running); ++k) {
            step::Budget b1; (void)rx.step(b1);
            step::Budget b2; (void)tx.step(b2);
            now += 50;
        }
        CHECK(tx.status() == Status::Done && rx.status() == Status::Done && rx.package() == data && rx.name() == "TEST");
        if (scenario) CHECK(tx.retries() >= 1);
    }
    // annullato dal PC, pacchetto troppo grande, nessuna risposta
    {
        LoopPort a, b; connect(a, b);
        std::uint32_t now = 0;
        Sender tx(a, [&] { return now; }, std::vector<std::uint8_t>(100, 1), "X");
        Receiver rx(b, [&] { return now; }, 50);
        for (int k = 0; k < 20; ++k) { step::Budget b1; (void)rx.step(b1); step::Budget b2; (void)tx.step(b2); now += 10; }
        CHECK(rx.status() == Status::Failed && rx.error().find("Memory ERROR") != std::string::npos);
        CHECK(tx.status() == Status::Failed && tx.error().find("Annullato") != std::string::npos);
    }
    {
        LoopPort a, b; connect(a, b);
        std::uint32_t now = 0;
        Sender tx(a, [&] { return now; }, std::vector<std::uint8_t>(100, 1), "X");
        for (int k = 0; k < 20; ++k) { step::Budget b2; (void)tx.step(b2); now += RetryMs; }
        CHECK(tx.status() == Status::Failed && tx.error().find("Time Out") != std::string::npos && tx.retries() == MaxRetries);
    }
    {
        LoopPort a, b; connect(a, b);
        std::uint32_t now = 0;
        Sender tx(a, [&] { return now; }, std::vector<std::uint8_t>(3000, 1), "X");
        Receiver rx(b, [&] { return now; });
        for (int k = 0; k < 3; ++k) { step::Budget b1; (void)rx.step(b1); step::Budget b2; (void)tx.step(b2); }
        tx.cancelled();                                         // AC sul dispositivo
        for (int k = 0; k < 3; ++k) { step::Budget b1; (void)rx.step(b1); }
        CHECK(tx.status() == Status::Failed && rx.status() == Status::Failed && rx.error().find("Annullato") != std::string::npos);
    }
    State st;
    st.sent = 3; st.received = 2; st.lastBytes = 77; st.lastName = "WORKSPACE"; st.lastError = "Time Out";
    std::vector<std::uint8_t> bb;
    encode(st, bb);
    State sb;
    CHECK(decode(sb, bb.data(), bb.size()) && sb == st);
}

void geometry() {
    geo::State s;
    geo::Object seg{geo::Kind::Segment, {0, 0, 3, 4}};
    geo::add(s, seg);
    geo::add(s, geo::Object{geo::Kind::Polygon, {0, 0, 2, 0, 2, 2, 0, 2}});
    geo::add(s, geo::Object{geo::Kind::Circle, {5, 5, 5, 7}});
    geo::add(s, geo::Object{geo::Kind::Point, {-3, 1}});
    geo::add(s, geo::Object{geo::Kind::Line, {0, -5, 1, -4}});
    CHECK(s.objects.size() == 5);
    CHECK_THROWS(geo::add(s, geo::Object{geo::Kind::Segment, {1, 1, 1, 1}}));
    CHECK_THROWS(geo::add(s, geo::Object{geo::Kind::Polygon, {0, 0, 1, 1}}));
    CHECK_THROWS(geo::add(s, geo::Object{geo::Kind::Point, {NAN, 0}}));
    CHECK(geo::measure(s.objects[0])[0] == "Lunghezza=5");
    const auto poly = geo::measure(s.objects[1]);
    CHECK(poly[0] == "Area=4" && poly[1] == "Perimetro=8" && poly[2] == "Angoli: 90 90 90 90");
    const auto circ = geo::measure(s.objects[2]);
    CHECK(circ[1] == "Raggio=2" && circ[3].rfind("Area=12.566370", 0) == 0);
    CHECK(geo::measure(s.objects[3])[0] == "(-3, 1)");
    CHECK(geo::measure(s.objects[4])[0] == "Y=1X-5");
    CHECK_NEAR(geo::angleDegrees(1, 0, 0, 0, 0, 1), 90, 1e-12);
    CHECK_NEAR(geo::polygonArea(geo::Object{geo::Kind::Polygon, {0, 0, 4, 0, 0, 3}}), 6, 1e-12);
    CHECK(geo::hitTest(s, 1.5, 2.05, 0.2) == 0);
    CHECK(geo::hitTest(s, 5, 7.1, 0.2) == 2);
    CHECK(geo::hitTest(s, 10, 5.05, 0.2) == 4);                 // la retta oltre i suoi due punti (y = x - 5)
    CHECK(geo::hitTest(s, 50, 50, 0.2) == -1);
    geo::move(s.objects[3], 1, -1);
    CHECK(s.objects[3].xy == std::vector<double>({-2, 0}));
    double sx, sy, x, y;
    geo::View v;
    v.cx = 1; v.cy = 2; v.scale = 10;
    geo::toScreen(v, 320, 200, 3, 1, sx, sy);
    geo::toPlane(v, 320, 200, sx, sy, x, y);
    CHECK(std::abs(x - 3) < 1e-12 && std::abs(y - 1) < 1e-12 && sx == 180 && sy == 110);
    geo::zoom(v, 2);
    CHECK(v.scale == 20);
    s.file = "TRI.pcgeo";
    std::vector<std::uint8_t> b;
    geo::encode(s, b);
    geo::State back;
    CHECK(geo::decode(back, b.data(), b.size()) && back == s);
    const auto file = geo::encodeFile(s);
    geo::State fromFile;
    fromFile.file = "TRI.pcgeo";
    CHECK(geo::decodeFile(file.data(), file.size(), fromFile) && fromFile == s);
    for (std::size_t cut = 0; cut < file.size(); cut += 7) { geo::State t; CHECK(!geo::decodeFile(file.data(), cut, t)); }
    geo::State full;
    for (std::size_t i = 0; i < geo::MaxObjects; ++i) geo::add(full, geo::Object{geo::Kind::Point, {double(i), 0}});
    CHECK_THROWS(geo::add(full, geo::Object{geo::Kind::Point, {0, 0}}));
}

void picturePlot() {
    pplot::State s;
    s.window = {0, 10, 0, 20};
    double x, y, px, py;
    pplot::toPlane(s.window, 321, 201, 160, 100, x, y);
    CHECK(x == 5 && y == 10);
    pplot::toScreen(s.window, 321, 201, x, y, px, py);
    CHECK(px == 160 && py == 100);
    for (int i = 0; i < 5; ++i) pplot::addPoint(s, i, 2 * i + 1);
    std::vector<double> xs, ys;
    pplot::coordinates(s, xs, ys);
    const auto r = stats::regress(stats::RegType::Linear, xs, ys);
    CHECK_NEAR(r.coef[0], 2, 1e-12);
    CHECK_NEAR(r.coef[1], 1, 1e-12);
    CHECK(!pplot::validate(pplot::Window{1, 1, 0, 1}).empty() && pplot::validate(s.window).empty());
    s.background = "PICT01.pcpict"; s.regression = 0; s.file = "MELA.pcplot";
    std::vector<std::uint8_t> b;
    pplot::encode(s, b);
    pplot::State back;
    CHECK(pplot::decode(back, b.data(), b.size()) && back == s);
    const auto file = pplot::encodeFile(s);
    pplot::State fromFile;
    fromFile.file = "MELA.pcplot";
    CHECK(pplot::decodeFile(file.data(), file.size(), fromFile) && fromFile == s);
    pplot::State many;
    for (std::size_t i = 0; i < pplot::MaxPoints; ++i) pplot::addPoint(many, 1, 1);
    CHECK_THROWS(pplot::addPoint(many, 1, 1));
}

struct TestCanvas final : py::Canvas {
    int pixels{0}, strings{0}, shows{0};
    void setPixel(int, int, std::uint8_t, std::uint8_t, std::uint8_t) override { ++pixels; }
    bool getPixel(int, int, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) override { r = 1; g = 2; b = 3; return true; }
    void drawString(int, int, const std::string&, std::uint8_t, std::uint8_t, std::uint8_t, bool) override { ++strings; }
    void clear() override {}
    void show() override { ++shows; }
};
std::string console(const py::Session& s) {
    std::string out;
    for (const auto& l : s.console()) out += l + "\n";
    return out;
}
void run(py::Session& s, std::uint32_t& now, int maxSteps = 100000) {
    for (int k = 0; k < maxSteps && s.running() && !s.waitingInput(); ++k) {
        step::Clock clock = [&now] { return now; };
        step::Budget b(64000, 20, &clock);           // unita' (salti della VM) e tempo
        (void)s.step(b);
        now += 25;
    }
}
void python() {
    std::uint32_t now = 0;
    TestCanvas canvas;
    const std::size_t before = mem::stats(mem::Pool::SRAM).used + mem::stats(mem::Pool::PSRAM_BOARD).used;
    {
        py::Session s([&now] { return now; }, &canvas);
        CHECK(py::Session::active() && s.heapBytes() == (mem::available(mem::Pool::PSRAM_BOARD) ? py::PsramHeapBytes : py::SramHeapBytes));
        CHECK_THROWS(py::Session([&now] { return now; }, nullptr));      // una sola sessione
        s.runScript("print(1+2)\nimport math\nprint(math.sqrt(16))\n");
        run(s, now);
        CHECK(s.outcome() == py::Outcome::Ok && console(s).find("3\n4.0\n") != std::string::npos);
        // SHELL: le variabili restano tra le righe; i blocchi chiedono altre righe
        s.runLine("x = 21");
        run(s, now);
        s.runLine("x * 2");
        run(s, now);
        CHECK(console(s).find("42") != std::string::npos);
        CHECK(s.needsMoreInput("for i in range(3):") && !s.needsMoreInput("x = 1"));
        s.runLine("for i in range(3):\n  print(i * 10)\n");
        run(s, now);
        CHECK(console(s).find("0\n10\n20\n") != std::string::npos);
        // input(): lo script aspetta una riga
        s.clearConsole();
        s.runScript("a = input('n? ')\nprint(int(a) * 2)\n");
        run(s, now);
        CHECK(s.waitingInput() && console(s).find("n? ") != std::string::npos);
        s.provideInput("21");
        run(s, now);
        CHECK(!s.running() && console(s).find("42") != std::string::npos);
        // AC: KeyboardInterrupt in un ciclo senza fine e durante input()
        s.runScript("while True:\n  pass\n");
        run(s, now, 20);
        CHECK(s.running());
        s.interrupt();
        run(s, now);
        CHECK(!s.running() && s.outcome() == py::Outcome::Interrupted && console(s).find("KeyboardInterrupt") != std::string::npos);
        s.runScript("input()\n");
        run(s, now);
        CHECK(s.waitingInput());
        s.interrupt();
        run(s, now);
        CHECK(s.outcome() == py::Outcome::Interrupted);
        // eccezioni con la riga, sys.exit, ricorsione profonda (RuntimeError, nessun crash)
        s.clearConsole();
        s.runScript("x = 1\ny = x / 0\n");
        run(s, now);
        CHECK(s.outcome() == py::Outcome::Exception && console(s).find("ZeroDivisionError") != std::string::npos && console(s).find("line 2") != std::string::npos);
        s.runScript("import sys\nsys.exit()\n");
        run(s, now);
        CHECK(s.outcome() == py::Outcome::Exit);
        s.runScript("def f(n):\n  return f(n + 1)\nf(0)\n");
        run(s, now);
        CHECK(s.outcome() == py::Outcome::Exception && console(s).find("RuntimeError") != std::string::npos);
        // memoria dello heap separato: MemoryError, poi la sessione continua
        s.runScript("x = 'y' * 100000000\n");
        run(s, now);
        CHECK(s.outcome() == py::Outcome::Exception && console(s).find("MemoryError") != std::string::npos);
        s.clearConsole();
        s.runScript("a = []\ntry:\n  while True:\n    a.append('x' * 1000)\nexcept MemoryError:\n  a = None\n  print('pieno')\n");
        run(s, now);
        CHECK(s.outcome() == py::Outcome::Ok && console(s).find("pieno") != std::string::npos);
        s.runScript("print('ok')\n");
        run(s, now);
        CHECK(s.outcome() == py::Outcome::Ok);
        // casioplot
        s.runScript("import casioplot as c\nc.set_pixel(1, 2, (255, 0, 0))\nc.draw_string(0, 0, 'hi', (0, 0, 0), 'small')\nprint(c.get_pixel(1, 2))\nc.show_screen()\n");
        run(s, now);
        CHECK(canvas.pixels == 1 && canvas.strings == 1 && canvas.shows == 1 && console(s).find("(1, 2, 3)") != std::string::npos);
        // un'interruzione lasciata a meta' viene chiusa dal distruttore
        s.runScript("while True:\n  pass\n");
        run(s, now, 5);
    }
    CHECK(!py::Session::active());
    const std::size_t after = mem::stats(mem::Pool::SRAM).used + mem::stats(mem::Pool::PSRAM_BOARD).used;
    CHECK(after == before);                                     // heap e pila liberati
    py::State st;
    st.lastScript = "DEMO.py"; st.history = {"1+2", "x=3"};
    std::vector<std::uint8_t> b;
    py::encode(st, b);
    py::State back;
    CHECK(py::decode(back, b.data(), b.size()) && back == st);
}
#if FXAI_MODE_DISTRIBUTION
// v1.18.0: nucleo della modalita' Base Distribution (stato, sezione 0x0117) sul
// nucleo delle distribuzioni condiviso con il DIST di Statistics.
void distribution() {
    dmode::State s;
    dmode::reset(s);
    CHECK(dmode::validate(s).empty() && s.kind == 1 && !s.listData);
    // Parametri iniziali di ogni funzione dalla tabella di dist_core.
    for (std::size_t k = 0; k < dmode::kKinds; ++k) CHECK(dmode::paramsOf(s, k).size() == dist::paramSpecs(dist::kindAt(k)).size());
    CHECK(dmode::paramsOf(s, 1) == std::vector<double>({-1, 1, 1, 0}));          // Normal C.D: Lower, Upper, sigma, mu
    // Andata e ritorno della sezione con valori cambiati
    s.kind = 14; s.params[14] = {0.6, 20, 0.35}; s.params[1] = {-2, 2, 1.5, 0.5}; s.tail = 2; s.listData = true;
    s.list1 = 3; s.list2 = 4; s.saveRes = 7; s.hasResult = true; s.lastResult = 0.25;
    std::vector<std::uint8_t> b;
    dmode::encode(s, b);
    dmode::State back;
    CHECK(dmode::decode(back, b.data(), b.size()) && back == s);
    // Troncata in ogni punto: rifiutata senza toccare lo stato.
    for (std::size_t n = 0; n < b.size(); ++n) { dmode::State t; CHECK(!dmode::decode(t, b.data(), n) && t == dmode::State{}); }
    // Numero di parametri sbagliato, NaN, indici fuori limite: rifiutati.
    dmode::State bad = s;
    bad.params[14] = {0.6, 20};
    b.clear(); dmode::encode(bad, b);
    CHECK(!dmode::decode(back, b.data(), b.size()));
    bad = s; bad.params[1][0] = NAN;
    b.clear(); dmode::encode(bad, b);
    CHECK(!dmode::decode(back, b.data(), b.size()) && !dmode::validate(bad).empty());
    bad = s; bad.kind = 30;
    CHECK(!dmode::validate(bad).empty());
    bad = s; bad.saveRes = 27;
    CHECK(!dmode::validate(bad).empty());
    // Il nucleo condiviso: lo stesso risultato del DIST di Statistics (calc_library).
    dist::Input in; in.values = dmode::paramsOf(s, 14);
    CHECK(dist::compute(dist::kindAt(14), in).main == calclib::invBinomialCD(0.6, 20, 0.35));
    // La sezione nel record v12: solo se diversa dai valori iniziali, isolata se danneggiata.
    Workspace ws;
    std::vector<std::uint8_t> rec = serializeWorkspace(ws);
    Workspace w2; CHECK(deserializeWorkspace(rec.data(), rec.size(), w2) && w2.distribution == dmode::State{});
    ws.distribution = s;
    rec = serializeWorkspace(ws);
    CHECK(deserializeWorkspace(rec.data(), rec.size(), w2) && w2.distribution == s);
    bool isolated = false;
    for (std::size_t i = 0; i + 3 < rec.size(); ++i) {
        if (rec[i] == 0x17 && rec[i + 1] == 0x01 && rec[i + 2] == 0x01) {    // id 0x0117, versione 1
            std::vector<std::uint8_t> bad2 = rec;
            bad2[i + 14] ^= 0x33;
            Workspace w3;
            isolated = deserializeWorkspace(bad2.data(), bad2.size(), w3) && w3.distribution == dmode::State{} &&
                       std::find(w3.damagedSections.begin(), w3.damagedSections.end(), 0x0117) != w3.damagedSections.end();
            break;
        }
    }
    CHECK(isolated);
}
#endif
} // namespace

int main() {
    spreadsheet();
    dynaGraph();
    recursion();
    conics();
    financial();
    program();
    linkProtocol();
    geometry();
    picturePlot();
    python();
#if FXAI_MODE_DISTRIBUTION
    distribution();
#endif
    return testutil::finish("base_modes_test");
}
