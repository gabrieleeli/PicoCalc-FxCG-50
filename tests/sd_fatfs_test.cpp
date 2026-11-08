// microSD stack on the host: the SD SPI-mode driver against a simulated card
// (SDHC, byte-addressed SDv2 and SDv1 cards, rejected writes, busy cards,
// missing card) and the FatFs file store over a RAM disk and over the
// simulated card, with the same file operations GRAPH uses.
#include "fatfs_store.hpp"
#include "file_store.hpp"
#include "graph_file.hpp"
#include "sd_card.hpp"
#include "test_util.hpp"
#include <cstring>
#include <deque>
#include <string>
#include <vector>

using namespace fx;

namespace {
// SD card in SPI mode, simulated byte by byte as seen on MISO/MOSI.
class SimCard final : public sd::SpiBus {
public:
    enum class Kind { SDHC, SDv2, SDv1 };
    SimCard(Kind k, std::size_t sectors) : kind_(k), data_(sectors * 512, 0), sectors_(sectors) {}
    bool present{true};
    bool rejectWrites{false};
    int busyBytes{3};
    int readyPolls{3};                       // ACMD41 answers "idle" this many times
    std::uint32_t clock{0};
    std::size_t commands{0};
    void setClock(std::uint32_t hz) override { clock = hz; }
    void select(bool s) override { selected_ = s; if (!s) { out_.clear(); cmdLen_ = 0; } }
    bool cardPresent() override { return present; }
    std::uint32_t millis() override { return ++now_; }
    std::uint8_t transfer(std::uint8_t in) override {
        if (!present || !selected_) return 0xFF;
        // Pending response bytes (R1 of CMD24 included) go out before the card waits for data.
        if (rx_ != Rx::Data && cmdLen_ == 0 && !out_.empty()) { const std::uint8_t b = out_.front(); out_.pop_front(); return b; }
        if (rx_ == Rx::Token) { if (in == 0xFE) { rx_ = Rx::Data; wbuf_.clear(); } return 0xFF; }
        if (rx_ == Rx::Data) {
            wbuf_.push_back(in);
            if (wbuf_.size() == 514) {           // 512 data bytes + CRC
                rx_ = Rx::None;
                if (rejectWrites) out_.push_back(0x0D);
                else {
                    std::memcpy(data_.data() + waddr_ * 512, wbuf_.data(), 512);
                    out_.push_back(0x05);
                    for (int i = 0; i < busyBytes; ++i) out_.push_back(0x00);
                }
            }
            return 0xFF;
        }
        if (cmdLen_ == 0 && !out_.empty()) { const std::uint8_t b = out_.front(); out_.pop_front(); return b; }
        if (cmdLen_ == 0) { if ((in & 0xC0) == 0x40) cmd_[cmdLen_++] = in; return 0xFF; }
        cmd_[cmdLen_++] = in;
        if (cmdLen_ == 6) { cmdLen_ = 0; execute(); }
        return 0xFF;
    }
    std::vector<std::uint8_t>& data() { return data_; }
private:
    enum class Rx { None, Token, Data };
    void r1(std::uint8_t v) { out_.push_back(0xFF); out_.push_back(v); }
    void execute() {
        ++commands;
        const std::uint8_t c = cmd_[0] & 0x3F;
        const std::uint32_t arg = (static_cast<std::uint32_t>(cmd_[1]) << 24) | (cmd_[2] << 16) | (cmd_[3] << 8) | cmd_[4];
        const bool app = app_;
        app_ = false;
        const std::uint8_t idle = idle_ ? 0x01 : 0x00;
        switch (c) {
        case 0: if (cmd_[5] != 0x95) { r1(0x09); return; } idle_ = true; polls_ = readyPolls; r1(0x01); return;
        case 8:
            if (kind_ == Kind::SDv1) { r1(0x05); return; }
            if (cmd_[5] != 0x87) { r1(0x09); return; }
            r1(idle); out_.insert(out_.end(), {0x00, 0x00, static_cast<std::uint8_t>((arg >> 8) & 0x0F), static_cast<std::uint8_t>(arg)});
            return;
        case 55: app_ = true; r1(idle); return;
        case 41:
            if (!app) { r1(0x04 | idle); return; }
            if (kind_ == Kind::SDHC && !(arg & 0x40000000)) { r1(0x01); return; }   // SDHC needs HCS
            if (polls_ > 0) { --polls_; r1(0x01); return; }
            idle_ = false; r1(0x00); return;
        case 58: r1(idle); out_.insert(out_.end(), {static_cast<std::uint8_t>(0x80 | (kind_ == Kind::SDHC ? 0x40 : 0)), 0xFF, 0x80, 0x00}); return;
        case 16: r1(arg == 512 ? idle : static_cast<std::uint8_t>(0x40 | idle)); return;
        case 9: {
            if (idle_) { r1(0x01); return; }
            std::uint8_t csd[16]{};
            if (kind_ == Kind::SDHC) {
                const std::uint32_t csize = static_cast<std::uint32_t>(sectors_ / 1024 - 1);
                csd[0] = 0x40; csd[7] = static_cast<std::uint8_t>((csize >> 16) & 0x3F); csd[8] = static_cast<std::uint8_t>(csize >> 8); csd[9] = static_cast<std::uint8_t>(csize);
            } else {
                // capacity = (C_SIZE+1) * 2^(C_SIZE_MULT+2) * 2^READ_BL_LEN with READ_BL_LEN = 9, MULT = 7
                const std::uint32_t csize = static_cast<std::uint32_t>(sectors_ / 512 - 1);
                csd[5] = 9; csd[6] = static_cast<std::uint8_t>((csize >> 10) & 0x03); csd[7] = static_cast<std::uint8_t>(csize >> 2);
                csd[8] = static_cast<std::uint8_t>((csize & 0x03) << 6); csd[9] = 0x03; csd[10] = 0x80;
            }
            r1(0x00); out_.push_back(0xFF); out_.push_back(0xFE);
            out_.insert(out_.end(), csd, csd + 16); out_.push_back(0xAB); out_.push_back(0xCD);
            return;
        }
        case 17: {
            const std::uint64_t lba = kind_ == Kind::SDHC ? arg : arg / 512;
            if (idle_ || lba >= sectors_ || (kind_ != Kind::SDHC && arg % 512)) { r1(0x20); return; }
            r1(0x00); out_.push_back(0xFF); out_.push_back(0xFF); out_.push_back(0xFE);
            out_.insert(out_.end(), data_.begin() + static_cast<std::ptrdiff_t>(lba * 512), data_.begin() + static_cast<std::ptrdiff_t>(lba * 512 + 512));
            out_.push_back(0x12); out_.push_back(0x34);
            return;
        }
        case 24: {
            const std::uint64_t lba = kind_ == Kind::SDHC ? arg : arg / 512;
            if (idle_ || lba >= sectors_) { r1(0x20); return; }
            waddr_ = lba; rx_ = Rx::Token; r1(0x00);
            return;
        }
        case 13: r1(0x00); out_.push_back(0x00); return;
        default: r1(0x04 | idle); return;
        }
    }
    Kind kind_;
    std::vector<std::uint8_t> data_;
    std::size_t sectors_;
    std::deque<std::uint8_t> out_;
    std::uint8_t cmd_[6]{};
    int cmdLen_{0};
    bool selected_{false}, idle_{true}, app_{false};
    int polls_{0};
    Rx rx_{Rx::None};
    std::vector<std::uint8_t> wbuf_;
    std::uint64_t waddr_{0};
    std::uint32_t now_{0};
};

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
        CHECK(fs.status() == files::Status::Corrupt);              // not formatted
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
        CHECK(fs.status() == files::Status::Corrupt);
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
    return testutil::finish("sd_fatfs_test");
}
