// v1.13.1 — // Fuzzing dei progetti .pcgraph e delle memorie G-Mem: il CRC viene aggiunto
// dall'harness; un progetto accettato deve codificarsi e rileggersi identico.
#include "graph_file.hpp"
#include "storage.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    // Body without CRC: the harness appends a valid CRC so the parser is reached.
    std::string body(reinterpret_cast<const char*>(d), n);
    std::string text = body + "crc32=";
    char b[16]; std::snprintf(b, sizeof b, "%08X\n", static_cast<unsigned>(storage::crc32(body.data(), body.size())));
    text += b;
    graphfile::Project p;
    std::string err = graphfile::decode(text, p);
    if (err.empty()) {
        std::string again = graphfile::encode(p);
        graphfile::Project q;
        std::string e2 = graphfile::decode(again, q);
        if (!e2.empty()) { std::fprintf(stderr, "PCGRAPH ROUNDTRIP: %s\n", e2.c_str()); std::abort(); }
        if (graphfile::encode(q) != again) { std::fprintf(stderr, "PCGRAPH MISMATCH\n"); std::abort(); }
    }
    std::array<GraphFunction, MaxGraphFunctions> fns; ViewWindow v;
    std::string t2 = text; if (t2.compare(0, 7, "PCGRAPH") == 0) t2.replace(0, 7, "PCGMEM");
    (void)graphfile::decodeMemory(text, fns, v);
    return 0;
}
