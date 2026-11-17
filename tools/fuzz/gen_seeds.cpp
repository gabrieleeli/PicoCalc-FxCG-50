// v1.13.1 — // Corpus iniziale: record del workspace delle fixture (v1.6.0-v1.12.0), un record
// attuale ricco e un progetto .pcgraph. Uso: gen_seeds <cartella con ws/ e pcg/>.
#include "workspace.hpp"
#include "graph_file.hpp"
#include "expression.hpp"
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
int main(int, char** argv) {
    std::string out = argv[1];
    put(out + "/ws/f6", f6, sizeof f6); put(out + "/ws/f8", f8, sizeof f8); put(out + "/ws/f9", f9, sizeof f9);
    put(out + "/ws/f10", f10, sizeof f10); put(out + "/ws/f11", f11, sizeof f11); put(out + "/ws/f11r", f11r, sizeof f11r); put(out + "/ws/f12", f12, sizeof f12);
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
