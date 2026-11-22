#pragma once
// Nucleo di Picture Plot (v1.17.0, modalita' Base; solo Pimoroni Pico Plus 2 W).
// Un'immagine .pcpict come sfondo, punti messi con il cursore e numerati nelle
// coordinate della finestra della modalita', coordinate verso List 1 e
// List 2 e regressione con il motore di Statistics (stats::regress).
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::pplot {
constexpr std::size_t MaxPoints = 999;
struct Point { double x{}, y{}; bool operator==(const Point& o) const noexcept { return x == o.x && y == o.y; } };
struct Window {
    double xmin{0}, xmax{10}, ymin{0}, ymax{10};
    bool operator==(const Window& o) const noexcept { return xmin == o.xmin && xmax == o.xmax && ymin == o.ymin && ymax == o.ymax; }
};
struct State {
    std::string background;                      // nome dell'immagine (PICOCALC/PICT o CAPT), "" = nessuna
    std::vector<Point> points;
    Window window;
    int regression{-1};                          // stats::RegType scelto (-1 = nessuno)
    std::string file;                            // file .pcplot aperto
    bool operator==(const State& o) const noexcept {
        return background == o.background && points == o.points && window == o.window && regression == o.regression && file == o.file;
    }
};
std::string validate(const Window& w);
void addPoint(State& s, double x, double y);     // "Memory ERROR" oltre MaxPoints
// Pixel <-> coordinate nell'area di disegno w x h.
void toPlane(const Window& w, int width, int height, int px, int py, double& x, double& y) noexcept;
void toScreen(const Window& w, int width, int height, double x, double y, double& px, double& py) noexcept;
void coordinates(const State& s, std::vector<double>& xs, std::vector<double>& ys);
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
bool decode(State& s, const std::uint8_t* data, std::size_t size);
std::vector<std::uint8_t> encodeFile(const State& s);
bool decodeFile(const std::uint8_t* data, std::size_t size, State& out);
} // namespace fx::pplot
