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

Table decode(const std::uint8_t* data, std::size_t size, const Format& f, std::size_t maxRows, std::size_t maxCols) {
    if (const std::string bad = validate(f); !bad.empty()) throw MathError("Argument ERROR: formato CSV non valido (" + bad + ")");
    // Righe: CR, LF o CRLF.
    std::vector<std::string> lines;
    std::string cur;
    for (std::size_t i = 0; i < size; ++i) {
        const char c = static_cast<char>(data[i]);
        if (c == '\r' || c == '\n') {
            lines.push_back(cur); cur.clear();
            if (c == '\r' && i + 1 < size && data[i + 1] == '\n') ++i;
            continue;
        }
        if (c == '\0') formatError("file binario, non CSV");
        cur += c;
    }
    if (!cur.empty()) lines.push_back(cur);
    while (!lines.empty() && trim(lines.back()).empty()) lines.pop_back();
    // Riga 1 con virgolette: intestazione, ignorata (fx-CG50).
    std::size_t first = 0;
    if (!lines.empty() && (lines[0].find('"') != std::string::npos || lines[0].find('\'') != std::string::npos)) first = 1;
    // Un CSV con il tabulatore come separatore non e' supportato.
    for (std::size_t i = first; i < lines.size(); ++i)
        if (lines[i].find('\t') != std::string::npos) formatError("il tabulatore non e' un separatore ammesso");
    Table t;
    t.rows = lines.size() - first;
    if (t.rows == 0) formatError("file vuoto");
    std::vector<std::vector<std::string>> fields(t.rows);
    for (std::size_t r = 0; r < t.rows; ++r) {
        const std::string& line = lines[first + r];
        std::size_t start = 0;
        for (std::size_t i = 0; i <= line.size(); ++i) {
            if (i == line.size() || line[i] == f.separator) { fields[r].push_back(line.substr(start, i - start)); start = i + 1; }
        }
        t.cols = std::max(t.cols, fields[r].size());
    }
    if (t.rows > maxRows || t.cols > maxCols)
        throw DimensionError("Dimension ERROR: il file ha " + std::to_string(t.rows) + " righe e " + std::to_string(t.cols) +
                             " colonne (massimo " + std::to_string(maxRows) + " x " + std::to_string(maxCols) + ")");
    t.cells.assign(t.rows * t.cols, 0.0);
    t.present.assign(t.rows * t.cols, 0);
    for (std::size_t r = 0; r < t.rows; ++r)
        for (std::size_t c = 0; c < fields[r].size(); ++c) {
            double v = 0;
            if (parseField(fields[r][c], f, v, r + first, c)) { t.cells[r * t.cols + c] = v; t.present[r * t.cols + c] = 1; }
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
