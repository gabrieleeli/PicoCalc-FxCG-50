#include "workspace.hpp"
#include <cmath>
#include <cstring>

namespace fx {
Workspace::Workspace() { resetAll(); }

void Workspace::resetAll() {
    settings = Settings{};
    memory.clearAll();
    history.clear();
    for (std::size_t i = 0; i < functions.size(); ++i) functions[i] = emptyGraphFunction(i);
    functions[0].expr = "sin(x)";
    view = ViewWindow{};
    graph = GraphState{};
    table = TableRange{};
    for (auto& l : lists) l.clear();
    equation = EquationState{};
    chat.clear();
}

void Workspace::apply() const { setAngleMode(settings.angle); }

EvalContext Workspace::evalContext() const {
    EvalContext c;
    c.memory = &memory;
    c.complexMode = settings.complex;
    c.names = this;
    return c;
}

bool Workspace::resolve(const std::string& n, double& out) const {
    const struct { const char* name; double value; } vars[] = {
        {"xmin", view.xmin}, {"xmax", view.xmax}, {"xscl", view.xscl}, {"ymin", view.ymin}, {"ymax", view.ymax},
        {"yscl", view.yscl}, {"tmin", view.tmin}, {"tmax", view.tmax}, {"tptch", view.tstep},
        {"tblstart", table.start}, {"tblend", table.end}, {"tblpitch", table.step}, {"xdot", viewXdot(view)}};
    for (const auto& v : vars) if (n == v.name) { out = v.value; return true; }
    return false;
}

void Workspace::addHistory(HistoryEntry entry) {
    history.push_back(std::move(entry));
    if (history.size() > MaxHistory) history.erase(history.begin());
}

// ---------------------------------------------------------------- serialisation
namespace {
constexpr std::uint32_t Magic = 0x46584157;   // "WAXF"
// Version 2 (v1.7.0) adds exact values, the Frac Result setting and complex Ans;
// version 3 (v1.8.0) the GRAPH data: 20 functions with line style and the
// parametric Yt, Setup items, V-Window/graph memories, sketches, overlay and
// project name. Version 1 (v1.6.0) and 2 records are still accepted.
constexpr std::uint16_t Version = 3;
constexpr std::size_t LegacyFunctions = 6;

struct Writer {
    std::vector<std::uint8_t> out;
    void u8(std::uint8_t v) { out.push_back(v); }
    void u16(std::uint16_t v) { u8(static_cast<std::uint8_t>(v)); u8(static_cast<std::uint8_t>(v >> 8)); }
    void u32(std::uint32_t v) { u16(static_cast<std::uint16_t>(v)); u16(static_cast<std::uint16_t>(v >> 16)); }
    void f64(double v) { std::uint64_t b; std::memcpy(&b, &v, 8); u32(static_cast<std::uint32_t>(b)); u32(static_cast<std::uint32_t>(b >> 32)); }
    void str(const std::string& s) { const auto n = static_cast<std::uint16_t>(std::min<std::size_t>(s.size(), 4000)); u16(n); out.insert(out.end(), s.begin(), s.begin() + n); }
    void blob(const std::string& s) { u32(static_cast<std::uint32_t>(s.size())); out.insert(out.end(), s.begin(), s.end()); }
    void f32(float v) { std::uint32_t b; std::memcpy(&b, &v, 4); u32(b); }
    void i64(std::int64_t v) { const auto b = static_cast<std::uint64_t>(v); u32(static_cast<std::uint32_t>(b)); u32(static_cast<std::uint32_t>(b >> 32)); }
    void slot(const ExactSlot& e) {
        u8(static_cast<std::uint8_t>((e.valid ? 1 : 0) | (e.decimal ? 2 : 0) | (e.value.form() == exact::Value::Form::Pi ? 4 : 0)));
        u8(static_cast<std::uint8_t>(e.value.termCount()));
        for (std::size_t i = 0; i < e.value.termCount(); ++i) { const auto& t = e.value.term(i); i64(t.coef.num); i64(t.coef.den); i64(t.radicand); }
    }
};
struct Reader {
    const std::uint8_t* p; std::size_t n; std::size_t i{}; bool ok{true};
    bool need(std::size_t k) { if (!ok || i + k > n) { ok = false; return false; } return true; }
    std::uint8_t u8() { if (!need(1)) return 0; return p[i++]; }
    std::uint16_t u16() { const std::uint16_t a = u8(); return static_cast<std::uint16_t>(a | (u8() << 8)); }
    std::uint32_t u32() { const std::uint32_t a = u16(); return a | (static_cast<std::uint32_t>(u16()) << 16); }
    double f64() { const std::uint64_t a = u32(); const std::uint64_t b = u32(); const std::uint64_t bits = a | (b << 32); double v; std::memcpy(&v, &bits, 8); return v; }
    std::string str() { const std::uint16_t k = u16(); if (!need(k)) return {}; std::string s(reinterpret_cast<const char*>(p + i), k); i += k; return s; }
    std::string blob(std::size_t max) { const std::uint32_t k = u32(); if (k > max) { ok = false; return {}; } if (!need(k)) return {}; std::string s(reinterpret_cast<const char*>(p + i), k); i += k; return s; }
    float f32() { const std::uint32_t b = u32(); float v; std::memcpy(&v, &b, 4); return v; }
    std::int64_t i64() { const std::uint64_t a = u32(); const std::uint64_t b = u32(); return static_cast<std::int64_t>(a | (b << 32)); }
    // Exact slots are rebuilt through the normalising constructors, so a
    // corrupted record can never produce a non-canonical value.
    ExactSlot slot() {
        ExactSlot e;
        const std::uint8_t flags = u8(), count = u8();
        if (count > exact::Value::MaxTerms || flags > 7) { ok = false; return e; }
        exact::Term t[exact::Value::MaxTerms]{};
        for (std::uint8_t k = 0; k < count; ++k) {
            const std::int64_t num = i64(), den = i64(), rad = i64();
            if (!exact::Rational::make(num, den, t[k].coef) || !(t[k].coef == exact::Rational{num, den}) || rad < 1 || rad > exact::Value::MaxRadicand) { ok = false; return e; }
            t[k].radicand = rad;
        }
        if (flags & 4) {
            if (count != 1 || t[0].radicand != 1) { ok = false; return e; }
            e.value = exact::Value::piTimes(t[0].coef);
        } else if (!exact::Value::fromTerms(t, count, e.value) || e.value.termCount() != count) { ok = false; return e; }
        e.valid = (flags & 1) != 0; e.decimal = (flags & 2) != 0;
        return e;
    }
};
} // namespace

std::vector<std::uint8_t> serializeWorkspace(const Workspace& ws) {
    Writer w;
    w.u32(Magic); w.u16(Version);
    const Settings& s = ws.settings;
    w.u8(static_cast<std::uint8_t>(s.angle)); w.u8(static_cast<std::uint8_t>(s.display.mode)); w.u8(s.display.digits);
    w.u8(static_cast<std::uint8_t>(s.complex)); w.u8(s.fractionResults ? 1 : 0); w.u8(s.lcdBacklight); w.u8(s.kbdBacklight);
    w.str(s.wifiSsid); w.str(s.wifiPassword); w.str(s.relayHost); w.u16(s.relayPort); w.str(s.relayPath); w.str(s.relayToken);
    w.u8(s.aiProvider); w.str(s.aiModel); w.u16(s.aiMaxChars);
    w.u8(s.mixedFractions ? 1 : 0);
    for (char v = 'A'; v <= 'Z'; ++v) { w.f64(ws.memory.recall(v)); w.slot(ws.memory.exactSlot(v)); }
    const CalculatorMemory& m = ws.memory;
    w.f64(m.answer()); w.f64(m.answerImag()); w.slot(m.exactAnswerSlot());
    w.f64(m.previousAnswer()); w.f64(m.previousAnswerImag()); w.slot(m.exactPreviousAnswerSlot());
    w.u16(static_cast<std::uint16_t>(ws.history.size()));
    for (const auto& h : ws.history) {
        w.str(h.expression); w.f64(h.value.real()); w.f64(h.value.imag()); w.str(h.error); w.u8(h.view);
        w.slot(h.exact); w.u8(static_cast<std::uint8_t>((h.numerical ? 1 : 0) | (h.flipMixed ? 2 : 0)));
    }
    w.u8(static_cast<std::uint8_t>(ws.functions.size()));
    for (const auto& f : ws.functions) {
        w.str(f.expr); w.u8(static_cast<std::uint8_t>(f.type)); w.u8(f.selected ? 1 : 0); w.u8(f.color);
        w.u8(static_cast<std::uint8_t>(f.style)); w.str(f.expr2);
    }
    auto view = [&](const ViewWindow& v) { for (double d : {v.xmin, v.xmax, v.xscl, v.ymin, v.ymax, v.yscl, v.tmin, v.tmax, v.tstep}) w.f64(d); };
    view(ws.view);
    const GraphState& g = ws.graph;
    const GraphSettings& gs = g.settings;
    w.u8(static_cast<std::uint8_t>(gs.funcType));
    w.u8(static_cast<std::uint8_t>((gs.plotDraw ? 1 : 0) | (gs.derivative ? 2 : 0) | (gs.coord ? 4 : 0) | (gs.label ? 8 : 0) |
                                   (gs.graphFunc ? 16 : 0) | (gs.simulGraph ? 32 : 0) | (gs.ineqUnion ? 64 : 0)));
    w.u8(static_cast<std::uint8_t>(gs.grid)); w.u8(static_cast<std::uint8_t>(gs.axes));
    w.u8(static_cast<std::uint8_t>(gs.sketchLine)); w.u8(gs.plotColor);
    w.f64(gs.xFactor); w.f64(gs.yFactor);
    w.u8(gs.background); w.str(gs.backgroundFile); w.u8(gs.fade);
    w.u8(g.vmemUsed);
    for (const auto& m : g.vmem) view(m);
    for (const auto& m : g.gmem) w.blob(m);
    w.u16(static_cast<std::uint16_t>(g.sketch.size()));
    for (const auto& s : g.sketch) {
        w.u8(static_cast<std::uint8_t>(s.kind)); w.f64(s.x0); w.f64(s.y0); w.f64(s.x1); w.f64(s.y1);
        w.u8(s.color); w.u8(static_cast<std::uint8_t>(s.style)); w.str(s.text);
        w.u16(static_cast<std::uint16_t>(s.path.size())); for (float f : s.path) w.f32(f);
    }
    w.str(g.overlay); w.str(g.project); w.u8(g.projectModified ? 1 : 0);
    w.f64(ws.table.start); w.f64(ws.table.end); w.f64(ws.table.step);
    for (const auto& l : ws.lists) { w.u16(static_cast<std::uint16_t>(l.size())); for (double d : l) w.f64(d); }
    const EquationState& e = ws.equation;
    w.u8(e.unknowns); for (double d : e.simultaneous) w.f64(d);
    w.u8(e.degree); for (double d : e.polynomial) w.f64(d);
    w.str(e.solverExpr); w.f64(e.solverGuess);
    w.u8(e.ineqDegree); w.u8(e.ineqRelation); for (double d : e.inequality) w.f64(d);
    w.u16(static_cast<std::uint16_t>(ws.chat.size()));
    for (const auto& m : ws.chat) { w.u8(m.fromUser ? 1 : 0); w.str(m.text); }
    return std::move(w.out);
}

bool deserializeWorkspace(const std::uint8_t* data, std::size_t size, Workspace& out) {
    if (!data) return false;
    Reader r{data, size};
    if (r.u32() != Magic || !r.ok) return false;
    const std::uint16_t version = r.u16();
    if (version < 1 || version > Version || !r.ok) return false;
    const bool v2 = version >= 2;
    Workspace ws;
    Settings& s = ws.settings;
    const auto angle = r.u8(); if (angle > 2) return false; s.angle = static_cast<AngleMode>(angle);
    const auto dmode = r.u8(); if (dmode > 4) return false; s.display.mode = static_cast<DisplayMode>(dmode);
    s.display.digits = r.u8(); if (s.display.digits > 10) return false;
    const auto cm = r.u8(); if (cm > 2) return false; s.complex = static_cast<ComplexMode>(cm);
    s.fractionResults = r.u8() != 0; s.lcdBacklight = r.u8(); s.kbdBacklight = r.u8();
    s.wifiSsid = r.str(); s.wifiPassword = r.str(); s.relayHost = r.str(); s.relayPort = r.u16(); s.relayPath = r.str(); s.relayToken = r.str();
    s.aiProvider = r.u8(); s.aiModel = r.str(); s.aiMaxChars = r.u16();
    if (v2) { const auto mixed = r.u8(); if (mixed > 1) return false; s.mixedFractions = mixed != 0; }
    for (char v = 'A'; v <= 'Z'; ++v) {
        const double d = r.f64();
        const ExactSlot e = v2 ? r.slot() : ExactSlot{};
        if (e.valid && !e.value.isZero()) ws.memory.storeExact(v, d, e.value, e.decimal);
        else if (d != 0) ws.memory.store(v, d);
    }
    if (v2) {
        const double ans = r.f64(), ansIm = r.f64(); const ExactSlot ansE = r.slot();
        const double pre = r.f64(), preIm = r.f64(); const ExactSlot preE = r.slot();
        ws.memory.restoreAnswers(ans, ansIm, ansE, pre, preIm, preE);
    } else {
        const double ans = r.f64(), pre = r.f64();
        ws.memory.setAnswer(pre); ws.memory.setAnswer(ans);
    }
    const auto hn = r.u16(); if (hn > Workspace::MaxHistory) return false;
    for (std::uint16_t i = 0; i < hn && r.ok; ++i) {
        HistoryEntry h; h.expression = r.str(); const double re = r.f64(), im = r.f64(); h.value = {re, im}; h.error = r.str(); h.view = r.u8();
        if (h.view > 7) return false;
        if (v2) {
            h.exact = r.slot();
            const auto flags = r.u8(); if (flags > 3) return false;
            h.numerical = (flags & 1) != 0; h.flipMixed = (flags & 2) != 0;
        }
        ws.history.push_back(std::move(h));
    }
    auto readView = [&](ViewWindow& v) { for (double* d : {&v.xmin, &v.xmax, &v.xscl, &v.ymin, &v.ymax, &v.yscl, &v.tmin, &v.tmax, &v.tstep}) *d = r.f64(); };
    ViewWindow& v = ws.view;
    if (version >= 3) {
        const auto count = r.u8(); if (count != ws.functions.size()) return false;
        for (auto& f : ws.functions) {
            f.expr = r.str(); const auto t = r.u8(); if (t >= GraphTypeCount) return false; f.type = static_cast<GraphType>(t);
            f.selected = r.u8() != 0; f.color = r.u8(); if (f.color >= GraphColorCount) return false;
            const auto st = r.u8(); if (st >= LineStyleCount) return false; f.style = static_cast<LineStyle>(st);
            f.expr2 = r.str();
            if (f.expr.size() > MaxGraphExpression || f.expr2.size() > MaxGraphExpression) return false;
        }
        readView(v);
        GraphState& g = ws.graph;
        GraphSettings& gs = g.settings;
        const auto ft = r.u8(); if (ft >= GraphTypeCount) return false; gs.funcType = static_cast<GraphType>(ft);
        const auto flags = r.u8(); if (flags > 127) return false;
        gs.plotDraw = flags & 1; gs.derivative = flags & 2; gs.coord = flags & 4; gs.label = flags & 8;
        gs.graphFunc = flags & 16; gs.simulGraph = flags & 32; gs.ineqUnion = flags & 64;
        const auto grid = r.u8(), axes = r.u8(), sk = r.u8(); if (grid > 2 || axes > 2 || sk >= LineStyleCount) return false;
        gs.grid = static_cast<GridMode>(grid); gs.axes = static_cast<AxesMode>(axes); gs.sketchLine = static_cast<LineStyle>(sk);
        gs.plotColor = r.u8(); if (gs.plotColor >= GraphColorCount) return false;
        gs.xFactor = r.f64(); gs.yFactor = r.f64();
        if (!(gs.xFactor > 0) || !(gs.yFactor > 0) || !std::isfinite(gs.xFactor) || !std::isfinite(gs.yFactor)) return false;
        gs.background = r.u8(); gs.backgroundFile = r.str(); gs.fade = r.u8();
        if (gs.background > BackgroundFile || gs.fade > 100) return false;
        g.vmemUsed = r.u8(); if (g.vmemUsed >> VWindowMemories) return false;
        for (auto& m : g.vmem) readView(m);
        std::size_t gbytes = 0;
        for (auto& m : g.gmem) { m = r.blob(MaxGraphMemoryBytes); gbytes += m.size(); }
        if (gbytes > MaxGraphMemoryBytes) return false;
        const auto sn = r.u16(); if (sn > MaxSketchItems) return false;
        for (std::uint16_t i = 0; i < sn && r.ok; ++i) {
            SketchItem s;
            const auto kind = r.u8(); s.kind = static_cast<SketchItem::Kind>(kind);
            s.x0 = r.f64(); s.y0 = r.f64(); s.x1 = r.f64(); s.y1 = r.f64();
            s.color = r.u8(); const auto style = r.u8(); if (style >= LineStyleCount) return false; s.style = static_cast<LineStyle>(style);
            s.text = r.str();
            const auto pn = r.u16(); if (pn > 2 * MaxPenPoints) return false;
            s.path.resize(pn); for (float& f : s.path) f = r.f32();
            if (r.ok && !validateSketch(s).empty()) return false;
            g.sketch.push_back(std::move(s));
        }
        g.overlay = r.str(); g.project = r.str(); g.projectModified = r.u8() != 0;
    } else {
        for (std::size_t i = 0; i < LegacyFunctions; ++i) {
            GraphFunction& f = ws.functions[i];
            f.expr = r.str(); const auto t = r.u8(); if (t > 5) return false; f.type = static_cast<GraphType>(t);
            f.selected = r.u8() != 0; f.color = static_cast<std::uint8_t>(r.u8() % 6);
        }
        for (std::size_t i = LegacyFunctions; i < ws.functions.size(); ++i) ws.functions[i] = emptyGraphFunction(i);
        readView(v);
        ws.graph.settings.grid = r.u8() != 0 ? GridMode::On : GridMode::Off;   // v1/v2 kept the grid in the V-Window
    }
    ws.table.start = r.f64(); ws.table.end = r.f64(); ws.table.step = r.f64();
    for (auto& l : ws.lists) {
        const auto n = r.u16(); if (n > Workspace::MaxListLength) return false;
        l.resize(n); for (auto& d : l) d = r.f64();
    }
    EquationState& e = ws.equation;
    e.unknowns = r.u8(); for (double& d : e.simultaneous) d = r.f64();
    e.degree = r.u8(); for (double& d : e.polynomial) d = r.f64();
    e.solverExpr = r.str(); e.solverGuess = r.f64();
    e.ineqDegree = r.u8(); e.ineqRelation = r.u8(); for (double& d : e.inequality) d = r.f64();
    if (e.unknowns < 2 || e.unknowns > 6 || e.degree < 2 || e.degree > 6 || e.ineqDegree < 1 || e.ineqDegree > 6 || e.ineqRelation > 3) return false;
    const auto cn = r.u16(); if (cn > Workspace::MaxChatMessages) return false;
    for (std::uint16_t i = 0; i < cn && r.ok; ++i) { ChatMessage m; m.fromUser = r.u8() != 0; m.text = r.str(); ws.chat.push_back(std::move(m)); }
    if (!r.ok || r.i != r.n) return false;
    if (version < 3) {
        // Older forms accepted values the v1.8.0 checks reject: keep the record
        // (the user's data) and repair only the window parameters.
        if (v.xmin < v.xmax && v.ymin < v.ymax) {
            v.xscl = std::abs(v.xscl); v.yscl = std::abs(v.yscl);
            if (!std::isfinite(v.tstep) || v.tstep == 0 || v.tmin == v.tmax || (v.tmax - v.tmin) / v.tstep < 0) {
                const ViewWindow d; v.tmin = d.tmin; v.tmax = d.tmax; v.tstep = d.tstep;
            }
        }
        if (!validateView(v).empty()) v = ViewWindow{};
    }
    if (!validateView(v).empty()) return false;
    out = std::move(ws);
    return true;
}
} // namespace fx
