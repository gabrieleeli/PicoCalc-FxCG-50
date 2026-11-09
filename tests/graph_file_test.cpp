// Persistence of GRAPH projects: file store (RAM card with fault injection),
// atomic replacement and recovery, the .pcgraph text format, graph memories
// and .pcpict images with BMP export.
#include "file_store.hpp"
#include "graph_file.hpp"
#include "pict.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include <cstring>
#include <string>
#include <vector>

using namespace fx;
using namespace fx::files;

namespace {
std::vector<std::uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }
std::string text(const std::vector<std::uint8_t>& v) { return {v.begin(), v.end()}; }
std::string readText(FileSystem& fs, const std::string& path) {
    std::vector<std::uint8_t> d;
    return readAll(fs, path, d, 1 << 20) == Status::Ok ? text(d) : std::string("<missing>");
}
graphfile::Project sampleProject() {
    graphfile::Project p;
    p.functions[0] = {"x^2-2", GraphType::Y, true, 0, LineStyle::Thick, {}};
    p.functions[1] = {"3sin(2theta)", GraphType::Polar, false, 3, LineStyle::Dot, {}};
    p.functions[2] = {"3sin(t),[0,pi]", GraphType::Param, true, 6, LineStyle::Thin, "3cos(t)"};
    p.functions[4] = {"Ax^2-3,[A=3,1,-1]", GraphType::Y, true, 1, LineStyle::Broken, {}};
    p.functions[7] = {"y^2|%weird", GraphType::XLessEq, true, 5, LineStyle::Normal, {}};   // needs escaping
    p.functions[19] = {"@Y1(@Y5)", GraphType::YGreater, true, 4, LineStyle::Normal, {}};
    p.view = {-5, 5, 1, -4, 4, 0.5, 0, 6.283185307179586, 0.1308996938995747};
    p.angle = AngleMode::Degrees;
    p.settings.grid = GridMode::Line; p.settings.axes = AxesMode::Scale; p.settings.label = true;
    p.settings.derivative = true; p.settings.plotDraw = true; p.settings.ineqUnion = false; p.settings.simulGraph = true;
    p.settings.coord = false; p.settings.graphFunc = false; p.settings.funcType = GraphType::Param;
    p.settings.sketchLine = LineStyle::Thin; p.settings.plotColor = 1; p.settings.xFactor = 3; p.settings.yFactor = 1.5;
    p.settings.background = BackgroundFile; p.settings.backgroundFile = "PICOCALC/PICT/BG.pcpict"; p.settings.fade = 35;
    p.vmem[1] = {-1, 1, 0.1, -2, 2, 0.5, 0, 1, 0.01}; p.vmemUsed = 2;
    p.tableStart = -2; p.tableEnd = 5; p.tableStep = 0.5;
    p.overlay = "PICOCALC/PICT/PICT03.pcpict";
    SketchItem line; line.kind = SketchItem::Kind::Segment; line.x0 = -1; line.y0 = 2; line.x1 = 3.5; line.y1 = -0.25; line.color = 1; line.style = LineStyle::Broken;
    SketchItem tan; tan.kind = SketchItem::Kind::Tangent; tan.x0 = 2; tan.y0 = 0; tan.x1 = 1; tan.y1 = 8;
    SketchItem txt; txt.kind = SketchItem::Kind::Text; txt.x0 = 1; txt.y0 = 1; txt.text = "Max|50%";
    SketchItem pen; pen.kind = SketchItem::Kind::Pen; pen.path = {0.5f, 0.25f, 1.0f, -1.5f, 2.0f, 0.125f};
    SketchItem circle; circle.kind = SketchItem::Kind::Circle; circle.x1 = 2;
    SketchItem inv; inv.kind = SketchItem::Kind::Inverse; inv.text = "x^3";
    p.sketch = {line, tan, txt, pen, circle, inv};
    return p;
}
bool sameFunction(const GraphFunction& a, const GraphFunction& b) {
    return a.expr == b.expr && a.expr2 == b.expr2 && a.type == b.type && a.selected == b.selected && a.color == b.color && a.style == b.style;
}
} // namespace

int main() {
    // ---------------------------------------------------------------- RAM card
    {
        RamFileSystem fs(4096);
        CHECK(fs.status() == Status::Ok);
        CHECK(fs.makeDirs("PICOCALC/GRAPH") == Status::Ok && fs.makeDirs("/picocalc/graph/") == Status::Ok);
        CHECK(writeAtomic(fs, "PICOCALC/GRAPH/A.pcgraph", bytes("hello").data(), 5) == Status::Ok);
        CHECK_EQ_STR(readText(fs, "picocalc/graph/a.PCGRAPH"), "hello");      // case-insensitive like FAT
        std::vector<Entry> list;
        CHECK(fs.list("PICOCALC/GRAPH", list) == Status::Ok && list.size() == 1 && list[0].name == "A.pcgraph" && list[0].size == 5);
        CHECK(fs.list("PICOCALC", list) == Status::Ok && list.size() == 1 && list[0].directory);
        CHECK(fs.list("NOPE", list) == Status::NotFound);
        CHECK(fs.rename("PICOCALC/GRAPH/A.pcgraph", "PICOCALC/GRAPH/B.pcgraph") == Status::Ok);
        CHECK(writeAtomic(fs, "PICOCALC/GRAPH/A.pcgraph", bytes("x").data(), 1) == Status::Ok);
        CHECK(fs.rename("PICOCALC/GRAPH/A.pcgraph", "PICOCALC/GRAPH/b.pcgraph") == Status::Exists);
        CHECK(fs.remove("PICOCALC/GRAPH/A.pcgraph") == Status::Ok && fs.remove("PICOCALC/GRAPH/A.pcgraph") == Status::NotFound);
        CHECK(fs.remove("PICOCALC/GRAPH") == Status::IoError);                 // not empty
        std::uint64_t free = 0;
        CHECK(fs.freeSpace(free) == Status::Ok && free == 4096 - 5);
        std::vector<std::uint8_t> big(5000, 7);
        CHECK(writeAtomic(fs, "PICOCALC/GRAPH/BIG.pcgraph", big.data(), big.size()) == Status::Full);
        CHECK(!fs.exists("PICOCALC/GRAPH/BIG.pcgraph") && !fs.exists("PICOCALC/GRAPH/BIG.pcgraph.tmp"));
        CHECK(readAll(fs, "PICOCALC/GRAPH/B.pcgraph", big, 3) == Status::TooLarge);
        fs.setPresent(false);
        CHECK(fs.status() == Status::NoCard && fs.list("", list) == Status::NoCard);
        CHECK(writeAtomic(fs, "PICOCALC/GRAPH/C.pcgraph", bytes("c").data(), 1) == Status::NoCard);
        fs.setPresent(true);
        CHECK(validName("SINE") && validName("a_b-9") && !validName("") && !validName("TOOLONGNAME") && !validName("a.b") && !validName("a b"));
        CHECK(validName("PROJECT_2026", 16));
        CHECK(hasExtension("X.PCGRAPH", ".pcgraph") && !hasExtension(".pcgraph", ".pcgraph") && stem("X.pcgraph") == "X");
        CHECK(std::strlen(statusMessage(Status::NoCard)) > 5);
    }
    // ---------------------------------------------------------------- atomic replacement under faults
    {
        const std::string path = "G/P.pcgraph", oldData = "old version of the project", newData = "NEW VERSION, LONGER THAN THE OLD ONE";
        // Failure of every single operation of the save.
        for (int n = 1; n <= 12; ++n) {
            RamFileSystem fs;
            CHECK(fs.makeDirs("G") == Status::Ok);
            CHECK(writeAtomic(fs, path, bytes(oldData).data(), oldData.size()) == Status::Ok);
            fs.failOperation(n);
            const Status st = writeAtomic(fs, path, bytes(newData).data(), newData.size());
            fs.powerOn();
            CHECK(recover(fs, "G") == Status::Ok);
            const std::string got = readText(fs, path);
            ::testutil::report(got == (st == Status::Ok ? newData : oldData) || (st != Status::Ok && got == newData),
                               "old or new file after a failed operation", __FILE__, __LINE__, std::to_string(n) + ": " + got);
            std::vector<Entry> list;
            CHECK(fs.list("G", list) == Status::Ok && list.size() == 1);        // no .tmp/.bak left
        }
        // Power loss after every possible number of written bytes.
        for (std::size_t b = 0; b <= newData.size() + 2; ++b) {
            RamFileSystem fs;
            CHECK(fs.makeDirs("G") == Status::Ok);
            CHECK(writeAtomic(fs, path, bytes(oldData).data(), oldData.size()) == Status::Ok);
            fs.failAfterBytes(b);
            const Status st = writeAtomic(fs, path, bytes(newData).data(), newData.size());
            fs.powerOn();
            CHECK(recover(fs, "G") == Status::Ok);
            const std::string got = readText(fs, path);
            ::testutil::report(got == oldData || (st == Status::Ok && got == newData), "power loss keeps a complete file", __FILE__, __LINE__, std::to_string(b));
        }
        // Interruption between the renames: recover() completes the save.
        {
            RamFileSystem fs;
            CHECK(fs.makeDirs("G") == Status::Ok);
            CHECK(writeAtomic(fs, "G/P.pcgraph.tmp", bytes(newData).data(), newData.size()) == Status::Ok);
            CHECK(writeAtomic(fs, "G/P.pcgraph.bak", bytes(oldData).data(), oldData.size()) == Status::Ok);
            CHECK(recover(fs, "G") == Status::Ok);
            CHECK_EQ_STR(readText(fs, path), newData);
            CHECK(!fs.exists("G/P.pcgraph.tmp") && !fs.exists("G/P.pcgraph.bak"));
            CHECK(writeAtomic(fs, "G/Q.pcgraph.bak", bytes(oldData).data(), oldData.size()) == Status::Ok);
            CHECK(recover(fs, "G") == Status::Ok);
            CHECK_EQ_STR(readText(fs, "G/Q.pcgraph"), oldData);                 // backup restored
        }
    }
    // ---------------------------------------------------------------- .pcgraph
    {
        const graphfile::Project p = sampleProject();
        const std::string enc = graphfile::encode(p);
        CHECK(enc.compare(0, 10, "PCGRAPH 1\n") == 0);
        CHECK(enc.find("crc32=") != std::string::npos);
        graphfile::Project q;
        CHECK_EQ_STR(graphfile::decode(enc, q), "");
        for (std::size_t i = 0; i < MaxGraphFunctions; ++i) CHECK(sameFunction(p.functions[i], q.functions[i]));
        CHECK(q.view == p.view && q.angle == AngleMode::Degrees);
        CHECK(q.settings.grid == GridMode::Line && q.settings.axes == AxesMode::Scale && q.settings.label && q.settings.derivative);
        CHECK(q.settings.plotDraw && !q.settings.ineqUnion && q.settings.simulGraph && !q.settings.coord && !q.settings.graphFunc);
        CHECK(q.settings.funcType == GraphType::Param && q.settings.sketchLine == LineStyle::Thin && q.settings.plotColor == 1);
        CHECK(q.settings.xFactor == 3 && q.settings.yFactor == 1.5 && q.settings.background == BackgroundFile && q.settings.fade == 35);
        CHECK_EQ_STR(q.settings.backgroundFile, "PICOCALC/PICT/BG.pcpict");
        CHECK(q.vmemUsed == 2 && q.vmem[1] == p.vmem[1]);
        CHECK(q.tableStart == -2 && q.tableEnd == 5 && q.tableStep == 0.5);
        CHECK_EQ_STR(q.overlay, p.overlay);
        CHECK(q.sketch.size() == p.sketch.size());
        for (std::size_t i = 0; i < p.sketch.size() && i < q.sketch.size(); ++i) {
            const auto &a = p.sketch[i], &b = q.sketch[i];
            CHECK(a.kind == b.kind && a.x0 == b.x0 && a.y0 == b.y0 && a.x1 == b.x1 && a.y1 == b.y1 && a.color == b.color &&
                  a.style == b.style && a.text == b.text && a.path == b.path);
        }
        CHECK(graphfile::encode(q) == enc);                                     // stable round trip
        // Damage detection: every single changed byte before the signature is detected.
        int detected = 0, tried = 0;
        for (std::size_t i = 0; i + 16 < enc.size(); i += 7) {
            std::string bad = enc; bad[i] = static_cast<char>(bad[i] ^ 0x20); ++tried;
            graphfile::Project r;
            if (!graphfile::decode(bad, r).empty()) ++detected;
        }
        CHECK(detected == tried);
        graphfile::Project r = q;
        r.functions[0].expr = "unchanged";
        CHECK(!graphfile::decode(enc.substr(0, enc.size() / 2), r).empty());   // truncated
        CHECK_EQ_STR(r.functions[0].expr, "unchanged");                          // untouched on error
        std::string newer = enc; newer.replace(8, 1, "2");
        CHECK(graphfile::decode(newer, r).find("CRC") != std::string::npos);   // header edited: signature broken
        // A hand-written file (unknown keys ignored, CRC computed here) and invalid values.
        auto sign = [](std::string body) { return body + "crc32=" + [&] { char b[12]; std::snprintf(b, sizeof(b), "%08X", static_cast<unsigned>(storage::crc32(body.data(), body.size()))); return std::string(b); }() + "\n"; };
        CHECK(graphfile::decode(sign("PCGRAPH 2\nview=-1;1;1;-1;1;1;0;1;0.1\nend\n"), r).find("recente") != std::string::npos);
        CHECK_EQ_STR(graphfile::decode(sign("PCGRAPH 1\nview=-1;1;1;-1;1;1;0;1;0.1\nfuture=42\nf3=Y=|On|Red|Thin|x+1|\nend\n"), r), "");
        CHECK(r.functions[2].expr == "x+1" && r.functions[2].color == 1 && r.functions[0].expr.empty());
        CHECK(!graphfile::decode(sign("PCGRAPH 1\nview=1;-1;1;-1;1;1;0;1;0.1\nend\n"), r).empty());             // Xmin > Xmax
        CHECK(!graphfile::decode(sign("PCGRAPH 1\nview=-1;1;1;-1;1;1;0;1;0.1\nf21=Y=|On|Red|Thin|x|\nend\n"), r).empty());
        CHECK(!graphfile::decode(sign("PCGRAPH 1\nview=-1;1;1;-1;1;1;0;1;0.1\nf1=Q=|On|Red|Thin|x|\nend\n"), r).empty());
        CHECK(!graphfile::decode(sign("PCGRAPH 1\nview=-1;1;1;-1;1;1;0;1;0.1\nf1=Y=|On|Red|Thin|x|\nf1=Y=|On|Red|Thin|x|\nend\n"), r).empty());
        CHECK(!graphfile::decode(sign("PCGRAPH 1\nview=-1;1;1;-1;1;1;0;1;0.1\n"), r).empty());                  // no 'end'
        CHECK(!graphfile::decode(sign("PCGRAPH 1\nend\n"), r).empty());                                          // no view
        CHECK(!graphfile::decode(sign("PCGRAPH 1\nview=-1;1;1;-1;1;1;0;1;0.1\ns=Circle|0;0;-1;0|Blue|Normal||\nend\n"), r).empty());
        CHECK(!graphfile::decode(sign("PCGMEM 1\nview=-1;1;1;-1;1;1;0;1;0.1\nend\n"), r).empty());              // wrong kind
        CHECK(!graphfile::decode("", r).empty());
        // Escaping
        std::string u;
        CHECK(graphfile::unescape(graphfile::escape("a|b%c\nd\xCE\xB8"), u) && u == "a|b%c\nd\xCE\xB8");
        CHECK(graphfile::escape("x|y").find('|') == std::string::npos);
        CHECK(!graphfile::unescape("%G1", u) && !graphfile::unescape("%4", u));
    }
    // ---------------------------------------------------------------- graph memory
    {
        const graphfile::Project p = sampleProject();
        const std::string m = graphfile::encodeMemory(p.functions, p.view);
        std::array<GraphFunction, MaxGraphFunctions> f{};
        ViewWindow v;
        CHECK_EQ_STR(graphfile::decodeMemory(m, f, v), "");
        for (std::size_t i = 0; i < MaxGraphFunctions; ++i) CHECK(sameFunction(f[i], p.functions[i]));
        CHECK(v == p.view);
        CHECK(!graphfile::decodeMemory(graphfile::encode(p), f, v).empty());
        CHECK(m.size() < 1024);
    }
    // ---------------------------------------------------------------- PICT images
    {
        std::vector<std::uint16_t> px(40 * 30, 0xFFFF);
        for (int x = 0; x < 40; ++x) px[static_cast<std::size_t>(15 * 40 + x)] = 0x001F;   // axis
        for (int y = 0; y < 30; ++y) px[static_cast<std::size_t>(y * 40 + y)] = 0xF800;    // diagonal
        pict::Image img;
        CHECK(pict::encode(px.data(), 40, 40, 30, img));
        CHECK(img.width == 40 && img.height == 30 && img.rle.size() < px.size() * 2);
        img.view = {-2, 2, 1, -1, 1, 1, 0, 1, 0.1}; img.fade = 20;
        std::vector<std::uint16_t> back(px.size(), 0);
        CHECK(pict::forEachSpan(img, [&](int x, int y, int n, std::uint16_t c) { for (int i = 0; i < n; ++i) back[static_cast<std::size_t>(y * 40 + x + i)] = c; }));
        CHECK(back == px);
        const auto file = pict::serialize(img);
        pict::Image read;
        CHECK_EQ_STR(pict::parse(file.data(), file.size(), read), "");
        CHECK(read.rle == img.rle && read.view == img.view && read.fade == 20 && read.width == 40);
        auto bad = file; bad[100] ^= 1;
        CHECK(!pict::parse(bad.data(), bad.size(), read).empty());
        CHECK(!pict::parse(file.data(), file.size() - 1, read).empty());
        pict::Image tiny;
        CHECK(!pict::encode(px.data(), 40, 40, 30, tiny, 16));                     // over the size limit
        CHECK_EQ_STR(pict::memoryFileName(3), "PICT03.pcpict");
        // BMP export streamed through the file store
        RamFileSystem fs;
        CHECK(fs.makeDirs(PictFolder) == Status::Ok);
        CHECK(writeAtomic(fs, "PICOCALC/PICT/G.bmp", [&](Writer& w) { return pict::writeBmp(img, w); }) == Status::Ok);
        const auto* bmp = fs.data("PICOCALC/PICT/G.bmp");
        CHECK(bmp && bmp->size() == 54u + 120u * 30u && (*bmp)[0] == 'B' && (*bmp)[1] == 'M');
        if (bmp && bmp->size() > 60) {
            CHECK((*bmp)[54] == 0 && (*bmp)[55] == 0 && (*bmp)[56] == 0xFF);   // first pixel red (B,G,R)
            CHECK((*bmp)[57] == 0xFF && (*bmp)[58] == 0xFF && (*bmp)[59] == 0xFF);
        }
    }
    return testutil::finish("graph_file_test");
}
