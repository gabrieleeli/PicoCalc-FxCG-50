#pragma once
// 3D surface renderer for z = f(x, y) (FXAI_ENABLE_3D).
//
// Buffers are allocated through mem::resource(PREFER_PSRAM): the vertex grid,
// the projected vertices and the 16-bit z-buffer live in PSRAM on the Pico
// Plus 2 W. Sizes come from fx::limits:
//   * Pico Plus 2 W: z-buffer over the viewport, grid up to 128x128;
//   * Pico 2 W: reduced profile chosen automatically (painter's algorithm, no
//     z-buffer, grid up to 24x24) so the feature fits in SRAM.
// On allocation failure the grid is halved and the render retried; if even the
// smallest grid does not fit, an error ("Memoria insufficiente") is returned
// and nothing is left allocated.

#include "graphics_engine.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace fx::g3d {

struct View {
    float yawDeg{35.0F};
    float pitchDeg{28.0F};
    float zoom{1.0F};
    double xmin{-3.0}, xmax{3.0}, ymin{-3.0}, ymax{3.0};
};

struct RenderStats {
    bool ok{};
    std::size_t grid{};
    bool zbuffer{};
    bool reducedProfile{};
    std::size_t bufferBytes{};
    std::size_t validVertices{};
    double zmin{}, zmax{};
    const char* error{};
};

RenderStats renderSurface(gfx::Canvas& canvas, const std::string& expression, const View& view, gfx::Rect viewport);
// Releases the cached surface samples (they are kept between renders so that
// rotation/zoom only re-project the mesh).
void clearCache() noexcept;
void rotate(View& view, float yawDelta, float pitchDelta) noexcept;

} // namespace fx::g3d
