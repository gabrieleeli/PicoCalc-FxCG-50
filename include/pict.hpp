#pragma once
// PICT images (.pcpict, original format documented in docs/PCGRAPH_FORMAT.md):
// the plot area of the graph screen compressed with run-length encoding of
// RGB565 pixels, saved with the V-Window (like the fx-CG50 g3p files, whose
// format is proprietary and not used here). Recalled images stay compressed
// in RAM and are decoded span by span while drawing. BMP export (24 bit,
// top-down) makes them readable on a PC.
#include "file_store.hpp"
#include "graph_model.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::pict {

constexpr std::size_t MaxEncodedBytes = 96 * 1024;   // RLE data kept in RAM
constexpr int FormatVersion = 1;
constexpr std::uint8_t PictMemories = 20;            // Pict1..Pict20 (PICT01.pcpict ..)

struct Image {
    std::uint16_t width{}, height{};
    ViewWindow view;                    // V-Window of the graph (the T values are not used)
    std::uint8_t fade{};                // lightness when used as background (0..100 %)
    std::vector<std::uint8_t> rle;      // per row: runs of (u16 count >= 1, u16 RGB565), little endian
    bool empty() const noexcept { return width == 0 || height == 0; }
};

// Encodes w x h pixels (row stride in pixels). False if the encoded data would
// be larger than maxBytes (the image is then not kept).
bool encode(const std::uint16_t* pixels, int stride, int w, int h, Image& out, std::size_t maxBytes = MaxEncodedBytes);
// Calls span(x, y, length, rgb565) for every run; false when the data is damaged.
template <typename F> bool forEachSpan(const Image& img, F&& span) {
    std::size_t i = 0;
    for (int y = 0; y < img.height; ++y) {
        int x = 0;
        while (x < img.width) {
            if (i + 4 > img.rle.size()) return false;
            const int n = img.rle[i] | (img.rle[i + 1] << 8);
            const auto c = static_cast<std::uint16_t>(img.rle[i + 2] | (img.rle[i + 3] << 8));
            i += 4;
            if (n <= 0 || x + n > img.width) return false;
            span(x, y, n, c);
            x += n;
        }
    }
    return i == img.rle.size();
}
std::vector<std::uint8_t> serialize(const Image& img);
std::string parse(const std::uint8_t* data, std::size_t size, Image& out);   // "" on success
// 24-bit uncompressed BMP, rows top-down (negative height).
files::Status writeBmp(const Image& img, files::Writer& w);
// "PICT01.pcpict" .. "PICT20.pcpict"
std::string memoryFileName(int n);
// White pixels are transparent when a picture is recalled over a graph.
constexpr std::uint16_t Transparent = 0xFFFF;
} // namespace fx::pict
