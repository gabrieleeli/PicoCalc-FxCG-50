// v1.13.1 (v1.14.0: record v9 con dati 3D, record v10 e semi di fuzz_graph3d) — // Corpus iniziale: record del workspace delle fixture (v1.6.0-v1.12.0), un record
// attuale ricco e un progetto .pcgraph. Uso: gen_seeds <cartella con ws/ e pcg/>.
#include "workspace.hpp"
#include "graph_file.hpp"
#include "graph3d_model.hpp"
#include "expression.hpp"
#include "eactivity.hpp"
#include <cstdio>
#include <string>
#include <vector>
using namespace fx;
static void put(const std::string& path, const void* d, size_t n) { FILE* f = std::fopen(path.c_str(), "wb"); std::fwrite(d, 1, n, f); std::fclose(f); }
static const unsigned char f6[] = {
#include "fixtures/workspace_v1_6_0.inc"
};
static const unsigned char f8[] = {
#include "fixtures/workspace_v1_8_0.inc"
};
static const unsigned char f9[] = {
#include "fixtures/workspace_v1_9_0.inc"
};
static const unsigned char f10[] = {
#include "fixtures/workspace_v1_10_0.inc"
};
static const unsigned char f11[] = {
#include "fixtures/workspace_v1_11_0.inc"
};
static const unsigned char f11r[] = {
#include "fixtures/workspace_v1_11_0_run_matrix.inc"
};
static const unsigned char f12[] = {
#include "fixtures/workspace_v1_12_0.inc"
};
static const unsigned char f1131[] = {
#include "fixtures/workspace_v1_13_1_graph3d.inc"
};
// v1.15.0: record v10 scritto dalla v1.14.0
static const unsigned char f1140[] = {
#include "fixtures/workspace_v1_14_0.inc"
};
int main(int, char** argv) {
    std::string out = argv[1];
    put(out + "/ws/f6", f6, sizeof f6); put(out + "/ws/f8", f8, sizeof f8); put(out + "/ws/f9", f9, sizeof f9);
    put(out + "/ws/f10", f10, sizeof f10); put(out + "/ws/f11", f11, sizeof f11); put(out + "/ws/f11r", f11r, sizeof f11r); put(out + "/ws/f12", f12, sizeof f12);
    put(out + "/ws/f1131", f1131, sizeof f1131);
    put(out + "/ws/f1140", f1140, sizeof f1140);
    // v1.15.0: record v11 con sezioni TLV conservate (sconosciute e di versioni future)
    {
        Workspace wt;
        RecordSection a; a.id = 0x0010; a.version = 1; a.data = {1, 2, 3};
        RecordSection b2; b2.id = 0x0102; b2.version = 7; b2.data = {'e', 'a', 'c', 't'};
        RecordSection c2; c2.id = 0x0300; c2.version = 1;
        wt.preservedSections = {a, b2, c2};
        const auto rt = serializeWorkspace(wt); put(out + "/ws/sections", rt.data(), rt.size());
    }
    // v1.14.0: record v10 with every kind of 3D graph, 3D G-Mem and 3D V-Win; seeds of fuzz_graph3d
    {
        Workspace w3;
        g3d::GraphSet& gs = w3.graph3d.set;
        gs.fns[0].kind = g3d::Kind::Line; gs.fns[0].form = 3; gs.fns[0].coef = {0, 0, 0, 1, 1, 1};
        gs.fns[1].kind = g3d::Kind::Plane; gs.fns[1].form = 1; gs.fns[1].coef = {1, 2, 0, 1, 1, 0, 0, 1, 1};
        gs.fns[2].kind = g3d::Kind::Sphere; gs.fns[2].coef = {0, 0, 0, 2};
        gs.window.theta = -45; gs.setup.label = false;
        w3.graph3d.gmem[0] = g3d::encode(gs);
        g3d::GraphSet g2 = gs;
        g2.fns[0].kind = g3d::Kind::Z; g2.fns[0].form = 0; g2.fns[0].expr[0] = "sin(x)cos(y)";
        g2.fns[1].kind = g3d::Kind::Param; g2.fns[1].form = 0; g2.fns[1].expr = {"cos(s)", "sin(s)", "t"};
        g2.fns[2].kind = g3d::Kind::Cone; g2.fns[2].form = 0; g2.fns[2].coef = {1, -2, 2, 0, 0, 0, 0, 0, 0};
        w3.graph3d.gmem[5] = g3d::encode(g2);
        w3.graph3d.vmem[1].xgrid = 10; w3.graph3d.vmemUsed = 2; w3.graph3d.zoom = 1.5F;
        const auto r3 = serializeWorkspace(w3); put(out + "/ws/graph3d", r3.data(), r3.size());
        for (const auto* set : {&gs, &g2}) {
            const std::string e = std::string(1, '\0') + g3d::encode(*set);
            put(out + "/g3d/set" + std::to_string(set == &gs ? 0 : 1), e.data(), e.size());
        }
        g2.fns[0].kind = g3d::Kind::Cylinder; g2.fns[0].coef = {1, -1, 1, 0, 0, 0, 0, 0, 0}; g2.fns[0].expr[0].clear();
        g2.fns[1].kind = g3d::Kind::Plane; g2.fns[1].form = 0; g2.fns[1].coef = {0, 0, 1, 0, 0, 0, 0, 0, 0}; g2.fns[1].expr = {};
        g2.fns[2].kind = g3d::Kind::Line; g2.fns[2].form = 1; g2.fns[2].coef = {-1, -2, 1, 1, 1, 0, 0, 0, 0};
        const std::string e2 = std::string(1, '\0') + g3d::encode(g2);
        put(out + "/g3d/set2", e2.data(), e2.size());
        const std::string win = std::string(1, '\2') + g3d::encodeWindow(gs.window);
        put(out + "/g3d/win", win.data(), win.size());
    }
    // v1.16.0: documenti eActivity (decodifica: byte 0; controller: byte 1 + lunghezza) e record con la sezione 0x0102
    {
        eact::Document d;
        d.lines.push_back(eact::Line::textLine("Testo " + eact::island("(1)/(2)") + " fine"));
        d.lines.back().memo = {true, 2, "nota"};
        d.lines.push_back(eact::Line::calcLine("1+2"));
        eact::Line stop; stop.kind = eact::LineKind::Stop; d.lines.push_back(stop);
        eact::Line pic; pic.kind = eact::LineKind::Picture; pic.text = "PICT01.pcpict"; d.lines.push_back(pic);
        eact::Line st; st.kind = eact::LineKind::Strip; st.strip = 1; st.text = "Graph";
        Workspace g; g.functions[0].expr = "x^2"; st.data = encodeParts(g, {part::Graph}); d.lines.push_back(st);
        eact::Line notes; notes.kind = eact::LineKind::Strip; notes.strip = 10; notes.data = {'a', '\n', 'b'}; d.lines.push_back(notes);
        d.memories = encodeParts(g, {part::Matrices, part::Lists});
        const auto bytes = eact::encode(d);
        std::string a(1, '\0'); a.append(bytes.begin(), bytes.end());
        put(out + "/ea/doc", a.data(), a.size());
        std::string c(1, '\1'); c += static_cast<char>(bytes.size() & 0xFF); c += static_cast<char>(bytes.size() >> 8);
        c.append(bytes.begin(), bytes.end()); c += "\x0e\x14\x0e\x18\x01\x0e\x05";
        put(out + "/ea/keys", c.data(), c.size());
        Workspace we; we.eactivity.folder = "SCUOLA"; we.eactivity.lastFile = "FISICA.pceact";
        we.eactivity.savedStrip = encodeParts(g, {part::Graph}); we.eactivity.savedMemories = d.memories;
        const auto r = serializeWorkspace(we); put(out + "/ws/eactivity", r.data(), r.size());
    }
    Workspace ws;
    auto b = serializeWorkspace(ws); put(out + "/ws/default", b.data(), b.size());
    // a rich workspace via current deserialization of v11 then edits
    Workspace w2; deserializeWorkspace(f11, sizeof f11, w2);
    w2.lists[0] = {1, 2, 3}; w2.lists[10] = {4.5, -1};
    w2.listNames[3] = "QTY";
    HistoryEntry h; h.expression = "1/3+2"; h.value = 7.0/3; h.fracNum = 14; h.fracDen = 6; h.base = 0; w2.addHistory(h);
    HistoryEntry h2; h2.expression = "[[1,2][3,4]]"; auto c = std::make_shared<CollectionResult>(); c->value = CalcValue::matrix(2, 2, ValueType::Matrix); c->value.re = {1,2,3,4}; h2.collection = c; w2.addHistory(h2);
    SketchItem s; s.kind = SketchItem::Kind::Circle; s.x0 = 0; s.y0 = 0; s.x1 = 2; w2.graph.sketch.push_back(s);
    CommandGraph cg; cg.type = GraphType::Y; cg.expr = "x^2"; w2.graph.commandGraphs.push_back(cg);
    w2.chat.push_back({true, "hello"});
    b = serializeWorkspace(w2); put(out + "/ws/rich", b.data(), b.size());
    // pcgraph
    graphfile::Project p; p.functions[0].expr = "sin(x)"; p.functions[1].expr = "x^2"; p.functions[1].type = GraphType::Polar;
    p.functions[2].type = GraphType::Param; p.functions[2].expr = "cos(t)"; p.functions[2].expr2 = "sin(t)";
    p.sketch.push_back(s); SketchItem pen; pen.kind = SketchItem::Kind::Pen; pen.path = {0,0,1,1,2,0}; p.sketch.push_back(pen);
    p.vmemUsed = 3; p.tableFromList = true; p.tableList = 4;
    std::string t = graphfile::encode(p);
    auto pos = t.rfind("crc32="); t = t.substr(0, pos);
    put(out + "/pcg/p1", t.data(), t.size());
    return 0;
}
