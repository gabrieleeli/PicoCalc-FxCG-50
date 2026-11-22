// v1.15.0 (F3): file tipizzati delle modalita'.
//  - tabella dei tipi per scheda (cartelle, estensioni, regole dei nomi);
//  - intestazione con magia, versione e CRC; testo UTF-8 validato;
//  - API comune (elenco, apri, salva, salva come, rinomina, elimina) su
//    RamFileSystem con guasti, su FatFs (microSD simulata) e su LittleFS con
//    interruzione di alimentazione durante il salvataggio;
//  - FileDialog unico (nome con regola del tipo, sovrascrittura, ricerca,
//    conferma) e MEMORY con la vista dei file per tipo.
#include "app_rig.hpp"
#include "fatfs_store.hpp"
#include "flash_fs.hpp"
#include "sd_card.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include "typed_files.hpp"
#include <cstring>

using namespace fx;
using files::FileType;
using files::Status;

namespace {
std::vector<std::uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }
KeyEvent k(Key key) { KeyEvent e; e.key = key; return e; }
KeyEvent ch(char c) { KeyEvent e; e.key = Key::Char; e.ch = c; return e; }

// Il contratto dell'API su un file system qualunque.
void exercise(files::FileSystem& fs, const char* what) {
    files::TypedStore store(fs);
    std::vector<files::TypedEntry> list;
    CHECK(store.list(FileType::EActivity, list) == Status::Ok && list.empty());     // cartella non ancora creata
    const auto doc = bytes(std::string("eActivity: ") + what);
    CHECK(store.save(FileType::EActivity, "FISICA", doc) == Status::Ok);
    CHECK(store.exists(FileType::EActivity, "FISICA") && store.exists(FileType::EActivity, "FISICA.pceact"));
    std::vector<std::uint8_t> back;
    std::uint16_t version = 0;
    CHECK(store.open(FileType::EActivity, "FISICA", back, &version) == Status::Ok && back == doc && version == 1);
    // il file su disco ha l'intestazione: 16 byte + dati
    std::vector<std::uint8_t> raw;
    CHECK(files::readAll(fs, "PICOCALC/EACT/FISICA.pceact", raw, 1u << 20) == Status::Ok && raw.size() == doc.size() + files::HeaderBytes);
    CHECK(std::memcmp(raw.data(), "PCEA", 4) == 0);
    // SAVE·AS: un nome esistente vuole la conferma
    CHECK(store.saveAs(FileType::EActivity, "FISICA", doc, false) == Status::Exists);
    CHECK(store.saveAs(FileType::EActivity, "FISICA", bytes("v2"), true) == Status::Ok);
    CHECK(store.open(FileType::EActivity, "FISICA", back) == Status::Ok && back == bytes("v2"));
    CHECK(store.saveAs(FileType::EActivity, "CHIMICA", bytes("c"), false) == Status::Ok);
    CHECK(store.saveAs(FileType::EActivity, "{A}'~9", bytes("s"), false) == Status::Ok);   // caratteri del manuale 10-5
    CHECK(store.list(FileType::EActivity, list) == Status::Ok && list.size() == 3);
    CHECK(list[0].stem == "CHIMICA" && list[1].stem == "FISICA" && list[2].stem == "{A}'~9" && list[1].size == 2 + files::HeaderBytes);
    // nomi non validi
    CHECK(store.save(FileType::EActivity, "fisica", doc) == Status::InvalidName);          // minuscole
    CHECK(store.save(FileType::EActivity, "TOOLONGNAME", doc) == Status::InvalidName);
    CHECK(store.save(FileType::EActivity, "", doc) == Status::InvalidName);
    CHECK(store.save(FileType::EActivity, "A B", doc) == Status::InvalidName);
    // rinomina
    CHECK(store.rename(FileType::EActivity, "CHIMICA", "BIO") == Status::Ok);
    CHECK(store.rename(FileType::EActivity, "BIO", "FISICA") == Status::Exists);
    CHECK(store.rename(FileType::EActivity, "BIO", "bio") == Status::InvalidName);
    CHECK(!store.exists(FileType::EActivity, "CHIMICA") && store.exists(FileType::EActivity, "BIO"));
    // elimina
    CHECK(store.remove(FileType::EActivity, "BIO") == Status::Ok && !store.exists(FileType::EActivity, "BIO"));
    CHECK(store.remove(FileType::EActivity, "BIO") == Status::NotFound);
    // file corrotto: CRC
    CHECK(files::readAll(fs, "PICOCALC/EACT/FISICA.pceact", raw, 1u << 20) == Status::Ok);
    raw.back() ^= 0x40;
    CHECK(files::writeAtomic(fs, "PICOCALC/EACT/FISICA.pceact", raw.data(), raw.size()) == Status::Ok);
    CHECK(store.open(FileType::EActivity, "FISICA", back) == Status::Corrupt);
    // file estranei nella cartella: ignorati dall'elenco
    CHECK(files::writeAtomic(fs, "PICOCALC/EACT/NOTE.txt", raw.data(), 4) == Status::Ok);
    CHECK(files::writeAtomic(fs, "PICOCALC/EACT/bad name.pceact", raw.data(), 4) == Status::Ok);
    CHECK(store.list(FileType::EActivity, list) == Status::Ok && list.size() == 2);
}
} // namespace

int main() {
    // ---------------------------------------------------------------- F3.1 tabella dei tipi per scheda
    {
        const files::FileTypeInfo& e = files::fileType(FileType::EActivity);
        CHECK(std::string(e.folder) == "PICOCALC/EACT" && std::string(e.extensions[0]) == ".pceact" && e.binary && e.maxNameLength == 8);
        CHECK(e.boards == app::boards::Both);
        CHECK(files::fileTypeAvailable(FileType::EActivity));
        CHECK(e.maxBytes == limits::kEActivityDocumentBytes);
        const char* folders[] = {"PICOCALC/EACT", "PICOCALC/PROG", "PICOCALC/SHEET", "PICOCALC/PY", "PICOCALC/GEOM", "PICOCALC/PPLOT"};
        for (std::size_t i = 0; i < files::FileTypeCount; ++i) {
            const auto t = static_cast<FileType>(i);
            const files::FileTypeInfo& info = files::fileType(t);
            CHECK(info.type == t && std::string(info.folder) == folders[i]);
            // solo eActivity sul Pico 2 W
            CHECK(files::fileTypeAvailable(t) == (t == FileType::EActivity || board::config::kIsPicoPlus2W));
            CHECK(info.binary == (t != FileType::Program && t != FileType::Python));
        }
        CHECK(std::string(files::fileType(FileType::Program).extensions[1]) == ".c" && std::string(files::fileType(FileType::Program).extensions[2]) == ".cs");
        CHECK(files::TypedStore::fileName(FileType::Program, "HELLO") == "HELLO.cpp");
        CHECK(files::TypedStore::fileName(FileType::Program, "HELLO.cs") == "HELLO.cs");
        CHECK(files::TypedStore::path(FileType::Python, "demo") == "PICOCALC/PY/demo.py");
        // regole dei nomi
        CHECK(files::validTypedName(FileType::EActivity, "A1{}'~") && !files::validTypedName(FileType::EActivity, "a1"));
        CHECK(files::typedNameChar(FileType::EActivity, '~') && !files::typedNameChar(FileType::EActivity, '_'));
        CHECK(files::typedNameCase(FileType::EActivity, 'q') == 'Q' && files::typedNameCase(FileType::Python, 'q') == 'q');
        CHECK(files::validTypedName(FileType::Python, "my_py") && !files::validTypedName(FileType::Python, "my py"));
    }
    // ---------------------------------------------------------------- F3.2 intestazione, CRC, testo
    {
        const auto payload = bytes("ciao");
        const auto f = files::encodeTypedFile(FileType::EActivity, payload);
        CHECK(f.size() == 20 && f[4] == 1 && f[5] == 0 && f[8] == 4);
        std::vector<std::uint8_t> out;
        std::uint16_t v = 0;
        CHECK(files::decodeTypedFile(FileType::EActivity, f, out, &v) == Status::Ok && out == payload && v == 1);
        for (std::size_t i = 0; i < f.size(); ++i) {                                  // ogni byte alterato e' rifiutato
            auto g = f;
            g[i] ^= 0x01;
            const Status st = files::decodeTypedFile(FileType::EActivity, g, out);
            CHECK(st == Status::Corrupt || st == Status::NotSupported);
        }
        auto newer = f; newer[4] = 2;
        CHECK(files::decodeTypedFile(FileType::EActivity, newer, out) == Status::NotSupported);
        for (std::size_t cut = 0; cut < f.size(); ++cut) {
            const std::vector<std::uint8_t> g(f.begin(), f.begin() + static_cast<std::ptrdiff_t>(cut));
            CHECK(files::decodeTypedFile(FileType::EActivity, g, out) == Status::Corrupt);
        }
        // un tipo diverso: magia sbagliata
        CHECK(files::decodeTypedFile(FileType::Spreadsheet, f, out) == Status::Corrupt);
        // testo: nessuna intestazione, UTF-8 valido senza NUL
        const auto src = bytes("int main() { return 0; } // \xC3\xA8");
        CHECK(files::encodeTypedFile(FileType::Program, src) == src);
        CHECK(files::decodeTypedFile(FileType::Program, src, out) == Status::Ok && out == src);
        CHECK(files::decodeTypedFile(FileType::Program, bytes(std::string("a\0b", 3)), out) == Status::Corrupt);
        CHECK(!files::validUtf8Text(reinterpret_cast<const std::uint8_t*>("\xC3"), 1));
        CHECK(!files::validUtf8Text(reinterpret_cast<const std::uint8_t*>("\xC0\xAF"), 2));          // forma troppo lunga
        CHECK(!files::validUtf8Text(reinterpret_cast<const std::uint8_t*>("\xED\xA0\x80"), 3));      // surrogato
        CHECK(files::validUtf8Text(reinterpret_cast<const std::uint8_t*>("\xF0\x9F\x98\x80"), 4));
        // oltre il limite della scheda
        std::vector<std::uint8_t> big(limits::kEActivityDocumentBytes + 1, 'x');
        CHECK(files::decodeTypedFile(FileType::EActivity, files::encodeTypedFile(FileType::EActivity, big), out) == Status::TooLarge);
    }
    // ---------------------------------------------------------------- F3.3 API su RamFileSystem, con guasti
    {
        files::RamFileSystem fs;
        exercise(fs, "RAM");
        files::TypedStore store(fs);
        // scheda assente
        fs.setPresent(false);
        std::vector<files::TypedEntry> list;
        CHECK(store.list(FileType::EActivity, list) == Status::NoCard);
        fs.setPresent(true);
        // guasto di un'operazione durante il salvataggio: il file vecchio resta
        CHECK(store.save(FileType::EActivity, "SAFE", bytes("old")) == Status::Ok);
        for (int n = 1; n <= 8; ++n) {
            fs.failOperation(n);
            const Status st = store.save(FileType::EActivity, "SAFE", bytes("new data"));
            fs.powerOn();
            CHECK(files::recover(fs, "PICOCALC/EACT") == Status::Ok);
            std::vector<std::uint8_t> back;
            CHECK(store.open(FileType::EActivity, "SAFE", back) == Status::Ok);
            CHECK(back == bytes("old") || back == bytes("new data"));
            if (st == Status::Ok) CHECK(back == bytes("new data"));
            CHECK(store.save(FileType::EActivity, "SAFE", bytes("old")) == Status::Ok);
        }
        // interruzione di alimentazione dopo n byte
        int cuts = 0;
        for (std::size_t budget = 1; budget < 200; budget += 7) {
            fs.failAfterBytes(budget);
            const Status st = store.save(FileType::EActivity, "SAFE", bytes(std::string(60, 'n')));
            cuts += st != Status::Ok;
            fs.powerOn();
            CHECK(files::recover(fs, "PICOCALC/EACT") == Status::Ok);
            std::vector<std::uint8_t> back;
            CHECK(store.open(FileType::EActivity, "SAFE", back) == Status::Ok);
            CHECK(back == bytes("old") || back == bytes(std::string(60, 'n')));
            CHECK(store.save(FileType::EActivity, "SAFE", bytes("old")) == Status::Ok);
        }
        CHECK(cuts > 3);
        // scheda piena
        files::RamFileSystem small(64);
        files::TypedStore tiny(small);
        CHECK(tiny.save(FileType::EActivity, "BIG", bytes(std::string(200, 'b'))) == Status::Full);
        // tipo non disponibile sulla scheda
        if (!board::config::kIsPicoPlus2W) CHECK(store.save(FileType::Program, "HELLO", bytes("x")) == Status::NotSupported);
        else {
            CHECK(store.save(FileType::Program, "HELLO.c", bytes("int main(){}")) == Status::Ok);
            CHECK(store.save(FileType::Program, "HELLO.cs", bytes("class P{}")) == Status::Ok);
            CHECK(store.save(FileType::Program, "BAD", bytes(std::string("\xFF", 1))) == Status::Corrupt);
            CHECK(store.list(FileType::Program, list) == Status::Ok && list.size() == 2 && list[0].file == "HELLO.c");
            CHECK(store.rename(FileType::Program, "HELLO.cs", "WORLD") == Status::Ok && store.exists(FileType::Program, "WORLD.cs"));
        }
    }
    // ---------------------------------------------------------------- F3.4 microSD simulata (FatFs) e LittleFS
    {
        sd::RamDisk disk(32768);
        files::FatFsFileSystem fs(disk);
        CHECK(fs.format() == Status::Ok);
        exercise(fs, "FatFs");
    }
    {
        storage::RamFlash flash(256 * 1024);
        files::LittleFsFileSystem fs(flash);
        CHECK(fs.status() == Status::Ok);
        exercise(fs, "LittleFS");
    }
    {
        // interruzione di alimentazione su LittleFS durante il salvataggio di un documento
        const std::vector<std::uint8_t> oldDoc(3000, 'o'), newDoc(5000, 'n');
        int cuts = 0, oldSeen = 0, newSeen = 0;
        for (std::size_t budget = 512; budget < 40000; budget += 2048) {
            storage::RamFlash flash(128 * 1024);
            {
                files::LittleFsFileSystem fs(flash);
                files::TypedStore store(fs);
                CHECK(store.save(FileType::EActivity, "DOC", oldDoc) == Status::Ok);
                flash.failAfter(budget);
                cuts += store.save(FileType::EActivity, "DOC", newDoc) != Status::Ok;
            }
            flash.powerOn();
            files::LittleFsFileSystem fs(flash);
            files::TypedStore store(fs);
            std::vector<files::TypedEntry> list;
            CHECK(store.list(FileType::EActivity, list) == Status::Ok && list.size() == 1);   // recover: nessun .tmp/.bak
            std::vector<std::uint8_t> back;
            CHECK(store.open(FileType::EActivity, "DOC", back) == Status::Ok);
            CHECK(back == oldDoc || back == newDoc);
            oldSeen += back == oldDoc; newSeen += back == newDoc;
        }
        CHECK(cuts > 5 && oldSeen > 0 && newSeen > 0);
    }
    // ---------------------------------------------------------------- ricerca (manuale 10-5)
    {
        std::vector<files::TypedEntry> e = {{"ABITC", "ABITC.pceact", 1}, {"ITABC", "ITABC.pceact", 1}, {"XXIT", "XXIT.pceact", 1}};
        CHECK(files::searchPrefix(e, "IT") == 1 && files::searchPrefix(e, "it") == 1);
        CHECK(files::searchPrefix(e, "XIT") == -1 && files::searchPrefix(e, "") == -1 && files::searchPrefix(e, "AB") == 0);
    }
    // ---------------------------------------------------------------- F3.5 FileDialog unico
    {
        app::FileDialog d;
        // valori predefiniti: la finestra dei CSV della v1.11.0
        d.openName("CSV SAVE", "");
        for (char c : std::string("ab_9-x")) d.key(ch(c));
        CHECK(d.key(k(Key::Exe)) == 1 && d.name() == "ab_9-x");
        // regola dei nomi di eActivity: maiuscole, { } ' ~, 8 caratteri
        d.setNameRule([](char c) { return files::typedNameChar(FileType::EActivity, c); },
                      [](char c) { return files::typedNameCase(FileType::EActivity, c); }, 8);
        d.setExisting({"FISICA"});
        d.openName("SAVE\xC2\xB7" "AS", "");
        for (char c : std::string("fi_sica~longer")) d.key(ch(c));
        CHECK(d.key(k(Key::Exe)) == 1 && d.name() == "FISICA~L" && !d.overwriting());
        d.openName("SAVE\xC2\xB7" "AS", "");
        for (char c : std::string("fisica")) d.key(ch(c));
        CHECK(d.key(k(Key::Exe)) == 0 && d.active());                                  // "Sovrascrivere FISICA?"
        CHECK(d.key(k(Key::F6)) == 0 && d.active() && d.mode() == app::FileDialog::Mode::SaveAs);   // No: torna al nome
        CHECK(d.key(k(Key::Exe)) == 0);
        CHECK(d.key(k(Key::F1)) == 1 && d.name() == "FISICA" && d.overwriting());
        // ricerca
        d.openSearch("SEARCH");
        d.key(ch('i')); d.key(ch('t'));
        CHECK(d.key(k(Key::Exe)) == 1 && d.name() == "IT");
        // conferma
        d.openConfirm("DELETE", "Delete eActivity?");
        CHECK(d.key(ch('x')) == 0 && d.active());
        CHECK(d.key(k(Key::Exit)) == -1 && !d.active());
        d.openConfirm("DELETE", "Delete eActivity?");
        CHECK(d.key(k(Key::Exe)) == 1);
        // elenco con estensione e testo vuoto propri
        d.setExtension(".pceact");
        d.openList("OPEN", {"A", "B"});
        CHECK(d.key(k(Key::Down)) == 0 && d.key(k(Key::Exe)) == 1 && d.name() == "B");
    }
    // ---------------------------------------------------------------- F3.6 MEMORY: file per tipo
    {
        board::init();
        testrig::Rig r;
        files::FileSystem* fs = r.platform.files();
        CHECK(fs != nullptr);
        files::TypedStore store(*fs);
        CHECK(store.save(FileType::EActivity, "LEZ1", bytes("uno")) == Status::Ok);
        CHECK(store.save(FileType::EActivity, "LEZ2", bytes("due")) == Status::Ok);
        r.fw.apps().open(app::AppId::Memory);
        auto* m = r.active<app::MemoryApp>();
        CHECK(m != nullptr && m->page() == app::MemoryApp::Page::Data);
        r.key(pckey::F3);
        CHECK(m->page() == app::MemoryApp::Page::Types && !r.error());
        r.key(pckey::Enter);
        CHECK(m->page() == app::MemoryApp::Page::Files && m->entries().size() == 2 && m->entries()[0].file == "LEZ1.pceact");
        r.key(pckey::Down);
        r.key(pckey::F1);                                                              // DELETE LEZ2: conferma
        CHECK(store.exists(FileType::EActivity, "LEZ2"));
        r.key(pckey::Enter);
        CHECK(!store.exists(FileType::EActivity, "LEZ2") && m->entries().size() == 1);
        r.key(pckey::Esc);
        CHECK(m->page() == app::MemoryApp::Page::Types);
        r.key(pckey::Esc);
        CHECK(m->page() == app::MemoryApp::Page::Data);
    }
    return testutil::finish("typed_files_test");
}
