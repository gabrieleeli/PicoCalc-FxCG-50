#pragma once
#include <cstddef>
#include <cstdint>
#include "limits.hpp"

namespace gfx {
struct Rect { int x{}, y{}, w{}, h{}; };
struct Point { int x{}, y{}; };
struct PointF { float x{}, y{}; };
struct Transform2D {
    float m00{1.0F}, m01{}, m02{};
    float m10{}, m11{1.0F}, m12{};
    static Transform2D identity() noexcept { return {}; }
    static Transform2D translation(float x, float y) noexcept;
    static Transform2D scale(float x, float y) noexcept;
    static Transform2D rotation(float radians) noexcept;
    PointF apply(PointF p) const noexcept;
};
struct DirtyRegions {
    // Per-board compile-time capacity (16 on Pico 2 W, 32 with PSRAM): the
    // list lives inside the Canvas so the drawing hot path never allocates.
    // Overflow is graceful: all regions merge into one bounding rectangle.
    static constexpr std::size_t Capacity = fx::limits::kDirtyRegionCapacity;
    Rect rects[Capacity]{};
    std::size_t count{};
};
constexpr std::uint16_t rgb888To565(std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
    return static_cast<std::uint16_t>(((static_cast<std::uint16_t>(r) >> 3U) << 11U) |
        ((static_cast<std::uint16_t>(g) >> 2U) << 5U) | (static_cast<std::uint16_t>(b) >> 3U));
}
struct RGB888 { std::uint8_t r{}, g{}, b{}; };
constexpr RGB888 rgb565To888(std::uint16_t c) noexcept {
    const auto r = static_cast<std::uint8_t>((c >> 11U) & 0x1FU);
    const auto g = static_cast<std::uint8_t>((c >> 5U) & 0x3FU);
    const auto b = static_cast<std::uint8_t>(c & 0x1FU);
    return {static_cast<std::uint8_t>((r << 3U) | (r >> 2U)),
            static_cast<std::uint8_t>((g << 2U) | (g >> 4U)),
            static_cast<std::uint8_t>((b << 3U) | (b >> 2U))};
}
constexpr std::uint16_t swapRgb565Bytes(std::uint16_t c) noexcept {
    return static_cast<std::uint16_t>((c << 8U) | (c >> 8U));
}
struct Layer {
    const std::uint16_t* pixels{}; int x{}, y{}, width{}, height{};
    std::uint16_t transparentKey{}; bool visible{true}; bool useTransparency{};
};

// Allocation-free RGB565 canvas. The caller owns the framebuffer storage.
class Canvas {
public:
    Canvas(std::uint16_t* pixels, int width, int height) noexcept;
    void attach(std::uint16_t* pixels, int width, int height) noexcept;
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    Rect clip() const noexcept { return clip_; }
    Rect dirtyRect() const noexcept { return dirty_; }
    Rect consumeDirty() noexcept;
    DirtyRegions consumeDirtyRegions() noexcept;
    void setClip(Rect clip) noexcept;
    void resetClip() noexcept;
    // Marks an area as modified without drawing (e.g. after a direct copy
    // into the framebuffer, such as a window snapshot restore).
    void invalidate(Rect area) noexcept;
    // Row-wise block copies between the canvas and an off-screen layer
    // (e.g. a cached plot kept in PSRAM). `stride` is in pixels.
    void copyFrom(Rect area, const std::uint16_t* src, int stride) noexcept;
    void copyTo(Rect area, std::uint16_t* dst, int stride) const noexcept;
    void clear(std::uint16_t color) noexcept;
    void pixel(int x, int y, std::uint16_t color) noexcept;
    std::uint16_t getPixel(int x, int y) const noexcept;
    void line(int x0, int y0, int x1, int y1, std::uint16_t color) noexcept;
    void dashedLine(int x0, int y0, int x1, int y1, std::uint16_t color,
                    int dashLength = 4, int gapLength = 3) noexcept;
    void ellipse(int cx, int cy, int radiusX, int radiusY, std::uint16_t color,
                 bool filled = false) noexcept;
    void polyline(const Point* points, std::size_t count, std::uint16_t color) noexcept;
    void bezier(const Point* controlPoints, std::size_t count, std::uint16_t color,
                unsigned segments = 32) noexcept;
    void transformedPolyline(const PointF* points, std::size_t count,
                             const Transform2D& transform, std::uint16_t color) noexcept;
    void hLine(int x, int y, int width, std::uint16_t color) noexcept;
    void vLine(int x, int y, int height, std::uint16_t color) noexcept;
    void rect(int x, int y, int width, int height, std::uint16_t color, bool filled) noexcept;
    void roundedRect(int x, int y, int width, int height, int radius,
                     std::uint16_t color, bool filled) noexcept;
    void circle(int cx, int cy, int radius, std::uint16_t color, bool filled) noexcept;
    void arc(int cx, int cy, int radius, int startDegrees, int endDegrees,
             std::uint16_t color) noexcept;
    // Filled polygons use a 32-entry stack buffer and fall back to a pooled
    // scratch buffer for larger ones (up to limits::kProfile.polygonMaxVertices);
    // if the scratch allocation fails only the outline is drawn.
    void polygon(const Point* points, std::size_t count, std::uint16_t color,
                 bool filled) noexcept;
    void bitmap(int x, int y, int width, int height, const std::uint16_t* pixels,
                std::uint16_t transparent = 0x0000, bool useTransparent = false) noexcept;
    void drawLayer(const Layer& layer) noexcept;
    void blendPixel(int x, int y, std::uint16_t color, std::uint8_t alpha) noexcept;
    static std::uint16_t blend(std::uint16_t foreground, std::uint16_t background,
                               std::uint8_t alpha) noexcept;
private:
    std::uint16_t* pixels_{};
    int width_{}, height_{};
    Rect clip_{};
    Rect dirty_{};
    DirtyRegions dirtyRegions_{};
    void markDirty(Rect r) noexcept;
    bool inside(int x, int y) const noexcept;
};

namespace palette {
constexpr std::uint16_t Black=0x0000, White=0xFFFF, Navy=0x000F, Blue=0x001F;
constexpr std::uint16_t Background=0x0841, Header=0x18E3, Panel=0x2124;
constexpr std::uint16_t Border=0x4208, Cyan=0x07FF, Green=0x07E0;
constexpr std::uint16_t Orange=0xFD20, Purple=0xA81F, Red=0xF800;
}
} // namespace gfx
