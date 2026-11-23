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

// RLE data kept in RAM: 96 KB without PSRAM (v1.8.0 budget), 1 MB when the
// PSRAM pool is active (limits::ram().pictEncodedBytes).
std::size_t maxEncodedBytes() noexcept;
// v1.18.0: version 2 = two layers (graph and background, manual 5-20); the
// single-layer images (Run-Matrix captures, 3D Graph) are still written as 1.
constexpr int FormatVersion = 2;
constexpr std::uint8_t PictMemories = 20;            // Pict1..Pict20 (PICT01.pcpict ..)

struct Image {
    std::uint16_t width{}, height{};
    ViewWindow view;                    // V-Window of the graph (the T values are not used)
    std::uint8_t fade{};                // lightness when used as background (0..100 %)
    std::vector<std::uint8_t> rle;      // per row: runs of (u16 count >= 1, u16 RGB565), little endian
                                        // (v1.18.0: with two layers, their composite: what every reader draws)
    // v1.18.0: the two layers of a GRAPH picture (same RLE format); empty = single layer.
    // backLayer: background picture as shown (Fade), grid, axes, labels;
    // graphLayer: regions, curves, SKETCH, dots and the recalled picture, white = transparent.
    std::vector<std::uint8_t> backLayer, graphLayer;
    bool empty() const noexcept { return width == 0 || height == 0; }
    bool layered() const noexcept { return !graphLayer.empty(); }
};

// Encodes w x h pixels (row stride in pixels). False if the encoded data would
// be larger than maxBytes (the image is then not kept).
bool encode(const std::uint16_t* pixels, int stride, int w, int h, Image& out, std::size_t maxBytes = maxEncodedBytes());
// Calls span(x, y, length, rgb565) for every run; false when the data is damaged.
template <typename F> bool forEachRun(const std::vector<std::uint8_t>& rle, int width, int height, F&& span) {
    std::size_t i = 0;
    for (int y = 0; y < height; ++y) {
        int x = 0;
        while (x < width) {
            if (i + 4 > rle.size()) return false;
            const int n = rle[i] | (rle[i + 1] << 8);
            const auto c = static_cast<std::uint16_t>(rle[i + 2] | (rle[i + 3] << 8));
            i += 4;
            if (n <= 0 || x + n > width) return false;
            span(x, y, n, c);
            x += n;
        }
    }
    return i == rle.size();
}
template <typename F> bool forEachSpan(const Image& img, F&& span) { return forEachRun(img.rle, img.width, img.height, span); }
// v1.18.0: rle = backLayer with the non-white pixels of graphLayer over it.
// False when a layer is damaged or the composite would exceed maxBytes.
bool compose(Image& img, std::size_t maxBytes = maxEncodedBytes());
std::vector<std::uint8_t> serialize(const Image& img);
std::string parse(const std::uint8_t* data, std::size_t size, Image& out);   // "" on success
// 24-bit uncompressed BMP, rows top-down (negative height).
files::Status writeBmp(const Image& img, files::Writer& w);
// "PICT01.pcpict" .. "PICT20.pcpict"
std::string memoryFileName(int n);
// White pixels are transparent when a picture is recalled over a graph.
constexpr std::uint16_t Transparent = 0xFFFF;
} // namespace fx::pict
