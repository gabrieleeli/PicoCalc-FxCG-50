// Host tool: operates on a microSD disk image (FAT12/16/32 or exFAT) with the
// firmware's own storage code (FatFs R0.16 + files::FileSystem + .pcgraph
// codec), so that independent tools (fsck.fat, fsck.exfat, mtools) can check
// what the firmware writes and the firmware code can read what a PC writes.
//
//   sd_image_tool <image> put <card-path> <host-file>     atomic write
//   sd_image_tool <image> get <card-path> <host-file>     read
//   sd_image_tool <image> ls <card-dir>                   list (name size [dir])
//   sd_image_tool <image> rm <card-path>
//   sd_image_tool <image> mkdir <card-dir>
//   sd_image_tool <image> project <card-path>             write a sample GRAPH project
//   sd_image_tool <image> check <card-path>               decode a .pcgraph (exit 1 if invalid)
//   sd_image_tool <image> recover <card-dir>
//   sd_image_tool - sample <host-file>                    the sample project as a host file
#include "file_disk.hpp"
#include "fatfs_store.hpp"
#include "file_store.hpp"
#include "graph_file.hpp"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace fx;

namespace {
graphfile::Project sample() {
    graphfile::Project p;
    p.functions[0].expr = "(x)^(3)-4x";
    p.functions[1] = {"2sin(x)", GraphType::Y, true, 1, LineStyle::Thick, {}};
    p.functions[2] = {"3cos(t)", GraphType::Param, true, 3, LineStyle::Broken, "1.5sin(2t)"};
    p.functions[3] = {"x^2-5", GraphType::YGreaterEq, true, 4, LineStyle::Normal, {}};
    p.view = {-4, 4, 1, -6, 6, 2, 0, 6.283185307179586, 0.05};
    p.settings.grid = GridMode::Line;
    p.settings.axes = AxesMode::Scale;
    p.settings.derivative = true;
    p.vmem[0] = p.view; p.vmemUsed = 1;
    SketchItem s; s.kind = SketchItem::Kind::Text; s.x0 = 1; s.y0 = 2; s.text = "Tangente 100% | ok";
    p.sketch.push_back(s);
    p.tableStart = -3; p.tableEnd = 3; p.tableStep = 0.5;
    return p;
}
int fail(const char* what, files::Status st) {
    std::fprintf(stderr, "%s: %s\n", what, files::statusMessage(st));
    return 1;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: see the comment at the top of tools/sd_image_tool.cpp\n"); return 2; }
    const std::string image = argv[1], cmd = argv[2];
    if (cmd == "sample" && argc == 4) {
        std::ofstream(argv[3], std::ios::binary) << graphfile::encode(sample());
        return 0;
    }
    board::host::FileDisk disk(image);
    files::FatFsFileSystem fs(disk);
    files::Status st = fs.status();
    if (st != files::Status::Ok) return fail("mount", st);
    if (cmd == "put" && argc == 5) {
        std::ifstream in(argv[4], std::ios::binary);
        const std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const std::string path = argv[3];
        const auto slash = path.rfind('/');
        if (slash != std::string::npos && (st = fs.makeDirs(path.substr(0, slash))) != files::Status::Ok) return fail("mkdir", st);
        st = files::writeAtomic(fs, path, data.data(), data.size());
        return st == files::Status::Ok ? 0 : fail("put", st);
    }
    if (cmd == "get" && argc == 5) {
        std::vector<std::uint8_t> data;
        st = files::readAll(fs, argv[3], data, 64u * 1024 * 1024);
        if (st != files::Status::Ok) return fail("get", st);
        std::ofstream(argv[4], std::ios::binary).write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        return 0;
    }
    if (cmd == "ls" && argc == 4) {
        std::vector<files::Entry> list;
        if ((st = fs.list(argv[3], list)) != files::Status::Ok) return fail("ls", st);
        for (const auto& e : list) std::printf("%s %u%s\n", e.name.c_str(), static_cast<unsigned>(e.size), e.directory ? " dir" : "");
        return 0;
    }
    if (cmd == "rm" && argc == 4) return (st = fs.remove(argv[3])) == files::Status::Ok ? 0 : fail("rm", st);
    if (cmd == "mkdir" && argc == 4) return (st = fs.makeDirs(argv[3])) == files::Status::Ok ? 0 : fail("mkdir", st);
    if (cmd == "recover" && argc == 4) return (st = files::recover(fs, argv[3])) == files::Status::Ok ? 0 : fail("recover", st);
    if (cmd == "project" && argc == 4) {
        const std::string text = graphfile::encode(sample());
        const std::string path = argv[3];
        const auto slash = path.rfind('/');
        if (slash != std::string::npos && (st = fs.makeDirs(path.substr(0, slash))) != files::Status::Ok) return fail("mkdir", st);
        st = files::writeAtomic(fs, path, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
        return st == files::Status::Ok ? 0 : fail("project", st);
    }
    if (cmd == "check" && argc == 4) {
        std::vector<std::uint8_t> data;
        if ((st = files::readAll(fs, argv[3], data, graphfile::MaxProjectBytes)) != files::Status::Ok) return fail("check", st);
        graphfile::Project p;
        const std::string err = graphfile::decode(std::string(data.begin(), data.end()), p);
        if (!err.empty()) { std::fprintf(stderr, "decode: %s\n", err.c_str()); return 1; }
        const graphfile::Project want = sample();
        const bool same = graphfile::encode(p) == graphfile::encode(want);
        std::printf("%s: valid project, %s the sample\n", argv[3], same ? "equal to" : "different from");
        return 0;
    }
    std::fprintf(stderr, "unknown command\n");
    return 2;
}
