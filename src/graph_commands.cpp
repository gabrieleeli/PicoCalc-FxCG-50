// Interprete dei comandi grafici di Run-Matrix (vedi graph_commands.hpp).
#include "graph_commands.hpp"
#include "expression.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace fx::graphcmd {
namespace {
enum class Cmd : std::uint8_t {
    Cls, ViewWindow, GraphY, GraphR, GraphX, GraphParam, Plot, PlotOn, PlotOff, PlotChg, Line, FLine, Circle,
    Vertical, Horizontal, Text, PxlOn, PxlOff, PxlChg, Tangent, Normal, Inverse,
};
struct Keyword { const char* word; Cmd cmd; };
// Le parole piu' lunghe prima ("PlotOn" prima di "Plot", "F-Line" prima di "Line").
const Keyword kKeywords[] = {
    {"ViewWindow", Cmd::ViewWindow}, {"Graph(X,Y)=", Cmd::GraphParam}, {"GraphY=", Cmd::GraphY}, {"Graphr=", Cmd::GraphR},
    {"GraphX=", Cmd::GraphX}, {"PlotOn", Cmd::PlotOn}, {"PlotOff", Cmd::PlotOff}, {"PlotChg", Cmd::PlotChg}, {"Plot", Cmd::Plot},
    {"F-Line", Cmd::FLine}, {"Line", Cmd::Line}, {"Circle", Cmd::Circle}, {"Vertical", Cmd::Vertical},
    {"Horizontal", Cmd::Horizontal}, {"Text", Cmd::Text}, {"PxlOn", Cmd::PxlOn}, {"PxlOff", Cmd::PxlOff},
    {"PxlChg", Cmd::PxlChg}, {"Tangent", Cmd::Tangent}, {"Normal", Cmd::Normal}, {"Inverse", Cmd::Inverse}, {"Cls", Cmd::Cls},
};
const char* const kMenu[] = {"Cls", "ViewWindow ", "Graph Y=", "Graph r=", "Graph X=", "Graph(X,Y)=(", "Plot ", "PlotOn ", "PlotOff ",
                             "PlotChg ", "Line", "F-Line ", "Circle ", "Vertical ", "Horizontal ", "Text ", "PxlOn ", "PxlOff ",
                             "PxlChg ", "Tangent ", "Normal ", "Inverse ", nullptr};

// Testo senza spazi con la mappa delle posizioni originali (per gli errori).
struct Compact { std::string text; std::vector<int> origin; };
Compact compact(const std::string& s) {
    Compact c;
    bool quoted = false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '"') quoted = !quoted;
        if (!quoted && (s[i] == ' ' || s[i] == '\t')) continue;
        c.text += s[i];
        c.origin.push_back(static_cast<int>(i));
    }
    c.origin.push_back(static_cast<int>(s.size()));
    return c;
}
bool match(const std::string& t, Cmd& cmd, std::size_t& length) {
    for (const auto& k : kKeywords) {
        const std::size_t n = std::strlen(k.word);
        if (t.compare(0, n, k.word) != 0) continue;
        // "Plot" non deve essere l'inizio di un'altra parola ("Plotx" e' un prodotto).
        const char last = k.word[n - 1];
        if (std::isalpha(static_cast<unsigned char>(last)) && n < t.size() && std::isalpha(static_cast<unsigned char>(t[n]))) continue;
        cmd = k.cmd; length = n;
        return true;
    }
    return false;
}
struct Arg { std::string text; int pos{}, len{}; };
// Argomenti separati dalle virgole al livello 0 (parentesi, quadre, graffe, virgolette).
std::vector<Arg> splitArgs(const Compact& c, std::size_t from) {
    std::vector<Arg> out;
    const std::string& t = c.text;
    if (from >= t.size()) return out;
    int depth = 0;
    bool quoted = false;
    std::size_t start = from;
    auto push = [&](std::size_t end) {
        Arg a;
        a.text = t.substr(start, end - start);
        a.pos = c.origin[start];
        a.len = c.origin[std::min(end, t.size())] - a.pos;
        out.push_back(a);
    };
    for (std::size_t i = from; i < t.size(); ++i) {
        const char ch = t[i];
        if (ch == '"') { quoted = !quoted; continue; }
        if (quoted) continue;
        if (ch == '(' || ch == '[' || ch == '{') ++depth;
        else if (ch == ')' || ch == ']' || ch == '}') --depth;
        else if (ch == ',' && depth == 0) { push(i); start = i + 1; }
    }
    push(t.size());
    return out;
}
[[noreturn]] void syntax(const std::string& what, int pos, int len) { throw SyntaxError("Syntax ERROR: " + what, pos, len); }

struct Runner {
    Workspace& ws;
    const Compact& c;
    Cmd cmd;
    std::size_t keyLength;
    double number(const Arg& a) {
        if (a.text.empty()) syntax("argomento mancante", a.pos, a.len);
        EvalInfo info;
        EvalContext ctx = ws.evalContext();
        ctx.info = &info;
        try {
            return evaluateReal(*parseExpression(a.text), ctx);
        } catch (SyntaxError& e) {
            e.where = info.error.known() ? info.error : e.where;
            if (e.where.known()) e.where.position += a.pos; else e.where = {a.pos, a.len};
            throw;
        } catch (MathError& e) {
            if (!e.where.known()) e.where = info.error;
            if (e.where.known()) e.where.position += a.pos; else e.where = {a.pos, a.len};
            throw;
        }
    }
    std::vector<double> numbers(const std::vector<Arg>& args, std::size_t minCount, std::size_t maxCount, const char* usage) {
        if (args.size() < minCount || args.size() > maxCount) syntax(std::string("uso: ") + usage, c.origin[0], c.origin.back() - c.origin[0]);
        std::vector<double> v;
        for (const Arg& a : args) v.push_back(number(a));
        return v;
    }
    // Pixel (riga, colonna) della schermata del grafico -> coordinate.
    void pixel(double row, double col, double& x, double& y, const std::vector<Arg>& args) {
        if (row != std::floor(row) || col != std::floor(col) || row < 1 || row > graphgeo::Rows || col < 1 || col > graphgeo::Columns)
            throw MathError("Argument ERROR: riga 1-" + std::to_string(graphgeo::Rows) + ", colonna 1-" + std::to_string(graphgeo::Columns),
                            args[0].pos, args.back().pos + args.back().len - args[0].pos);
        const ViewWindow& v = ws.view;
        x = v.xmin + (col - 1) * viewXdot(v);
        y = v.ymax - (row - 1) * viewYdot(v);
    }
    // Una funzione di x per Tangent / Normal: valore e derivata nel punto.
    void slopeAt(const Arg& f, double x0, double& y0, double& slope) {
        if (f.text.empty()) syntax("funzione mancante", f.pos, f.len);
        const auto body = parseExpression(f.text);
        EvalContext base = ws.evalContext();
        auto eval = [&](double x) { EvalContext ctx = base; ctx.bind("x", x); return evaluateReal(*body, ctx); };
        try {
            y0 = eval(x0);
            slope = differentiateFunction(eval, x0);
        } catch (MathError& e) {
            e.where = {f.pos, f.len};
            throw;
        }
        if (!std::isfinite(y0) || !std::isfinite(slope)) throw MathError("Math ERROR: funzione non derivabile nel punto", f.pos, f.len);
    }
    SketchItem item(SketchItem::Kind kind) const {
        SketchItem s;
        s.kind = kind;
        s.color = ws.graph.settings.plotColor;
        s.style = ws.graph.settings.sketchLine;
        return s;
    }
    void addSketch(std::vector<SketchItem>& out, const SketchItem& s) {
        const std::string bad = validateSketch(s);
        if (!bad.empty()) throw MathError("Argument ERROR: " + bad);
        out.push_back(s);
    }
    std::string graphText(std::size_t from) const {
        std::string body = c.text.substr(from);
        if (body.empty()) syntax("funzione mancante", c.origin[std::min(from, c.text.size())], 1);
        return body;
    }
    void run() {
        GraphState& g = ws.graph;
        std::vector<SketchItem> added;
        const std::vector<Arg> args = splitArgs(c, keyLength);
        auto noArgs = [&]() { if (!args.empty()) syntax("il comando non ha argomenti", args[0].pos, args.back().pos + args.back().len - args[0].pos); };
        switch (cmd) {
        case Cmd::Cls:
            noArgs();
            g.sketch.clear();
            g.commandGraphs.clear();
            break;
        case Cmd::ViewWindow: {
            const auto v = numbers(args, 6, 9, "ViewWindow Xmin,Xmax,Xscl,Ymin,Ymax,Yscl[,Tmin,Tmax,Tptch]");
            if (args.size() != 6 && args.size() != 9) syntax("ViewWindow ha 6 o 9 argomenti", args[0].pos, args.back().pos + args.back().len - args[0].pos);
            ViewWindow w = ws.view;
            w.xmin = v[0]; w.xmax = v[1]; w.xscl = v[2]; w.ymin = v[3]; w.ymax = v[4]; w.yscl = v[5];
            if (v.size() == 9) { w.tmin = v[6]; w.tmax = v[7]; w.tstep = v[8]; }
            const std::string bad = validateView(w);
            if (!bad.empty()) throw MathError(bad, args[0].pos, args.back().pos + args.back().len - args[0].pos);
            ws.view = w;
            break;
        }
        case Cmd::GraphY: case Cmd::GraphR: case Cmd::GraphX: case Cmd::GraphParam: {
            if (g.commandGraphs.size() >= MaxCommandGraphs)
                throw MathError("Memory ERROR: massimo " + std::to_string(MaxCommandGraphs) + " grafici dei comandi (Cls li cancella)");
            CommandGraph cg;
            cg.type = cmd == Cmd::GraphY ? GraphType::Y : cmd == Cmd::GraphR ? GraphType::Polar : cmd == Cmd::GraphX ? GraphType::X : GraphType::Param;
            const std::string body = graphText(keyLength);
            if (cmd == Cmd::GraphParam) {
                // (xt,yt): le due componenti al livello 0 delle parentesi.
                std::string inner = body;
                if (inner.front() != '(') syntax("Graph(X,Y)=(xt,yt)", c.origin[keyLength], 1);
                inner = inner.substr(1);
                if (!inner.empty() && inner.back() == ')') inner.pop_back();
                Compact sub; sub.text = inner;
                for (std::size_t i = 0; i <= inner.size(); ++i) sub.origin.push_back(c.origin[std::min(keyLength + 1 + i, c.origin.size() - 1)]);
                const auto parts = splitArgs(sub, 0);
                if (parts.size() != 2 || parts[0].text.empty() || parts[1].text.empty()) syntax("Graph(X,Y)=(xt,yt)", c.origin[keyLength], 1);
                cg.expr = parts[0].text; cg.expr2 = parts[1].text;
            } else cg.expr = body;
            for (const std::string* e : {&cg.expr, &cg.expr2}) {
                if (e->empty()) continue;
                if (e->size() > MaxGraphExpression) throw MathError("Memory ERROR: espressione troppo lunga");
                try { (void)parseExpression(*e); }
                catch (SyntaxError& err) { if (err.where.known()) err.where.position += c.origin[keyLength]; throw; }
            }
            cg.color = defaultGraphColor(g.commandGraphs.size());
            g.commandGraphs.push_back(std::move(cg));
            break;
        }
        case Cmd::Plot: case Cmd::PlotOn: case Cmd::PlotOff: case Cmd::PlotChg: case Cmd::PxlOn: case Cmd::PxlOff: case Cmd::PxlChg: {
            const bool pixelCmd = cmd == Cmd::PxlOn || cmd == Cmd::PxlOff || cmd == Cmd::PxlChg;
            const auto v = numbers(args, 2, 2, pixelCmd ? "Pxl riga,colonna" : "Plot x,y");
            double x = v[0], y = v[1];
            if (pixelCmd) pixel(v[0], v[1], x, y, args);
            SketchItem::Kind kind = (cmd == Cmd::PlotOff || cmd == Cmd::PxlOff) ? SketchItem::Kind::PointOff : SketchItem::Kind::Point;
            if (cmd == Cmd::PlotChg || cmd == Cmd::PxlChg) {
                // Inverte il punto: acceso se l'ultimo elemento in quel punto e' spento o manca.
                kind = SketchItem::Kind::Point;
                for (auto it = g.sketch.rbegin(); it != g.sketch.rend(); ++it)
                    if ((it->kind == SketchItem::Kind::Point || it->kind == SketchItem::Kind::PointOff) && it->x0 == x && it->y0 == y) {
                        kind = it->kind == SketchItem::Kind::Point ? SketchItem::Kind::PointOff : SketchItem::Kind::Point;
                        break;
                    }
            }
            SketchItem s = item(kind);
            s.x0 = x; s.y0 = y;
            addSketch(added, s);
            break;
        }
        case Cmd::Line: {
            noArgs();
            // Unisce gli ultimi due punti di Plot (fx-CG50).
            const SketchItem* p[2] = {nullptr, nullptr};
            int found = 0;
            for (auto it = g.sketch.rbegin(); it != g.sketch.rend() && found < 2; ++it)
                if (it->kind == SketchItem::Kind::Point) p[found++] = &*it;
            if (found < 2) throw MathError("Math ERROR: Line unisce gli ultimi due punti di Plot");
            SketchItem s = item(SketchItem::Kind::Segment);
            s.x0 = p[1]->x0; s.y0 = p[1]->y0; s.x1 = p[0]->x0; s.y1 = p[0]->y0;
            addSketch(added, s);
            break;
        }
        case Cmd::FLine: {
            const auto v = numbers(args, 4, 4, "F-Line x1,y1,x2,y2");
            SketchItem s = item(SketchItem::Kind::Segment);
            s.x0 = v[0]; s.y0 = v[1]; s.x1 = v[2]; s.y1 = v[3];
            addSketch(added, s);
            break;
        }
        case Cmd::Circle: {
            const auto v = numbers(args, 3, 3, "Circle x,y,r");
            if (!(v[2] > 0)) throw MathError("Argument ERROR: il raggio deve essere positivo", args[2].pos, args[2].len);
            SketchItem s = item(SketchItem::Kind::Circle);
            s.x0 = v[0]; s.y0 = v[1]; s.x1 = v[2];
            addSketch(added, s);
            break;
        }
        case Cmd::Vertical: case Cmd::Horizontal: {
            const auto v = numbers(args, 1, 1, cmd == Cmd::Vertical ? "Vertical x" : "Horizontal y");
            SketchItem s = item(cmd == Cmd::Vertical ? SketchItem::Kind::Vertical : SketchItem::Kind::Horizontal);
            if (cmd == Cmd::Vertical) s.x0 = v[0]; else s.y0 = v[0];
            addSketch(added, s);
            break;
        }
        case Cmd::Text: {
            if (args.size() != 3) syntax("uso: Text riga,colonna,\"testo\"", c.origin[0], c.origin.back() - c.origin[0]);
            const std::string& t = args[2].text;
            if (t.size() < 2 || t.front() != '"' || t.back() != '"') syntax("il testo va tra virgolette", args[2].pos, args[2].len);
            const double row = number(args[0]), col = number(args[1]);
            SketchItem s = item(SketchItem::Kind::Text);
            pixel(row, col, s.x0, s.y0, args);
            s.text = t.substr(1, t.size() - 2);
            if (s.text.size() > MaxSketchText) throw MathError("Argument ERROR: testo di al piu' " + std::to_string(MaxSketchText) + " caratteri", args[2].pos, args[2].len);
            addSketch(added, s);
            break;
        }
        case Cmd::Tangent: case Cmd::Normal: {
            if (args.size() != 2) syntax(cmd == Cmd::Tangent ? "uso: Tangent f(x),x" : "uso: Normal f(x),x", c.origin[0], c.origin.back() - c.origin[0]);
            const double x0 = number(args[1]);
            double y0 = 0, slope = 0;
            slopeAt(args[0], x0, y0, slope);
            SketchItem s = item(cmd == Cmd::Tangent ? SketchItem::Kind::Tangent : SketchItem::Kind::Normal);
            s.x0 = x0; s.y0 = y0;
            if (cmd == Cmd::Tangent) { s.x1 = 1; s.y1 = slope; } else { s.x1 = -slope; s.y1 = 1; }
            addSketch(added, s);
            break;
        }
        case Cmd::Inverse: {
            if (args.size() != 1 || args[0].text.empty()) syntax("uso: Inverse f(x)", c.origin[0], c.origin.back() - c.origin[0]);
            try { (void)parseExpression(args[0].text); }
            catch (SyntaxError& err) { if (err.where.known()) err.where.position += args[0].pos; throw; }
            SketchItem s = item(SketchItem::Kind::Inverse);
            s.text = args[0].text;
            if (s.text.size() > MaxGraphExpression) throw MathError("Memory ERROR: espressione troppo lunga");
            addSketch(added, s);
            break;
        }
        }
        // Tutto o niente: i limiti di SKETCH prima di cambiare i dati.
        if (g.sketch.size() + added.size() > MaxSketchItems)
            throw MathError("Memory ERROR: massimo " + std::to_string(MaxSketchItems) + " elementi SKETCH (Cls li cancella)");
        for (auto& s : added) g.sketch.push_back(std::move(s));
        g.projectModified = true;
        g.commandScreen = true;
    }
};
} // namespace

bool isCommand(const std::string& text) {
    const Compact c = compact(text);
    Cmd cmd{};
    std::size_t length = 0;
    return match(c.text, cmd, length);
}

void execute(Workspace& ws, const std::string& text) {
    const Compact c = compact(text);
    Cmd cmd{};
    std::size_t length = 0;
    if (!match(c.text, cmd, length)) throw SyntaxError("Syntax ERROR: comando grafico sconosciuto", 0, static_cast<int>(text.size()));
    Runner r{ws, c, cmd, length};
    r.run();
}

const char* const* menuNames() noexcept { return kMenu; }
} // namespace fx::graphcmd
