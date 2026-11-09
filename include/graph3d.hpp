#pragma once
// 3D surfaces z = f(x, y) for the 3D Graph mode.
//
// The samples of each surface (Mesh) and the render buffers (projected
// vertices, 16-bit z-buffer, painter's order) are allocated through
// mem::resource(PREFER_PSRAM): on the Pimoroni Pico Plus 2 W they live in the
// 8 MB PSRAM, which allows a z-buffer over the whole viewport and grids up to
// 128 x 128 cells per surface. Without PSRAM (Pico 2 W, or PSRAM disabled) the
// reduced profile is used automatically: painter's algorithm and grids up to
// 24 x 24 (fx::limits::ram()). Up to three surfaces share one depth test, so
// intersections are drawn correctly with the z-buffer.
//
// Every allocation may throw std::bad_alloc; callers degrade (no z-buffer,
// smaller grid) and nothing stays allocated after a failure.

#include "graphics_engine.hpp"
#include "mem_pools.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory_resource>
#include <vector>

namespace fx::g3d {

struct View {
    float yawDeg{35.0F};
    float pitchDeg{28.0F};
    float zoom{1.0F};
};

// Box of the graph (the 3D V-Window). Surface points outside [zmin, zmax]
// are clipped (the quads that touch them are not drawn).
struct Window {
    double xmin{-3}, xmax{3}, ymin{-3}, ymax{3}, zmin{-3}, zmax{3};
};

struct Vertex {
    float x, y, z;   // x, y in [-1, 1] (grid position), z = f(x, y) in user units
    bool valid;
};

// Samples of one surface on a (grid + 1)^2 lattice.
struct Mesh {
    std::pmr::vector<Vertex> verts{mem::resource(mem::Policy::PREFER_PSRAM)};
    std::size_t grid{};
    std::size_t valid{};
    double zmin{}, zmax{};       // range of the valid samples
    void clear() noexcept { verts.clear(); verts.shrink_to_fit(); grid = valid = 0; zmin = zmax = 0; }
    std::size_t bytes() const noexcept { return verts.capacity() * sizeof(Vertex); }
};

// f(x, y, z) returns false where the function is undefined.
using Function = std::function<bool(double x, double y, double& z)>;
// Incremental sampling (the 3D Graph mode spreads it over several ticks so the
// keyboard stays responsive): beginSample() allocates the lattice (throws
// std::bad_alloc, leaving the mesh empty), sampleStep() evaluates at most
// `budget` points from `next` on and returns true when the mesh is complete.
// Undefined points become invalid vertices.
void beginSample(Mesh& mesh, std::size_t grid);
bool sampleStep(Mesh& mesh, const Function& f, const Window& w, std::size_t& next, std::size_t budget);
// Whole mesh at once.
void sample(Mesh& mesh, const Function& f, const Window& w, std::size_t grid);

struct RenderStats {
    bool ok{};
    bool zbuffer{};
    std::size_t bufferBytes{};   // projected vertices + z-buffer/painter's order
    std::size_t triangles{};     // triangles drawn
    const char* error{};
};

// Draws the box and the surfaces inside the viewport. zbuffer selects the
// depth-buffer renderer (else painter's algorithm). Throws std::bad_alloc.
RenderStats render(gfx::Canvas& canvas, const Mesh* const* meshes, std::size_t count, const Window& w,
                   const View& view, gfx::Rect viewport, bool zbuffer);

void rotate(View& view, float yawDelta, float pitchDelta) noexcept;
// Colour of surface `index` (0..2: blue, red, green families) at relative
// height t in [0, 1] and light.
std::uint16_t surfaceColor(std::size_t index, float t, float light) noexcept;

} // namespace fx::g3d
