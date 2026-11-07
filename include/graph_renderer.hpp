#pragma once
#include "graphics_engine.hpp"
#include <string>
namespace fx {
struct GraphView { double xmin{-5},xmax{5},ymin{-5},ymax{5}; int cursorX{160}; bool trace{false}; };
class GraphRenderer {
public:
    static void draw(gfx::Canvas& canvas,const std::string& expression,const GraphView& view);
    static void zoom(GraphView& view,double factor) noexcept;
    static void pan(GraphView& view,double dx,double dy) noexcept;
};
}
