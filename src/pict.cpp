#include "pict.hpp"
#include "storage.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace fx::pict {
namespace {
constexpr char Magic[8] = {'P', 'C', 'P', 'I', 'C', 'T', '\r', '\n'};
constexpr std::size_t HeaderSize = 8 + 2 + 2 + 2 + 1 + 1 + 9 * 8 + 4;   // 92
void put16(std::vector<std::uint8_t>& o, std::uint32_t v) { o.push_back(static_cast<std::uint8_t>(v)); o.push_back(static_cast<std::uint8_t>(v >> 8)); }
void put32(std::vector<std::uint8_t>& o, std::uint32_t v) { put16(o, v & 0xFFFF); put16(o, v >> 16); }
void putF64(std::vector<std::uint8_t>& o, double d) { std::uint64_t b; std::memcpy(&b, &d, 8); put32(o, static_cast<std::uint32_t>(b)); put32(o, static_cast<std::uint32_t>(b >> 32)); }
std::uint32_t get16(const std::uint8_t* p) { return p[0] | (p[1] << 8); }
std::uint32_t get32(const std::uint8_t* p) { return get16(p) | (get16(p + 2) << 16); }
double getF64(const std::uint8_t* p) { const std::uint64_t b = get32(p) | (static_cast<std::uint64_t>(get32(p + 4)) << 32); double d; std::memcpy(&d, &b, 8); return d; }
// RGB565 -> 8-bit channels (the engine layer does not use the graphics module).
struct Rgb { std::uint8_t r, g, b; };
Rgb expand(std::uint16_t c) {
    const unsigned r = (c >> 11) & 31u, g = (c >> 5) & 63u, b = c & 31u;
    return {static_cast<std::uint8_t>((r << 3) | (r >> 2)), static_cast<std::uint8_t>((g << 2) | (g >> 4)), static_cast<std::uint8_t>((b << 3) | (b >> 2))};
}
} // namespace

bool encode(const std::uint16_t* pixels, int stride, int w, int h, Image& out, std::size_t maxBytes) {
    if (!pixels || w <= 0 || h <= 0 || w > 4096 || h > 4096 || stride < w) return false;
    std::vector<std::uint8_t> rle;
    rle.reserve(std::min<std::size_t>(maxBytes, 16 * 1024));
    for (int y = 0; y < h; ++y) {
        const std::uint16_t* row = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
        int x = 0;
        while (x < w) {
            const std::uint16_t c = row[x];
            int n = 1;
            while (x + n < w && row[x + n] == c && n < 0xFFFF) ++n;
            if (rle.size() + 4 > maxBytes) return false;
            put16(rle, static_cast<std::uint32_t>(n)); put16(rle, c);
            x += n;
        }
    }
    out.width = static_cast<std::uint16_t>(w);
    out.height = static_cast<std::uint16_t>(h);
    out.rle.swap(rle);
    return true;
}

std::vector<std::uint8_t> serialize(const Image& img) {
    std::vector<std::uint8_t> o;
    o.reserve(HeaderSize + img.rle.size() + 4);
    o.insert(o.end(), Magic, Magic + 8);
    put16(o, FormatVersion); put16(o, img.width); put16(o, img.height);
    o.push_back(1);                       // flags: bit 0 = V-Window present
    o.push_back(img.fade);
    const ViewWindow& v = img.view;
    for (double d : {v.xmin, v.xmax, v.xscl, v.ymin, v.ymax, v.yscl, v.tmin, v.tmax, v.tstep}) putF64(o, d);
    put32(o, static_cast<std::uint32_t>(img.rle.size()));
    o.insert(o.end(), img.rle.begin(), img.rle.end());
    put32(o, storage::crc32(o.data(), o.size()));
    return o;
}

std::string parse(const std::uint8_t* data, std::size_t size, Image& out) {
    if (!data || size < HeaderSize + 4) return "file troppo corto";
    if (std::memcmp(data, Magic, 8) != 0) return "non e' un file .pcpict";
    const std::uint32_t version = get16(data + 8);
    if (version > FormatVersion) return "creato da una versione piu' recente";
    if (version < 1) return "versione non valida";
    if (storage::crc32(data, size - 4) != get32(data + size - 4)) return "CRC errato: immagine danneggiata";
    Image img;
    img.width = static_cast<std::uint16_t>(get16(data + 10));
    img.height = static_cast<std::uint16_t>(get16(data + 12));
    const std::uint8_t flags = data[14];
    img.fade = data[15];
    if (img.width == 0 || img.height == 0 || img.width > 4096 || img.height > 4096 || flags > 1 || img.fade > 100) return "intestazione non valida";
    const std::uint8_t* p = data + 16;
    double d[9];
    for (double& x : d) { x = getF64(p); p += 8; }
    img.view = {d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8]};
    if ((flags & 1) && !validateView(img.view).empty()) return "V-Window non valida";
    const std::uint32_t n = get32(p);
    p += 4;
    if (n > MaxEncodedBytes) return "immagine troppo grande per la memoria";
    if (HeaderSize + n + 4 != size) return "lunghezza dei dati errata";
    img.rle.assign(p, p + n);
    if (!forEachSpan(img, [](int, int, int, std::uint16_t) {})) return "dati dell'immagine danneggiati";
    out = std::move(img);
    return {};
}

files::Status writeBmp(const Image& img, files::Writer& w) {
    if (img.empty()) return files::Status::Corrupt;
    const std::uint32_t rowBytes = (static_cast<std::uint32_t>(img.width) * 3u + 3u) & ~3u;
    const std::uint32_t pixelBytes = rowBytes * img.height;
    std::vector<std::uint8_t> h;
    h.reserve(54);
    h.push_back('B'); h.push_back('M');
    put32(h, 54 + pixelBytes); put32(h, 0); put32(h, 54);
    put32(h, 40); put32(h, img.width);
    put32(h, static_cast<std::uint32_t>(-static_cast<std::int32_t>(img.height)));   // top-down rows
    put16(h, 1); put16(h, 24); put32(h, 0); put32(h, pixelBytes);
    put32(h, 2835); put32(h, 2835); put32(h, 0); put32(h, 0);
    files::Status st = w.write(h.data(), h.size());
    if (st != files::Status::Ok) return st;
    std::vector<std::uint8_t> row(rowBytes, 0);
    int currentRow = 0;
    bool ok = true;
    const bool parsed = forEachSpan(img, [&](int x, int y, int n, std::uint16_t c) {
        if (!ok) return;
        if (y != currentRow) {
            if (w.write(row.data(), row.size()) != files::Status::Ok) { ok = false; return; }
            currentRow = y;
        }
        const Rgb rgb = expand(c);
        for (int i = 0; i < n; ++i) {
            std::uint8_t* px = row.data() + static_cast<std::size_t>(x + i) * 3;
            px[0] = rgb.b; px[1] = rgb.g; px[2] = rgb.r;   // BMP stores B, G, R
        }
    });
    if (!parsed) return files::Status::Corrupt;
    if (!ok) return files::Status::IoError;
    return w.write(row.data(), row.size());
}

std::string memoryFileName(int n) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "PICT%02d%s", n, files::PictExtension);
    return buf;
}
} // namespace fx::pict
