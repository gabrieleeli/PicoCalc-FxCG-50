// File CSV di liste e matrici (vedi csv_io.hpp).
#include "csv_io.hpp"
#include "expression.hpp"
#include "limits.hpp"
#include "workspace.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace fx::csv {
namespace {
[[noreturn]] void formatError(const std::string& what) { throw MathError("Format ERROR: " + what); }

// Numero con 15 cifre significative e il simbolo decimale scelto.
std::string number(double v, const Format& f) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.15g", v == 0.0 ? 0.0 : v);
    std::string s(buf);
    if (f.decimal != '.') std::replace(s.begin(), s.end(), '.', f.decimal);
    return s;
}
std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
    return s.substr(a, b - a);
}
// Campo numerico; false per un campo vuoto. Lancia per testo non numerico.
bool parseField(std::string field, const Format& f, double& out, std::size_t row, std::size_t col) {
    field = trim(field);
    if (field.size() >= 2 && (field.front() == '"' || field.front() == '\'') && field.back() == field.front())
        field = trim(field.substr(1, field.size() - 2));
    if (field.empty()) return false;
    if (f.decimal != '.') {
        if (field.find('.') != std::string::npos) formatError("simbolo decimale diverso da quello impostato");
        std::replace(field.begin(), field.end(), f.decimal, '.');
    }
    // Solo numeri: cifre, segno, punto, esponente (niente nomi come "inf" o "nan").
    for (char c : field)
        if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E'))
            formatError("valore non numerico alla riga " + std::to_string(row + 1) + ", colonna " + std::to_string(col + 1));
    char* end = nullptr;
    const double v = std::strtod(field.c_str(), &end);
    if (end != field.c_str() + field.size() || !std::isfinite(v))
        formatError("valore non numerico alla riga " + std::to_string(row + 1) + ", colonna " + std::to_string(col + 1));
    out = v == 0.0 ? 0.0 : v;
    return true;
}
} // namespace

std::string validate(const Format& f) {
    if (f.separator != ',' && f.separator != ';') return "separatore , oppure ;";
    if (f.decimal != '.' && f.decimal != ',') return "simbolo decimale . oppure ,";
    if (f.separator == f.decimal) return "separatore e simbolo decimale devono essere diversi";
    return {};
}
Format formatOf(const Workspace& ws) { return Format{ws.settings.csvSeparator, ws.settings.csvDecimal}; }

std::size_t maxFileBytes() noexcept {
    // Il caso piu' grande: tutte le liste o la matrice piu' grande, ~24 byte per cella.
    const std::size_t cells = std::max(limits::kListStoreElements, limits::kMatrixElements);
    return cells * 24 + 4096;
}

std::string encodeColumns(const std::vector<const std::vector<double>*>& columns, const Format& f) {
    std::size_t rows = 0;
    for (const auto* c : columns) rows = std::max(rows, c ? c->size() : 0);
    std::string out;
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t k = 0; k < columns.size(); ++k) {
            if (k) out += f.separator;
            if (columns[k] && r < columns[k]->size()) out += number((*columns[k])[r], f);
        }
        out += "\r\n";
    }
    return out;
}
std::string encodeMatrix(const CalcValue& m, const Format& f) {
    if (m.empty() || !m.isMatrixLike()) throw MathError("Dimension ERROR: memoria non definita");
    if (m.complex() && std::any_of(m.im.begin(), m.im.end(), [](double d) { return d != 0.0; }))
        throw MathError("Complex Number In Data: i file CSV contengono solo numeri reali");
    std::string out;
    for (std::size_t r = 0; r < m.rows; ++r) {
        for (std::size_t c = 0; c < m.cols; ++c) {
            if (c) out += f.separator;
            out += number(m.re[r * m.cols + c], f);
        }
        out += "\r\n";
    }
    return out;
}

namespace {
// Fine della riga che inizia in `at` (CR, LF, CRLF o fine del testo) e inizio della successiva.
std::size_t lineEnd(const std::uint8_t* data, std::size_t size, std::size_t at, std::size_t& next) {
    std::size_t e = at;
    while (e < size && data[e] != '\r' && data[e] != '\n') ++e;
    next = e < size ? (data[e] == '\r' && e + 1 < size && data[e + 1] == '\n' ? e + 2 : e + 1) : e;
    return e;
}
} // namespace

// v1.13.1 (E06): due passate sul testo. La prima conta righe e colonne senza
// copiare nulla, cosi' un file entro maxFileBytes ma con troppe righe o colonne
// da' Dimension ERROR prima di allocare (prima una stringa per campo e per riga:
// centinaia di KB sul Pico 2 W); la seconda legge i campi nella tabella.
// Righe, intestazione, tabulatori ed errori sono quelli di prima.
Table decode(const std::uint8_t* data, std::size_t size, const Format& f, std::size_t maxRows, std::size_t maxCols, std::size_t maxCells) {
    if (const std::string bad = validate(f); !bad.empty()) throw MathError("Argument ERROR: formato CSV non valido (" + bad + ")");
    // 1) Righe: CR, LF o CRLF; le righe vuote (spazi e tabulatori) in fondo non contano.
    //    Riga 1 con virgolette: intestazione, ignorata (fx-CG50).
    bool header = false;
    std::size_t lines = 0, lastFilled = 0;          // lastFilled: righe fino all'ultima non vuota
    std::size_t colsSoFar = 0, cols = 0;
    bool tabSoFar = false, tab = false;
    for (std::size_t at = 0; at < size;) {
        std::size_t next = 0;
        const std::size_t end = lineEnd(data, size, at, next);
        bool quote = false, tabHere = false, filled = false;
        std::size_t fields = 1;
        for (std::size_t i = at; i < end; ++i) {
            const char c = static_cast<char>(data[i]);
            if (c == '\0') formatError("file binario, non CSV");
            if (c == '"' || c == '\'') quote = true;
            if (c == '\t') tabHere = true;
            else if (c != ' ') filled = true;
            if (c == f.separator) ++fields;
        }
        if (lines == 0 && quote) header = true;
        else { colsSoFar = std::max(colsSoFar, fields); tabSoFar = tabSoFar || tabHere; }
        ++lines;
        if (filled) { lastFilled = lines; cols = colsSoFar; tab = tabSoFar; }
        at = next;
    }
    const std::size_t first = header && lastFilled >= 1 ? 1 : 0;
    // Un CSV con il tabulatore come separatore non e' supportato.
    if (tab) formatError("il tabulatore non e' un separatore ammesso");
    Table t;
    t.rows = lastFilled - first;
    t.cols = cols;
    if (t.rows == 0) formatError("file vuoto");
    if (t.rows > maxRows || t.cols > maxCols)
        throw DimensionError("Dimension ERROR: il file ha " + std::to_string(t.rows) + " righe e " + std::to_string(t.cols) +
                             " colonne (massimo " + std::to_string(maxRows) + " x " + std::to_string(maxCols) + ")");
    if (t.rows * t.cols > maxCells)
        throw DimensionError("Dimension ERROR: il file ha " + std::to_string(t.rows * t.cols) + " celle (massimo " + std::to_string(maxCells) + ")");
    // 2) Campi.
    t.cells.assign(t.rows * t.cols, 0.0);
    t.present.assign(t.rows * t.cols, 0);
    std::size_t at = 0, next = 0;
    if (first) { (void)lineEnd(data, size, at, next); at = next; }
    for (std::size_t r = 0; r < t.rows; ++r) {
        const std::size_t end = lineEnd(data, size, at, next);
        std::size_t start = at, c = 0;
        for (std::size_t i = at; i <= end; ++i) {
            if (i < end && static_cast<char>(data[i]) != f.separator) continue;
            double v = 0;
            const std::string field(reinterpret_cast<const char*>(data + start), i - start);
            if (parseField(field, f, v, r + first, c)) { t.cells[r * t.cols + c] = v; t.present[r * t.cols + c] = 1; }
            ++c; start = i + 1;
        }
        at = next;
    }
    return t;
}

std::string pathOf(const std::string& name) { return files::join(Folder, name + Extension); }

files::Status list(files::FileSystem& fs, std::vector<std::string>& names) {
    names.clear();
    std::vector<files::Entry> entries;
    const files::Status st = fs.list(Folder, entries);
    if (st == files::Status::NotFound) return files::Status::Ok;      // cartella non ancora creata
    if (st != files::Status::Ok) return st;
    for (const auto& e : entries)
        if (!e.directory && files::hasExtension(e.name, Extension)) names.push_back(files::stem(e.name));
    std::sort(names.begin(), names.end());
    return files::Status::Ok;
}
files::Status save(files::FileSystem& fs, const std::string& name, const std::string& text) {
    if (!files::validName(name, MaxNameLength)) throw MathError("Argument ERROR: nome del file di 1-8 caratteri A-Z, 0-9, _ -");
    if (text.size() > maxFileBytes()) return files::Status::TooLarge;
    files::Status st = fs.makeDirs(Folder);
    if (st != files::Status::Ok) return st;
    return files::writeAtomic(fs, pathOf(name), reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
}
files::Status load(files::FileSystem& fs, const std::string& name, std::vector<std::uint8_t>& out) {
    if (!files::validName(name, MaxNameLength)) return files::Status::InvalidName;
    (void)files::recover(fs, Folder);          // completa o annulla un salvataggio interrotto
    return files::readAll(fs, pathOf(name), out, maxFileBytes());
}

std::size_t loadLists(Workspace& ws, std::size_t first, const Table& t) {
    if (first >= Workspace::ListCount) throw MathError("Argument ERROR: List da 1 a 26");
    if (first + t.cols > Workspace::ListCount)
        throw DimensionError("Dimension ERROR: " + std::to_string(t.cols) + " colonne non entrano da List " + std::to_string(first + 1) + " a List 26");
    std::vector<std::vector<double>> columns(t.cols);
    for (std::size_t c = 0; c < t.cols; ++c) {
        std::size_t length = 0;
        while (length < t.rows && t.has(length, c)) ++length;
        for (std::size_t r = length; r < t.rows; ++r)
            if (t.has(r, c)) formatError("cella vuota dentro la colonna " + std::to_string(c + 1));
        if (length > Workspace::MaxListLength)
            throw DimensionError("Dimension ERROR: massimo " + std::to_string(Workspace::MaxListLength) + " elementi in una lista");
        columns[c].reserve(length);
        for (std::size_t r = 0; r < length; ++r) columns[c].push_back(t.at(r, c));
    }
    // Budget di tutte le liste dopo il caricamento (tutto o niente).
    std::size_t total = ws.listElements();
    for (std::size_t c = 0; c < t.cols; ++c) total = total - ws.lists[first + c].size() + columns[c].size();
    if (total > Workspace::MaxListElements)
        throw MathError("Memory ERROR: memoria delle liste piena (" + std::to_string(total) + " elementi, massimo " +
                        std::to_string(Workspace::MaxListElements) + ")");
    for (std::size_t c = 0; c < t.cols; ++c) ws.lists[first + c].swap(columns[c]);
    return t.cols;
}
void loadMatrix(Workspace& ws, bool vectors, std::size_t slot, const Table& t) {
    for (std::size_t i = 0; i < t.present.size(); ++i)
        if (!t.present[i]) formatError("una matrice non ha celle vuote (riga " + std::to_string(i / t.cols + 1) + ")");
    if (t.rows > limits::kMatrixMaxDim || t.cols > limits::kMatrixMaxDim || t.rows * t.cols > limits::kMatrixElements)
        throw DimensionError("Dimension ERROR: matrice troppo grande (massimo " + std::to_string(limits::kMatrixMaxDim) +
                             " per lato e " + std::to_string(limits::kMatrixElements) + " elementi)");
    CalcValue m = CalcValue::matrix(t.rows, t.cols, vectors ? ValueType::Vector : ValueType::Matrix);
    m.re = t.cells;
    if (vectors) ws.storeVector(slot, std::move(m));
    else ws.storeMatrix(slot, std::move(m));
}
std::string encodeLists(const Workspace& ws) {
    std::size_t last = 0;
    for (std::size_t i = 0; i < Workspace::ListCount; ++i) if (!ws.lists[i].empty()) last = i + 1;
    if (last == 0) throw MathError("Dimension ERROR: tutte le liste sono vuote");
    std::vector<const std::vector<double>*> columns;
    for (std::size_t i = 0; i < last; ++i) columns.push_back(&ws.lists[i]);
    return encodeColumns(columns, formatOf(ws));
}
} // namespace fx::csv
