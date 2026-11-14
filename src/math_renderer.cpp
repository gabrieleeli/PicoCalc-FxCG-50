// Two-dimensional layout of the shared VPAM tree (fractions, radicals, powers,
// limits, calculus templates). Measuring and drawing use one box model
// (width, ascent, descent) so the cursor is placed with exactly the metrics
// used to draw the expression.
#include "math_renderer.hpp"
#include "font_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace fx::math {
namespace {
using ast::Kind;
constexpr int MaxDepth = 32;

struct Face {
    std::uint8_t id; int adv, height, base, topPad;
    int asc() const { return base - topPad; }
    int desc() const { return height - base; }
    int axis() const { return id == FaceMedium ? 5 : 3; }   // fraction bar height above the baseline
};
const Face kFaces[2] = {{FaceMedium, 8, 16, 14, 2}, {FaceSmall, 6, 12, 9, 1}};
const Face& faceFor(int level) { return kFaces[level > 0 ? 1 : 0]; }
const Face& smallFace() { return kFaces[1]; }

struct Box { int w{}, asc{}, desc{}; };
int satAdd(int a, int b) noexcept { const long long v = static_cast<long long>(a) + b; return static_cast<int>(std::clamp<long long>(v, -1000000, 1000000)); }

const char* const kGreekNames[] = {"alpha","beta","gamma","delta","epsilon","zeta","eta","theta","iota","kappa","lambda","mu","nu","xi","omicron","pi","rho","sigma","tau","upsilon","phi","chi","psi","omega"};
const char* const kGreekGlyphs[] = {"α","β","γ","δ","ε","ζ","η","θ","ι","κ","λ","μ","ν","ξ","ο","π","ρ","σ","τ","υ","φ","χ","ψ","ω"};

struct Ctx {
    const CursorTarget* cursor{};
    std::vector<NaturalDrawOp>* out{};
    Layout* layout{};
    int parenDepth{};          // nesting level of the parentheses drawn so far
};

int textWidth(const std::string& s, const Face& f) { return static_cast<int>(font::glyphCount(s.c_str())) * f.adv; }
Box faceBox(const Face& f, int w) { return {w, f.asc(), f.desc()}; }
Box measureNode(const NaturalNode& n, int level, bool isRoot, unsigned depth);

Box measureRow(const NaturalNode& n, int level, bool isRoot, unsigned depth) {
    const Face& f = faceFor(level);
    if (n.children.empty()) return faceBox(f, isRoot ? 0 : f.adv);
    Box b{0, f.asc(), f.desc()};
    for (const auto& c : n.children) {
        if (!c) continue;
        const Box q = measureNode(*c, level, false, depth + 1);
        b.w = satAdd(b.w, q.w); b.asc = std::max(b.asc, q.asc); b.desc = std::max(b.desc, q.desc);
    }
    return b;
}
const NaturalNode* child(const NaturalNode& n, std::size_t i) { return i < n.children.size() ? n.children[i].get() : nullptr; }
bool shapeOk(const NaturalNode& n) {
    if (!ast::validShape(n)) return false;
    for (const auto& c : n.children) if (!c) return false;
    return true;
}

// Grid of a Matrix node: cells in row-major order, `cols` per row; every
// column is as wide as its widest cell, every row as tall as its tallest one.
struct MatrixGrid {
    std::size_t rows{}, cols{};
    std::vector<int> width, asc, desc;
    int w{}, h{};
};
constexpr int MatrixColumnGap = 8, MatrixRowGap = 2, MatrixBracket = 5;
MatrixGrid matrixGrid(const NaturalNode& n, int level, unsigned depth) {
    MatrixGrid g;
    g.cols = ast::matrixColumns(n);
    if (g.cols == 0) return g;
    g.rows = n.children.size() / g.cols;
    g.width.assign(g.cols, 0); g.asc.assign(g.rows, 0); g.desc.assign(g.rows, 0);
    for (std::size_t i = 0; i < g.rows * g.cols; ++i) {
        const Box b = measureNode(*n.children[i], level, false, depth + 1);
        const std::size_t r = i / g.cols, c = i % g.cols;
        g.width[c] = std::max(g.width[c], b.w);
        g.asc[r] = std::max(g.asc[r], b.asc); g.desc[r] = std::max(g.desc[r], b.desc);
    }
    for (std::size_t c = 0; c < g.cols; ++c) g.w = satAdd(g.w, g.width[c] + (c ? MatrixColumnGap : 0));
    for (std::size_t r = 0; r < g.rows; ++r) g.h = satAdd(g.h, g.asc[r] + g.desc[r] + (r ? MatrixRowGap : 0));
    g.w = satAdd(g.w, 2 * MatrixBracket);
    return g;
}

Box measureNode(const NaturalNode& n, int level, bool isRoot, unsigned depth) {
    const Face& f = faceFor(level);
    if (depth > MaxDepth) return faceBox(f, 0);
    switch (n.kind) {
    case Kind::Token: return faceBox(f, textWidth(displayToken(n.text), f));
    case Kind::Row: return measureRow(n, level, isRoot, depth);
    default: break;
    }
    if (!shapeOk(n)) return {0, f.asc(), f.desc()};
    switch (n.kind) {
    case Kind::Fraction: {
        const Box a = measureNode(*child(n, 0), level, false, depth + 1), b = measureNode(*child(n, 1), level, false, depth + 1);
        const int axis = f.axis();
        return {std::max(a.w, b.w) + 4, axis + 2 + a.desc + a.asc, std::max(f.desc(), b.asc + b.desc + 2 - axis)};
    }
    case Kind::Radical: {
        const Box c = measureNode(*child(n, 0), level, false, depth + 1);
        const bool showIndex = !(n.text.empty() || n.text == "2");
        const int prefix = showIndex ? std::max(0, textWidth(n.text, smallFace()) - 3) : 0;
        return {prefix + 8 + c.w + 2, c.asc + 3, c.desc + 1};
    }
    case Kind::Power: {
        const Box b = measureNode(*child(n, 0), level, false, depth + 1), e = measureNode(*child(n, 1), level + 1, false, depth + 1);
        const int raise = b.asc / 2 + e.desc;
        return {b.w + e.w + 1, std::max(b.asc, raise + e.asc), b.desc};
    }
    case Kind::Subscript: {
        const Box b = measureNode(*child(n, 0), level, false, depth + 1), s = measureNode(*child(n, 1), level + 1, false, depth + 1);
        return {b.w + s.w + 1, b.asc, std::max(b.desc, 3 + s.desc)};
    }
    case Kind::Group: {
        const Box c = measureNode(*child(n, 0), level, false, depth + 1);
        return {c.w + 2 * f.adv, std::max(c.asc, f.asc()), std::max(c.desc, f.desc())};
    }
    case Kind::Call: {
        if (n.text == "abs" && n.children.size() == 1) {
            const Box c = measureNode(*child(n, 0), level, false, depth + 1);
            return {c.w + 6, std::max(c.asc, f.asc()) + 1, std::max(c.desc, f.desc()) + 1};
        }
        if (n.text == "mixed" && n.children.size() == 3) {
            // Whole part followed by a stacked fraction: 2 1/3.
            const Box w = measureNode(*child(n, 0), level, false, depth + 1);
            const Box a = measureNode(*child(n, 1), level, false, depth + 1), b = measureNode(*child(n, 2), level, false, depth + 1);
            const int axis = f.axis();
            return {w.w + 1 + std::max(a.w, b.w) + 4, std::max(w.asc, axis + 2 + a.desc + a.asc), std::max({w.desc, f.desc(), b.asc + b.desc + 2 - axis})};
        }
        if ((n.text == "period" || n.text == "overline") && n.children.size() == 1) {
            const Box c = measureNode(*child(n, 0), level, false, depth + 1);
            return {c.w, c.asc + 2, c.desc};
        }
        if (n.text == "root" && n.children.size() == 2) {
            // Radical with an editable index (x√ template): index in Small before the sign.
            const Box i = measureNode(*child(n, 0), level + 1, false, depth + 1), c = measureNode(*child(n, 1), level, false, depth + 1);
            const int prefix = std::max(0, i.w - 3);
            return {prefix + 8 + c.w + 2, std::max(c.asc + 3, c.asc / 2 + 2 + i.asc + i.desc), c.desc + 1};
        }
        if (n.text == "log" && n.children.size() == 2) {
            const Box b = measureNode(*child(n, 0), level + 1, false, depth + 1), v = measureNode(*child(n, 1), level, false, depth + 1);
            return {3 * f.adv + b.w + 1 + v.w + 2 * f.adv, std::max(v.asc, f.asc()), std::max({v.desc, f.desc(), 3 + b.desc})};
        }
        Box out{textWidth(n.text, f) + f.adv, f.asc(), f.desc()};
        for (std::size_t i = 0; i < n.children.size(); ++i) {
            const Box c = measureNode(*n.children[i], level, false, depth + 1);
            out.w = satAdd(out.w, c.w + (i ? f.adv : 0)); out.asc = std::max(out.asc, c.asc); out.desc = std::max(out.desc, c.desc);
        }
        out.w += f.adv;
        return out;
    }
    case Kind::Calculus: {
        const Box core = measureNode(*child(n, 0), level, false, depth + 1);
        const int casc = std::max(core.asc, f.asc()), cdesc = std::max(core.desc, f.desc());
        if (n.text == "derivative") {
            const Box a = measureNode(*child(n, 1), level + 1, false, depth + 1);
            const Face& s = smallFace();
            const int head = 2 * s.adv + 2;                     // "d/dx" as a small fraction
            return {head + 2 * f.adv + core.w + 4 + 2 * s.adv + a.w, std::max(casc, s.axis() + 2 * s.height), std::max(cdesc, 4 + a.desc + a.asc)};
        }
        const Box lo = measureNode(*child(n, 1), level + 1, false, depth + 1), hi = measureNode(*child(n, 2), level + 1, false, depth + 1);
        if (n.text == "integral") {
            const int bounds = std::max(lo.w, hi.w);
            return {f.adv + bounds + 2 + core.w + 2 * f.adv, casc + std::max(0, hi.asc + hi.desc - 6), cdesc + std::max(0, lo.asc + lo.desc - 4)};
        }
        const Face& s = smallFace();
        const int lowW = 2 * s.adv + lo.w;                       // "x=" + lower
        const int col = std::max({f.adv + 2, hi.w, lowW});
        return {col + 2 + core.w, casc + hi.asc + hi.desc, cdesc + lo.asc + lo.desc};
    }
    case Kind::Binary: {
        const Box a = measureNode(*child(n, 0), level, false, depth + 1), b = measureNode(*child(n, 1), level, false, depth + 1);
        return {a.w + textWidth(displayToken(n.text), f) + b.w, std::max(a.asc, b.asc), std::max(a.desc, b.desc)};
    }
    case Kind::Unary: {
        const Box a = measureNode(*child(n, 0), level, false, depth + 1);
        return {textWidth(displayToken(n.text), f) + a.w, a.asc, a.desc};
    }
    case Kind::Matrix: {
        const MatrixGrid g = matrixGrid(n, level, depth);
        const int h = std::max(f.height, g.h + 2);
        return {g.w, h / 2 + f.axis(), h - h / 2 - f.axis()};
    }
    default: return faceBox(f, 0);
    }
}

void addText(Ctx& c, int x, int base, const std::string& s, const Face& f, std::uint8_t color = 0) {
    if (s.empty()) return;
    c.out->push_back({NaturalDrawOp::Kind::Text, x, base - f.base, 0, 0, s, f.id, color});
}
void addLine(Ctx& c, int x, int y, int x2, int y2, std::uint8_t color = 0) { c.out->push_back({NaturalDrawOp::Kind::Line, x, y, x2, y2, {}, 0, color}); }
bool colorParens(const Ctx& c) { return c.cursor && c.cursor->colorParens; }
void parens(Ctx& c, int x, int base, int width, int asc, int desc, const Face& f, std::uint8_t color = 0) {
    // Text-height content uses the font's parenthesis glyphs; taller content gets drawn brackets.
    if (asc <= f.asc() && desc <= f.desc()) { addText(c, x, base, "(", f, color); addText(c, x + width - f.adv, base, ")", f, color); return; }
    const int top = base - asc, bottom = base + desc, l = x + 2, r = x + width - 3;
    addLine(c, l + 2, top, l, top + 2, color); addLine(c, l, top + 2, l, bottom - 2, color); addLine(c, l, bottom - 2, l + 2, bottom, color);
    addLine(c, r - 2, top, r, top + 2, color); addLine(c, r, top + 2, r, bottom - 2, color); addLine(c, r, bottom - 2, r - 2, bottom, color);
}
// Colour of a parenthesis token: an opening one takes the colour of the
// current level and opens the next, a closing one closes a level and takes
// its colour, so matching parentheses have the same colour.
std::uint8_t parenToken(Ctx& c, const std::string& t) {
    if (!colorParens(c)) return 0;
    if (t == "(") return parenColorIndex(c.parenDepth++);
    if (t == ")") { if (c.parenDepth > 0) --c.parenDepth; return parenColorIndex(c.parenDepth); }
    return 0;
}
void emitCursor(Ctx& c, int x, int base, const Box& b) {
    const int top = base - b.asc, bottom = base + b.desc - 1;
    c.out->push_back({NaturalDrawOp::Kind::Cursor, x, top, x, bottom, {}, 0});
    c.layout->hasCursor = true; c.layout->cursorX = x; c.layout->cursorTop = top; c.layout->cursorBottom = bottom;
}
void drawNode(Ctx& c, const NaturalNode& n, int x, int base, int level, bool isRoot, unsigned depth);

void drawRow(Ctx& c, const NaturalNode& n, int x, int base, int level, bool isRoot, unsigned depth) {
    const Face& f = faceFor(level);
    const Box rowBox = measureRow(n, level, isRoot, depth);
    const bool cursorHere = c.cursor && c.cursor->row == &n;
    const bool markHere = c.cursor && c.cursor->markRow == &n && c.cursor->markBegin < c.cursor->markEnd;
    if (n.children.empty()) {
        if (!isRoot) c.out->push_back({NaturalDrawOp::Kind::Placeholder, x + 1, base - f.asc() + 2, x + f.adv - 2, base + f.desc() - 1, {}, f.id, 0});
        if (cursorHere) emitCursor(c, x, base, rowBox);
        return;
    }
    // Parentheses left open inside a template field do not change the colours
    // after the template.
    const int parenDepth = c.parenDepth;
    int px = x, markX = x;
    for (std::size_t i = 0; i < n.children.size(); ++i) {
        if (cursorHere && c.cursor->index == i) emitCursor(c, px, base, rowBox);
        if (markHere && c.cursor->markBegin == i) markX = px;
        if (markHere && c.cursor->markEnd == i)
            c.out->push_back({NaturalDrawOp::Kind::Highlight, markX, base - rowBox.asc, px - 1, base + rowBox.desc - 1, {}, 0,
                              static_cast<std::uint8_t>(c.cursor->markIsError ? 1 : 0)});
        const auto& ch = n.children[i];
        if (!ch) continue;
        drawNode(c, *ch, px, base, level, false, depth + 1);
        px = satAdd(px, measureNode(*ch, level, false, depth + 1).w);
    }
    if (markHere && c.cursor->markEnd >= n.children.size())
        c.out->push_back({NaturalDrawOp::Kind::Highlight, markX, base - rowBox.asc, px - 1, base + rowBox.desc - 1, {}, 0,
                          static_cast<std::uint8_t>(c.cursor->markIsError ? 1 : 0)});
    if (cursorHere && c.cursor->index >= n.children.size()) emitCursor(c, px, base, rowBox);
    if (!isRoot) c.parenDepth = parenDepth;
}

void drawNode(Ctx& c, const NaturalNode& n, int x, int base, int level, bool isRoot, unsigned depth) {
    const Face& f = faceFor(level);
    if (depth > MaxDepth) return;
    if (n.kind == Kind::Token) { const std::uint8_t color = parenToken(c, n.text); addText(c, x, base, displayToken(n.text), f, color); return; }
    if (n.kind == Kind::Row) { drawRow(c, n, x, base, level, isRoot, depth); return; }
    if (!shapeOk(n)) return;
    switch (n.kind) {
    case Kind::Fraction: {
        const Box a = measureNode(*child(n, 0), level, false, depth + 1), b = measureNode(*child(n, 1), level, false, depth + 1);
        const int w = std::max(a.w, b.w) + 4, bar = base - f.axis();
        drawNode(c, *child(n, 0), x + (w - a.w) / 2, bar - 2 - a.desc, level, false, depth + 1);
        addLine(c, x + 1, bar, x + w - 2, bar);
        drawNode(c, *child(n, 1), x + (w - b.w) / 2, bar + 2 + b.asc, level, false, depth + 1);
        return;
    }
    case Kind::Radical: {
        const Box cb = measureNode(*child(n, 0), level, false, depth + 1);
        const bool showIndex = !(n.text.empty() || n.text == "2");
        const int prefix = showIndex ? std::max(0, textWidth(n.text, smallFace()) - 3) : 0;
        const int x0 = x + prefix, top = base - cb.asc - 2, mid = base - cb.asc / 2 + 1, bottom = base + cb.desc;
        addLine(c, x0, mid, x0 + 2, mid - 1);
        addLine(c, x0 + 2, mid - 1, x0 + 4, bottom);
        addLine(c, x0 + 4, bottom, x0 + 7, top);
        addLine(c, x0 + 7, top, x0 + 8 + cb.w + 1, top);
        if (showIndex) addText(c, x, mid - 2, n.text, smallFace());
        drawNode(c, *child(n, 0), x0 + 8, base, level, false, depth + 1);
        return;
    }
    case Kind::Power: {
        const Box b = measureNode(*child(n, 0), level, false, depth + 1), e = measureNode(*child(n, 1), level + 1, false, depth + 1);
        drawNode(c, *child(n, 0), x, base, level, false, depth + 1);
        drawNode(c, *child(n, 1), x + b.w + 1, base - (b.asc / 2 + e.desc), level + 1, false, depth + 1);
        return;
    }
    case Kind::Subscript: {
        const Box b = measureNode(*child(n, 0), level, false, depth + 1);
        drawNode(c, *child(n, 0), x, base, level, false, depth + 1);
        drawNode(c, *child(n, 1), x + b.w + 1, base + 3, level + 1, false, depth + 1);
        return;
    }
    case Kind::Group: {
        const Box gb = measureNode(n, level, false, depth);
        const std::uint8_t color = colorParens(c) ? parenColorIndex(c.parenDepth) : 0;
        parens(c, x, base, gb.w, gb.asc, gb.desc, f, color);
        ++c.parenDepth;
        drawNode(c, *child(n, 0), x + f.adv, base, level, false, depth + 1);
        --c.parenDepth;
        return;
    }
    case Kind::Call: {
        if (n.text == "abs" && n.children.size() == 1) {
            const Box gb = measureNode(n, level, false, depth);
            addLine(c, x + 1, base - gb.asc + 1, x + 1, base + gb.desc - 2);
            addLine(c, x + gb.w - 2, base - gb.asc + 1, x + gb.w - 2, base + gb.desc - 2);
            drawNode(c, *child(n, 0), x + 3, base, level, false, depth + 1);
            return;
        }
        if (n.text == "mixed" && n.children.size() == 3) {
            const Box w = measureNode(*child(n, 0), level, false, depth + 1);
            const Box a = measureNode(*child(n, 1), level, false, depth + 1), b = measureNode(*child(n, 2), level, false, depth + 1);
            drawNode(c, *child(n, 0), x, base, level, false, depth + 1);
            const int fx = x + w.w + 1, fw = std::max(a.w, b.w) + 4, bar = base - f.axis();
            drawNode(c, *child(n, 1), fx + (fw - a.w) / 2, bar - 2 - a.desc, level, false, depth + 1);
            addLine(c, fx + 1, bar, fx + fw - 2, bar);
            drawNode(c, *child(n, 2), fx + (fw - b.w) / 2, bar + 2 + b.asc, level, false, depth + 1);
            return;
        }
        if ((n.text == "period" || n.text == "overline") && n.children.size() == 1) {
            const Box cb = measureNode(*child(n, 0), level, false, depth + 1);
            drawNode(c, *child(n, 0), x, base, level, false, depth + 1);
            addLine(c, x, base - cb.asc, x + cb.w - 1, base - cb.asc);
            return;
        }
        if (n.text == "root" && n.children.size() == 2) {
            const Box ib = measureNode(*child(n, 0), level + 1, false, depth + 1), cb = measureNode(*child(n, 1), level, false, depth + 1);
            const int prefix = std::max(0, ib.w - 3);
            const int x0 = x + prefix, top = base - cb.asc - 2, mid = base - cb.asc / 2 + 1, bottom = base + cb.desc;
            addLine(c, x0, mid, x0 + 2, mid - 1);
            addLine(c, x0 + 2, mid - 1, x0 + 4, bottom);
            addLine(c, x0 + 4, bottom, x0 + 7, top);
            addLine(c, x0 + 7, top, x0 + 8 + cb.w + 1, top);
            drawNode(c, *child(n, 0), x, mid - 2 - ib.desc, level + 1, false, depth + 1);
            drawNode(c, *child(n, 1), x0 + 8, base, level, false, depth + 1);
            return;
        }
        if (n.text == "log" && n.children.size() == 2) {
            const Box b = measureNode(*child(n, 0), level + 1, false, depth + 1), v = measureNode(*child(n, 1), level, false, depth + 1);
            addText(c, x, base, "log", f);
            drawNode(c, *child(n, 0), x + 3 * f.adv, base + 3, level + 1, false, depth + 1);
            const int px = x + 3 * f.adv + b.w + 1;
            parens(c, px, base, v.w + 2 * f.adv, std::max(v.asc, f.asc()), std::max(v.desc, f.desc()), f);
            drawNode(c, *child(n, 1), px + f.adv, base, level, false, depth + 1);
            return;
        }
        addText(c, x, base, n.text + "(", f);
        int px = x + textWidth(n.text, f) + f.adv;
        for (std::size_t i = 0; i < n.children.size(); ++i) {
            if (i) { addText(c, px, base, ",", f); px += f.adv; }
            drawNode(c, *n.children[i], px, base, level, false, depth + 1);
            px += measureNode(*n.children[i], level, false, depth + 1).w;
        }
        addText(c, px, base, ")", f);
        return;
    }
    case Kind::Calculus: {
        const Box core = measureNode(*child(n, 0), level, false, depth + 1);
        const Box all = measureNode(n, level, false, depth);
        const Face& s = smallFace();
        if (n.text == "derivative") {
            const Box a = measureNode(*child(n, 1), level + 1, false, depth + 1);
            const int bar = base - f.axis();
            addText(c, x + s.adv / 2 + 1, bar - 1 - s.desc(), "d", s);
            addLine(c, x, bar, x + 2 * s.adv + 1, bar);
            addText(c, x + 1, bar + 1 + s.asc(), "dx", s);
            int px = x + 2 * s.adv + 2;
            parens(c, px, base, core.w + 2 * f.adv, std::max(core.asc, f.asc()), std::max(core.desc, f.desc()), f);
            drawNode(c, *child(n, 0), px + f.adv, base, level, false, depth + 1);
            px += core.w + 2 * f.adv + 1;
            addLine(c, px, base - all.asc + 2, px, base + all.desc - 1);
            addText(c, px + 3, base + 4 + a.asc, "x=", s);
            drawNode(c, *child(n, 1), px + 3 + 2 * s.adv, base + 4 + a.asc, level + 1, false, depth + 1);
            return;
        }
        const Box lo = measureNode(*child(n, 1), level + 1, false, depth + 1), hi = measureNode(*child(n, 2), level + 1, false, depth + 1);
        if (n.text == "integral") {
            addText(c, x, base, "∫", f);
            const int bx = x + f.adv;
            drawNode(c, *child(n, 2), bx, base - all.asc + hi.asc, level + 1, false, depth + 1);
            drawNode(c, *child(n, 1), bx, base + all.desc - lo.desc, level + 1, false, depth + 1);
            const int px = bx + std::max(lo.w, hi.w) + 2;
            drawNode(c, *child(n, 0), px, base, level, false, depth + 1);
            addText(c, px + core.w, base, "dx", f);
            return;
        }
        const int lowW = 2 * s.adv + lo.w;
        const int col = std::max({f.adv + 2, hi.w, lowW});
        addText(c, x + (col - f.adv) / 2, base, n.text == "prod" ? "Π" : "Σ", f);
        drawNode(c, *child(n, 2), x + (col - hi.w) / 2, base - f.asc() - hi.desc, level + 1, false, depth + 1);
        const int lx = x + (col - lowW) / 2, lb = base + f.desc() + lo.asc;
        addText(c, lx, lb, "x=", s);
        drawNode(c, *child(n, 1), lx + 2 * s.adv, lb, level + 1, false, depth + 1);
        drawNode(c, *child(n, 0), x + col + 2, base, level, false, depth + 1);
        return;
    }
    case Kind::Binary: {
        const Box a = measureNode(*child(n, 0), level, false, depth + 1);
        drawNode(c, *child(n, 0), x, base, level, false, depth + 1);
        const std::string op = displayToken(n.text);
        addText(c, x + a.w, base, op, f);
        drawNode(c, *child(n, 1), x + a.w + textWidth(op, f), base, level, false, depth + 1);
        return;
    }
    case Kind::Unary: {
        const std::string op = displayToken(n.text);
        addText(c, x, base, op, f);
        drawNode(c, *child(n, 0), x + textWidth(op, f), base, level, false, depth + 1);
        return;
    }
    case Kind::Matrix: {
        // Square brackets around a grid of cells (each cell centred in its column).
        const Box mb = measureNode(n, level, false, depth);
        const MatrixGrid g = matrixGrid(n, level, depth);
        const int top = base - mb.asc, bottom = base + mb.desc - 1;
        addLine(c, x + 1, top, x + 1, bottom); addLine(c, x + 1, top, x + 3, top); addLine(c, x + 1, bottom, x + 3, bottom);
        addLine(c, x + mb.w - 2, top, x + mb.w - 2, bottom); addLine(c, x + mb.w - 4, top, x + mb.w - 2, top); addLine(c, x + mb.w - 4, bottom, x + mb.w - 2, bottom);
        int y = top + 1 + (mb.asc + mb.desc - 2 - g.h) / 2;
        for (std::size_t r = 0; r < g.rows; ++r) {
            int cx = x + MatrixBracket;
            for (std::size_t col = 0; col < g.cols; ++col) {
                const NaturalNode& cell = *n.children[r * g.cols + col];
                const Box cb = measureNode(cell, level, false, depth + 1);
                drawNode(c, cell, cx + (g.width[col] - cb.w) / 2, y + g.asc[r], level, false, depth + 1);
                cx += g.width[col] + MatrixColumnGap;
            }
            y += g.asc[r] + g.desc[r] + MatrixRowGap;
        }
        return;
    }
    default: return;
    }
}

void clipOps(Layout& r, int left, int top, int right, int bottom) {
    std::vector<NaturalDrawOp> kept; kept.reserve(r.ops.size());
    for (auto op : r.ops) {
        if (op.kind == NaturalDrawOp::Kind::Text) {
            const Face& f = kFaces[op.face ? 1 : 0];
            if (op.y + f.topPad < top || op.y + f.height - 1 > bottom + 1 || op.x >= right) { r.clipped = true; continue; }
            const std::size_t glyphs = font::glyphCount(op.text.c_str());
            std::size_t skip = 0;
            if (op.x < left) { skip = static_cast<std::size_t>((left - op.x + f.adv - 1) / f.adv); r.clipped = true; }
            const int startX = op.x + static_cast<int>(skip) * f.adv;
            const std::size_t fit = static_cast<std::size_t>(std::max(0, (right - startX) / f.adv));
            if (skip >= glyphs) { r.clipped = true; continue; }
            std::size_t take = std::min(glyphs - skip, fit);
            if (take < glyphs - skip) r.clipped = true;
            if (take == 0) continue;
            // Re-slice the UTF-8 string on glyph boundaries.
            std::string sliced; std::size_t g = 0;
            const auto* p = reinterpret_cast<const unsigned char*>(op.text.c_str());
            while (*p) { const unsigned char* start = p; (void)font::nextCodepoint(p); if (g >= skip && g < skip + take) sliced.append(reinterpret_cast<const char*>(start), static_cast<std::size_t>(p - start)); ++g; }
            op.text = std::move(sliced); op.x = startX;
            kept.push_back(std::move(op));
        } else if (op.kind == NaturalDrawOp::Kind::Highlight) {
            // Cut to the visible area (the input may be scrolled horizontally).
            op.x = std::max(op.x, left); op.x2 = std::min(op.x2, right);
            op.y = std::max(op.y, top); op.y2 = std::min(op.y2, bottom);
            if (op.x > op.x2 || op.y > op.y2) { r.clipped = true; continue; }
            kept.push_back(std::move(op));
        } else if (op.kind == NaturalDrawOp::Kind::Cursor || op.kind == NaturalDrawOp::Kind::Placeholder) {
            if (op.x < left || op.x > right || op.y2 < top || op.y > bottom) { r.clipped = true; continue; }
            op.y = std::max(op.y, top); op.y2 = std::min(op.y2, bottom);
            if (op.kind == NaturalDrawOp::Kind::Placeholder) op.x2 = std::min(op.x2, right);
            kept.push_back(std::move(op));
        } else {
            double x0 = op.x, y0 = op.y, x1 = op.x2, y1 = op.y2;
            const double xmin = left, xmax = right, ymin = top, ymax = bottom;
            auto code = [&](double xx, double yy) { return (xx < xmin ? 1 : xx > xmax ? 2 : 0) | (yy < ymin ? 4 : yy > ymax ? 8 : 0); };
            int c0 = code(x0, y0), c1 = code(x1, y1); bool accept = false;
            for (int guard = 0; guard < 8; ++guard) {
                if (!(c0 | c1)) { accept = true; break; }
                if (c0 & c1) break;
                const int cc = c0 ? c0 : c1; double xx = 0, yy = 0;
                if (cc & 8) { if (y1 == y0) break; xx = x0 + (x1 - x0) * (ymax - y0) / (y1 - y0); yy = ymax; }
                else if (cc & 4) { if (y1 == y0) break; xx = x0 + (x1 - x0) * (ymin - y0) / (y1 - y0); yy = ymin; }
                else if (cc & 2) { if (x1 == x0) break; yy = y0 + (y1 - y0) * (xmax - x0) / (x1 - x0); xx = xmax; }
                else { if (x1 == x0) break; yy = y0 + (y1 - y0) * (xmin - x0) / (x1 - x0); xx = xmin; }
                if (cc == c0) { x0 = xx; y0 = yy; c0 = code(x0, y0); } else { x1 = xx; y1 = yy; c1 = code(x1, y1); }
            }
            if (accept) {
                if (x0 != op.x || y0 != op.y || x1 != op.x2 || y1 != op.y2) r.clipped = true;
                op.x = static_cast<int>(std::lround(x0)); op.y = static_cast<int>(std::lround(y0));
                op.x2 = static_cast<int>(std::lround(x1)); op.y2 = static_cast<int>(std::lround(y1));
                kept.push_back(std::move(op));
            } else r.clipped = true;
        }
    }
    r.ops = std::move(kept);
}
} // namespace

std::string displayToken(const std::string& t) {
    if (t.size() == 1) {
        switch (t[0]) {
        case '*': return "×"; case '/': return "÷"; case '-': return "−"; default: return t;
        }
    }
    if (t.size() > 1 && t[0] == '@') {            // graph function reference: "@Y3" -> "Y3", "@XT2" -> "Xt2"
        std::string r = t.substr(1);
        for (std::size_t i = 1; i < r.size(); ++i) r[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(r[i])));
        if (!r.empty()) r[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(r[0])));
        return r;
    }
    std::string low = t; for (char& ch : low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    for (std::size_t i = 0; i < sizeof(kGreekNames) / sizeof(kGreekNames[0]); ++i) if (low == kGreekNames[i]) return kGreekGlyphs[i];
    if (low == "ans") return "Ans";
    if (low == "preans") return "PreAns";
    // Shared variables (VARS) and operator words in fx-CG50 spelling.
    static const struct { const char* name; const char* shown; } kNames[] = {
        // Matrices, vectors and lists (fx-CG50 spelling of the OPTN commands).
        {"mat", "Mat "}, {"vct", "Vct "}, {"list", "List "}, {"list1", "List 1"}, {"list2", "List 2"}, {"list3", "List 3"},
        {"list4", "List 4"}, {"list5", "List 5"}, {"list6", "List 6"}, {"det", "Det "}, {"trn", "Trn "}, {"identity", "Identity "},
        {"dim", "Dim "}, {"augment", "Augment"}, {"fill", "Fill"}, {"ref", "Ref "}, {"rref", "Rref "},
        {"mattolist", "Mat\xE2\x86\x92List"}, {"listtomat", "List\xE2\x86\x92Mat"}, {"dotp", "DotP"}, {"crossp", "CrossP"},
        {"angle", "Angle"}, {"unitv", "UnitV"}, {"norm", "Norm"}, {"seq", "Seq"}, {"mean", "Mean"}, {"median", "Median"},
        {"cuml", "Cuml "}, {"dlist", "\xCE\x94List "}, {"sorta", "SortA"}, {"sortd", "SortD"},
        {"xmin", "Xmin"}, {"xmax", "Xmax"}, {"xscl", "Xscl"}, {"ymin", "Ymin"}, {"ymax", "Ymax"}, {"yscl", "Yscl"},
        {"tmin", "Tmin"}, {"tmax", "Tmax"}, {"tptch", "Tptch"}, {"tblstart", "TblStart"}, {"tblend", "TblEnd"},
        {"tblpitch", "TblPitch"}, {"and", " And "}, {"or", " Or "}, {"xor", " Xor "}, {"not", "Not "}, {"rmdr", " Rmdr "},
        {"modexp", "MOD_Exp"}, {"rndfix", "RndFix"}, {"rannorm", "RanNorm#"}, {"ranbin", "RanBin#"}, {"randint", "RanInt#"}};
    for (const auto& k : kNames) if (low == k.name) return k.shown;
    if (t == "<=") return "\xE2\x89\xA4";
    if (t == ">=") return "\xE2\x89\xA5";
    if (t == "->") return "→";
    return t;
}

Size measure(const NaturalNode& n) noexcept {
    try {
        const Box b = measureNode(n, 0, n.kind == Kind::Row, 0);
        if (n.kind != Kind::Row && n.kind != Kind::Token && !shapeOk(n)) return {0, b.asc + b.desc, b.asc};
        return {b.w, b.asc + b.desc, b.asc};
    } catch (...) { return {}; }
}

Layout layout(const NaturalNode& n, int x, int baseline, int maxWidth, int maxHeight, Align align, const CursorTarget* cursor) {
    Layout r;
    r.size = measure(n);
    r.width = std::max(0, maxWidth); r.height = std::max(0, maxHeight);
    r.top = baseline - r.size.baseline;
    if (maxWidth <= 0 || maxHeight <= 0) { r.clipped = true; return r; }
    int drawX = x;
    if (align == Align::Center) drawX += std::max(0, (maxWidth - r.size.width) / 2);
    else if (align == Align::Right) drawX += std::max(0, maxWidth - r.size.width);
    r.x = drawX;
    Ctx c{cursor, &r.ops, &r};
    drawNode(c, n, drawX, baseline, 0, n.kind == Kind::Row, 0);
    if (r.size.width > maxWidth || r.size.height > maxHeight) r.clipped = true;
    clipOps(r, x, r.top, x + maxWidth, r.top + maxHeight);
    return r;
}

void draw(gfx::Canvas& canvas, const Layout& l, const Palette& p) {
    for (const auto& op : l.ops)
        if (op.kind == NaturalDrawOp::Kind::Highlight)
            canvas.rect(op.x, op.y, op.x2 - op.x + 1, op.y2 - op.y + 1, op.color ? p.error : p.selection, true);
    auto ink = [&p](std::uint8_t color) { return color >= 1 && color <= 5 ? p.paren[color - 1] : p.ink; };
    for (const auto& op : l.ops) {
        switch (op.kind) {
        case NaturalDrawOp::Kind::Text:
            font::draw(canvas, op.face ? font::Face::Small : font::Face::Medium, op.x, op.y, op.text.c_str(), ink(op.color));
            break;
        case NaturalDrawOp::Kind::Line: canvas.line(op.x, op.y, op.x2, op.y2, ink(op.color)); break;
        case NaturalDrawOp::Kind::Placeholder:
            canvas.dottedRect(op.x, op.y, op.x2 - op.x + 1, op.y2 - op.y + 1, p.placeholder);
            break;
        case NaturalDrawOp::Kind::Cursor:
            canvas.vLine(op.x, op.y, op.y2 - op.y + 1, p.cursor);
            canvas.vLine(op.x + 1, op.y, op.y2 - op.y + 1, p.cursor);
            break;
        case NaturalDrawOp::Kind::Highlight: break;
        }
    }
}
void draw(gfx::Canvas& canvas, const Layout& l, std::uint16_t ink, std::uint16_t cursorColor, std::uint16_t placeholderColor) {
    Palette p;
    p.ink = ink; p.cursor = cursorColor; p.placeholder = placeholderColor;
    for (auto& c : p.paren) c = ink;
    p.selection = gfx::rgb888To565(190, 215, 250);
    p.error = gfx::rgb888To565(255, 205, 205);
    draw(canvas, l, p);
}
} // namespace fx::math
