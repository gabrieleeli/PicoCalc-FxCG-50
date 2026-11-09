#include "graph3d.hpp"

#include "expression.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"

#include <algorithm>
#include <cmath>
#include <memory_resource>
#include <new>
#include <vector>

namespace fx::g3d {
namespace {
constexpr float kPi = 3.14159265358979323846F;

struct Vertex {   // world-space sample
    float x, y, z;
    bool valid;
};
struct Projected { // screen space; depth grows away from the viewer
    float sx, sy, depth;
};

std::uint16_t heightColor(float t, float light) noexcept {
    // Blue -> cyan -> green -> yellow -> red gradient, scaled by the light factor.
    t = std::clamp(t, 0.0F, 1.0F);
    float r = 0, g = 0, b = 0;
    if (t < 0.25F) { r = 0; g = t * 4; b = 1; }
    else if (t < 0.5F) { r = 0; g = 1; b = 1 - (t - 0.25F) * 4; }
    else if (t < 0.75F) { r = (t - 0.5F) * 4; g = 1; b = 0; }
    else { r = 1; g = 1 - (t - 0.75F) * 4; b = 0; }
    light = std::clamp(light, 0.25F, 1.0F);
    return gfx::rgb888To565(static_cast<std::uint8_t>(r * light * 255), static_cast<std::uint8_t>(g * light * 255),
                            static_cast<std::uint8_t>(b * light * 255));
}

float edge(const Projected& a, const Projected& b, float px, float py) noexcept {
    return (b.sx - a.sx) * (py - a.sy) - (b.sy - a.sy) * (px - a.sx);
}

// Flat-shaded triangle with per-pixel depth test; contiguous visible pixels are
// emitted as horizontal runs to keep the canvas dirty tracking cheap.
void fillTriangleZ(gfx::Canvas& c, std::uint16_t* zb, const gfx::Rect& vp, const Projected& a, const Projected& b,
                   const Projected& d, std::uint16_t color, float depthMin, float depthScale) noexcept {
    const float area = edge(a, b, d.sx, d.sy);
    if (std::fabs(area) < 1e-6F) return;
    const int minX = std::max(vp.x, static_cast<int>(std::floor(std::min({a.sx, b.sx, d.sx}))));
    const int maxX = std::min(vp.x + vp.w - 1, static_cast<int>(std::ceil(std::max({a.sx, b.sx, d.sx}))));
    const int minY = std::max(vp.y, static_cast<int>(std::floor(std::min({a.sy, b.sy, d.sy}))));
    const int maxY = std::min(vp.y + vp.h - 1, static_cast<int>(std::ceil(std::max({a.sy, b.sy, d.sy}))));
    for (int y = minY; y <= maxY; ++y) {
        int runStart = -1;
        const float py = static_cast<float>(y) + 0.5F;
        for (int x = minX; x <= maxX + 1; ++x) {
            bool draw = false;
            if (x <= maxX) {
                const float px = static_cast<float>(x) + 0.5F;
                const float w0 = edge(b, d, px, py) / area;
                const float w1 = edge(d, a, px, py) / area;
                const float w2 = 1.0F - w0 - w1;
                if (w0 >= -1e-4F && w1 >= -1e-4F && w2 >= -1e-4F) {
                    const float depth = w0 * a.depth + w1 * b.depth + w2 * d.depth;
                    const float q = std::clamp((depth - depthMin) * depthScale, 0.0F, 65534.0F);
                    const auto zq = static_cast<std::uint16_t>(q);
                    std::uint16_t& slot = zb[static_cast<std::size_t>(y - vp.y) * static_cast<std::size_t>(vp.w) +
                                             static_cast<std::size_t>(x - vp.x)];
                    if (zq < slot) { slot = zq; draw = true; }
                }
            }
            if (draw) {
                if (runStart < 0) runStart = x;
            } else if (runStart >= 0) {
                c.hLine(runStart, y, x - runStart, color);
                runStart = -1;
            }
        }
    }
}

// Evaluated surface samples, kept between renders (PSRAM on the Pico Plus 2 W):
// rotating or zooming only re-projects, it never re-evaluates the expression.
struct SampleCache {
    std::pmr::vector<Vertex> verts{mem::resource(mem::Policy::PREFER_PSRAM)};
    std::string expression;
    double xmin{}, xmax{}, ymin{}, ymax{};
    std::size_t grid{};
    double zmin{}, zmax{};
    std::size_t valid{};
    bool ready{};
};
SampleCache& sampleCache() {
    static SampleCache cache;
    return cache;
}

RenderStats renderWithGrid(gfx::Canvas& canvas, const std::string& expr, const View& view, gfx::Rect vp,
                           std::size_t grid, bool zbuffer) {
    RenderStats st;
    st.grid = grid;
    st.zbuffer = zbuffer;
    st.reducedProfile = !zbuffer;
    std::pmr::memory_resource* heavy = mem::resource(mem::Policy::PREFER_PSRAM);
    const std::size_t n = grid + 1;
    SampleCache& cache = sampleCache();
    const bool reuse = cache.ready && cache.grid == grid && cache.expression == expr && cache.xmin == view.xmin &&
                       cache.xmax == view.xmax && cache.ymin == view.ymin && cache.ymax == view.ymax;
    if (!reuse) {
        cache.ready = false;
        cache.verts.clear();
        cache.verts.shrink_to_fit();
        cache.verts.resize(n * n); // may throw std::bad_alloc: the cache stays invalid
    }
    std::pmr::vector<Vertex>& verts = cache.verts;
    std::pmr::vector<Projected> proj(heavy);
    std::pmr::vector<std::uint16_t> depth(heavy);
    std::pmr::vector<std::uint32_t> order(heavy);
    proj.resize(n * n);
    st.bufferBytes = verts.size() * sizeof(Vertex) + proj.size() * sizeof(Projected);
    if (zbuffer) {
        depth.assign(static_cast<std::size_t>(vp.w) * static_cast<std::size_t>(vp.h), 0xFFFF);
        st.bufferBytes += depth.size() * sizeof(std::uint16_t);
    } else {
        order.resize(grid * grid);
        st.bufferBytes += order.size() * sizeof(std::uint32_t);
    }

    // Sample z = f(x, y) (skipped when the cached samples still match).
    double zmin = reuse ? cache.zmin : 1e300, zmax = reuse ? cache.zmax : -1e300;
    if (reuse) st.validVertices = cache.valid;
    for (std::size_t j = 0; j < n && !reuse; ++j) {
        for (std::size_t i = 0; i < n; ++i) {
            const double tx = static_cast<double>(i) / static_cast<double>(grid);
            const double ty = static_cast<double>(j) / static_cast<double>(grid);
            const double x = view.xmin + (view.xmax - view.xmin) * tx;
            const double y = view.ymin + (view.ymax - view.ymin) * ty;
            Vertex v{static_cast<float>(tx * 2 - 1), static_cast<float>(ty * 2 - 1), 0.0F, false};
            try {
                const double z = fx::evaluate(expr, x, y);
                if (std::isfinite(z)) {
                    v.z = static_cast<float>(z);
                    v.valid = true;
                    zmin = std::min(zmin, z);
                    zmax = std::max(zmax, z);
                    ++st.validVertices;
                }
            } catch (const std::bad_alloc&) {
                throw;
            } catch (...) {
            }
            verts[j * n + i] = v;
        }
    }
    if (!reuse) {
        cache.expression = expr; // may throw: leave the cache invalid
        cache.xmin = view.xmin; cache.xmax = view.xmax; cache.ymin = view.ymin; cache.ymax = view.ymax;
        cache.grid = grid;
        cache.zmin = zmin;
        cache.zmax = zmax;
        cache.valid = st.validVertices;
        cache.ready = true;
    }
    if (!st.validVertices) { st.error = "funzione non definita nel dominio"; return st; }
    st.zmin = zmin;
    st.zmax = zmax;
    const float zrange = zmax > zmin ? static_cast<float>(zmax - zmin) : 1.0F;

    // Rotate (yaw around Z, pitch around X) and project orthographically.
    const float yaw = view.yawDeg * kPi / 180.0F, pitch = view.pitchDeg * kPi / 180.0F;
    const float cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    const float scale = 0.36F * static_cast<float>(std::min(vp.w, vp.h)) * view.zoom;
    const float cx0 = static_cast<float>(vp.x) + static_cast<float>(vp.w) * 0.5F;
    const float cy0 = static_cast<float>(vp.y) + static_cast<float>(vp.h) * 0.52F;
    float dmin = 1e30F, dmax = -1e30F;
    for (std::size_t k = 0; k < n * n; ++k) {
        const Vertex& v = verts[k];
        const float zn = v.valid ? ((v.z - static_cast<float>(zmin)) / zrange * 2.0F - 1.0F) * 0.75F : 0.0F;
        const float rx = v.x * cy - v.y * sy;
        const float ry = v.x * sy + v.y * cy;
        const float screenY = ry * sp + zn * cp;   // up
        const float depthV = ry * cp - zn * sp;    // into the screen
        proj[k] = {cx0 + rx * scale, cy0 - screenY * scale, depthV};
        dmin = std::min(dmin, depthV);
        dmax = std::max(dmax, depthV);
    }
    const float depthScale = dmax > dmin ? 65000.0F / (dmax - dmin) : 1.0F;

    auto quadColor = [&](std::size_t i, std::size_t j) {
        const Vertex& a = verts[j * n + i];
        const Vertex& b = verts[j * n + i + 1];
        const Vertex& d = verts[(j + 1) * n + i];
        const float h = (a.z + b.z + d.z + verts[(j + 1) * n + i + 1].z) * 0.25F;
        // Lambert term from the quad normal (light from the viewer's upper left).
        const float dzx = (b.z - a.z) / zrange * 1.5F * static_cast<float>(grid) / 2.0F;
        const float dzy = (d.z - a.z) / zrange * 1.5F * static_cast<float>(grid) / 2.0F;
        const float len = std::sqrt(dzx * dzx + dzy * dzy + 1.0F);
        const float light = (0.35F * -dzx + 0.45F * -dzy + 0.82F) / len;
        return heightColor((h - static_cast<float>(zmin)) / zrange, 0.45F + 0.55F * light);
    };
    auto quadValid = [&](std::size_t i, std::size_t j) {
        return verts[j * n + i].valid && verts[j * n + i + 1].valid && verts[(j + 1) * n + i].valid &&
               verts[(j + 1) * n + i + 1].valid;
    };

    const gfx::Rect oldClip = canvas.clip();
    canvas.setClip(vp);
    if (zbuffer) {
        for (std::size_t j = 0; j < grid; ++j)
            for (std::size_t i = 0; i < grid; ++i) {
                if (!quadValid(i, j)) continue;
                const std::uint16_t color = quadColor(i, j);
                const Projected& p00 = proj[j * n + i];
                const Projected& p10 = proj[j * n + i + 1];
                const Projected& p01 = proj[(j + 1) * n + i];
                const Projected& p11 = proj[(j + 1) * n + i + 1];
                fillTriangleZ(canvas, depth.data(), vp, p00, p10, p11, color, dmin, depthScale);
                fillTriangleZ(canvas, depth.data(), vp, p00, p11, p01, color, dmin, depthScale);
            }
    } else {
        // Painter's algorithm: far quads first.
        std::size_t count = 0;
        for (std::size_t j = 0; j < grid; ++j)
            for (std::size_t i = 0; i < grid; ++i)
                if (quadValid(i, j)) order[count++] = static_cast<std::uint32_t>(j * grid + i);
        auto quadDepth = [&](std::uint32_t q) {
            const std::size_t i = q % grid, j = q / grid;
            return proj[j * n + i].depth + proj[j * n + i + 1].depth + proj[(j + 1) * n + i].depth +
                   proj[(j + 1) * n + i + 1].depth;
        };
        std::sort(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(count),
                  [&](std::uint32_t a, std::uint32_t b) { return quadDepth(a) > quadDepth(b); });
        for (std::size_t k = 0; k < count; ++k) {
            const std::size_t i = order[k] % grid, j = order[k] / grid;
            const auto& p00 = proj[j * n + i];
            const auto& p10 = proj[j * n + i + 1];
            const auto& p11 = proj[(j + 1) * n + i + 1];
            const auto& p01 = proj[(j + 1) * n + i];
            const gfx::Point quad[4] = {{static_cast<int>(std::lround(p00.sx)), static_cast<int>(std::lround(p00.sy))},
                                        {static_cast<int>(std::lround(p10.sx)), static_cast<int>(std::lround(p10.sy))},
                                        {static_cast<int>(std::lround(p11.sx)), static_cast<int>(std::lround(p11.sy))},
                                        {static_cast<int>(std::lround(p01.sx)), static_cast<int>(std::lround(p01.sy))}};
            canvas.polygon(quad, 4, quadColor(i, j), true);
            canvas.polygon(quad, 4, gfx::palette::Background, false);
        }
    }
    canvas.setClip(oldClip);
    st.ok = true;
    return st;
}
} // namespace

RenderStats renderSurface(gfx::Canvas& canvas, const std::string& expression, const View& view, gfx::Rect viewport) {
    const std::size_t pixels = static_cast<std::size_t>(std::max(0, viewport.w)) * static_cast<std::size_t>(std::max(0, viewport.h));
    std::size_t grid = fx::limits::graph3dGrid(pixels);
    bool zbuffer = fx::limits::graph3dZBuffer(pixels);
    for (;;) {
        try {
            return renderWithGrid(canvas, expression, view, viewport, grid, zbuffer);
        } catch (const std::bad_alloc&) {
            // Roll back to a cheaper profile: release the sample cache, drop the
            // z-buffer, then halve the grid.
            clearCache();
            if (zbuffer) { zbuffer = false; continue; }
            if (grid > 4) { grid /= 2; continue; }
            RenderStats st;
            st.error = fx::limits::kOutOfMemoryMessage;
            return st;
        }
    }
}

void clearCache() noexcept {
    SampleCache& cache = sampleCache();
    cache.ready = false;
    cache.verts.clear();
    cache.verts.shrink_to_fit();
    cache.expression.clear();
    cache.expression.shrink_to_fit();
}

void rotate(View& view, float yawDelta, float pitchDelta) noexcept {
    view.yawDeg = std::fmod(view.yawDeg + yawDelta + 360.0F, 360.0F);
    view.pitchDeg = std::clamp(view.pitchDeg + pitchDelta, -5.0F, 85.0F);
}

} // namespace fx::g3d
