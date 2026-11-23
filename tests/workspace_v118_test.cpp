// v1.18.0: persistenza del Dual Screen di Graph e Table su entrambe le schede.
//  * sezioni 0x0104 (Graph) e 0x0106 (Table) del record, scritte solo se lo
//    stato non e' quello iniziale; il Pico 2 W resta sul record v11, la Plus v12;
//  * parte Dual delle strisce di eActivity, aggiunta solo quando serve;
//  * chiavi "dual", "subview", "side", "tabledual" dei progetti .pcgraph;
//  * immagini .pcpict versione 2 a due livelli (lettura della versione 1).
#include "graph_file.hpp"
#include "pict.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include "board_config.hpp"
#include <algorithm>
#include <memory>
#include <string>

using namespace fx;

namespace {
const std::uint8_t kV1160[] = {
#include "fixtures/workspace_v1_16_0.inc"
};
bool contains(const std::vector<std::uint8_t>& hay, const std::vector<std::uint8_t>& needle) {
    return std::search(hay.begin(), hay.end(), needle.begin(), needle.end()) != hay.end();
}
GraphDual sample() {
    GraphDual d;
    d.mode = DualScreen::GraphGraph;
    d.sub = ViewWindow{};
    d.sub.xmin = -4; d.sub.xmax = 4;
    d.side[0] = DualSide::Both; d.side[3] = DualSide::Sub;
    d.gtotType = GraphType::Polar;
    d.gtot = {0.5, 1.25, -2};
    return d;
}
} // namespace

int main() {
    // ---- record version: the Pico 2 W keeps v11, the Plus v12
    CHECK(workspaceRecordVersion() == (board::config::kIsPicoPlus2W ? 12 : 11));

    // ---- default state: no section of Graph/Table, the record is the one of the v1.17.0
    {
        auto ws = std::make_unique<Workspace>();
        const auto plain = serializeWorkspace(*ws);
        ws->graph.dual = GraphDual{};
        ws->table.dual = false;
        CHECK(serializeWorkspace(*ws) == plain);
        std::vector<std::uint8_t> encoded;
        encodeGraphDual(sample(), encoded);
        CHECK(!contains(plain, encoded));
        // the v1.16.0 record (v11) is read with the initial Dual Screen
        auto old = std::make_unique<Workspace>();
        CHECK(deserializeWorkspace(kV1160, sizeof kV1160, *old) && old->graph.dual.initial() && !old->table.dual);
    }
    // ---- round trip of the two sections
    {
        auto ws = std::make_unique<Workspace>();
        ws->graph.dual = sample();
        ws->table.dual = true;
        const auto rec = serializeWorkspace(*ws);
        std::vector<std::uint8_t> encoded;
        encodeGraphDual(sample(), encoded);
        CHECK(contains(rec, encoded));
        auto back = std::make_unique<Workspace>();
        CHECK(deserializeWorkspace(rec.data(), rec.size(), *back));
        CHECK(back->graph.dual == sample() && back->table.dual);
        CHECK(back->preservedSections.empty());
        // damaged data of the Graph section
        auto bad = rec;
        const auto at = std::search(bad.begin(), bad.end(), encoded.begin(), encoded.end());
        CHECK(at != bad.end());
        *at = 7;                                                      // mode 7 does not exist
        auto broken = std::make_unique<Workspace>();
        const bool read = deserializeWorkspace(bad.data(), bad.size(), *broken);
#if FXAI_BOARD_PICO_PLUS_2W
        // v12: the CRC of the section isolates it, the rest of the record is read
        CHECK(read && broken->graph.dual.initial() && broken->table.dual);
        CHECK(std::find(broken->damagedSections.begin(), broken->damagedSections.end(), static_cast<std::uint16_t>(0x0104)) != broken->damagedSections.end());
#else
        CHECK(!read);                                                 // v11: the record is refused (the other slot is used)
#endif
    }
    // ---- codec of the Dual Screen: invalid data refused
    {
        std::vector<std::uint8_t> e;
        encodeGraphDual(sample(), e);
        GraphDual d;
        CHECK(decodeGraphDual(e.data(), e.size(), d) && d == sample());
        CHECK(!decodeGraphDual(e.data(), e.size() - 1, d));          // truncated
        auto side = e; side[1 + 72] = 3;                              // indicator 3
        CHECK(!decodeGraphDual(side.data(), side.size(), d));
        auto type = e; type[1 + 72 + 20] = static_cast<std::uint8_t>(GraphType::X);
        CHECK(!decodeGraphDual(type.data(), type.size(), d));
        auto view = e; for (int k = 0; k < 8; ++k) view[1 + 8 + k] = view[1 + k];   // Xmax = Xmin
        CHECK(!decodeGraphDual(view.data(), view.size(), d));
        GraphDual big; big.gtot.assign(MaxGtoTRows + 5, 1.0);
        std::vector<std::uint8_t> eb;
        encodeGraphDual(big, eb);
        CHECK(decodeGraphDual(eb.data(), eb.size(), d) && d.gtot.size() == MaxGtoTRows);
    }
    // ---- strip parts of eActivity: Dual only when needed, all or nothing
    {
        auto ws = std::make_unique<Workspace>();
        const auto plain = encodeParts(*ws, {part::Graph, part::Table});
        CHECK(plain[0] == 2);                                        // no Dual part: old firmware reads the document
        ws->graph.dual = sample(); ws->table.dual = true;
        const auto withDual = encodeParts(*ws, {part::Graph, part::Table});
        CHECK(withDual[0] == 3);
        auto other = std::make_unique<Workspace>();
        CHECK(decodeParts(*other, withDual.data(), withDual.size()) && other->graph.dual == sample() && other->table.dual);
        // a document without the Dual part brings the initial Dual Screen
        CHECK(decodeParts(*other, plain.data(), plain.size()) && other->graph.dual.initial() && !other->table.dual);
        // a duplicated Dual part is refused
        auto dup = withDual;
        const auto dualPart = encodePart(*ws, part::Dual);
        dup[0] = 4;
        dup.push_back(part::Dual);
        for (int k = 0; k < 4; ++k) dup.push_back(static_cast<std::uint8_t>(dualPart.size() >> (8 * k)));
        dup.insert(dup.end(), dualPart.begin(), dualPart.end());
        CHECK(!decodeParts(*other, dup.data(), dup.size()));
        // resetPart of Graph brings the Dual Screen back to Off
        resetPart(*ws, part::Graph);
        CHECK(ws->graph.dual.initial());
    }
    // ---- .pcgraph: new keys only when used, ignored by older readers
    {
        graphfile::Project p;
        const std::string plain = graphfile::encode(p);
        CHECK(plain.find("dual=") == std::string::npos && plain.find("side=") == std::string::npos && plain.find("tabledual") == std::string::npos);
        p.dual = DualScreen::GraphTable;
        p.subView.ymin = -1;
        p.side[2] = DualSide::Sub; p.side[5] = DualSide::Both;
        p.tableDual = true;
        const std::string text = graphfile::encode(p);
        CHECK(text.find("dual=GtoT\n") != std::string::npos && text.find("side=MMSMMEMMMMMMMMMMMMMM\n") != std::string::npos);
        graphfile::Project q;
        CHECK(graphfile::decode(text, q).empty());
        CHECK(q.dual == DualScreen::GraphTable && q.subView.ymin == -1 && q.side[2] == DualSide::Sub && q.side[5] == DualSide::Both && q.tableDual);
        graphfile::Project r;
        CHECK(graphfile::decode(plain, r).empty() && r.dual == DualScreen::Off && !r.tableDual);
    }
    // ---- .pcpict version 2: two layers, composite for every reader
    {
        pict::Image img;
        img.width = 4; img.height = 2;
        std::vector<std::uint16_t> back = {0x0001, 0x0001, 0x0001, 0x0001, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
        std::vector<std::uint16_t> graph = {0xFFFF, 0xF800, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0x07E0, 0xFFFF};
        pict::Image b, g;
        CHECK(pict::encode(back.data(), 4, 4, 2, b) && pict::encode(graph.data(), 4, 4, 2, g));
        img.backLayer = b.rle; img.graphLayer = g.rle;
        CHECK(pict::compose(img));
        std::vector<std::uint16_t> px;
        CHECK(pict::forEachSpan(img, [&](int, int, int n, std::uint16_t c) { for (int k = 0; k < n; ++k) px.push_back(c); }));
        CHECK(px == std::vector<std::uint16_t>({0x0001, 0xF800, 0x0001, 0x0001, 0xFFFF, 0xFFFF, 0x07E0, 0xFFFF}));
        const auto bytes = pict::serialize(img);
        CHECK(bytes[8] == 2 && bytes[14] == 3);
        pict::Image read;
        CHECK(pict::parse(bytes.data(), bytes.size(), read).empty() && read.layered() && read.rle == img.rle && read.backLayer == img.backLayer);
        // the layers must cover the image
        pict::Image wrong = img;
        wrong.graphLayer.resize(wrong.graphLayer.size() - 4);
        CHECK(!pict::compose(wrong));
    }
    return testutil::finish("workspace_v118_test");
}
