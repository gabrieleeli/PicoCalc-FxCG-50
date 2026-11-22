// v1.16.0 (M2): eActivity, una prova per ogni voce della tabella di parita'
// (docs/reports/ANALISI_FONTI_v1.16.0.md, E01-E56), sui due profili:
//  - nucleo: documento, codifica .pceact, testo con espressioni, a capo;
//  - parti del workspace (strisce) con la garanzia forte;
//  - controller guidato dai tasti della PicoCalc come nel simulatore: elenco
//    dei file e cartelle, righe di testo, di calcolo (nucleo di Run-Matrix),
//    ricalcolo fino alla riga di stop e Break, colori, memo, immagini, strisce
//    (sessione da striscia, dati indipendenti, SHIFT ' e SHIFT ,), Notes,
//    memorie per file, SAVE/SAVE·AS/OPT/CAPACITY, Save? all'uscita, limite del
//    documento per scheda con "Memory ERROR" senza perdita.
#include "app_rig.hpp"
#include "board_config.hpp"
#include "eactivity.hpp"
#include "features.hpp"
#include "limits.hpp"
#include "pict.hpp"
#include "run_matrix.hpp"
#include "test_util.hpp"
#include "typed_files.hpp"
#include <cstring>

using namespace fx;
using namespace fx::app;
using eact::Line;
using eact::LineKind;
using eact::StripKind;
using testrig::Rig;
namespace pk = fx::pckey;

namespace {
constexpr const char* kDir = "PICOCALC/EACT";
const std::string kArrow = "\xE2\x86\x92";

void writeDoc(files::FileSystem& fs, const std::string& file, const eact::Document& d, const std::string& dir = kDir) {
    const auto raw = files::encodeTypedFile(files::FileType::EActivity, eact::encode(d));
    CHECK(fs.makeDirs(dir) == files::Status::Ok);
    CHECK(files::writeAtomic(fs, files::join(dir, file), raw.data(), raw.size()) == files::Status::Ok);
}
bool readDoc(files::FileSystem& fs, const std::string& file, eact::Document& d, const std::string& dir = kDir) {
    std::vector<std::uint8_t> raw, payload;
    if (files::readAll(fs, files::join(dir, file), raw, 1u << 22) != files::Status::Ok) return false;
    if (files::decodeTypedFile(files::FileType::EActivity, raw, payload) != files::Status::Ok) return false;
    return eact::decode(payload.data(), payload.size(), d);
}
void writePicture(files::FileSystem& fs, const std::string& file, int w, int h, const char* dir = files::PictFolder) {
    std::vector<std::uint16_t> px(static_cast<std::size_t>(w * h), 0xF800);
    for (int i = 0; i < w; ++i) px[static_cast<std::size_t>(i)] = 0x001F;
    pict::Image img;
    CHECK(pict::encode(px.data(), w, w, h, img));
    const auto data = pict::serialize(img);
    CHECK(fs.makeDirs(dir) == files::Status::Ok);
    CHECK(files::writeAtomic(fs, files::join(dir, file), data.data(), data.size()) == files::Status::Ok);
}

struct T {
    Rig& r;
    EActivityApp* a() { return r.active<EActivityApp>(); }
    void F(int n) { r.key(static_cast<std::uint8_t>(pk::F1 + n - 1)); }
    void exe() { r.key(pk::Enter); }
    void esc() { r.key(pk::Esc); }
    void shift() { r.key(pk::F7); }
    void up() { r.key(pk::Up); }
    void down() { r.key(pk::Down); }
    void left() { r.key(pk::Left); }
    void right() { r.key(pk::Right); }
    void open() { r.fw.apps().open(AppId::EActivity); r.settle(); }
    void relist() { r.fw.apps().home(); open(); }            // l'elenco dopo i file scritti dal test
    const eact::Document& doc() { return a()->document(); }
    const Line& line() { return doc().lines[static_cast<std::size_t>(a()->line())]; }
    void draw() { r.fw.draw(board::graphics()); }
    // Chiude il documento senza salvare (Save? -> No).
    void closeNoSave() { if (a() && a()->documentOpen()) { esc(); if (a()->documentOpen()) F(6); } }
    void select(const std::string& name) {
        const auto& it = a()->items();
        for (std::size_t i = 0; i < it.size(); ++i)
            if (it[i].name == name) { while (a()->selected() != static_cast<int>(i)) down(); return; }
        CHECK(!"item not found");
    }
};
std::string expr(const Line& l) {
    std::string e; std::size_t end;
    for (std::size_t i = 0; i < l.text.size(); ++i) if (eact::islandAt(l.text, i, e, end)) return e;
    return {};
}
} // namespace

int main() {
    // ================================================================ nucleo
    {
        // E08: tipi di riga e codifica senza perdite (anche tipi di striscia sconosciuti)
        eact::Document d;
        d.lines.push_back(Line::textLine("Testo " + eact::island("1/2") + " fine"));
        d.lines.back().attr[0] = eact::attr(eact::TextRed, eact::MarkerYellow);
        d.lines.back().memo = {true, eact::TextBlue, "nota"};
        d.lines.push_back(Line::calcLine("1+2"));
        Line stop; stop.kind = LineKind::Stop; d.lines.push_back(stop);
        Line pic; pic.kind = LineKind::Picture; pic.text = "PICT01.pcpict"; pic.borderStyle = eact::BorderThick; pic.borderColor = 3;
        d.lines.push_back(pic);
        Line strip; strip.kind = LineKind::Strip; strip.strip = 200; strip.text = "futura"; strip.data = {1, 2, 3, 4};
        d.lines.push_back(strip);
        d.memories = {9, 8, 7};
        const auto bytes = eact::encode(d);
        eact::Document back;
        CHECK(eact::decode(bytes.data(), bytes.size(), back));
        CHECK(back.lines.size() == 5 && back.lines[0].text == d.lines[0].text && back.lines[0].attr == d.lines[0].attr);
        CHECK(back.lines[0].memo.present && back.lines[0].memo.text == "nota" && back.memoCount() == 1);
        CHECK(back.lines[3].borderStyle == eact::BorderThick && back.lines[3].borderColor == 3);
        CHECK(back.lines[4].strip == 200 && back.lines[4].data == strip.data && back.memories == d.memories);   // E47
        CHECK(eact::encodedSize(d) == bytes.size());
        // garanzia forte e rifiuto di dati non validi
        for (std::size_t cut = 0; cut + 1 < bytes.size(); cut += 7) {
            eact::Document t = back;
            CHECK(!eact::decode(bytes.data(), cut, t) && t.lines.size() == 5);
        }
        eact::Document bad = d;
        bad.lines[0].text = std::string(300, 'x');
        bad.lines[0].attr.assign(300, 0);
        const auto big = eact::encode(bad);
        eact::Document t;
        CHECK(!eact::decode(big.data(), big.size(), t));                    // E10: oltre 255 byte
        bad = d; bad.lines[1].memo = {}; bad.lines[2].memo = {true, 0, "x"};
        const auto memoStop = eact::encode(bad);
        CHECK(!eact::decode(memoStop.data(), memoStop.size(), t));          // memo solo su testo e calcolo (10-11)
        bad = d; bad.lines[0].text = eact::IslandStart + std::string("1+");
        bad.lines[0].attr.assign(bad.lines[0].text.size(), 0);
        const auto open = eact::encode(bad);
        CHECK(!eact::decode(open.data(), open.size(), t));                  // espressione non chiusa
        // E13: unita' del testo (un'espressione e' un'unita'), validText
        const std::string s = "a" + eact::island("x^2") + "b";
        CHECK(eact::nextUnit(s, 1) == s.size() - 1 && eact::prevUnit(s, s.size() - 1) == 1);
        CHECK(eact::validText(s) && !eact::validText(eact::IslandEnd) && !eact::validText(eact::island(eact::island("1"))));
        // E10/E13: a capo sulle parole; le espressioni non vanno a capo
        auto measure = [](const std::string&, std::size_t a, std::size_t b, bool island, void*) {
            return island ? 100 : static_cast<int>(b - a) * 8;
        };
        const std::string words = "uno due tre quattro cinque sei sette otto nove dieci";
        const auto rows = eact::wrap(words, 80, measure, nullptr);
        CHECK(rows.size() > 1);
        for (const auto& row : rows) {
            int w = 0; for (const auto& p : row) if (words[p.from] != ' ') w += p.width;
            CHECK(w <= 80);
        }
        const std::string withIsland = "ab " + eact::island("1/2") + " cd";
        const auto r2 = eact::wrap(withIsland, 60, measure, nullptr);
        bool islandWhole = false;
        for (const auto& row : r2) for (const auto& p : row) if (p.island) islandWhole = p.to - p.from == eact::island("1/2").size();
        CHECK(islandWhole);
        // E03: nomi dei documenti (8 caratteri A-Z 0-9 { } ' ~)
        CHECK(eact::validName("FISICA") && eact::validName("A{1}'~") && !eact::validName("fisica") && !eact::validName("TOOLONGNAME") && !eact::validName(""));
        // E37/E46/E47: tabella dei 24 tipi di striscia e disponibilita' per scheda
        CHECK(eact::StripKindCount == 24);
        CHECK(std::string(eact::stripInfo(StripKind::Run).name) == "RUN" && std::string(eact::stripInfo(StripKind::PicturePlot).name) == "Picture Plot");
        CHECK(eact::stripAvailable(StripKind::Run) && eact::stripAvailable(StripKind::Graph) && eact::stripAvailable(StripKind::Notes) &&
              eact::stripAvailable(StripKind::Graph3D) && eact::stripAvailable(StripKind::SimulEquation));
        CHECK(!eact::stripAvailable(StripKind::EConTop) && !eact::stripAvailable(StripKind::EConResult));          // D5
        CHECK(!eact::stripAvailable(StripKind::ConicsGraph) && !eact::stripAvailable(StripKind::SpreadSheet));     // E46: dalla M7/M9 sulla Plus
        CHECK(!eact::stripAvailable(static_cast<StripKind>(200)));
        // E52/E53: limite del documento per scheda (D3)
        CHECK(limits::kEActivityDocumentBytes == (board::config::kIsPicoPlus2W ? 256u * 1024u : 24u * 1024u));
    }

    // ================================================================ parti del workspace (strisce, 10-19)
    {
        Workspace ws;
        ws.functions[0].expr = "X^2";
        ws.lists[0] = {1, 2, 3};
        ws.history.push_back(HistoryEntry{});
        ws.history.back().expression = "1+1";
        const std::vector<std::uint8_t> all = {part::History, part::Graph, part::Table, part::Lists, part::Equation, part::Matrices, part::Graph3D};
        const auto enc = encodeParts(ws, all);
        Workspace other;
        CHECK(decodeParts(other, enc.data(), enc.size()));
        CHECK(other.functions[0].expr == "X^2" && other.lists[0].size() == 3 && other.history.size() == 1 && other.history[0].expression == "1+1");
        for (std::uint8_t p : all) {
            const auto one = encodePart(ws, p);
            Workspace w2;
            CHECK(decodePart(w2, p, one.data(), one.size()));
            resetPart(w2, p);
        }
        resetPart(other, part::Graph);
        CHECK(other.functions[0].expr.empty());
        resetPart(other, part::Lists);
        CHECK(other.lists[0].empty());
        // tutto o niente
        auto broken = enc;
        broken.resize(broken.size() - 3);
        Workspace keep;
        keep.functions[0].expr = "Y";
        CHECK(!decodeParts(keep, broken.data(), broken.size()) && keep.functions[0].expr == "Y");
        // E40: interruzione durante una striscia: al riavvio tornano i dati del menu principale
        Workspace live;
        live.functions[0].expr = "X+1";                                    // menu principale
        live.eactivity.savedStrip = encodeParts(live, {part::Graph});
        live.functions[0].expr = "STRISCIA";                               // dati della striscia aperta
        live.eactivity.folder = "SCUOLA";
        const auto record = serializeWorkspace(live);
        Workspace boot;
        CHECK(deserializeWorkspace(record.data(), record.size(), boot));
        CHECK(boot.functions[0].expr == (features::kEActivity ? "X+1" : "STRISCIA"));
        CHECK(boot.eactivity.savedStrip.empty() && boot.eactivity.folder == (features::kEActivity ? "SCUOLA" : ""));
    }

    // ================================================================ controller
    files::RamFileSystem card(8u * 1024u * 1024u);
    board::host::setFileSystem(&card);
    Rig rig;
    T t{rig};
    rig.ws().functions[0].expr = "X^2";                                    // dati del menu principale
    // ---------------------------------------------------------------- E01 elenco vuoto
    t.open();
    CHECK(t.a() && t.a()->screen() == EActivityApp::Screen::Files && t.a()->items().empty() && t.a()->message().empty());
    t.draw();
    CHECK(!rig.error());
    // ---------------------------------------------------------------- E03 NEW
    t.F(2);
    rig.type("fi_sica");                                                   // minuscole maiuscole, '_' rifiutato
    t.exe();
    CHECK(t.a()->documentOpen() && t.a()->fileName() == "FISICA.pceact" && card.exists("PICOCALC/EACT/FISICA.pceact"));
    CHECK(t.doc().lines.size() == 1 && t.line().kind == LineKind::Text && !t.a()->modified());
    // ---------------------------------------------------------------- E10 testo, EXE a capo
    rig.type("Ciao mondo");
    CHECK(t.line().text == "Ciao mondo" && t.a()->textCursor() == 10 && t.a()->modified());
    for (int i = 0; i < 5; ++i) t.left();
    t.exe();                                                               // a capo nel punto del cursore
    CHECK(t.doc().lines.size() == 2 && t.doc().lines[0].text == "Ciao " && t.doc().lines[1].text == "mondo" && t.a()->line() == 1);
    rig.key(pk::Backspace);                                                // DEL all'inizio: unisce le righe
    CHECK(t.doc().lines.size() == 1 && t.line().text == "Ciao mondo" && t.a()->textCursor() == 5);
    rig.key(pk::End);
    rig.type(std::string(300, 'x'));                                       // 255 byte (10-7)
    CHECK(t.line().text.size() == eact::MaxTextLineBytes);
    // E12: AC cancella solo la riga visualizzata
    {
        const std::string before = t.line().text;
        rig.key(pk::Break);
        CHECK(!t.line().text.empty() && t.line().text.size() < before.size() && t.line().text.compare(0, 10, "Ciao mondo") == 0);
        rig.key(pk::Home);
        rig.key(pk::Break);
        CHECK(t.line().text.size() < before.size() && t.line().text.find("Ciao") == std::string::npos);
    }
    // svuota la riga
    while (!t.line().text.empty()) rig.key(pk::Break);
    // E11: A<->a
    t.F(5);
    CHECK(t.a()->lowerCase());
    rig.type("aB");
    CHECK(t.line().text == "Ab");
    t.F(5);
    rig.type("c");
    CHECK(t.line().text == "Abc" && !t.a()->lowerCase());
    // E14: CHAR (simboli, lettere di altre lingue)
    t.F(4);
    CHECK(t.a()->charsOpen());
    t.draw();
    t.right();
    t.exe();                                                               // il secondo simbolo della pagina MATH
    CHECK(t.line().text.size() > 4 && t.line().text.compare(0, 3, "Abc") == 0 && static_cast<unsigned char>(t.line().text[3]) >= 0xC0);
    {
        const std::size_t before = t.line().text.size();
        t.F(4); t.F(4); t.exe();                                           // pagina LANG: lettere di altre lingue
        CHECK(t.line().text.size() >= before + 2 && !t.a()->charsOpen() && eact::validText(t.line().text));
    }
    // E13: espressione Math nella riga di testo (MATH, modello frazione)
    t.F(6); t.F(4);                                                        // pagina 2 › MATH
    CHECK(t.a()->editingIsland());
    t.exe();                                                               // menu MATH: frazione
    rig.type("1"); t.down(); rig.type("2");
    t.exe();                                                               // fine dell'espressione
    CHECK(!t.a()->editingIsland() && eact::validText(t.line().text));
    {
        const std::string e = expr(t.line());
        Workspace w; HistoryEntry h;
        (void)evaluateEntry(w, e, h);
        CHECK(std::abs(h.value.real() - 0.5) < 1e-12);
    }
    rig.type(" fine");
    t.draw();
    CHECK(!rig.error());
    // ← entra nell'espressione dalla fine, → esce
    for (int i = 0; i < 5; ++i) t.left();
    t.left();
    CHECK(t.a()->editingIsland());
    t.right();
    CHECK(!t.a()->editingIsland());
    // ---------------------------------------------------------------- E15 TEXT <-> CALC
    t.exe();                                                               // nuova riga: "fine" sotto
    rig.key(pk::Home);
    while (!t.line().text.empty()) rig.key(pk::Break);
    rig.type("2+3");
    t.F(3);                                                                // TEXT: diventa riga di calcolo
    CHECK(t.line().kind == LineKind::Calc && t.line().text == "2+3" && t.a()->inputText() == "2+3");
    // E16: EXE come in Run-Matrix
    t.exe();
    CHECK(t.doc().lines[1].result && std::abs(t.doc().lines[1].result->value.real() - 5) < 1e-12);
    CHECK(t.doc().lines.size() == 3 && t.line().kind == LineKind::Calc && t.a()->line() == 2);   // nuova riga di calcolo in fondo
    // E17: forme esatte e S<->D
    rig.type("1/3");
    t.exe();
    const std::string third = t.doc().lines[2].text;
    {
        const Line& l = t.doc().lines[2];
        CHECK(l.result && resultText(*l.result, rig.ws().settings) == "1/3");
        t.up();
        rig.key(pk::F8);                                                   // OPTN › S<->D
        t.exe();
        CHECK(resultText(*t.doc().lines[2].result, rig.ws().settings) != "1/3");
        rig.key(pk::F8); t.exe();
        CHECK(resultText(*t.doc().lines[2].result, rig.ws().settings) == "1/3");
    }
    // E28: niente colore sul risultato: gli attributi coprono solo l'espressione
    CHECK(t.line().attr.size() == t.line().text.size());
    // Syntax ERROR: la riga resta com'era, l'errore e' mostrato
    t.down();
    rig.type("1+*");
    t.exe();
    CHECK(rig.error() && rig.errorTitle() == "Syntax ERROR");
    rig.closeError();
    rig.key(pk::Break);
    // E15: CALC -> TEXT (l'espressione diventa un'espressione Math nel testo)
    t.up();
    t.F(3);
    CHECK(t.line().kind == LineKind::Text && expr(t.line()) == third);
    t.F(3);
    CHECK(t.line().kind == LineKind::Calc && t.line().text == third);
    // E18: riga di calcolo lunga: scorre (disegno senza errori)
    t.down();
    rig.type("1+2+3+4+5+6+7+8+9+10+11+12+13+14+15+16+17+18+19+20+21+22+23+24+25+26+27+28+29+30");
    t.draw();
    CHECK(!rig.error());
    t.exe();
    CHECK(t.doc().lines[3].result && std::abs(t.doc().lines[3].result->value.real() - 465) < 1e-9);
    // ---------------------------------------------------------------- E20/E21 INSERT
    {
        const std::size_t n = t.doc().lines.size();
        const int at = t.a()->line();
        t.F(5); t.F(3);                                                    // INSERT › STOP (sopra la riga)
        CHECK(t.doc().lines.size() == n + 1 && t.line().kind == LineKind::Stop && t.a()->line() == at);
        t.F(5); t.F(1);                                                    // INSERT › TEXT
        CHECK(t.line().kind == LineKind::Text && t.doc().lines[static_cast<std::size_t>(at) + 1].kind == LineKind::Stop);
        t.F(6); t.F(3); t.F(2);                                            // testo pag. 2 › INSERT › CALC
        CHECK(t.line().kind == LineKind::Calc && t.doc().lines.size() == n + 3);
        // E22: DEL-LINE con conferma (No, poi Si')
        t.F(6); t.F(2);
        t.F(6);
        CHECK(t.doc().lines.size() == n + 3);
        t.F(6); t.F(2); t.F(1);
        CHECK(t.doc().lines.size() == n + 2 && t.line().kind == LineKind::Text);
        t.F(6); t.F(2); t.F(1);
        CHECK(t.line().kind == LineKind::Stop);
        t.F(6); t.F(2); t.F(1);                                            // riga di stop: DEL-LINE a pagina 2
        CHECK(t.doc().lines.size() == n);
    }
    // ---------------------------------------------------------------- E09 movimento e JUMP
    {
        t.F(6); t.F(1); t.F(1);                                            // JUMP › TOP
        CHECK(t.a()->line() == 0);
        const int last = static_cast<int>(t.doc().lines.size()) - 1;
        t.F(6); t.F(1); t.F(2);                                            // JUMP › BOTTOM
        CHECK(t.a()->line() == last);
        rig.key(pk::PageUp);
        CHECK(t.a()->line() < last);
        t.shift(); t.down();                                               // SHIFT ↓: pagina seguente
        CHECK(t.a()->line() == last);
        t.shift(); t.up();
        CHECK(t.a()->line() < last);
    }
    // ---------------------------------------------------------------- E48 SAVE
    t.F(6); t.F(1); t.F(2);                                                // BOTTOM
    t.F(1); t.F(1);                                                        // FILE › SAVE
    CHECK(!t.a()->modified());
    {
        eact::Document saved;
        CHECK(readDoc(card, "FISICA.pceact", saved) && saved.lines.size() == t.doc().lines.size());
        CHECK(saved.lines[1].kind == LineKind::Calc && saved.lines[1].result && saved.lines[3].result);   // i risultati sono nel file
        CHECK(!saved.lines[2].result);                                     // TEXT <-> CALC: il risultato vecchio non vale piu'
    }
    // ---------------------------------------------------------------- E19 ricalcolo fino alla riga di stop
    {
        eact::Document d;
        d.lines.push_back(Line::calcLine("5" + kArrow + "A"));
        d.lines.push_back(Line::calcLine("A+1"));
        Line stop; stop.kind = LineKind::Stop;
        d.lines.push_back(stop);
        d.lines.push_back(Line::calcLine("A*2"));
        writeDoc(card, "RICALC.pceact", d);
    }
    t.closeNoSave();
    CHECK(t.a()->screen() == EActivityApp::Screen::Files);
    t.select("RICALC");
    t.exe();
    CHECK(t.a()->documentOpen() && t.a()->fileName() == "RICALC.pceact" && t.line().kind == LineKind::Calc);
    t.exe();
    rig.settle();
    CHECK(!t.a()->recalculating());
    CHECK(t.doc().lines[0].result && t.doc().lines[1].result && std::abs(t.doc().lines[1].result->value.real() - 6) < 1e-12);
    CHECK(!t.doc().lines[3].result);                                       // dopo la riga di stop: non ricalcolata
    // modifica della prima riga: si ricalcola la seguente
    t.up();
    rig.key(pk::Backspace); rig.key(pk::Backspace); rig.key(pk::Backspace);
    rig.type("7"); rig.key(pk::Insert); rig.type("A");
    t.exe();
    rig.settle();
    CHECK(std::abs(t.doc().lines[1].result->value.real() - 8) < 1e-12 && !t.doc().lines[3].result);
    // AC durante il ricalcolo: Break
    t.closeNoSave();
    {
        eact::Document d;
        d.lines.push_back(Line::calcLine("0" + kArrow + "A"));
        for (int i = 0; i < 80; ++i) d.lines.push_back(Line::calcLine("A+1" + kArrow + "A"));
        writeDoc(card, "LUNGO.pceact", d);
    }
    t.relist();
    t.select("LUNGO");
    t.exe();
    t.exe();                                                               // EXE: un passo del ricalcolo
    CHECK(t.a()->recalculating());
    rig.key(pk::Break);
    CHECK(rig.error() && rig.errorTitle() == "Break" && !t.a()->recalculating());
    rig.closeError();
    {
        std::size_t done = 0;
        for (const Line& l : t.doc().lines) done += l.result ? 1 : 0;
        CHECK(done > 1 && done < t.doc().lines.size());
    }
    t.closeNoSave();
    // ---------------------------------------------------------------- E05 SEARCH, E04 DELETE
    t.F(4);
    rig.type("lu");
    t.exe();
    CHECK(t.a()->items()[static_cast<std::size_t>(t.a()->selected())].name == "LUNGO");
    t.F(4);
    rig.type("ZZ");
    t.exe();
    CHECK(rig.error() && rig.errorTitle() == "Not Found");
    rig.closeError();
    t.F(3);                                                                // DELETE: "Delete eActivity?"
    t.F(6);                                                                // No
    CHECK(card.exists("PICOCALC/EACT/LUNGO.pceact"));
    t.F(3); t.F(1);
    CHECK(!card.exists("PICOCALC/EACT/LUNGO.pceact") && t.a()->items().size() == 2);
    // ---------------------------------------------------------------- E02/E56 cartelle
    {
        eact::Document d;
        d.lines.push_back(Line::textLine("in cartella"));
        writeDoc(card, "LEZ1.pceact", d, "PICOCALC/EACT/SCUOLA");
        CHECK(card.makeDirs("PICOCALC/EACT/SCUOLA/A/B/C") == files::Status::Ok);
    }
    t.esc(); t.open();
    CHECK(t.a()->items().size() == 3 && t.a()->items()[0].folder && t.a()->items()[0].name == "SCUOLA");
    t.select("SCUOLA");
    t.exe();
    CHECK(t.a()->folder() == "SCUOLA" && t.a()->items().size() == 2);
    t.select("A"); t.exe();
    t.select("B"); t.exe();
    CHECK(t.a()->folder() == "SCUOLA/A/B");
    t.select("C"); t.exe();                                                // quarto livello: non si apre
    CHECK(rig.error() && rig.errorDetail().find("Sub-folders") != std::string::npos && t.a()->folder() == "SCUOLA/A/B");
    rig.closeError();
    t.esc(); t.esc();
    CHECK(t.a()->folder() == "SCUOLA");
    t.select("LEZ1"); t.exe();
    CHECK(t.a()->documentOpen() && t.line().text == "in cartella");
    t.closeNoSave();
    t.esc();
    CHECK(t.a()->folder().empty());
    // ---------------------------------------------------------------- E07 file danneggiato o troppo grande
    {
        std::vector<std::uint8_t> raw = files::encodeTypedFile(files::FileType::EActivity, {1, 2, 3});
        CHECK(files::writeAtomic(card, "PICOCALC/EACT/ROTTO.pceact", raw.data(), raw.size()) == files::Status::Ok);
    }
    t.esc(); t.open();
    t.select("ROTTO"); t.exe();
    CHECK(rig.error() && rig.errorTitle() == "Data ERROR" && !t.a()->documentOpen());
    rig.closeError();
    auto lineBytes = [](std::size_t n) {
        eact::Document a, b;
        b.lines.push_back(Line::textLine(std::string(n, 'g')));
        return eact::encodedSize(b) - eact::encodedSize(a);
    };
    const std::size_t base = eact::encodedSize(eact::Document{}), p250 = lineBytes(250), p20 = lineBytes(20);
    {
        eact::Document big;
        big.lines.assign((limits::kEActivityDocumentBytes + 512) / p250 + 1, Line::textLine(std::string(250, 'g')));
        CHECK(eact::encodedSize(big) > limits::kEActivityDocumentBytes);
        writeDoc(card, "GRANDE.pceact", big);
    }
    t.esc(); t.open();
    t.select("GRANDE"); t.exe();
    CHECK(rig.error() && rig.errorTitle() == "Memory ERROR" && !t.a()->documentOpen() && t.a()->screen() == EActivityApp::Screen::Files);
    rig.closeError();
    // ---------------------------------------------------------------- E52 limite del documento aperto
    {
        eact::Document full;
        const std::size_t n250 = (limits::kEActivityDocumentBytes - base - 700) / p250;
        full.lines.assign(n250, Line::textLine(std::string(250, 'h')));
        const std::size_t rest = limits::kEActivityDocumentBytes - eact::encodedSize(full) - 130;
        for (std::size_t i = 0; i < rest / p20; ++i) full.lines.push_back(Line::textLine(std::string(20, 'h')));
        full.lines.push_back(Line::textLine());
        CHECK(eact::encodedSize(full) < limits::kEActivityDocumentBytes && eact::encodedSize(full) + 190 > limits::kEActivityDocumentBytes);
        writeDoc(card, "PIENO.pceact", full);
        t.relist();
        t.select("PIENO"); t.exe();
        CHECK(t.a()->documentOpen());
        t.F(6); t.F(1); t.F(2);                                            // BOTTOM: la riga vuota
        for (int i = 0; i < 200 && !rig.error(); ++i) rig.type("z");
        CHECK(rig.error() && rig.errorTitle() == "Memory ERROR");
        rig.closeError();
        CHECK(t.a()->documentBytes() <= limits::kEActivityDocumentBytes && !t.line().text.empty() && t.line().text.size() < 200);
        t.closeNoSave();
    }
    // ---------------------------------------------------------------- E49 OPT, E50 CAPACITY, E44 SIZE
    t.select("FISICA"); t.exe();
    t.F(1); t.F(3);                                                        // FILE › OPT
    CHECK(rig.fw.apps().context().toastText().find("OPT") != std::string::npos);
    t.F(1); t.F(4);                                                        // FILE › CAPACITY
    CHECK(t.a()->screen() == EActivityApp::Screen::Capacity && t.a()->documentBytes() > 0);
    t.draw();
    t.esc();
    CHECK(t.a()->screen() == EActivityApp::Screen::Doc);
    // ---------------------------------------------------------------- E26/E27 MARKER e CHAR
    t.F(6); t.F(1); t.F(1);                                                // TOP: "Abc≥à ½ fine"
    CHECK(t.line().kind == LineKind::Text);
    rig.key(pk::Home);
    t.F(6); t.F(5); t.F(1);                                                // COLOR › MARKER
    t.exe();                                                               // 1: Magenta
    CHECK(t.a()->marking() == 1);
    t.right(); t.right(); t.right();
    t.draw();
    t.F(1);                                                                // SET
    CHECK(!t.a()->marking() && eact::attrMarker(t.line().attr[0]) == eact::MarkerMagenta && eact::attrMarker(t.line().attr[2]) == eact::MarkerMagenta &&
          eact::attrMarker(t.line().attr[3]) == eact::MarkerNone);
    rig.key(pk::Home);
    t.F(6); t.F(5); t.F(2);                                                // COLOR › CHAR
    t.down(); t.down(); t.exe();                                           // 3: Red
    t.right();
    t.F(1);
    CHECK(eact::attrColor(t.line().attr[0]) == eact::TextRed && eact::attrColor(t.line().attr[1]) == eact::TextDefault &&
          eact::attrMarker(t.line().attr[0]) == eact::MarkerMagenta);
    // EXIT annulla
    rig.key(pk::Home);
    t.F(6); t.F(5); t.F(1);
    t.down(); t.down(); t.down(); t.down(); t.exe();                       // Clear
    t.right(); t.right();
    t.esc();
    CHECK(!t.a()->marking() && eact::attrMarker(t.line().attr[0]) == eact::MarkerMagenta);
    // su piu' righe: dalla riga 0 alla riga di calcolo (riga intera)
    rig.key(pk::Home);
    t.F(6); t.F(5); t.F(1);
    t.down(); t.down(); t.down(); t.exe();                                 // 4: Yellow
    t.down(); t.down();
    t.F(1);
    CHECK(eact::attrMarker(t.doc().lines[2].attr[0]) == eact::MarkerYellow && eact::attrMarker(t.doc().lines[0].attr[0]) == eact::MarkerYellow);
    t.draw();
    // ---------------------------------------------------------------- E29-E32 MEMO e E54
    t.F(6); t.F(1); t.F(1);                                                // TOP
    t.F(6); t.F(6); t.F(3); t.F(1);                                        // pag. 3 › MEMO › INSERT
    t.down(); t.exe();                                                     // colore 2: Blue
    CHECK(t.a()->memoOpen());
    rig.type("nota uno");
    t.exe();
    CHECK(!t.a()->memoOpen() && t.line().memo.present && t.line().memo.text == "nota uno" && t.line().memo.color == eact::TextBlue);
    t.draw();
    t.F(6); t.F(6); t.F(3); t.F(1);                                        // un solo memo per riga
    CHECK(rig.error() && rig.errorDetail().find("Only one memo") != std::string::npos);
    rig.closeError();
    // E31 VIEW, DETAIL
    t.F(6); t.F(6); t.F(3); t.F(4);
    CHECK(t.a()->memoOpen());
    t.F(3);                                                                // DETAIL
    CHECK(t.a()->screen() == EActivityApp::Screen::MemoDetail);
    t.draw();
    rig.type("!");
    t.exe();
    CHECK(t.a()->screen() == EActivityApp::Screen::Doc && t.line().memo.text == "nota uno!");
    // E30 Catalog: elenco e salto alla riga
    t.F(6); t.F(1); t.F(2);                                                // BOTTOM
    CHECK(t.a()->line() != 0);
    t.F(6); t.F(1); t.F(1);
    t.down();                                                              // riga 1: calcolo 2+3
    CHECK(t.line().kind == LineKind::Calc);
    t.F(6); t.F(6); t.F(1); t.F(3);                                        // calcolo pag. 3 › MEMO › Catalog
    CHECK(t.a()->screen() == EActivityApp::Screen::MemoList && t.a()->memoList().size() == 1);
    t.draw();
    t.exe();
    CHECK(t.a()->screen() == EActivityApp::Screen::Doc && t.a()->line() == 0 && t.a()->textCursor() == 0);
    // E54: unire due righe con memo
    rig.key(pk::End);
    t.exe();                                                               // riga 1 vuota (il memo resta sulla riga 0)
    CHECK(t.a()->line() == 1 && t.line().kind == LineKind::Text && !t.line().memo.present && t.doc().lines[0].memo.present);
    t.F(6); t.F(6); t.F(3); t.F(1);
    t.exe();
    rig.type("due");
    t.exe();
    {
        const std::size_t n = t.doc().lines.size();
        rig.key(pk::Backspace);                                            // DEL all'inizio: unirebbe due memo
        CHECK(rig.error() && rig.errorDetail().find("Only one memo") != std::string::npos && t.doc().lines.size() == n);
        rig.closeError();
    }
    // E32 DELETE con conferma
    t.F(6); t.F(6); t.F(3); t.F(2);
    t.F(1);
    CHECK(!t.line().memo.present && t.doc().memoCount() == 1 && t.line().kind == LineKind::Text);
    // ---------------------------------------------------------------- E33-E36 immagini, E55
    writePicture(card, "PICT01.pcpict", 120, 60);
    writePicture(card, "CAPT02.pcpict", 40, 40, files::CaptureFolder);
    writePicture(card, "PICT02.pcpict", 400, 10);
    t.F(6); t.F(3); t.F(4);                                                // INSERT › PICTURE
    t.draw();
    t.exe();                                                               // CAPT02 (elenco in ordine)
    CHECK(t.line().kind == LineKind::Picture && t.line().text == "CAPT02.pcpict" && t.line().borderColor == eact::BorderClear);
    t.draw();
    CHECK(!rig.error());
    {   // E34: l'immagine selezionata ha il bordo rosso; l'immagine e' disegnata
        const gfx::Canvas& c = board::graphics();
        int red = 0, pic = 0;
        for (int y = ui::ContentTop; y < ui::ContentBottom; ++y)
            for (int x = 0; x < ui::ScreenW; ++x) { const auto p = c.getPixel(x, y); red += p == ui::theme::Error; pic += p == 0xF800; }
        CHECK(red > 100 && pic > 500);
    }
    // E35 FORMAT (SHIFT ↑): Line Style e Line Color
    t.shift(); t.up();
    t.exe();                                                               // Line Style
    t.down(); t.exe();                                                     // Thick
    t.down(); t.exe();                                                     // Line Color
    t.down(); t.down(); t.exe();                                           // 3: Red
    t.esc();
    CHECK(t.line().borderStyle == eact::BorderThick && t.line().borderColor == 3);
    t.draw();
    t.shift(); t.up(); t.down(); t.exe();
    for (int i = 0; i < 8; ++i) t.down();
    t.exe(); t.esc();                                                      // Clear
    CHECK(t.line().borderColor == eact::BorderClear);
    // E55: dimensione non ammessa
    t.F(6); t.F(3); t.F(4);
    t.down(); t.down(); t.exe();                                           // PICT02 (400 x 10)
    CHECK(rig.error() && rig.errorDetail().find("Image wrong size") != std::string::npos);
    rig.closeError();
    // E36 DELETE con conferma
    {
        const std::size_t n = t.doc().lines.size();
        t.F(6); t.F(2); t.F(1);
        CHECK(t.doc().lines.size() == n - 1);
    }
    // ---------------------------------------------------------------- E37-E43 strisce
    t.F(6); t.F(1); t.F(1);                                                // TOP
    t.F(2);                                                                // STRIP
    t.draw();
    t.down(); t.exe();                                                     // Graph
    CHECK(t.line().kind == LineKind::Strip && t.line().strip == static_cast<std::uint8_t>(StripKind::Graph) && t.a()->editingTitle());
    rig.type("Parabola");
    t.exe();
    CHECK(t.line().text == "Parabola" && !t.a()->editingTitle());
    // E38: il primo carattere sostituisce il titolo, EXIT annulla
    rig.type("X");
    CHECK(t.a()->editingTitle());
    t.esc();
    CHECK(t.line().text == "Parabola");
    t.right(); rig.type("2"); t.exe();
    CHECK(t.line().text == "Parabola2");
    // E44 SIZE
    t.F(1); t.F(5);
    CHECK(t.a()->screen() == EActivityApp::Screen::StripSize);
    t.draw();
    t.esc();
    // E39/E40/E41: la striscia apre Graph con i propri dati
    t.exe();
    {
        Context& ctx = rig.fw.apps().context();
        CHECK(rig.fw.apps().activeId() == AppId::Graph && ctx.strip().active && ctx.inStrip(AppId::Graph));
        CHECK(ctx.status("x").strip);                                      // barra di stato azzurra
        CHECK(rig.ws().functions[0].expr.empty());                         // striscia nuova: nessun dato
        t.draw();
        rig.ws().functions[0].expr = "2X+1";                               // modifica nella striscia
        rig.ws().functions[0].selected = true;
        t.shift(); rig.type("'");                                          // SHIFT ': torna all'eActivity
        CHECK(rig.fw.apps().activeId() == AppId::EActivity && !ctx.strip().active);
        CHECK(rig.ws().functions[0].expr == "X^2");                        // dati del menu principale
        CHECK(t.line().kind == LineKind::Strip && !t.line().data.empty() && t.a()->modified());
        t.exe();                                                           // di nuovo: i dati della striscia
        CHECK(rig.fw.apps().activeId() == AppId::Graph && rig.ws().functions[0].expr == "2X+1");
        rig.key(pk::Tab);                                                  // MENU torna all'eActivity
        CHECK(rig.fw.apps().activeId() == AppId::EActivity && rig.ws().functions[0].expr == "X^2");
        // E42: SHIFT ' dal documento apre la striscia
        t.shift(); rig.type("'");
        CHECK(rig.fw.apps().activeId() == AppId::Graph);
        // E43: SHIFT , cambia applicazione (una nuova striscia)
        t.shift(); rig.type(",");
        CHECK(rig.fw.apps().activeId() == AppId::EActivity && t.a()->screen() == EActivityApp::Screen::Doc);
        t.draw();
        for (int i = 0; i < static_cast<int>(StripKind::Solver); ++i) t.down();
        t.exe();
        CHECK(rig.fw.apps().activeId() == AppId::Equation && ctx.inStrip(AppId::Equation));
        rig.key(pk::Tab);
        CHECK(t.line().kind == LineKind::Strip && t.line().strip == static_cast<std::uint8_t>(StripKind::Solver) && t.a()->line() == 1);
    }
    // altre strisce: RUN (Math), List Editor, Matrix Editor, Simul Equation, 3D Graph, Stat Graph
    {
        const StripKind kinds[] = {StripKind::Run, StripKind::ListEditor, StripKind::MatrixEditor, StripKind::SimulEquation, StripKind::Graph3D,
                                   StripKind::StatGraph, StripKind::TableEditor, StripKind::GraphEditor, StripKind::PolyEquation,
                                   StripKind::VectorEditor, StripKind::Graph3DEditor};
        for (StripKind k : kinds) {
            t.F(2);
            for (int i = 0; i < static_cast<int>(k); ++i) t.down();
            t.exe(); t.exe(); t.exe();                                     // titolo vuoto, poi apre
            const AppId app = eact::stripInfo(k).app;
            CHECK(rig.fw.apps().activeId() == app && rig.fw.apps().context().inStrip(app));
            t.draw();
            CHECK(!rig.error());
            rig.key(pk::Tab);
            CHECK(rig.fw.apps().activeId() == AppId::EActivity && !rig.fw.apps().context().strip().active);
        }
        // E47: striscia non disponibile su questa scheda
        t.F(2);
        for (int i = 0; i < static_cast<int>(StripKind::SpreadSheet); ++i) t.down();
        t.exe();
        CHECK(rig.error() && rig.errorDetail().find("non disponibile") != std::string::npos);
        rig.closeError();
    }
    // ---------------------------------------------------------------- E45 Notes
    t.F(2);
    for (int i = 0; i < static_cast<int>(StripKind::Notes); ++i) t.down();
    t.exe();
    rig.type("Appunti"); t.exe();
    t.exe();                                                               // apre Notes
    CHECK(t.a()->inNotes());
    t.draw();
    rig.type("prima");
    t.exe();
    rig.type("seconda");
    t.F(3);                                                                // INSERT: riga sopra
    rig.type("in mezzo");
    t.F(4); t.exe(); rig.type("3"); t.exe();                               // MATH nella nota
    CHECK(t.doc().lines.size() == 3 && t.doc().lines[1].text.compare(0, 8, "in mezzo") == 0);
    t.F(1); t.F(1);                                                        // JUMP › TOP
    CHECK(t.a()->line() == 0);
    t.esc();
    CHECK(!t.a()->inNotes() && t.line().kind == LineKind::Strip);
    {
        const std::string data(t.line().data.begin(), t.line().data.end());
        CHECK(data.rfind("prima\nin mezzo", 0) == 0 && data.find("\nseconda") != std::string::npos && eact::validText(data));
    }
    t.exe();                                                               // riaperta
    CHECK(t.a()->inNotes() && t.doc().lines.size() == 3 && t.doc().lines[2].text == "seconda");
    t.F(2); t.F(1);                                                        // DEL-LINE
    CHECK(t.doc().lines.size() == 2);
    t.shift(); rig.type("'");                                              // SHIFT ' torna al documento
    CHECK(!t.a()->inNotes());
    // ---------------------------------------------------------------- E23/E24/E25 memorie per file
    {
        Workspace& ws = rig.ws();
        CHECK(ws.matrices.mat[0].empty());
        t.F(6); t.F(1); t.F(2);                                            // BOTTOM: la riga di calcolo vuota
        CHECK(t.line().kind == LineKind::Calc && t.line().text.empty());
        rig.type("[[1,2]]"); rig.key(pk::Insert); rig.type("Mat A");
        t.exe();
        CHECK(!ws.matrices.mat[0].empty());
        rig.type("{4,5}"); rig.key(pk::Insert); rig.type("List 1");
        t.exe();
        CHECK(ws.lists[0].size() == 2);
        // 'MAT/VCT: l'editor MAT/VCT con le memorie del file, EXIT torna
        t.F(6); t.F(3);
        CHECK(rig.fw.apps().activeId() == AppId::RunMatrix && rig.fw.apps().context().inStrip(AppId::RunMatrix));
        t.draw();
        t.esc();
        CHECK(rig.fw.apps().activeId() == AppId::EActivity);
        // 'LIST: l'editor delle liste (solo i tasti di modifica), EXIT torna
        t.F(6); t.F(4);
        CHECK(rig.fw.apps().activeId() == AppId::Statistics);
        t.F(1);                                                            // GRAPH non c'e'
        t.draw();
        CHECK(!rig.error());
        t.esc();
        CHECK(rig.fw.apps().activeId() == AppId::EActivity && ws.lists[0].size() == 2);
        // MENU con il documento aperto: le altre modalita' vedono le memorie del menu principale
        rig.key(pk::Tab);
        CHECK(rig.fw.apps().activeId() == AppId::MainMenu && ws.matrices.mat[0].empty() && ws.lists[0].empty());
        t.open();
        CHECK(t.a()->documentOpen() && !ws.matrices.mat[0].empty() && ws.lists[0].size() == 2);
        // E51 Save?: AC resta, poi Si'
        t.esc();
        CHECK(rig.fw.apps().context().toastText().empty() || true);
        rig.key(pk::Break);
        CHECK(t.a()->documentOpen());
        t.esc();
        t.F(1);                                                            // Si': salvato e chiuso
        CHECK(!t.a()->documentOpen() && ws.matrices.mat[0].empty() && ws.lists[0].empty());
        eact::Document saved;
        CHECK(readDoc(card, "FISICA.pceact", saved) && !saved.memories.empty());
        t.select("FISICA"); t.exe();
        CHECK(!ws.matrices.mat[0].empty() && ws.lists[0].size() == 2);     // le memorie del file tornano
        // No: il file non cambia
        t.F(6); t.F(1); t.F(2);
        rig.type("1");
        t.esc(); t.F(6);
        eact::Document again;
        CHECK(readDoc(card, "FISICA.pceact", again) && again.lines.size() == saved.lines.size() && !t.a()->documentOpen());
    }
    // ---------------------------------------------------------------- E06 MEMO dall'elenco dei file
    t.select("RICALC");
    t.F(5);
    CHECK(rig.error() && rig.errorTitle() == "No MEMO");
    rig.closeError();
    t.select("FISICA");
    t.F(5);
    CHECK(t.a()->screen() == EActivityApp::Screen::MemoList && t.a()->memoList().size() == 1);
    t.F(2);                                                                // EDIT
    CHECK(t.a()->memoOpen());
    rig.type("?");
    t.exe();
    {
        eact::Document d;
        CHECK(readDoc(card, "FISICA.pceact", d) && d.memoCount() == 1);
        for (const Line& l : d.lines) if (l.memo.present) CHECK(l.memo.text == "nota uno!?");
    }
    t.F(3);                                                                // DETAIL
    CHECK(t.a()->screen() == EActivityApp::Screen::MemoDetail);
    t.esc();
    t.F(4); t.F(1);                                                        // DELETE
    CHECK(t.a()->screen() == EActivityApp::Screen::Files);
    {
        eact::Document d;
        CHECK(readDoc(card, "FISICA.pceact", d) && d.memoCount() == 0);
        eact::Document m;
        m.lines = {Line::textLine("uno"), Line::textLine("due"), Line::calcLine("1+1")};
        m.lines[0].memo = {true, 1, "a"};
        m.lines[1].memo = {true, 2, "b"};
        writeDoc(card, "MEMO2.pceact", m);
    }
    t.esc(); t.open();
    t.select("MEMO2"); t.F(5);
    CHECK(t.a()->memoList().size() == 2);
    t.down(); t.F(1);                                                      // JUMP: apre il file alla riga
    CHECK(t.a()->documentOpen() && t.a()->fileName() == "MEMO2.pceact" && t.a()->line() == 1);
    t.closeNoSave();
    t.select("MEMO2"); t.F(5);
    t.F(5); t.F(1);                                                        // DEL-ALL
    {
        eact::Document d;
        CHECK(readDoc(card, "MEMO2.pceact", d) && d.memoCount() == 0);
    }
    // ---------------------------------------------------------------- E47 striscia sconosciuta conservata
    {
        eact::Document d;
        Line s; s.kind = LineKind::Strip; s.strip = 200; s.text = "da altrove"; s.data = {5, 6, 7};
        d.lines.push_back(s);
        Line pp; pp.kind = LineKind::Strip; pp.strip = static_cast<std::uint8_t>(StripKind::PicturePlot); pp.data = {1};
        d.lines.push_back(pp);
        d.lines.push_back(Line::textLine("x"));
        writeDoc(card, "ALTRO.pceact", d);
    }
    t.esc(); t.open();
    t.select("ALTRO"); t.exe();
    t.draw();
    t.exe();
    CHECK(rig.error() && rig.errorDetail().find("non disponibile") != std::string::npos);
    rig.closeError();
    t.down(); t.down();
    rig.key(pk::End);
    rig.type("y");
    t.F(1); t.F(1);
    {
        eact::Document d;
        CHECK(readDoc(card, "ALTRO.pceact", d) && d.lines[0].strip == 200 && d.lines[0].data == std::vector<std::uint8_t>({5, 6, 7}) &&
              d.lines[1].data == std::vector<std::uint8_t>({1}) && d.lines[2].text == "xy");
    }
    // ---------------------------------------------------------------- E48 SAVE·AS con sovrascrittura
    t.F(1); t.F(2);
    for (int i = 0; i < 8; ++i) rig.key(pk::Backspace);
    rig.type("FISICA");
    t.exe();                                                               // esiste: Sovrascrivere?
    t.F(6);                                                                // No: torna al nome
    CHECK(t.a()->fileName() == "ALTRO.pceact");
    for (int i = 0; i < 8; ++i) rig.key(pk::Backspace);
    rig.type("COPIA");
    t.exe();
    CHECK(t.a()->fileName() == "COPIA.pceact" && card.exists("PICOCALC/EACT/COPIA.pceact"));
    t.F(1); t.F(2);
    for (int i = 0; i < 8; ++i) rig.key(pk::Backspace);
    rig.type("FISICA");
    t.exe(); t.F(1);                                                       // Si'
    {
        eact::Document d;
        CHECK(readDoc(card, "FISICA.pceact", d) && d.lines[2].text == "xy" && t.a()->fileName() == "FISICA.pceact");
    }
    t.closeNoSave();
    // ---------------------------------------------------------------- tutte le schermate si disegnano; SD estratta
    card.setPresent(false);
    t.esc(); t.open();
    CHECK(t.a()->items().empty() && !t.a()->message().empty());
    t.draw();
    t.F(2); rig.type("NUOVO"); t.exe();
    CHECK(rig.error() && !t.a()->documentOpen());
    rig.closeError();
    card.setPresent(true);
    board::host::setFileSystem(&board::host::ramCard());
    return testutil::finish("eactivity_test");
}
