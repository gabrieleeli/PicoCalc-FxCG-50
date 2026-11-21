#pragma once
// 3D Graph drawing library (v1.14.0): lattices (meshes) of every graph type,
// exact clipping to the box of the 3D View Window, the projection defined by
// the angles θ and φ of the V-Window (fx-CG50 manual 16-4), the renderers
// (16-bit z-buffer shared by every graph, lines included, or painter's
// algorithm), box, labels and axes indicator, cross sections of the
// templates (G-Solve CROSS) and the overlays of TRACE and G-Solve.
//
// The lattices (Mesh), the projected vertices, the z-buffer and the painter's
// order are allocated through mem::resource(PREFER_PSRAM): on the Pimoroni
// Pico Plus 2 W they live in the 8 MB PSRAM (z-buffer over the whole graph
// area, 50 x 50 points per graph); without PSRAM (Pico 2 W) the reduced
// profile is used automatically (painter's algorithm, 25 x 25 points). Every
// allocation may throw std::bad_alloc; callers degrade (smaller lattice, no
// z-buffer) and nothing stays allocated after a failure.
// The model (functions, window, setup, analytic geometry) is graph3d_model.hpp.

#include "graph3d_model.hpp"
#include "graphics_engine.hpp"
#include "mem_pools.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory_resource>
#include <vector>

namespace fx::g3d {

// ---------------------------------------------------------------- lattices
struct Vertex {
    float x, y, z;   // world coordinates (user units)
    bool valid;      // false where the function is undefined (a hole)
};

// One patch of a graph: nu x nv points of a parameter domain, row-major in v.
struct Mesh {
    std::pmr::vector<Vertex> verts{mem::resource(mem::Policy::PREFER_PSRAM)};
    std::size_t nu{}, nv{};
    std::size_t valid{};
    double zmin{}, zmax{};       // z range of the valid points
    void clear() noexcept { verts.clear(); verts.shrink_to_fit(); nu = nv = valid = 0; zmin = zmax = 0; }
    std::size_t bytes() const noexcept { return verts.capacity() * sizeof(Vertex); }
};

// Point of a patch at the parameters (u, v); false where undefined.
using PointFn = std::function<bool(double u, double v, Vec3& out)>;
struct Domain { double u0{0}, u1{1}, v0{0}, v1{1}; };
double latticeAt(double a, double b, std::size_t i, std::size_t n) noexcept;   // i-th of n points of [a, b]
// Incremental sampling (spread over the ticks of the mode so the keyboard stays
// responsive): beginSample() allocates the lattice with every point invalid
// (throws std::bad_alloc, leaving the mesh empty); sampleStep() computes at
// most `budget` points from `next` on and returns true when the mesh is done.
void beginSample(Mesh& mesh, std::size_t nu, std::size_t nv);
bool sampleStep(Mesh& mesh, const PointFn& f, const Domain& d, std::size_t& next, std::size_t budget);
void sample(Mesh& mesh, const PointFn& f, const Domain& d, std::size_t nu, std::size_t nv);

// Patches of a template (Xgrid/Ygrid are the divisions of the graph itself,
// manual 16-5): Plane 1 (over the part of the plane inside the box), Sphere 1,
// Cylinder 3 (side and the two bases), Cone 2 (side and base), Line 0.
struct PatchSpec { Domain domain; std::size_t nu{}, nv{}; };
constexpr std::size_t MaxPatches = 3;
std::size_t templatePatches(const Shape& s, const Window& w, std::size_t nu, std::size_t nv, PatchSpec out[MaxPatches]);
bool templatePoint(const Shape& s, std::size_t patch, double u, double v, Vec3& out) noexcept;
// Visible part p + t d, t in [t0, t1], of a line inside the box; false if none.
bool clipLine(const Window& w, Vec3 p, Vec3 d, double& t0, double& t1) noexcept;
bool inBox(const Window& w, Vec3 p) noexcept;

// ---------------------------------------------------------------- projection
struct Camera {
    double theta{20}, phi{70};   // V-Window Angle θ / Angle φ (degrees)
    float zoom{1.0F};            // ZOOM IN / OUT on the centre of the screen
};
struct ScreenPoint { float x, y, depth; };   // depth grows away from the viewer
// The box of the window is a cube [-1, 1]^3 seen from the direction (θ, φ):
// θ turns the x axis clockwise seen from above (θ = 0: x towards the viewer,
// y to the right), φ is the angle between the line of sight and the z axis
// (φ = 0: from above, φ = 90: horizontal).
class Projector {
public:
    Projector(const Window& w, const Camera& cam, gfx::Rect viewport) noexcept;
    void normalize(Vec3 world, double& nx, double& ny, double& nz) const noexcept;
    ScreenPoint fromNormalized(double nx, double ny, double nz) const noexcept;
    ScreenPoint project(Vec3 world) const noexcept;
    Vec3 towardViewer() const noexcept { return toward_; }   // in normalized coordinates
    Vec3 screenRight() const noexcept { return right_; }
    Vec3 screenUp() const noexcept { return up_; }
    gfx::Rect viewport() const noexcept { return vp_; }
private:
    Window w_;
    gfx::Rect vp_;
    Vec3 right_{}, up_{}, toward_{};
    double scale_{}, cx_{}, cy_{};
    double sx_{}, sy_{}, sz_{}, ox_{}, oy_{}, oz_{};   // normalisation
};

// ---------------------------------------------------------------- scene
struct Item {
    const Mesh* patches[MaxPatches]{};
    std::size_t patchCount{};
    bool line{};                 // Line template: the visible segment a-b instead of patches
    Vec3 a{}, b{};
    std::uint16_t fill{}, wire{};
    bool hasFill{}, hasWire{};
};
std::uint16_t color565(Color c) noexcept;
// Colours of a graph (manual 16-7): the area colour, or the line colour when
// the area is Clear; the line template uses the area colour.
void itemColors(const Function& f, Item& item) noexcept;
std::uint16_t oppositeColor(Color area) noexcept;   // cross section colour (16-17)

struct RenderStats {
    bool ok{};
    bool zbuffer{};
    std::size_t bufferBytes{};   // projected vertices + z-buffer / painter's order
    std::size_t triangles{};     // filled triangles (after clipping)
    std::size_t lines{};         // lattice and graph lines drawn
};
// Draws the box (Setup Box), the graphs, the labels (Setup Label) and the axes
// indicator (Setup Axes) inside the viewport. zbuffer selects the depth-buffer
// renderer, else the painter's algorithm. Throws std::bad_alloc.
RenderStats render(gfx::Canvas& canvas, const Item* items, std::size_t count, const Window& w, const Setup& setup,
                   const Camera& cam, gfx::Rect viewport, bool zbuffer);
// Rotation with the cursor keys: θ stays in (-180, 180], φ in [0, 360).
void rotate(Camera& cam, double dTheta, double dPhi) noexcept;

// ---------------------------------------------------------------- overlays (no depth test)
// Segments are clipped to the box before being projected.
void drawSegment(gfx::Canvas& c, const Projector& p, const Window& w, Vec3 a, Vec3 b, std::uint16_t color, int width);
void drawPolyline(gfx::Canvas& c, const Projector& p, const Window& w, const Vec3* pts, std::size_t n, bool closed,
                  std::uint16_t color, int width);
void drawPointer(gfx::Canvas& c, int x, int y, std::uint16_t color);   // TRACE / SKETCH pointer (a cross)

// G-Solve CROSS (16-16, 16-17): the plane perpendicular to `axis` at `value`.
enum class Axis : std::uint8_t { X, Y, Z };
struct Polyline {
    std::pmr::vector<Vec3> pts{mem::resource(mem::Policy::PREFER_PSRAM)};
    bool closed{};
};
constexpr std::size_t MaxSectionCurves = 4;
// Cross section of a template with the plane (analytic: points, segments,
// circles; the cone's hyperbolas are computed point by point). Returns the
// number of curves; `coincident` is true when a Plane graph is the section
// plane itself (its outer boundary is returned, to be highlighted).
std::size_t crossSection(const Shape& s, const Window& w, Axis axis, double value, Polyline out[MaxSectionCurves], bool& coincident);
// The section plane inside the box drawn as a grid of lines.
void drawSectionPlane(gfx::Canvas& c, const Projector& p, const Window& w, Axis axis, double value, std::uint16_t color);

} // namespace fx::g3d
