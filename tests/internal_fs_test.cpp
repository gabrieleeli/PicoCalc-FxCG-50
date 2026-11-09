// Internal flash file system (v1.9.0): files::FileSystem on LittleFS over a
// NOR flash model (storage::RamFlash: erase to 0xFF, program clears bits,
// power loss after n bytes). Same contract as the FatFs backend, so GRAPH
// projects/pictures and writeAtomic()/recover() work on both stores.
#include "app_rig.hpp"
#include "flash_fs.hpp"
#include "graph_file.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include <cstring>
#include <string>
#include <vector>

using namespace fx;
using files::Status;

namespace {
std::vector<std::uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }
std::string readText(files::FileSystem& fs, const std::string& path) {
    std::vector<std::uint8_t> out;
    if (files::readAll(fs, path, out, 1u << 20) != Status::Ok) return "<err>";
    return {out.begin(), out.end()};
}
} // namespace

int main() {
    // ================================================================ basic contract
    {
        storage::RamFlash flash(256 * 1024);
        files::LittleFsFileSystem fs(flash);
        CHECK(!fs.mounted());
        CHECK(fs.status() == Status::Ok && fs.mounted() && fs.formattedAtMount());   // blank partition: formatted
        CHECK(fs.totalBytes() == 256u * 1024u && fs.describe().find("LittleFS") != std::string::npos);
        std::uint64_t free0 = 0;
        CHECK(fs.freeSpace(free0) == Status::Ok && free0 > 200u * 1024u && free0 <= 256u * 1024u);
        CHECK(fs.makeDirs("PICOCALC/GRAPH") == Status::Ok);
        CHECK(fs.makeDirs("PICOCALC/GRAPH") == Status::Ok);                           // existing: Ok
        CHECK(fs.makeDirs("picocalc/graph/SUB") == Status::Ok);                       // case-insensitive path
        std::vector<files::Entry> list;
        CHECK(fs.list("PICOCALC", list) == Status::Ok && list.size() == 1 && list[0].name == "GRAPH" && list[0].directory);
        CHECK(files::writeAtomic(fs, "PICOCALC/GRAPH/Sine.pcgraph", bytes("y=sin(x)").data(), 8) == Status::Ok);
        files::Entry e;
        CHECK(fs.stat("picocalc/graph/SINE.PCGRAPH", e) == Status::Ok && e.size == 8 && !e.directory && e.name == "Sine.pcgraph");
        CHECK(readText(fs, "PICOCALC/GRAPH/sine.pcgraph") == "y=sin(x)");
        // overwrite through the same case-insensitive name keeps one file
        CHECK(files::writeAtomic(fs, "PICOCALC/GRAPH/SINE.pcgraph", bytes("y=cos(x)!").data(), 9) == Status::Ok);
        CHECK(fs.list("PICOCALC/GRAPH", list) == Status::Ok);
        std::size_t projects = 0;
        for (const auto& x : list) projects += files::hasExtension(x.name, ".pcgraph");
        CHECK(projects == 1 && readText(fs, "PICOCALC/GRAPH/Sine.pcgraph") == "y=cos(x)!");
        // rename: an existing target is refused (FatFs semantics), case-only rename works
        CHECK(files::writeAtomic(fs, "PICOCALC/GRAPH/B.pcgraph", bytes("b").data(), 1) == Status::Ok);
        CHECK(fs.rename("PICOCALC/GRAPH/B.pcgraph", "PICOCALC/GRAPH/sine.pcgraph") == Status::Exists);
        CHECK(fs.rename("PICOCALC/GRAPH/B.pcgraph", "PICOCALC/GRAPH/b2.pcgraph") == Status::Ok);
        CHECK(fs.rename("PICOCALC/GRAPH/sine.pcgraph", "PICOCALC/GRAPH/SINE.pcgraph") == Status::Ok);
        CHECK(fs.stat("PICOCALC/GRAPH/sine.pcgraph", e) == Status::Ok && e.name == "SINE.pcgraph");
        CHECK(fs.rename("PICOCALC/GRAPH/none", "PICOCALC/GRAPH/x") == Status::NotFound);
        // remove: a non-empty folder is refused, files and empty folders go
        CHECK(fs.remove("PICOCALC/GRAPH") == Status::IoError);
        CHECK(fs.remove("PICOCALC/GRAPH/SUB") == Status::Ok);
        CHECK(fs.remove("PICOCALC/GRAPH/b2.pcgraph") == Status::Ok);
        CHECK(fs.remove("PICOCALC/GRAPH/b2.pcgraph") == Status::NotFound);
        std::unique_ptr<files::Reader> rd;
        CHECK(fs.openRead("PICOCALC/NOPE/x", rd) == Status::NotFound);
        std::unique_ptr<files::Writer> wr;
        CHECK(fs.openWrite("PICOCALC/NOPE/x", wr) == Status::NotFound);              // missing folder
        // data survive an unmount: a new instance on the same flash finds them
        files::LittleFsFileSystem again(flash);
        CHECK(again.status() == Status::Ok && !again.formattedAtMount());
        CHECK(readText(again, "PICOCALC/GRAPH/SINE.pcgraph") == "y=cos(x)!");
    }
    // ================================================================ full partition
    {
        storage::RamFlash flash(64 * 1024);
        files::LittleFsFileSystem fs(flash);
        CHECK(fs.makeDirs("D") == Status::Ok);
        std::vector<std::uint8_t> chunk(8 * 1024, 0x5A);
        Status last = Status::Ok;
        int written = 0;
        for (int i = 0; i < 20 && last == Status::Ok; ++i) {
            last = files::writeAtomic(fs, "D/F" + std::to_string(i), chunk.data(), chunk.size());
            if (last == Status::Ok) ++written;
        }
        CHECK(last == Status::Full && written >= 3 && written < 8);
        CHECK(files::recover(fs, "D") == Status::Ok);
        CHECK(fs.status() == Status::Ok && readText(fs, "D/F0").size() == chunk.size());   // earlier files intact
    }
    // ================================================================ power loss during writeAtomic
    {
        const std::string oldData(3000, 'o'), newData(5000, 'n');
        int cuts = 0, oldSeen = 0, newSeen = 0;
        for (std::size_t budget = 512; budget < 40000; budget += 1536) {
            storage::RamFlash flash(128 * 1024);
            {
                files::LittleFsFileSystem fs(flash);
                CHECK(fs.makeDirs("PICOCALC/GRAPH") == Status::Ok);
                CHECK(files::writeAtomic(fs, "PICOCALC/GRAPH/P.pcgraph", bytes(oldData).data(), oldData.size()) == Status::Ok);
                flash.failAfter(budget);
                const Status st = files::writeAtomic(fs, "PICOCALC/GRAPH/P.pcgraph", bytes(newData).data(), newData.size());
                cuts += st != Status::Ok;
            }
            flash.powerOn();
            files::LittleFsFileSystem fs(flash);                              // reboot: mount again
            CHECK(fs.status() == Status::Ok && !fs.formattedAtMount());       // never a corrupt volume
            CHECK(files::recover(fs, "PICOCALC/GRAPH") == Status::Ok);
            const std::string now = readText(fs, "PICOCALC/GRAPH/P.pcgraph");
            CHECK(now == oldData || now == newData);                          // the old or the new file, never a mix
            oldSeen += now == oldData; newSeen += now == newData;
            std::vector<files::Entry> list;
            CHECK(fs.list("PICOCALC/GRAPH", list) == Status::Ok && list.size() == 1);   // no .tmp/.bak left
        }
        CHECK(cuts > 5 && oldSeen > 0 && newSeen > 0);
    }
    // ================================================================ GRAPH projects on the internal store
    {
        board::init();
        testrig::Rig r;
        auto& ws = r.ws();
        files::FileSystem* internal = r.platform.internalFiles();
        CHECK(internal != nullptr && internal->status() == Status::Ok);
        ws.settings.fileStore = Settings::FileStore::Internal;              // SYSTEM > "Archivio file"
        // GRAPH > FILE > SAVE AS writes the project to the store chosen in SYSTEM
        r.type("5");
        CHECK(r.active<app::GraphApp>() != nullptr);
        ws.functions[0].expr = "(x)^(2)";
        r.key(pckey::F4); r.type("5"); r.type("4"); r.enter("INTPROJ");
        CHECK(!r.error());
        files::Entry e;
        CHECK(internal->stat("PICOCALC/GRAPH/INTPROJ.pcgraph", e) == Status::Ok && e.size > 0);
        CHECK(board::host::ramCard().stat("PICOCALC/GRAPH/INTPROJ.pcgraph", e) == Status::NotFound);   // not on the card
        std::vector<std::uint8_t> raw;
        CHECK(files::readAll(*internal, "PICOCALC/GRAPH/INTPROJ.pcgraph", raw, graphfile::MaxProjectBytes) == Status::Ok);
        graphfile::Project p;
        CHECK(graphfile::decode(std::string(raw.begin(), raw.end()), p).empty() && p.functions[0].expr == "(x)^(2)");
        // the setting is saved in the record (version 5)
        const auto blob = serializeWorkspace(ws);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back) && back.settings.fileStore == Settings::FileStore::Internal);
        ws.settings.fileStore = Settings::FileStore::MicroSd;
    }
    return testutil::finish("internal_fs_test");
}
