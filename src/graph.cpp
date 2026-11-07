#include "graph.hpp"
#include "expression.hpp"
#include <cmath>
#include <algorithm>
namespace fx {
std::vector<Point2> sample2D(const std::string&e,double xmin,double xmax,double ymin,double ymax,int w,int h){
 std::vector<Point2> out; if(w<2||h<2||xmax<=xmin||ymax<=ymin)return out;
 for(int px=0;px<w;++px){double x=xmin+(xmax-xmin)*px/(w-1);try{double y=evaluate(e,x);if(std::isfinite(y)&&y>=ymin&&y<=ymax){int py=static_cast<int>(std::lround((ymax-y)*(h-1)/(ymax-ymin)));out.push_back({static_cast<int16_t>(px),static_cast<int16_t>(std::clamp(py,0,h-1))});}}catch(...){}}
 return out;
}
std::vector<Point3> sample3DSurface(const std::string&e,double xmin,double xmax,double ymin,double ymax,double zmin,double zmax,int w,int h,int n){
 std::vector<Point3> out; if(n<2||w<2||h<2||xmax<=xmin||ymax<=ymin||zmax<=zmin)return out;
 // Isometric projection of actual z=f(x,y), centered and scaled to the requested viewport.
 for(int i=0;i<n;++i)for(int j=0;j<n;++j){double tx=i/double(n-1),ty=j/double(n-1);double x=xmin+(xmax-xmin)*tx,y=ymin+(ymax-ymin)*ty;try{double z=evaluate(e,x,y);if(!std::isfinite(z)||z<zmin||z>zmax)continue;double xn=2*tx-1,yn=2*ty-1,zn=2*(z-zmin)/(zmax-zmin)-1;double u=(xn-yn)*0.38;double v=(xn+yn)*0.20-zn*0.42;int px=static_cast<int>(std::lround(w*0.5+u*w));int py=static_cast<int>(std::lround(h*0.52+v*h));if(px>=0&&px<w&&py>=0&&py<h)out.push_back({static_cast<int16_t>(px),static_cast<int16_t>(py)});}catch(...){}}
 return out;
}
}
