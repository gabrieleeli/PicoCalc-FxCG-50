// Nucleo di Financial (v1.17.0): vedi financial.hpp.
#include "financial.hpp"
#include "calc_library.hpp"
#include "expression.hpp"
#include "mode_codec.hpp"
#include <cmath>

namespace fx::fin {
const char* tvmName(TvmVar v) noexcept {
    static const char* const n[] = {"n", "I%", "PV", "PMT", "FV", "P/Y", "C/Y"};
    return n[static_cast<int>(v) % 7];
}
double& Tvm::at(TvmVar v) {
    switch (v) { case TvmVar::N: return n; case TvmVar::I: return i; case TvmVar::PV: return pv; case TvmVar::PMT: return pmt; case TvmVar::FV: return fv; case TvmVar::PY: return py; default: return cy; }
}
double Tvm::at(TvmVar v) const { return const_cast<Tvm*>(this)->at(v); }
double solve(TvmVar unknown, const Tvm& t, bool begin) {
    if (unknown == TvmVar::PY || unknown == TvmVar::CY) throw MathError("Argument ERROR: P/Y e C/Y non si risolvono");
    const double v = calclib::cmpdSolve(static_cast<int>(unknown), t.n, t.i, t.pv, t.pmt, t.fv, t.py, t.cy, begin);
    if (!std::isfinite(v)) throw MathError("Math ERROR: risultato non finito");
    return v;
}
bool State::operator==(const State& o) const noexcept {
    return begin == o.begin && smplN == o.smplN && smplI == o.smplI && smplPV == o.smplPV && cash == o.cash && cashI == o.cashI &&
           pm1 == o.pm1 && pm2 == o.pm2 && cnvN == o.cnvN && cnvI == o.cnvI && dates == o.dates;
}
SimpleResult simple(const State& s) { return {calclib::smplSI(s.smplN, s.smplI, s.smplPV), calclib::smplSFV(s.smplN, s.smplI, s.smplPV)}; }
CashResult cashFlow(const State& s) {
    CashResult r{};
    r.npv = calclib::cashNPV(s.cashI, s.cash);
    r.nfv = calclib::cashNFV(s.cashI, s.cash);
    try { r.irr = calclib::cashIRR(s.cash); r.irrOk = true; } catch (const MathError&) { r.irrOk = false; }
    try { r.pbp = calclib::cashPBP(s.cashI, s.cash); r.pbpOk = true; } catch (const MathError&) { r.pbpOk = false; }
    return r;
}
AmortResult amortization(const State& s, const Tvm& t) {
    AmortResult r{};
    double* out[] = {&r.bal, &r.interest, &r.principal, &r.sumInterest, &r.sumPrincipal};
    for (int k = 0; k < 5; ++k) *out[k] = calclib::amortizationAt(k, s.pm1, s.pm2, t.i, t.pv, t.pmt, t.py, t.cy, s.begin);
    return r;
}
double toEff(const State& s) { return calclib::cnvtEFF(s.cnvN, s.cnvI); }
double toApr(const State& s) { return calclib::cnvtAPR(s.cnvN, s.cnvI); }
double days(const State& s) { return calclib::daysPrd(s.dates[0], s.dates[1], s.dates[2], s.dates[3], s.dates[4], s.dates[5]); }
void reset(State& s) { s = State{}; }
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1);
    w.u8(s.begin ? 1 : 0);
    for (double v : {s.smplN, s.smplI, s.smplPV, s.cashI, s.pm1, s.pm2, s.cnvN, s.cnvI}) w.f64(v);
    for (double v : s.dates) w.f64(v);
    w.u16(static_cast<std::uint16_t>(s.cash.size()));
    for (double v : s.cash) w.f64(v);
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    const std::uint8_t b = r.u8();
    for (double* v : {&t.smplN, &t.smplI, &t.smplPV, &t.cashI, &t.pm1, &t.pm2, &t.cnvN, &t.cnvI}) *v = r.f64();
    for (double& v : t.dates) v = r.f64();
    const std::size_t n = r.u16();
    if (n > 999) return false;
    t.cash.assign(n, 0);
    for (double& v : t.cash) v = r.f64();
    if (!r.end() || b > 1) return false;
    // v1.17.0 (fuzz_modes): solo numeri finiti (NaN o infinito: sezione danneggiata).
    for (double v : {t.smplN, t.smplI, t.smplPV, t.cashI, t.pm1, t.pm2, t.cnvN, t.cnvI}) if (!std::isfinite(v)) return false;
    for (double v : t.dates) if (!std::isfinite(v)) return false;
    for (double v : t.cash) if (!std::isfinite(v)) return false;
    t.begin = b == 1;
    s = std::move(t);
    return true;
}
bool FileData::operator==(const FileData& o) const noexcept {
    const Tvm& a = tvm;
    const Tvm& b = o.tvm;
    return state == o.state && a.n == b.n && a.i == b.i && a.pv == b.pv && a.pmt == b.pmt && a.fv == b.fv && a.py == b.py && a.cy == b.cy;
}
std::vector<std::uint8_t> encodeFile(const FileData& f) {
    std::vector<std::uint8_t> sec, out;
    encode(f.state, sec);
    codec::Writer w{out};
    w.blob(sec);
    const Tvm& t = f.tvm;
    for (double d : {t.n, t.i, t.pv, t.pmt, t.fv, t.py, t.cy}) w.f64(d);
    return out;
}
bool decodeFile(const std::uint8_t* data, std::size_t size, FileData& out) {
    codec::Reader r{data, size};
    const auto sec = r.blob(64 * 1024);
    FileData f;
    Tvm& t = f.tvm;
    for (double* d : {&t.n, &t.i, &t.pv, &t.pmt, &t.fv, &t.py, &t.cy}) *d = r.f64();
    if (!r.end() || !decode(f.state, sec.data(), sec.size())) return false;
    for (double d : {t.n, t.i, t.pv, t.pmt, t.fv, t.py, t.cy}) if (!std::isfinite(d)) return false;
    out = std::move(f);
    return true;
}
} // namespace fx::fin
