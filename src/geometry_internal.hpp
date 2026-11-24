#pragma once
// Parti interne del nucleo di Geometry (v1.19.0), condivise da src/geometry*.cpp:
// vettori del piano, accesso alle forme dei genitori, intersezioni, aggiunta
// degli oggetti con i limiti. Non e' un'interfaccia: usare geometry.hpp.
#include "geometry.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace fx::geo::detail {
constexpr double Pi = 3.14159265358979323846, TwoPi = 2 * Pi;

struct V2 { double x{0}, y{0}; };
inline V2 operator+(V2 a, V2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
inline V2 operator-(V2 a, V2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
inline V2 operator*(double k, V2 a) noexcept { return {k * a.x, k * a.y}; }
inline double dot(V2 a, V2 b) noexcept { return a.x * b.x + a.y * b.y; }
inline double cross(V2 a, V2 b) noexcept { return a.x * b.y - a.y * b.x; }
inline double norm(V2 a) noexcept { return std::hypot(a.x, a.y); }
inline V2 rot90(V2 a) noexcept { return {-a.y, a.x}; }
inline V2 rotate(V2 a, double ang) noexcept { const double c = std::cos(ang), s = std::sin(ang); return {c * a.x - s * a.y, s * a.x + c * a.y}; }
inline V2 dir(double ang) noexcept { return {std::cos(ang), std::sin(ang)}; }
inline double angleOf(V2 a) noexcept { return std::atan2(a.y, a.x); }
// (-pi, pi] e [0, 2 pi)
inline double wrapPi(double a) noexcept { a = std::fmod(a, TwoPi); if (a <= -Pi) a += TwoPi; else if (a > Pi) a -= TwoPi; return a; }
inline double wrap2Pi(double a) noexcept { a = std::fmod(a, TwoPi); if (a < 0) a += TwoPi; if (a >= TwoPi) a -= TwoPi; return a; }
inline double scaleOf(std::initializer_list<double> v) noexcept { double s = 1; for (double x : v) s = std::max(s, std::abs(x)); return s; }

inline V2 pointOf(const Shape& g) noexcept { return {g.a[0], g.a[1]}; }
inline V2 startOf(const Shape& g) noexcept { return {g.a[0], g.a[1]}; }
inline V2 endOf(const Shape& g) noexcept { return {g.a[2], g.a[3]}; }

// Forma definita di un oggetto (nullptr se manca o e' indefinita).
const Shape* shapeOf(const State& s, Id id) noexcept;
const Object* objectOf(const State& s, Id id) noexcept;

// Curva per le intersezioni e le proiezioni: retta P + t d con t nell'intervallo,
// oppure circonferenza/arco (centro, raggio, a0, ampiezza; full = circonferenza).
struct Curve {
    bool line{true};
    V2 p, d;
    double tmin{-HUGE_VAL}, tmax{HUGE_VAL};
    V2 c;
    double r{0}, a0{0}, sweep{TwoPi};
    bool full{true};
};
bool curveOf(const State& s, const Object& o, Curve& out) noexcept;
// Intersezioni delle due curve (al piu' 2), ordinate per parametro sulla prima.
int intersect(const Curve& c1, const Curve& c2, V2 out[2]) noexcept;
bool onArc(const Curve& c, double angle, double eps) noexcept;      // angolo dentro l'ampiezza
double arcParam(const Curve& c, double angle) noexcept;              // posizione nell'arco (per ordinare)

// Aggiunge un oggetto (id nuovo, limiti, geometria calcolata) e restituisce il suo id.
Id append(State& s, Object o);
void checkRoom(const State& s, std::size_t more);                    // Memory ERROR se non ci stanno
[[noreturn]] void fail(const std::string& message);                  // MathError
void evaluate(State& s, Object& o);                                   // ricalcola la forma di un oggetto
double toRadians(const Options& o, double v) noexcept;               // unita' delle Option -> radianti
double fromRadians(const Options& o, double v) noexcept;
// Variabili libere di un oggetto (oggetto, indice) che il risolutore puo' muovere.
bool solverVar(const Object& o, std::size_t k) noexcept;
// Indice di un oggetto (che deve esistere).
inline std::size_t at(const State& s, Id id) { return static_cast<std::size_t>(indexOf(s, id)); }
void removeReferences(State& s, const std::vector<char>& gone);      // blocchi, animazioni, tabella
bool decodeObjects(State& t, const std::uint8_t* data, std::size_t size, bool withFile);
} // namespace fx::geo::detail
