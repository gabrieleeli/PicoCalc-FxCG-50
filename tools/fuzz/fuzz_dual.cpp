// v1.18.0 — Dual Screen di Graph e Table (G+G, GtoT, T+G) sotto sequenze di
// tasti casuali. I primi byte scelgono la disposizione, le funzioni, le due
// V-Window e una tabella del GtoT; il resto sono tasti di GRAPH/TABLE.
// Invarianti controllati a ogni input (abort se violati):
//  * le curve di ogni riquadro restano nel proprio riquadro (piu' il margine
//    del clipping), quelle del principale non entrano nel sotto-schermo;
//  * la tabella del GtoT non supera 999 righe e contiene solo valori finiti;
//  * il workspace con il Dual Screen si salva e si rilegge identico (sezioni
//    0x0104/0x0106) e le parti delle strisce di eActivity fanno andata e ritorno.
#include "app_rig.hpp"
#include "graph_file.hpp"
#include "storage.hpp"
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
using namespace fx;

namespace {
const std::uint8_t kKeys[] = {
    pckey::F1, pckey::F2, pckey::F3, pckey::F4, pckey::F5, pckey::F6, pckey::F7, pckey::F8, pckey::F10,
    pckey::Up, pckey::Down, pckey::Left, pckey::Right, pckey::Enter, pckey::Esc, pckey::Break, pckey::Backspace,
    pckey::Home, pckey::End, pckey::PageUp, pckey::PageDown, pckey::Enter, pckey::Right, pckey::Right,
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '.', 'x', 'S', 'E'};
const char* const kFunctions[] = {"x^2-3", "-x+1", "sin(x)", "1/x", "Ax,[A=1,2]", "x^3-4x", "2", "tan(x)"};
double pick(std::uint8_t b, double lo, double hi) { return lo + (hi - lo) * (b / 255.0); }
void check(bool ok) { if (!ok) std::abort(); }
void curvesIn(const std::vector<graph::Curve>& curves, int top, int bottom) {
    for (const auto& c : curves)
        for (std::size_t i = 0; i + 1 < c.xy.size(); i += 2)
            if (c.xy[i] != graph::PenUp) check(c.xy[i + 1] >= top - 6 && c.xy[i + 1] <= bottom + 6);
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 12 || n > 300) return 0;
    { storage::RecordStore rs(board::storageFlash()); (void)rs.eraseAll(); }
    testrig::Rig r;
    Workspace& ws = r.ws();
    // configuration
    const int layout = d[0] % 3;                                    // 0 G+G, 1 GtoT, 2 T+G
    for (std::size_t i = 0; i < 3; ++i) {
        ws.functions[i] = emptyGraphFunction(i);
        ws.functions[i].expr = kFunctions[d[1 + i] % 8];
        ws.functions[i].type = GraphType::Y;
        ws.graph.dual.side[i] = static_cast<DualSide>(d[4 + i] % 3);
    }
    ws.view.xmin = pick(d[7], -20, -0.5); ws.view.xmax = pick(d[8], 0.5, 20);
    ws.view.ymin = pick(d[9], -20, -0.5); ws.view.ymax = pick(d[10], 0.5, 20);
    ws.graph.dual.sub = ws.view;
    ws.graph.dual.sub.xmax += pick(d[11], 0, 30);
    ws.graph.settings.derivative = (d[0] & 0x10) != 0;
    ws.graph.settings.simulGraph = (d[0] & 0x20) != 0;
    if (layout == 0) ws.graph.dual.mode = DualScreen::GraphGraph;
    else if (layout == 1) { ws.graph.dual.mode = DualScreen::GraphTable; ws.graph.dual.gtot = {pick(d[1], -3, 3), pick(d[2], -3, 3)}; }
    else { ws.table.dual = true; ws.table.start = -3; ws.table.end = 3; ws.table.step = 1; }
    r.type(layout == 2 ? "7" : "5");
    if (layout == 2) r.key(pckey::F6);                              // TABLE
    for (size_t i = 12; i < n; ++i) {
        const std::uint8_t k = kKeys[d[i] % sizeof(kKeys)];
        if (k >= 0x80 || k < 0x20) r.key(k); else r.raw(testrig::press(k));
        if (r.error()) r.closeError();
        if (auto* g = r.active<app::GraphApp>()) {
            const auto lay = g->builtLayout();
            if (lay == app::GraphApp::Layout::GraphGraph) {
                curvesIn(g->curves(), graphgeo::Top, graphgeo::Top + graphgeo::SplitRows - 1);
                curvesIn(g->subCurves(), graphgeo::SubTop, graphgeo::SubTop + graphgeo::SplitRows - 1);
            } else if (lay != app::GraphApp::Layout::Full) {
                curvesIn(g->curves(), graphgeo::Top, graphgeo::Top + graphgeo::SplitRows - 1);
            }
        }
        check(ws.graph.dual.gtot.size() <= MaxGtoTRows);
        for (double v : ws.graph.dual.gtot) check(std::isfinite(v));
    }
    r.settle(300);
    // persistence: record and strip parts
    const auto rec = serializeWorkspace(ws);
    auto back = std::make_unique<Workspace>();
    check(deserializeWorkspace(rec.data(), rec.size(), *back));
    check(back->graph.dual == ws.graph.dual && back->table.dual == ws.table.dual);
    const auto parts = encodeParts(ws, {part::Graph, part::Table});
    auto other = std::make_unique<Workspace>();
    check(decodeParts(*other, parts.data(), parts.size()));
    check(other->graph.dual == ws.graph.dual && other->table.dual == ws.table.dual);
    // project keys
    graphfile::Project p, q;
    p.dual = ws.graph.dual.mode; p.subView = ws.graph.dual.sub; p.side = ws.graph.dual.side; p.tableDual = ws.table.dual;
    check(graphfile::decode(graphfile::encode(p), q).empty());
    check(q.dual == p.dual && q.side == p.side && q.tableDual == p.tableDual);
    return 0;
}
