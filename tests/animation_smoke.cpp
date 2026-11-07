#include "animation_system.hpp"
#include <cassert>
int main(){ui::Animation a;a.start(100,100);assert(a.active(100));assert(a.alpha(150)>=127);assert(a.active(150));assert(a.alpha(200)==255);assert(!a.active(200));a.start(10,0);assert(!a.active(10));}
