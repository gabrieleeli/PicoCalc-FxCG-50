// geometria2d 1.0.0 - vedi geometria2d.hpp
#include "geometria2d.hpp"
#include <cmath>

double distanza(Punto a, Punto b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

double perimetro(const std::vector<Punto>& p) {
    double s = 0;
    for (std::size_t i = 0; i < p.size(); i++) s += distanza(p[i], p[(i + 1) % p.size()]);
    return s;
}

double areaPoligono(const std::vector<Punto>& p) {
    double s = 0;
    for (std::size_t i = 0; i < p.size(); i++) {
        const Punto& a = p[i];
        const Punto& b = p[(i + 1) % p.size()];
        s += a.x * b.y - b.x * a.y;
    }
    return std::fabs(s) / 2;
}

Punto baricentro(const std::vector<Punto>& p) {
    Punto g{0, 0};
    if (p.empty()) return g;
    for (const Punto& q : p) { g.x += q.x; g.y += q.y; }
    g.x /= p.size();
    g.y /= p.size();
    return g;
}

Punto ruota(Punto p, Punto c, double gradi) {
    const double a = gradi * M_PI / 180;
    const double dx = p.x - c.x, dy = p.y - c.y;
    return Punto{c.x + dx * std::cos(a) - dy * std::sin(a), c.y + dx * std::sin(a) + dy * std::cos(a)};
}

bool dentro(const std::vector<Punto>& p, Punto q) {
    bool in = false;
    for (std::size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
        if ((p[i].y > q.y) != (p[j].y > q.y) && q.x < (p[j].x - p[i].x) * (q.y - p[i].y) / (p[j].y - p[i].y) + p[i].x) in = !in;
    }
    return in;
}
