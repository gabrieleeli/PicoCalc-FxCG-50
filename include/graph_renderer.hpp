#pragma once
#include "graphics_engine.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace fx {
struct GraphView { double xmin{-5},xmax{5},ymin{-5},ymax{5}; int cursorX{160}; bool trace{false}; };

// Cartesian plotter. Several functions are drawn together; their samples are
// kept in a cache (PREFER_PSRAM pool) so that tracing/redrawing does not
// re-evaluate the expressions, and with PSRAM the finished plot is also kept
// as an off-screen layer. Oversampling (samples per pixel column) comes from
// fx::limits and is drawn as min/max vertical spans, so steep or oscillating
// functions are not aliased. Without memory for the cache the renderer falls
// back to direct evaluation (one sample per column, no allocation).
class GraphRenderer {
public:
    static void draw(gfx::Canvas& canvas,const std::string& expression,const GraphView& view);
    static void draw(gfx::Canvas& canvas,const std::vector<std::string>& functions,const GraphView& view,std::size_t traced);
    static void zoom(GraphView& view,double factor) noexcept;
    static void pan(GraphView& view,double dx,double dy) noexcept;
    static std::uint16_t colorFor(std::size_t index) noexcept;

    struct CacheStats {
        std::size_t functions{};
        std::size_t columns{};
        std::size_t samplesPerPixel{};
        std::size_t sampleBytes{};
        std::size_t layerBytes{};
        std::size_t sampleHits{}, sampleMisses{}, layerHits{};
        bool directFallback{};
    };
    static CacheStats cacheStats() noexcept;
    static void clearCache() noexcept;
};
}
