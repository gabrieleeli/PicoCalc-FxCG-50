// 3D surface renderer (orthographic projection, flat shading). Derived from
// the v1.9.0 renderer; extended to several surfaces sharing one depth test,
// the 3D V-Window box and z clipping.
#include "graph3d.hpp"

#include <algorithm>
#include <cmath>
#include <new>

namespace fx::g3d {
namespace {
constexpr float kPi = 3.14159265358979323846F;
constexpr float kZScale = 0.75F;   // box height relative to its width

struct Projected {   // screen space; depth grows away from the viewer
    float sx, sy, depth;
};

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

struct Projector {
    float cy, sy, cp, sp, scale, cx0, cy0;
    Projector(const View& view, const gfx::Rect& vp) {
        const float yaw = view.yawDeg * kPi / 180.0F, pitch = view.pitchDeg * kPi / 180.0F;
        cy = std::cos(yaw); sy = std::sin(yaw); cp = std::cos(pitch); sp = std::sin(pitch);
        scale = 0.36F * static_cast<float>(std::min(vp.w, vp.h)) * view.zoom;
        cx0 = static_cast<float>(vp.x) + static_cast<float>(vp.w) * 0.5F;
        cy0 = static_cast<float>(vp.y) + static_cast<float>(vp.h) * 0.52F;
    }
    // x, y in [-1, 1], zn in [-kZScale, kZScale].
    Projected operator()(float x, float y, float zn) const noexcept {
        const float rx = x * cy - y * sy;
        const float ry = x * sy + y * cy;
        const float screenY = ry * sp + zn * cp;   // up
        const float depth = ry * cp - zn * sp;     // into the screen
        return {cx0 + rx * scale, cy0 - screenY * scale, depth};
    }
};

gfx::Point toPoint(const Projected& p) noexcept {
    return {static_cast<int>(std::lround(p.sx)), static_cast<int>(std::lround(p.sy))};
}
} // namespace

std::uint16_t surfaceColor(std::size_t index, float t, float light) noexcept {
    t = std::clamp(t, 0.0F, 1.0F);
    light = std::clamp(light, 0.25F, 1.0F);
    float r = 0, g = 0, b = 0;
    // One hue family per surface, in the order of the GRAPH relations (blue,
    // red, green), so that intersecting surfaces stay distinguishable; the
    // brightness follows the height and the light.
    switch (index % 3) {
    case 0:   // dark blue -> sky blue
        r = 0.05F + 0.45F * t; g = 0.25F + 0.55F * t; b = 0.6F + 0.4F * t;
        break;
    case 1:   // dark red -> orange
        r = 0.6F + 0.4F * t; g = 0.1F + 0.5F * t; b = 0.1F + 0.2F * t;
        break;
    default:  // dark green -> light green
        r = 0.1F + 0.4F * t; g = 0.45F + 0.55F * t; b = 0.1F + 0.3F * t;
        break;
    }
    return gfx::rgb888To565(static_cast<std::uint8_t>(r * light * 255), static_cast<std::uint8_t>(g * light * 255),
                            static_cast<std::uint8_t>(b * light * 255));
}

void beginSample(Mesh& mesh, std::size_t grid) {
    mesh.clear();
    if (grid < 1) grid = 1;
    const std::size_t n = grid + 1;
    try {
        mesh.verts.resize(n * n, Vertex{0.0F, 0.0F, 0.0F, false});
    } catch (...) {
        mesh.clear();
        throw;
    }
    mesh.grid = grid;
}

bool sampleStep(Mesh& mesh, const Function& f, const Window& w, std::size_t& next, std::size_t budget) {
    const std::size_t grid = mesh.grid, n = grid + 1, total = mesh.verts.size();
    if (grid == 0) return true;
    for (; next < total && budget > 0; ++next, --budget) {
        const std::size_t i = next % n, j = next / n;
        const double tx = static_cast<double>(i) / static_cast<double>(grid);
        const double ty = static_cast<double>(j) / static_cast<double>(grid);
        const double x = w.xmin + (w.xmax - w.xmin) * tx;
        const double y = w.ymin + (w.ymax - w.ymin) * ty;
        Vertex v{static_cast<float>(tx * 2 - 1), static_cast<float>(ty * 2 - 1), 0.0F, false};
        double z = 0;
        if (f(x, y, z) && std::isfinite(z) && std::fabs(z) < 3.0e38) {
            v.z = static_cast<float>(z);
            v.valid = true;
            if (mesh.valid == 0) { mesh.zmin = mesh.zmax = z; }
            else { mesh.zmin = std::min(mesh.zmin, z); mesh.zmax = std::max(mesh.zmax, z); }
            ++mesh.valid;
        }
        mesh.verts[next] = v;
    }
    return next >= total;
}

void sample(Mesh& mesh, const Function& f, const Window& w, std::size_t grid) {
    beginSample(mesh, grid);
    std::size_t next = 0;
    (void)sampleStep(mesh, f, w, next, mesh.verts.size());
}

RenderStats render(gfx::Canvas& canvas, const Mesh* const* meshes, std::size_t count, const Window& w,
                   const View& view, gfx::Rect vp, bool zbuffer) {
    RenderStats st;
    st.zbuffer = zbuffer;
    if (vp.w <= 0 || vp.h <= 0) { st.error = "area del grafico vuota"; return st; }
    std::pmr::memory_resource* heavy = mem::resource(mem::Policy::PREFER_PSRAM);
    const Projector project(view, vp);
    const float zrange = w.zmax > w.zmin ? static_cast<float>(w.zmax - w.zmin) : 1.0F;
    const auto zNorm = [&](float z) { return ((z - static_cast<float>(w.zmin)) / zrange * 2.0F - 1.0F) * kZScale; };
    const auto inBox = [&](const Vertex& v) {
        return v.valid && v.z >= static_cast<float>(w.zmin) && v.z <= static_cast<float>(w.zmax);
    };

    const gfx::Rect oldClip = canvas.clip();
    canvas.setClip(vp);
    // Box (3D V-Window): drawn first, the surfaces cover its hidden edges.
    const float corners[8][3] = {{-1, -1, -kZScale}, {1, -1, -kZScale}, {1, 1, -kZScale}, {-1, 1, -kZScale},
                                 {-1, -1, kZScale},  {1, -1, kZScale},  {1, 1, kZScale},  {-1, 1, kZScale}};
    Projected box[8];
    for (int k = 0; k < 8; ++k) box[k] = project(corners[k][0], corners[k][1], corners[k][2]);
    const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    const std::uint16_t boxColor = gfx::rgb888To565(150, 150, 160);
    for (const auto& e : edges) {
        const gfx::Point a = toPoint(box[e[0]]), b = toPoint(box[e[1]]);
        canvas.line(a.x, a.y, b.x, b.y, boxColor);
    }

    // Project every surface (vertex order = mesh order, one block per surface).
    std::size_t total = 0;
    for (std::size_t s = 0; s < count; ++s) if (meshes[s]) total += meshes[s]->verts.size();
    std::pmr::vector<Projected> proj(heavy);
    proj.resize(total);
    st.bufferBytes = proj.size() * sizeof(Projected);
    float dmin = 1e30F, dmax = -1e30F;
    {
        std::size_t k = 0;
        for (std::size_t s = 0; s < count; ++s) {
            if (!meshes[s]) continue;
            for (const Vertex& v : meshes[s]->verts) {
                const Projected p = project(v.x, v.y, zNorm(v.valid ? v.z : static_cast<float>(w.zmin)));
                proj[k++] = p;
                dmin = std::min(dmin, p.depth);
                dmax = std::max(dmax, p.depth);
            }
        }
    }
    const float depthScale = dmax > dmin ? 65000.0F / (dmax - dmin) : 1.0F;

    // Quad helpers (index = vertex offset of the surface + lattice position).
    struct Quad { std::uint32_t base; std::uint16_t i, j; std::uint8_t surface; };
    auto quadColor = [&](const Mesh& m, std::size_t surface, std::size_t i, std::size_t j) {
        const std::size_t n = m.grid + 1;
        const Vertex& a = m.verts[j * n + i];
        const Vertex& b = m.verts[j * n + i + 1];
        const Vertex& d = m.verts[(j + 1) * n + i];
        const float h = (a.z + b.z + d.z + m.verts[(j + 1) * n + i + 1].z) * 0.25F;
        // Lambert term from the quad normal (light from the viewer's upper left).
        const float dzx = (b.z - a.z) / zrange * kZScale * static_cast<float>(m.grid);
        const float dzy = (d.z - a.z) / zrange * kZScale * static_cast<float>(m.grid);
        const float len = std::sqrt(dzx * dzx + dzy * dzy + 1.0F);
        const float light = (0.35F * -dzx + 0.45F * -dzy + 0.82F) / len;
        return surfaceColor(surface, (h - static_cast<float>(w.zmin)) / zrange, 0.45F + 0.55F * light);
    };
    auto quadVisible = [&](const Mesh& m, std::size_t i, std::size_t j) {
        const std::size_t n = m.grid + 1;
        return inBox(m.verts[j * n + i]) && inBox(m.verts[j * n + i + 1]) && inBox(m.verts[(j + 1) * n + i]) &&
               inBox(m.verts[(j + 1) * n + i + 1]);
    };

    if (zbuffer) {
        std::pmr::vector<std::uint16_t> depth(heavy);
        depth.assign(static_cast<std::size_t>(vp.w) * static_cast<std::size_t>(vp.h), 0xFFFF);
        st.bufferBytes += depth.size() * sizeof(std::uint16_t);
        std::size_t base = 0;
        for (std::size_t s = 0; s < count; ++s) {
            if (!meshes[s]) continue;
            const Mesh& m = *meshes[s];
            const std::size_t n = m.grid + 1;
            for (std::size_t j = 0; j < m.grid; ++j)
                for (std::size_t i = 0; i < m.grid; ++i) {
                    if (!quadVisible(m, i, j)) continue;
                    const std::uint16_t color = quadColor(m, s, i, j);
                    const Projected& p00 = proj[base + j * n + i];
                    const Projected& p10 = proj[base + j * n + i + 1];
                    const Projected& p01 = proj[base + (j + 1) * n + i];
                    const Projected& p11 = proj[base + (j + 1) * n + i + 1];
                    fillTriangleZ(canvas, depth.data(), vp, p00, p10, p11, color, dmin, depthScale);
                    fillTriangleZ(canvas, depth.data(), vp, p00, p11, p01, color, dmin, depthScale);
                    st.triangles += 2;
                }
            base += m.verts.size();
        }
    } else {
        // Painter's algorithm over the quads of every surface: far quads first.
        std::pmr::vector<Quad> order(heavy);
        std::size_t quads = 0;
        for (std::size_t s = 0; s < count; ++s) if (meshes[s]) quads += meshes[s]->grid * meshes[s]->grid;
        order.reserve(quads);
        st.bufferBytes += quads * sizeof(Quad);
        std::size_t base = 0;
        for (std::size_t s = 0; s < count; ++s) {
            if (!meshes[s]) continue;
            const Mesh& m = *meshes[s];
            for (std::size_t j = 0; j < m.grid; ++j)
                for (std::size_t i = 0; i < m.grid; ++i)
                    if (quadVisible(m, i, j))
                        order.push_back({static_cast<std::uint32_t>(base), static_cast<std::uint16_t>(i),
                                         static_cast<std::uint16_t>(j), static_cast<std::uint8_t>(s)});
            base += m.verts.size();
        }
        auto quadDepth = [&](const Quad& q) {
            const std::size_t n = meshes[q.surface]->grid + 1;
            return proj[q.base + q.j * n + q.i].depth + proj[q.base + q.j * n + q.i + 1].depth +
                   proj[q.base + (q.j + 1) * n + q.i].depth + proj[q.base + (q.j + 1) * n + q.i + 1].depth;
        };
        std::sort(order.begin(), order.end(), [&](const Quad& a, const Quad& b) { return quadDepth(a) > quadDepth(b); });
        for (const Quad& q : order) {
            const Mesh& m = *meshes[q.surface];
            const std::size_t n = m.grid + 1;
            const gfx::Point quad[4] = {toPoint(proj[q.base + q.j * n + q.i]), toPoint(proj[q.base + q.j * n + q.i + 1]),
                                        toPoint(proj[q.base + (q.j + 1) * n + q.i + 1]), toPoint(proj[q.base + (q.j + 1) * n + q.i])};
            canvas.polygon(quad, 4, quadColor(m, q.surface, q.i, q.j), true);
            canvas.polygon(quad, 4, gfx::palette::Background, false);
            st.triangles += 2;
        }
    }
    canvas.setClip(oldClip);
    st.ok = true;
    return st;
}

void rotate(View& view, float yawDelta, float pitchDelta) noexcept {
    // fmod keeps the sign of the dividend: normalise into [0, 360) for any delta.
    view.yawDeg = std::fmod(view.yawDeg + yawDelta, 360.0F);
    if (view.yawDeg < 0.0F) view.yawDeg += 360.0F;
    if (view.yawDeg >= 360.0F) view.yawDeg = 0.0F;
    view.pitchDeg = std::clamp(view.pitchDeg + pitchDelta, -5.0F, 85.0F);
}

} // namespace fx::g3d
