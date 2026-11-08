// microSD stack on the host: the SD SPI-mode driver against a simulated card
// (SDHC, byte-addressed SDv2 and SDv1 cards, rejected writes, busy cards,
// missing card) and the FatFs file store over a RAM disk and over the
// simulated card, with the same file operations GRAPH uses.
#include "fatfs_store.hpp"
#include "file_store.hpp"
#include "graph_file.hpp"
#include "sd_card.hpp"
#include "sim_sd_card.hpp"
#include "test_util.hpp"
#include <cstring>
#include <deque>
#include <string>
#include <vector>

using namespace fx;

namespace {
using test::SimCard;

std::string readText(files::FileSystem& fs, const std::string& path) {
    std::vector<std::uint8_t> d;
    return files::readAll(fs, path, d, 1 << 20) == files::Status::Ok ? std::string(d.begin(), d.end()) : std::string("<missing>");
}
// The file operations of GRAPH on any FileSystem.
void exercise(files::FileSystem& fs, const char* label) {
    using files::Status;
    CHECK(fs.status() == Status::Ok);
    CHECK(fs.makeDirs(files::GraphFolder) == Status::Ok && fs.makeDirs(files::GraphFolder) == Status::Ok);
    CHECK(fs.makeDirs(files::PictFolder) == Status::Ok);
    graphfile::Project p;
    p.functions[0].expr = "x^2-2";
    p.functions[2] = {"3cos(t)", GraphType::Param, true, 3, LineStyle::Thick, "3sin(t)"};
    const std::string text = graphfile::encode(p);
    const std::string path = files::join(files::GraphFolder, std::string("PROJECT_") + label + files::ProjectExtension);
    CHECK(files::writeAtomic(fs, path, reinterpret_cast<const std::uint8_t*>(text.data()), text.size()) == Status::Ok);
    CHECK_EQ_STR(readText(fs, path), text);
    graphfile::Project q;
    CHECK_EQ_STR(graphfile::decode(readText(fs, path), q), "");
    CHECK(q.functions[2].expr2 == "3sin(t)" && q.functions[2].style == LineStyle::Thick);
    // Replace with a longer version (several sectors), case-insensitive names like FAT
    std::string big(5000, 'x');
    CHECK(files::writeAtomic(fs, path, reinterpret_cast<const std::uint8_t*>(big.data()), big.size()) == Status::Ok);
    std::string lower = path;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    CHECK(readText(fs, lower).size() == big.size());
    std::vector<files::Entry> list;
    CHECK(fs.list(files::GraphFolder, list) == Status::Ok && list.size() == 1 && list[0].size == 5000);
    CHECK(files::hasExtension(list[0].name, files::ProjectExtension));          // long file name kept
    CHECK(fs.rename(path, files::join(files::GraphFolder, "RENAMED.pcgraph")) == Status::Ok);
    files::Entry e;
    CHECK(fs.stat(files::join(files::GraphFolder, "RENAMED.pcgraph"), e) == Status::Ok && e.size == 5000 && !e.directory);
    CHECK(fs.stat(path, e) == Status::NotFound);
    CHECK(fs.stat(files::GraphFolder, e) == Status::Ok && e.directory);
    CHECK(fs.remove(files::GraphFolder) == Status::IoError);                    // not empty
    // Interrupted save left a .tmp: recover() removes it, the file is intact.
    std::unique_ptr<files::Writer> w;
    CHECK(fs.openWrite(files::join(files::GraphFolder, "RENAMED.pcgraph.tmp"), w) == Status::Ok);
    CHECK(w->write("partial", 7) == Status::Ok && w->close() == Status::Ok);
    w.reset();
    CHECK(files::recover(fs, files::GraphFolder) == Status::Ok);
    CHECK(fs.list(files::GraphFolder, list) == Status::Ok && list.size() == 1);
    CHECK(readText(fs, files::join(files::GraphFolder, "RENAMED.pcgraph")).size() == 5000);
    std::uint64_t free1 = 0, free2 = 0;
    CHECK(fs.freeSpace(free1) == Status::Ok && free1 > 0);
    std::string huge(64 * 1024, 'y');
    CHECK(files::writeAtomic(fs, files::join(files::PictFolder, "HUGE.bmp"), reinterpret_cast<const std::uint8_t*>(huge.data()), huge.size()) == Status::Ok);
    CHECK(fs.freeSpace(free2) == Status::Ok && free2 < free1);
    CHECK(fs.remove(files::join(files::PictFolder, "HUGE.bmp")) == Status::Ok);
    CHECK(fs.remove(files::join(files::PictFolder, "HUGE.bmp")) == Status::NotFound);
    CHECK(fs.list("NOPE", list) == Status::NotFound);
}
} // namespace

int main() {
    // ---------------------------------------------------------------- SD protocol
    {
        SimCard card(SimCard::Kind::SDHC, 32768);                  // 16 MB, block addressing
        sd::SdCard sdc(card);
        CHECK(sdc.init() && sdc.type() == sd::SdCard::Type::SDHC && sdc.sectors() == 32768);
        CHECK(card.clock == sd::SdCard::FastClockHz);
        std::vector<std::uint8_t> out(1024), in(1024);
        for (std::size_t i = 0; i < out.size(); ++i) out[i] = static_cast<std::uint8_t>(i * 7 + 3);
        CHECK(sdc.write(100, out.data(), 2) && sdc.read(100, in.data(), 2) && in == out);
        CHECK(std::memcmp(card.data().data() + 100 * 512, out.data(), 1024) == 0);
        CHECK(!sdc.read(32768, in.data(), 1));                     // out of range
        card.rejectWrites = true;
        CHECK(!sdc.write(5, out.data(), 1) && sdc.errors() > 0);
        card.rejectWrites = false;
        card.busyBytes = 2000;                                     // card busy longer than the timeout
        CHECK(!sdc.write(6, out.data(), 1));
        card.busyBytes = 3;
        CHECK(sdc.init() && sdc.write(6, out.data(), 1) && sdc.sync());
        card.present = false;
        CHECK(!sdc.init() && !sdc.ready());
    }
    {
        SimCard card(SimCard::Kind::SDv2, 8192);                   // byte-addressed SDv2, CSD v1
        sd::SdCard sdc(card);
        CHECK(sdc.init() && sdc.type() == sd::SdCard::Type::SDv2 && sdc.sectors() == 8192);
        std::vector<std::uint8_t> out(512, 0x5A), in(512);
        CHECK(sdc.write(4000, out.data(), 1) && sdc.read(4000, in.data(), 1) && in == out);
        CHECK(card.data()[4000 * 512] == 0x5A);
    }
    {
        SimCard card(SimCard::Kind::SDv1, 8192);                   // SD version 1: CMD8 illegal
        sd::SdCard sdc(card);
        CHECK(sdc.init() && sdc.type() == sd::SdCard::Type::SDv1 && sdc.sectors() == 8192);
        card.readyPolls = 100000;                                  // never leaves idle: init times out
        CHECK(!sdc.init());
    }
    // ---------------------------------------------------------------- FatFs on a RAM disk
    {
        sd::RamDisk disk(32768);                                   // 16 MB
        files::FatFsFileSystem fs(disk);
        CHECK(fs.status() == files::Status::Unformatted);              // not formatted
        CHECK(fs.format() == files::Status::Ok && fs.mounted());
        exercise(fs, "RAM");
        // Card removed and inserted again: re-mounted, data kept
        disk.setPresent(false);
        CHECK(fs.status() == files::Status::NoCard && !fs.mounted());
        std::vector<files::Entry> list;
        CHECK(fs.list(files::GraphFolder, list) == files::Status::NoCard);
        disk.setPresent(true);
        CHECK(fs.status() == files::Status::Ok);
        CHECK(readText(fs, "PICOCALC/GRAPH/RENAMED.pcgraph").size() == 5000);
        // Write errors: the save fails, the previous file stays complete after re-mounting
        disk.failWrites(true);
        const std::string next(3000, 'n');
        CHECK(files::writeAtomic(fs, "PICOCALC/GRAPH/RENAMED.pcgraph", reinterpret_cast<const std::uint8_t*>(next.data()), next.size()) != files::Status::Ok);
        disk.failWrites(false);
        disk.setPresent(false); (void)fs.status(); disk.setPresent(true);
        CHECK(fs.status() == files::Status::Ok);
        CHECK(files::recover(fs, "PICOCALC/GRAPH") == files::Status::Ok);
        CHECK(readText(fs, "PICOCALC/GRAPH/RENAMED.pcgraph").size() == 5000);
    }
    // ---------------------------------------------------------------- FatFs on the simulated SD card (whole stack)
    {
        SimCard card(SimCard::Kind::SDHC, 32768);
        sd::SdCard sdc(card);
        files::FatFsFileSystem fs(sdc);
        CHECK(fs.status() == files::Status::Unformatted);
        CHECK(fs.format() == files::Status::Ok);
        exercise(fs, "SD");
        card.present = false;
        CHECK(fs.status() == files::Status::NoCard);
        card.present = true;
        CHECK(fs.status() == files::Status::Ok && readText(fs, "PICOCALC/GRAPH/RENAMED.pcgraph").size() == 5000);
        CHECK(card.commands > 100);
    }
    {
        SimCard card(SimCard::Kind::SDHC, 32768);
        card.present = false;
        sd::SdCard sdc(card);
        files::FatFsFileSystem fs(sdc);
        CHECK(fs.status() == files::Status::NoCard);
    }
    // ---------------------------------------------------------------- unformatted card, faults and recovery
    {
        SimCard card(SimCard::Kind::SDHC, 32768);                  // blank card: no file system
        sd::SdCard sdc(card);
        files::FatFsFileSystem fs(sdc);
        CHECK(fs.status() == files::Status::Unformatted);
        CHECK(std::string(files::statusMessage(files::Status::Unformatted)).find("formattat") != std::string::npos);
        CHECK(fs.format() == files::Status::Ok);
        const std::string text(1500, 'p');
        auto write = [&](const char* path) { return files::writeAtomic(fs, path, reinterpret_cast<const std::uint8_t*>(text.data()), text.size()); };
        CHECK(fs.makeDirs(files::GraphFolder) == files::Status::Ok);
        CHECK(write("PICOCALC/GRAPH/A.pcgraph") == files::Status::Ok);
        // Card taken out and put back between two operations (the card-detect
        // switch was never seen open): the card lost power and is no longer in
        // SPI mode. The operation re-initialises the card and re-mounts the volume.
        card.powerCycle();
        CHECK(readText(fs, "PICOCALC/GRAPH/A.pcgraph") == text);
        card.powerCycle();
        CHECK(write("PICOCALC/GRAPH/B.pcgraph") == files::Status::Ok);
        CHECK(readText(fs, "PICOCALC/GRAPH/B.pcgraph") == text);
        // A read error (data error token) is reported, the next operation works
        card.failReads = 1000;
        std::vector<std::uint8_t> data;
        CHECK(files::readAll(fs, "PICOCALC/GRAPH/A.pcgraph", data, 1 << 20) == files::Status::IoError);
        card.failReads = 0;
        CHECK(readText(fs, "PICOCALC/GRAPH/A.pcgraph") == text);
        // A transient read error is absorbed by one retry
        card.failReads = 1;
        CHECK(readText(fs, "PICOCALC/GRAPH/B.pcgraph") == text);
        // Rejected writes: the save fails and the previous file stays intact
        card.rejectWrites = true;
        const std::string other(900, 'q');
        CHECK(files::writeAtomic(fs, "PICOCALC/GRAPH/A.pcgraph", reinterpret_cast<const std::uint8_t*>(other.data()), other.size()) != files::Status::Ok);
        card.rejectWrites = false;
        CHECK(files::recover(fs, files::GraphFolder) == files::Status::Ok);
        CHECK(readText(fs, "PICOCALC/GRAPH/A.pcgraph") == text);
        std::vector<files::Entry> list;
        CHECK(fs.list(files::GraphFolder, list) == files::Status::Ok && list.size() == 2);   // no .tmp/.bak left
    }
    // ---------------------------------------------------------------- sizes, names, folders, full card (whole SD stack)
    {
        SimCard card(SimCard::Kind::SDHC, 4096);                   // 2 MB card: FAT12, small enough to fill
        sd::SdCard sdc(card);
        files::FatFsFileSystem fs(sdc);
        CHECK(fs.format() == files::Status::Ok);
        auto bytes = [](std::size_t n) { std::vector<std::uint8_t> v(n); for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::uint8_t>(i * 131 + n); return v; };
        // sector and cluster boundaries, written and read back byte for byte
        for (std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{511}, std::size_t{512}, std::size_t{513}, std::size_t{4095}, std::size_t{4096}, std::size_t{4097}, std::size_t{65536}, std::size_t{300000}}) {
            const auto data = bytes(n);
            const std::string path = "SIZES/F" + std::to_string(n) + ".bin";
            CHECK(fs.makeDirs("SIZES") == files::Status::Ok);
            CHECK(files::writeAtomic(fs, path, data.data(), data.size()) == files::Status::Ok);
            std::vector<std::uint8_t> back;
            CHECK(files::readAll(fs, path, back, 1 << 20) == files::Status::Ok && back == data);
        }
        // nested folders and navigation
        CHECK(fs.makeDirs("A/B/C/D") == files::Status::Ok);
        const auto small = bytes(100);
        CHECK(files::writeAtomic(fs, "A/B/C/D/deep.pcgraph", small.data(), small.size()) == files::Status::Ok);
        std::vector<files::Entry> list;
        CHECK(fs.list("A/B", list) == files::Status::Ok && list.size() == 1 && list[0].directory && list[0].name == "C");
        CHECK(fs.list("/A//B/C/", list) == files::Status::Ok && list.size() == 1);   // tolerant paths
        CHECK(fs.makeDirs("A/B/C/D/deep.pcgraph") == files::Status::Exists);        // a file is not a folder
        // many files in one folder (the directory grows over several clusters)
        CHECK(fs.makeDirs("MANY") == files::Status::Ok);
        for (int i = 0; i < 150; ++i) CHECK(files::writeAtomic(fs, "MANY/PROJECT_" + std::to_string(i) + ".pcgraph", small.data(), small.size()) == files::Status::Ok);
        CHECK(fs.list("MANY", list) == files::Status::Ok && list.size() == 150);
        // names: case-only rename, long names, invalid characters
        CHECK(fs.rename("MANY/PROJECT_0.pcgraph", "MANY/project_0.pcgraph") == files::Status::Ok);
        files::Entry e;
        CHECK(fs.stat("MANY/project_0.pcgraph", e) == files::Status::Ok && e.size == 100);
        CHECK(fs.rename("MANY/PROJECT_1.pcgraph", "MANY/PROJECT_2.pcgraph") == files::Status::Exists);
        CHECK(files::writeAtomic(fs, "MANY/A_VERY_LONG_PROJECT_NAME_1234567890.pcgraph", small.data(), small.size()) == files::Status::Ok);
        CHECK(fs.openWrite("MANY/bad*name.pcgraph", *std::make_unique<std::unique_ptr<files::Writer>>()) == files::Status::InvalidName);
        // full card: a save that does not fit fails without touching the old file
        std::uint64_t freeBytes = 0;
        CHECK(fs.freeSpace(freeBytes) == files::Status::Ok && freeBytes > 0 && freeBytes < 2u * 1024 * 1024);
        const auto first = bytes(10000);
        CHECK(files::writeAtomic(fs, "KEEP.pcgraph", first.data(), first.size()) == files::Status::Ok);
        const auto filler = bytes(static_cast<std::size_t>(freeBytes));
        CHECK(files::writeAtomic(fs, "FILLER.bin", filler.data(), filler.size()) == files::Status::Full);
        const auto larger = bytes(static_cast<std::size_t>(freeBytes));
        CHECK(files::writeAtomic(fs, "KEEP.pcgraph", larger.data(), larger.size()) == files::Status::Full);
        CHECK(files::recover(fs, "") == files::Status::Ok);
        std::vector<std::uint8_t> back;
        CHECK(files::readAll(fs, "KEEP.pcgraph", back, 1 << 20) == files::Status::Ok && back == first);
        CHECK(fs.stat("FILLER.bin", e) == files::Status::NotFound && fs.stat("KEEP.pcgraph.tmp", e) == files::Status::NotFound);
        // the space is given back: the same file fits after deleting others
        for (int i = 2; i < 150; ++i) (void)fs.remove("MANY/PROJECT_" + std::to_string(i) + ".pcgraph");
        CHECK(fs.remove("SIZES/F300000.bin") == files::Status::Ok);
        const auto medium = bytes(250000);
        CHECK(files::writeAtomic(fs, "KEEP.pcgraph", medium.data(), medium.size()) == files::Status::Ok);
        CHECK(files::readAll(fs, "KEEP.pcgraph", back, 1 << 20) == files::Status::Ok && back == medium);
    }
    return testutil::finish("sd_fatfs_test");
}
