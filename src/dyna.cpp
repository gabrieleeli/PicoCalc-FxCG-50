// Nucleo di Dyna Graph (v1.17.0): vedi dyna.hpp.
#include "dyna.hpp"
#include "expression.hpp"
#include "mode_codec.hpp"
#include <cmath>

namespace fx::dyna {
std::uint32_t frameDelayMs(Speed s) noexcept { return s == Speed::Slow ? 800 : s == Speed::Fast ? 120 : 350; }
const char* speedName(Speed s) noexcept { return s == Speed::Slow ? "Slow" : s == Speed::Fast ? "Fast" : "Normal"; }
std::vector<double> frameValues(double start, double end, double pitch) {
    if (!std::isfinite(start) || !std::isfinite(end) || !std::isfinite(pitch) || pitch == 0 || (end - start) / pitch < 0)
        throw MathError("Argument ERROR: Start, End e Pitch non validi");
    const double count = std::floor((end - start) / pitch + 1e-9) + 1;
    if (count > static_cast<double>(MaxFrames)) throw MathError("Range ERROR: oltre 100 grafici (Pitch troppo piccolo)");
    std::vector<double> v;
    for (int k = 0; k < static_cast<int>(count); ++k) v.push_back(start + k * pitch);
    return v;
}
std::string validate(const State& s) {
    if (s.variable < 'A' || s.variable > 'Z') return "Argument ERROR: coefficiente A-Z";
    if (s.function.empty()) return "Argument ERROR: funzione vuota";
    if (s.function.size() > 255) return "Memory ERROR: funzione oltre 255 caratteri";
    try { (void)parseExpression(s.function); } catch (const std::exception& e) { return e.what(); }
    return {};
}
void reset(State& s) { s = State{}; }
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1);
    w.str(s.function);
    w.u8(static_cast<std::uint8_t>(s.variable));
    w.u8(s.loop ? 1 : 0);
    w.u8(static_cast<std::uint8_t>(s.speed));
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    t.function = r.str(255);
    t.variable = static_cast<char>(r.u8());
    const std::uint8_t loop = r.u8(), speed = r.u8();
    if (!r.end() || loop > 1 || speed > 2 || t.variable < 'A' || t.variable > 'Z' || t.function.empty()) return false;
    t.loop = loop == 1;
    t.speed = static_cast<Speed>(speed);
    s = std::move(t);
    return true;
}
std::vector<std::uint8_t> encodeFile(const FileData& f) {
    std::vector<std::uint8_t> sec, out;
    encode(f.state, sec);
    codec::Writer w{out};
    w.blob(sec);
    w.f64(f.start); w.f64(f.end); w.f64(f.pitch);
    return out;
}
bool decodeFile(const std::uint8_t* data, std::size_t size, FileData& out) {
    codec::Reader r{data, size};
    const auto sec = r.blob(64 * 1024);
    FileData f;
    f.start = r.f64(); f.end = r.f64(); f.pitch = r.f64();
    if (!r.end() || !decode(f.state, sec.data(), sec.size()) || !validate(f.state).empty()) return false;
    try { (void)frameValues(f.start, f.end, f.pitch); } catch (const std::exception&) { return false; }
    out = std::move(f);
    return true;
}
} // namespace fx::dyna
