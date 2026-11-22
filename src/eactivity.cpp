// Nucleo di eActivity (v1.16.0): vedi eactivity.hpp.
#include "eactivity.hpp"
#include "typed_files.hpp"
#include <algorithm>
#include <cstring>

namespace fx::eact {
namespace {
namespace p = fx::part;
constexpr std::uint8_t bit(std::uint8_t id) { return static_cast<std::uint8_t>(1u << id); }
using app::AppId;
// Manuale 10-17: nomi del dialogo STRIP, modalita' e parti del workspace.
constexpr StripInfo kStrips[StripKindCount] = {
    // RUN: anche Graph, perche' i comandi grafici di Run-Matrix scrivono V-Window, SKETCH e grafici dei comandi
    {StripKind::Run, "RUN", AppId::RunMatrix, static_cast<std::uint8_t>(bit(p::History) | bit(p::Graph))},
    {StripKind::Graph, "Graph", AppId::Graph, bit(p::Graph)},
    {StripKind::GraphEditor, "Graph Editor", AppId::Graph, bit(p::Graph)},
    {StripKind::TableEditor, "Table Editor", AppId::Table, static_cast<std::uint8_t>(bit(p::Graph) | bit(p::Table))},
    {StripKind::ConicsGraph, "Conics Graph", AppId::ConicGraphs, 0},
    {StripKind::ConicsEditor, "Conics Editor", AppId::ConicGraphs, 0},
    {StripKind::StatGraph, "Stat Graph", AppId::Statistics, bit(p::Lists)},
    {StripKind::ListEditor, "List Editor", AppId::Statistics, bit(p::Lists)},
    {StripKind::Solver, "Solver", AppId::Equation, bit(p::Equation)},
    {StripKind::RecurEditor, "Recur Editor", AppId::Recursion, 0},
    {StripKind::Notes, "Notes", AppId::EActivity, 0},
    {StripKind::MatrixEditor, "Matrix Editor", AppId::RunMatrix, bit(p::Matrices)},
    {StripKind::VectorEditor, "Vector Editor", AppId::RunMatrix, bit(p::Matrices)},
    {StripKind::SimulEquation, "Simul Equation", AppId::Equation, bit(p::Equation)},
    {StripKind::PolyEquation, "Poly Equation", AppId::Equation, bit(p::Equation)},
    {StripKind::DynamicGraph, "Dynamic Graph", AppId::DynaGraph, 0},
    {StripKind::Financial, "Financial", AppId::Financial, 0},
    {StripKind::SpreadSheet, "SpreadSheet", AppId::Spreadsheet, 0},
    {StripKind::EConTop, "E-CON Top", AppId::ECon4, 0},
    {StripKind::EConResult, "E-CON Result", AppId::ECon4, 0},
    {StripKind::Graph3D, "3D Graph", AppId::Graph3D, bit(p::Graph3D)},
    {StripKind::Graph3DEditor, "3D Graph Editor", AppId::Graph3D, bit(p::Graph3D)},
    {StripKind::Geometry, "Geometry", AppId::Geometry, 0},
    {StripKind::PicturePlot, "Picture Plot", AppId::PicturePlot, 0},
};

struct Out {
    std::vector<std::uint8_t> b;
    void u8(std::uint8_t v) { b.push_back(v); }
    void u16(std::uint16_t v) { u8(static_cast<std::uint8_t>(v)); u8(static_cast<std::uint8_t>(v >> 8)); }
    void u32(std::uint32_t v) { u16(static_cast<std::uint16_t>(v)); u16(static_cast<std::uint16_t>(v >> 16)); }
    void bytes(const void* d, std::size_t n) { const auto* c = static_cast<const std::uint8_t*>(d); b.insert(b.end(), c, c + n); }
    void str(const std::string& s) { u16(static_cast<std::uint16_t>(s.size())); bytes(s.data(), s.size()); }
    void blob(const std::vector<std::uint8_t>& v) { u32(static_cast<std::uint32_t>(v.size())); bytes(v.data(), v.size()); }
};
struct In {
    const std::uint8_t* p; std::size_t n; std::size_t i{0}; bool ok{true};
    bool need(std::size_t k) { if (!ok || k > n - i) { ok = false; return false; } return true; }
    std::uint8_t u8() { if (!need(1)) return 0; return p[i++]; }
    std::uint16_t u16() { const std::uint16_t a = u8(); return static_cast<std::uint16_t>(a | (u8() << 8)); }
    std::uint32_t u32() { const std::uint32_t a = u16(); return a | (static_cast<std::uint32_t>(u16()) << 16); }
    std::string str(std::size_t max) { const auto k = u16(); if (k > max || !need(k)) { ok = false; return {}; } std::string s(reinterpret_cast<const char*>(p + i), k); i += k; return s; }
    std::vector<std::uint8_t> blob(std::size_t max) {
        const auto k = u32();
        if (k > max || !need(k)) { ok = false; return {}; }
        std::vector<std::uint8_t> v(p + i, p + i + k); i += k; return v;
    }
};
bool startsWith(const std::string& s, std::size_t pos, const char* what) { return s.compare(pos, std::strlen(what), what) == 0; }
// Attributi in sequenze (attributo, lunghezza): poche coppie per riga.
void writeAttr(Out& o, const std::vector<std::uint8_t>& a) {
    std::vector<std::pair<std::uint8_t, std::uint16_t>> runs;
    for (std::uint8_t v : a) {
        if (!runs.empty() && runs.back().first == v && runs.back().second < 0xFFFF) ++runs.back().second;
        else runs.emplace_back(v, 1);
    }
    o.u16(static_cast<std::uint16_t>(runs.size()));
    for (const auto& r : runs) { o.u8(r.first); o.u16(r.second); }
}
bool readAttr(In& in, std::size_t length, std::vector<std::uint8_t>& out) {
    const auto n = in.u16();
    out.clear();
    for (std::uint16_t k = 0; k < n && in.ok; ++k) {
        const std::uint8_t v = in.u8();
        const std::uint16_t len = in.u16();
        if (attrColor(v) >= TextColorCount || attrMarker(v) >= MarkerCount || len == 0 || out.size() + len > length) return false;
        out.insert(out.end(), len, v);
    }
    return in.ok && out.size() == length;
}
} // namespace

const StripInfo& stripInfo(StripKind kind) noexcept {
    const auto i = static_cast<std::size_t>(kind);
    return kStrips[i < StripKindCount ? i : 0];
}
bool stripAvailable(StripKind kind) noexcept {
    if (static_cast<std::size_t>(kind) >= StripKindCount) return false;
    const StripInfo& s = stripInfo(kind);
    if (s.app == AppId::ECon4) return false;
    if (kind == StripKind::Notes) return true;
    // la modalita' c'e' su questa scheda e la sua striscia e' realizzata (ha parti da conservare)
    return s.parts != 0 && app::appInfo(s.app).status == app::AppStatus::Available;
}
std::vector<std::uint8_t> stripParts(StripKind kind) {
    std::vector<std::uint8_t> out;
    const std::uint8_t mask = stripInfo(kind).parts;
    for (std::uint8_t id = 1; id < p::Count; ++id) if (mask & bit(id)) out.push_back(id);
    return out;
}

Line Line::textLine(std::string text) {
    Line l;
    l.kind = LineKind::Text;
    l.setText(std::move(text));
    return l;
}
Line Line::calcLine(std::string expression) {
    Line l;
    l.kind = LineKind::Calc;
    l.setText(std::move(expression));
    return l;
}
void Line::setText(std::string t) {
    text = std::move(t);
    attr.assign(text.size(), 0);
}

std::size_t Document::memoCount() const noexcept {
    std::size_t n = 0;
    for (const Line& l : lines) n += l.memo.present ? 1 : 0;
    return n;
}

// ---------------------------------------------------------------- testo con espressioni
std::size_t nextUnit(const std::string& t, std::size_t pos) noexcept {
    if (pos >= t.size()) return t.size();
    if (startsWith(t, pos, IslandStart)) {
        const std::size_t e = t.find(IslandEnd, pos + 3);
        return e == std::string::npos ? t.size() : e + 3;
    }
    std::size_t i = pos + 1;
    while (i < t.size() && (static_cast<unsigned char>(t[i]) & 0xC0) == 0x80) ++i;
    return i;
}
std::size_t prevUnit(const std::string& t, std::size_t pos) noexcept {
    if (pos == 0) return 0;
    if (pos >= 3 && startsWith(t, pos - 3, IslandEnd)) {
        const std::size_t s = t.rfind(IslandStart, pos - 3);
        return s == std::string::npos ? 0 : s;
    }
    std::size_t i = pos - 1;
    while (i > 0 && (static_cast<unsigned char>(t[i]) & 0xC0) == 0x80) --i;
    return i;
}
bool islandAt(const std::string& t, std::size_t pos, std::string& expr, std::size_t& end) {
    if (!startsWith(t, pos, IslandStart)) return false;
    const std::size_t e = t.find(IslandEnd, pos + 3);
    if (e == std::string::npos) return false;
    expr = t.substr(pos + 3, e - pos - 3);
    end = e + 3;
    return true;
}
std::string island(const std::string& expr) { return std::string(IslandStart) + expr + IslandEnd; }
bool validText(const std::string& t) noexcept {
    if (!files::validUtf8Text(reinterpret_cast<const std::uint8_t*>(t.data()), t.size())) return t.empty();
    bool inside = false;
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (startsWith(t, i, IslandStart)) { if (inside) return false; inside = true; i += 2; }
        else if (startsWith(t, i, IslandEnd)) { if (!inside) return false; inside = false; i += 2; }
    }
    return !inside;
}

bool validLineText(const std::string& t) noexcept {
    for (char c : t) if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) return false;
    return validText(t);
}

std::vector<std::vector<Piece>> wrap(const std::string& text, int width,
                                     int (*measure)(const std::string&, std::size_t, std::size_t, bool, void*), void* user) {
    // Parole: sequenze senza spazi; un'espressione e' una parola a se'.
    std::vector<Piece> words;
    std::size_t i = 0;
    while (i < text.size()) {
        if (startsWith(text, i, IslandStart)) {
            const std::size_t e = nextUnit(text, i);
            words.push_back(Piece{i, e, true, 0});
            i = e;
            continue;
        }
        std::size_t j = i;
        if (text[j] == ' ') { while (j < text.size() && text[j] == ' ') ++j; }   // gli spazi restano alla fine della parola precedente
        else { while (j < text.size() && text[j] != ' ' && text[j] != '\n' && !startsWith(text, j, IslandStart)) j = nextUnit(text, j); }
        if (j < text.size() && text[j] == '\n' && j == i) ++j;
        words.push_back(Piece{i, j, false, 0});
        i = j;
    }
    std::vector<std::vector<Piece>> rows(1);
    int x = 0;
    for (Piece w : words) {
        const bool newline = !w.island && w.to > w.from && text[w.from] == '\n';
        if (newline) { rows.emplace_back(); x = 0; continue; }
        w.width = measure(text, w.from, w.to, w.island, user);
        const bool spaces = !w.island && text[w.from] == ' ';
        if (x > 0 && x + w.width > width && !spaces) { rows.emplace_back(); x = 0; }
        // una parola piu' larga della riga si spezza per caratteri (non le espressioni)
        if (!w.island && w.width > width && !spaces) {
            std::size_t a = w.from;
            while (a < w.to) {
                std::size_t b = a;
                int wb = 0;
                while (b < w.to) {
                    const std::size_t c = nextUnit(text, b);
                    const int wc = measure(text, a, c, false, user);
                    if (wc > width - x && b > a) break;
                    b = c; wb = wc;
                }
                rows.back().push_back(Piece{a, b, false, wb});
                x += wb;
                a = b;
                if (a < w.to) { rows.emplace_back(); x = 0; }
            }
            continue;
        }
        rows.back().push_back(w);
        x += w.width;
    }
    return rows;
}

bool validName(const std::string& name) { return files::validTypedName(files::FileType::EActivity, name); }

// ---------------------------------------------------------------- file
std::vector<std::uint8_t> encode(const Document& doc) {
    Out o;
    o.u16(FormatVersion);
    o.u16(static_cast<std::uint16_t>(doc.lines.size()));
    for (const Line& l : doc.lines) {
        o.u8(static_cast<std::uint8_t>(l.kind));
        o.str(l.text);
        const bool attrs = l.kind == LineKind::Text || l.kind == LineKind::Calc;
        if (attrs) writeAttr(o, l.attr);
        o.u8(l.memo.present ? 1 : 0);
        if (l.memo.present) { o.u8(l.memo.color); o.str(l.memo.text); }
        switch (l.kind) {
        case LineKind::Calc: {
            if (l.result) o.blob(encodeHistoryEntries({*l.result}));
            else o.u32(0);
            break;
        }
        case LineKind::Picture: o.u8(l.borderStyle); o.u8(l.borderColor); break;
        case LineKind::Strip: o.u8(l.strip); o.blob(l.data); break;
        default: break;
        }
    }
    o.blob(doc.memories);
    return std::move(o.b);
}

std::size_t encodedSize(const Document& doc) { return encode(doc).size(); }

bool decode(const std::uint8_t* data, std::size_t size, Document& out) {
    In in{data, size};
    const auto version = in.u16();
    if (!in.ok || version == 0 || version > FormatVersion) return false;
    const auto n = in.u16();
    if (!in.ok || n > MaxLines) return false;
    Document doc;
    doc.lines.reserve(n);
    for (std::uint16_t k = 0; k < n && in.ok; ++k) {
        Line l;
        const auto kind = in.u8();
        if (kind > static_cast<std::uint8_t>(LineKind::Strip)) return false;
        l.kind = static_cast<LineKind>(kind);
        l.text = in.str(4000);
        if (!in.ok) return false;
        switch (l.kind) {
        case LineKind::Text: if (l.text.size() > MaxTextLineBytes || !validLineText(l.text)) return false; break;
        case LineKind::Calc: if (l.text.size() > limits::kEquationExpression * 4 || !files::validUtf8Text(reinterpret_cast<const std::uint8_t*>(l.text.data()), l.text.size())) return false; break;
        case LineKind::Stop: if (!l.text.empty()) return false; break;
        case LineKind::Picture: if (!l.text.empty() && !files::validName(files::stem(l.text), 8)) return false; break;
        case LineKind::Strip: if (l.text.size() > MaxStripTitleBytes || !validLineText(l.text)) return false; break;
        }
        if (l.kind == LineKind::Text || l.kind == LineKind::Calc) { if (!readAttr(in, l.text.size(), l.attr)) return false; }
        const auto memo = in.u8();
        if (memo > 1) return false;
        if (memo) {
            if (l.kind != LineKind::Text && l.kind != LineKind::Calc) return false;   // memo solo su testo e calcolo (10-11)
            l.memo.present = true;
            l.memo.color = in.u8();
            l.memo.text = in.str(MaxMemoBytes);
            if (!in.ok || l.memo.color >= TextColorCount || !validText(l.memo.text)) return false;
        }
        switch (l.kind) {
        case LineKind::Calc: {
            const auto bytes = in.blob(1u << 20);
            if (!in.ok) return false;
            if (!bytes.empty()) {
                std::vector<HistoryEntry> h;
                if (!decodeHistoryEntries(bytes.data(), bytes.size(), h) || h.size() != 1) return false;
                l.result = std::make_shared<HistoryEntry>(std::move(h[0]));
            }
            break;
        }
        case LineKind::Picture:
            l.borderStyle = in.u8(); l.borderColor = in.u8();
            if (l.borderStyle > BorderThin || (l.borderColor != BorderClear && (l.borderColor < 1 || l.borderColor > 8))) return false;
            break;
        case LineKind::Strip:
            l.strip = in.u8();                       // anche tipi sconosciuti: conservati
            l.data = in.blob(limits::kEActivityDocumentBytes);
            if (l.strip == static_cast<std::uint8_t>(StripKind::Notes) &&
                !files::validUtf8Text(l.data.data(), l.data.size()) && !l.data.empty()) return false;
            break;
        default: break;
        }
        if (!in.ok) return false;
        doc.lines.push_back(std::move(l));
    }
    doc.memories = in.blob(limits::kEActivityDocumentBytes);
    if (!in.ok || in.i != in.n) return false;
    out = std::move(doc);
    return true;
}

} // namespace fx::eact
