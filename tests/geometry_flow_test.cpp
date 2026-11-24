// v1.19.0 (solo Pimoroni Pico Plus 2 W): GEOMETRY alla parita' del capitolo 14,
// un caso per voce della tabella di docs/reports/ANALISI_FONTI_v1.19.0.md (§3,
// colonna GF): tasti veri dal firmware simulato (app_rig), menu, strumenti,
// casella di misura, moduli, file sulla microSD simulata, pixel del disegno,
// liste e Statistics, riavvio. I numeri del nucleo sono in geometry_test.
#include "app_rig.hpp"
#include "file_store.hpp"
#include "geometry.hpp"
#include "pict.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include "typed_files.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {
#include "fixtures/geometry_v1_17_0.inc"
}

using namespace fx;
using namespace fx::app;
namespace pk = fx::pckey;
using testrig::Rig;

namespace {
#define T(id, cond) ::testutil::report(static_cast<bool>(cond), id, __FILE__, __LINE__, #cond)
geo::State& S(Rig& r) { return r.ws().geometry; }
GeometryApp* openGeo(Rig& r) { r.home(); r.type("h"); return r.active<GeometryApp>(); }
GeometryApp* fresh(Rig& r) {
    r.closeError();
    r.home();
    S(r) = geo::State{};
    return openGeo(r);
}
std::string scr(Rig& r) { GeometryApp* g = r.active<GeometryApp>(); return g ? g->screenName() : "-"; }
double niceStep(double scale) {
    const double raw = 6.0 / scale;
    const double p = std::pow(10.0, std::floor(std::log10(raw)));
    for (double m : {1.0, 2.0, 2.5, 5.0, 10.0}) if (m * p >= raw) return m * p;
    return 10 * p;
}
void moveTo(Rig& r, double x, double y) {
    GeometryApp* g = r.active<GeometryApp>();
    if (!g) return;
    const double st = niceStep(S(r).view.scale);
    const long nx = std::lround((x - g->pointerX()) / st), ny = std::lround((y - g->pointerY()) / st);
    for (long i = 0; i < std::abs(nx) && i < 2000; ++i) r.key(nx > 0 ? pk::Right : pk::Left);
    for (long i = 0; i < std::abs(ny) && i < 2000; ++i) r.key(ny > 0 ? pk::Up : pk::Down);
}
void exeAt(Rig& r, double x, double y) { moveTo(r, x, y); r.key(pk::Enter); }
// menu F1-F6: scheda (frecce a destra) e voce (cifra 1-9)
void menu(Rig& r, std::uint8_t fkey, int tab, int item) {
    r.key(fkey);
    for (int i = 0; i < tab; ++i) r.key(pk::Right);
    r.type(std::to_string(item));
}
void segment(Rig& r, double x0, double y0, double x1, double y1, int item = 2) {
    menu(r, pk::F3, 0, item);
    exeAt(r, x0, y0);
    exeAt(r, x1, y1);
    r.key(pk::Esc);
}
void box(Rig& r, int item, double x0, double y0, double x1, double y1) {
    menu(r, pk::F3, 1, item);
    exeAt(r, x0, y0);
    exeAt(r, x1, y1);
    r.key(pk::Esc);
}
std::vector<geo::Id> ofKind(Rig& r, geo::Kind k) { std::vector<geo::Id> v; for (const auto& o : S(r).objects) if (o.kind == k) v.push_back(o.id); return v; }
std::size_t countKind(Rig& r, geo::Kind k) { return ofKind(r, k).size(); }
double px(Rig& r, geo::Id id) { const geo::Object* o = geo::find(S(r), id); return o && o->g.ok ? o->g.a[0] : NAN; }
double py(Rig& r, geo::Id id) { const geo::Object* o = geo::find(S(r), id); return o && o->g.ok ? o->g.a[1] : NAN; }
double val(Rig& r, geo::MType t, std::vector<geo::Id> ops, int i = 0) { const auto m = geo::measure(S(r), geo::MSpec{t, std::move(ops)}); return m.ok ? m.v[i] : NAN; }
bool near(double a, double b, double tol = 1e-9) { return std::abs(a - b) <= tol * std::max(1.0, std::abs(b)); }
std::size_t pixels(std::uint16_t color, int top = ui::ContentTop + 16, int bottom = ui::ContentBottom) {
    std::vector<std::uint16_t> buf(static_cast<std::size_t>(ui::ScreenW) * ui::ScreenH);
    board::graphics().copyTo(gfx::Rect{0, 0, ui::ScreenW, ui::ScreenH}, buf.data(), ui::ScreenW);
    std::size_t n = 0;
    for (int y = top; y < bottom; ++y) for (int x = 0; x < ui::ScreenW; ++x) n += buf[static_cast<std::size_t>(y) * ui::ScreenW + static_cast<std::size_t>(x)] == color;
    return n;
}
void redraw(Rig& r) { r.fw.draw(board::graphics()); board::present(); }
bool fileExists(const std::string& path) { files::Entry e; return board::host::ramCard().stat(path, e) == files::Status::Ok && !e.directory; }
std::string err(Rig& r) { return r.errorTitle() + ": " + r.errorDetail(); }
bool selected(Rig& r, geo::Id id) { const auto s = r.active<GeometryApp>()->selection(); return std::find(s.begin(), s.end(), id) != s.end(); }
void selectAt(Rig& r, double x, double y) { exeAt(r, x, y); }
// colori della tavolozza di Geometry (geometry_view.cpp)
constexpr std::uint16_t Red = gfx::rgb888To565(220, 30, 30);
constexpr std::uint16_t Muted = ui::theme::Muted, GridColor = ui::theme::Grid, Warn = ui::theme::Warn;

// ---------------------------------------------------------------- tasti e menu
void keysAndMenus(Rig& r) {
    GeometryApp* g = fresh(r);
    T("R01 Geometry aperta da MENU H", g && r.fw.apps().activeId() == AppId::Geometry && scr(r) == "main");
    r.key(pk::F1);
    T("K01 F1 File", g->menuTitle() == "File" && g->menuItems().size() == 5 && g->menuItems()[3] == "Key Help" && g->menuItems()[4] == "Option");
    r.key(pk::Right);
    T("K01 scheda View", g->menuTitle() == "View" && g->menuItems().size() == 8 && g->menuItems()[0] == "Zoom Box" && g->menuItems()[5] == "Zoom to Fit");
    r.key(pk::Left);
    T("K01 scheda indietro", g->menuTitle() == "File");
    r.key(pk::Esc);
    T("K01 EXIT chiude", scr(r) == "main");
    r.key(pk::F2);
    T("K01 F2 Edit", g->menuTitle() == "Edit" && g->menuItems()[0] == "Undo/Redo" && g->menuItems()[5] == "Clear All");
    r.key(pk::Right);
    T("K01 scheda Option", g->menuTitle() == "Option" && g->menuItems()[4] == "Clr Constraint" && g->menuItems()[7] == "Area Calc");
    r.key(pk::Right);
    T("K01 scheda Properties", g->menuTitle() == "Properties" && g->menuItems()[3] == "Fade I/O" && g->menuItems()[4] == "Store Picture");
    r.key(pk::Esc);
    r.key(pk::F3);
    T("K01 F3 Draw", g->menuTitle() == "Draw" && g->menuItems().size() == 9 && g->menuItems()[7] == "SemiCirc (Diam)");
    r.key(pk::Right);
    T("K01 scheda Draw Spec", g->menuTitle() == "Draw Spec" && g->menuItems().size() == 7 && g->menuItems()[6] == "Function f(x)");
    r.key(pk::Esc);
    r.key(pk::F4);
    T("K01 F4 Construct", g->menuTitle() == "Construct" && g->menuItems().size() == 8 && g->menuItems()[7] == "Attached Angle");
    r.key(pk::Esc);
    r.key(pk::F5);
    T("K01 F5 Transform", g->menuTitle() == "Transform" && g->menuItems().size() == 6 && g->menuItems()[2] == "Trans(Sel Vec)");
    r.key(pk::Esc);
    r.key(pk::F6);
    T("K01 F6 Animate", g->menuTitle() == "Animate" && g->menuItems().size() == 8 && g->menuItems()[7] == "Display Table");
    r.type("9");
    T("K01 cifra fuori dalle voci: il menu resta", scr(r) == "menu");
    r.key(pk::Esc);
    menu(r, pk::F3, 0, 1);
    T("K01 scelta con la cifra", scr(r) == "point");
    r.key(pk::Esc);
    r.key(pk::F3); r.key(pk::Down); r.key(pk::Enter);
    T("K01 scelta con EXE", scr(r) == "line");
    r.key(pk::Esc);
    // K02 puntatore
    T("K02 puntatore all'origine", g->pointerX() == 0 && g->pointerY() == 0);
    for (int i = 0; i < 4; ++i) r.key(pk::Right);
    T("K02 passo rotondo", g->pointerX() == 1 && g->headerText().find("(1, 0)") == 0);
    r.key(pk::F7); r.key(pk::Up);
    T("K02 SHIFT: passo x4", g->pointerY() == 1);
    r.type("9");
    T("K02 9: settore in alto a destra", g->pointerX() > 4 && g->pointerY() > 3);
    r.type("1");
    T("K02 1: settore in basso a sinistra", g->pointerX() < -4 && g->pointerY() < -3);
    r.type("5");
    T("K02 5: centro", std::abs(g->pointerX()) < 0.3 && std::abs(g->pointerY()) < 0.3);
    segment(r, 0, 0, 2, 0);
    moveTo(r, 2, 0);
    redraw(r);
    T("K02 evidenziazione: il nome sotto il puntatore", g->headerText().find("Punto B") != std::string::npos);
    moveTo(r, 1, 0);
    redraw(r);
    T("K02 evidenziazione del segmento", g->headerText().find("Segmento AB") != std::string::npos);
    // K03 tasti diretti (gli altri nelle voci): MENU esce
    r.key(pk::Tab);
    T("K03 MENU esce", r.fw.apps().activeId() == AppId::MainMenu);
    // dalla striscia di eActivity: EXIT a riposo torna al documento, OPTN apre la casella
    r.fw.apps().context().beginStrip(AppId::EActivity, AppId::Geometry, 0);
    T("K03 Geometry dalla striscia", r.fw.apps().activeId() == AppId::Geometry);
    r.key(pk::F8);
    T("K03 striscia: OPTN apre la casella", r.active<GeometryApp>() && r.active<GeometryApp>()->boxFocused());
    r.key(pk::F8);
    r.key(pk::Esc);
    T("K03 striscia: EXIT torna a eActivity", r.fw.apps().activeId() == AppId::EActivity);
    r.fw.apps().context().endStrip();
    r.home();
}

// ---------------------------------------------------------------- File
void fileMenu(Rig& r) {
    GeometryApp* g = fresh(r);
    box(r, 1, 0, 0, 4, 3);
    T("F03 prima: un triangolo", countKind(r, geo::Kind::Polygon) == 1);
    menu(r, pk::F1, 0, 3);
    T("F03 Save as: nome", scr(r) == "file");
    r.enter("TRI");
    T("F03 file scritto", fileExists("PICOCALC/GEOM/TRI.pcgeo") && S(r).file == "TRI.pcgeo" && !r.error());
    menu(r, pk::F1, 0, 1);
    T("F01 New chiede conferma", scr(r) == "confirm");
    r.key(pk::Enter);
    T("F01 costruzione vuota", S(r).objects.empty() && S(r).file.empty());
    menu(r, pk::F1, 0, 2);
    T("F02 Open: elenco", scr(r) == "file");
    r.key(pk::Enter);
    T("F02 aperto", countKind(r, geo::Kind::Polygon) == 1 && S(r).file == "TRI.pcgeo" && !r.error());
    // file della v1.17.0 sulla microSD: migrato all'apertura
    {
        auto& card = board::host::ramCard();
        (void)card.makeDirs("PICOCALC/GEOM");
        CHECK(files::writeAtomic(card, "PICOCALC/GEOM/OLD.pcgeo", kGeometryV1File, sizeof kGeometryV1File) == files::Status::Ok);
    }
    menu(r, pk::F1, 0, 2);
    T("F02 con una costruzione aperta: conferma", scr(r) == "confirm");
    r.key(pk::Enter);
    r.key(pk::Home);
    r.key(pk::Enter);                                     // OLD (primo in ordine)
    T("F02 file della v1.17.0 migrato", S(r).file == "OLD.pcgeo" && countKind(r, geo::Kind::Circle) == 1 && countKind(r, geo::Kind::Polygon) == 1 && !r.error());
    // DELETE nell'elenco (F1)
    menu(r, pk::F1, 0, 2);
    r.key(pk::Enter);                                     // conferma Open
    r.key(pk::F1);                                        // DELETE
    r.key(pk::Down);
    r.key(pk::Enter);                                     // TRI (secondo)
    T("F02 DELETE chiede conferma", scr(r) == "confirm");
    r.key(pk::Enter);
    T("F02 file cancellato", !fileExists("PICOCALC/GEOM/TRI.pcgeo") && fileExists("PICOCALC/GEOM/OLD.pcgeo"));
    // F04 Key Help
    menu(r, pk::F1, 0, 4);
    T("F04 Key Help", scr(r) == "help");
    redraw(r);
    T("F04 pagina 1", g->headerText() == "Key Help 1/3");
    r.key(pk::Right); redraw(r);
    T("F04 pagina 2", g->headerText() == "Key Help 2/3");
    r.key(pk::Right); redraw(r);
    r.key(pk::Right); redraw(r);
    T("F04 tre pagine", g->headerText() == "Key Help 1/3");
    r.key(pk::Esc);
    T("F04 EXIT torna", scr(r) == "main");
    // F05 Option: unita' degli angoli e Number Format
    menu(r, pk::F1, 0, 5);
    T("F05 Option", scr(r) == "form");
    r.key(pk::Right);                                     // Number Format: Fix3
    r.key(pk::Down); r.key(pk::Right);                    // Angle: Rad
    r.key(pk::Esc);
    T("F05 applicato", S(r).options.angle == 1 && S(r).options.numFormat == 3 && scr(r) == "main");
    menu(r, pk::F1, 0, 5);
    r.key(pk::Down); r.key(pk::Right);
    r.key(pk::Break);                                     // AC: annulla
    T("F05 AC annulla", S(r).options.angle == 1);
    S(r).options.angle = 0;
    S(r).options.numFormat = 2;
}

// ---------------------------------------------------------------- View
void views(Rig& r) {
    GeometryApp* g = fresh(r);
    (void)g;
    menu(r, pk::F1, 1, 1);
    T("V01 Zoom Box", scr(r) == "zoom-box");
    exeAt(r, 0, 0);
    exeAt(r, 2, 1);
    T("V01 riquadro sullo schermo", near(S(r).view.cx, 1) && near(S(r).view.cy, 0.5) && near(S(r).view.scale, 160) && scr(r) == "main");
    S(r).view = geo::View{};
    r.home(); openGeo(r);
    menu(r, pk::F1, 1, 2);
    T("V02 Pan", scr(r) == "pan");
    r.key(pk::Right);
    T("V02 le frecce spostano la vista", near(S(r).view.cx, 0.25));
    r.key(pk::Esc);
    r.type(".");
    T("V03 Scroll col tasto .", scr(r) == "scroll");
    r.key(pk::Up);
    T("V03 scorre", near(S(r).view.cy, 0.25));
    r.key(pk::Esc);
    r.type("+");
    T("V04 Zoom In x2", near(S(r).view.scale, 48));
    r.type("-");
    T("V05 Zoom Out", near(S(r).view.scale, 24));
    menu(r, pk::F1, 1, 4);
    T("V04 dal menu", near(S(r).view.scale, 48));
    menu(r, pk::F1, 1, 5);
    T("V05 dal menu", near(S(r).view.scale, 24));
    S(r).view = geo::View{};
    r.home(); openGeo(r);
    segment(r, 0, 0, 1, 0);
    S(r).view.cx = 50;
    menu(r, pk::F1, 1, 6);
    T("V06 Zoom to Fit", near(S(r).view.cx, 0.5) && S(r).view.scale > 100);
    S(r).view = geo::View{};
    r.home(); openGeo(r);
    // V07 assi
    redraw(r);
    const std::size_t axesOn = pixels(Muted);
    menu(r, pk::F1, 1, 7);
    redraw(r);
    T("V07 Axes Scale (tacche e numeri)", S(r).options.axes == geo::Axes::Scale && pixels(Muted) > axesOn);
    menu(r, pk::F1, 1, 7);
    redraw(r);
    T("V07 Axes Off", S(r).options.axes == geo::Axes::Off && pixels(Muted) < axesOn / 4);
    menu(r, pk::F1, 1, 7);
    T("V10 Axes dal menu View: di nuovo On", S(r).options.axes == geo::Axes::On);
    // V08 griglia
    menu(r, pk::F1, 1, 8);
    redraw(r);
    T("V08 Grid On (punti)", S(r).options.grid == geo::Grid::On && pixels(Muted) > axesOn + 50);
    menu(r, pk::F1, 1, 8);
    redraw(r);
    T("V08 Grid Line", S(r).options.grid == geo::Grid::Line && pixels(GridColor) > 1000);
    menu(r, pk::F1, 1, 8);
    T("V10 Grid dal menu View: Off", S(r).options.grid == geo::Grid::Off);
    // V09 V-Window
    r.key(pk::F7); r.key(pk::F3);
    T("V09 V-Window", scr(r) == "form");
    r.key(pk::Backspace); r.enter("-8");
    r.key(pk::Backspace); r.enter("8");
    r.key(pk::Backspace); r.enter("1");
    r.key(pk::Esc);
    T("V09 Xmin, Xmax, Ymid", near(S(r).view.scale, 20) && near(S(r).view.cx, 0) && near(S(r).view.cy, 1) && !r.error());
    r.key(pk::F7); r.key(pk::F3);
    r.key(pk::F1);
    r.key(pk::Esc);
    T("V09 INITIAL", near(S(r).view.scale, 24) && near(S(r).view.cy, 0));
    r.key(pk::F7); r.key(pk::F3);
    r.key(pk::Backspace); r.enter("3");
    r.key(pk::Backspace); r.enter("3");
    r.key(pk::Esc);
    T("V09 Xmin >= Xmax: errore", r.error());
    r.closeError();
    r.key(pk::Break);
}

// ---------------------------------------------------------------- Edit
void edits(Rig& r) {
    GeometryApp* g = fresh(r);
    box(r, 1, 0, 0, 4, 3);                               // A(0,0) B(4,0) C(1,3)
    const geo::Id a = S(r).objects[0].id;
    selectAt(r, 0, 0);
    redraw(r);
    T("E07 selezione con EXE", selected(r, a) && g->headerText().find("1 vertice selezionato") != std::string::npos);
    selectAt(r, 0, 0);
    T("E07 EXE di nuovo deseleziona", g->selection().empty());
    selectAt(r, 2, 0);
    selectAt(r, 2.5, 1.5);
    redraw(r);
    T("E07 due lati", g->headerText().find("2 lati selezionati") != std::string::npos);
    r.key(pk::Break);
    T("E03 AC: Deselect All", g->selection().empty());
    menu(r, pk::F2, 0, 2);
    T("E02 Select All", g->selection().size() == S(r).objects.size());
    menu(r, pk::F2, 0, 3);
    T("E03 Deselect All dal menu", g->selection().empty());
    selectAt(r, 4, 0);
    r.type("x");
    T("E04 Select Figure col tasto x", g->selection().size() == 7);
    r.key(pk::Break);
    selectAt(r, 4, 0);
    menu(r, pk::F2, 0, 4);
    T("E04 Select Figure dal menu", g->selection().size() == 7);
    r.key(pk::Break);
    // E08 spostamento della figura
    selectAt(r, 2.5, 1.5);                               // lato BC
    r.type("v");
    T("E08 MOVE", scr(r) == "move");
    for (int i = 0; i < 4; ++i) r.key(pk::Right);
    r.key(pk::Enter);
    T("E08 i vertici del lato si spostano", near(px(r, S(r).objects[1].id), 5) && near(px(r, S(r).objects[2].id), 2) && near(px(r, a), 0));
    r.type("*");
    T("E01 Undo dello spostamento", near(px(r, S(r).objects[1].id), 4));
    r.type("*");
    T("E01 Redo", near(px(r, S(r).objects[1].id), 5));
    r.key(pk::Break);
    selectAt(r, 5, 0);
    r.type("v");
    r.key(pk::Up);
    r.key(pk::Break);
    T("E08 AC annulla lo spostamento", near(py(r, S(r).objects[1].id), 0));
    r.key(pk::Break);
    // E05 Delete con i dipendenti
    const std::size_t before = S(r).objects.size();
    selectAt(r, 0, 0);
    r.key(pk::Backspace);
    T("E05 DEL: il vertice, i lati e il poligono", S(r).objects.size() == before - 4 && countKind(r, geo::Kind::Polygon) == 0);
    menu(r, pk::F2, 0, 1);
    T("E01 Undo/Redo dal menu", S(r).objects.size() == before);
    selectAt(r, 0, 0);
    menu(r, pk::F2, 0, 5);
    T("E05 Delete dal menu", S(r).objects.size() == before - 4);
    menu(r, pk::F2, 0, 6);
    T("E06 Clear All chiede conferma", scr(r) == "confirm");
    r.key(pk::Enter);
    T("E06 tutto cancellato", S(r).objects.empty());
    r.type("*");
    T("E06 Clear All non si annulla", r.error() && S(r).objects.empty());
    r.closeError();
}

// ---------------------------------------------------------------- Draw
void draws(Rig& r) {
    fresh(r);
    menu(r, pk::F3, 0, 1);
    exeAt(r, -2, 2);
    exeAt(r, -1, 2);
    T("D01 Point resta attivo", countKind(r, geo::Kind::Point) == 2 && scr(r) == "point");
    r.key(pk::Esc);
    segment(r, 0, 0, 4, 0);
    T("D02 Line Segment", countKind(r, geo::Kind::Segment) == 1);
    menu(r, pk::F3, 0, 1);
    exeAt(r, 2, 0);
    r.key(pk::Esc);
    const geo::Object& on = S(r).objects.back();
    T("D01 punto sopra il segmento: legato", on.kind == geo::Kind::Point && on.def == geo::Def::On);
    segment(r, 0, 2, 1, 2, 3);
    T("D03 Infinite Line", countKind(r, geo::Kind::Line) == 1);
    segment(r, 0, 3, 1, 3, 4);
    T("D04 Ray", countKind(r, geo::Kind::Ray) == 1);
    segment(r, 0, 4, 1, 4.5, 5);
    T("D05 Vector", countKind(r, geo::Kind::Vector) == 1);
    const std::size_t pointsBefore = countKind(r, geo::Kind::Point);
    segment(r, 0, 0, 0, -2, 2);
    T("D02 un punto esistente si riusa", countKind(r, geo::Kind::Point) == pointsBefore + 1);
    segment(r, 6, 0, 7, 0, 6);
    T("D06 Circle", countKind(r, geo::Kind::Circle) == 1 && near(val(r, geo::MType::Radius, ofKind(r, geo::Kind::Circle)), 1));
    menu(r, pk::F3, 0, 7);
    exeAt(r, -5, -3); exeAt(r, -4, -3); exeAt(r, -5, -1);
    r.key(pk::Esc);
    T("D07 Arc (tre EXE)", countKind(r, geo::Kind::Arc) == 1 && near(val(r, geo::MType::Circumference, ofKind(r, geo::Kind::Arc)), M_PI / 2));
    segment(r, -3, -4, -1, -4, 8);
    T("D08 SemiCirc (Diam)", countKind(r, geo::Kind::Arc) == 2);
    menu(r, pk::F3, 0, 9);
    exeAt(r, 2, -3); exeAt(r, 3, -2); exeAt(r, 4, -3);
    r.key(pk::Esc);
    T("D09 3-Point Arc", countKind(r, geo::Kind::Arc) == 3);
    fresh(r);
    box(r, 1, 0, 0, 4, 3);
    T("D10 Triangle dal riquadro", near(val(r, geo::MType::Area, ofKind(r, geo::Kind::Polygon)), 6));
    box(r, 2, -6, -4, -2, -1);
    T("D11 Isosc Triangle", S(r).objects.back().kind == geo::Kind::Polygon && near(val(r, geo::MType::Area, {S(r).objects.back().id}), 6));
    box(r, 3, 5, -4, 6, -1);
    T("D12 Rectangle", near(val(r, geo::MType::Area, {S(r).objects.back().id}), 3));
    box(r, 4, 0, -5, 3, -4);
    T("D13 Square (lato minore)", near(val(r, geo::MType::Area, {S(r).objects.back().id}), 1));
    menu(r, pk::F3, 1, 6);
    T("D15 Regular n-gon: numero dei lati", scr(r) == "ask");
    r.enter("5");
    exeAt(r, -6, 1); exeAt(r, -4, 3);
    r.key(pk::Esc);
    T("D15 pentagono", geo::find(S(r), S(r).objects.back().id)->parents.size() == 5);
    menu(r, pk::F3, 1, 6);
    r.enter("13");
    T("D15 13 lati: errore", r.error());
    r.closeError();
    fresh(r);
    menu(r, pk::F3, 1, 5);
    exeAt(r, 0, 0); exeAt(r, 2, 0); exeAt(r, 2, 2); exeAt(r, 0, 2); exeAt(r, 0, 0);
    T("D14 Polygon chiuso sul primo vertice", countKind(r, geo::Kind::Polygon) == 1 && near(val(r, geo::MType::Area, ofKind(r, geo::Kind::Polygon)), 4));
    exeAt(r, 4, 0); exeAt(r, 5, 1); exeAt(r, 6, 0);
    r.key(pk::Esc);
    T("D14 EXIT: spezzata aperta", countKind(r, geo::Kind::Polygon) == 1 && countKind(r, geo::Kind::Segment) == 6);
    menu(r, pk::F3, 1, 7);
    r.type("x^2"); r.key(pk::Right); r.enter("-1");
    T("D16 Function f(x)", countKind(r, geo::Kind::Function) == 1 && !r.error());
    {
        double y = 0;
        T("D16 y = x^2-1", geo::evalFunction(S(r), S(r).objects.back(), 3, y) && near(y, 8));
    }
    menu(r, pk::F3, 1, 7);
    r.enter("x+");
    T("D16 espressione non valida: errore", r.error());
    r.closeError();
    fresh(r);
    menu(r, pk::F3, 1, 3);
    exeAt(r, 1, 1); r.key(pk::Enter);
    r.key(pk::Esc);
    T("D17 riquadro di un punto: la figura piu' grande dello schermo", val(r, geo::MType::Area, ofKind(r, geo::Kind::Polygon)) > 50);
}

// ---------------------------------------------------------------- Construct
void constructs(Rig& r) {
    fresh(r);
    segment(r, 0, 0, 4, 0);                               // A B, AB
    const geo::Id ab = ofKind(r, geo::Kind::Segment)[0];
    selectAt(r, 2, 0);
    menu(r, pk::F4, 0, 1);
    T("C01 Perp Bisector", countKind(r, geo::Kind::Line) == 1 && geo::find(S(r), ofKind(r, geo::Kind::Line)[0])->def == geo::Def::PerpBisector);
    menu(r, pk::F3, 0, 1); exeAt(r, 1, 3); r.key(pk::Esc);
    const geo::Id c = S(r).objects.back().id;
    selectAt(r, 3, 0); selectAt(r, 1, 3);
    menu(r, pk::F4, 0, 2);
    T("C02 Perpendicular", countKind(r, geo::Kind::Line) == 2 && geo::find(S(r), S(r).objects.back().id)->def == geo::Def::Perpendicular);
    selectAt(r, 3, 0);
    menu(r, pk::F4, 0, 3);
    const geo::Id mid = S(r).objects.back().id;
    T("C03 Midpoint", near(px(r, mid), 2) && near(py(r, mid), 0));
    selectAt(r, 3, 0); selectAt(r, 1, 3);
    menu(r, pk::F4, 0, 6);
    T("C06 Parallel", geo::find(S(r), S(r).objects.back().id)->def == geo::Def::Parallel);
    redraw(r);
    selectAt(r, 3, 3); selectAt(r, 1, 2);                 // parallela e perpendicolare per C
    menu(r, pk::F4, 0, 4);
    T("C04 Intersection", geo::find(S(r), S(r).objects.back().id)->def == geo::Def::Intersection && near(px(r, S(r).objects.back().id), 1) && near(py(r, S(r).objects.back().id), 3));
    segment(r, 0, 0, 0, 4);                               // lato AD: angolo retto con AB in A
    selectAt(r, 3, 0); selectAt(r, 0, 1);
    menu(r, pk::F4, 0, 5);
    T("C05 Angle Bisector: due rette", S(r).objects.back().def == geo::Def::AngleBisector && S(r).objects[S(r).objects.size() - 2].def == geo::Def::AngleBisector);
    segment(r, 6, 0, 8, 0, 6);
    menu(r, pk::F4, 0, 7);
    T("C07 Tangent: Select Curve", scr(r) == "tangent");
    exeAt(r, 6, 2);
    T("C07 tangente", S(r).objects.back().def == geo::Def::Tangent && scr(r) == "main");
    selectAt(r, 3, 0); selectAt(r, 0, 3);
    menu(r, pk::F4, 0, 8);
    T("C08 Attached Angle: Select Display Position", scr(r) == "angle-position");
    exeAt(r, 0.5, 0.5);
    T("C08 angolo posato", S(r).objects.back().kind == geo::Kind::Measure && S(r).objects.back().flag2 == 1);
    redraw(r);
    T("C08 testo dell'angolo", geo::measureText(S(r), S(r).objects.back()).find("Angolo=90") == 0);
    // C09 spostando B il punto medio segue
    selectAt(r, 4, 0);
    r.type("v");
    for (int i = 0; i < 8; ++i) r.key(pk::Right);
    r.key(pk::Enter);
    T("C09 il punto medio segue", near(px(r, mid), 3));
    (void)ab; (void)c;
    // C11 selezione sbagliata
    r.key(pk::Break);
    menu(r, pk::F4, 0, 3);
    T("C11 selezione sbagliata: messaggio", r.error() && r.errorTitle() == "Argument ERROR");
    r.closeError();
}

// ---------------------------------------------------------------- Transform
void transforms(Rig& r) {
    fresh(r);
    box(r, 1, 1, 1, 3, 2);                                // triangolo A(1,1) B(3,1) C(1.5,2)
    const geo::Id a = S(r).objects[0].id;
    segment(r, 0, -3, 0, 3, 3);                           // asse x = 0 (retta)
    selectAt(r, 1, 1); r.type("x");
    menu(r, pk::F5, 0, 1);
    T("T01 Reflection: Select Axis", scr(r) == "axis");
    exeAt(r, 0, -2);
    T("T01 immagini", countKind(r, geo::Kind::Polygon) == 2 && scr(r) == "main");
    const geo::Id a1 = S(r).objects[S(r).objects.size() - 7].id;
    T("T01 A' riflesso", near(px(r, a1), -1) && near(py(r, a1), 1) && geo::find(S(r), a1)->label == "A'");
    selectAt(r, 1, 1);
    r.type("v"); r.key(pk::Up); r.key(pk::Enter);
    T("T07 legata all'originale", near(py(r, a1), 1.25));
    r.key(pk::Break);
    selectAt(r, 3, 1); r.type("x");
    menu(r, pk::F5, 0, 2);
    r.enter("2"); r.enter("-1");
    T("T02 Translation con i valori", countKind(r, geo::Kind::Polygon) == 3);
    segment(r, -4, -4, -3, -2, 5);                        // vettore (1, 2)
    selectAt(r, 3, 1); r.type("x");
    menu(r, pk::F5, 0, 3);
    exeAt(r, -3.5, -3);
    T("T03 Trans(Sel Vec)", countKind(r, geo::Kind::Polygon) == 4);
    const double area0 = val(r, geo::MType::Area, {ofKind(r, geo::Kind::Polygon)[0]});   // A spostato da T07
    selectAt(r, 3, 1); r.type("x");
    menu(r, pk::F5, 0, 4);
    T("T04 Sel Rotation Center", scr(r) == "center");
    exeAt(r, 0, 0);
    T("T04 angolo", scr(r) == "ask");
    r.enter("90");
    const geo::Id poly = S(r).objects.back().id;
    T("T04 rotazione di 90 gradi attorno all'origine", countKind(r, geo::Kind::Polygon) == 5 && near(val(r, geo::MType::Area, {poly}), area0) &&
                                                          near(px(r, S(r).objects[S(r).objects.size() - 6].id), -1) && near(py(r, S(r).objects[S(r).objects.size() - 6].id), 3));
    selectAt(r, 3, 1); r.type("x");
    menu(r, pk::F5, 0, 5);
    exeAt(r, 0, 0);
    r.enter("2");
    T("T05 Dilation k=2: area x4", near(val(r, geo::MType::Area, {S(r).objects.back().id}), 4 * area0));
    selectAt(r, 3, 1); r.type("x");
    menu(r, pk::F5, 0, 5);
    exeAt(r, 0, 0);
    r.enter("20");
    T("T05 fattore fuori intervallo", r.error());
    r.closeError();
    selectAt(r, 3, 1); r.type("x");
    menu(r, pk::F5, 0, 6);
    exeAt(r, 5, 5);                                       // centro nuovo: disegnato
    T("T06 Symmetry con un centro nuovo", countKind(r, geo::Kind::Polygon) == 7 && scr(r) == "main");
    selectAt(r, 3, 1);
    menu(r, pk::F5, 0, 2);
    r.enter("0"); r.enter("1");
    T("T08 selezione parziale: solo il punto", S(r).objects.back().kind == geo::Kind::Point);
    (void)a;
    fresh(r);
    menu(r, pk::F3, 1, 7); r.enter("x^2");
    menu(r, pk::F2, 0, 2);
    menu(r, pk::F5, 0, 2);
    r.enter("1"); r.enter("1");
    T("T09 f(x) non trasformabile", r.error() && r.errorDetail().find("f(x)") != std::string::npos);
    r.closeError();
}

// ---------------------------------------------------------------- casella di misura
void measureBox(Rig& r) {
    GeometryApp* g = fresh(r);
    segment(r, 0, 0, 3, 0);
    segment(r, 0, 0, 0, 4);
    const geo::Id ab = ofKind(r, geo::Kind::Segment)[0];
    selectAt(r, 1.5, 0);
    r.key(pk::F8);
    redraw(r);
    T("M01 OPTN apre la casella", g->boxFocused() && g->boxText() == "Lunghezza: 3");
    r.key(pk::Right); redraw(r);
    T("M01 tipo seguente", g->boxText().find("Pendenza") == 0);
    r.key(pk::Left); redraw(r);
    r.key(pk::Esc);
    T("M01 EXIT: casella visibile, puntatore libero", !g->boxFocused() && !g->boxText().empty());
    selectAt(r, 0, 2);
    redraw(r);
    T("M01 la casella segue la selezione", g->boxText().find("Angolo") == 0);
    r.key(pk::F8);
    r.key(pk::F1);
    T("M01 F1 TYPE: elenco dei tipi", scr(r) == "menu");
    r.type("2");
    redraw(r);
    T("M07 supplementare", g->boxText().find("Supplementare: 90") == 0);
    r.key(pk::F8);
    T("M01 OPTN chiude", g->boxText().empty());
    r.key(pk::Break);
    selectAt(r, 1.5, 0);
    r.key(pk::Esc);
    T("M01 EXIT a riposo apre la casella (fx-CG50)", g->boxFocused());
    r.type("5");
    T("M18 valore scritto", scr(r) == "ask");
    r.key(pk::Enter);
    r.settle();
    redraw(r);
    T("M18 lunghezza 5 e bloccata", near(val(r, geo::MType::Distance, {ab}), 5, 1e-9) && geo::isLocked(S(r), geo::MSpec{geo::MType::Distance, {ab}}) &&
                                    g->boxText().find("[bloccata]") != std::string::npos);
    r.key(pk::F2);
    T("M19 F2: sblocca", S(r).locks.empty());
    r.key(pk::F2);
    r.settle();
    T("M19 F2: blocca il valore attuale", S(r).locks.size() == 1);
    r.key(pk::F3);
    T("M20 PASTE", S(r).objects.back().kind == geo::Kind::Measure && geo::measureText(S(r), S(r).objects.back()) == "Lunghezza=5");
    r.key(pk::F5);
    T("M22 F5 LABEL: modulo", scr(r) == "form");
    r.key(pk::Esc);                                       // EXIT annulla il modulo di testo
    T("M22 annullato", scr(r) == "box");
    r.key(pk::Esc);
    T("M01 EXIT nella casella: fuoco al disegno", !g->boxFocused());
    // M19: spostando un estremo la lunghezza bloccata resta
    const geo::Id b = geo::find(S(r), ab)->parents[1];
    const double bx = px(r, b), by = py(r, b);
    r.key(pk::Break);
    selectAt(r, bx, by);
    r.type("v"); r.key(pk::Up); r.key(pk::Up); r.key(pk::Enter);
    T("M19 resta bloccata spostando", near(val(r, geo::MType::Distance, {ab}), 5, 1e-8));
    T("M20 misura incollata aggiornata", geo::measureText(S(r), *geo::find(S(r), ofKind(r, geo::Kind::Measure)[0])) == "Lunghezza=5");
    // M21 vincoli incompatibili
    fresh(r);
    segment(r, 0, 0, 3, 0);
    selectAt(r, 0, 0);
    r.key(pk::F8); r.type("0,0"); r.key(pk::Enter); r.settle();
    r.key(pk::Esc); r.key(pk::Break);
    selectAt(r, 3, 0);
    r.key(pk::F8); r.type("3,0"); r.key(pk::Enter); r.settle();
    r.key(pk::Esc); r.key(pk::Break);
    T("M02 coordinate scritte e bloccate", S(r).locks.size() == 2);
    const geo::State before = S(r);
    selectAt(r, 1.5, 0);
    r.key(pk::F8); r.type("7"); r.key(pk::Enter); r.settle();
    T("M21 vincoli incompatibili: errore", r.error() && r.errorDetail().find("incompatibili") != std::string::npos);
    r.closeError();
    T("M21 nessun cambiamento", S(r) == before);
}

// ---------------------------------------------------------------- Option e Properties
void options(Rig& r) {
    GeometryApp* g = fresh(r);
    menu(r, pk::F2, 1, 1);
    T("O01 Text: modulo", scr(r) == "form");
    r.enter("Ciao");
    T("O01 Select Display Position", scr(r) == "place-text");
    exeAt(r, 1, 1);
    T("O01 testo posato", S(r).objects.back().kind == geo::Kind::Text && S(r).objects.back().text == "Ciao");
    menu(r, pk::F3, 0, 1); exeAt(r, -2, -2); r.key(pk::Esc);
    const geo::Id p = S(r).objects.back().id;
    T("O02 etichetta automatica", geo::find(S(r), p)->label == "A");
    selectAt(r, -2, -2);
    menu(r, pk::F2, 1, 2);
    r.key(pk::Backspace);
    r.enter("P");
    T("O02 Label", geo::find(S(r), p)->label == "P");
    // misura incollata e Expression
    r.key(pk::F8); r.key(pk::F3); r.key(pk::Esc);          // coordinate di P incollate (@1)
    r.key(pk::Break);
    menu(r, pk::F2, 1, 3);
    r.enter("@1Y*3");
    exeAt(r, 2, 3);
    const geo::Object& ex = S(r).objects.back();
    T("O03 Expression", ex.kind == geo::Kind::Expression && geo::measureText(S(r), ex) == "@1Y*3=-6");
    // O04 Number Format di una misura incollata
    const geo::Id meas = ofKind(r, geo::Kind::Measure)[0];
    int sx, sy;
    (void)sx; (void)sy;
    S(r).objects[static_cast<std::size_t>(geo::indexOf(S(r), meas))].vars = {4, -4};
    r.home(); g = openGeo(r);
    selectAt(r, 4.25, -4.25);
    T("O04 selezione della misura incollata", g->selection().size() == 1 && g->selection()[0] == meas);
    menu(r, pk::F2, 1, 4);
    T("O04 Number Format: elenco", scr(r) == "menu");
    r.type("5");
    T("O04 Fix4", geo::find(S(r), meas)->fmt == 4);
    r.key(pk::Break);
    // O07 Hide e O06 Show All
    selectAt(r, -2, -2);
    menu(r, pk::F2, 1, 7);
    T("O07 Hide", geo::find(S(r), p)->hidden && geo::hitTest(S(r), -2, -2, 0.25) == geo::NoId);
    menu(r, pk::F2, 1, 6);
    T("O06 Show All", !geo::find(S(r), p)->hidden);
    // O05 Clr Constraint
    fresh(r);
    box(r, 3, 0, 0, 4, 2);
    menu(r, pk::F2, 1, 5);
    T("O05 Clr Constraint", S(r).objects[2].def == geo::Def::Free && S(r).objects[3].def == geo::Def::Free);
    // O14 FORMAT (SHIFT 5): colore e stile
    selectAt(r, 2, 0);
    r.key(pk::F7); r.type("5");
    T("O14 FORMAT", scr(r) == "form");
    r.key(pk::Down); r.key(pk::Right);                     // Line Color: Blue -> Red
    r.key(pk::Up); r.key(pk::Right);                       // Line Style: Thick
    r.key(pk::Esc);
    const geo::Id side = ofKind(r, geo::Kind::Segment)[0];
    T("O14 stile salvato", geo::find(S(r), side)->style.line == 2 && geo::find(S(r), side)->style.lineStyle == geo::LineStyle::Thick);
    r.key(pk::Break);
    redraw(r);
    T("O14 pixel nel colore scelto (tratto spesso)", pixels(Red) > 150);
    // area colorata e Area Calc (O08)
    selectAt(r, 2, 1);                                    // interno del rettangolo
    g = r.active<GeometryApp>();
    T("O08 selezione dell'interno", g->selection().size() == 1 && geo::find(S(r), g->selection()[0])->kind == geo::Kind::Polygon);
    menu(r, pk::F2, 1, 8);
    T("O08 senza Area Color: errore", r.error());
    r.closeError();
    r.key(pk::F7); r.type("5");
    r.key(pk::Down); r.key(pk::Down); r.key(pk::Right);    // Area Color: Black
    r.key(pk::Esc);
    menu(r, pk::F2, 1, 8);
    T("O08 Area Calc", S(r).objects.back().kind == geo::Kind::Measure && geo::measureText(S(r), S(r).objects.back()) == "Area=8");
    // O09, O10 ordine di disegno
    r.key(pk::Break);
    selectAt(r, 2, 0);
    menu(r, pk::F2, 2, 1);
    T("O09 to the front", geo::find(S(r), side)->z > 0);
    menu(r, pk::F2, 2, 2);
    T("O09 to the back", geo::find(S(r), side)->z < 0);
    menu(r, pk::F2, 2, 3);
    T("O10 All TEXT", S(r).objects.back().z > 0);
    // O12 Store Picture, O11 Recall e Fade, O13 Clear Picture
    menu(r, pk::F2, 2, 5);
    r.enter("3");
    r.settle();
    T("O12 Store Picture in Pict 3", fileExists("PICOCALC/PICT/PICT03.pcpict") && !r.error());
    menu(r, pk::F2, 2, 6);
    r.enter("3");
    T("O11 Recall Picture come sfondo", S(r).background == "PICT03.pcpict" && !r.error());
    menu(r, pk::F2, 2, 4);
    r.enter("60");
    T("O11 Fade I/O", S(r).fade == 60);
    menu(r, pk::F2, 2, 7);
    T("O13 Clear Picture", S(r).background.empty());
    menu(r, pk::F2, 2, 6);
    r.enter("9");
    T("O11 Pict mancante: errore", r.error());
    r.closeError();
}

// ---------------------------------------------------------------- Animate
void animate(Rig& r) {
    GeometryApp* g = fresh(r);
    segment(r, 0, 0, 4, 0);                               // A B
    menu(r, pk::F3, 0, 1); exeAt(r, 1, 2); r.key(pk::Esc);   // C
    const geo::Id c = S(r).objects.back().id;
    segment(r, 1, 2, -2, 2);                              // C D
    const geo::Id cd = S(r).objects.back().id;
    selectAt(r, 1, 2); selectAt(r, 2, 0);
    menu(r, pk::F6, 0, 1);
    T("A01 Add Animation", S(r).animations.size() == 1 && S(r).animations[0].point == c);
    menu(r, pk::F6, 0, 2);
    T("A02 Replace Animation", S(r).animations.size() == 1);
    r.key(pk::Break);
    selectAt(r, 1, 2);
    menu(r, pk::F6, 0, 3);
    T("A03 Trace", S(r).traced.size() == 1 && S(r).traced[0] == c);
    r.key(pk::Break);
    // A07 Add Table: la lunghezza di CD dalla casella
    selectAt(r, -0.5, 2);
    r.key(pk::F8);
    r.key(pk::F4);
    r.key(pk::Esc);
    T("A07 TABLE dalla casella", S(r).columns.size() == 1);
    menu(r, pk::F6, 0, 7);
    T("A07 Add Table dal menu", S(r).columns.size() == 2);
    r.key(pk::F8); r.key(pk::F8);
    r.key(pk::Break);
    // A04 Edit Animation
    menu(r, pk::F6, 0, 4);
    T("A04 Edit Animation", scr(r) == "animations");
    r.key(pk::Enter);
    T("A04 modulo", scr(r) == "form");
    r.key(pk::Backspace); r.enter("5");
    r.key(pk::Esc);
    T("A04 Steps", S(r).animations[0].steps == 5 && scr(r) == "animations");
    r.key(pk::Esc);
    // A05 Go (once)
    menu(r, pk::F6, 0, 5);
    T("A05 Go (once): in corso", scr(r) == "animate");
    r.settle();
    redraw(r);
    T("A05 finita, punti a posto", scr(r) == "main" && near(px(r, c), 1) && near(py(r, c), 2));
    T("A03 traccia disegnata", pixels(Warn) >= 5 * 4);
    T("A08 tabella riempita", S(r).tableRows == 5 && near(S(r).table[0], std::hypot(-2.0, 2.0)) && near(S(r).table[4 * 2], std::hypot(4 + 2.0, 2.0)));
    // A06 Go (repeat) con Times 0 e A12 AC
    menu(r, pk::F6, 0, 6);
    for (int i = 0; i < 200; ++i) { board::host::advance(50); if (r.fw.step()) redraw(r); }
    T("A06 Go (repeat): continua", scr(r) == "animate");
    r.key(pk::Break);
    T("A12 AC la ferma, punti a posto", scr(r) == "main" && near(px(r, c), 1) && !r.error());
    // A08 Display Table, A09 LIST, A11 DELETE
    S(r).table.clear(); S(r).tableRows = 0;
    menu(r, pk::F6, 0, 8);
    r.settle();
    T("A08 Display Table (calcolata a passi)", scr(r) == "table" && S(r).tableRows == 5);
    r.key(pk::F1); r.type("1");
    r.enter("3");
    T("A09 STORE > LIST 3", r.ws().lists[2].size() == 5 && near(r.ws().lists[2][0], std::hypot(-2.0, 2.0)));
    r.key(pk::F2); r.type("1");
    T("A11 DELETE di una colonna", S(r).columns.size() == 1);
    r.key(pk::F1); r.type("2");
    T("A10 STORE > STAT: Statistics con List 1", r.fw.apps().activeId() == AppId::Statistics && r.ws().lists[0].size() == 5);
    g = openGeo(r);
    menu(r, pk::F6, 0, 8);
    r.key(pk::F2); r.type("2");
    T("A11 DEL-ALL chiede conferma", scr(r) == "confirm");
    r.key(pk::Enter);
    T("A11 tabella vuota", S(r).columns.empty() && scr(r) == "main");
    // A04 DELETE della riga
    menu(r, pk::F6, 0, 4);
    r.key(pk::F5);
    T("A04 DELETE", S(r).animations.empty());
    r.key(pk::Esc);
    menu(r, pk::F6, 0, 5);
    T("A05 senza animazioni: errore", r.error());
    r.closeError();
    (void)cd; (void)g;
}

// ---------------------------------------------------------------- SET UP, persistenza, limiti
void setupAndRecord(std::unique_ptr<Rig>& rp) {
    Rig& r = *rp;
    GeometryApp* g = fresh(r);
    segment(r, 0, 0, 3, 0);
    r.key(pk::F10);
    T("S01 SET UP (SHIFT MENU)", scr(r) == "form");
    r.key(pk::Down); r.key(pk::Down); r.key(pk::Right); r.key(pk::Right);   // Length Unit: cm
    r.key(pk::Esc);
    selectAt(r, 1.5, 0);
    r.key(pk::F8);
    redraw(r);
    T("S01 unita' di lunghezza nella casella", g->boxText() == "Lunghezza: 3 cm");
    r.type("4"); r.key(pk::Enter); r.settle();
    r.key(pk::Esc); r.key(pk::Break);
    menu(r, pk::F3, 0, 1); exeAt(r, 0, 2); r.key(pk::Esc);
    selectAt(r, 0, 2);
    selectAt(r, 2, 0);
    menu(r, pk::F6, 0, 1);
    r.home();
    CHECK(r.fw.saveNow());
    rp.reset();
    rp = std::make_unique<Rig>();
    Rig& r2 = *rp;
    T("P03 dopo il riavvio: oggetti, blocchi, animazioni e opzioni", S(r2).objects.size() == 4 && S(r2).locks.size() == 1 && S(r2).animations.size() == 1 &&
                                                                    S(r2).options.lengthUnit == 2 && near(val(r2, geo::MType::Distance, ofKind(r2, geo::Kind::Segment)), 4, 1e-9));
    // P04 Memory ERROR senza perdita
    g = fresh(r2);
    while (S(r2).objects.size() + 1 < geo::maxObjects()) geo::addPoint(S(r2), static_cast<double>(S(r2).objects.size() % 40), 9);
    r2.home(); g = openGeo(r2);
    const std::size_t n = S(r2).objects.size();
    menu(r2, pk::F3, 0, 2);
    exeAt(r2, -3, -3);
    exeAt(r2, -1, -3);
    T("P04 Memory ERROR", r2.error() && r2.errorTitle() == "Memory ERROR");
    r2.closeError();
    T("P04 nulla perso", S(r2).objects.size() >= n && S(r2).objects.size() <= n + 1);
    r2.home();
    S(r2) = geo::State{};
    (void)g;
}
} // namespace

int main() {
    board::init();
    storage::RecordStore(board::storageFlash()).eraseAll();
    auto r = std::make_unique<Rig>();
    const std::pair<const char*, void (*)(Rig&)> groups[] = {{"keys", keysAndMenus}, {"file", fileMenu}, {"view", views}, {"edit", edits}, {"draw", draws},
                                                             {"construct", constructs}, {"transform", transforms}, {"measure", measureBox}, {"options", options},
                                                             {"animate", animate}};
    for (const auto& gr : groups) {
        try { gr.second(*r); } catch (const std::exception& e) { ::testutil::report(false, gr.first, __FILE__, __LINE__, std::string("eccezione: ") + e.what()); }
        if (r->error()) { ::testutil::report(false, gr.first, __FILE__, __LINE__, "errore rimasto aperto: " + err(*r)); r->closeError(); }
    }
    try { setupAndRecord(r); } catch (const std::exception& e) { ::testutil::report(false, "record", __FILE__, __LINE__, e.what()); }
    return testutil::finish("geometry_flow_test");
}
