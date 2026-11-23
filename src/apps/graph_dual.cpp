// v1.18.0: Dual Screen di GRAPH e il T+G di TABLE (manuale fx-CG50 5-23,
// 5-24, 5-36, 5-37, 5-55; Setup 1-36). Sulla PicoCalc il principale sta sopra
// e il sotto-schermo o la tabella sotto (320x140 ciascuno, decisione D1).
//  * G+G: due grafici; il principale e' sempre attivo; lo ZOOM parte dalla
//    V-Window del principale e scrive il risultato in quella del sotto-schermo
//    (decisione D2); OPTN > COPY e SWAP; indicatori S/E nella lista.
//  * GtoT: Trace + EXE memorizza la variabile del punto nella tabella del
//    sotto-schermo; OPTN > CHANGE attiva la tabella (R-DEL, DEL-ALL, LISTMEM).
//  * T+G: GPH-CON/GPH-PLT di TABLE con la tabella sotto il grafico; CHANGE o
//    AC tornano alla tabella attiva (in TABLE).
// Stesso codice sulle due schede (sul Pico 2 W compilato con -Os).
#include "apps.hpp"
#include <algorithm>
#include <cmath>

namespace fx::app {
using namespace ui::theme;

void GraphApp::dualOnly(Context& ctx, const char* what) const {
    if (layout(ctx) == Layout::Full) return;
    throw MathError(std::string("Argument ERROR: ") + what + " non disponibile con il Dual Screen (SET UP: Dual Screen Off)");
}

// D2: in G+G every ZOOM starts from the main V-Window and writes the sub-screen one.
ViewWindow& GraphApp::zoomTarget(Context& ctx) {
    return builtLayout_ == Layout::GraphGraph ? ctx.ws.graph.dual.sub : ctx.ws.view;
}

// Columns of the table pane: GtoT uses the type of its stored points, T+G the
// TABLE type; both follow Derivative (Setup, manual 1-35).
bool GraphApp::gtotColumns(Context& ctx) {
    ensureBuilt(ctx);
    const GraphType type = builtLayout_ == Layout::GraphTable ? ctx.ws.graph.dual.gtotType : ctx.ws.table.funcType;
    const bool deriv = ctx.ws.graph.settings.derivative;
    const std::uint64_t key = fingerprint_ ^ (static_cast<std::uint64_t>(type) << 56) ^ (deriv ? 0x5bd1e995ull : 0) ^
                              (static_cast<std::uint64_t>(builtLayout_) << 48);
    if (key == gtotKey_) return gtotError_.empty();
    gtotKey_ = key;
    gtotError_.clear();
    gtotCols_.clear();
    try {
        gtotCols_ = table::columns(engine_, type, deriv);
    } catch (const std::bad_alloc&) {
        gtotKey_ = 0;
        throw;
    } catch (const std::exception& e) {
        gtotError_ = e.what();
    }
    return gtotError_.empty();
}

void GraphApp::drawTablePane(Context& ctx, gfx::Canvas& c) {
    const gfx::Rect r = graphview::paneRect(graphview::Pane::Sub);
    c.resetClip();
    c.rect(r.x, r.y, r.w, r.h, 0xFFFF, true);
    if (!gtotColumns(ctx)) {
        ui::textFit(c, 6, r.y + 8, gtotError_, ui::ScreenW - 12, Error, font::Face::Small);
        return;
    }
    const bool gtot = builtLayout_ == Layout::GraphTable;
    const std::vector<double>& rows = gtot ? ctx.ws.graph.dual.gtot : ctx.ws.table.rows;
    const GraphType type = gtot ? ctx.ws.graph.dual.gtotType : ctx.ws.table.funcType;
    graphview::TableCursor& cur = gtot ? gtotCursor_ : tgCursor_;
    if (!gtot && mode_ == Mode::Trace) {
        // T+G: the table row follows the trace pointer (the pointer moves on the rows).
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i] == cursorV_) { cur.row = static_cast<int>(i); break; }
    }
    graphview::TableData d{&engine_, &gtotCols_, &rows, type, ctx.ws.settings.display};
    graphview::drawTable(c, r.y, r.y + r.h, d, cur, gtot && tableActive_);
}

// GtoT (manual 5-55): EXE during Trace stores the variable of the point.
void GraphApp::storeGtoT(Context& ctx, const graph::Plot& p) {
    GraphDual& d = ctx.ws.graph.dual;
    if (!table::tableType(p.type)) throw MathError("Argument ERROR: Graph to Table memorizza solo grafici Y=, r= e Param");
    if (!d.gtot.empty() && d.gtotType != p.type)
        throw MathError("Argument ERROR: la tabella contiene un altro tipo di grafico (DEL\xE2\x80\xA2" "ALL per vuotarla)");
    if (d.gtot.size() >= MaxGtoTRows) throw MathError("Memory ERROR: massimo 999 righe nella tabella");
    if (d.gtot.empty()) d.gtotType = p.type;
    d.gtot.push_back(cursorV_);
    gtotCursor_.row = static_cast<int>(d.gtot.size()) - 1;
    gtotKey_ = 0;
    ctx.markDirty();
}

// GtoT with the table active (OPTN > CHANGE): cursor, R-DEL, DEL-ALL, LISTMEM.
void GraphApp::tableKeyGtoT(Context& ctx, const KeyEvent& ev) {
    GraphDual& d = ctx.ws.graph.dual;
    const int rows = static_cast<int>(d.gtot.size());
    const int cols = gtotColumns(ctx) ? static_cast<int>(gtotCols_.size()) : 0;
    auto& cur = gtotCursor_;
    switch (ev.key) {
    case Key::Up: cur.row = std::max(0, cur.row - 1); return;
    case Key::Down: cur.row = std::min(std::max(0, rows - 1), cur.row + 1); return;
    case Key::PageUp: cur.row = std::max(0, cur.row - 5); return;
    case Key::PageDown: cur.row = std::min(std::max(0, rows - 1), cur.row + 5); return;
    case Key::Home: cur.row = 0; return;
    case Key::End: cur.row = std::max(0, rows - 1); return;
    case Key::Left: cur.col = std::max(-1, cur.col - 1); return;
    case Key::Right: cur.col = std::min(cols - 1, cur.col + 1); return;
    case Key::F1:                                                   // R-DEL
        if (rows == 0) return;
        d.gtot.erase(d.gtot.begin() + std::clamp(cur.row, 0, rows - 1));
        cur.row = std::min(cur.row, std::max(0, rows - 2));
        gtotKey_ = 0;
        ctx.markDirty();
        return;
    case Key::F2:                                                   // DEL-ALL
        if (rows > 0) confirm(Confirm::GtoTDeleteAll, "Eliminare tutte le righe della tabella?");
        return;
    case Key::Optn: showMenu(Menu::GtoTOptn, "OPTN", {"CHANGE", "LISTMEM"}); return;
    case Key::AC: case Key::Exit: tableActive_ = false; return;
    default: return;
    }
}
} // namespace fx::app
