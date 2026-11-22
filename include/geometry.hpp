#pragma once
// Nucleo di Geometry (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Costruzione di punti, segmenti, rette, circonferenze e poligoni con
// coordinate reali; selezione per vicinanza, spostamento, cancellazione,
// misure (lunghezza, angolo, area, coordinate) e vista (centro e scala per
// zoom e pan). Funzioni pure: il disegno e i tasti sono nel controller.
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::geo {
constexpr std::size_t MaxObjects = 500;
enum class Kind : std::uint8_t { Point = 0, Segment = 1, Line = 2, Circle = 3, Polygon = 4 };
const char* kindName(Kind k) noexcept;
// Punti necessari per completare l'oggetto (Polygon: almeno 3, chiuso con EXE sul primo).
std::size_t pointsNeeded(Kind k) noexcept;
struct Object {
    Kind kind{Kind::Point};
    std::vector<double> xy;                      // x0, y0, x1, y1... (Circle: centro, poi un punto della circonferenza)
    std::size_t count() const noexcept { return xy.size() / 2; }
    bool operator==(const Object& o) const noexcept { return kind == o.kind && xy == o.xy; }
};
struct View {
    double cx{0}, cy{0};                         // centro della finestra
    double scale{24};                            // pixel per unita'
    bool operator==(const View& o) const noexcept { return cx == o.cx && cy == o.cy && scale == o.scale; }
};
struct State {
    std::vector<Object> objects;
    View view;
    std::string file;                            // file .pcgeo aperto ("" = nessuno)
    bool operator==(const State& o) const noexcept { return objects == o.objects && view == o.view && file == o.file; }
};
std::string validate(const Object& o);           // "" se l'oggetto e' valido
// Aggiunge un oggetto (MathError "Memory ERROR" oltre MaxObjects, "Argument ERROR" se non valido).
void add(State& s, Object o);
// Oggetto piu' vicino a (x, y) entro `tolerance` unita' (-1 = nessuno).
int hitTest(const State& s, double x, double y, double tolerance);
void move(Object& o, double dx, double dy) noexcept;
// Misure di un oggetto: righe di testo ("Lunghezza=5", "Area=12"...).
std::vector<std::string> measure(const Object& o);
double distance(double x0, double y0, double x1, double y1) noexcept;
double angleDegrees(double ax, double ay, double vx, double vy, double bx, double by) noexcept;   // angolo AVB
double polygonArea(const Object& o) noexcept;    // area (formula di Gauss, valore assoluto)
// Vista: coordinate di schermo <-> piano (area di disegno w x h con l'origine in alto a sinistra).
void toScreen(const View& v, int w, int h, double x, double y, double& sx, double& sy) noexcept;
void toPlane(const View& v, int w, int h, double sx, double sy, double& x, double& y) noexcept;
void zoom(View& v, double factor) noexcept;      // factor > 1 avvicina
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
bool decode(State& s, const std::uint8_t* data, std::size_t size);
// File .pcgeo (oggetti e vista).
std::vector<std::uint8_t> encodeFile(const State& s);
bool decodeFile(const std::uint8_t* data, std::size_t size, State& out);
} // namespace fx::geo
