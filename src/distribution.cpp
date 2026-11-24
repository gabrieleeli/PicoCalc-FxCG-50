// Nucleo della modalita' Distribution (v1.18.0, Base, solo Plus): vedi distribution.hpp.
#include "distribution.hpp"
#include "dist_core.hpp"
#include "mode_codec.hpp"
#include <cmath>

namespace fx::dmode {
static_assert(kKinds == dist::KindCount, "una voce dello stato per funzione");

bool State::operator==(const State& o) const noexcept {
    return kind == o.kind && params == o.params && tail == o.tail && listData == o.listData && list1 == o.list1 && list2 == o.list2 &&
           saveRes == o.saveRes && hasResult == o.hasResult && lastResult == o.lastResult;
}
void reset(State& s) { s = State{}; }
std::vector<double> paramsOf(const State& s, std::size_t kind) {
    kind %= kKinds;
    if (!s.params[kind].empty()) return s.params[kind];
    std::vector<double> v;
    for (const dist::ParamSpec& p : dist::paramSpecs(dist::kindAt(kind))) v.push_back(p.def);
    return v;
}
std::string validate(const State& s) {
    if (s.kind >= kKinds) return "funzione fuori elenco";
    for (std::size_t k = 0; k < kKinds; ++k) {
        if (s.params[k].empty()) continue;
        if (s.params[k].size() != dist::paramSpecs(dist::kindAt(k)).size()) return "numero di parametri";
        for (double v : s.params[k]) if (!std::isfinite(v)) return "parametro non finito";
    }
    if (s.tail > 2 || s.list1 >= 26 || s.list2 >= 26 || s.saveRes > 26) return "impostazione fuori limiti";
    if (!std::isfinite(s.lastResult)) return "risultato non finito";
    return {};
}
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1);
    w.u8(s.kind); w.u8(s.tail); w.u8(s.listData ? 1 : 0); w.u8(s.list1); w.u8(s.list2); w.u8(s.saveRes);
    w.u8(s.hasResult ? 1 : 0); w.f64(s.lastResult);
    std::uint8_t used = 0; for (const auto& p : s.params) if (!p.empty()) ++used;
    w.u8(used);
    for (std::size_t k = 0; k < kKinds; ++k) {
        if (s.params[k].empty()) continue;
        w.u8(static_cast<std::uint8_t>(k)); w.u8(static_cast<std::uint8_t>(s.params[k].size()));
        for (double v : s.params[k]) w.f64(v);
    }
}
bool decode(State& out, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State s;
    s.kind = r.u8(); s.tail = r.u8();
    const std::uint8_t list = r.u8();
    s.list1 = r.u8(); s.list2 = r.u8(); s.saveRes = r.u8();
    const std::uint8_t has = r.u8();
    s.lastResult = r.f64();
    if (!r.ok || list > 1 || has > 1) return false;
    s.listData = list == 1; s.hasResult = has == 1;
    const std::uint8_t used = r.u8();
    for (std::uint8_t i = 0; i < used && r.ok; ++i) {
        const std::uint8_t k = r.u8(), n = r.u8();
        if (!r.ok || k >= kKinds || !s.params[k].empty() || n == 0 || n > 6) return false;
        for (std::uint8_t j = 0; j < n; ++j) s.params[k].push_back(r.f64());
    }
    if (!r.end() || !validate(s).empty()) return false;
    out = std::move(s);
    return true;
}
} // namespace fx::dmode
