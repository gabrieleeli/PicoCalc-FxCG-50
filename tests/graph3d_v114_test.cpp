// v1.14.0: completamento di 3D Graph (fx-CG50 manual chapter 16). One group
// of checks per item of the parity table (docs/reports/ANALISI_FONTI_v1.14.0.md):
// model and validation (T, V, E), analytic geometry (INTSECT, RELATION,
// CROSS), clipping and renderers (P), the mode through the keyboard (G, T,
// V, S, P, R, X, I), the record v10 and the migration of records v1-v9 with
// the fixtures. Runs on both board profiles (with and without PSRAM).
#include "app_rig.hpp"
#include "board_config.hpp"
#include "file_store.hpp"
#include "graph3d.hpp"
#include "graph3d_model.hpp"
#include "graph_commands.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"
#include "pict.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace fx;
using G3 = app::Graph3DApp;

namespace {
const std::uint8_t kV160Record[] = {
#include "fixtures/workspace_v1_6_0.inc"
};
const std::uint8_t kV180Record[] = {
#include "fixtures/workspace_v1_8_0.inc"
};
const std::uint8_t kV190Record[] = {
#include "fixtures/workspace_v1_9_0.inc"
};
const std::uint8_t kV1100Record[] = {
#include "fixtures/workspace_v1_10_0.inc"
};
const std::uint8_t kV1110Record[] = {
#include "fixtures/workspace_v1_11_0.inc"
};
const std::uint8_t kV1120Record[] = {
#include "fixtures/workspace_v1_12_0.inc"
};
// Record v9 written by the v1.13.1 sources with non-default 3D data.
const std::uint8_t kV1131Record[] = {
#include "fixtures/workspace_v1_13_1_graph3d.inc"
};

bool near(double a, double b, double tol = 1e-9) { return std::fabs(a - b) <= tol; }
bool near(g3d::Vec3 a, g3d::Vec3 b, double tol = 1e-9) { return near(a.x, b.x, tol) && near(a.y, b.y, tol) && near(a.z, b.z, tol); }

g3d::Function line(g3d::LineForm form, std::initializer_list<double> c) {
    g3d::Function f = g3d::emptyFunction(0);
    f.kind = g3d::Kind::Line; f.form = static_cast<std::uint8_t>(form);
    std::size_t i = 0;
    for (double v : c) f.coef[i++] = v;
    return f;
}
g3d::Function plane(g3d::PlaneForm form, std::initializer_list<double> c) {
    g3d::Function f = g3d::emptyFunction(1);
    f.kind = g3d::Kind::Plane; f.form = static_cast<std::uint8_t>(form);
    std::size_t i = 0;
    for (double v : c) f.coef[i++] = v;
    return f;
}
g3d::Function tmpl(g3d::Kind kind, std::uint8_t form, std::initializer_list<double> c) {
    g3d::Function f = g3d::emptyFunction(2);
    f.kind = kind; f.form = form;
    std::size_t i = 0;
    for (double v : c) f.coef[i++] = v;
    return f;
}
g3d::Shape shape(const g3d::Function& f) {
    g3d::Shape s;
    const std::string err = g3d::shapeOf(f, s);
    CHECK(err.empty());
    return s;
}
bool invalid(const g3d::Function& f) { return g3d::validate(f).rfind("Invalid Setting", 0) == 0; }
g3d::Function zf(const char* e, std::size_t row = 0) {
    g3d::Function f = g3d::emptyFunction(row);
    f.kind = g3d::Kind::Z; f.expr[0] = e;
    return f;
}

std::uint16_t fb[320 * 320];
std::size_t countNot(std::uint16_t color, gfx::Rect r) {
    std::size_t n = 0;
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) n += fb[y * 320 + x] != color;
    return n;
}
// Convex hull of the projected box (all drawn pixels must be inside).
bool insideHull(const std::vector<std::pair<double, double>>& h, double x, double y) {
    for (std::size_t i = 0; i < h.size(); ++i) {
        const auto& a = h[i];
        const auto& b = h[(i + 1) % h.size()];
        const double cr = (b.first - a.first) * (y - a.second) - (b.second - a.second) * (x - a.first);
        const double len = std::hypot(b.first - a.first, b.second - a.second);
        if (cr / len < -2.0) return false;                  // 2 px of rasterisation tolerance
    }
    return true;
}
std::vector<std::pair<double, double>> hullOf(std::vector<std::pair<double, double>> p) {
    std::sort(p.begin(), p.end());
    std::vector<std::pair<double, double>> h;
    auto turn = [](auto o, auto a, auto b) { return (a.first - o.first) * (b.second - o.second) - (a.second - o.second) * (b.first - o.first); };
    for (int pass = 0; pass < 2; ++pass) {
        const std::size_t start = h.size();
        for (const auto& q : p) {
            while (h.size() >= start + 2 && turn(h[h.size() - 2], h.back(), q) <= 0) h.pop_back();
            h.push_back(q);
        }
        h.pop_back();
        std::reverse(p.begin(), p.end());
    }
    return h;   // counter-clockwise in a y-down frame is clockwise on screen: the sign test above accepts either orientation via abs below
}
} // namespace

int main() {
    // ================================================================ V: 3D View Window (16-4, 16-5)
    {
        g3d::Window w;
        CHECK(w.xmin == -3 && w.xmax == 3 && w.xgrid == 25 && w.ygrid == 25 && w.zmin == -3 && w.zmax == 3);   // V6
        CHECK(w.theta == 20 && w.phi == 70 && w.sgrid == 25 && w.tgrid == 25);                                   // (scelta)
        CHECK(g3d::validate(w).empty());
        g3d::Window b = w; b.xgrid = 1; CHECK(g3d::validate(b).rfind("Out of Domain", 0) == 0);                   // V5: 2 <= grid <= 50
        b = w; b.ygrid = 51; CHECK(g3d::validate(b).rfind("Out of Domain", 0) == 0);
        b = w; b.tgrid = 2; CHECK(g3d::validate(b).empty());
        b = w; b.sgrid = 50; CHECK(g3d::validate(b).empty());
        b = w; b.xmax = 1e98; CHECK(g3d::validate(b).rfind("Out of Domain", 0) == 0);                              // |v| < 1E98
        b = w; b.zmin = -9.9e97; CHECK(g3d::validate(b).empty());
        b = w; b.theta = -180; CHECK(g3d::validate(b).rfind("Out of Domain", 0) == 0);                             // -180 < θ <= 180
        b = w; b.theta = 180; CHECK(g3d::validate(b).empty());
        b = w; b.phi = 360; CHECK(g3d::validate(b).rfind("Out of Domain", 0) == 0);                               // 0 <= φ < 360
        b = w; b.phi = 0; CHECK(g3d::validate(b).empty());
        b = w; b.xmin = 3; CHECK(g3d::validate(b).rfind("Invalid Setting", 0) == 0);                              // Xmin = Xmax
        b = w; b.tmin = 5; CHECK(g3d::validate(b).rfind("Invalid Setting", 0) == 0);
        CHECK(g3d::normalizeTheta(190) == -170 && g3d::normalizeTheta(-180) == 180 && g3d::normalizeTheta(540) == 180);
        CHECK(g3d::normalizePhi(-15) == 345 && g3d::normalizePhi(360) == 0 && g3d::normalizePhi(725) == 5);
        std::string enc = g3d::encodeWindow(w);
        g3d::Window back;
        CHECK(g3d::decodeWindow(enc, back) && back == w);
        enc[5] ^= 0x7F;
        g3d::Window keep = back;
        CHECK(!g3d::decodeWindow(enc.substr(0, enc.size() - 1), keep) && keep == back);
    }

    // ================================================================ T: templates and their checks (16-9...16-12)
    {
        using LF = g3d::LineForm;
        using PF = g3d::PlaneForm;
        // T3 EXPRESS (X-X0)/a=(Y-Y0)/b=(Z-Z0)/c, T7 a=0 or b=0 or c=0
        g3d::Shape s = shape(line(LF::Express, {1, 2, 3, 4, 5, 6}));
        CHECK(near(s.p, {1, 2, 3}) && near(s.d, {4, 5, 6}));
        CHECK(invalid(line(LF::Express, {0, 0, 0, 1, 0, 1})) && invalid(line(LF::Express, {0, 0, 0, 0, 1, 1})) && invalid(line(LF::Express, {0, 0, 0, 1, 1, 0})));
        // T4 VECTOR, T5 P&V: zero vector refused
        CHECK(near(shape(line(LF::Vector, {-1, -2, 1, 1, 1, 0})).d, {1, 1, 0}));
        CHECK(invalid(line(LF::Vector, {1, 2, 3, 0, 0, 0})) && invalid(line(LF::PointVector, {1, 2, 3, 0, 0, 0})));
        CHECK(!invalid(line(LF::PointVector, {1, 2, 3, 0, 0, 1})));
        // T6 POINTS: P1 = P2 refused
        CHECK(near(shape(line(LF::Points, {0, 0, 0, 1, 1, 1})).d, {1, 1, 1}));
        CHECK(invalid(line(LF::Points, {1, 1, 1, 1, 1, 1})));
        // T8 EXPRESS aX+bY+cZ+d=0, T11 a=b=c=0
        s = shape(plane(PF::Express, {1, 1, 1, -5}));
        CHECK(near(s.n, {1 / std::sqrt(3.0), 1 / std::sqrt(3.0), 1 / std::sqrt(3.0)}) && near(s.h, 5 / std::sqrt(3.0)));
        CHECK(invalid(plane(PF::Express, {0, 0, 0, 1})) && !invalid(plane(PF::Express, {0, 0, 2, 0})));
        CHECK(near(g3d::dot(s.u, s.n), 0) && near(g3d::dot(s.v, s.n), 0) && near(g3d::dot(s.u, s.v), 0) && near(g3d::norm(s.u), 1));
        // T9 VECTOR: u or v zero, same direction (also opposite) refused
        CHECK(!invalid(plane(PF::Vector, {1, 2, 0, 1, 1, 0, 0, 1, 1})));
        CHECK(invalid(plane(PF::Vector, {1, 2, 0, 0, 0, 0, 0, 1, 1})) && invalid(plane(PF::Vector, {1, 2, 0, 1, 1, 0, 0, 0, 0})));
        CHECK(invalid(plane(PF::Vector, {1, 2, 0, 1, 1, 0, 2, 2, 0})) && invalid(plane(PF::Vector, {1, 2, 0, 1, 1, 0, -3, -3, 0})));
        // T10 POINTS: two equal points, three aligned points refused
        CHECK(!invalid(plane(PF::Points, {0, 0, 0, 1, 0, 0, 0, 1, 0})));
        CHECK(invalid(plane(PF::Points, {0, 0, 0, 0, 0, 0, 0, 1, 0})) && invalid(plane(PF::Points, {0, 0, 0, 1, 1, 1, 2, 2, 2})));
        // T12-T14 Sphere FACTOR / EXPAND
        s = shape(tmpl(g3d::Kind::Sphere, 0, {1, -2, 3, 2}));
        CHECK(near(s.p, {1, -2, 3}) && s.r == 2);
        CHECK(invalid(tmpl(g3d::Kind::Sphere, 0, {0, 0, 0, 0})) && invalid(tmpl(g3d::Kind::Sphere, 0, {0, 0, 0, -1})));
        s = shape(tmpl(g3d::Kind::Sphere, 1, {-2, 4, -6, 10}));                       // (x-1)²+(y+2)²+(z-3)² = 4
        CHECK(near(s.p, {1, -2, 3}) && near(s.r, 2));
        CHECK(invalid(tmpl(g3d::Kind::Sphere, 1, {2, 0, 0, 1})) && invalid(tmpl(g3d::Kind::Sphere, 1, {0, 0, 0, 0})));   // a²+b²+c² > 4d
        // T15 Cylinder, T16 Cone
        s = shape(tmpl(g3d::Kind::Cylinder, 0, {1.5, 2, -1, 0.5, -0.5}));
        CHECK(s.r == 1.5 && s.z0 == -1 && s.z1 == 2 && s.cx == 0.5 && s.cy == -0.5);
        CHECK(invalid(tmpl(g3d::Kind::Cylinder, 0, {0, 0, 1, 0, 0})) && invalid(tmpl(g3d::Kind::Cylinder, 0, {1, 1, 1, 0, 0})));
        s = shape(tmpl(g3d::Kind::Cone, 0, {1, -2, 2, 0, 0}));
        CHECK(s.z0 == -2 && s.z1 == 2);                                               // base at Zmin, apex at Zmax
        CHECK(invalid(tmpl(g3d::Kind::Cone, 0, {-1, 0, 1, 0, 0})) && invalid(tmpl(g3d::Kind::Cone, 0, {1, 3, 3, 0, 0})));
        CHECK(g3d::formCount(g3d::Kind::Line) == 4 && g3d::formCount(g3d::Kind::Plane) == 3 && g3d::formCount(g3d::Kind::Sphere) == 2 &&
              g3d::formCount(g3d::Kind::Cylinder) == 1);
        CHECK(std::string(g3d::formName(g3d::Kind::Line, 2)) == "P&V" && std::string(g3d::formName(g3d::Kind::Sphere, 1)) == "EXPAND");
        // (scelta) conversion between the input forms keeps the object
        g3d::Function f = line(LF::Points, {0, 0, 0, 1, 1, 1});
        g3d::convertForm(f, static_cast<std::uint8_t>(LF::Vector));
        CHECK(f.form == 1 && near(shape(f).d, {1, 1, 1}) && f.coef[0] == 0);
        g3d::convertForm(f, static_cast<std::uint8_t>(LF::Express));
        CHECK(f.coef[3] == 1 && f.coef[5] == 1);
        f = line(LF::Points, {0, 0, 0, 1, 0, 0});                                     // direction with zeros: no EXPRESS form
        g3d::convertForm(f, static_cast<std::uint8_t>(LF::Express));
        CHECK(f.coef == (std::array<double, g3d::Coefficients>{}));
        f = plane(PF::Points, {1, 2, 0, 2, 3, 0, 1, 3, 1});
        g3d::convertForm(f, static_cast<std::uint8_t>(PF::Express));
        CHECK(f.coef[0] == 1 && f.coef[1] == -1 && f.coef[2] == 1 && f.coef[3] == 1);  // X-Y+Z+1=0
        g3d::convertForm(f, static_cast<std::uint8_t>(PF::Vector));
        CHECK(near(g3d::dot(g3d::Vec3{f.coef[3], f.coef[4], f.coef[5]}, {1, -1, 1}), 0));
        f = tmpl(g3d::Kind::Sphere, 0, {1, -2, 3, 2});
        g3d::convertForm(f, 1);
        CHECK(f.coef[0] == -2 && f.coef[1] == 4 && f.coef[2] == -6 && f.coef[3] == 10);
        g3d::convertForm(f, 0);
        CHECK(near(f.coef[3], 2) && f.coef[0] == 1);
        // G9: both colours Clear refused, Area Clear allowed
        f = zf("x");
        f.area = g3d::Color::Clear; CHECK(g3d::validate(f).empty());
        f.line = g3d::Color::Clear; CHECK(invalid(f));
        // T19 Param needs the three expressions
        g3d::Function p = g3d::emptyFunction(0);
        p.kind = g3d::Kind::Param; p.expr = {"s", "t", ""};
        CHECK(!p.defined() && g3d::validate(p).rfind("Syntax ERROR", 0) == 0);
        p.expr[2] = "s+t"; CHECK(p.defined() && g3d::validate(p).empty());
        // G12: the default colours of the rows
        for (std::size_t i = 0; i < 3; ++i) CHECK(g3d::emptyFunction(i).line == g3d::Color::Black);
        CHECK(g3d::emptyFunction(0).area == g3d::Color::Blue && g3d::emptyFunction(1).area == g3d::Color::Red && g3d::emptyFunction(2).area == g3d::Color::Green);
        // G7: binary form of a 3D G-Mem set, refused when damaged
        g3d::GraphSet set;
        set.fns[0] = zf("sin(x)cos(y)");
        set.fns[1] = tmpl(g3d::Kind::Cone, 0, {1, -2, 2, 0, 1});
        set.fns[1].area = g3d::Color::Cyan; set.fns[1].selected = false;
        set.fns[2] = plane(PF::Points, {0, 0, 0, 1, 0, 0, 0, 1, 0});
        set.window.theta = -45; set.window.xgrid = 7;
        set.setup.box = false; set.setup.plot = g3d::Color::Magenta;
        const std::string data = g3d::encode(set);
        g3d::GraphSet round;
        CHECK(g3d::decode(data, round) && round.fns[0].expr[0] == "sin(x)cos(y)" && round.fns[1].kind == g3d::Kind::Cone && !round.fns[1].selected);
        CHECK(round.fns[1].area == g3d::Color::Cyan && round.window == set.window && round.setup == set.setup && round.fns[2].coef == set.fns[2].coef);
        g3d::GraphSet untouched;
        CHECK(!g3d::decode(data.substr(0, data.size() - 3), untouched) && !g3d::decode(data + "x", untouched));
        std::string bad = data; bad[1] = 20;                                          // kind out of range
        CHECK(!g3d::decode(bad, untouched) && untouched.fns[0].kind == g3d::Kind::None);
        g3d::GraphSet invalidSet = set;
        invalidSet.fns[2].coef = {};                                                   // three equal points
        CHECK(!g3d::decode(g3d::encode(invalidSet), untouched));
    }

    // ================================================================ X5, X6: INTSECT and RELATION (16-18, 16-19)
    {
        using LF = g3d::LineForm;
        using PF = g3d::PlaneForm;
        using IK = g3d::Intersection::Kind;
        using R = g3d::Relation;
        // manual 16-18: P1=0,0,0 P2=1,1,1 and P1=1,1,0 P2=2,2,2 meet at (2,2,2)
        const g3d::Shape l1 = shape(line(LF::Points, {0, 0, 0, 1, 1, 1})), l2 = shape(line(LF::Points, {1, 1, 0, 2, 2, 2}));
        g3d::Intersection r = g3d::intersect(l1, l2);
        CHECK(r.kind == IK::Point && near(r.point, {2, 2, 2}));
        CHECK(g3d::relation(l1, l2) == R::Intersect);
        // parallel, same line, skew, right angle
        const g3d::Shape l3 = shape(line(LF::Vector, {0, 1, 0, 2, 2, 2})), l4 = shape(line(LF::Points, {3, 3, 3, -1, -1, -1}));
        CHECK(g3d::intersect(l1, l3).kind == IK::NotFound && g3d::relation(l1, l3) == R::Parallel);
        CHECK(g3d::intersect(l1, l4).kind == IK::Infinite && g3d::relation(l1, l4) == R::SameLine);
        const g3d::Shape sx = shape(line(LF::Vector, {0, 0, 0, 1, 0, 0})), sy = shape(line(LF::Vector, {0, 0, 1, 0, 1, 0}));
        CHECK(g3d::intersect(sx, sy).kind == IK::NotFound && g3d::relation(sx, sy) == R::Skew);
        const g3d::Shape py = shape(line(LF::Vector, {2, -1, 0, 0, 1, 0}));
        r = g3d::intersect(sx, py);
        CHECK(r.kind == IK::Point && near(r.point, {2, 0, 0}) && g3d::relation(sx, py) == R::Perpendicular);
        // manual 16-19: r=(-1,-2,1)+t(1,1,0) and r=(1,2,0)+s(1,1,0)+t(0,1,1) are PARALLEL
        const g3d::Shape ml = shape(line(LF::Vector, {-1, -2, 1, 1, 1, 0})), mp = shape(plane(PF::Vector, {1, 2, 0, 1, 1, 0, 0, 1, 1}));
        CHECK(g3d::relation(ml, mp) == R::Parallel && std::string(g3d::relationName(R::Parallel)) == "PARALLEL");
        CHECK(g3d::intersect(ml, mp).kind == IK::NotFound && g3d::intersect(mp, ml).kind == IK::NotFound);
        // line in the plane, line through the plane, perpendicular line
        const g3d::Shape z0 = shape(plane(PF::Express, {0, 0, 1, 0}));
        CHECK(g3d::intersect(sx, z0).kind == IK::Infinite && g3d::relation(sx, z0) == R::OnPlane);
        const g3d::Shape vz = shape(line(LF::PointVector, {1, 2, 5, 0, 0, -2}));
        r = g3d::intersect(vz, z0);
        CHECK(r.kind == IK::Point && near(r.point, {1, 2, 0}) && g3d::relation(z0, vz) == R::Perpendicular);
        const g3d::Shape slant = shape(line(LF::Express, {0, 0, 1, 1, 1, 1}));
        r = g3d::intersect(slant, z0);
        CHECK(r.kind == IK::Point && near(r.point, {-1, -1, 0}) && g3d::relation(slant, z0) == R::Intersect);
        // plane-plane: a line, parallel, same plane, perpendicular
        const g3d::Shape x0 = shape(plane(PF::Express, {1, 0, 0, -1}));               // X = 1
        r = g3d::intersect(z0, x0);
        CHECK(r.kind == IK::Line && near(g3d::norm(g3d::cross(r.direction, {0, 1, 0})), 0) && near(r.point.x, 1) && near(r.point.z, 0));
        CHECK(g3d::relation(z0, x0) == R::Perpendicular);
        const g3d::Shape z2 = shape(plane(PF::Points, {0, 0, 2, 1, 0, 2, 0, 1, 2}));
        CHECK(g3d::intersect(z0, z2).kind == IK::NotFound && g3d::relation(z0, z2) == R::Parallel);
        const g3d::Shape zz = shape(plane(PF::Express, {0, 0, -3, 0}));               // the same plane z = 0, opposite normal
        CHECK(g3d::intersect(z0, zz).kind == IK::Infinite && g3d::relation(z0, zz) == R::SamePlane);
        const g3d::Shape tilt = shape(plane(PF::Express, {1, 1, 1, -5}));
        r = g3d::intersect(tilt, z0);
        CHECK(r.kind == IK::Line && g3d::relation(tilt, z0) == R::Intersect);
        // every point of the line of intersection is on both planes
        for (double t : {-3.0, 0.0, 2.5}) {
            const g3d::Vec3 q = r.point + r.direction * t;
            CHECK(near(q.x + q.y + q.z, 5, 1e-9) && near(q.z, 0, 1e-9));
        }
        CHECK(g3d::isLineOrPlane(line(LF::Vector, {0, 0, 0, 1, 0, 0})) && !g3d::isLineOrPlane(zf("x")));
    }

    // ================================================================ P11 clipping, X2 cross sections, patches
    {
        g3d::Window w;
        w.xmin = -1; w.xmax = 1; w.ymin = -1; w.ymax = 1; w.zmin = -1; w.zmax = 1;
        double t0, t1;
        CHECK(g3d::clipLine(w, {0, 0, 0}, {1, 1, 1}, t0, t1) && near(t0, -1) && near(t1, 1));
        CHECK(!g3d::clipLine(w, {5, 0, 0}, {0, 1, 0}, t0, t1));
        CHECK(g3d::clipLine(w, {0.5, 0, 0}, {0, 0, 2}, t0, t1) && near(t0, -0.5) && near(t1, 0.5));
        // the plane patch covers its section of the box, nothing when it misses the box
        g3d::PatchSpec spec[g3d::MaxPatches];
        CHECK(g3d::templatePatches(shape(plane(g3d::PlaneForm::Express, {0, 0, 1, 0})), w, 25, 25, spec) == 1);
        CHECK(spec[0].domain.u1 - spec[0].domain.u0 >= 2 && spec[0].nu == 25);
        CHECK(g3d::templatePatches(shape(plane(g3d::PlaneForm::Express, {0, 0, 1, -5})), w, 25, 25, spec) == 0);
        const g3d::Shape cyl = shape(tmpl(g3d::Kind::Cylinder, 0, {0.5, -0.5, 0.5, 0, 0}));
        CHECK(g3d::templatePatches(cyl, w, 10, 10, spec) == 3 && spec[1].nv == 2);    // side and two bases
        const g3d::Shape cone = shape(tmpl(g3d::Kind::Cone, 0, {0.5, -0.5, 0.5, 0, 0}));
        CHECK(g3d::templatePatches(cone, w, 10, 10, spec) == 2);
        g3d::Vec3 q;
        CHECK(g3d::templatePoint(cone, 0, 0, 1, q) && near(q, {0, 0, 0.5}));          // apex at Zmax
        CHECK(g3d::templatePoint(cone, 0, 0, 0, q) && near(q, {0.5, 0, -0.5}));       // base at Zmin
        // cross sections: sphere at z = 0.6 (circle of radius 0.8), outside, tangent
        const g3d::Shape sph = shape(tmpl(g3d::Kind::Sphere, 0, {0, 0, 0, 1}));
        g3d::Polyline out[g3d::MaxSectionCurves];
        bool coincident = false;
        std::size_t n = g3d::crossSection(sph, w, g3d::Axis::Z, 0.6, out, coincident);
        CHECK(n == 1 && out[0].closed && !coincident);
        bool onCircle = true;
        for (const auto& pnt : out[0].pts) onCircle = onCircle && near(std::hypot(pnt.x, pnt.y), 0.8, 1e-9) && near(pnt.z, 0.6);
        CHECK(onCircle && out[0].pts.size() == 64);
        CHECK(g3d::crossSection(sph, w, g3d::Axis::X, 1.2, out, coincident) == 0);
        CHECK(g3d::crossSection(sph, w, g3d::Axis::Y, 1.0, out, coincident) == 1 && out[0].pts.size() == 1 && near(out[0].pts[0], {0, 1, 0}));
        // cylinder: a circle across the axis, a rectangle along it
        CHECK(g3d::crossSection(cyl, w, g3d::Axis::Z, 0, out, coincident) == 1 && near(std::hypot(out[0].pts[5].x, out[0].pts[5].y), 0.5));
        CHECK(g3d::crossSection(cyl, w, g3d::Axis::Z, 0.8, out, coincident) == 0);
        n = g3d::crossSection(cyl, w, g3d::Axis::X, 0.3, out, coincident);
        CHECK(n == 1 && out[0].pts.size() == 4 && near(out[0].pts[0].y, -0.4) && near(out[0].pts[1].y, 0.4) && near(out[0].pts[2].z, 0.5));
        // cone: circle radius shrinking towards the apex, conic section along the axis
        CHECK(g3d::crossSection(cone, w, g3d::Axis::Z, 0, out, coincident) == 1 && near(std::hypot(out[0].pts[0].x, out[0].pts[0].y), 0.25));
        n = g3d::crossSection(cone, w, g3d::Axis::Y, 0.1, out, coincident);
        CHECK(n == 1 && out[0].closed && out[0].pts.size() > 30);
        bool onCone = true;
        for (const auto& pnt : out[0].pts) {
            const double rho = 0.5 * (0.5 - pnt.z) / 1.0;
            onCone = onCone && near(pnt.y, 0.1) && near(std::hypot(pnt.x, pnt.y), rho, 1e-9);
        }
        CHECK(onCone);
        // line: one point, or the whole line when it lies in the section plane
        const g3d::Shape ln = shape(line(g3d::LineForm::Vector, {0, 0, 0, 1, 1, 1}));
        CHECK(g3d::crossSection(ln, w, g3d::Axis::X, 0.5, out, coincident) == 1 && near(out[0].pts[0], {0.5, 0.5, 0.5}));
        const g3d::Shape lz = shape(line(g3d::LineForm::Vector, {0, 0, 0.25, 1, 0, 0}));
        CHECK(g3d::crossSection(lz, w, g3d::Axis::Z, 0.25, out, coincident) == 1 && out[0].pts.size() == 2);
        // plane: a segment, or its outline when it is the section plane (16-17)
        const g3d::Shape pz = shape(plane(g3d::PlaneForm::Express, {0, 0, 1, -0.5}));
        CHECK(g3d::crossSection(pz, w, g3d::Axis::Z, 0.5, out, coincident) == 1 && coincident && out[0].pts.size() == 4);
        CHECK(g3d::crossSection(pz, w, g3d::Axis::Z, 0.2, out, coincident) == 0 && !coincident);
        CHECK(g3d::crossSection(pz, w, g3d::Axis::X, 0, out, coincident) == 1 && near(out[0].pts[0].z, 0.5) && near(out[0].pts[1].z, 0.5));
        // the section colour is the opposite of the Area Color
        CHECK(g3d::oppositeColor(g3d::Color::Blue) != g3d::color565(g3d::Color::Blue));
        CHECK(g3d::oppositeColor(g3d::Color::White) == gfx::rgb888To565(0, 0, 0));

        // P11: a sphere larger than the box is cut exactly: nothing is drawn outside the box
        gfx::Canvas c(fb, 320, 320);
        const gfx::Rect vp{0, 18, 320, 282};
        g3d::Mesh m[g3d::MaxPatches];
        const g3d::Shape big = shape(tmpl(g3d::Kind::Sphere, 0, {0, 0, 0, 1.4}));
        const std::size_t np = g3d::templatePatches(big, w, 25, 25, spec);
        g3d::Item it;
        for (std::size_t k = 0; k < np; ++k) {
            g3d::sample(m[k], [&](double u, double v, g3d::Vec3& o) { return g3d::templatePoint(big, k, u, v, o); }, spec[k].domain, spec[k].nu, spec[k].nv);
            it.patches[k] = &m[k];
        }
        it.patchCount = np; it.hasFill = true; it.fill = g3d::color565(g3d::Color::Blue); it.hasWire = true; it.wire = 0;
        const g3d::Setup bare{false, false, false, g3d::Color::Blue};
        for (bool zbuffer : {true, false}) {
            for (const g3d::Camera cam : {g3d::Camera{}, g3d::Camera{-125, 62, 1.0F}, g3d::Camera{90, 200, 1.4F}}) {
                c.clear(0xFFFF);
                const auto st = g3d::render(c, &it, 1, w, bare, cam, vp, zbuffer);
                CHECK(st.ok && st.triangles > 0);
                const g3d::Projector pr(w, cam, vp);
                std::vector<std::pair<double, double>> corners;
                for (int k = 0; k < 8; ++k) {
                    const g3d::ScreenPoint sp = pr.fromNormalized(k & 1 ? 1 : -1, k & 2 ? 1 : -1, k & 4 ? 1 : -1);
                    corners.emplace_back(sp.x, sp.y);
                }
                auto hull = hullOf(corners);
                // orientation of the hull: accept both by testing against the reversed one
                bool inside = true;
                std::vector<std::pair<double, double>> rev(hull.rbegin(), hull.rend());
                for (int y = vp.y; y < vp.y + vp.h && inside; ++y)
                    for (int x = vp.x; x < vp.x + vp.w; ++x)
                        if (fb[y * 320 + x] != 0xFFFF && !insideHull(hull, x + 0.5, y + 0.5) && !insideHull(rev, x + 0.5, y + 0.5)) { inside = false; break; }
                CHECK(inside);
            }
        }
        // a sphere completely outside the box draws nothing
        const g3d::Shape far = shape(tmpl(g3d::Kind::Sphere, 0, {5, 5, 5, 1}));
        g3d::Mesh fm;
        g3d::sample(fm, [&](double u, double v, g3d::Vec3& o) { return g3d::templatePoint(far, 0, u, v, o); }, {0, 6.283185307179586, -1.5707963267948966, 1.5707963267948966}, 25, 25);
        g3d::Item farItem = it;
        farItem.patches[0] = &fm; farItem.patchCount = 1;
        c.clear(0xFFFF);
        const auto none = g3d::render(c, &farItem, 1, w, bare, g3d::Camera{}, vp, true);
        CHECK(none.ok && none.triangles == 0 && countNot(0xFFFF, vp) == 0);

        // P10: z-buffer shared by surfaces and lines: the line through a sphere
        // is hidden by the sphere's front at the centre, visible outside it
        g3d::Mesh sm;
        g3d::sample(sm, [&](double u, double v, g3d::Vec3& o) { return g3d::templatePoint(sph, 0, u, v, o); }, {0, 6.283185307179586, -1.5707963267948966, 1.5707963267948966}, 25, 25);
        g3d::Window w2 = w; w2.xmin = w2.ymin = w2.zmin = -2; w2.xmax = w2.ymax = w2.zmax = 2;
        g3d::Item items[2];
        items[0].patches[0] = &sm; items[0].patchCount = 1; items[0].hasFill = true; items[0].fill = g3d::color565(g3d::Color::Blue);
        items[1].line = true; items[1].a = {0, -2, 0}; items[1].b = {0, 2, 0}; items[1].hasFill = true; items[1].fill = g3d::color565(g3d::Color::Red);
        const g3d::Camera viewX{0, 90, 1.0F};                                         // VIEW-X: the line runs across the screen
        for (bool zbuffer : {true, false}) {
            c.clear(0xFFFF);
            const auto st = g3d::render(c, items, 2, w2, bare, viewX, vp, zbuffer);
            CHECK(st.ok && st.lines > 0 && st.zbuffer == zbuffer);
            const g3d::Projector pr(w2, viewX, vp);
            const g3d::ScreenPoint centre = pr.project({0, 0, 0}), outside = pr.project({0, 1.6, 0});
            const std::uint16_t pc = fb[static_cast<int>(centre.y) * 320 + static_cast<int>(centre.x)];
            CHECK(pc != 0xFFFF && pc != g3d::color565(g3d::Color::Red));               // the sphere hides the line
            bool lineSeen = false;
            for (int dy = -2; dy <= 2; ++dy) lineSeen = lineSeen || fb[(static_cast<int>(outside.y) + dy) * 320 + static_cast<int>(outside.x)] == g3d::color565(g3d::Color::Red);
            CHECK(lineSeen);
        }
        // projection: VIEW-X shows y to the right and z up; VIEW-Z x right and y up
        const g3d::Projector px(w2, g3d::Camera{0, 90, 1.0F}, vp);
        CHECK(px.project({0, 1, 0}).x > px.project({0, 0, 0}).x && px.project({0, 0, 1}).y < px.project({0, 0, 0}).y);
        const g3d::Projector pz2(w2, g3d::Camera{-90, 0, 1.0F}, vp);
        CHECK(pz2.project({1, 0, 0}).x > pz2.project({0, 0, 0}).x && pz2.project({0, 1, 0}).y < pz2.project({0, 0, 0}).y);
        const g3d::Projector pdef(w2, g3d::Camera{}, vp);                             // θ 20, φ 70: x towards the viewer, to the left
        CHECK(pdef.project({1, 0, 0}).x < pdef.project({0, 0, 0}).x && pdef.project({1, 0, 0}).depth < pdef.project({0, 0, 0}).depth);
        CHECK(pdef.project({0, 1, 0}).x > pdef.project({0, 0, 0}).x && pdef.project({0, 0, 1}).y < pdef.project({0, 0, 0}).y);
        // zoom on the centre of the screen
        const g3d::Projector pzoom(w2, g3d::Camera{20, 70, 2.0F}, vp);
        CHECK(near(pzoom.project({0, 0, 0}).x, pdef.project({0, 0, 0}).x, 1e-3) &&
              std::fabs(pzoom.project({0, 1, 0}).x - pzoom.project({0, 0, 0}).x) > 1.9 * std::fabs(pdef.project({0, 1, 0}).x - pdef.project({0, 0, 0}).x));
        // P9: Area Clear draws the lattice only; Line Clear the surface only
        g3d::Function wf = zf("x");
        wf.area = g3d::Color::Clear; wf.line = g3d::Color::Red;
        g3d::Item wi;
        g3d::itemColors(wf, wi);
        CHECK(!wi.hasFill && wi.hasWire && wi.wire == g3d::color565(g3d::Color::Red));
        wf.area = g3d::Color::Green; wf.line = g3d::Color::Clear;
        g3d::itemColors(wf, wi);
        CHECK(wi.hasFill && !wi.hasWire && wi.fill == g3d::color565(g3d::Color::Green));
        g3d::Function lf = line(g3d::LineForm::Vector, {0, 0, 0, 1, 0, 0});
        lf.area = g3d::Color::Magenta;
        g3d::itemColors(lf, wi);
        CHECK(wi.fill == g3d::color565(g3d::Color::Magenta));                         // the Line uses the Area Color
        // fuzzing (v1.14.0): a tiny window with huge coordinates does not overflow
        // into NaN (shading, projection): rendered without undefined behaviour
        {
            g3d::Window tiny;
            tiny.xmin = 0; tiny.xmax = 1e-300; tiny.ymin = -1e97; tiny.ymax = 1e97; tiny.zmin = 0; tiny.zmax = 1e-300;
            CHECK(g3d::validate(tiny).empty());
            g3d::Mesh hm;
            g3d::sample(hm, [](double u, double v, g3d::Vec3& o) { o = {u * 1e29, v * 1e29, (u - v) * 1e29}; return true; }, {-1, 1, -1, 1}, 4, 4);
            g3d::Item hi;
            hi.patches[0] = &hm; hi.patchCount = 1; hi.hasFill = hi.hasWire = true; hi.fill = 0x001F; hi.wire = 0;
            for (bool zbuffer : {true, false}) {
                c.clear(0xFFFF);
                CHECK(g3d::render(c, &hi, 1, tiny, g3d::Setup{}, g3d::Camera{}, vp, zbuffer).ok);
            }
            double nx, ny, nz;
            g3d::Projector(tiny, g3d::Camera{}, vp).normalize({1e29, 0, -1e29}, nx, ny, nz);
            CHECK(std::isfinite(nx) && std::isfinite(ny) && std::isfinite(nz));
        }
        // a lattice that does not fit throws std::bad_alloc and stays empty
        // (bounded simulated SRAM without PSRAM, as on the Pico 2 W: the pool
        // refuses the request instead of the host malloc)
        mem::testing::simulateSram(256u * 1024u);
        mem::testing::simulatePsram(0);
        {
            g3d::Mesh huge;
            bool oom = false;
            try { g3d::beginSample(huge, 300, 300); } catch (const std::bad_alloc&) { oom = true; }   // 1.4 MB
            CHECK(oom && huge.verts.empty() && huge.nu == 0);
            g3d::Mesh fits;
            CHECK_NOTHROW(g3d::beginSample(fits, 25, 25));
            CHECK(fits.verts.size() == 625);
        }
        mem::testing::reset();
        CHECK(std::string(files::CaptureFolder) == graphcmd::CaptureFolder);
    }

    // ================================================================ I7: record v10 and migration of v1-v9
    {
        struct Fixture { const std::uint8_t* data; std::size_t size; int version; };
        const Fixture fixtures[] = {{kV160Record, sizeof kV160Record, 1}, {kV180Record, sizeof kV180Record, 4}, {kV190Record, sizeof kV190Record, 5},
                                    {kV1100Record, sizeof kV1100Record, 6}, {kV1110Record, sizeof kV1110Record, 7},
                                    {kV1120Record, sizeof kV1120Record, 8}, {kV1131Record, sizeof kV1131Record, 9}};
        for (const Fixture& fx : fixtures) {
            CHECK(fx.data[4] == fx.version);
            Workspace w;
            CHECK(deserializeWorkspace(fx.data, fx.size, w));
            if (fx.version < 5) CHECK(w.graph3d.set.fns[0].kind == g3d::Kind::None && w.graph3d.set.window == g3d::Window{});
            for (const auto& f : w.graph3d.set.fns) CHECK(f.kind == g3d::Kind::None || f.kind == g3d::Kind::Z);
            CHECK(g3d::validate(w.graph3d.set.window).empty());
            // saved again as v10 and read back unchanged
            const auto blob = serializeWorkspace(w);
            CHECK(blob[4] == 11 && blob[5] == 0);
            Workspace back;
            CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
            CHECK(g3d::encode(back.graph3d.set) == g3d::encode(w.graph3d.set) && back.graph3d.zoom == w.graph3d.zoom);
        }
        // the v1.9.0 / v1.10.0 records: Z2 = x*y
        Workspace w9;
        CHECK(deserializeWorkspace(kV190Record, sizeof kV190Record, w9));
        CHECK(w9.graph3d.set.fns[1].kind == g3d::Kind::Z && w9.graph3d.set.fns[1].expr[0] == "x*y");
        // the v1.13.1 record with 3D data that is not the default
        Workspace w;
        CHECK(deserializeWorkspace(kV1131Record, sizeof kV1131Record, w));
        const auto& s = w.graph3d.set;
        CHECK(w.settings.angle == AngleMode::Degrees && w.memory.recall('A') == 2.5);
        CHECK(s.fns[0].kind == g3d::Kind::Z && s.fns[0].expr[0] == "x^2-y^2" && s.fns[0].selected);
        CHECK(s.fns[1].kind == g3d::Kind::Z && s.fns[1].expr[0] == "sin(x)+cos(y)" && !s.fns[1].selected);
        CHECK(s.fns[2].kind == g3d::Kind::Z && s.fns[2].expr[0] == "A*x*y" && s.fns[2].selected);
        CHECK(s.fns[0].area == g3d::Color::Blue && s.fns[1].area == g3d::Color::Red && s.fns[2].area == g3d::Color::Green);
        CHECK(s.window.xmin == -4 && s.window.xmax == 5 && s.window.ymin == -2 && s.window.ymax == 6 && s.window.zmin == -10 && s.window.zmax == 8);
        CHECK(s.window.xgrid == 31 && s.window.ygrid == 31);                           // 30 cells -> 31 points
        CHECK(s.window.theta == -150 && s.window.phi == 50 && w.graph3d.zoom == 1.5F); // yaw 60, pitch 40 -> same orientation
        // the converted view keeps the orientation of the v1.13.1 projector (yaw/pitch)
        {
            const double yaw = 60 * M_PI / 180, pitch = 40 * M_PI / 180;
            const g3d::Window box = s.window;
            const g3d::Projector p(box, g3d::Camera{box.theta, box.phi, 1.0F}, {0, 0, 300, 300});
            for (const g3d::Vec3 v : {g3d::Vec3{1, 0, 0}, g3d::Vec3{0, 1, 0}}) {
                const double oldRight = v.x * std::cos(yaw) - v.y * std::sin(yaw);
                const double oldUp = (v.x * std::sin(yaw) + v.y * std::cos(yaw)) * std::sin(pitch);
                CHECK(near(g3d::dot(v, p.screenRight()), oldRight, 1e-9) && near(g3d::dot(v, p.screenUp()), oldUp, 1e-9));
            }
        }
        // corrupt v10 data is rejected like any other field
        auto rejects = [](Workspace bad) {
            const auto blob = serializeWorkspace(bad);
            Workspace sink;
            return !deserializeWorkspace(blob.data(), blob.size(), sink);
        };
        Workspace ok;
        ok.graph3d.gmem[3] = g3d::encode(ok.graph3d.set);
        ok.graph3d.vmem[5].theta = 90; ok.graph3d.vmemUsed = 32;
        {
            const auto blob = serializeWorkspace(ok);
            Workspace back;
            CHECK(deserializeWorkspace(blob.data(), blob.size(), back) && back.graph3d.gmem[3] == ok.graph3d.gmem[3] && back.graph3d.vmem[5].theta == 90 &&
                  back.graph3d.vmemUsed == 32);
        }
        Workspace bad = ok; bad.graph3d.gmem[0] = "garbage"; CHECK(rejects(bad));
        bad = ok; bad.graph3d.vmemUsed = 64; CHECK(rejects(bad));
        bad = ok; bad.graph3d.zoom = 0.0F; CHECK(rejects(bad));
        bad = ok; bad.graph3d.set.window.xgrid = 60; CHECK(rejects(bad));
        bad = ok; bad.graph3d.vmem[0].phi = 400; CHECK(rejects(bad));
        bad = ok; bad.graph3d.set.fns[1] = tmpl(g3d::Kind::Sphere, 0, {0, 0, 0, -1}); CHECK(rejects(bad));
        bad = ok; for (auto& m : bad.graph3d.gmem) m = g3d::encode(g3d::GraphSet{}) + std::string(limits::kGraph3DMemoryBytes / 10, '\0');
        CHECK(rejects(bad));
    }

    // ================================================================ the mode through the keyboard, both profiles
    board::init();
    files::RamFileSystem card(4u * 1024u * 1024u);
    board::host::setFileSystem(&card);
    for (const bool psram : {false, true}) {
        mem::testing::simulatePsram(psram ? (8u << 20) : 0u);
        storage::RecordStore(board::storageFlash()).eraseAll();
        testrig::Rig r;
        Workspace& ws = r.ws();
        auto& set = ws.graph3d.set;
        r.type("j");
        G3* g = r.active<G3>();
        CHECK(g && g->screen() == G3::Screen::List);
        if (!g) continue;

        // ---- G12: empty list; T1/T2: selection screen and the sphere of example 1 (16-2)
        CHECK(set.fns[0].kind == g3d::Kind::None && set.fns[1].kind == g3d::Kind::None);
        r.key(pckey::F3);                                                  // TYPE
        CHECK(g->screen() == G3::Screen::Select);
        r.key(pckey::Down); r.key(pckey::Down);                            // Sphere
        r.key(pckey::Enter);
        CHECK(g->screen() == G3::Screen::Coef && g->editing().kind == g3d::Kind::Sphere && g->editing().form == 0);
        r.enter("0"); r.enter("0"); r.enter("0"); r.enter("2");            // (x-0)²+(y-0)²+(z-0)² = 2²
        r.key(pckey::F6);                                                  // SET
        CHECK(g->screen() == G3::Screen::List && set.fns[0].kind == g3d::Kind::Sphere && set.fns[0].coef[3] == 2 && set.fns[0].selected);
        // T17: expressions in the cells (A-Z, fractions), EDIT, EXPAND
        ws.memory.store('A', 3);
        r.key(pckey::Left);                                                // registered function: its input screen (16-8)
        CHECK(g->screen() == G3::Screen::Coef && g->editing().coef[3] == 2);
        r.enter("1/2");
        CHECK(g->editing().coef[0] == 0.5 && g->coefCell() == 1);
        r.type("A"); r.key(pckey::Enter);
        CHECK(g->editing().coef[1] == 3);
        r.key(pckey::F3);                                                  // EDIT the third cell
        r.key(pckey::Break); r.type("-1"); r.key(pckey::Enter);
        CHECK(g->editing().coef[2] == -1);
        r.type("1+*"); r.key(pckey::Enter);                                // Syntax ERROR keeps the cell editor open
        CHECK(r.error() && r.errorTitle() == "Syntax ERROR");
        r.closeError(); r.key(pckey::Esc);
        r.key(pckey::F2);                                                  // EXPAND: the same sphere
        CHECK(g->editing().form == 1 && near(g->editing().coef[0], -1) && near(g->editing().coef[1], -6));
        r.key(pckey::F6);
        CHECK(set.fns[0].form == 1 && ws.memory.recall('A') == 3);
        // T14 via the keyboard: r = 0 refused, the screen stays open
        r.key(pckey::Left);
        r.key(pckey::F1);                                                  // FACTOR
        r.key(pckey::Right); r.key(pckey::Right); r.key(pckey::Right);
        r.enter("0");
        r.key(pckey::F6);
        CHECK(r.error() && r.errorTitle() == "Invalid Setting" && set.fns[0].form == 1);
        r.closeError();
        CHECK(g->screen() == G3::Screen::Coef);
        r.key(pckey::Esc);                                                 // EXIT: nothing stored
        CHECK(g->screen() == G3::Screen::List && set.fns[0].form == 1);

        // ---- T6/X5: the two lines of 16-18 on rows 2 and 3, then INTSECT
        r.key(pckey::Down); r.key(pckey::F3);
        CHECK(g->screen() == G3::Screen::Select);
        r.key(pckey::Enter);                                               // Line
        r.key(pckey::F4);                                                  // POINTS
        for (const char* v : {"0", "0", "0", "1", "1", "1"}) r.enter(v);
        r.key(pckey::F6);
        r.key(pckey::Down); r.key(pckey::F3); r.key(pckey::Enter); r.key(pckey::F4);
        for (const char* v : {"1", "1", "0", "2", "2", "2"}) r.enter(v);
        r.key(pckey::F6);
        CHECK(set.fns[1].kind == g3d::Kind::Line && set.fns[2].kind == g3d::Kind::Line && set.fns[2].coef[5] == 2);
        // T7: P1 = P2 refused
        r.key(pckey::Right);                                              // the input screen of row 3
        CHECK(g->screen() == G3::Screen::Coef);
        r.key(pckey::Down);                                               // P2
        CHECK(g->coefCell() == 3);
        for (const char* v : {"1", "1", "0"}) r.enter(v);
        r.key(pckey::F6);
        CHECK(r.error() && r.errorTitle() == "Invalid Setting");
        r.closeError(); r.key(pckey::Esc);
        // G8: EXE draws
        r.key(pckey::Enter);
        CHECK(g->screen() == G3::Screen::Plot);
        r.settle();
        CHECK(g->drawn() && !g->sampling() && g->zbuffer() == psram);
        CHECK(g->mesh(0).valid == g->mesh(0).verts.size() && g->mesh(1).verts.empty());   // lines have no lattice
        CHECK(g->points() == (psram ? 50u : 25u) || g->points() <= 25u);
        r.key(pckey::F5); CHECK(g->sub() == G3::Sub::GSolve);
        r.key(pckey::F2);                                                  // INTSECT
        CHECK(g->mode() == G3::Mode::Intsect && g->solvePair() == std::make_pair(1, 2));
        r.key(pckey::F1);                                                  // Next: only one pair of lines
        CHECK(g->solvePair() == std::make_pair(1, 2));
        r.key(pckey::Esc);
        CHECK(g->mode() == G3::Mode::View);
        // X6 RELATION
        r.key(pckey::F5); r.key(pckey::F3);
        CHECK(g->mode() == G3::Mode::Relation);
        r.key(pckey::Esc);
        // X7: INTSECT with one line only
        set.fns[2].selected = false;
        r.key(pckey::F6);                                                  // G<->T
        r.key(pckey::F6);                                                  // DRAW again
        r.settle();
        r.key(pckey::F5); r.key(pckey::F2);
        CHECK(r.error() && r.errorTitle() == "INTSECT requires");
        r.closeError();
        r.key(pckey::F5); r.key(pckey::F3);
        CHECK(r.error() && r.errorTitle() == "RELATION requires");
        r.closeError();
        set.fns[2].selected = true;
        // three lines / planes: Next and Back go through the combinations
        set.fns[0] = plane(g3d::PlaneForm::Express, {0, 0, 1, 0});
        r.key(pckey::Esc);
        r.key(pckey::F6); r.settle();
        r.key(pckey::F5); r.key(pckey::F2);
        CHECK(g->solvePair() == std::make_pair(0, 1));
        r.key(pckey::F1); CHECK(g->solvePair() == std::make_pair(0, 2));
        r.key(pckey::F1); CHECK(g->solvePair() == std::make_pair(1, 2));
        r.key(pckey::F1); CHECK(g->solvePair() == std::make_pair(0, 1));
        r.key(pckey::F2); CHECK(g->solvePair() == std::make_pair(1, 2));
        r.key(pckey::Esc);

        // ---- X1-X4: CROSS
        r.key(pckey::F5); r.key(pckey::F1);
        CHECK(g->sub() == G3::Sub::CrossAxis);
        r.key(pckey::F3);                                                  // Z
        CHECK(g->mode() == G3::Mode::Cross && g->crossAxis() == g3d::Axis::Z && g->crossValue() == 0);
        r.key(pckey::F1);                                                  // UP: one step (Xgrid)
        CHECK(near(g->crossValue(), 6.0 / 24));
        r.key(pckey::F2); r.key(pckey::F2);
        CHECK(near(g->crossValue(), -6.0 / 24));
        r.type("1"); r.key(pckey::Enter);                                  // value typed
        CHECK(g->crossValue() == 1);
        r.type("9"); r.key(pckey::Enter);                                  // clamped to Zmax
        CHECK(g->crossValue() == 3);
        r.key(pckey::F4);                                                  // VIEW-Y
        CHECK(set.window.theta == 90 && set.window.phi == 90);
        r.key(pckey::F6);                                                  // ORIGINAL
        CHECK(set.window.theta == 20 && set.window.phi == 70);
        r.key(pckey::Esc);
        CHECK(g->mode() == G3::Mode::View);

        // ---- P2: rotation with the cursor keys follows the V-Window angles
        r.key(pckey::Right); CHECK(set.window.theta == 35);
        r.key(pckey::Left); r.key(pckey::Left); CHECK(set.window.theta == 5);
        r.key(pckey::Up); CHECK(set.window.phi == 85);
        for (int k = 0; k < 20; ++k) r.key(pckey::Down);
        CHECK(set.window.phi == 145);                                      // 85 - 300 = -215 -> 145: a full turn
        CHECK(g->mesh(0).valid > 0 && !g->sampling());                     // no new computation
        // P5-P7: ZOOM
        r.key(pckey::F2); CHECK(g->sub() == G3::Sub::Zoom);
        r.key(pckey::F1); CHECK(ws.graph3d.zoom == 1.5F);
        r.key(pckey::F2); r.key(pckey::F2); CHECK(near(ws.graph3d.zoom, 1.0, 1e-6));                  // OUT
        r.key(pckey::F2); r.key(pckey::F2); CHECK(near(ws.graph3d.zoom, 1.0 / 1.5, 1e-6));
        r.key(pckey::F2); r.key(pckey::F3); CHECK(set.window.theta == 0 && set.window.phi == 90);       // VIEW-X
        r.key(pckey::F2); r.key(pckey::F5); CHECK(set.window.theta == -90 && set.window.phi == 0);      // VIEW-Z
        r.key(pckey::F2); r.key(pckey::F6); CHECK(set.window.theta == 20 && set.window.phi == 70 && ws.graph3d.zoom == 1.0F);
        // S5, S6: '.' cycles Axes/Box, ',' Label
        r.type(".");  CHECK(set.setup.axes && !set.setup.box);
        r.type(".");  CHECK(!set.setup.axes && set.setup.box);
        r.type(".");  CHECK(!set.setup.axes && !set.setup.box);
        r.type(".");  CHECK(set.setup.axes && set.setup.box);
        r.type(",");  CHECK(!set.setup.label);
        r.type(",");  CHECK(set.setup.label);
        // image cache of the scene (PSRAM): TRACE does not draw the scene again
        r.settle();
        CHECK(g->layerCached() == psram);

        // ---- P8: SKETCH (Cls, Plot, Text)
        r.key(pckey::F4); CHECK(g->sub() == G3::Sub::Sketch);
        r.key(pckey::F2); CHECK(g->mode() == G3::Mode::SketchPlot);
        r.key(pckey::Right); r.key(pckey::Enter);
        r.key(pckey::Up); r.key(pckey::Enter);
        CHECK(g->sketchMarks() == 2);
        r.key(pckey::Esc);
        r.key(pckey::F4); r.key(pckey::F3);
        r.type("Hi"); r.key(pckey::Enter);
        CHECK(g->sketchMarks() == 3);
        r.key(pckey::Esc);
        r.key(pckey::F4); r.key(pckey::F1);                                // Cls
        CHECK(g->sketchMarks() == 0);
        r.key(pckey::F4); r.key(pckey::F2); r.key(pckey::Enter); r.key(pckey::Esc);
        r.key(pckey::Right);                                               // (scelta) a new view clears the sketch
        CHECK(g->sketchMarks() == 0);
        r.key(pckey::Left);

        // ---- P4: PICTURE STORE (OPTN) and I8 CAPTURE (SHIFT 7)
        r.key(pckey::F8); CHECK(g->sub() == G3::Sub::Optn);
        r.key(pckey::F1); r.key(pckey::F1);
        r.type("3"); r.key(pckey::Enter);
        r.settle();
        files::Entry e;
        CHECK(card.stat(files::join(files::PictFolder, pict::memoryFileName(3)), e) == files::Status::Ok && e.size > 0);
        r.key(pckey::F7); r.type("7");
        r.type("5"); r.key(pckey::Enter);
        CHECK(card.stat(files::join(files::CaptureFolder, "CAPT05.pcpict"), e) == files::Status::Ok);

        // ---- P3: Auto Rotate stops after two turns; AC stops it earlier
        r.key(pckey::F8); r.key(pckey::F2); r.key(pckey::F1);              // OPTN > ROTATE > L->R
        CHECK(g->autoRotating());
        for (int k = 0; k < 200 && g->autoRotating(); ++k) { board::host::advance(50); r.settle(); }
        CHECK(!g->autoRotating() && set.window.theta == 20);               // 72 x 10° = two turns
        r.key(pckey::F8); r.key(pckey::F2); r.key(pckey::F4);              // B->T
        board::host::advance(50); r.settle(); board::host::advance(50); r.settle();
        CHECK(g->autoRotating() && set.window.phi != 70);
        r.key(pckey::Break);                                               // AC
        CHECK(!g->autoRotating() && g->screen() == G3::Screen::Plot);
        r.key(pckey::F2); r.key(pckey::F6);

        // ---- R1-R5: TRACE on Z = X×Y (16-15)
        r.key(pckey::F6);                                                  // list
        set.fns[0] = zf("x*y");
        r.key(pckey::Up); r.key(pckey::Up);
        r.key(pckey::F6); r.settle();
        ws.memory.store('X', 7); ws.memory.store('Y', -4);
        r.key(pckey::F7); r.key(pckey::F1);                                // SHIFT F1 TRACE
        CHECK(g->mode() == G3::Mode::Trace && g->traceGraph() == 0);
        g3d::Vec3 p;
        CHECK(g->tracePoint(r.fw.apps().context(), p) && near(p, {0, 0, 0}));
        r.key(pckey::Right);
        CHECK(g->tracePoint(r.fw.apps().context(), p) && near(p, {0.25, 0, 0}));
        r.key(pckey::Up); r.key(pckey::Up);
        CHECK(g->tracePoint(r.fw.apps().context(), p) && near(p, {0.25, 0.5, 0.125}));
        r.type("x"); r.type("1"); r.key(pckey::Enter); r.type("-2"); r.key(pckey::Enter);   // X,θ,T: coordinates
        CHECK(g->tracePoint(r.fw.apps().context(), p) && near(p, {1, -2, -2}));
        r.type("2"); r.key(pckey::Enter); r.type("1.5"); r.key(pckey::Enter);              // a value typed directly
        CHECK(g->tracePoint(r.fw.apps().context(), p) && near(p, {2, 1.5, 3}));
        CHECK(ws.memory.recall('X') == 7 && ws.memory.recall('Y') == -4);   // I1: A-Z untouched
        r.key(pckey::F1);                                                  // Next: the lines
        CHECK(g->traceGraph() == 1 && g->tracePoint(r.fw.apps().context(), p));
        r.key(pckey::F2);                                                  // Back
        CHECK(g->traceGraph() == 0);
        r.key(pckey::F7); r.key(pckey::F1);                                // SHIFT F1 ends TRACE
        CHECK(g->mode() == G3::Mode::View);
        r.key(pckey::F1); r.key(pckey::Esc);
        CHECK(g->mode() == G3::Mode::View);

        // ---- G10, G11: G<->T keeps the lattices; AC back to the list
        r.key(pckey::Break);
        CHECK(g->screen() == G3::Screen::List && g->drawn());
        r.key(pckey::F7); r.key(pckey::F6);
        CHECK(g->screen() == G3::Screen::Plot && !g->sampling() && g->drawn());

        // ---- V1-V8: V-Window from the graph, Out of Domain, INITIAL, 3D-VMEM
        r.key(pckey::F3);
        CHECK(g->screen() == G3::Screen::Window);
        r.key(pckey::Down); r.key(pckey::Down);
        r.enter("51");
        r.key(pckey::Esc);
        CHECK(r.error() && r.errorTitle() == "Out of Domain");
        r.closeError();
        r.key(pckey::Up); r.enter("10");                                   // Xgrid 10
        r.key(pckey::F4); r.key(pckey::F1); r.type("2"); r.key(pckey::Enter);   // STORE 3D V-Win 2
        CHECK(ws.graph3d.vmemUsed == 2 && ws.graph3d.vmem[1].xgrid == 10);
        r.key(pckey::F1);                                                  // INITIAL
        r.key(pckey::F4); r.key(pckey::F2); r.type("2"); r.key(pckey::Enter);   // RECALL 3D V-Win 2
        r.key(pckey::F4); r.key(pckey::F2); r.type("4"); r.key(pckey::Enter);
        CHECK(r.error() && r.errorTitle() == "Memory ERROR");              // empty memory
        r.closeError();
        r.key(pckey::Esc);                                                 // back to the graph, recomputed
        CHECK(set.window.xgrid == 10 && g->screen() == G3::Screen::Plot);
        r.settle();
        CHECK(g->mesh(0).nu == 10);
        r.key(pckey::F3); r.key(pckey::F1); r.key(pckey::Esc); r.settle();   // INITIAL again
        CHECK(set.window == g3d::Window{});

        // ---- S1-S4, S7: the Setup of the mode (SHIFT MENU)
        r.key(pckey::F10);
        CHECK(r.active<G3>() == g && g->screen() == G3::Screen::Setup);    // not the SYSTEM settings
        for (int k = 0; k < 3; ++k) r.key(pckey::Down);
        r.key(pckey::Right);                                               // Plot/LineCol Blue -> Red
        r.key(pckey::Down); r.key(pckey::Right);                           // Axes Off
        r.key(pckey::Down); r.key(pckey::Right);                           // Box Off
        r.key(pckey::Down); r.key(pckey::Right);                           // Label Off
        r.key(pckey::Esc);
        CHECK(g->screen() == G3::Screen::Plot && !set.setup.axes && !set.setup.box && !set.setup.label && set.setup.plot == g3d::Color::Red);
        r.key(pckey::F10); r.key(pckey::Down); r.key(pckey::Right);        // Angle Rad -> Gra
        r.key(pckey::Esc);
        CHECK(ws.settings.angle == AngleMode::Gradians);
        r.settle();
        set.setup = g3d::Setup{};
        ws.settings.angle = AngleMode::Radians;

        // ---- G5, G9: DELETE with confirmation, FORMAT (SHIFT 5)
        r.key(pckey::F6);                                                  // list
        set.fns[0].area = g3d::Color::Yellow;
        r.key(pckey::F2);                                                  // DELETE row 1
        r.key(pckey::F6);                                                  // No
        CHECK(set.fns[0].kind == g3d::Kind::Z);
        r.key(pckey::F2); r.key(pckey::F1);                                // Yes
        CHECK(set.fns[0].kind == g3d::Kind::None && set.fns[0].area == g3d::Color::Yellow);
        r.key(pckey::Down);
        r.key(pckey::F7); r.type("5");                                     // FORMAT of row 2
        CHECK(g->screen() == G3::Screen::Format);
        r.key(pckey::Right);                                               // Line Color Black -> Blue
        r.key(pckey::Down); r.key(pckey::Left); r.key(pckey::Left); r.key(pckey::Left);   // Area Color Red -> Clear
        r.key(pckey::Esc);
        CHECK(set.fns[1].line == g3d::Color::Blue && set.fns[1].area == g3d::Color::Clear);
        r.key(pckey::F8);                                                  // OPTN: FORMAT too
        r.key(pckey::Left);                                                // Line Color Blue -> Black
        r.key(pckey::Left);                                                // -> Clear
        r.key(pckey::Esc);
        CHECK(r.error() && r.errorTitle() == "Invalid Setting");           // both Clear
        r.closeError();
        r.key(pckey::Break);                                               // AC: cancelled
        CHECK(g->screen() == G3::Screen::List && set.fns[1].line == g3d::Color::Blue);

        // ---- T18-T21: Z=, Param, Rot X, Rot Y input
        r.key(pckey::Up);
        r.key(pckey::F3); r.key(pckey::F2);                                // Z=
        CHECK(g->screen() == G3::Screen::Input && g->editing().kind == g3d::Kind::Z);
        r.key(pckey::Enter);                                               // edit the line
        r.key(pckey::F1); r.type("+"); r.key(pckey::F2);                   // F1 X, F2 Y
        r.key(pckey::Enter);
        CHECK(g->editing().expr[0] == "X+Y");
        r.key(pckey::F6);
        CHECK(set.fns[0].kind == g3d::Kind::Z && set.fns[0].expr[0] == "X+Y");
        r.key(pckey::F3); r.key(pckey::F3);                                // Param
        r.key(pckey::Enter); r.key(pckey::F1); r.key(pckey::Enter);        // Xst = S
        r.key(pckey::Enter); r.key(pckey::F2); r.key(pckey::Enter);        // Yst = T
        r.key(pckey::F6);
        CHECK(r.error() && r.errorTitle() == "Syntax ERROR");              // Zst missing
        r.closeError();
        r.key(pckey::Enter); r.type("s*t"); r.key(pckey::Enter);
        r.key(pckey::F6);
        CHECK(set.fns[0].kind == g3d::Kind::Param && set.fns[0].expr[0] == "S" && set.fns[0].expr[1] == "T" && set.fns[0].expr[2] == "s*t");
        r.key(pckey::F3); r.key(pckey::F4); CHECK(g->sub() == G3::Sub::Rotate);
        r.key(pckey::F1);                                                  // Rot X
        CHECK(g->editing().kind == g3d::Kind::RotX);
        r.type("1"); r.key(pckey::Enter); r.key(pckey::F6);
        CHECK(set.fns[0].kind == g3d::Kind::RotX && set.fns[0].expr[0] == "1");
        r.key(pckey::F3); r.key(pckey::F4); r.key(pckey::F2);              // Rot Y
        r.type("x"); r.key(pckey::Enter); r.key(pckey::F6);
        CHECK(set.fns[0].kind == g3d::Kind::RotY);
        // drawing every kind together: rotation solid y = x about Y is a cone-like surface
        set.fns[1] = g3d::emptyFunction(1);
        set.fns[1].kind = g3d::Kind::Param; set.fns[1].expr = {"cos(s)", "sin(s)", "t"};
        set.fns[2] = tmpl(g3d::Kind::Cylinder, 0, {1, -1, 1, 0, 0});
        r.key(pckey::F6); r.settle();
        CHECK(!r.error() && g->drawn() && g->mesh(0).valid > 0 && g->mesh(1).valid > 0 && g->mesh(2, 2).valid > 0);
        g3d::Vec3 sample{g->mesh(0).verts[5].x, g->mesh(0).verts[5].y, g->mesh(0).verts[5].z};
        CHECK(near(std::hypot(sample.x, sample.z), std::fabs(sample.y), 1e-4));      // |x| = y on the solid of rotation
        const auto& pm = g->mesh(1);
        bool onCyl = true;
        for (const auto& v : pm.verts) onCyl = onCyl && near(std::hypot(v.x, v.y), 1.0, 1e-5);
        CHECK(onCyl);                                                       // S, T of the parametric graph
        r.key(pckey::Esc);

        // ---- I2: the angle unit of the functions; I3: non-real points are holes
        set.fns[0] = zf("sin(x)");
        set.fns[1] = zf("\xE2\x88\x9A(x)", 1);
        set.fns[2].kind = g3d::Kind::None;
        ws.settings.angle = AngleMode::Degrees;
        r.key(pckey::F6); r.settle();
        const float degValue = g->mesh(0).verts[0].z;
        CHECK(near(degValue, std::sin(-3 * M_PI / 180), 1e-6));
        CHECK(g->mesh(1).valid > 0 && g->mesh(1).valid < g->mesh(1).verts.size());   // sqrt(x) for x < 0: holes
        r.key(pckey::Esc);
        ws.settings.angle = AngleMode::Radians;
        r.key(pckey::F6); r.settle();
        CHECK(near(g->mesh(0).verts[0].z, std::sin(-3.0), 1e-6));
        // P12: AC stops the computation, the drawing is partial
        set.window.xgrid = set.window.ygrid = 50;
        set.fns[1] = zf("x^2+y^2", 1);
        r.key(pckey::Esc);
        r.key(pckey::F6);
        CHECK(g->sampling());
        r.key(pckey::Break);
        CHECK(!g->sampling() && g->drawn() && g->screen() == G3::Screen::Plot);
        CHECK(g->mesh(1).valid < g->mesh(1).verts.size());
        r.key(pckey::Esc);
        set.window = g3d::Window{};

        // ---- G7: 3D-GMEM STORE / RECALL, empty and full memories
        r.key(pckey::F4); CHECK(g->sub() == G3::Sub::GMem);
        r.key(pckey::F1); r.type("1"); r.key(pckey::Enter);
        CHECK(!ws.graph3d.gmem[0].empty());
        const std::string stored = g3d::encode(set);
        set.fns[0] = g3d::emptyFunction(0); set.window.theta = 0; set.setup.box = false;
        r.key(pckey::F4); r.key(pckey::F2); r.type("1"); r.key(pckey::Enter);
        CHECK(g3d::encode(set) == stored && set.fns[0].expr[0] == "sin(x)");
        r.key(pckey::F4); r.key(pckey::F2); r.type("7"); r.key(pckey::Enter);
        CHECK(r.error() && r.errorTitle() == "Memory ERROR");
        r.closeError();
        r.key(pckey::F4); r.key(pckey::F1); r.type("21"); r.key(pckey::Enter);
        CHECK(r.error() && r.errorTitle() == "Argument ERROR");
        r.closeError();
        for (std::size_t k = 1; k < g3d::GraphMemories; ++k) ws.graph3d.gmem[k] = std::string(limits::kGraph3DMemoryBytes / 19, 'x');
        r.key(pckey::F4); r.key(pckey::F1); r.type("1"); r.key(pckey::Enter);
        CHECK(r.error() && r.errorTitle() == "Memory ERROR");
        r.closeError();
        for (std::size_t k = 1; k < g3d::GraphMemories; ++k) ws.graph3d.gmem[k].clear();

        // ---- I6: MEMORY deletes the 3D G-Mem and the 3D V-Win (3DGMEM n, 3DVWIN_n)
        ws.graph3d.vmemUsed = 3;
        r.home();
        CHECK(g->retainedBytes() == 0);                                    // nothing kept after leaving the mode
        r.type("f");
        CHECK(r.fw.apps().activeId() == app::AppId::Memory);
        for (int k = 0; k < 8; ++k) r.key(pckey::Down);                    // "3D G-Mem 1-20"
        r.key(pckey::F1); r.key(pckey::Enter);
        CHECK(std::all_of(ws.graph3d.gmem.begin(), ws.graph3d.gmem.end(), [](const std::string& m) { return m.empty(); }));
        CHECK(ws.graph3d.vmemUsed == 3 && set.fns[0].kind == g3d::Kind::Z);
        r.key(pckey::Down); r.key(pckey::F1); r.key(pckey::Enter);         // "3D V-Win 1-6"
        CHECK(ws.graph3d.vmemUsed == 0);
        r.key(pckey::Up); r.key(pckey::Up); r.key(pckey::Up);              // "3D Graph (funzioni)"
        r.key(pckey::F1); r.key(pckey::Enter);
        CHECK(set.fns[0].kind == g3d::Kind::None && set.window == g3d::Window{});
        set.fns[0] = zf("x");
        r.home();
        // E7: DRAW with nothing to draw shows the empty box
        r.type("j");
        for (auto& f : set.fns) f.selected = false;
        r.key(pckey::F6); r.settle();
        CHECK(!r.error() && g->drawn() && g->lastRender().ok);
        r.home();
        ws.graph3d = Graph3DState{};
        mem::testing::reset();
    }
    board::host::setFileSystem(&board::host::ramCard());
    return testutil::finish("graph3d_v114_test");
}
