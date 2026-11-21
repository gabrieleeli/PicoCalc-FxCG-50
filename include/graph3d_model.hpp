#pragma once
// 3D Graph model (v1.14.0, fx-CG50 software manual chapter 16): the three
// graph functions of the list (templates Line, Plane, Sphere, Cylinder, Cone
// and the directly input Z=, Param, Rot X, Rot Y), their colours, the 3D View
// Window, the 3D Graph Setup items, the validation of the manual ("Invalid
// Setting", "Out of Domain") and the analytic geometry of the templates:
// canonical shapes, line/plane intersections and relationships (G-Solve
// INTSECT and RELATION). Pure code: no graphics, no UI, no parser; the
// meshes and the drawing are in graph3d.hpp. Every value is a double in user
// units; angles of the V-Window are in degrees whatever the angle unit.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace fx::g3d {

constexpr std::size_t Functions = 3;            // up to three graphs at the same time (16-1)
constexpr std::size_t Coefficients = 9;         // largest template input (plane VECTOR / POINTS)
constexpr std::size_t GraphMemories = 20;       // 3D G-Mem 1..20 (16-7)
constexpr std::size_t WindowMemories = 6;       // 3D V-Win 1..6 (16-5)
constexpr int MinGrid = 2, MaxGrid = 50;        // 2 <= grid <= 50 (16-4)
constexpr double MaxCoordinate = 1e98;          // |min|, |max| < 1 x 10^98 (16-4)
constexpr std::size_t MaxExpression = 1000;     // bytes of one stored expression (as GRAPH)

// ---------------------------------------------------------------- functions
enum class Kind : std::uint8_t { None, Z, Param, RotX, RotY, Line, Plane, Sphere, Cylinder, Cone };
constexpr std::uint8_t KindCount = 10;
bool isTemplate(Kind k) noexcept;               // Line, Plane, Sphere, Cylinder, Cone
const char* kindName(Kind k) noexcept;          // "Z=", "Param", "Rot X", "Rot Y", "Line", ...

// Coefficient input screens of the templates (16-9...16-12). Layout of
// Function::coef for each form:
//   Line   EXPRESS (X-X0)/a=(Y-Y0)/b=(Z-Z0)/c   X0 Y0 Z0 a b c
//          VECTOR  r = r0 + t v                  r0x r0y r0z vx vy vz
//          P&V     point and direction vector    X0 Y0 Z0 a b c
//          POINTS  P1, P2                        x1 y1 z1 x2 y2 z2
//   Plane  EXPRESS aX+bY+cZ+d=0                  a b c d
//          VECTOR  r = r0 + s u + t v            r0 (3) u (3) v (3)
//          POINTS  P1, P2, P3                    9 coordinates
//   Sphere FACTOR  (X-a)^2+(Y-b)^2+(Z-c)^2=r^2   a b c r
//          EXPAND  X^2+Y^2+Z^2+aX+bY+cZ+d=0      a b c d
//   Cylinder / Cone: Radius, Zmin, Zmax, X, Y (centre of the base)
enum class LineForm : std::uint8_t { Express, Vector, PointVector, Points };
enum class PlaneForm : std::uint8_t { Express, Vector, Points };
enum class SphereForm : std::uint8_t { Factor, Expand };
std::uint8_t formCount(Kind k) noexcept;                        // Line 4, Plane 3, Sphere 2, others 1
const char* formName(Kind k, std::uint8_t form) noexcept;       // "EXPRESS", "VECTOR", "P&V", "POINTS", "FACTOR", "EXPAND"
std::size_t coefficientCount(Kind k, std::uint8_t form) noexcept;

// FORMAT colours (16-7). "Auto" is greyed out on the calculator and not offered.
enum class Color : std::uint8_t { Black, Blue, Red, Magenta, Green, Cyan, Yellow, White, Clear };
constexpr std::uint8_t ColorCount = 9;
const char* colorName(Color c) noexcept;
std::uint32_t colorRgb(Color c) noexcept;          // 0xRRGGBB (Clear: white)
Color defaultAreaColor(std::size_t row) noexcept;  // Blue, Red, Green for rows 1, 2, 3

struct Function {
    Kind kind{Kind::None};
    bool selected{true};                             // draw / don't draw (SELECT)
    std::uint8_t form{0};                            // LineForm / PlaneForm / SphereForm
    std::array<std::string, 3> expr{};               // Z: [0]; Param: Xst Yst Zst; Rot X/Y: Y=f(X) in [0]
    std::array<double, Coefficients> coef{};
    Color line{Color::Black}, area{Color::Blue};
    bool defined() const noexcept;                   // something to draw
};
Function emptyFunction(std::size_t row);

// ---------------------------------------------------------------- 3D View Window (16-4)
struct Window {
    double xmin{-3}, xmax{3};
    int xgrid{25};
    double ymin{-3}, ymax{3};
    int ygrid{25};
    double zmin{-3}, zmax{3};
    double smin{-3}, smax{3};                        // (scelta) S and T as X and Y
    int sgrid{25};
    double tmin{-3}, tmax{3};
    int tgrid{25};
    double theta{20}, phi{70};                       // (scelta) degrees, initial values of the ClassPad
    bool operator==(const Window&) const = default;
};
// "" when valid; "Out of Domain: ..." (value outside the input range) or
// "Invalid Setting: ..." (min >= max) otherwise (Appendix α-12).
std::string validate(const Window& w);
double normalizeTheta(double degrees) noexcept;      // (-180, 180]
double normalizePhi(double degrees) noexcept;        // [0, 360)

// 3D Graph Setup items (16-1, 16-2, 16-15).
struct Setup {
    bool axes{true}, box{true}, label{true};
    Color plot{Color::Blue};                         // Plot/LineCol of SKETCH Plot and Text
    bool operator==(const Setup&) const = default;
};

// Everything one 3D G-Mem stores (16-7): functions with selection and colours,
// one 3D View Window, the Setup. The current state of the mode is one too.
struct GraphSet {
    std::array<Function, Functions> fns{emptyFunction(0), emptyFunction(1), emptyFunction(2)};
    Window window{};
    Setup setup{};
};
// Compact binary form (3D G-Mem and record v10). decode() validates every
// field (kinds, forms, colours, lengths, templates, window) and leaves `out`
// unchanged on failure.
std::string encode(const GraphSet& set);
bool decode(const std::string& data, GraphSet& out);
std::string encodeWindow(const Window& w);
bool decodeWindow(const std::string& data, Window& out);

// ---------------------------------------------------------------- geometry
struct Vec3 {
    double x{}, y{}, z{};
};
inline Vec3 operator+(Vec3 a, Vec3 b) noexcept { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, double k) noexcept { return {a.x * k, a.y * k, a.z * k}; }
inline double dot(Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) noexcept { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double norm(Vec3 a) noexcept;

// Canonical form of a template, whatever form it was entered in.
struct Shape {
    Kind kind{Kind::None};
    Vec3 p{};                 // Line: point; Plane: point; Sphere: centre
    Vec3 d{};                 // Line: direction (not normalised)
    Vec3 u{}, v{};            // Plane: orthonormal in-plane axes
    Vec3 n{};                 // Plane: unit normal (n·r = h)
    double h{};               // Plane: offset
    double r{};               // Sphere, Cylinder, Cone: radius
    double cx{}, cy{};        // Cylinder, Cone: centre of the base
    double z0{}, z1{};        // Cylinder: z range (z0 < z1); Cone: base z0, apex z1
};
// "" and the shape, or "Invalid Setting: ..." with the conditions of 16-10...16-12.
std::string shapeOf(const Function& f, Shape& out);
// Validation of a function before SET (templates, colours, lengths).
std::string validate(const Function& f);
// Switches a template to another input form keeping the same object when it
// can be written in the new form, zeros otherwise (scelta).
void convertForm(Function& f, std::uint8_t form);

// G-Solve INTSECT (16-18): line-line point, line-plane point, plane-plane line.
struct Intersection {
    enum class Kind : std::uint8_t { Point, Line, NotFound, Infinite } kind{Kind::NotFound};
    Vec3 point{};             // Point; Line: a point of the line
    Vec3 direction{};         // Line: direction, largest component ±1
};
Intersection intersect(const Shape& a, const Shape& b);
// G-Solve RELATION (16-19).
enum class Relation : std::uint8_t { Intersect, Perpendicular, Parallel, Skew, SameLine, OnPlane, SamePlane };
Relation relation(const Shape& a, const Shape& b);
const char* relationName(Relation r) noexcept;
bool isLineOrPlane(const Function& f) noexcept;

} // namespace fx::g3d
