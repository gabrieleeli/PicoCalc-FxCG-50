#include "graph_renderer.hpp"
#include "expression.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory_resource>
#include <new>

namespace fx {
namespace {
struct Geometry { int left, right, top, bottom; };
Geometry geometryFor(const gfx::Canvas& c) { return {12, c.width() - 12, 42, c.height() - 52}; }
gfx::Rect plotClip(const gfx::Canvas& c) { return {8, 34, std::max(0, c.width() - 16), std::max(0, c.height() - 78)}; }

struct Cache {
    std::pmr::vector<float> samples{mem::resource(mem::Policy::PREFER_PSRAM)};
    std::vector<std::string> functions;
    double xmin{}, xmax{};
    std::size_t columns{}, spp{};
    bool samplesValid{};
    std::uint16_t* layer{};
    std::size_t layerBytes{};
    gfx::Rect layerRect{};
    std::vector<std::string> layerFunctions;
    GraphView layerView{};
    int layerWidth{}, layerHeight{};
    bool layerValid{};
    GraphRenderer::CacheStats stats{};
};
Cache& cache() { static Cache c; return c; }

float sampleAt(const std::string& expression, double x) noexcept {
    try {
        const double y = evaluate(expression, x);
        return std::isfinite(y) ? static_cast<float>(y) : std::numeric_limits<float>::quiet_NaN();
    } catch (const std::bad_alloc&) {
        return std::numeric_limits<float>::quiet_NaN();
    } catch (...) {
        return std::numeric_limits<float>::quiet_NaN();
    }
}

void dropLayer(Cache& k) noexcept {
    mem::free(k.layer);
    k.layer = nullptr;
    k.layerBytes = 0;
    k.layerValid = false;
}

// Ensures the sample cache matches the functions and the horizontal range.
// Returns false if there is not enough memory (caller falls back).
bool refreshSamples(Cache& k, const std::vector<std::string>& functions, const GraphView& v, std::size_t columns) {
    const std::size_t spp = limits::graphSamplesPerPixel(functions.size(), columns);
    if (k.samplesValid && k.functions == functions && k.xmin == v.xmin && k.xmax == v.xmax &&
        k.columns == columns && k.spp == spp) {
        ++k.stats.sampleHits;
        return true;
    }
    ++k.stats.sampleMisses;
    k.samplesValid = false;
    try {
        k.samples.resize(functions.size() * columns * spp);
        k.functions = functions;
    } catch (const std::bad_alloc&) {
        k.samples.clear();
        k.samples.shrink_to_fit();
        k.functions.clear();
        return false;
    }
    const double span = v.xmax - v.xmin;
    const double denom = columns > 1 ? static_cast<double>(columns - 1) : 1.0;
    for (std::size_t f = 0; f < functions.size(); ++f)
        for (std::size_t col = 0; col < columns; ++col)
            for (std::size_t s = 0; s < spp; ++s) {
                const double t = (static_cast<double>(col) + static_cast<double>(s) / static_cast<double>(spp)) / denom;
                k.samples[(f * columns + col) * spp + s] = functions[f].empty()
                    ? std::numeric_limits<float>::quiet_NaN() : sampleAt(functions[f], v.xmin + span * t);
            }
    k.xmin = v.xmin;
    k.xmax = v.xmax;
    k.columns = columns;
    k.spp = spp;
    k.samplesValid = true;
    k.stats.functions = functions.size();
    k.stats.columns = columns;
    k.stats.samplesPerPixel = spp;
    k.stats.sampleBytes = k.samples.size() * sizeof(float);
    return true;
}

void drawFrame(gfx::Canvas& c, const GraphView& v, const Geometry& g) {
    c.rect(g.left, g.top, g.right - g.left, g.bottom - g.top, gfx::palette::Border, false);
    auto sx = [&](double x) { return g.left + static_cast<int>(std::lround((x - v.xmin) * (g.right - g.left) / (v.xmax - v.xmin))); };
    auto sy = [&](double y) { return g.bottom - static_cast<int>(std::lround((y - v.ymin) * (g.bottom - g.top) / (v.ymax - v.ymin))); };
    const double dx = (v.xmax - v.xmin) / 10.0, dy = (v.ymax - v.ymin) / 10.0;
    if (dx > 0 && dy > 0) {
        for (int i = 0; i <= 10; ++i) {
            const int x = sx(v.xmin + i * dx), y = sy(v.ymin + i * dy);
            c.vLine(x, g.top, g.bottom - g.top, gfx::palette::Panel);
            c.hLine(g.left, y, g.right - g.left, gfx::palette::Panel);
        }
    }
    if (v.xmin <= 0 && v.xmax >= 0) c.vLine(sx(0), g.top, g.bottom - g.top, gfx::palette::White);
    if (v.ymin <= 0 && v.ymax >= 0) c.hLine(g.left, sy(0), g.right - g.left, gfx::palette::White);
}

// Curve from a strided sample array (min/max span per column + joins).
void drawCurve(gfx::Canvas& c, const GraphView& v, const Geometry& g, const float* samples, std::size_t columns,
               std::size_t spp, std::uint16_t color) {
    auto sy = [&](double y) { return g.bottom - static_cast<int>(std::lround((y - v.ymin) * (g.bottom - g.top) / (v.ymax - v.ymin))); };
    bool previous = false;
    int prevY = 0;
    for (std::size_t col = 0; col < columns; ++col) {
        const int x = g.left + static_cast<int>(col);
        bool any = false, lastValid = false;
        int first = 0, last = 0, lo = 0, hi = 0;
        for (std::size_t s = 0; s < spp; ++s) {
            const float y = samples[col * spp + s];
            if (!std::isfinite(y) || y < v.ymin || y > v.ymax) { lastValid = false; continue; }
            const int py = sy(y);
            if (!any) { first = lo = hi = py; any = true; }
            lo = std::min(lo, py);
            hi = std::max(hi, py);
            last = py;
            lastValid = true;
        }
        if (!any) { previous = false; continue; }
        if (previous && std::abs(first - prevY) < (g.bottom - g.top)) c.line(x - 1, prevY, x, first, color);
        c.vLine(x, lo, hi - lo + 1, color);
        previous = lastValid;
        prevY = last;
    }
}

void drawTrace(gfx::Canvas& c, const std::string& expression, const GraphView& v, const Geometry& g, std::uint16_t color) {
    const int cx = std::clamp(v.cursorX, g.left, g.right);
    const double xv = v.xmin + (v.xmax - v.xmin) * (cx - g.left) / (g.right - g.left);
    c.vLine(cx, g.top, g.bottom - g.top, gfx::palette::Orange);
    const float yv = sampleAt(expression, xv);
    if (!std::isfinite(yv)) return;
    const int cy = g.bottom - static_cast<int>(std::lround((yv - v.ymin) * (g.bottom - g.top) / (v.ymax - v.ymin)));
    if (cy >= g.top && cy <= g.bottom) c.circle(cx, cy, 3, color, true);
}
} // namespace

std::uint16_t GraphRenderer::colorFor(std::size_t index) noexcept {
    static constexpr std::uint16_t colors[] = {gfx::palette::Cyan, gfx::palette::Orange, gfx::palette::Green,
                                               0xF81F, 0xFFE0, gfx::palette::Red, 0x867F, 0xFD20 ^ 0x0841};
    return colors[index % (sizeof colors / sizeof colors[0])];
}

void GraphRenderer::draw(gfx::Canvas& c, const std::string& expression, const GraphView& v) {
    const std::vector<std::string> one{expression};
    draw(c, one, v, 0);
}

void GraphRenderer::draw(gfx::Canvas& c, const std::vector<std::string>& functions, const GraphView& v, std::size_t traced) {
    const auto old = c.clip();
    const gfx::Rect clip = plotClip(c);
    c.setClip(clip);
    const Geometry g = geometryFor(c);
    if (!(v.xmax > v.xmin) || !(v.ymax > v.ymin) || g.right <= g.left || g.bottom <= g.top) { c.setClip(old); return; }
    const std::size_t columns = static_cast<std::size_t>(g.right - g.left + 1);
    Cache& k = cache();

    const bool layerHit = k.layerValid && k.layer && k.layerFunctions == functions && k.layerView.xmin == v.xmin &&
                          k.layerView.xmax == v.xmax && k.layerView.ymin == v.ymin && k.layerView.ymax == v.ymax &&
                          k.layerWidth == c.width() && k.layerHeight == c.height();
    if (layerHit) {
        c.copyFrom(k.layerRect, k.layer, k.layerRect.w);
        ++k.stats.layerHits;
    } else {
        drawFrame(c, v, g);
        k.stats.directFallback = false;
        if (refreshSamples(k, functions, v, columns)) {
            for (std::size_t f = 0; f < functions.size(); ++f)
                drawCurve(c, v, g, k.samples.data() + f * columns * k.spp, columns, k.spp, colorFor(f));
        } else {
            // Not enough memory for the cache: evaluate column by column, no allocation.
            k.stats.directFallback = true;
            for (std::size_t f = 0; f < functions.size(); ++f) {
                bool previous = false;
                int px = 0, py = 0;
                for (int x = g.left; x <= g.right; ++x) {
                    const double xv = v.xmin + (v.xmax - v.xmin) * (x - g.left) / (g.right - g.left);
                    const float yv = functions[f].empty() ? std::numeric_limits<float>::quiet_NaN() : sampleAt(functions[f], xv);
                    if (!std::isfinite(yv) || yv < v.ymin || yv > v.ymax) { previous = false; continue; }
                    const int yy = g.bottom - static_cast<int>(std::lround((yv - v.ymin) * (g.bottom - g.top) / (v.ymax - v.ymin)));
                    if (previous && std::abs(yy - py) < (g.bottom - g.top)) c.line(px, py, x, yy, colorFor(f));
                    px = x; py = yy; previous = true;
                }
            }
        }
        // Keep the finished plot as a PSRAM layer (Pico Plus 2 W profile).
        dropLayer(k);
        const std::size_t bytes = static_cast<std::size_t>(clip.w) * static_cast<std::size_t>(clip.h) * sizeof(std::uint16_t);
        if (limits::kProfile.graphLayerCache && bytes && limits::fitsHeavy(bytes * 2)) {
            k.layer = static_cast<std::uint16_t*>(mem::tryAlloc(mem::Policy::PSRAM_ONLY, bytes, 32));
            if (k.layer) {
                try {
                    k.layerFunctions = functions;
                    c.copyTo(clip, k.layer, clip.w);
                    k.layerBytes = bytes;
                    k.layerRect = clip;
                    k.layerView = v;
                    k.layerWidth = c.width();
                    k.layerHeight = c.height();
                    k.layerValid = true;
                } catch (const std::bad_alloc&) {
                    dropLayer(k);
                }
            }
        }
        k.stats.layerBytes = k.layerBytes;
    }
    if (v.trace && !functions.empty()) {
        const std::size_t t = traced < functions.size() ? traced : 0;
        drawTrace(c, functions[t], v, g, colorFor(t));
    }
    c.resetClip();
    c.setClip(old);
}

void GraphRenderer::zoom(GraphView& v,double f) noexcept {if(!(f>0.05&&f<20))return;double cx=(v.xmin+v.xmax)*0.5,cy=(v.ymin+v.ymax)*0.5;double hw=(v.xmax-v.xmin)*0.5*f,hh=(v.ymax-v.ymin)*0.5*f;v.xmin=cx-hw;v.xmax=cx+hw;v.ymin=cy-hh;v.ymax=cy+hh;}
void GraphRenderer::pan(GraphView& v,double dx,double dy) noexcept {v.xmin+=dx;v.xmax+=dx;v.ymin+=dy;v.ymax+=dy;}

GraphRenderer::CacheStats GraphRenderer::cacheStats() noexcept { return cache().stats; }

void GraphRenderer::clearCache() noexcept {
    Cache& k = cache();
    dropLayer(k);
    k.samples.clear();
    k.samples.shrink_to_fit();
    k.functions.clear();
    k.functions.shrink_to_fit();
    k.layerFunctions.clear();
    k.samplesValid = false;
    k.stats = {};
}
}
