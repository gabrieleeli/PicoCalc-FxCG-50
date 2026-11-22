// Nucleo di Spreadsheet (v1.17.0): vedi spreadsheet.hpp.
#include "spreadsheet.hpp"
#include "binding.hpp"
#include "limits.hpp"
#include "mode_codec.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <deque>
#include <set>

namespace fx::sheet {
namespace {
bool upperCol(char c) noexcept { return std::isalpha(static_cast<unsigned char>(c)) != 0; }
// Riferimento A1..Z999 ($ facoltativi) che inizia in s[p], come il lexer
// (cellAt in expression.cpp): fine del riferimento o npos.
std::size_t refAt(const std::string& s, std::size_t p, Ref& r, bool& absCol, bool& absRow) {
    absCol = absRow = false;
    if (p < s.size() && s[p] == '$') { absCol = true; ++p; }
    if (p >= s.size() || !upperCol(s[p])) return std::string::npos;
    r.col = std::tolower(static_cast<unsigned char>(s[p])) - 'a';
    ++p;
    if (p < s.size() && s[p] == '$') { absRow = true; ++p; }
    const std::size_t d = p;
    while (p < s.size() && std::isdigit(static_cast<unsigned char>(s[p]))) ++p;
    if (p == d || p - d > 3 || s[d] == '0') return std::string::npos;
    if (p < s.size() && (std::isalnum(static_cast<unsigned char>(s[p])) || s[p] == '_')) return std::string::npos;
    r.row = std::atoi(s.substr(d, p - d).c_str());
    return p;
}
bool wordChar(char c) noexcept { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }
// Inizio possibile di un riferimento: non dentro una parola (Sum, Ans, xA1...).
bool boundary(const std::string& s, std::size_t p) noexcept { return p == 0 || (!wordChar(s[p - 1]) && s[p - 1] != '$'); }
} // namespace

std::string refName(Ref r) { return std::string(1, static_cast<char>('A' + r.col)) + std::to_string(r.row); }
bool parseRef(const std::string& text, Ref& out) noexcept {
    Ref r;
    bool a, b;
    if (refAt(text, 0, r, a, b) != text.size() || !r.valid()) return false;
    out = r;
    return true;
}

Kind classify(const std::string& input, double& number) {
    number = 0;
    if (input.empty()) return Kind::Empty;
    if (input[0] == '=') return Kind::Formula;
    // Numero: solo cifre, punto, esponente e segni, letto dal parser unico.
    bool digit = false;
    for (char c : input) {
        if (std::isdigit(static_cast<unsigned char>(c))) digit = true;
        else if (!(c == '.' || c == 'E' || c == 'e' || c == '-' || c == '+' || c == ' ')) return Kind::Text;
    }
    if (!digit) return Kind::Text;
    try {
        number = evaluateReal(*parseExpression(input), EvalContext{});
        return Kind::Number;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (...) {
        return Kind::Text;
    }
}

std::size_t Sheet::maxCells() noexcept { return limits::ram().graphLayerCache ? 26000 : 3000; }   // PSRAM attiva / solo SRAM
void Sheet::set(Ref r, const std::string& input) {
    if (!r.valid()) throw MathError("Range ERROR: cella fuori dal foglio");
    if (input.empty()) { clear(r); return; }
    if (input.size() > MaxInput) throw MathError("Memory ERROR: cella oltre 255 caratteri");
    const auto k = key(r);
    if (cells_.find(k) == cells_.end() && cells_.size() >= maxCells()) throw MathError("Memory ERROR: troppe celle per la memoria");
    Cell c;
    c.input = input;
    c.kind = classify(input, c.value);
    if (c.kind == Kind::Formula) {
        ParseOptions opt;
        opt.cellReferences = true;
        (void)parseExpression(input.substr(1), opt);      // Syntax ERROR subito, la cella non cambia
        c.stale = true;
    }
    cells_[k] = std::move(c);
}
void Sheet::clear(Ref r) noexcept { cells_.erase(key(r)); }
const Cell* Sheet::cell(Ref r) const noexcept {
    const auto it = cells_.find(key(r));
    return it == cells_.end() ? nullptr : &it->second;
}
Cell* Sheet::cell(Ref r) noexcept {
    const auto it = cells_.find(key(r));
    return it == cells_.end() ? nullptr : &it->second;
}
bool Sheet::numberAt(Ref r, double& out) const noexcept {
    const Cell* c = cell(r);
    if (!c || !c->numeric()) return false;
    out = c->value;
    return true;
}
bool Sheet::operator==(const Sheet& o) const {
    if (cells_.size() != o.cells_.size()) return false;
    for (auto a = cells_.begin(), b = o.cells_.begin(); a != cells_.end(); ++a, ++b)
        if (a->first != b->first || a->second.input != b->second.input) return false;
    return true;
}

bool shiftFormula(const std::string& f, int dCol, int dRow, std::string& out) {
    out.clear();
    bool ok = true;
    bool inString = false;
    for (std::size_t p = 0; p < f.size();) {
        const char c = f[p];
        if (c == '"') inString = !inString;
        if (!inString && (c == '$' || upperCol(c)) && boundary(f, p)) {
            Ref r;
            bool ac, ar;
            const std::size_t e = refAt(f, p, r, ac, ar);
            if (e != std::string::npos) {
                if (!ac) r.col += dCol;
                if (!ar) r.row += dRow;
                if (!r.valid()) { ok = false; r.col = std::clamp(r.col, 0, Columns - 1); r.row = std::clamp(r.row, 1, Rows); }
                out += ac ? "$" : "";
                out += static_cast<char>('A' + r.col);
                out += ar ? "$" : "";
                out += std::to_string(r.row);
                p = e;
                continue;
            }
        }
        out += c;
        ++p;
    }
    return ok;
}

std::vector<Block> formulaReferences(const std::string& f) {
    std::vector<Block> out;
    bool inString = false;
    for (std::size_t p = 0; p < f.size();) {
        const char c = f[p];
        if (c == '"') inString = !inString;
        if (!inString && (c == '$' || upperCol(c)) && boundary(f, p)) {
            Ref a;
            bool x, y;
            const std::size_t e = refAt(f, p, a, x, y);
            if (e != std::string::npos) {
                Ref b = a;
                std::size_t end = e;
                if (e < f.size() && f[e] == ':') {
                    const std::size_t e2 = refAt(f, e + 1, b, x, y);
                    if (e2 != std::string::npos) end = e2; else b = a;
                }
                if (a.valid() && b.valid()) out.push_back(Block{a, b});
                p = end;
                continue;
            }
        }
        ++p;
    }
    return out;
}

// ---------------------------------------------------------------- ricalcolo
Recalc::Recalc(Sheet& sheet, EvalContext base) : sheet_(sheet), base_(base) {
    // Formule e loro dipendenze verso altre formule; Kahn: nessuna ricorsione.
    std::vector<std::uint32_t> formulas;
    for (const auto& [k, c] : sheet_.cells()) if (c.kind == Kind::Formula) formulas.push_back(k);
    std::map<std::uint32_t, std::size_t> index;
    for (std::size_t i = 0; i < formulas.size(); ++i) index[formulas[i]] = i;
    std::vector<std::vector<std::size_t>> users(formulas.size());   // users[j]: formule che dipendono da j
    std::vector<std::size_t> pending(formulas.size(), 0);
    for (std::size_t i = 0; i < formulas.size(); ++i) {
        const Cell* c = sheet_.cell(refOf(formulas[i]));
        std::set<std::size_t> deps;
        for (const Block& b : formulaReferences(c->input.substr(1))) {
            // Solo le formule dentro il blocco (le celle sono in ordine riga, colonna).
            for (int row = b.r0(); row <= b.r1(); ++row) {
                const auto lo = sheet_.cells().lower_bound(key(Ref{b.c0(), row}));
                const auto hi = sheet_.cells().upper_bound(key(Ref{b.c1(), row}));
                for (auto it = lo; it != hi; ++it)
                    if (it->second.kind == Kind::Formula) deps.insert(index[it->first]);
            }
        }
        for (std::size_t d : deps) { users[d].push_back(i); ++pending[i]; }
    }
    std::deque<std::size_t> ready;
    for (std::size_t i = 0; i < formulas.size(); ++i) if (pending[i] == 0) ready.push_back(i);
    std::vector<bool> placed(formulas.size(), false);
    while (!ready.empty()) {
        const std::size_t i = ready.front();
        ready.pop_front();
        placed[i] = true;
        order_.push_back(formulas[i]);
        for (std::size_t u : users[i]) if (--pending[u] == 0) ready.push_back(u);
    }
    // Le formule rimaste sono in un ciclo (o dipendono da un ciclo).
    for (std::size_t i = 0; i < formulas.size(); ++i) {
        if (placed[i]) continue;
        Cell* c = sheet_.cell(refOf(formulas[i]));
        c->error = CircularError;
        c->stale = false;
        c->value = 0;
    }
    for (std::uint32_t k : order_) sheet_.cell(refOf(k))->stale = true;
}
void Recalc::evaluate(std::uint32_t k) {
    Cell* c = sheet_.cell(refOf(k));
    if (!c) return;
    c->stale = false;
    c->error.clear();
    bind::BindingResolver r(base_.names, base_.collections);
    r.setCells([this](bind::CellRef cr, double& out) { return sheet_.numberAt(Ref{cr.col, cr.row}, out); });
    try {
        ParseOptions opt;
        opt.cellReferences = true;
        const auto tree = parseExpression(c->input.substr(1), opt);
        EvalContext ec = r.context(base_);
        ec.complexMode = ComplexMode::Real;
        c->value = evaluateReal(*tree, ec);
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& e) {
        std::string m = e.what();
        const std::size_t colon = m.find(':');
        c->error = colon == std::string::npos ? m : m.substr(0, colon);
        c->value = 0;
    }
}
bool Recalc::step(step::Budget& budget) {
    while (next_ < order_.size()) {
        evaluate(order_[next_++]);
        if (!budget.consume()) break;
    }
    return next_ >= order_.size();
}
void Recalc::cancelled() {}
void Recalc::runAll() { while (next_ < order_.size()) evaluate(order_[next_++]); }
void recalculate(Sheet& sheet, const EvalContext& base) { Recalc r(sheet, base); r.runAll(); }

// ---------------------------------------------------------------- appunti
void copyBlock(const Sheet& sheet, Block b, bool cut, Clipboard& out) {
    Clipboard c;
    c.valid = true;
    c.cut = cut;
    c.source = b;
    for (int row = b.r0(); row <= b.r1(); ++row)
        for (int col = b.c0(); col <= b.c1(); ++col)
            if (const Cell* cell = sheet.cell(Ref{col, row})) c.items.push_back({Ref{col - b.c0(), row - b.r0()}, cell->input});
    out = std::move(c);
}
void paste(Sheet& sheet, Clipboard& clip, Ref at) {
    if (!clip.valid) return;
    const int w = clip.source.c1() - clip.source.c0(), h = clip.source.r1() - clip.source.r0();
    if (at.col + w >= Columns || at.row + h > Rows) throw MathError("Range ERROR: il blocco esce dal foglio");
    const int dc = at.col - clip.source.c0(), dr = at.row - clip.source.r0();
    // Prima tutto in una copia: garanzia forte (Memory ERROR, Syntax ERROR).
    Sheet next = sheet;
    if (clip.cut) clearBlock(next, clip.source);
    for (int row = at.row; row <= at.row + h; ++row)
        for (int col = at.col; col <= at.col + w; ++col) next.clear(Ref{col, row});
    for (const auto& [rel, input] : clip.items) {
        std::string text = input;
        if (!text.empty() && text[0] == '=' && !clip.cut) {
            std::string shifted;
            if (!shiftFormula(text, dc, dr, shifted)) throw MathError("Range ERROR: riferimento fuori dal foglio");
            text = shifted;
        }
        next.set(Ref{at.col + rel.col, at.row + rel.row}, text);
    }
    sheet = std::move(next);
    if (clip.cut) clip = Clipboard{};
}
void fill(Sheet& sheet, Block b, const std::string& input) {
    Sheet next = sheet;
    for (int row = b.r0(); row <= b.r1(); ++row)
        for (int col = b.c0(); col <= b.c1(); ++col) {
            std::string text = input;
            if (!text.empty() && text[0] == '=') {
                std::string shifted;
                if (!shiftFormula(text, col - b.c0(), row - b.r0(), shifted)) throw MathError("Range ERROR: riferimento fuori dal foglio");
                text = shifted;
            }
            next.set(Ref{col, row}, text);
        }
    sheet = std::move(next);
}
void clearBlock(Sheet& sheet, Block b) noexcept {
    for (int row = b.r0(); row <= b.r1(); ++row)
        for (int col = b.c0(); col <= b.c1(); ++col) sheet.clear(Ref{col, row});
}

std::vector<double> columnValues(const Sheet& sheet, int col, int r0, int r1) {
    std::vector<double> out;
    for (int row = r0; row <= r1; ++row) {
        double v;
        if (sheet.numberAt(Ref{col, row}, v)) out.push_back(v);
    }
    return out;
}
void setColumn(Sheet& sheet, int col, int r0, const std::vector<double>& values) {
    if (r0 + static_cast<int>(values.size()) - 1 > Rows) throw MathError("Range ERROR: la lista esce dal foglio");
    Sheet next = sheet;
    for (std::size_t i = 0; i < values.size(); ++i) {
        // Il numero con 15 cifre significative, letto poi dal parser unico.
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%.15G", values[i]);
        next.set(Ref{col, r0 + static_cast<int>(i)}, buf);
    }
    sheet = std::move(next);
}

// ---------------------------------------------------------------- codifica
namespace {
struct Out {
    std::vector<std::uint8_t>& b;
    void u8(std::uint8_t v) { b.push_back(v); }
    void u16(std::uint16_t v) { u8(static_cast<std::uint8_t>(v)); u8(static_cast<std::uint8_t>(v >> 8)); }
    void u32(std::uint32_t v) { u16(static_cast<std::uint16_t>(v)); u16(static_cast<std::uint16_t>(v >> 16)); }
    void str(const std::string& s) { u16(static_cast<std::uint16_t>(s.size())); b.insert(b.end(), s.begin(), s.end()); }
};
struct In {
    const std::uint8_t* p; std::size_t n; std::size_t i{0}; bool ok{true};
    bool need(std::size_t k) { if (!ok || k > n - i) ok = false; return ok; }
    std::uint8_t u8() { return need(1) ? p[i++] : 0; }
    std::uint16_t u16() { const std::uint16_t a = u8(); return static_cast<std::uint16_t>(a | (u8() << 8)); }
    std::uint32_t u32() { const std::uint32_t a = u16(); return a | (static_cast<std::uint32_t>(u16()) << 16); }
    std::string str(std::size_t max) {
        const std::size_t k = u16();
        if (k > max || !need(k)) { ok = false; return {}; }
        std::string s(reinterpret_cast<const char*>(p + i), k);
        i += k;
        return s;
    }
};
void writeCells(Out& o, const Sheet& s) {
    o.u32(static_cast<std::uint32_t>(s.size()));
    for (const auto& [k, c] : s.cells()) { o.u16(static_cast<std::uint16_t>(k)); o.str(c.input); }
}
bool readCells(In& in, Sheet& s) {
    const std::uint32_t n = in.u32();
    if (!in.ok || n > static_cast<std::uint32_t>(Columns * Rows)) return false;
    Sheet out;
    std::uint32_t previous = 0;
    for (std::uint32_t i = 0; i < n && in.ok; ++i) {
        const std::uint32_t k = in.u16();
        const std::string input = in.str(MaxInput);
        const Ref r = refOf(k);
        if (!in.ok || !r.valid() || (i > 0 && k <= previous) || input.empty()) return false;
        previous = k;
        try { out.set(r, input); } catch (const std::bad_alloc&) { throw; } catch (...) { return false; }
    }
    if (!in.ok) return false;
    s = std::move(out);
    return true;
}
} // namespace

void reset(State& s) { s = State{}; }
void encode(const State& s, std::vector<std::uint8_t>& out) {
    Out o{out};
    o.u8(1);                                        // versione dei dati
    writeCells(o, s.sheet);
    o.u8(static_cast<std::uint8_t>(s.cursor.col)); o.u16(static_cast<std::uint16_t>(s.cursor.row));
    o.u8(static_cast<std::uint8_t>(s.left)); o.u16(static_cast<std::uint16_t>(s.top));
    o.str(s.file);
    o.u8(s.autoCalc ? 1 : 0);
}
bool decode(State& s, const std::uint8_t* data, std::size_t size) {
    In in{data, size};
    if (in.u8() != 1) return false;
    State t;
    if (!readCells(in, t.sheet)) return false;
    t.cursor.col = in.u8(); t.cursor.row = in.u16();
    t.left = in.u8(); t.top = in.u16();
    t.file = in.str(64);
    const std::uint8_t ac = in.u8();
    if (!in.ok || in.i != in.n || !t.cursor.valid() || t.left >= Columns || t.top < 1 || t.top > Rows || ac > 1) return false;
    t.autoCalc = ac == 1;
    s = std::move(t);
    return true;
}
std::vector<std::uint8_t> encodeFile(const Sheet& sheet) {
    std::vector<std::uint8_t> out;
    Out o{out};
    o.u8(1);
    writeCells(o, sheet);
    return out;
}
bool decodeFile(const std::uint8_t* data, std::size_t size, Sheet& out) {
    In in{data, size};
    if (in.u8() != 1) return false;
    Sheet s;
    if (!readCells(in, s) || in.i != in.n) return false;
    out = std::move(s);
    return true;
}

} // namespace fx::sheet
