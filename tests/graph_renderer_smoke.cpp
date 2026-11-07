#include "graph_renderer.hpp"
#include <array>
#include <cassert>
int main(){std::array<unsigned short,320*320> fb{};gfx::Canvas c(fb.data(),320,320);fx::GraphView v;fx::GraphRenderer::draw(c,"x^2",v);bool changed=false;for(auto p:fb)if(p)changed=true;assert(changed);double old=v.xmax;fx::GraphRenderer::zoom(v,0.5);assert(v.xmax-v.xmin<10);fx::GraphRenderer::pan(v,1,2);assert(v.xmin>-5);(void)old;}
