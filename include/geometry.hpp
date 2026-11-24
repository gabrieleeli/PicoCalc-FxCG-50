#pragma once
// Nucleo di Geometry (v1.19.0: parita' con il capitolo 14 del manuale fx-CG50;
// solo Pimoroni Pico Plus 2 W). Descrizione del comportamento in
// docs/reports/ANALISI_FONTI_v1.19.0.md; nessun codice o testo Casio.
//
// Modello: oggetti con un id stabile, un tipo (Kind), una definizione (Def),
// i genitori e i propri scalari liberi. I genitori hanno sempre un id minore
// e gli oggetti sono in ordine di id: l'ordine del vettore e' un ordine
// topologico del grafo delle dipendenze. La geometria calcolata (Shape) e'
// una cache: recompute() la rifa' tutta o solo per i discendenti di cio' che
// e' cambiato (ricalcolo incrementale). Un oggetto che non si puo' calcolare
// (rette parallele, cerchi concentrici, punti coincidenti) e' "indefinito":
// non si disegna e torna appena la configurazione lo permette.
//
// Funzioni pure su State: il disegno, i tasti e i file sono nel controller
// (src/apps/geometry_app.cpp). Gli errori sono MathError con il messaggio
// della fx-CG50 ("Memory ERROR", "Argument ERROR", "Math ERROR").
#include "step_task.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fx::geo {

using Id = std::uint16_t;
constexpr Id NoId = 0;

// ---- limiti (P04) ---------------------------------------------------------------------
std::size_t maxObjects() noexcept;                 // 1000 con la PSRAM, 250 solo SRAM
constexpr std::size_t MaxVertices = 50;            // vertici di un poligono
constexpr std::size_t MaxAnimations = 16;
constexpr std::size_t MaxColumns = 26;             // colonne della tabella delle animazioni
constexpr std::size_t MaxRows = 100;               // righe (= Steps massimo)
constexpr std::size_t MaxLocks = 64;
constexpr std::size_t MaxText = 31;                // Text
constexpr std::size_t MaxLabel = 14;               // etichette e nomi delle misure incollate
constexpr std::size_t MaxFormula = 128;            // f(x) ed Expression
constexpr int MinSteps = 2, MaxSteps = 100;
constexpr double MinT = -10, MaxT = 10;            // t0, t1 delle animazioni (funzioni: x)
constexpr double MinDilation = 0.1, MaxDilation = 10;
constexpr int MinNgon = 3, MaxNgon = 12;

// ---- tolleranze dichiarate (ANALISI_FONTI §4.4) -----------------------------------
constexpr double EpsParallel = 1e-12;              // |sin| tra le direzioni: rette parallele
constexpr double EpsTangent = 1e-9;                // |d - r| <= eps*s: tangenza (radice doppia)
constexpr double EpsConcentric = 1e-12;            // |C1C2| <= eps*s: cerchi concentrici
constexpr double EpsParam = 1e-9;                  // parametro dei segmenti in [-eps, 1+eps]
constexpr double EpsAngle = 1e-9;                  // ampiezza degli archi (rad)
constexpr double EpsCoincident = 1e-12;            // |AB| <= eps*s: punti coincidenti
constexpr double EpsYesNo = 1e-9;                  // incidenza, tangenza, congruenza
constexpr double EpsSolve = 1e-10;                 // residuo del risolutore dei blocchi

enum class Kind : std::uint8_t { Point = 0, Segment, Line, Ray, Vector, Circle, Arc, Polygon, Function, Text, Measure, Expression, Transform, Count };
enum class Def : std::uint8_t {
    // punti
    Free = 0, On, Midpoint, Intersection, Image, RectVertex, SquareVertex, IsoApex, NgonVertex, ArcEnd,
    // linee (Segment, Line, Ray, Vector)
    TwoPoints, PerpBisector, Perpendicular, Parallel, AngleBisector, Tangent, ImageCurve,
    // circonferenze, archi, poligoni, funzioni
    CircleCP, ArcCSE, Semicircle, Arc3, PolygonV, FunctionX,
    // testi
    TextAt, MeasureAt, ExpressionAt,
    // trasformazioni
    Reflect, Translate, TranslateVec, Rotate, Dilate, Symmetry,
    Count
};
const char* kindName(Kind k) noexcept;             // "Punto", "Segmento"...

// Colori della tavolozza del progetto (0 nero, 1 blu, 2 rosso, 3 magenta, 4 verde, 5 ciano, 6 giallo).
constexpr std::uint8_t ColorCount = 7, NoArea = 0xFF;
enum class LineStyle : std::uint8_t { Normal = 0, Thick, Thin, Broken, Dot, Count };
struct Style {
    std::uint8_t line{1};                          // colore delle linee (blu)
    LineStyle lineStyle{LineStyle::Normal};
    std::uint8_t area{NoArea};                     // colore dell'area (NoArea = Clear)
    std::uint8_t paint{0};                         // 0 Normal, 1 Lighter
    std::uint8_t chars{0};                         // colore del testo e delle etichette
    bool operator==(const Style& o) const noexcept {
        return line == o.line && lineStyle == o.lineStyle && area == o.area && paint == o.paint && chars == o.chars;
    }
};

// Geometria calcolata. Point/Text/Measure: a[0], a[1]. Linee: (a0, a1) -> (a2, a3).
// Circle: centro a0, a1, raggio a2, angolo del punto che la definisce a3.
// Arc: centro, raggio, angolo iniziale a3, ampiezza a4 (> 0 antiorario, < 0 orario).
// Transform: la mappa affine x' = a0 x + a1 y + a4, y' = a2 x + a3 y + a5.
struct Shape {
    bool ok{false};
    double a[6]{};
};
struct FunctionCache;                               // albero dell'espressione di f(x) (geometry.cpp)

struct Object {
    Id id{NoId};
    Kind kind{Kind::Point};
    Def def{Def::Free};
    std::uint8_t flag{0};      // ramo (Intersection, AngleBisector), verso (archi: 1 orario), n (NgonVertex),
                               // tipo di misura (MeasureAt), tipo di linea (ImageCurve: Kind della linea)
    std::uint8_t flag2{0};     // k (NgonVertex), vertice (RectVertex/SquareVertex), segno d'angolo (MeasureAt)
    std::vector<Id> parents;
    std::vector<double> vars;  // scalari liberi (Free x,y; On t; RectVertex w; IsoApex h; ArcEnd phi;
                               // Translate dx,dy; Rotate angolo in radianti; Dilate k; testi: posizione)
    std::string label;         // etichetta (A, B...; nome della misura incollata)
    std::string text;          // Text, f(x), Expression
    Style style;
    bool hidden{false};
    bool showLabel{true};
    std::uint16_t figure{0};   // figura di Draw / Draw Spec / Transform (Select Figure)
    std::int16_t z{0};         // ordine di disegno (to the front / to the back)
    std::uint8_t fmt{0xFF};    // Number Format della misura incollata (0xFF = quello delle Option)
    std::uint16_t seq{0};      // numero @n della misura incollata (Expression)
    // ---- cache di esecuzione (non salvata, non confrontata)
    Shape g;
    std::shared_ptr<FunctionCache> fn;
    bool sameData(const Object& o) const noexcept;
};

// ---- misure (casella di misura, 14-37...14-52) ----------------------------------------
enum class MType : std::uint8_t {
    Coordinates = 0, Distance, Slope, Direction, Equation, Components, Radius, Circumference, Area, Perimeter,
    Angle, Supplementary, Incidence, Tangency, Congruence, RotationAngle, DilationScale, Label, Expression, Count
};
const char* mtypeName(MType t) noexcept;            // nome della casella ("Coordinate", "Lunghezza"...)
bool lockable(MType t) noexcept;
struct MSpec {
    MType type{MType::Coordinates};
    std::vector<Id> ops;
    bool operator==(const MSpec& o) const noexcept { return type == o.type && ops == o.ops; }
};
struct Measurement {
    bool ok{false};            // definita
    int count{0};              // numeri: 0 (solo testo), 1 o 2
    double v[2]{};             // nell'unita' delle Option (angoli in gradi o radianti)
    bool yesNo{false}, yes{false};
    std::string text;          // equazione, etichetta, espressione
};
struct Lock {
    MSpec m;
    double v[2]{};             // valore canonico (angoli in radianti)
    double aux{0};             // tangenza tra circonferenze: 0 esterna, 1 interna
    bool operator==(const Lock& o) const noexcept { return m == o.m && v[0] == o.v[0] && v[1] == o.v[1] && aux == o.aux; }
};

// ---- animazioni (14-55...14-66) -------------------------------------------------------
struct Animation {
    Id point{NoId}, path{NoId};
    std::uint8_t steps{20};
    double t0{0}, t1{1};
    std::uint16_t times{0};    // Go (repeat): 0 = finche' AC
    bool operator==(const Animation& o) const noexcept {
        return point == o.point && path == o.path && steps == o.steps && t0 == o.t0 && t1 == o.t1 && times == o.times;
    }
};
struct TableColumn {
    MSpec m;
    std::uint8_t comp{0};      // componente (coordinate, componenti: 0 = x, 1 = y)
    std::string name;
    bool operator==(const TableColumn& o) const noexcept { return m == o.m && comp == o.comp && name == o.name; }
};

// ---- opzioni (File > Option, SET UP) e vista -------------------------------------------
// Number Format: 0..9 Fix0..Fix9, 10..19 Sci1..Sci10, 20 Norm.
constexpr std::uint8_t FmtFix0 = 0, FmtSci1 = 10, FmtNorm = 20, FmtDefault = 2;
enum class Grid : std::uint8_t { Off = 0, On, Line };
enum class Axes : std::uint8_t { Off = 0, On, Scale };
constexpr std::uint8_t LengthUnitCount = 9;          // Off, mm, cm, m, km, inch, feet, yard, mile
const char* lengthUnitName(std::uint8_t u) noexcept;
struct Options {
    std::uint8_t numFormat{FmtDefault};
    std::uint8_t angle{0};                           // 0 Deg, 1 Rad
    std::uint8_t lengthUnit{0};
    Grid grid{Grid::Off};
    double gridSpace{1};                             // 0.01 ... 1000
    Axes axes{Axes::On};
    bool operator==(const Options& o) const noexcept {
        return numFormat == o.numFormat && angle == o.angle && lengthUnit == o.lengthUnit && grid == o.grid && gridSpace == o.gridSpace && axes == o.axes;
    }
};
struct View {
    double cx{0}, cy{0};                             // centro della finestra
    double scale{24};                                // pixel per unita' (uguale in x e in y)
    bool operator==(const View& o) const noexcept { return cx == o.cx && cy == o.cy && scale == o.scale; }
};
constexpr double MinScale = 1e-4, MaxScale = 1e6;

struct State {
    std::vector<Object> objects;                     // in ordine di id (topologico)
    std::vector<Lock> locks;
    std::vector<Animation> animations;
    std::vector<Id> traced;
    std::vector<TableColumn> columns;
    std::vector<double> table;                       // righe x colonne (riga per riga)
    std::uint16_t tableRows{0};
    View view;
    Options options;
    std::string file;                                // .pcgeo aperto ("" = nessuno)
    std::string background;                          // immagine di sfondo (.pcpict, "" = nessuna)
    std::uint8_t fade{0};                            // Fade I/O: 0..100 %
    Id nextId{1};
    std::uint16_t nextFigure{1}, nextLabel{0}, nextSeq{1};
    bool operator==(const State& o) const noexcept;
};

// ---- oggetti e grafo ---------------------------------------------------------------
int indexOf(const State& s, Id id) noexcept;       // -1 se non c'e' (ricerca binaria: ordine di id)
const Object* find(const State& s, Id id) noexcept;
Object* find(State& s, Id id) noexcept;
bool defined(const State& s, Id id) noexcept;
bool isLineLike(Kind k) noexcept;                   // Segment, Line, Ray, Vector
bool isCurve(Kind k) noexcept;                      // linee, Circle, Arc, Function
// Ricalcolo: tutto, oppure solo gli oggetti cambiati e i loro discendenti. Restituisce
// il numero di oggetti valutati (C09).
std::size_t recompute(State& s);
std::size_t recompute(State& s, const std::vector<Id>& changed);
bool isAncestor(const State& s, Id ancestor, Id id);
std::vector<Id> descendants(const State& s, const std::vector<Id>& ids);   // ids compresi
std::string validate(const State& s, const Object& o);                      // "" se la definizione e' coerente
std::string nextPointLabel(State& s);                // A..Z, A1..Z1...
std::string shortName(const State& s, Id id);         // etichetta, o i vertici ("AB", "ABC") delle figure

// ---- Draw / Draw Spec (14-11...14-18) --------------------------------------------------
Id addPoint(State& s, double x, double y);                       // punto libero
Id addPointOn(State& s, Id curve, double x, double y);            // punto legato a una curva (D01)
Id addLine(State& s, Kind kind, Id a, Id b);                     // Segment, Line, Ray, Vector
Id addCircle(State& s, Id center, Id through);
Id addArc(State& s, Id center, Id start, double ex, double ey);   // crea anche il punto finale (ArcEnd)
Id addSemicircle(State& s, Id a, Id b);
Id addArc3(State& s, Id a, Id m, Id b);
// Poligono: lati e (se chiuso) l'oggetto poligono della stessa figura; restituisce il
// poligono (chiuso) o l'ultimo lato (spezzata aperta).
Id addPolygon(State& s, const std::vector<Id>& vertices, bool closed);
enum class Spec : std::uint8_t { Triangle, Isosceles, Rectangle, Square, Ngon };
// Figura dal riquadro (x0, y0)-(x1, y1); `fit` e' la finestra visibile per i riquadri troppo piccoli (D17).
struct Box { double x0{0}, y0{0}, x1{0}, y1{0}; };
Id addSpec(State& s, Spec spec, Box box, Box fit, int n = 0);
Id addFunction(State& s, const std::string& expression);
Id addText(State& s, double x, double y, const std::string& text);
Id addMeasureText(State& s, const MSpec& m, double x, double y);   // Paste (M20) e Attached Angle (C08)
Id addExpression(State& s, double x, double y, const std::string& formula);  // O03
bool evalFunction(const State& s, const Object& f, double x, double& y);
void setFunction(State& s, Id f, const std::string& expression);

// ---- Construct (14-22...14-26) ---------------------------------------------------------
enum class Construct : std::uint8_t { PerpBisector, Perpendicular, Midpoint, Intersection, AngleBisector, Parallel, Tangent, AttachedAngle };
// Selezione nell'ordine in cui e' stata fatta; (x, y) e' il puntatore (Tangent, Attached Angle).
std::vector<Id> construct(State& s, Construct c, const std::vector<Id>& selection, double x = 0, double y = 0);

// ---- Transform (14-26...14-29) ---------------------------------------------------------
enum class TType : std::uint8_t { Reflection, Translation, TransVec, Rotation, Dilation, Symmetry };
// ref: asse, vettore o centro; p0, p1: dx, dy / angolo nell'unita' delle Option / k.
// Restituisce gli oggetti immagine (punti e figure), legati all'originale.
std::vector<Id> transform(State& s, TType t, const std::vector<Id>& selection, Id ref, double p0 = 0, double p1 = 0);

// ---- Edit ------------------------------------------------------------------------------
std::size_t remove(State& s, const std::vector<Id>& ids);           // con i discendenti (E05)
std::vector<Id> figureOf(const State& s, const std::vector<Id>& ids); // Select Figure (E04)
void clearConstraints(State& s);                                     // Clr Constraint (O05)
void toFront(State& s, const std::vector<Id>& ids);
void toBack(State& s, const std::vector<Id>& ids);
void textsToFront(State& s);                                         // All TEXT
// Spostamento: i punti liberi della selezione (o il grado di liberta' di un vertice
// vincolato) e poi i blocchi. false: nulla si e' mosso (lo stato e' quello di prima).
bool movable(const State& s, Id point) noexcept;
bool dragPoint(State& s, Id point, double x, double y);
bool translate(State& s, const std::vector<Id>& selection, double dx, double dy);
std::vector<Id> freePoints(const State& s, const std::vector<Id>& selection);

// ---- selezione col puntatore ----------------------------------------------------------
// Oggetto piu' vicino a (x, y) entro tol (punti prima delle curve, curve prima
// dell'interno dei poligoni); i testi li prova il controller (conosce il font).
Id hitTest(const State& s, double x, double y, double tol, bool pointsOnly = false);
Id hitCurve(const State& s, double x, double y, double tol);       // solo curve (Point su un oggetto)
bool project(const State& s, Id curve, double x, double y, double& t, double& px, double& py);
bool curvePoint(const State& s, Id curve, double t, double& x, double& y);   // punto della curva al parametro t

// ---- misure, blocchi, valori ------------------------------------------------------------
std::vector<MSpec> available(const State& s, const std::vector<Id>& selection);
Measurement measure(const State& s, const MSpec& m);
std::string formatNumber(double v, std::uint8_t fmt);
std::string measureValueText(const State& s, const MSpec& m, std::uint8_t fmt);   // "5.00", "(1.00, 2.00)", "Si'"
std::string measureText(const State& s, const Object& pasted);                     // "AB=5.00 cm"
bool isLocked(const State& s, const MSpec& m) noexcept;
// Scrive un valore (M18): cambia la figura con lo spostamento minimo e blocca la misura
// (Label ed Expression: rinomina / formula; RotationAngle/DilationScale: parametro).
void setValue(State& s, const MSpec& m, double v0, double v1 = 0, const std::string& text = {});
void lock(State& s, const MSpec& m);                                   // blocca il valore attuale (Si'/No: forza Si')
// Le stesse operazioni a passi (controller): prepareValue/prepareLock controllano il
// valore e preparano il blocco, putLock lo inserisce senza risolvere; poi SolveTask
// (con AC) e, se fallisce, lo stato di prima. setValue = prepareValue + putLock + solve.
Lock prepareValue(const State& s, const MSpec& m, double v0, double v1 = 0);
Lock prepareLock(const State& s, const MSpec& m);
void putLock(State& s, const Lock& l);
void unlock(State& s, const MSpec& m);
double residual(const State& s, const Lock& l, std::size_t i) noexcept;
std::size_t residualCount(const Lock& l) noexcept;

// Risolutore dei blocchi (§4.3): proiezione di Gauss-Newton a norma minima pesata.
// Un'unita' di lavoro = un ricalcolo del grafo. AC (cancelled) rimette lo stato iniziale.
class SolveTask final : public step::Task {
public:
    SolveTask(State& s, std::vector<Id> pinned);
    bool step(step::Budget& budget) override;
    void cancelled() override;
    bool ok() const noexcept { return ok_; }
    bool failed() const noexcept { return failed_; }
    std::size_t total() const override { return 60; }
    std::size_t done() const override { return iter_; }
    std::size_t variables() const noexcept { return vars_.size(); }
private:
    struct Var { std::size_t obj, k; double weight; };
    double eval(std::vector<double>& r);
    void setVar(std::size_t j, double v);
    State& s_;
    std::vector<Var> vars_;
    std::vector<double> start_;
    std::size_t iter_{0};
    bool ok_{false}, failed_{false};
};
bool solve(State& s, const std::vector<Id>& pinned);                   // senza budget

// ---- animazioni -------------------------------------------------------------------
void addAnimation(State& s, Id point, Id path, bool replace);         // A01, A02
void setTrace(State& s, const std::vector<Id>& points);               // A03
bool pathPoint(const State& s, const Animation& a, double t, double& x, double& y);
void addTableColumn(State& s, const MSpec& m);                        // A07
// Edit Animation (A04): Steps 2-100, t0/t1 -10...10 (funzioni: x), Times 0-9999 (0 = senza fine).
void setAnimation(State& s, std::size_t index, int steps, double t0, double t1, int times);
void removeAnimation(State& s, std::size_t index);                     // DELETE di una riga
void removeColumn(State& s, std::size_t column);                       // tabella > DELETE (A11)
void clearTable(State& s);                                             // DEL-ALL
constexpr double NoValue = 1e300;                                      // cella indefinita della tabella
class AnimationTask final : public step::Task {
public:
    // repeat: Go (repeat); draw: false per calcolare solo la tabella (Display Table).
    AnimationTask(State& s, bool repeat, bool recordTable);
    bool step(step::Budget& budget) override;    // un'unita' = un fotogramma
    void cancelled() override;
    std::size_t total() const override { return frames_; }
    std::size_t done() const override { return frame_; }
    std::size_t frames() const noexcept { return frames_; }
    std::size_t cycle() const noexcept { return cycle_; }
    const std::vector<std::vector<double>>& traces() const noexcept { return traces_; }   // x, y per punto con traccia
private:
    void apply(std::size_t frame);
    void restore();
    State& s_;
    bool repeat_, record_;
    std::size_t frames_{0}, frame_{0}, cycle_{0}, cycles_{1};
    std::vector<std::pair<std::size_t, std::vector<double>>> saved_;
    std::vector<std::vector<double>> traces_;
    bool restored_{false};
};

// ---- vista ---------------------------------------------------------------------------
void toScreen(const View& v, int w, int h, double x, double y, double& sx, double& sy) noexcept;
void toPlane(const View& v, int w, int h, double sx, double sy, double& x, double& y) noexcept;
void zoom(View& v, double factor) noexcept;
void zoomBox(View& v, int w, int h, double x0, double y0, double x1, double y1) noexcept;
bool fit(State& s, int w, int h);                // Zoom to Fit (V06): false se nulla da inquadrare
void setWindow(View& v, int w, double xmin, double xmax, double ymid);   // V-Window (V09)

// ---- stato, sezione 0x0110 e file .pcgeo -------------------------------------------------
constexpr std::uint8_t FormatVersion = 2;
void reset(State& s);
void encode(const State& s, std::vector<std::uint8_t>& out);
bool decode(State& s, const std::uint8_t* data, std::size_t size);       // v1 (migrata) e v2
std::vector<std::uint8_t> encodeFile(const State& s);
bool decodeFile(const std::uint8_t* data, std::size_t size, State& out); // v1 (migrata) e v2; out.file resta
} // namespace fx::geo
