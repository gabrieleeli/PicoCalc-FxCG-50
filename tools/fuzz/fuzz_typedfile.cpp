// v1.15.0 (F3) — Fuzzing dei file tipizzati: decodifica con intestazione e CRC
// (eActivity, fogli, figure...) e validazione UTF-8 dei file di testo. Primo
// byte: tipo; ogni file accettato si ricodifica identico.
#include "typed_files.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n < 1) return 0;
    const auto type = static_cast<files::FileType>(d[0] % files::FileTypeCount);
    const std::vector<std::uint8_t> raw(d + 1, d + n);
    std::vector<std::uint8_t> payload;
    std::uint16_t version = 0;
    if (files::decodeTypedFile(type, raw, payload, &version) != files::Status::Ok) return 0;
    if (version == files::fileType(type).formatVersion && files::encodeTypedFile(type, payload) != raw) { std::fprintf(stderr, "ROUNDTRIP MISMATCH\n"); std::abort(); }
    return 0;
}
