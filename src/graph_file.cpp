#include "graph_file.hpp"
#include "storage.hpp"
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef FXAI_VERSION_STRING
#define FXAI_VERSION_STRING "dev"
#endif

namespace fx::graphfile {
namespace {
std::string number(double v) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}
bool parseNumber(const std::string& s, double& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end == s.c_str() + s.size() && std::isfinite(out);
}
std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const auto p = s.find(sep, start);
        out.push_back(s.substr(start, p == std::string::npos ? std::string::npos : p - start));
        if (p == std::string::npos) return out;
        start = p + 1;
    }
}
bool numbers(const std::string& s, char sep, std::size_t count, double* out) {
    const auto parts = split(s, sep);
    if (parts.size() != count) return false;
    for (std::size_t i = 0; i < count; ++i) if (!parseNumber(parts[i], out[i])) return false;
    return true;
}
std::string viewText(const ViewWindow& v) {
    std::string s;
    for (double d : {v.xmin, v.xmax, v.xscl, v.ymin, v.ymax, v.yscl, v.tmin, v.tmax, v.tstep}) { if (!s.empty()) s += ';'; s += number(d); }
    return s;
}
bool parseView(const std::string& s, ViewWindow& v) {
    double d[9];
    if (!numbers(s, ';', 9, d)) return false;
    v = {d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8]};
    return validateView(v).empty();
}
const char* onOff(bool b) { return b ? "On" : "Off"; }
bool parseOnOff(const std::string& s, bool& out) {
    if (s == "On") { out = true; return true; }
    if (s == "Off") { out = false; return true; }
    return false;
}
const char* angleName(AngleMode a) { return a == AngleMode::Degrees ? "Deg" : a == AngleMode::Gradians ? "Gra" : "Rad"; }
std::string hex8(std::uint32_t v) { char b[12]; std::snprintf(b, sizeof(b), "%08X", static_cast<unsigned>(v)); return b; }

std::string functionLine(std::size_t i, const GraphFunction& f) {
    return "f" + std::to_string(i + 1) + "=" + graphTypeName(f.type) + "|" + onOff(f.selected) + "|" + graphColorName(f.color) + "|" +
           lineStyleName(f.style) + "|" + escape(f.expr) + "|" + escape(f.expr2) + "\n";
}
// Parses "f<n>=..." into functions[n-1]; error text or "".
std::string parseFunction(const std::string& key, const std::string& value, std::array<GraphFunction, MaxGraphFunctions>& fns, std::uint32_t& seen) {
    const int n = std::atoi(key.c_str() + 1);
    if (n < 1 || n > static_cast<int>(MaxGraphFunctions) || key != "f" + std::to_string(n)) return "indice di funzione non valido";
    if (seen & (1u << (n - 1))) return "funzione ripetuta";
    seen |= 1u << (n - 1);
    const auto parts = split(value, '|');
    if (parts.size() != 6) return "riga di funzione incompleta";
    GraphFunction f = emptyGraphFunction(static_cast<std::size_t>(n - 1));
    if (!parseGraphTypeName(parts[0], f.type)) return "tipo di grafico sconosciuto";
    if (!parseOnOff(parts[1], f.selected)) return "stato di disegno non valido";
    if (!parseGraphColorName(parts[2], f.color)) return "colore sconosciuto";
    if (!parseLineStyleName(parts[3], f.style)) return "stile di linea sconosciuto";
    if (!unescape(parts[4], f.expr) || !unescape(parts[5], f.expr2)) return "testo della funzione non valido";
    if (f.expr.size() > MaxGraphExpression || f.expr2.size() > MaxGraphExpression) return "espressione troppo lunga";
    if (f.type != GraphType::Param && !f.expr2.empty()) return "Yt presente in una funzione non parametrica";
    fns[static_cast<std::size_t>(n - 1)] = std::move(f);
    return {};
}
std::string sketchLine(const SketchItem& s) {
    std::string path;
    for (std::size_t i = 0; i + 1 < s.path.size(); i += 2) {
        if (!path.empty()) path += ';';
        path += number(s.path[i]) + "," + number(s.path[i + 1]);
    }
    return std::string("s=") + sketchKindName(s.kind) + "|" + number(s.x0) + ";" + number(s.y0) + ";" + number(s.x1) + ";" + number(s.y1) +
           "|" + graphColorName(s.color) + "|" + lineStyleName(s.style) + "|" + escape(s.text) + "|" + path + "\n";
}
std::string parseSketch(const std::string& value, SketchItem& s) {
    const auto parts = split(value, '|');
    if (parts.size() != 6) return "elemento SKETCH incompleto";
    if (!parseSketchKindName(parts[0], s.kind)) return "elemento SKETCH sconosciuto";
    double d[4];
    if (!numbers(parts[1], ';', 4, d)) return "coordinate SKETCH non valide";
    s.x0 = d[0]; s.y0 = d[1]; s.x1 = d[2]; s.y1 = d[3];
    if (!parseGraphColorName(parts[2], s.color) || !parseLineStyleName(parts[3], s.style)) return "stile SKETCH non valido";
    if (!unescape(parts[4], s.text)) return "testo SKETCH non valido";
    s.path.clear();
    if (!parts[5].empty()) {
        for (const auto& pt : split(parts[5], ';')) {
            double xy[2];
            if (!numbers(pt, ',', 2, xy)) return "tracciato SKETCH non valido";
            s.path.push_back(static_cast<float>(xy[0])); s.path.push_back(static_cast<float>(xy[1]));
            if (s.path.size() > 2 * MaxPenPoints) return "tracciato SKETCH troppo lungo";
        }
    }
    const std::string e = validateSketch(s);
    return e.empty() ? std::string() : "SKETCH: " + e;
}

// Checks header and CRC; returns the lines between them.
std::string open(const std::string& text, const char* magic, std::vector<std::string>& lines) {
    if (text.size() > MaxProjectBytes) return "file troppo grande";
    const auto crcPos = text.rfind("crc32=");
    if (crcPos == std::string::npos || (crcPos > 0 && text[crcPos - 1] != '\n')) return "firma CRC mancante (file troncato?)";
    std::string crcText = text.substr(crcPos + 6);
    while (!crcText.empty() && (crcText.back() == '\n' || crcText.back() == '\r')) crcText.pop_back();
    if (crcText.size() != 8) return "firma CRC non valida";
    char* end = nullptr;
    const unsigned long want = std::strtoul(crcText.c_str(), &end, 16);
    if (end != crcText.c_str() + 8) return "firma CRC non valida";
    if (storage::crc32(text.data(), crcPos) != static_cast<std::uint32_t>(want)) return "CRC errato: file danneggiato";
    lines = split(text.substr(0, crcPos), '\n');
    for (auto& l : lines) if (!l.empty() && l.back() == '\r') l.pop_back();
    const std::string head = lines.empty() ? std::string() : lines.front();
    const std::size_t m = std::strlen(magic);
    if (head.compare(0, m, magic) != 0 || head.size() <= m + 1 || head[m] != ' ') return std::string("intestazione ") + magic + " mancante";
    const int version = std::atoi(head.c_str() + m + 1);
    if (version > FormatVersion) return "creato da una versione piu' recente (formato " + std::to_string(version) + ")";
    if (version < 1) return "versione del formato non valida";
    return {};
}
std::string close(std::string body) {
    body += "end\n";
    return body + "crc32=" + hex8(storage::crc32(body.data(), body.size())) + "\n";
}
} // namespace

Project::Project() { for (std::size_t i = 0; i < functions.size(); ++i) functions[i] = emptyGraphFunction(i); }

std::string escape(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (c == '%' || c == '|' || c < 0x20 || c == 0x7F) { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += static_cast<char>(c);
    }
    return out;
}
bool unescape(const std::string& s, std::string& out) {
    out.clear();
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '%') { out += s[i]; continue; }
        if (i + 2 >= s.size() || !std::isxdigit(static_cast<unsigned char>(s[i + 1])) || !std::isxdigit(static_cast<unsigned char>(s[i + 2]))) return false;
        out += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
        i += 2;
    }
    return true;
}

std::string encode(const Project& p) {
    const GraphSettings& g = p.settings;
    std::string s = "PCGRAPH " + std::to_string(FormatVersion) + "\n";
    s += "# PicoCalc FX-CG50 AI - progetto GRAPH (formato documentato in docs/PCGRAPH_FORMAT.md)\n";
    s += std::string("generator=PicoCalc FX-CG50 AI ") + FXAI_VERSION_STRING + "\n";
    s += std::string("angle=") + angleName(p.angle) + "\n";
    s += "view=" + viewText(p.view) + "\n";
    s += std::string("functype=") + graphTypeName(g.funcType) + "\n";
    s += std::string("draw=") + (g.plotDraw ? "Plot" : "Connect") + "\n";
    s += std::string("derivative=") + onOff(g.derivative) + "\n";
    s += std::string("coord=") + onOff(g.coord) + "\n";
    s += std::string("grid=") + (g.grid == GridMode::On ? "On" : g.grid == GridMode::Line ? "Line" : "Off") + "\n";
    s += std::string("axes=") + (g.axes == AxesMode::Off ? "Off" : g.axes == AxesMode::Scale ? "Scale" : "On") + "\n";
    s += std::string("label=") + onOff(g.label) + "\n";
    s += std::string("graphfunc=") + onOff(g.graphFunc) + "\n";
    s += std::string("simul=") + onOff(g.simulGraph) + "\n";
    s += std::string("ineq=") + (g.ineqUnion ? "Union" : "Intsect") + "\n";
    s += std::string("sketchline=") + lineStyleName(g.sketchLine) + "\n";
    s += std::string("plotcolor=") + graphColorName(g.plotColor) + "\n";
    s += "factor=" + number(g.xFactor) + ";" + number(g.yFactor) + "\n";
    s += "background=" + std::to_string(g.background) + "\n";
    if (!g.backgroundFile.empty()) s += "bgfile=" + escape(g.backgroundFile) + "\n";
    s += "fade=" + std::to_string(g.fade) + "\n";
    for (std::size_t i = 0; i < VWindowMemories; ++i)
        if (p.vmemUsed & (1u << i)) s += "vmem" + std::to_string(i + 1) + "=" + viewText(p.vmem[i]) + "\n";
    s += "table=" + number(p.tableStart) + ";" + number(p.tableEnd) + ";" + number(p.tableStep) + "\n";
    if (!p.overlay.empty()) s += "overlay=" + escape(p.overlay) + "\n";
    for (std::size_t i = 0; i < p.functions.size(); ++i) if (!p.functions[i].empty()) s += functionLine(i, p.functions[i]);
    for (const auto& item : p.sketch) s += sketchLine(item);
    return close(std::move(s));
}

std::string decode(const std::string& text, Project& out) {
    std::vector<std::string> lines;
    std::string err = open(text, "PCGRAPH", lines);
    if (!err.empty()) return err;
    Project p;
    std::uint32_t seen = 0;
    bool haveView = false, ended = false;
    for (std::size_t li = 1; li < lines.size(); ++li) {
        const std::string& line = lines[li];
        if (line.empty() || line[0] == '#') continue;
        if (ended) return "testo dopo 'end'";
        if (line == "end") { ended = true; continue; }
        const auto eq = line.find('=');
        if (eq == std::string::npos) return "riga " + std::to_string(li + 1) + " non valida";
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        GraphSettings& g = p.settings;
        bool ok = true;
        if (key == "angle") {
            if (value == "Deg") p.angle = AngleMode::Degrees; else if (value == "Rad") p.angle = AngleMode::Radians;
            else if (value == "Gra") p.angle = AngleMode::Gradians; else ok = false;
        } else if (key == "view") { ok = parseView(value, p.view); haveView = ok; }
        else if (key == "functype") ok = parseGraphTypeName(value, g.funcType);
        else if (key == "draw") { ok = value == "Plot" || value == "Connect"; g.plotDraw = value == "Plot"; }
        else if (key == "derivative") ok = parseOnOff(value, g.derivative);
        else if (key == "coord") ok = parseOnOff(value, g.coord);
        else if (key == "grid") {
            if (value == "On") g.grid = GridMode::On; else if (value == "Line") g.grid = GridMode::Line; else if (value == "Off") g.grid = GridMode::Off; else ok = false;
        } else if (key == "axes") {
            if (value == "On") g.axes = AxesMode::On; else if (value == "Scale") g.axes = AxesMode::Scale; else if (value == "Off") g.axes = AxesMode::Off; else ok = false;
        } else if (key == "label") ok = parseOnOff(value, g.label);
        else if (key == "graphfunc") ok = parseOnOff(value, g.graphFunc);
        else if (key == "simul") ok = parseOnOff(value, g.simulGraph);
        else if (key == "ineq") { ok = value == "Union" || value == "Intsect"; g.ineqUnion = value == "Union"; }
        else if (key == "sketchline") ok = parseLineStyleName(value, g.sketchLine);
        else if (key == "plotcolor") ok = parseGraphColorName(value, g.plotColor);
        else if (key == "factor") { double f[2]; ok = numbers(value, ';', 2, f) && f[0] > 0 && f[1] > 0; if (ok) { g.xFactor = f[0]; g.yFactor = f[1]; } }
        else if (key == "background") { double b; ok = parseNumber(value, b) && b >= 0 && b <= BackgroundFile && b == std::floor(b); if (ok) g.background = static_cast<std::uint8_t>(b); }
        else if (key == "bgfile") ok = unescape(value, g.backgroundFile) && g.backgroundFile.size() <= 64;
        else if (key == "fade") { double f; ok = parseNumber(value, f) && f >= 0 && f <= 100 && f == std::floor(f); if (ok) g.fade = static_cast<std::uint8_t>(f); }
        else if (key.size() == 5 && key.compare(0, 4, "vmem") == 0 && key[4] >= '1' && key[4] < '1' + static_cast<int>(VWindowMemories)) {
            const auto i = static_cast<std::size_t>(key[4] - '1');
            ok = parseView(value, p.vmem[i]); if (ok) p.vmemUsed |= static_cast<std::uint8_t>(1u << i);
        } else if (key == "table") {
            double t[3]; ok = numbers(value, ';', 3, t) && t[2] > 0 && t[0] <= t[1];
            if (ok) { p.tableStart = t[0]; p.tableEnd = t[1]; p.tableStep = t[2]; }
        } else if (key == "overlay") ok = unescape(value, p.overlay) && p.overlay.size() <= 64;
        else if (key == "s") {
            if (p.sketch.size() >= MaxSketchItems) return "troppi elementi SKETCH";
            SketchItem item;
            err = parseSketch(value, item);
            if (!err.empty()) return err;
            p.sketch.push_back(std::move(item));
        } else if (key.size() >= 2 && key[0] == 'f' && std::isdigit(static_cast<unsigned char>(key[1]))) {
            err = parseFunction(key, value, p.functions, seen);
            if (!err.empty()) return err;
        }
        // Other keys (generator, keys of newer minor revisions) are ignored.
        if (!ok) return "valore non valido per '" + key + "'";
    }
    if (!ended) return "marcatore 'end' mancante";
    if (!haveView) return "V-Window mancante";
    if (p.settings.background == BackgroundFile && p.settings.backgroundFile.empty()) return "immagine di sfondo mancante";
    out = std::move(p);
    return {};
}

std::string encodeMemory(const std::array<GraphFunction, MaxGraphFunctions>& fns, const ViewWindow& view) {
    std::string s = "PCGMEM " + std::to_string(FormatVersion) + "\n";
    s += "view=" + viewText(view) + "\n";
    for (std::size_t i = 0; i < fns.size(); ++i) if (!fns[i].empty()) s += functionLine(i, fns[i]);
    return close(std::move(s));
}
std::string decodeMemory(const std::string& text, std::array<GraphFunction, MaxGraphFunctions>& fns, ViewWindow& view) {
    std::vector<std::string> lines;
    std::string err = open(text, "PCGMEM", lines);
    if (!err.empty()) return err;
    std::array<GraphFunction, MaxGraphFunctions> f;
    for (std::size_t i = 0; i < f.size(); ++i) f[i] = emptyGraphFunction(i);
    ViewWindow v;
    std::uint32_t seen = 0;
    bool haveView = false;
    for (std::size_t li = 1; li < lines.size(); ++li) {
        const std::string& line = lines[li];
        if (line.empty() || line == "end") continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) return "riga non valida";
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "view") { if (!parseView(value, v)) return "V-Window non valida"; haveView = true; }
        else if (key.size() >= 2 && key[0] == 'f') { err = parseFunction(key, value, f, seen); if (!err.empty()) return err; }
    }
    if (!haveView) return "V-Window mancante";
    fns = std::move(f); view = v;
    return {};
}
} // namespace fx::graphfile
