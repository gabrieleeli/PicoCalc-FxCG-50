// v1.20.0 — Fuzzing del download HTTPS delle librerie (parti portabili): parser
// della risposta HTTP/1.1 (intestazioni, Content-Length, chunked, chiusura) a
// pezzi di lunghezza scelta dal fuzzer, URL https, cartella dell'indice.
// Invarianti: mai piu' byte di corpo del limite; finish() dopo complete() e' vero.
#include "lib_fetch.hpp"
#include <cstdint>
#include <string>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 2 || n > 40000) return 0;
    const std::size_t piece = 1 + d[0] % 64;
    const std::uint64_t limit = 64 + static_cast<std::uint64_t>(d[1]) * 97;
    net::HttpBodyParser p(limit);
    std::string body;
    for (std::size_t i = 2; i < n; i += piece) {
        const std::size_t k = std::min(piece, n - i);
        if (!p.feed(reinterpret_cast<const char*>(d + i), k, body)) break;
        if (body.size() > limit) __builtin_trap();
        if (p.complete()) break;
    }
    const bool complete = p.complete();
    const bool fin = p.finish();
    if (complete && !fin) __builtin_trap();
    if (body.size() > limit) __builtin_trap();
    net::Url u;
    std::string err;
    const std::string text(reinterpret_cast<const char*>(d + 2), std::min<std::size_t>(n - 2, 400));
    if (net::parseHttpsUrl(text, u, err)) {
        if (u.host.empty() || u.path.empty() || u.path[0] != '/') __builtin_trap();
        (void)net::buildGet(u);
    }
    (void)net::urlFolder(text);
    return 0;
}
