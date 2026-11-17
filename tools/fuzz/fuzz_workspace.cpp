// v1.13.1 — // Fuzzing della lettura dei record del workspace: ogni record accettato deve
// salvarsi e rileggersi identico (andata e ritorno), come all'avvio successivo.
#include "workspace.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    Workspace ws;
    if (!deserializeWorkspace(d, n, ws)) return 0;
    auto bytes = serializeWorkspace(ws);
    Workspace ws2;
    if (!deserializeWorkspace(bytes.data(), bytes.size(), ws2)) { std::fprintf(stderr, "ROUNDTRIP FAIL size=%zu\n", bytes.size()); std::abort(); }
    auto bytes2 = serializeWorkspace(ws2);
    if (bytes != bytes2) { std::fprintf(stderr, "ROUNDTRIP MISMATCH\n"); std::abort(); }
    return 0;
}
