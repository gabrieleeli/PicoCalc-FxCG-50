// File tipizzati delle modalita' (v1.15.0, F3): vedi typed_files.hpp.
#include "typed_files.hpp"
#include "app_registry.hpp"
#include "board_config.hpp"
#include "features.hpp"
#include "limits.hpp"
#include "storage.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fx::files {
namespace {
namespace b = app::boards;
constexpr std::uint32_t fourcc(char a, char b2, char c, char d) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) | (static_cast<std::uint32_t>(static_cast<unsigned char>(b2)) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16) | (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
}
const char* const kEactExt[] = {".pceact", nullptr};
const char* const kProgExt[] = {".cpp", ".c", ".cs", nullptr};
const char* const kSheetExt[] = {".sheet", nullptr};
const char* const kPyExt[] = {".py", nullptr};
const char* const kGeomExt[] = {".pcgeo", nullptr};
const char* const kPplotExt[] = {".pcplot", nullptr};
constexpr FileTypeInfo kTypes[FileTypeCount] = {
    {FileType::EActivity, "eActivity", "PICOCALC/EACT", kEactExt, true, fourcc('P', 'C', 'E', 'A'), 1, 8, limits::kEActivityDocumentBytes, b::Both},
    {FileType::Program, "Program", "PICOCALC/PROG", kProgExt, false, 0, 1, 8, limits::kTextFileBytes, b::PicoPlus2W},
    {FileType::Spreadsheet, "Spreadsheet", "PICOCALC/SHEET", kSheetExt, true, fourcc('P', 'C', 'S', 'H'), 1, 8, limits::kTypedFileBytes, b::PicoPlus2W},
    {FileType::Python, "Python", "PICOCALC/PY", kPyExt, false, 0, 1, 8, limits::kTextFileBytes, b::PicoPlus2W},
    {FileType::Geometry, "Geometry", "PICOCALC/GEOM", kGeomExt, true, fourcc('P', 'C', 'G', 'O'), 1, 8, limits::kTypedFileBytes, b::PicoPlus2W},
    {FileType::PicturePlot, "Picture Plot", "PICOCALC/PPLOT", kPplotExt, true, fourcc('P', 'C', 'P', 'P'), 1, 8, limits::kTypedFileBytes, b::PicoPlus2W},
};
constexpr std::uint8_t kBoard = board::config::kIsPicoPlus2W ? b::PicoPlus2W : b::Pico2W;

void put16(std::vector<std::uint8_t>& o, std::uint16_t v) { o.push_back(static_cast<std::uint8_t>(v)); o.push_back(static_cast<std::uint8_t>(v >> 8)); }
void put32(std::vector<std::uint8_t>& o, std::uint32_t v) { put16(o, static_cast<std::uint16_t>(v)); put16(o, static_cast<std::uint16_t>(v >> 16)); }
std::uint32_t get32(const std::uint8_t* p) { return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }
std::uint16_t get16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

// Estensione del tipo presente in `file` (indice), -1 se nessuna.
int extensionIndex(FileType type, const std::string& file) {
    const FileTypeInfo& t = fileType(type);
    for (int i = 0; t.extensions[i]; ++i) if (hasExtension(file, t.extensions[i])) return i;
    return -1;
}
} // namespace

const FileTypeInfo& fileType(FileType type) noexcept {
    const auto i = static_cast<std::size_t>(type);
    return kTypes[i < FileTypeCount ? i : 0];
}

bool fileTypeAvailable(FileType type) noexcept {
    if ((fileType(type).boards & kBoard) == 0) return false;
    switch (type) {
    case FileType::EActivity: return features::kEActivity;
    case FileType::Program: return features::kProgram;
    case FileType::Spreadsheet: return features::kSpreadsheet;
    case FileType::Python: return features::kPython;
    case FileType::Geometry: return features::kGeometry;
    case FileType::PicturePlot: return features::kPicturePlot;
    default: return false;
    }
}

bool typedNameChar(FileType type, char c) noexcept {
    const auto u = static_cast<unsigned char>(c);
    if (type == FileType::EActivity) return std::isalnum(u) || c == '{' || c == '}' || c == '\'' || c == '~';
    return std::isalnum(u) || c == '_' || c == '-';
}
char typedNameCase(FileType type, char c) noexcept {
    return type == FileType::EActivity ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
}
bool validTypedName(FileType type, const std::string& stem) {
    const FileTypeInfo& t = fileType(type);
    if (stem.empty() || stem.size() > t.maxNameLength) return false;
    if (type == FileType::EActivity) {
        for (char c : stem) {
            const auto u = static_cast<unsigned char>(c);
            if (!(std::isupper(u) || std::isdigit(u) || c == '{' || c == '}' || c == '\'' || c == '~')) return false;
        }
        return true;
    }
    return validName(stem, t.maxNameLength);
}

bool validUtf8Text(const std::uint8_t* d, std::size_t n) noexcept {
    std::size_t i = 0;
    while (i < n) {
        const std::uint8_t c = d[i];
        if (c == 0) return false;
        std::size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
        if (len == 0 || i + len > n) return false;
        std::uint32_t cp = len == 1 ? c : len == 2 ? (c & 0x1Fu) : len == 3 ? (c & 0x0Fu) : (c & 0x07u);
        for (std::size_t k = 1; k < len; ++k) {
            if ((d[i + k] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (d[i + k] & 0x3Fu);
        }
        // forme troppo lunghe, surrogati e valori oltre U+10FFFF
        if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        i += len;
    }
    return true;
}

std::vector<std::uint8_t> encodeTypedFile(FileType type, const std::vector<std::uint8_t>& payload) {
    const FileTypeInfo& t = fileType(type);
    if (!t.binary) return payload;
    std::vector<std::uint8_t> out;
    out.reserve(HeaderBytes + payload.size());
    put32(out, t.magic); put16(out, t.formatVersion); put16(out, 0);
    put32(out, static_cast<std::uint32_t>(payload.size()));
    put32(out, storage::crc32(payload.data(), payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

Status decodeTypedFile(FileType type, const std::vector<std::uint8_t>& raw, std::vector<std::uint8_t>& payload, std::uint16_t* version) {
    const FileTypeInfo& t = fileType(type);
    if (!t.binary) {
        if (raw.size() > t.maxBytes) return Status::TooLarge;
        if (!validUtf8Text(raw.data(), raw.size())) return Status::Corrupt;
        payload = raw;
        if (version) *version = t.formatVersion;
        return Status::Ok;
    }
    if (raw.size() < HeaderBytes || get32(raw.data()) != t.magic) return Status::Corrupt;
    const std::uint16_t v = get16(raw.data() + 4), flags = get16(raw.data() + 6);
    const std::uint32_t len = get32(raw.data() + 8), crc = get32(raw.data() + 12);
    if (v == 0 || flags != 0) return Status::Corrupt;
    if (v > t.formatVersion) return Status::NotSupported;
    if (len != raw.size() - HeaderBytes) return Status::Corrupt;
    if (len > t.maxBytes) return Status::TooLarge;
    if (storage::crc32(raw.data() + HeaderBytes, len) != crc) return Status::Corrupt;
    payload.assign(raw.begin() + static_cast<std::ptrdiff_t>(HeaderBytes), raw.end());
    if (version) *version = v;
    return Status::Ok;
}

std::string TypedStore::fileName(FileType type, const std::string& file) {
    if (extensionIndex(type, file) >= 0) return file;
    return file + fileType(type).extensions[0];
}
std::string TypedStore::path(FileType type, const std::string& file) { return join(fileType(type).folder, fileName(type, file)); }

Status TypedStore::check(FileType type, const std::string& file) const {
    if (!fileTypeAvailable(type)) return Status::NotSupported;
    const std::string name = fileName(type, file);
    if (!validTypedName(type, stem(name))) return Status::InvalidName;
    return Status::Ok;
}

Status TypedStore::list(FileType type, std::vector<TypedEntry>& out) {
    out.clear();
    if (!fileTypeAvailable(type)) return Status::NotSupported;
    Status st = fs_.status();
    if (st != Status::Ok) return st;
    const FileTypeInfo& t = fileType(type);
    std::vector<Entry> entries;
    st = fs_.list(t.folder, entries);
    if (st == Status::NotFound) return Status::Ok;                 // cartella non ancora creata: nessun file
    if (st != Status::Ok) return st;
    (void)recover(fs_, t.folder);                                   // salvataggi interrotti
    entries.clear();
    st = fs_.list(t.folder, entries);
    if (st != Status::Ok) return st;
    for (const Entry& e : entries) {
        if (e.directory || extensionIndex(type, e.name) < 0) continue;
        const std::string s = stem(e.name);
        if (!validTypedName(type, s)) continue;
        out.push_back(TypedEntry{s, e.name, e.size});
    }
    std::sort(out.begin(), out.end(), [](const TypedEntry& a, const TypedEntry& b) { return a.file < b.file; });
    return Status::Ok;
}

Status TypedStore::open(FileType type, const std::string& file, std::vector<std::uint8_t>& payload, std::uint16_t* version) {
    Status st = check(type, file);
    if (st != Status::Ok) return st;
    std::vector<std::uint8_t> raw;
    const FileTypeInfo& t = fileType(type);
    st = readAll(fs_, path(type, file), raw, t.maxBytes + (t.binary ? HeaderBytes : 0));
    if (st != Status::Ok) return st;
    return decodeTypedFile(type, raw, payload, version);
}

Status TypedStore::save(FileType type, const std::string& file, const std::vector<std::uint8_t>& payload) {
    Status st = check(type, file);
    if (st != Status::Ok) return st;
    const FileTypeInfo& t = fileType(type);
    if (payload.size() > t.maxBytes) return Status::TooLarge;
    if (!t.binary && !validUtf8Text(payload.data(), payload.size())) return Status::Corrupt;
    st = fs_.makeDirs(t.folder);
    if (st != Status::Ok) return st;
    const std::vector<std::uint8_t> raw = encodeTypedFile(type, payload);
    return writeAtomic(fs_, path(type, file), raw.data(), raw.size());
}

Status TypedStore::saveAs(FileType type, const std::string& file, const std::vector<std::uint8_t>& payload, bool overwrite) {
    const Status st = check(type, file);
    if (st != Status::Ok) return st;
    if (!overwrite && exists(type, file)) return Status::Exists;
    return save(type, file, payload);
}

Status TypedStore::rename(FileType type, const std::string& from, const std::string& toStem) {
    Status st = check(type, from);
    if (st != Status::Ok) return st;
    if (!validTypedName(type, toStem)) return Status::InvalidName;
    const std::string src = fileName(type, from);
    const int ext = extensionIndex(type, src);
    const std::string dst = toStem + fileType(type).extensions[ext < 0 ? 0 : ext];
    if (dst == src) return Status::Ok;
    return fs_.rename(path(type, src), path(type, dst));
}

Status TypedStore::remove(FileType type, const std::string& file) {
    const Status st = check(type, file);
    if (st != Status::Ok) return st;
    return fs_.remove(path(type, file));
}

bool TypedStore::exists(FileType type, const std::string& file) {
    Entry e;
    return fs_.stat(path(type, file), e) == Status::Ok && !e.directory;
}

int searchPrefix(const std::vector<TypedEntry>& entries, const std::string& prefix) {
    if (prefix.empty()) return -1;
    std::string p = prefix;
    for (char& c : p) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (std::size_t i = 0; i < entries.size(); ++i) {
        std::string s = entries[i].stem;
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (s.compare(0, p.size(), p) == 0) return static_cast<int>(i);
    }
    return -1;
}
} // namespace fx::files
