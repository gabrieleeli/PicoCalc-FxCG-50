#pragma once
#include "natural_editor.hpp"
#include <vector>
namespace fx::math {
struct Size { int width{}, height{}, baseline{}; };
enum class Align { Left, Center, Right };
struct Layout {
    std::vector<NaturalDrawOp> ops;
    Size size;
    int x{}, top{}, width{}, height{};
    bool clipped{};
};
Size measure(const NaturalNode& node) noexcept;
// baseline is the text baseline. maxWidth/maxHeight define a clipping viewport.
Layout layout(const NaturalNode& node,int x,int baseline,int maxWidth,int maxHeight=220,Align align=Align::Left);
}
