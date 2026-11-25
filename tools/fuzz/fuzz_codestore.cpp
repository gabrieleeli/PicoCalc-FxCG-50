// v1.20.0 — Fuzzing dell'archivio dei programmi (gestore dei file di Program):
// sequenze di operazioni (salvataggio atomico, file grandi a blocchi, NEW,
// RENAME, COPY, MOVE, DEL, DEL-ALL, SEARCH, password, migrazione) su una SD in
// RAM con guasti iniettati (SD tolta, piena, interruzione dopo n byte, n-esima
// operazione fallita). Invariante: dopo il ripristino un file salvato con
// successo e mai cancellato o rinominato e' intero (vecchio o nuovo contenuto).
#include "code_store.hpp"
#include "file_store.hpp"
#include <cstdint>
#include <map>
#include <string>
using namespace fx;
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    if (n > 3000) return 0;
    files::RamFileSystem fs(256u * 1024u);
    code::Store st(&fs);
    if (st.ready() != files::Status::Ok) return 0;
    std::size_t i = 0;
    auto byte = [&]() -> std::uint8_t { return i < n ? d[i++] : 0; };
    static const char* const kNames[] = {"a.cpp", "b.c", "Prog", "c.cs", "dati.txt", "x_1.cpp", "Sub", "z.java"};
    static const char* const kDirs[] = {"PICOCALC/CODE/CPP", "PICOCALC/CODE/C", "PICOCALC/CODE/CPP/Prog", "PICOCALC/CODE/CS", "PICOCALC/CODE"};
    auto name = [&]() { return std::string(kNames[byte() % 8]); };
    auto dir = [&]() { return std::string(kDirs[byte() % 5]); };
    // percorso -> contenuti ammessi dopo il ripristino: quello di prima e quelli scritti (anche non
    // riusciti: il guasto puo' arrivare dopo la rinomina), mai un file troncato o mescolato
    std::map<std::string, std::vector<std::string>> saved;
    while (i < n) {
        const std::uint8_t op = byte() % 18;
        const std::string p = dir() + "/" + name();
        switch (op) {
        case 0: case 1: {
            std::string text(byte() * 7u % 4000u, static_cast<char>('a' + byte() % 26));
            std::string before;
            const bool had = st.readText(p, before) == files::Status::Ok;
            const bool ok = st.writeText(p, text) == files::Status::Ok;
            auto it = saved.find(p);
            if (ok) saved[p] = {before, text};
            else if (it != saved.end()) it->second.push_back(text);
            else if (had) saved[p] = {before, text};
            break;
        }
        case 2: { std::string out; (void)st.readText(p, out); break; }
        case 3: { std::string out; std::uint64_t total = 0; (void)st.readBlock(p, byte() * 13u, byte() * 17u, out, &total); break; }
        case 4: if (st.spliceBlock(p, byte() * 11u, byte() * 5u, std::string(byte() % 200, 'x')) == files::Status::Ok) saved.erase(p); break;
        case 5: { std::string out; (void)st.newFile(static_cast<code::Lang>(byte() % code::LangCount), dir(), name(), out); break; }
        case 6: { std::string out; (void)st.newProject(static_cast<code::Lang>(byte() % code::LangCount), dir(), "P" + std::to_string(byte() % 4), out); break; }
        case 7: (void)st.makeFolder(dir() + "/" + name()); break;
        case 8: if (st.rename(p, name()) == files::Status::Ok) saved.erase(p); break;
        case 9: { std::string out; (void)st.copy(p, dir(), &out); break; }
        case 10: if (st.move(p, dir()) == files::Status::Ok) saved.erase(p); break;
        case 11: (void)st.remove(p); saved.erase(p); break;
        case 12: { const std::string dd = dir(); (void)st.removeAll(dd); for (auto it = saved.begin(); it != saved.end();) it = it->first.rfind(dd + "/", 0) == 0 ? saved.erase(it) : std::next(it); break; }
        case 13: (void)st.search(dir(), std::string(1, static_cast<char>('a' + byte() % 26))); break;
        case 14: (void)st.setPassword(p, byte() % 2 ? std::string("pw") : std::string()); (void)st.checkPassword(p, "pw"); break;
        case 15: { std::vector<code::Item> items; (void)st.list(dir(), items, static_cast<code::SortBy>(byte() % 3)); break; }
        case 16: {
            // guasti: interruzione dopo n byte, n-esima operazione, SD tolta o piena
            const std::uint8_t g = byte();
            if (g % 4 == 0) fs.failAfterBytes(byte() * 31u);
            else if (g % 4 == 1) fs.failOperation(1 + byte() % 6);
            else if (g % 4 == 2) fs.setPresent(byte() % 2);
            else fs.setCapacity(fs.used() + byte() * 64u);
            break;
        }
        default: {
            // ripristino: corrente, SD presente, spazio; poi il controllo dei file salvati
            fs.powerOn();
            fs.setPresent(true);
            fs.setCapacity(256u * 1024u);
            for (const char* dd : kDirs) { std::vector<code::Item> items; (void)st.list(dd, items); }
            for (auto it = saved.begin(); it != saved.end();) {
                std::string now;
                if (st.readText(it->first, now) != files::Status::Ok) { it = saved.erase(it); continue; }   // cancellato da un'operazione dopo
                bool known = false;
                for (const std::string& c : it->second) if (c == now) known = true;
                if (!known) __builtin_trap();                                                                // file troncato o misto
                it->second = {now};
                ++it;
            }
            break;
        }
        }
    }
    return 0;
}
