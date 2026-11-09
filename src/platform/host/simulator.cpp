// PicoCalc FX host simulator: runs the real Firmware class against the host
// board port and writes PNG screenshots. Script commands (one per line):
//   type <text>      press the characters of <text>
//   key <NAME>       F1..F10 EXE EXIT MENU DEL DELF AC STO UP DOWN LEFT RIGHT HOME END PGUP PGDN
//   ctrl <c>         Ctrl+character
//   hold <NAME>      long press (first HOLD report)
//   wait <ms>        advance the simulated clock
//   shot <file.png>  save a screenshot (2x scale)
// The microSD card is a 16 MB RAM card; with PICOCALC_SD_IMAGE=<file> it is a
// FAT/exFAT disk image instead (for example a copy of a real card), used
// through the same FatFs code as the firmware.
#include "board_config.hpp"
#include "board_host.hpp"
#include "board_platform.hpp"
#include "board_port.hpp"
#include "fatfs_store.hpp"
#include "file_disk.hpp"
#include "firmware.hpp"
#include "keyboard_input.hpp"
#include "mem_pools.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace {
std::uint16_t press(std::uint8_t code, std::uint8_t state = 1) { return static_cast<std::uint16_t>((state << 8) | code); }
const std::map<std::string, std::uint8_t>& names() {
    using namespace fx::pckey;
    static const std::map<std::string, std::uint8_t> m = {
        {"F1", F1}, {"F2", F2}, {"F3", F3}, {"F4", F4}, {"F5", F5}, {"F6", F6}, {"F7", F7}, {"F8", F8}, {"F9", F9}, {"F10", F10},
        {"SHIFT", F7}, {"OPTN", F8}, {"VARS", F9}, {"SETUP", F10},
        {"EXE", Enter}, {"EXIT", Esc}, {"MENU", Tab}, {"DEL", Backspace}, {"DELF", Del}, {"AC", Break}, {"STO", Insert},
        {"UP", Up}, {"DOWN", Down}, {"LEFT", Left}, {"RIGHT", Right}, {"HOME", Home}, {"END", End}, {"PGUP", PageUp}, {"PGDN", PageDown}};
    return m;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: %s <out-dir> <script|->\n", argv[0]); return 2; }
    const std::string out = argv[1];
    std::ifstream file;
    std::istream* in = &std::cin;
    if (std::string(argv[2]) != "-") { file.open(argv[2]); if (!file) { std::fprintf(stderr, "cannot open %s\n", argv[2]); return 2; } in = &file; }
    board::init();
    // Same memory as the module: with the Pico Plus 2 W profile the 8 MB PSRAM
    // pool exists (on the device it is created after the boot self-test), so
    // GRAPH, PICT and 3D Graph use the PSRAM limits as on the calculator.
    if (board::config::kHasPsram) mem::testing::simulatePsram(board::config::kPsramExpectedBytes);
    const char* image = std::getenv("PICOCALC_SD_IMAGE");
    static board::host::FileDisk disk(image ? image : "");
    static fx::files::FatFsFileSystem imageFs(disk);
    if (image) {
        if (imageFs.status() != fx::files::Status::Ok) { std::fprintf(stderr, "cannot mount %s: %s\n", image, fx::files::statusMessage(imageFs.status())); return 2; }
        board::host::setFileSystem(&imageFs);
    }
    fx::BoardPlatform platform;
    fx::Firmware fw(platform, board::storageFlash());
    fw.boot();
    // Like the firmware main loop: keys, then frames while the active mode
    // still has work (e.g. GRAPH draws its curves incrementally).
    auto pump = [&]() {
        std::uint16_t raw;
        while (board::pollKey(raw)) fw.rawKey(raw);
        board::host::advance(20);
        for (int i = 0; i < 2000 && fw.step(); ++i) { fw.draw(board::graphics()); board::present(); board::host::advance(2); }
    };
    pump();
    std::string line;
    int lineNo = 0;
    while (std::getline(*in, line)) {
        ++lineNo;
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string cmd; ss >> cmd;
        std::string arg; std::getline(ss, arg); if (!arg.empty() && arg[0] == ' ') arg.erase(0, 1);
        if (cmd == "type") { for (char c : arg) { board::host::pushKey(press(static_cast<std::uint8_t>(c))); pump(); } }
        else if (cmd == "key" || cmd == "hold") {
            auto it = names().find(arg);
            if (it == names().end()) { std::fprintf(stderr, "line %d: unknown key %s\n", lineNo, arg.c_str()); return 1; }
            board::host::pushKey(press(it->second, cmd == "hold" ? 2 : 1));
            pump();
            board::host::pushKey(press(it->second, 3));
            pump();
        } else if (cmd == "ctrl") {
            board::host::pushKey(press(fx::pckey::ModCtrl, 1));
            board::host::pushKey(press(static_cast<std::uint8_t>(arg[0])));
            board::host::pushKey(press(fx::pckey::ModCtrl, 3));
            pump();
        } else if (cmd == "wait") { board::host::advance(static_cast<std::uint32_t>(std::stoul(arg))); pump(); }
        else if (cmd == "shot") {
            fw.draw(board::graphics()); board::present();
            if (!board::host::savePng(out + "/" + arg)) { std::fprintf(stderr, "cannot write %s\n", arg.c_str()); return 1; }
            std::printf("shot %s\n", arg.c_str());
        } else { std::fprintf(stderr, "line %d: unknown command %s\n", lineNo, cmd.c_str()); return 1; }
    }
    return 0;
}
