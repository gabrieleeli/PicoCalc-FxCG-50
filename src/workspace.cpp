#include "workspace.hpp"
#include <cstring>

namespace fx {
const char* graphTypeLabel(GraphType t) noexcept {
    switch (t) {
    case GraphType::Y: return "Y=";
    case GraphType::Polar: return "r=";
    case GraphType::YGreater: return "Y>";
    case GraphType::YLess: return "Y<";
    case GraphType::YGreaterEq: return "Y\xE2\x89\xA5";
    case GraphType::YLessEq: return "Y\xE2\x89\xA4";
    }
    return "Y=";
}

Workspace::Workspace() { resetAll(); }

void Workspace::resetAll() {
    settings = Settings{};
    memory.clearAll();
    history.clear();
    for (std::size_t i = 0; i < functions.size(); ++i) functions[i] = GraphFunction{{}, GraphType::Y, true, static_cast<std::uint8_t>(i)};
    functions[0].expr = "sin(x)";
    view = ViewWindow{};
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
    return c;
}

void Workspace::addHistory(HistoryEntry entry) {
    history.push_back(std::move(entry));
    if (history.size() > MaxHistory) history.erase(history.begin());
}

// ---------------------------------------------------------------- serialisation
namespace {
constexpr std::uint32_t Magic = 0x46584157;   // "WAXF"
constexpr std::uint16_t Version = 1;

struct Writer {
    std::vector<std::uint8_t> out;
    void u8(std::uint8_t v) { out.push_back(v); }
    void u16(std::uint16_t v) { u8(static_cast<std::uint8_t>(v)); u8(static_cast<std::uint8_t>(v >> 8)); }
    void u32(std::uint32_t v) { u16(static_cast<std::uint16_t>(v)); u16(static_cast<std::uint16_t>(v >> 16)); }
    void f64(double v) { std::uint64_t b; std::memcpy(&b, &v, 8); u32(static_cast<std::uint32_t>(b)); u32(static_cast<std::uint32_t>(b >> 32)); }
    void str(const std::string& s) { const auto n = static_cast<std::uint16_t>(std::min<std::size_t>(s.size(), 4000)); u16(n); out.insert(out.end(), s.begin(), s.begin() + n); }
};
struct Reader {
    const std::uint8_t* p; std::size_t n; std::size_t i{}; bool ok{true};
    bool need(std::size_t k) { if (!ok || i + k > n) { ok = false; return false; } return true; }
    std::uint8_t u8() { if (!need(1)) return 0; return p[i++]; }
    std::uint16_t u16() { const std::uint16_t a = u8(); return static_cast<std::uint16_t>(a | (u8() << 8)); }
    std::uint32_t u32() { const std::uint32_t a = u16(); return a | (static_cast<std::uint32_t>(u16()) << 16); }
    double f64() { const std::uint64_t a = u32(); const std::uint64_t b = u32(); const std::uint64_t bits = a | (b << 32); double v; std::memcpy(&v, &bits, 8); return v; }
    std::string str() { const std::uint16_t k = u16(); if (!need(k)) return {}; std::string s(reinterpret_cast<const char*>(p + i), k); i += k; return s; }
};
} // namespace

std::vector<std::uint8_t> serializeWorkspace(const Workspace& ws) {
    Writer w;
    w.u32(Magic); w.u16(Version);
    const Settings& s = ws.settings;
    w.u8(static_cast<std::uint8_t>(s.angle)); w.u8(static_cast<std::uint8_t>(s.display.mode)); w.u8(s.display.digits);
    w.u8(static_cast<std::uint8_t>(s.complex)); w.u8(s.fractionResults ? 1 : 0); w.u8(s.lcdBacklight); w.u8(s.kbdBacklight);
    w.str(s.wifiSsid); w.str(s.wifiPassword); w.str(s.relayHost); w.u16(s.relayPort); w.str(s.relayPath); w.str(s.relayToken);
    w.u8(s.aiProvider); w.str(s.aiModel); w.u16(s.aiMaxTokens);
    for (char v = 'A'; v <= 'Z'; ++v) w.f64(ws.memory.recall(v));
    w.f64(ws.memory.answer()); w.f64(ws.memory.previousAnswer());
    w.u16(static_cast<std::uint16_t>(ws.history.size()));
    for (const auto& h : ws.history) { w.str(h.expression); w.f64(h.value.real()); w.f64(h.value.imag()); w.str(h.error); w.u8(h.view); }
    for (const auto& f : ws.functions) { w.str(f.expr); w.u8(static_cast<std::uint8_t>(f.type)); w.u8(f.selected ? 1 : 0); w.u8(f.color); }
    const ViewWindow& v = ws.view;
    for (double d : {v.xmin, v.xmax, v.xscl, v.ymin, v.ymax, v.yscl, v.tmin, v.tmax, v.tstep}) w.f64(d);
    w.u8(v.grid ? 1 : 0);
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
    if (r.u32() != Magic || r.u16() != Version || !r.ok) return false;
    Workspace ws;
    Settings& s = ws.settings;
    const auto angle = r.u8(); if (angle > 2) return false; s.angle = static_cast<AngleMode>(angle);
    const auto dmode = r.u8(); if (dmode > 4) return false; s.display.mode = static_cast<DisplayMode>(dmode);
    s.display.digits = r.u8(); if (s.display.digits > 10) return false;
    const auto cm = r.u8(); if (cm > 2) return false; s.complex = static_cast<ComplexMode>(cm);
    s.fractionResults = r.u8() != 0; s.lcdBacklight = r.u8(); s.kbdBacklight = r.u8();
    s.wifiSsid = r.str(); s.wifiPassword = r.str(); s.relayHost = r.str(); s.relayPort = r.u16(); s.relayPath = r.str(); s.relayToken = r.str();
    s.aiProvider = r.u8(); s.aiModel = r.str(); s.aiMaxTokens = r.u16();
    for (char v = 'A'; v <= 'Z'; ++v) { const double d = r.f64(); if (d != 0) ws.memory.store(v, d); }
    const double ans = r.f64(), pre = r.f64();
    ws.memory.setAnswer(pre); ws.memory.setAnswer(ans);
    const auto hn = r.u16(); if (hn > Workspace::MaxHistory) return false;
    for (std::uint16_t i = 0; i < hn && r.ok; ++i) {
        HistoryEntry h; h.expression = r.str(); const double re = r.f64(), im = r.f64(); h.value = {re, im}; h.error = r.str(); h.view = r.u8();
        ws.history.push_back(std::move(h));
    }
    for (auto& f : ws.functions) {
        f.expr = r.str(); const auto t = r.u8(); if (t > 5) return false; f.type = static_cast<GraphType>(t);
        f.selected = r.u8() != 0; f.color = static_cast<std::uint8_t>(r.u8() % 6);
    }
    ViewWindow& v = ws.view;
    for (double* d : {&v.xmin, &v.xmax, &v.xscl, &v.ymin, &v.ymax, &v.yscl, &v.tmin, &v.tmax, &v.tstep}) *d = r.f64();
    v.grid = r.u8() != 0;
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
    if (!(v.xmin < v.xmax) || !(v.ymin < v.ymax)) return false;
    out = std::move(ws);
    return true;
}
} // namespace fx
