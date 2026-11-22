#pragma once
// File tipizzati delle modalita' (v1.15.0, F3).
//
// Una tabella dei tipi dice per ogni modalita' cartella, estensioni, regola dei
// nomi, formato e schede: eActivity su entrambe (PICOCALC/EACT), Program,
// Spreadsheet, Python, Geometry e Picture Plot solo sulla Pimoroni Pico Plus
// 2 W. I file binari hanno un'intestazione con magia, versione del formato e
// CRC-32 dei dati; i file di testo (sorgenti, script) sono UTF-8 senza
// intestazione, cosi' si modificano anche sul PC. TypedStore e' l'API comune
// (elenco, apri, salva, salva come, rinomina, elimina) sopra files::FileSystem
// (microSD o LittleFS): ogni scrittura passa da writeAtomic. Formati:
// docs/FILE_FORMATS.md.
#include "file_store.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::files {

enum class FileType : std::uint8_t { EActivity = 0, Program, Spreadsheet, Python, Geometry, PicturePlot, Count };
constexpr std::size_t FileTypeCount = static_cast<std::size_t>(FileType::Count);

struct FileTypeInfo {
    FileType type;
    const char* name;                    // "eActivity"
    const char* folder;                  // "PICOCALC/EACT"
    const char* const* extensions;       // nullptr-terminated, la prima e' quella dei file nuovi
    bool binary;                         // intestazione + CRC (false: testo UTF-8)
    std::uint32_t magic;                 // primi 4 byte dei file binari
    std::uint16_t formatVersion;         // versione scritta (e la piu' nuova che si sa leggere)
    std::size_t maxNameLength;
    std::size_t maxBytes;                // dati di un file su questa scheda
    std::uint8_t boards;                 // app::boards::
};
const FileTypeInfo& fileType(FileType type) noexcept;
// True se il tipo esiste su questa scheda (cartella e modalita' presenti).
bool fileTypeAvailable(FileType type) noexcept;

// Regole dei nomi (senza estensione).
// eActivity (manuale 10-5): 1-8 caratteri A-Z 0-9 { } ' ~ (le minuscole si
// scrivono maiuscole); gli altri tipi: validName() di file_store (A-Z a-z 0-9 _ -).
bool validTypedName(FileType type, const std::string& stem);
bool typedNameChar(FileType type, char c) noexcept;     // carattere ammesso mentre si digita
char typedNameCase(FileType type, char c) noexcept;     // 'a' -> 'A' per eActivity

// Intestazione dei file binari (16 byte, little endian):
//   u32 magia, u16 versione del formato, u16 flag (0), u32 lunghezza dei dati, u32 CRC-32 dei dati.
constexpr std::size_t HeaderBytes = 16;
std::vector<std::uint8_t> encodeTypedFile(FileType type, const std::vector<std::uint8_t>& payload);
// Ok, Corrupt (magia, lunghezza, CRC, UTF-8 non valido o NUL nei file di testo),
// NotSupported (formato piu' nuovo), TooLarge (oltre maxBytes).
Status decodeTypedFile(FileType type, const std::vector<std::uint8_t>& raw, std::vector<std::uint8_t>& payload,
                       std::uint16_t* version = nullptr);
bool validUtf8Text(const std::uint8_t* data, std::size_t size) noexcept;

struct TypedEntry {
    std::string stem;                    // nome senza estensione ("FISICA")
    std::string file;                    // nome del file ("FISICA.pceact")
    std::uint32_t size{};
};

class TypedStore {
public:
    explicit TypedStore(FileSystem& fs) noexcept : fs_(fs) {}
    FileSystem& fileSystem() noexcept { return fs_; }
    // File del tipo nella sua cartella, in ordine di nome; i residui di un
    // salvataggio interrotto (.tmp/.bak) vengono prima recuperati da recover().
    Status list(FileType type, std::vector<TypedEntry>& out);
    // `file` e' il nome con l'estensione (TypedEntry::file) o solo il nome:
    // in questo caso si usa la prima estensione del tipo.
    Status open(FileType type, const std::string& file, std::vector<std::uint8_t>& payload, std::uint16_t* version = nullptr);
    Status save(FileType type, const std::string& file, const std::vector<std::uint8_t>& payload);
    // Exists se il file c'e' gia' e overwrite e' false (la calcolatrice chiede conferma).
    Status saveAs(FileType type, const std::string& file, const std::vector<std::uint8_t>& payload, bool overwrite);
    Status rename(FileType type, const std::string& from, const std::string& toStem);
    Status remove(FileType type, const std::string& file);
    bool exists(FileType type, const std::string& file);
    // Nome completo: stem + estensione (la prima se `file` non ne ha una del tipo).
    static std::string fileName(FileType type, const std::string& file);
    static std::string path(FileType type, const std::string& file);
private:
    Status check(FileType type, const std::string& file) const;
    FileSystem& fs_;
};

// Ricerca della fx-CG50 (manuale 10-5): il testo deve coincidere con l'inizio
// del nome ("IT" trova ITXX, IT123, non XXIT). Indice del primo file, -1 se nessuno.
int searchPrefix(const std::vector<TypedEntry>& entries, const std::string& prefix);
} // namespace fx::files
