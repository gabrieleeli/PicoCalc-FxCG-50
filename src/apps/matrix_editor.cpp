// Editor MAT/VCT di Run-Matrix (fx-CG50: SHIFT 3 'MAT/VCT', manuale 2-42…2-48
// e 2-59…2-60). Elenco Mat A-Z / Vct A-Z con le dimensioni, DIM, modifica
// delle celle (EXE passa alla cella a destra), ROW-OP, ROW, COLUMN, DELETE,
// DEL-ALL, M⇔V. I valori sono quelli del workspace (ws.matrices): gli stessi
// usati dai calcoli, salvati con il resto dei dati.
#include "apps.hpp"
#include "csv_io.hpp"
#include "limits.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace fx::app {
using namespace ui::theme;

namespace {
CalcValue* memoryOf(Workspace& ws, bool vectors, int row) {
    auto& m = ws.matrices;
    if (row == MatrixEditorView::AnsRow) return vectors ? &m.vctAns : &m.matAns;
    if (row < 0 || row >= static_cast<int>(CollectionMemory::Slots)) return nullptr;
    return &(vectors ? m.vct : m.mat)[static_cast<std::size_t>(row)];
}
// ROW-OP (fx-CG50 manuale 2-45, 2-46): nome e domande in ordine.
struct RowOp { const char* label; const char* prompts[3]; std::size_t count; };
const RowOp kRowOps[] = {
    {"SWAP  scambia le righe m e n", {"Riga m", "Riga n", nullptr}, 2},
    {"\xC3\x97Rw  riga m per k", {"k", "Riga m", nullptr}, 2},
    {"\xC3\x97Rw+  k \xC3\x97 riga m + riga n", {"k", "Riga m", "Riga n"}, 3},
    {"Rw+  riga m + riga n", {"Riga m", "Riga n", nullptr}, 2},
};
std::size_t rowIndex(double v, std::size_t rows) {
    if (v != static_cast<double>(static_cast<long long>(v)) || v < 1 || v > static_cast<double>(rows))
        throw DimensionError("Dimension ERROR: riga " + formatReal(v) + " inesistente (righe 1-" + std::to_string(rows) + ")");
    return static_cast<std::size_t>(v) - 1;
}
// Copia di una matrice con una riga o una colonna in meno o in piu' (zeri).
CalcValue resized(const CalcValue& v, bool rows, std::size_t at, bool remove) {
    const std::size_t nr = v.rows + (rows ? (remove ? -1 : 1) : 0), nc = v.cols + (rows ? 0 : (remove ? -1 : 1));
    if (nr == 0 || nc == 0) throw DimensionError(std::string("Dimension ERROR: una matrice ha almeno una ") + (rows ? "riga" : "colonna"));
    CalcValue out = CalcValue::matrix(nr, nc, v.type);
    for (std::size_t r = 0; r < nr; ++r)
        for (std::size_t c = 0; c < nc; ++c) {
            std::size_t sr = r, sc = c;
            if (rows) { if (remove) { if (r >= at) ++sr; } else { if (r == at) continue; if (r > at) --sr; } }
            else { if (remove) { if (c >= at) ++sc; } else { if (c == at) continue; if (c > at) --sc; } }
            if (v.hasExact()) out.setExact(r * nc + c, v.exact[sr * v.cols + sc], v.decimalAt(sr * v.cols + sc));   // forme esatte e decimali restano
            else out.set(r * nc + c, v.at(sr, sc));
        }
    out.normalize();
    return out;
}
// Testo di una cella: forma esatta quando esiste (1/3, 2√3, π/2), altrimenti
// il numero (complesso: a+bi anche nel modo Real) nel formato del display.
std::string cellText(const CalcValue& m, std::size_t i, const Settings& s) {
    // Una cella digitata come decimale resta decimale (0.25, v1.12.0).
    if (m.hasExact() && !m.decimalAt(i)) {
        const exact::Value& e = m.exact[i];
        exact::Rational r;
        if (!(e.rational(r) && r.isInteger()) && exact::fitsRelaxed(e)) return exact::toText(e, s.mixedFractions);
    }
    const Complex z = m.at(i);
    const ComplexMode mode = z.imag() != 0 && s.complex == ComplexMode::Real ? ComplexMode::Rectangular : s.complex;
    return formatValue(z, mode, s.display);
}
} // namespace

// Valore di una cella digitata: un'espressione dello stesso parser (frazioni,
// radici, π, Mat B[1,2]...) che deve dare un numero; nel modo Real un input con
// i e' calcolato come complesso, come sulla fx-CG50. La forma esatta, quando
// esiste, resta nella memoria (budget della scheda, limits::kExactStoreElements);
// un decimale digitato resta mostrato come decimale (v1.12.0).
void MatrixEditorView::writeCell(Context& ctx, int r, int c, const std::string& text) {
    if (selected_ == AnsRow) throw MathError(std::string("Math ERROR: ") + (vectors_ ? "Vct" : "Mat") + " Ans si legge soltanto");
    CalcValue* m = current(ctx);
    if (!m || r >= m->rows || c >= m->cols) return;
    ctx.ws.apply();
    const auto tree = parseExpression(text);
    EvalContext ec = ctx.ws.evalContext();
    if (ec.complexMode == ComplexMode::Real && (text.find('i') != std::string::npos || m->complex())) ec.complexMode = ComplexMode::Rectangular;
    const CalcValue v = evaluateAny(*tree, ec);
    if (!v.isScalar()) throw TypeError(std::string("Argument ERROR: una cella contiene un numero, non una ") + valueTypeName(v.type));
    const Complex z = v.scalar();
    const std::size_t index = static_cast<std::size_t>(r) * m->cols + static_cast<std::size_t>(c);
    exact::Value e;
    bool decimalInput = false;
    if (z.imag() == 0 && evaluateExact(*tree, ec, e, &decimalInput) && std::isfinite(e.toDouble())) m->setExact(index, e, decimalInput);
    else m->set(index, z);
    m->normalize();
    ctx.markDirty();
}
std::string MatrixEditorView::cellEditText(Context& ctx, int r, int c) const {
    const CalcValue* m = current(ctx);
    if (!m || r >= m->rows || c >= m->cols) return {};
    const std::size_t i = static_cast<std::size_t>(r) * m->cols + static_cast<std::size_t>(c);
    if (m->hasExact() && !m->decimalAt(i)) return exact::toExpression(m->exact[i]);
    const Complex z = m->at(i);
    char buf[80];
    if (z.imag() == 0) std::snprintf(buf, sizeof(buf), "%.15g", z.real());
    else std::snprintf(buf, sizeof(buf), "%.15g%+.15gi", z.real(), z.imag());
    return buf;
}

void MatrixEditorView::open(Context&, bool vectors) {
    open_ = true; vectors_ = vectors; screen_ = Screen::List; confirm_ = Confirm::None;
    menu_.close(); menuKind_ = Menu::None; asking_ = false; dialog_.close();
}
std::string MatrixEditorView::name(int row) const {
    std::string s = vectors_ ? "Vct " : "Mat ";
    if (row == AnsRow) return s + "Ans";
    return s + static_cast<char>('A' + row);
}
CalcValue* MatrixEditorView::current(Context& ctx) const { return memoryOf(ctx.ws, vectors_, selected_); }

void MatrixEditorView::store(Context& ctx, CalcValue value) {
    if (selected_ == AnsRow) throw MathError("Math ERROR: " + name(AnsRow) + " si legge soltanto (contiene l'ultimo risultato)");
    if (vectors_) ctx.ws.storeVector(static_cast<std::size_t>(selected_), std::move(value));
    else ctx.ws.storeMatrix(static_cast<std::size_t>(selected_), std::move(value));
    ctx.markDirty();
}

void MatrixEditorView::key(Context& ctx, const KeyEvent& ev) {
    if (dialog_.active()) {
        if (dialog_.key(ev) == 1) csvChosen(ctx);
        return;
    }
    if (screen_ == Screen::List && menu_.open) {
        const int choice = menu_.key(ev);
        if (choice == -2) menuKind_ = Menu::None;
        if (choice >= 0) { menu_.close(); menuKind_ = Menu::None; csvMenu(ctx, choice); }
        return;
    }
    switch (screen_) {
    case Screen::List: listKey(ctx, ev); return;
    case Screen::Dim: dimKey(ctx, ev); return;
    case Screen::Cells: cellsKey(ctx, ev); return;
    }
}

// ---------------------------------------------------------------- elenco delle memorie
void MatrixEditorView::listKey(Context& ctx, const KeyEvent& ev) {
    auto& m = ctx.ws.matrices;
    if (confirm_ != Confirm::None) {
        if (ev.is(Key::Exe) || ev.is(Key::F1)) {
            if (confirm_ == Confirm::Delete) { if (CalcValue* v = current(ctx)) *v = CalcValue{}; }
            else {
                for (CalcValue& v : vectors_ ? m.vct : m.mat) v = CalcValue{};
                (vectors_ ? m.vctAns : m.matAns) = CalcValue{};
            }
            ctx.markDirty();
            ctx.toast("Eliminato");
        }
        confirm_ = Confirm::None;
        return;
    }
    const int rows = AnsRow + 1;
    switch (ev.key) {
    case Key::Up: selected_ = std::max(0, selected_ - 1); return;
    case Key::Down: selected_ = std::min(rows - 1, selected_ + 1); return;
    case Key::PageUp: selected_ = std::max(0, selected_ - 8); return;
    case Key::PageDown: selected_ = std::min(rows - 1, selected_ + 8); return;
    case Key::Home: selected_ = 0; return;
    case Key::End: selected_ = rows - 1; return;
    case Key::Char: {
        // Una lettera porta alla memoria con quel nome (Mat N, Vct N).
        const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(ev.ch)));
        if (!ev.ctrl && c >= 'A' && c <= 'Z') selected_ = c - 'A';
        return;
    }
    case Key::Exe: case Key::Right:
        if (current(ctx)->empty()) {
            if (selected_ == AnsRow) { ctx.toast(name(AnsRow) + ": nessun risultato"); return; }
            openDim(ctx);
        } else openCells(ctx);
        return;
    case Key::F1: if (!current(ctx)->empty()) confirm_ = Confirm::Delete; return;
    case Key::F2: confirm_ = Confirm::DeleteAll; return;
    case Key::F3:
        if (selected_ == AnsRow) { ctx.toast(name(AnsRow) + " si legge soltanto"); return; }
        openDim(ctx);
        return;
    case Key::F4:
        // CSV (fx-CG50 2-48): carica o salva la memoria selezionata.
        menu_.show("CSV", {"LOAD  carica in " + name(selected_), "SAVE\xC2\xB7" "AS  salva " + name(selected_)});
        menuKind_ = Menu::Csv;
        return;
    case Key::F6: vectors_ = !vectors_; return;
    case Key::Exit: case Key::AC: close(); return;
    default: return;
    }
}

// ---------------------------------------------------------------- CSV
void MatrixEditorView::csvMenu(Context& ctx, int choice) {
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) throw MathError("Storage ERROR: archivio file non disponibile in questa build");
    csvSave_ = choice == 1;
    if (csvSave_) {
        if (current(ctx)->empty()) throw MathError("Dimension ERROR: " + name(selected_) + " non definita");
        const std::string suggested = (vectors_ ? "VCT" : "MAT") + std::string(selected_ == AnsRow ? "ANS" : std::string(1, static_cast<char>('A' + selected_)));
        dialog_.openName("CSV SAVE\xC2\xB7" "AS  " + name(selected_), suggested);
        return;
    }
    if (selected_ == AnsRow) throw MathError("Math ERROR: " + name(AnsRow) + " si legge soltanto");
    std::vector<std::string> names;
    const files::Status st = csv::list(*fs, names);
    if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
    dialog_.openList("CSV LOAD  in " + name(selected_), std::move(names));
}
void MatrixEditorView::csvChosen(Context& ctx) {
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) throw MathError("Storage ERROR: archivio file non disponibile in questa build");
    const std::string file = dialog_.name();
    if (csvSave_) {
        const files::Status st = csv::save(*fs, file, csv::encodeMatrix(*current(ctx), csv::formatOf(ctx.ws)));
        if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
        ctx.toast(name(selected_) + " salvata in " + file + ".csv");
        return;
    }
    std::vector<std::uint8_t> bytes;
    const files::Status st = csv::load(*fs, file, bytes);
    if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
    const csv::Table t = csv::decode(bytes.data(), bytes.size(), csv::formatOf(ctx.ws), limits::kMatrixMaxDim, limits::kMatrixMaxDim,
                                     limits::kMatrixElements);
    csv::loadMatrix(ctx.ws, vectors_, static_cast<std::size_t>(selected_), t);
    ctx.markDirty();
    ctx.toast(name(selected_) + " " + dimensionText(*current(ctx)) + " da " + file + ".csv");
}

// ---------------------------------------------------------------- DIM
void MatrixEditorView::openDim(Context& ctx) {
    const CalcValue* v = current(ctx);
    dim_[0].set(v && !v->empty() ? std::to_string(v->rows) : std::string(vectors_ ? "3" : ""));
    dim_[1].set(v && !v->empty() ? std::to_string(v->cols) : std::string(vectors_ ? "1" : ""));
    dimField_ = 0;
    screen_ = Screen::Dim;
}
void MatrixEditorView::dimKey(Context& ctx, const KeyEvent& ev) {
    auto apply = [&]() {
        std::size_t n[2] = {};
        for (int i = 0; i < 2; ++i) {
            const std::string& t = dim_[i].text();
            const bool digits = !t.empty() && t.size() <= 3 && std::all_of(t.begin(), t.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
            n[i] = digits ? static_cast<std::size_t>(std::atoi(t.c_str())) : 0;
            if (n[i] < 1 || n[i] > limits::kMatrixMaxDim) {
                dimField_ = i;
                throw DimensionError("Dimension ERROR: " + std::string(i ? "colonne" : "righe") + " da 1 a " + std::to_string(limits::kMatrixMaxDim));
            }
        }
        if (vectors_ && n[0] != 1 && n[1] != 1)
            throw DimensionError("Dimension ERROR: un vettore ha una riga o una colonna (m\xC3\x97" "1 o 1\xC3\x97n)");
        // Come sulla fx-CG50 le nuove dimensioni cancellano il contenuto.
        store(ctx, CalcValue::matrix(n[0], n[1], vectors_ ? ValueType::Vector : ValueType::Matrix));
        openCells(ctx);
    };
    switch (ev.key) {
    case Key::Up: case Key::Down: dimField_ = 1 - dimField_; return;
    case Key::Exe: if (dimField_ == 0) { dimField_ = 1; return; } apply(); return;
    case Key::F1: apply(); return;
    case Key::Exit: screen_ = Screen::List; return;
    case Key::Char:
        if (ev.ctrl || ev.ch < '0' || ev.ch > '9') return;
        (void)dim_[dimField_].key(ev);
        return;
    default: (void)dim_[dimField_].key(ev); return;
    }
}

// ---------------------------------------------------------------- celle
void MatrixEditorView::openCells(Context& ctx) {
    const CalcValue* v = current(ctx);
    if (!v || v->empty()) { screen_ = Screen::List; return; }
    const int row = grid_.row, col = grid_.col;
    const bool keep = screen_ == Screen::Cells;
    grid_ = CellGrid{};
    grid_.rows = v->rows; grid_.cols = v->cols;
    grid_.visibleCols = std::min(4, static_cast<int>(v->cols));
    grid_.rowMajor = true;                 // EXE: cella a destra, poi riga successiva
    grid_.rowLabels = true;
    grid_.header = [](int c) { return std::to_string(c + 1); };
    Context* cp = &ctx;
    const bool vectors = vectors_;
    const int slot = selected_;
    grid_.get = [cp, vectors, slot](int r, int c, double& out) {
        const CalcValue* m = memoryOf(cp->ws, vectors, slot);
        if (!m || m->empty() || r >= m->rows || c >= m->cols) return false;
        out = m->at(static_cast<std::size_t>(r), static_cast<std::size_t>(c)).real();
        return true;
    };
    grid_.text = [cp, vectors, slot](int r, int c) {
        const CalcValue* m = memoryOf(cp->ws, vectors, slot);
        if (!m || r >= m->rows || c >= m->cols) return std::string();
        return cellText(*m, static_cast<std::size_t>(r) * m->cols + static_cast<std::size_t>(c), cp->ws.settings);
    };
    // Celle con espressioni, forme esatte e complessi (v1.11.0).
    grid_.setText = [this, cp](int r, int c, const std::string& text) { writeCell(*cp, r, c, text); };
    grid_.editText = [this, cp](int r, int c) { return cellEditText(*cp, r, c); };
    grid_.scrollMarks = true;
    auto write = [cp, vectors, slot](int r, int c, Complex z) {
        if (slot == AnsRow) throw MathError(std::string("Math ERROR: ") + (vectors ? "Vct" : "Mat") + " Ans si legge soltanto");
        CalcValue* m = memoryOf(cp->ws, vectors, slot);
        if (!m || r >= m->rows || c >= m->cols) return;
        m->set(static_cast<std::size_t>(r) * m->cols + static_cast<std::size_t>(c), z);
        m->normalize();
        cp->markDirty();
    };
    grid_.set = [write](int r, int c, double v) { write(r, c, Complex(v, 0.0)); };
    grid_.erase = [write](int r, int c) { write(r, c, Complex(0.0, 0.0)); };
    grid_.row = keep ? std::min(row, grid_.rows - 1) : 0;
    grid_.col = keep ? std::min(col, grid_.cols - 1) : 0;
    screen_ = Screen::Cells;
}
void MatrixEditorView::askNext(Context& ctx) {
    const RowOp& op = kRowOps[rowOp_];
    if (args_.size() < op.count) {
        asking_ = true;
        const std::string prompt = op.prompts[args_.size()];
        ask_.set(prompt == "k" ? std::string() : std::to_string(grid_.row + 1));
        return;
    }
    asking_ = false;
    CalcValue v = *current(ctx);
    const std::size_t cols = v.cols;
    auto rowAt = [&](std::size_t i) { return rowIndex(args_[i], v.rows); };
    switch (rowOp_) {
    case 0: {                                           // SWAP m n
        const std::size_t a = rowAt(0), b = rowAt(1);
        for (std::size_t c = 0; c < cols; ++c) { const Complex t = v.at(a, c); v.set(a * cols + c, v.at(b, c)); v.set(b * cols + c, t); }
        break;
    }
    case 1: {                                           // k × riga m
        const std::size_t m = rowAt(1);
        for (std::size_t c = 0; c < cols; ++c) v.set(m * cols + c, args_[0] * v.at(m, c));
        break;
    }
    case 2: {                                           // k × riga m + riga n -> riga n
        const std::size_t m = rowAt(1), n = rowAt(2);
        for (std::size_t c = 0; c < cols; ++c) v.set(n * cols + c, args_[0] * v.at(m, c) + v.at(n, c));
        break;
    }
    default: {                                          // riga m + riga n -> riga n
        const std::size_t m = rowAt(0), n = rowAt(1);
        for (std::size_t c = 0; c < cols; ++c) v.set(n * cols + c, v.at(m, c) + v.at(n, c));
        break;
    }
    }
    v.normalize();
    store(ctx, std::move(v));
    openCells(ctx);
}
void MatrixEditorView::menuChoice(Context& ctx, int choice) {
    const Menu kind = menuKind_;
    menuKind_ = Menu::None;
    if (kind == Menu::RowOp) {
        rowOp_ = choice;
        args_.clear();
        askNext(ctx);
        return;
    }
    // ROW / COLUMN: 0 DELETE, 1 INSERT (prima della cella), 2 ADD (dopo la cella)
    const bool rows = kind == Menu::Row;
    const std::size_t at = static_cast<std::size_t>(rows ? grid_.row : grid_.col);
    const CalcValue& v = *current(ctx);
    store(ctx, resized(v, rows, choice == 2 ? at + 1 : at, choice == 0));
    if (choice == 2) { if (rows) ++grid_.row; else ++grid_.col; }
    openCells(ctx);
}
void MatrixEditorView::cellsKey(Context& ctx, const KeyEvent& ev) {
    if (asking_) {
        if (ev.is(Key::Exe)) { args_.push_back(evalNumber(ctx, ask_.text())); askNext(ctx); return; }
        if (ev.is(Key::Exit)) { asking_ = false; return; }
        (void)ask_.key(ev);
        return;
    }
    if (menu_.open) {
        const int choice = menu_.key(ev);
        if (choice == -2) menuKind_ = Menu::None;
        if (choice >= 0) { menu_.close(); menuChoice(ctx, choice); }
        return;
    }
    if (grid_.editing()) { (void)grid_.key(ctx, ev); return; }
    const bool readOnly = selected_ == AnsRow;
    switch (ev.key) {
    case Key::F1:
        if (vectors_ || readOnly) return;               // l'editor dei vettori non ha ROW-OP
        menu_.show("ROW-OP", {kRowOps[0].label, kRowOps[1].label, kRowOps[2].label, kRowOps[3].label});
        menuKind_ = Menu::RowOp;
        return;
    case Key::F2: case Key::F3:
        if (readOnly) return;
        menu_.show(ev.is(Key::F2) ? "ROW" : "COLUMN", {"DELETE  elimina", "INSERT  inserisci prima", "ADD  aggiungi dopo"});
        menuKind_ = ev.is(Key::F2) ? Menu::Row : Menu::Column;
        return;
    case Key::F4: { KeyEvent edit; edit.key = Key::Exe; (void)grid_.key(ctx, edit); return; }
    case Key::Exit: case Key::AC: screen_ = Screen::List; return;
    default: (void)grid_.key(ctx, ev); return;
    }
}

// ---------------------------------------------------------------- disegno
void MatrixEditorView::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, vectors_ ? "RUN-MATRIX  VCT" : "RUN-MATRIX  MAT");
    const int top = ui::ContentTop + 2, bottom = ui::ContentBottom - 2;
    if (screen_ == Screen::List) {
        c.rect(0, top, ui::ScreenW, 18, SelectSoft, true);
        ui::text(c, 6, top + 1, vectors_ ? "Editor vettori" : "Editor matrici", Ink);
        char used[48];
        std::snprintf(used, sizeof(used), "%u/%u el.", static_cast<unsigned>(ctx.ws.matrices.elements()),
                      static_cast<unsigned>(limits::kMatrixStoreElements));
        ui::textRight(c, ui::ScreenW - 6, top + 3, used, Muted, font::Face::Small);
        const int rowH = 19, listTop = top + 20;
        const int visible = std::max(1, (bottom - listTop) / rowH);
        if (selected_ < first_) first_ = selected_;
        if (selected_ >= first_ + visible) first_ = selected_ - visible + 1;
        for (int i = 0; i < visible && first_ + i <= AnsRow; ++i) {
            const int row = first_ + i;
            const CalcValue* v = memoryOf(ctx.ws, vectors_, row);
            const std::string label = name(row), dims = v ? dimensionText(*v) : std::string("None");
            ui::listRow(c, listTop + i * rowH, rowH - 1, label.c_str(), dims.c_str(), row == selected_);
        }
        ui::scrollBar(c, ui::ScreenW - 3, listTop, visible * rowH, AnsRow + 1, visible, first_);
        const char* keys[6] = {"DELETE", "DEL-ALL", "DIM", "CSV", nullptr, "M\xE2\x87\x94V"};
        ui::softKeys(c, keys);
        menu_.draw(c);
        dialog_.draw(c);
        if (confirm_ == Confirm::Delete) ui::errorBox(c, "ELIMINARE?", (name(selected_) + "  EXE = conferma").c_str());
        if (confirm_ == Confirm::DeleteAll) ui::errorBox(c, "ELIMINARE TUTTO?", vectors_ ? "Vct A-Z e Vct Ans  EXE = conferma" : "Mat A-Z e Mat Ans  EXE = conferma");
        return;
    }
    if (screen_ == Screen::Dim) {
        c.rect(0, top, ui::ScreenW, 18, SelectSoft, true);
        ui::text(c, 6, top + 1, (name(selected_) + "  DIM").c_str(), Ink);
        const char* labels[2] = {"Righe     m:", "Colonne   n:"};
        for (int i = 0; i < 2; ++i) {
            const int y = top + 34 + i * 26;
            if (i == dimField_) c.rect(0, y - 3, ui::ScreenW, 22, SelectSoft, true);
            ui::text(c, 10, y, labels[i], Ink);
            dim_[i].draw(c, 120, y, 80, Ink, i == dimField_);
        }
        std::string notes[4];
        const std::string hint = std::string(vectors_ ? "Vettore: m\xC3\x97" "1 oppure 1\xC3\x97n. " : "") +
            "Massimo " + std::to_string(limits::kMatrixMaxDim) + " per lato e " + std::to_string(limits::kMatrixElements) +
            " elementi. Le nuove dimensioni azzerano il contenuto.";
        const std::size_t n = ui::wrapText(hint, 50, notes, 4);
        for (std::size_t k = 0; k < n; ++k) ui::text(c, 8, top + 100 + static_cast<int>(k) * 14, notes[k].c_str(), Muted, font::Face::Small);
        const char* keys[6] = {"OK", nullptr, nullptr, nullptr, nullptr, nullptr};
        ui::softKeys(c, keys);
        return;
    }
    const CalcValue* v = current(ctx);
    c.rect(0, top, ui::ScreenW, 18, SelectSoft, true);
    ui::text(c, 6, top + 1, (name(selected_) + "  " + (v ? dimensionText(*v) : std::string())).c_str(), Ink);
    // Cella corrente (riga, colonna) e, per Ans, la sola lettura.
    char where[48];
    std::snprintf(where, sizeof(where), "%sr%d c%d", selected_ == AnsRow ? "sola lettura  " : "", grid_.row + 1, grid_.col + 1);
    ui::textRight(c, ui::ScreenW - 6, top + 3, where, Muted, font::Face::Small);
    grid_.draw(ctx, c, top + 20, bottom);
    const bool readOnly = selected_ == AnsRow;
    const char* keys[6] = {vectors_ || readOnly ? nullptr : "ROW-OP", readOnly ? nullptr : "ROW", readOnly ? nullptr : "COLUMN",
                           readOnly ? nullptr : "EDIT", nullptr, nullptr};
    ui::softKeys(c, keys);
    menu_.draw(c);
    if (asking_) {
        const gfx::Rect box{30, 120, ui::ScreenW - 60, 64};
        ui::panel(c, box, Bg, Ink);
        ui::text(c, box.x + 8, box.y + 6, kRowOps[rowOp_].label, Accent, font::Face::Small);
        const std::string prompt = std::string(kRowOps[rowOp_].prompts[args_.size()]) + ":";
        ui::text(c, box.x + 8, box.y + 26, prompt.c_str(), Ink);
        ask_.draw(c, box.x + 80, box.y + 26, box.w - 90, Ink);
    }
}
} // namespace fx::app
