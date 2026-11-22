// Nucleo di Recursion (v1.17.0): vedi recursion.hpp.
#include "recursion.hpp"
#include "mode_codec.hpp"
#include <cmath>

namespace fx::rec {
std::string validate(const Setup& s) {
    if (s.formula.empty()) return "Math ERROR: formula non definita";
    if (s.start < 0 || s.end > MaxN || s.start > s.end) return "Range ERROR: 0 <= Start <= End <= 999";
    if (!std::isfinite(s.a0) || !std::isfinite(s.a1)) return "Math ERROR: termini iniziali";
    return {};
}
Table::Table(Setup setup, EvalContext base) : setup_(std::move(setup)), base_(base) {
    const std::string e = validate(setup_);
    if (!e.empty()) throw MathError(e);
    f_ = parseExpression(setup_.formula);
    prev_ = setup_.a0;
    cur_ = setup_.a1;
    rows_.reserve(static_cast<std::size_t>(setup_.end - setup_.start + 1));
}
bool Table::step(step::Budget& budget) {
    while (n_ <= setup_.end) {
        // Termine n: an+1 -> a0 e poi f(n-1, a(n-1)); an+2 -> a0, a1, poi f(n-2, a(n-2), a(n-1)).
        double a;
        if (n_ == 0) a = setup_.a0;
        else if (setup_.type == Type::An2 && n_ == 1) a = setup_.a1;
        else {
            EvalContext c = base_;
            c.complexMode = ComplexMode::Real;
            if (setup_.type == Type::An1) { c.bind("rec_n", n_ - 1); c.bind("rec_an", prev_); }
            else { c.bind("rec_n", n_ - 2); c.bind("rec_an", prev_); c.bind("rec_an1", cur_); }
            try {
                a = evaluateReal(*f_, c);
            } catch (const MathError& e) {
                throw MathError(std::string(e.what()) + " (n=" + std::to_string(n_) + ")");
            }
            if (!std::isfinite(a) || std::abs(a) >= MaxMagnitude) throw MathError("Math ERROR: termine oltre 1E100 (n=" + std::to_string(n_) + ")");
        }
        if (setup_.type == Type::An1) prev_ = a;
        else if (n_ >= 1) { if (n_ >= 2) { prev_ = cur_; cur_ = a; } }
        if (n_ >= setup_.start) { sum_ += a; rows_.push_back(Row{n_, a, sum_}); }
        ++n_;
        if (!budget.consume()) break;
    }
    return n_ > setup_.end;
}
void Table::runAll() { step::Budget b; while (!step(b)) {} }
std::vector<Row> table(const Setup& setup, const EvalContext& base) { Table t(setup, base); t.runAll(); return t.rows(); }
void cobweb(const std::vector<Row>& rows, std::vector<double>& xs, std::vector<double>& ys) {
    xs.clear(); ys.clear();
    for (std::size_t i = 0; i + 1 < rows.size(); ++i) {
        xs.push_back(rows[i].a); ys.push_back(i == 0 ? 0 : rows[i].a);
        xs.push_back(rows[i].a); ys.push_back(rows[i + 1].a);
    }
    if (!rows.empty() && rows.size() >= 2) { xs.push_back(rows.back().a); ys.push_back(rows.back().a); }
}
void reset(State& s) { s = State{}; }
void encode(const State& s, std::vector<std::uint8_t>& out) {
    codec::Writer w{out};
    w.u8(1);
    w.u8(static_cast<std::uint8_t>(s.type));
    w.str(s.an2);
    w.u8(s.sigma ? 1 : 0);
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    codec::Reader r{data, size};
    if (r.u8() != 1) return false;
    State t;
    const std::uint8_t type = r.u8();
    t.an2 = r.str(255);
    const std::uint8_t sigma = r.u8();
    if (!r.end() || type > 1 || sigma > 1) return false;
    t.type = static_cast<Type>(type);
    t.sigma = sigma == 1;
    s = std::move(t);
    return true;
}
std::vector<std::uint8_t> encodeFile(const FileData& f) {
    std::vector<std::uint8_t> sec, out;
    encode(f.state, sec);
    codec::Writer w{out};
    w.blob(sec);
    w.str(f.an1);
    for (double d : {f.start, f.end, f.a0, f.a1}) w.f64(d);
    return out;
}
bool decodeFile(const std::uint8_t* data, std::size_t size, FileData& out) {
    codec::Reader r{data, size};
    const auto sec = r.blob(64 * 1024);
    FileData f;
    f.an1 = r.str(255);
    f.start = r.f64(); f.end = r.f64(); f.a0 = r.f64(); f.a1 = r.f64();
    const bool range = f.start == std::floor(f.start) && f.end == std::floor(f.end) && f.start >= 0 && f.end <= MaxN && f.start <= f.end;
    if (!r.end() || !decode(f.state, sec.data(), sec.size()) || !range || !std::isfinite(f.a0) || !std::isfinite(f.a1)) return false;
    out = std::move(f);
    return true;
}
} // namespace fx::rec
