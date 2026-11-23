#pragma once
// v1.18.0: disegno condiviso di GRAPH e TABLE (servizio GraphCore, entrambe le
// schede). Il Dual Screen della fx-CG50 (manuale 5-23, 5-36, 5-55) divide lo
// schermo in due riquadri: sulla PicoCalc (320x320) il principale sta SOPRA e
// il sotto-schermo o la tabella SOTTO, 320x140 ciascuno (decisione D1 della
// v1.18.0). Qui stanno la geometria dei riquadri, la cornice di un riquadro
// (griglia, assi, scala, etichette), le curve e i tratti, le curve dei punti
// di una tabella (GPH-CON / GPH-PLT) e il riquadro di una tabella numerica:
// GraphApp e TableApp li usano entrambe, cosi' il T+G non richiede che una
// modalita' chiami l'altra e non c'e' codice duplicato.
#include "graph_engine.hpp"
#include "graphics_engine.hpp"
#include "number_format.hpp"
#include "table_model.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace fx::graphview {

// ---- riquadri
enum class Pane : std::uint8_t { Full = 0, Main = 1, Sub = 2 };
// Viewport del riquadro: Full = 320x282, Main = righe 18..157, Sub = righe 160..299.
graph::Viewport viewport(const ViewWindow& v, Pane p) noexcept;
gfx::Rect paneRect(Pane p) noexcept;
// Linea di separazione tra i due riquadri (righe 158 e 159).
void drawSeparator(gfx::Canvas& c);

// ---- colori, penne, tratti
std::uint16_t lineColor(std::uint8_t color) noexcept;          // colori della fx-CG50 (GraphColor)
struct Pen { int width; int on; int off; };
Pen penFor(LineStyle s, bool strict) noexcept;                  // strict: disequazione > o < (bordo escluso)
void stamp(gfx::Canvas& c, int x, int y, int w, std::uint16_t col);
void segment(gfx::Canvas& c, int x0, int y0, int x1, int y1, const Pen& pen, std::uint16_t col, int& phase);
void drawCurve(gfx::Canvas& c, const std::vector<std::int16_t>& xy, const Pen& pen, std::uint16_t col);
// Segmento in coordinate del grafico, tagliato al riquadro prima del disegno.
void mathSegment(gfx::Canvas& c, const graph::Viewport& vp, double x0, double y0, double x1, double y1, const Pen& pen, std::uint16_t col);
std::string shortNumber(double v);                              // "%.4g", -0 -> 0

// ---- cornice di un riquadro: griglia (On/Line), assi, tacche, scala, etichette x, y, O
void drawFrame(gfx::Canvas& c, const graph::Viewport& vp, const GraphSettings& gs);

// ---- GPH-CON / GPH-PLT: punti delle righe della tabella per il grafico p
// (Y=: (v, Y(v)); r=: polare; Param: (Xt, Yt)), collegati o isolati.
void tableCurve(const graph::GraphEngine& engine, const graph::Plot& p, const std::vector<double>& rows,
                const graph::Viewport& vp, bool connect, graph::Curve& out);

// ---- tabella numerica (TABLE, T+G, GtoT): intestazione con la variabile e le
// colonne colorate, celle di 6 caratteri, cursore, barra di scorrimento e
// valore completo della cella in basso. Disegnata tra top e bottom, larghezza
// dello schermo; con active = false il cursore e' solo un contorno.
struct TableCursor { int row{0}, col{-1}, firstRow{0}, firstCol{0}; };
struct TableData {
    const graph::GraphEngine* engine{};
    const std::vector<table::Column>* columns{};
    const std::vector<double>* rows{};
    GraphType type{GraphType::Y};
    DisplayFormat display{};
};
bool tableCell(const TableData& d, int row, int column, double& out) noexcept;   // column -1: la variabile
void drawTable(gfx::Canvas& c, int top, int bottom, const TableData& d, TableCursor& cur, bool active);
int tableVisibleRows(int top, int bottom) noexcept;

} // namespace fx::graphview
