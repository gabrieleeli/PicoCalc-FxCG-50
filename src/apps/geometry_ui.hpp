#pragma once
// Controller di GEOMETRY (v1.19.0, solo Pimoroni Pico Plus 2 W): stato della
// schermata, condiviso dai tasti (geometry_app.cpp) e dal disegno
// (geometry_view.cpp). Il nucleo puro e' in geometry.hpp.
#include "apps.hpp"
#include "geometry.hpp"
#include "pict.hpp"
#include "step_task.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fx::app {
namespace geoui {

enum class Tool : std::uint8_t {
    None, Point, Line, Circle, Arc, Semicircle, Arc3, Box, Polygon,       // Draw e Draw Spec
    Tangent, AnglePosition, Axis, Vector, Center,                          // Construct e Transform
    ZoomBox, Pan, Scroll, Move, PlaceText, PlaceExpression                 // View, Edit, Option
};
enum class Screen : std::uint8_t { Main, Help, Form, Animations, Table };
enum class FormKind : std::uint8_t { None, Setup, Window, Format, Animation, Text, Label, Expression };
enum class BoxState : std::uint8_t { Hidden, Shown, Focused };
enum class Job : std::uint8_t { None, Animate, Table, Solve };
// Menu a tendina (indice = tasto F1-F6; Format e Pict sono elenchi di scelta).
enum MenuId : int { MenuNone = -1, MenuFile = 0, MenuEdit, MenuDraw, MenuConstruct, MenuTransform, MenuAnimate, MenuNumberFormat, MenuBoxType, MenuStore, MenuDelete };

struct Tab { const char* name; const char* const* items; int count; };
struct MenuDef { const Tab* tabs; int count; };
const MenuDef& menuDef(int menu) noexcept;

constexpr int AreaTop = ui::ContentTop + 16;   // sotto la riga del puntatore
constexpr int BoxH = 22;                        // casella di misura
constexpr int AreaH = ui::ContentBottom - AreaTop;  // la vista si mappa sempre su quest'area

struct View {
    ModeShell shell;
    TypedFileMenu files{files::FileType::Geometry};
    Screen screen{Screen::Main};
    double px{0}, py{0};                        // puntatore nel piano
    std::vector<geo::Id> sel;                    // selezione nell'ordine
    Tool tool{Tool::None};
    geo::Kind lineKind{geo::Kind::Segment};
    geo::Spec spec{geo::Spec::Triangle};
    int ngon{0};
    geo::TType ttype{geo::TType::Reflection};
    double tp0{0}, tp1{0};
    std::vector<geo::Id> picks;                  // punti scelti dallo strumento
    std::vector<geo::Id> created;                // punti creati dallo strumento (tolti con EXIT)
    bool corner{false};
    double cx0{0}, cy0{0};                       // primo angolo (Draw Spec, Zoom Box)
    std::string pendingText;                     // Text / Expression da posare
    // menu
    int menu{MenuNone}, tab{0};
    // casella di misura
    BoxState box{BoxState::Hidden};
    int boxType{0};
    std::vector<geo::Id> boxSel;                 // selezione per cui vale boxType
    // moduli
    Form form;
    FormKind formKind{FormKind::None};
    std::size_t formIndex{0};
    // Undo/Redo: un livello che si alterna (E01)
    std::vector<std::uint8_t> undo;
    bool undoValid{false};
    // spostamento (v): stato all'inizio, per EXIT
    std::vector<std::uint8_t> moveStart;
    // lavori a passi (animazioni, tabella, risolutore)
    step::Runner runner;
    std::unique_ptr<step::Task> task;
    Job job{Job::None};
    std::vector<std::uint8_t> jobStart;
    std::vector<std::vector<double>> traces;     // tracce dell'ultima animazione
    std::vector<geo::Id> traceIds;
    // tabella e animazioni
    int tableRow{0}, tableCol{0}, tableFirst{0}, animRow{0};
    int helpPage{0};
    // sfondo e Store Picture
    pict::Image background;
    std::string backgroundPath;
    int pendingStore{0};                          // Pict n da catturare al prossimo disegno
    pict::Image capture;
    bool captureReady{false};
    // testi dell'ultimo disegno (test e simulatore)
    std::string header, boxLine;
};

int areaBottom(const View& v) noexcept;
std::uint16_t paletteColor(std::uint8_t index) noexcept;     // 0 nero, 1 blu, 2 rosso, 3 magenta, 4 verde, 5 ciano, 6 giallo
const char* paletteName(std::uint8_t index) noexcept;
std::string formatName(std::uint8_t fmt);                    // "Fix2", "Sci3", "Norm"
// Oggetto testo (Text, misura, espressione) sotto il punto di schermo (sx, sy).
geo::Id hitText(const Context& ctx, const View& v, int sx, int sy);
void toScreen(const Context& ctx, double x, double y, int& sx, int& sy);
double pixelTolerance(const Context& ctx) noexcept;           // 6 pixel nel piano

// disegno (geometry_view.cpp)
void drawMain(Context& ctx, View& v, gfx::Canvas& c);
void drawHelp(Context& ctx, View& v, gfx::Canvas& c);
void drawAnimations(Context& ctx, View& v, gfx::Canvas& c);
void drawTable(Context& ctx, View& v, gfx::Canvas& c);
const char* const* helpLines(int page, int& count) noexcept;
constexpr int HelpPages = 3;
} // namespace geoui
} // namespace fx::app
