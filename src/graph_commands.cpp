// Interprete dei comandi grafici di Run-Matrix (vedi graph_commands.hpp).
#include "graph_commands.hpp"
#include "expression.hpp"
#include "file_store.hpp"
#include "graph_engine.hpp"
#include "pict.hpp"
#include "limits.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace fx::graphcmd {
namespace {
enum class Cmd : std::uint8_t {
    Cls, ViewWindow, GraphY, GraphR, GraphX, GraphParam, Plot, PlotOn, PlotOff, PlotChg, Line, FLine, Circle,
    Vertical, Horizontal, Text, PxlOn, PxlOff, PxlChg, Tangent, Normal, Inverse,
    // v1.12.0
    GraphYGreater, GraphYLess, GraphYGreaterEq, GraphYLessEq, GraphXGreater, GraphXLess, GraphXGreaterEq, GraphXLessEq,
    GIntegral, StoPict, RclPict, BgPict, DrawStat, DrawGraph, DrawDyna, DrawRCon, DrawRPlt, DrawFtgCon, DrawFtgPlt, DrawWeb,
    GSelOn, GSelOff, RclCapt,
};
struct Keyword { const char* word; Cmd cmd; };
// Le parole piu' lunghe prima ("PlotOn" prima di "Plot", "F-Line" prima di "Line").
const Keyword kKeywords[] = {
    {"ViewWindow", Cmd::ViewWindow}, {"Graph(X,Y)=", Cmd::GraphParam}, {"GraphY=", Cmd::GraphY}, {"Graphr=", Cmd::GraphR},
    // Disequazioni (anche con >= e <= ASCII), G∫dx, immagini, Draw..., G-SelOn/Off (v1.12.0)
    {"GraphY>=", Cmd::GraphYGreaterEq}, {"GraphY<=", Cmd::GraphYLessEq}, {"GraphY\xE2\x89\xA5", Cmd::GraphYGreaterEq},
    {"GraphY\xE2\x89\xA4", Cmd::GraphYLessEq}, {"GraphY>", Cmd::GraphYGreater}, {"GraphY<", Cmd::GraphYLess},
    {"GraphX>=", Cmd::GraphXGreaterEq}, {"GraphX<=", Cmd::GraphXLessEq}, {"GraphX\xE2\x89\xA5", Cmd::GraphXGreaterEq},
    {"GraphX\xE2\x89\xA4", Cmd::GraphXLessEq}, {"GraphX>", Cmd::GraphXGreater}, {"GraphX<", Cmd::GraphXLess},
    {"G\xE2\x88\xAB" "dx", Cmd::GIntegral}, {"StoPict", Cmd::StoPict}, {"RclPict", Cmd::RclPict}, {"BG-Pict", Cmd::BgPict}, {"RclCapt", Cmd::RclCapt},
    {"DrawStat", Cmd::DrawStat}, {"DrawGraph", Cmd::DrawGraph}, {"DrawDyna", Cmd::DrawDyna}, {"DrawR-Con", Cmd::DrawRCon},
    {"DrawR-Plt", Cmd::DrawRPlt}, {"DrawFTG-Con", Cmd::DrawFtgCon}, {"DrawFTG-Plt", Cmd::DrawFtgPlt}, {"DrawWeb", Cmd::DrawWeb},
    {"G-SelOn", Cmd::GSelOn}, {"G-SelOff", Cmd::GSelOff},
    {"GraphX=", Cmd::GraphX}, {"PlotOn", Cmd::PlotOn}, {"PlotOff", Cmd::PlotOff}, {"PlotChg", Cmd::PlotChg}, {"Plot", Cmd::Plot},
    {"F-Line", Cmd::FLine}, {"Line", Cmd::Line}, {"Circle", Cmd::Circle}, {"Vertical", Cmd::Vertical},
    {"Horizontal", Cmd::Horizontal}, {"Text", Cmd::Text}, {"PxlOn", Cmd::PxlOn}, {"PxlOff", Cmd::PxlOff},
    {"PxlChg", Cmd::PxlChg}, {"Tangent", Cmd::Tangent}, {"Normal", Cmd::Normal}, {"Inverse", Cmd::Inverse}, {"Cls", Cmd::Cls},
};
const char* const kMenu[] = {"Cls", "ViewWindow ", "Graph Y=", "Graph r=", "Graph X=", "Graph(X,Y)=(",
                             "Graph Y>", "Graph Y<", "Graph Y\xE2\x89\xA5", "Graph Y\xE2\x89\xA4", "Graph X>", "Graph X<",
                             "Graph X\xE2\x89\xA5", "Graph X\xE2\x89\xA4", "G\xE2\x88\xAB" "dx ", "Plot ", "PlotOn ", "PlotOff ",
                             "PlotChg ", "Line", "F-Line ", "Circle ", "Vertical ", "Horizontal ", "Text ", "PxlOn ", "PxlOff ",
                             "PxlChg ", "PxlTest(", "Tangent ", "Normal ", "Inverse ", "SketchNormal ", "SketchThick ", "SketchBroken ",
                             "SketchDot ", "SketchThin ", "StoPict ", "RclPict ", "BG-Pict ", "RclCapt ", "DrawGraph", "DrawStat", "DrawDyna",
                             "DrawR-Con", "DrawR-Plt", "DrawWeb", "DrawFTG-Con", "DrawFTG-Plt", "G-SelOn ", "G-SelOff ", nullptr};
// Prefisso di stile della fx-CG50 (SketchThick F-Line ...): lo stile dell'elemento disegnato.
struct StylePrefix { const char* word; LineStyle style; };
const StylePrefix kStyles[] = {{"SketchNormal", LineStyle::Normal}, {"SketchThick", LineStyle::Thick}, {"SketchBroken", LineStyle::Broken},
                               {"SketchDot", LineStyle::Dot}, {"SketchThin", LineStyle::Thin}};

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
bool match(const Compact& c, Cmd& cmd, std::size_t& length) {
    const std::string& t = c.text;
    for (const auto& k : kKeywords) {
        const std::size_t n = std::strlen(k.word);
        if (t.compare(0, n, k.word) != 0) continue;
        // "Plot" non deve essere l'inizio di un'altra parola ("Plotx" e' un prodotto),
        // salvo uno spazio dopo il nome ("Plot A,B").
        const char last = k.word[n - 1];
        const bool spaced = n < t.size() && c.origin[n] > c.origin[n - 1] + 1;
        if (std::isalpha(static_cast<unsigned char>(last)) && n < t.size() && std::isalpha(static_cast<unsigned char>(t[n])) && !spaced) continue;
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
    bool styled{false};                 // prefisso SketchNormal/Thick/Broken/Dot/Thin
    LineStyle style{LineStyle::Normal};
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
        s.style = styled ? style : ws.graph.settings.sketchLine;
        return s;
    }
    CommandGraph pendingGraph;
    bool hasPendingGraph{false};
    static GraphType graphTypeOf(Cmd k) {
        switch (k) {
        case Cmd::GraphR: return GraphType::Polar;
        case Cmd::GraphX: return GraphType::X;
        case Cmd::GraphParam: return GraphType::Param;
        case Cmd::GraphYGreater: return GraphType::YGreater;
        case Cmd::GraphYLess: return GraphType::YLess;
        case Cmd::GraphYGreaterEq: return GraphType::YGreaterEq;
        case Cmd::GraphYLessEq: return GraphType::YLessEq;
        case Cmd::GraphXGreater: return GraphType::XGreater;
        case Cmd::GraphXLess: return GraphType::XLess;
        case Cmd::GraphXGreaterEq: return GraphType::XGreaterEq;
        case Cmd::GraphXLessEq: return GraphType::XLessEq;
        default: return GraphType::Y;
        }
    }
    static std::string pictPath(int n) { return files::join(files::PictFolder, pict::memoryFileName(n)); }
    // Punti (DrawStat, DrawR, DrawFTG): un elemento SKETCH "Data".
    void addData(std::vector<SketchItem>& out, const std::vector<double>& xs, const std::vector<double>& ys, bool connected, int color = -1) {
        SketchItem s = item(SketchItem::Kind::Data);
        if (color >= 0) s.color = static_cast<std::uint8_t>(color);
        s.y1 = connected ? 1 : 0;
        for (std::size_t i = 0; i < xs.size() && i < MaxDataPoints; ++i) {
            if (!std::isfinite(xs[i]) || !std::isfinite(ys[i]) || std::abs(xs[i]) > 1e30 || std::abs(ys[i]) > 1e30) continue;
            s.path.push_back(static_cast<float>(xs[i])); s.path.push_back(static_cast<float>(ys[i]));
        }
        if (s.path.empty()) return;
        addSketch(out, s);
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
            if (!bad.empty()) throw MathError("Range ERROR: " + bad.substr(bad.find(':') == std::string::npos ? 0 : bad.find(':') + 2), args[0].pos,
                                              args.back().pos + args.back().len - args[0].pos);
            ws.view = w;
            break;
        }
        case Cmd::GraphY: case Cmd::GraphR: case Cmd::GraphX: case Cmd::GraphParam:
        case Cmd::GraphYGreater: case Cmd::GraphYLess: case Cmd::GraphYGreaterEq: case Cmd::GraphYLessEq:
        case Cmd::GraphXGreater: case Cmd::GraphXLess: case Cmd::GraphXGreaterEq: case Cmd::GraphXLessEq: {
            if (g.commandGraphs.size() >= MaxCommandGraphs)
                throw MathError("Memory ERROR: massimo " + std::to_string(MaxCommandGraphs) + " grafici dei comandi (Cls li cancella)");
            CommandGraph cg;
            cg.type = graphTypeOf(cmd);
            if (styled) cg.style = style;
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
        case Cmd::GIntegral: {
            // G∫dx f(x),a,b: il grafico di f e l'area colorata tra la curva e l'asse x (manuale 5-?).
            if (args.size() != 3 || args[0].text.empty()) syntax("uso: G\xE2\x88\xAB" "dx f(x),inferiore,superiore", c.origin[0], c.origin.back() - c.origin[0]);
            try { (void)parseExpression(args[0].text); }
            catch (SyntaxError& err) { if (err.where.known()) err.where.position += args[0].pos; throw; }
            const double a = number(args[1]), b = number(args[2]);
            if (!(a < b)) throw MathError("Argument ERROR: inferiore < superiore", args[1].pos, args[2].pos + args[2].len - args[1].pos);
            if (g.commandGraphs.size() >= MaxCommandGraphs)
                throw MathError("Memory ERROR: massimo " + std::to_string(MaxCommandGraphs) + " grafici dei comandi (Cls li cancella)");
            if (args[0].text.size() > MaxGraphExpression) throw MathError("Memory ERROR: espressione troppo lunga");
            SketchItem s = item(SketchItem::Kind::Integral);
            s.text = args[0].text; s.x0 = a; s.x1 = b;
            addSketch(added, s);
            CommandGraph cg;
            cg.type = GraphType::Y; cg.expr = args[0].text; cg.color = s.color;
            if (styled) cg.style = style;
            pendingGraph = std::move(cg);
            hasPendingGraph = true;
            break;
        }
        case Cmd::RclCapt: {
            // RclCapt n: la schermata catturata (SHIFT 7) mostrata nella schermata del grafico.
            const auto v = numbers(args, 1, 1, "RclCapt n (n da 1 a 20)");
            if (v[0] != std::floor(v[0]) || v[0] < 1 || v[0] > 20) throw MathError("Argument ERROR: Capture da 1 a 20", args[0].pos, args[0].len);
            char name[32];
            std::snprintf(name, sizeof(name), "CAPT%02d%s", static_cast<int>(v[0]), files::PictExtension);
            g.overlay = files::join(CaptureFolder, name);
            break;
        }
        case Cmd::StoPict: case Cmd::RclPict: case Cmd::BgPict: {
            const auto v = numbers(args, 1, 1, "StoPict n / RclPict n / BG-Pict n (n da 1 a 20)");
            if (v[0] != std::floor(v[0]) || v[0] < 1 || v[0] > 20) throw MathError("Argument ERROR: Pict da 1 a 20", args[0].pos, args[0].len);
            const auto n = static_cast<std::uint8_t>(v[0]);
            if (cmd == Cmd::StoPict) g.storePict = n;                         // GRAPH salva la schermata disegnata
            else if (cmd == Cmd::BgPict) g.settings.background = n;
            else g.overlay = pictPath(n);                                     // GRAPH la richiama sopra lo sfondo
            break;
        }
        case Cmd::GSelOn: case Cmd::GSelOff: {
            // G-SelOn n / G-SelOff n: selezione di Yn nella lista delle relazioni.
            const auto v = numbers(args, 1, 1, "G-SelOn n (n da 1 a 20)");
            if (v[0] != std::floor(v[0]) || v[0] < 1 || v[0] > static_cast<double>(MaxGraphFunctions)) throw MathError("Argument ERROR: n da 1 a 20", args[0].pos, args[0].len);
            ws.functions[static_cast<std::size_t>(v[0]) - 1].selected = cmd == Cmd::GSelOn;
            break;
        }
        case Cmd::DrawGraph: noArgs(); break;                                 // disegna la lista delle relazioni
        case Cmd::DrawStat: {
            // StatGraph1 predefinito della fx-CG50: Scatter di XList/YList (SET di Statistics).
            noArgs();
            const auto& x = ws.lists[ws.settings.statXList];
            const auto& y = ws.lists[ws.settings.statYList];
            if (x.empty() || x.size() != y.size()) throw DimensionError("Dimension ERROR: XList e YList di Statistics vuote o di lunghezza diversa");
            std::vector<double> xs(x), ys(y);
            addData(added, xs, ys, false);
            break;
        }
        case Cmd::DrawRCon: case Cmd::DrawRPlt: case Cmd::DrawWeb: {
            // Successione an+1 = f(n, an) (VARS › RECURSION): punti (n, an) da R Start a R End.
            noArgs();
            const ModeVariables& m = ws.modeVars;
            if (m.recFormula.empty()) throw MathError("Math ERROR: an+1 non definita (\"formula\"\xE2\x86\x92" "an+1)");
            const auto f = parseExpression(m.recFormula);
            std::vector<double> xs, ys;
            double a = m.recInitial[0];
            const int start = static_cast<int>(m.recStart), end = static_cast<int>(m.recEnd);
            if (end - start + 1 > static_cast<int>(MaxDataPoints)) throw MathError("Argument ERROR: troppi termini (R End - R Start)");
            for (int k = 0; k <= end; ++k) {
                if (k >= start) { xs.push_back(k); ys.push_back(a); }
                EvalContext ctx = ws.evalContext();
                ctx.bind("rec_n", k); ctx.bind("rec_an", a);
                a = evaluateReal(*f, ctx);
            }
            if (cmd == Cmd::DrawWeb) {
                // Grafico a ragnatela: y = an+1(x) e y = x, poi i segmenti (an, an) -> (an, an+1).
                std::vector<double> wx, wy;
                for (std::size_t i = 0; i + 1 < ys.size(); ++i) { wx.push_back(ys[i]); wy.push_back(i == 0 ? 0 : ys[i]); wx.push_back(ys[i]); wy.push_back(ys[i + 1]); }
                if (!wx.empty()) addData(added, wx, wy, true);
            } else addData(added, xs, ys, cmd == Cmd::DrawRCon);
            break;
        }
        case Cmd::DrawFtgCon: case Cmd::DrawFtgPlt: {
            // La tabella delle funzioni selezionate (TblStart..TblEnd, TblPitch): punti (x, Yn(x)).
            noArgs();
            // Sullo heap: il motore e' troppo grande per lo stack da 32 KB dell'RP2350.
            const auto enginePtr = std::make_unique<graph::GraphEngine>();
            graph::GraphEngine& engine = *enginePtr;
            engine.build({&ws.functions, &ws.memory, &ws, &ws.lists, nullptr});
            const TableRange& t = ws.table;
            if (t.step == 0 || (t.end - t.start) / t.step < 0) throw MathError("Argument ERROR: intervallo di TABLE non valido");
            const double count = std::floor((t.end - t.start) / t.step + 1e-9) + 1;
            if (count > static_cast<double>(MaxDataPoints)) throw MathError("Argument ERROR: troppe righe nella tabella");
            for (const auto& p : engine.plots()) {
                if (!isYType(p.type) || p.pasted) continue;
                std::vector<double> xs, ys;
                for (int k = 0; k < static_cast<int>(count); ++k) {
                    const double x = t.start + k * t.step;
                    double y = 0;
                    if (engine.eval(p, x, y)) { xs.push_back(x); ys.push_back(y); }
                }
                if (!xs.empty()) addData(added, xs, ys, cmd == Cmd::DrawFtgCon, p.color);
            }
            break;
        }
        case Cmd::DrawDyna: {
            // Dyna Graph: Y1 per ogni valore del coefficiente A da D Start a D End con passo D pitch.
            noArgs();
            const GraphFunction& f = ws.functions[0];
            if (f.expr.empty() || !isYType(f.type)) throw MathError("Math ERROR: Y1 (funzione di Dyna Graph) non definita");
            const ModeVariables& m = ws.modeVars;
            if (m.dynaPitch == 0 || (m.dynaEnd - m.dynaStart) / m.dynaPitch < 0) throw MathError("Argument ERROR: D Start, D End, D pitch non validi");
            const double steps = std::floor((m.dynaEnd - m.dynaStart) / m.dynaPitch + 1e-9) + 1;
            if (g.commandGraphs.size() + static_cast<std::size_t>(steps) > MaxCommandGraphs)
                throw MathError("Memory ERROR: massimo " + std::to_string(MaxCommandGraphs) + " grafici dei comandi (Cls li cancella)");
            for (int k = 0; k < static_cast<int>(steps); ++k) {
                const double a = m.dynaStart + k * m.dynaPitch;
                char buf[40];
                std::snprintf(buf, sizeof(buf), "(%.15g)", a);
                CommandGraph cg;
                cg.type = GraphType::Y;
                cg.expr = rewriteIdentifiers(f.expr, [&](const std::string& id) { return id == "a" || id == "A" ? std::string(buf) : std::string(); });
                cg.color = defaultGraphColor(g.commandGraphs.size());
                g.commandGraphs.push_back(std::move(cg));
            }
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
        std::size_t points = 0;
        for (const auto* list : {&g.sketch, &added})
            for (const auto& s : *list) if (s.kind == SketchItem::Kind::Data) points += s.path.size() / 2;
        if (points > limits::kSketchDataPoints)
            throw MathError("Memory ERROR: massimo " + std::to_string(limits::kSketchDataPoints) + " punti di DrawStat/DrawR/DrawFTG (Cls li cancella)");
        for (auto& s : added) g.sketch.push_back(std::move(s));
        if (hasPendingGraph) g.commandGraphs.push_back(std::move(pendingGraph));
        g.projectModified = true;
        g.commandScreen = true;
    }
};
} // namespace

namespace {
// Toglie il prefisso di stile (SketchThick ...) e ne restituisce lo stile.
bool stylePrefix(Compact& c, LineStyle& style) {
    for (const auto& st : kStyles) {
        const std::size_t n = std::strlen(st.word);
        if (c.text.compare(0, n, st.word) != 0) continue;
        style = st.style;
        c.text.erase(0, n);
        c.origin.erase(c.origin.begin(), c.origin.begin() + static_cast<std::ptrdiff_t>(n));
        return true;
    }
    return false;
}
} // namespace

bool isCommand(const std::string& text) {
    Compact c = compact(text);
    LineStyle style{};
    if (stylePrefix(c, style) && c.text.empty()) return true;
    Cmd cmd{};
    std::size_t length = 0;
    return match(c, cmd, length);
}

std::size_t keywordLength(const std::string& text) {
    Compact c = compact(text);
    LineStyle style{};
    const std::size_t before = c.origin.size();
    const bool styled = stylePrefix(c, style);
    const std::size_t prefix = before - c.origin.size();      // caratteri compatti del prefisso
    (void)prefix;
    Cmd cmd{};
    std::size_t length = 0;
    std::size_t end = 0;
    if (styled && c.text.empty()) end = text.size();
    else if (match(c, cmd, length)) end = static_cast<std::size_t>(c.origin[length]);
    else return 0;
    // "(" di Graph(X,Y)=( e PxlTest( restano fuori; gli spazi dopo il nome entrano.
    while (end < text.size() && text[end] == ' ') ++end;
    return end;
}

void execute(Workspace& ws, const std::string& text) {
    Compact c = compact(text);
    LineStyle style{};
    const bool styled = stylePrefix(c, style);
    if (styled && c.text.empty()) {
        // SketchThick da solo: lo stile delle linee dello SKETCH (SET UP Sketch Line).
        ws.graph.settings.sketchLine = style;
        ws.graph.projectModified = true;
        return;
    }
    Cmd cmd{};
    std::size_t length = 0;
    if (!match(c, cmd, length)) throw SyntaxError("Syntax ERROR: comando grafico sconosciuto", 0, static_cast<int>(text.size()));
    Runner r{ws, c, cmd, length, styled, style, CommandGraph{}, false};
    r.run();
}

const char* const* menuNames() noexcept { return kMenu; }
} // namespace fx::graphcmd
