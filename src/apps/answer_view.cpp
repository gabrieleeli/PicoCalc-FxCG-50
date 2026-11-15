// Schermate MatAns / VctAns / ListAns di Run-Matrix (fx-CG50, v1.11.0): un
// risultato lista, matrice o vettore piu' grande della vista naturale (oltre
// 6x6 o 20 elementi) si consulta qui, scorrendo righe e colonne. I valori sono
// quelli delle memorie delle risposte del workspace (sola lettura).
#include "apps.hpp"
#include <cstdio>

namespace fx::app {
using namespace ui::theme;

const CalcValue* AnswerView::value(Context& ctx) const {
    const CollectionMemory& m = ctx.ws.matrices;
    switch (type_) {
    case ValueType::List: return &m.listAns;
    case ValueType::Vector: return &m.vctAns;
    default: return &m.matAns;
    }
}

void AnswerView::open(Context& ctx, ValueType type) {
    type_ = type == ValueType::Vector ? ValueType::Vector : type == ValueType::List ? ValueType::List : ValueType::Matrix;
    const CalcValue* v = value(ctx);
    if (!v || v->empty()) { ctx.toast(std::string(type_ == ValueType::List ? "List" : type_ == ValueType::Vector ? "Vct" : "Mat") + " Ans: nessun risultato"); return; }
    open_ = true;
    grid_ = CellGrid{};
    // Una lista e' una colonna; matrici e vettori righe per colonne.
    grid_.rows = v->rows;
    grid_.cols = type_ == ValueType::List ? 1 : v->cols;
    grid_.visibleCols = type_ == ValueType::List ? 1 : std::min(4, static_cast<int>(v->cols));
    grid_.rowLabels = true;
    grid_.scrollMarks = true;
    grid_.header = [this](int c) { return type_ == ValueType::List ? std::string("List Ans") : std::to_string(c + 1); };
    Context* cp = &ctx;
    grid_.get = [this, cp](int r, int c, double& out) {
        const CalcValue* m = value(*cp);
        if (!m || m->empty() || r >= m->rows || c >= m->cols) return false;
        out = m->at(static_cast<std::size_t>(r), static_cast<std::size_t>(c)).real();
        return true;
    };
    grid_.text = [this, cp](int r, int c) {
        const CalcValue* m = value(*cp);
        if (!m || r >= m->rows || c >= m->cols) return std::string();
        const std::size_t i = static_cast<std::size_t>(r) * m->cols + static_cast<std::size_t>(c);
        const Settings& s = cp->ws.settings;
        if (m->hasExact()) {
            exact::Rational q;
            if (!(m->exact[i].rational(q) && q.isInteger()) && exact::fitsRelaxed(m->exact[i])) return exact::toText(m->exact[i], s.mixedFractions);
        }
        const Complex z = m->at(i);
        return formatValue(z, z.imag() != 0 && s.complex == ComplexMode::Real ? ComplexMode::Rectangular : s.complex, s.display);
    };
}

void AnswerView::key(Context& ctx, const KeyEvent& ev) {
    switch (ev.key) {
    case Key::Exit: case Key::AC: case Key::Exe: close(); return;
    case Key::Up: case Key::Down: case Key::Left: case Key::Right: case Key::PageUp: case Key::PageDown: case Key::Home: case Key::End:
        (void)grid_.key(ctx, ev);                   // solo navigazione: la memoria si legge soltanto
        return;
    default: return;
    }
}

void AnswerView::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    const char* name = type_ == ValueType::List ? "ListAns" : type_ == ValueType::Vector ? "VctAns" : "MatAns";
    ctx.drawStatus(c, (std::string("RUN-MATRIX  ") + name).c_str());
    const int top = ui::ContentTop + 2, bottom = ui::ContentBottom - 2;
    const CalcValue* v = value(ctx);
    if (!v || v->empty()) { close(); return; }
    c.rect(0, top, ui::ScreenW, 18, SelectSoft, true);
    const std::string title = std::string(type_ == ValueType::List ? "List Ans  " : type_ == ValueType::Vector ? "Vct Ans  " : "Mat Ans  ") + dimensionText(*v);
    ui::text(c, 6, top + 1, title.c_str(), Ink);
    char where[40];
    if (type_ == ValueType::List) std::snprintf(where, sizeof(where), "elemento %d", grid_.row + 1);
    else std::snprintf(where, sizeof(where), "r%d c%d", grid_.row + 1, grid_.col + 1);
    ui::textRight(c, ui::ScreenW - 6, top + 3, where, Muted, font::Face::Small);
    grid_.draw(ctx, c, top + 20, bottom);
    const char* keys[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, "EXIT"};
    ui::softKeys(c, keys);
}
} // namespace fx::app
