#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace fx {
struct Point2 { int16_t x,y; };
std::vector<Point2> sample2D(const std::string& expr,double xmin,double xmax,double ymin,double ymax,int width,int height);
struct Point3 { int16_t x,y; };
std::vector<Point3> sample3DSurface(const std::string& expr,double xmin,double xmax,double ymin,double ymax,double zmin,double zmax,int width,int height,int grid=18);
}
