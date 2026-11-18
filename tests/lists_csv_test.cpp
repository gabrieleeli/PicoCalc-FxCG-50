// v1.11.0 — List 1-26 con sottonomi e limiti per scheda, file CSV di liste e
// matrici (microSD e flash interna, scrittura atomica, errori, interruzione
// di alimentazione), celle con espressioni esatte e complesse e scorrimento
// delle colonne dell'editor MAT/VCT, migrazione dei record v6 (v1.10.0) al v7.
#include "app_rig.hpp"
#include "csv_io.hpp"
#include "flash_fs.hpp"
#include "graph_engine.hpp"
#include "run_matrix.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include <cstdio>
#include <string>
#include <vector>

using namespace fx;
using files::Status;

namespace {
const std::uint8_t kV1100Record[] = {
#include "fixtures/workspace_v1_10_0.inc"
};
std::string last(Workspace& w) { return resultText(w.history.back(), w.settings); }
std::vector<std::uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }
csv::Table csvTable(const std::string& text, csv::Format f = {}, std::size_t rows = 999, std::size_t cols = 26) {
    const auto b = bytes(text);
    return csv::decode(b.data(), b.size(), f, rows, cols);
}
template <typename E> bool decodeFails(const std::string& text, csv::Format f = {}, std::size_t rows = 999, std::size_t cols = 26) {
    try { (void)csvTable(text, f, rows, cols); } catch (const E&) { return true; } catch (...) { return false; }
    return false;
}
std::string readText(files::FileSystem& fs, const std::string& path) {
    std::vector<std::uint8_t> out;
    if (files::readAll(fs, path, out, 1u << 20) != Status::Ok) return "<err>";
    return {out.begin(), out.end()};
}
} // namespace

int main() {
    board::init();
    // ================================================================ List 1-26 nei calcoli
    {
        Workspace w;
        runCalculation(w, "{1,2,3}\xE2\x86\x92List 26");
        CHECK(w.lists[25] == std::vector<double>({1, 2, 3}));
        runCalculation(w, "List 26[2]");
        CHECK_EQ_STR(last(w), "2");
        runCalculation(w, "{4,5}\xE2\x86\x92List 12");
        runCalculation(w, "Sum(List 12)+Dim(List 26)");
        CHECK_EQ_STR(last(w), "12");
        runCalculation(w, "List12+1");                                     // nome attaccato
        CHECK_EQ_STR(last(w), "{5,6}");
        runCalculation(w, "7\xE2\x86\x92List 20[1]");                       // una lista cresce di una cella
        CHECK(w.lists[19] == std::vector<double>({7}));
        CHECK(parseExpression("List 26")->text == "list26" && parseExpression("List 9")->text == "list9");
        CHECK_THROWS_AS(parseExpression("List 27"), SyntaxError);
        CHECK_THROWS_AS(parseExpression("List 0"), SyntaxError);
        // Le liste restano reali
        w.settings.complex = ComplexMode::Rectangular;
        CHECK_THROWS_AS(runCalculation(w, "{1+i}\xE2\x86\x92List 15"), MathError);
        CHECK(w.lists[14].empty());
    }
    // ================================================================ sottonomi
    {
        Workspace w;
        w.setListName(0, "QTY");
        w.setListName(25, "PRICE_1");
        CHECK(w.listName(0) == "QTY" && w.listName(25) == "PRICE_1" && w.listName(30).empty());
        CHECK_THROWS(w.setListName(1, "TOOLONGNAME"));                     // al massimo 8 caratteri
        CHECK_THROWS(w.setListName(1, "A\"B"));
        CHECK_THROWS(w.setListName(26, "X"));
        CHECK(w.listName(1).empty());
        const auto blob = serializeWorkspace(w);
        Workspace back;
        CHECK(blob[4] == 10 && deserializeWorkspace(blob.data(), blob.size(), back));
        CHECK(back.listName(0) == "QTY" && back.listName(25) == "PRICE_1");
    }
    // ================================================================ budget delle liste per scheda
    {
        Workspace w;
        CHECK(Workspace::ListCount == 26 && Workspace::MaxListElements == limits::kListStoreElements);
        const std::size_t perList = Workspace::MaxListLength;
        std::size_t stored = 0, index = 0;
        bool full = false;
        for (; index < Workspace::ListCount; ++index) {
            std::vector<double> values(perList, 1.0);
            try { w.setList(index, values); stored += perList; } catch (const MathError& e) {
                full = std::string(e.what()).find("Memory ERROR") == 0; break;
            }
        }
        CHECK(full && stored <= Workspace::MaxListElements && stored + perList > Workspace::MaxListElements);
        CHECK(w.listElements() == stored);
        CHECK(!w.listFits(index, perList) && w.listFits(index, Workspace::MaxListElements - stored));
        CHECK_THROWS_AS(runCalculation(w, "Seq(X,X,1," + std::to_string(perList) + ",1)\xE2\x86\x92List " + std::to_string(index + 1)), MathError);
        CHECK(w.lists[index].empty());                                          // tutto o niente
        CHECK_THROWS_AS(w.setList(0, std::vector<double>(perList + 1, 0.0)), DimensionError);
        if (board::config::kIsPicoPlus2W) CHECK(Workspace::MaxListElements == 20000 && index == 20);
        else CHECK(Workspace::MaxListElements == 1530 && index == 6);
        // Un record con le liste oltre il budget non si legge
        Workspace big = w;
        auto blob = serializeWorkspace(big);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back) && back.listElements() == stored);
    }
    // ================================================================ GRAPH con List 7-26
    {
        Workspace w;
        w.lists[11] = {1, 2, 3};
        w.functions[0].expr = "List 12x";
        w.apply();
        graph::GraphEngine e;
        e.build({&w.functions, &w.memory, &w, &w.lists});
        std::size_t plots = 0;
        for (const auto& p : e.plots()) if (p.function == 0) ++plots;
        CHECK(plots == 3);                                                      // un grafico per elemento
        CHECK(e.error(0).empty());
    }
    // ================================================================ Statistics: 26 colonne, SUB, SET
    {
        testrig::Rig r;
        r.type("2");
        auto* st = r.active<app::StatisticsApp>();
        CHECK(st != nullptr);
        for (int i = 0; i < 25; ++i) r.key(pckey::Right);                    // colonna di List 26
        CHECK(st && st->grid().col == 25 && st->grid().firstColumn() > 0);
        r.enter("4"); r.enter("8");
        CHECK(r.ws().lists[25] == std::vector<double>({4, 8}));
        r.key(pckey::Up); r.key(pckey::Up); r.key(pckey::Up);                // riga SUB sopra la prima cella
        CHECK(st && st->grid().row == app::CellGrid::SubRow);
        r.enter("QTY");
        CHECK(r.ws().listName(25) == "QTY");
        CHECK(st && st->grid().row == 0);                                     // EXE scende alla prima cella
        // SET: XList = List 26
        r.ws().settings.statXList = 25;
        r.key(pckey::F2); r.key(pckey::Enter);                                  // CALC > 1-Var
        CHECK(!r.error() && st && st->screen() == app::StatisticsApp::Screen::Results);
        CHECK(st && st->results().title == "1-Variable (List26)");
        bool mean6 = false;
        if (st) for (const auto& row : st->results().rows) if (row.second == "6") mean6 = true;
        CHECK(mean6);
        r.key(pckey::Esc);
        r.ws().settings.statXList = 0;
        r.home();
    }
    // ================================================================ CSV: codifica e decodifica
    {
        const std::vector<double> a = {1, 2.5, -3}, b = {10};
        CHECK_EQ_STR(csv::encodeColumns({&a, &b}, {}), "1,10\r\n2.5,\r\n-3,\r\n");
        CHECK_EQ_STR(csv::encodeColumns({&a}, {';', ','}), "1\r\n2,5\r\n-3\r\n");
        CalcValue m = CalcValue::matrix(2, 2);
        m.re = {1, 0.125, 1e20, -4};
        CHECK_EQ_STR(csv::encodeMatrix(m, {}), "1,0.125\r\n1e+20,-4\r\n");
        m.set(1, Complex(0, 1));
        CHECK_THROWS_AS(csv::encodeMatrix(m, {}), MathError);                  // Complex Number In Data
        CHECK(!csv::validate({',', ','}).empty() && !csv::validate({'\t', '.'}).empty() && csv::validate({';', ','}).empty());
        // Righe CR, LF, CRLF; intestazione con virgolette ignorata
        csv::Table t = csvTable("\"x\",\"y\"\n1,2\r\n3,4\r5,6");
        CHECK(t.rows == 3 && t.cols == 2 && t.at(0, 0) == 1 && t.at(2, 1) == 6);
        t = csvTable("1;2,5\n3;-4e2\n", {';', ','});
        CHECK(t.rows == 2 && t.at(0, 1) == 2.5 && t.at(1, 1) == -400);
        t = csvTable("1,2\n3\n");
        CHECK(t.rows == 2 && t.cols == 2 && t.has(1, 0) && !t.has(1, 1));
        CHECK(decodeFails<MathError>("1,abc\n"));                              // non numerico
        CHECK(decodeFails<MathError>("1\t2\n"));                               // tabulatore
        CHECK(decodeFails<MathError>(""));                                     // vuoto
        CHECK(decodeFails<MathError>("1.5\n", {';', ','}));                    // decimale sbagliato
        CHECK(decodeFails<MathError>("inf\n"));
        CHECK(decodeFails<DimensionError>("1,2,3\n", {}, 999, 2));              // oltre le colonne
        CHECK(decodeFails<DimensionError>("1\n2\n3\n", {}, 2, 26));             // oltre le righe
        // Liste: colonne di lunghezze diverse, celle vuote solo alla fine
        Workspace w;
        CHECK(csv::loadLists(w, 2, csvTable("1,10\n2,\n3,\n")) == 2);
        CHECK(w.lists[2] == std::vector<double>({1, 2, 3}) && w.lists[3] == std::vector<double>({10}));
        CHECK_THROWS_AS(csv::loadLists(w, 0, csvTable("1\n\n3\n")), MathError);   // vuota in mezzo
        CHECK_THROWS_AS(csv::loadLists(w, 25, csvTable("1,2\n")), DimensionError);  // oltre List 26
        // Matrici: nessuna cella vuota, limiti della scheda
        csv::loadMatrix(w, false, 0, csvTable("1,2\n3,4\n"));
        CHECK(w.matrices.mat[0].rows == 2 && w.matrices.mat[0].re[3] == 4);
        CHECK_THROWS_AS(csv::loadMatrix(w, false, 1, csvTable("1,2\n3\n")), MathError);
        CHECK_THROWS_AS(csv::loadMatrix(w, true, 1, csvTable("1,2\n3,4\n")), DimensionError);   // non e' un vettore
        csv::loadMatrix(w, true, 1, csvTable("1,2,3\n"));
        CHECK(w.matrices.vct[1].type == ValueType::Vector && w.matrices.vct[1].cols == 3);
        CHECK_EQ_STR(csv::encodeLists(w), ",,1,10\r\n,,2,\r\n,,3,\r\n");
    }
    // ================================================================ CSV sulla microSD e sulla flash interna
    {
        for (int store = 0; store < 2; ++store) {
            storage::RamFlash flash(256 * 1024);
            files::LittleFsFileSystem lfs(flash);
            files::RamFileSystem card;
            files::FileSystem& fs = store == 0 ? static_cast<files::FileSystem&>(card) : lfs;
            std::vector<std::string> names;
            CHECK(csv::list(fs, names) == Status::Ok && names.empty());       // cartella non ancora creata
            CHECK(csv::save(fs, "DATA1", "1,2\r\n") == Status::Ok);
            CHECK(csv::save(fs, "B", "3\r\n") == Status::Ok);
            CHECK(csv::list(fs, names) == Status::Ok && names == std::vector<std::string>({"B", "DATA1"}));
            std::vector<std::uint8_t> got;
            CHECK(csv::load(fs, "DATA1", got) == Status::Ok && std::string(got.begin(), got.end()) == "1,2\r\n");
            CHECK(csv::load(fs, "NONE", got) == Status::NotFound);              // file mancante
            CHECK_THROWS_AS(csv::save(fs, "TOO_LONG_NAME", "1"), MathError);
            CHECK(csv::load(fs, "a/b", got) == Status::InvalidName);
        }
        // Interruzione di alimentazione durante il salvataggio sulla flash interna:
        // dopo il riavvio il file e' quello vecchio o quello nuovo, mai un misto.
        const std::string oldText(2000, '1'), newText(6000, '2');
        int cuts = 0, oldSeen = 0, newSeen = 0;
        for (std::size_t budget = 256; budget < 40000; budget += 1024) {
            storage::RamFlash flash(128 * 1024);
            {
                files::LittleFsFileSystem fs(flash);
                CHECK(csv::save(fs, "LISTS", oldText) == Status::Ok);
                flash.failAfter(budget);
                cuts += csv::save(fs, "LISTS", newText) != Status::Ok;
            }
            flash.powerOn();
            files::LittleFsFileSystem fs(flash);
            CHECK(fs.status() == Status::Ok && !fs.formattedAtMount());
            std::vector<std::uint8_t> got;
            CHECK(csv::load(fs, "LISTS", got) == Status::Ok);                  // load() completa o annulla il salvataggio
            const std::string now(got.begin(), got.end());
            CHECK(now == oldText || now == newText);
            oldSeen += now == oldText; newSeen += now == newText;
            std::vector<files::Entry> entries;
            CHECK(fs.list(csv::Folder, entries) == Status::Ok && entries.size() == 1);   // niente .tmp/.bak
        }
        CHECK(cuts > 5 && oldSeen > 0 && newSeen > 0);
    }
    // ================================================================ CSV dalle applicazioni (archivio scelto in SYSTEM)
    {
        testrig::Rig r;
        auto& ws = r.ws();
        for (int store = 0; store < 2; ++store) {
            ws.settings.fileStore = store ? Settings::FileStore::Internal : Settings::FileStore::MicroSd;
            files::FileSystem* fs = store ? r.platform.internalFiles() : r.platform.files();
            CHECK(fs != nullptr);
            for (auto& l : ws.lists) l.clear();                               // dati del blocco precedente
            ws.lists[0] = {1, 2, 3}; ws.lists[1] = {4.5};
            r.type("2");
            r.key(pckey::F8);                                                  // OPTN dell'editor delle liste
            r.key(pckey::Down); r.key(pckey::Enter);                           // CSV SAVE·AS
            r.enter("STAT" + std::to_string(store));
            CHECK(!r.error());
            CHECK(fs && readText(*fs, "PICOCALC/CSV/STAT" + std::to_string(store) + ".csv") == "1,4.5\r\n2,\r\n3,\r\n");
            for (auto& l : ws.lists) l.clear();
            for (int i = 0; i < 26; ++i) r.key(pckey::Left);                   // colonna di List 1
            r.key(pckey::Right); r.key(pckey::Right);                          // LOAD da List 3
            r.key(pckey::F8); r.key(pckey::Enter);                             // CSV LOAD
            r.key(pckey::Enter);                                               // primo file dell'elenco
            CHECK(!r.error());
            CHECK(ws.lists[2] == std::vector<double>({1, 2, 3}) && ws.lists[3] == std::vector<double>({4.5}) && ws.lists[0].empty());
            r.home();
            // MAT/VCT: CSV SAVE·AS di Mat A e LOAD in Mat B
            ws.matrices.clear();
            runCalculation(ws, "[[1,2,3][4,5,6]]\xE2\x86\x92Mat A");
            r.type("1");
            r.key(pckey::F7); r.type("3");                                     // SHIFT 3: editor MAT/VCT
            r.key(pckey::Home);                                                // Mat A
            auto* rm = r.active<app::RunMatrixApp>();
            CHECK(rm && rm->matrixEditor().isOpen());
            r.key(pckey::F4); r.key(pckey::Down); r.key(pckey::Enter);         // CSV > SAVE·AS (nome proposto MATA)
            r.key(pckey::Enter);
            CHECK(!r.error() && fs && readText(*fs, "PICOCALC/CSV/MATA.csv") == "1,2,3\r\n4,5,6\r\n");
            r.key(pckey::Down);                                                // Mat B
            r.key(pckey::F4); r.key(pckey::Enter);                             // CSV > LOAD
            // Il primo file in ordine alfabetico e' MATA
            r.key(pckey::Enter);
            CHECK(!r.error() && ws.matrices.mat[1] == ws.matrices.mat[0]);
            // Errore: un file con testo non numerico non cambia nulla
            CHECK(csv::save(*fs, "AAA", "1,x\r\n") == Status::Ok);
            r.key(pckey::Down);                                                // Mat C
            r.key(pckey::F4); r.key(pckey::Enter); r.key(pckey::Enter);       // LOAD AAA
            CHECK(r.error() && r.errorTitle() == "Format ERROR");
            r.closeError();
            CHECK(ws.matrices.mat[2].empty());
            r.key(pckey::Esc);
            r.home();
        }
        // Scheda assente: Storage ERROR
        static_cast<files::RamFileSystem&>(board::host::ramCard()).setPresent(false);
        ws.settings.fileStore = Settings::FileStore::MicroSd;
        r.type("2");
        r.key(pckey::F8); r.key(pckey::Enter);
        CHECK(r.error() && r.errorTitle() == "Storage ERROR");
        r.closeError();
        static_cast<files::RamFileSystem&>(board::host::ramCard()).setPresent(true);
        r.home();
    }
    // ================================================================ editor MAT/VCT: celle esatte e complesse, colonne
    {
        testrig::Rig r;
        auto& ws = r.ws();
        ws.matrices.clear();
        r.type("1");
        r.key(pckey::F7); r.type("3");
        auto* rm = r.active<app::RunMatrixApp>();
        CHECK(rm && rm->matrixEditor().isOpen());
        r.key(pckey::F3);                                                      // DIM 2x8
        r.key(pckey::Del); r.enter("2"); r.enter("8");
        CHECK(ws.matrices.mat[0].rows == 2 && ws.matrices.mat[0].cols == 8);
        r.enter("1/3");                                                        // forma esatta
        r.enter("2sqrt(3)");
        CHECK(ws.matrices.mat[0].hasExact());
        CHECK_NEAR(ws.matrices.mat[0].re[0], 1.0 / 3.0, 1e-15);
        r.enter("1+2i");                                                       // complesso anche nel modo Real
        CHECK(ws.matrices.mat[0].at(2) == Complex(1, 2) && !ws.matrices.mat[0].hasExact());
        r.enter("Mat A[1,1]*6");                                               // un'espressione con una memoria
        CHECK_NEAR(ws.matrices.mat[0].re[3], 2.0 / 3.0 * 3.0, 1e-12);
        for (int i = 0; i < 3; ++i) r.key(pckey::Right);
        CHECK(rm && rm->matrixEditor().grid().col == 7 && rm->matrixEditor().grid().firstColumn() >= 4);   // scorrimento
        CHECK_THROWS(runCalculation(ws, "[[1]]\xE2\x86\x92Mat Ans"));
        r.key(pckey::Esc); r.key(pckey::Esc);
        // Le forme esatte memorizzate partecipano ai calcoli esatti
        ws.matrices.clear();
        runCalculation(ws, "[[1/3,1/2][2,1/5]]\xE2\x86\x92Mat A");
        CHECK(ws.matrices.mat[0].hasExact());
        runCalculation(ws, "Mat A\xC3\x97" "6");
        CHECK_EQ_STR(last(ws), "[[2,3][12,6/5]]");
        runCalculation(ws, "Det(Mat A)");
        CHECK_EQ_STR(last(ws), "-14/15");
        runCalculation(ws, "1/7\xE2\x86\x92Mat A[2,2]");
        runCalculation(ws, "Mat A[2,2]\xC3\x97" "7");
        CHECK_EQ_STR(last(ws), "1");
        CHECK(ws.matrices.matAns.hasExact());
        // Persistenza delle forme esatte (record v7)
        const auto blob = serializeWorkspace(ws);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back) && back.matrices.mat[0].hasExact());
        CHECK(back.matrices.mat[0].exact[3] == ws.matrices.mat[0].exact[3]);
        // Forme esatte che non corrispondono ai valori: record rifiutato
        Workspace bad = ws;
        bad.matrices.mat[0].re[0] = 5;
        const auto badBlob = serializeWorkspace(bad);
        CHECK(!deserializeWorkspace(badBlob.data(), badBlob.size(), back));
        r.home();
    }
    // ================================================================ migrazione v6 (v1.10.0) -> v7
    {
        Workspace w;
        CHECK(deserializeWorkspace(kV1100Record, sizeof kV1100Record, w));
        CHECK(w.settings.angle == AngleMode::Degrees && w.settings.inputMode == Settings::InputMode::Line && w.settings.mixedFractions);
        CHECK(w.settings.complex == ComplexMode::Rectangular);
        CHECK(w.memory.recall('B') == 7);
        CHECK(w.history.size() == 10);
        CHECK(w.matrices.mat[0].rows == 2 && w.matrices.mat[2].complex() && w.matrices.vct[1].cols == 3);
        CHECK(w.lists[0] == std::vector<double>({1.5, 2.5, 3.5}) && w.lists[1] == std::vector<double>({4, 5, 6}));
        CHECK(w.functions[1].expr == "List 1x" && w.graph3d.set.fns[1].kind == g3d::Kind::Z && w.graph3d.set.fns[1].expr[0] == "x*y" && w.chat.size() == 1);
        CHECK(w.table.start == -2 && w.table.step == 0.5);
        CHECK_EQ_STR(resultText(w.history[0], w.settings), "1/2");
        CHECK(isCollection(w.history[8]) && w.history[8].collection->value.empty());   // 7x7: elementi omessi
        // Valori predefiniti dei campi della v1.11.0
        CHECK(w.settings.baseMode == Settings::BaseMode::Comp && !w.settings.simplifyManual);
        CHECK(w.settings.csvSeparator == ',' && w.settings.csvDecimal == '.' && w.settings.statXList == 0 && w.settings.statYList == 1);
        for (std::size_t i = 6; i < Workspace::ListCount; ++i) CHECK(w.lists[i].empty());
        for (const auto& n : w.listNames) CHECK(n.empty());
        CHECK(w.graph.commandGraphs.empty());
        for (const auto& h : w.history) CHECK(h.base == 0 && h.fracDen == 0 && !h.command);
        // Calcoli con i dati importati
        runCalculation(w, "Sum(List 1+List 2)");
        CHECK_EQ_STR(last(w), "22.5");
        runCalculation(w, "Det(Mat A)");
        CHECK_EQ_STR(last(w), "-4");
        // Salvato come v7, riletto uguale
        const auto blob = serializeWorkspace(w);
        CHECK(blob[4] == 10 && blob[5] == 0);
        Workspace back;
        CHECK(deserializeWorkspace(blob.data(), blob.size(), back));
        CHECK(back.history.size() == w.history.size() && back.lists == w.lists && back.matrices.mat[2] == w.matrices.mat[2]);
        CHECK(serializeWorkspace(back) == blob);
        // Dati troncati: rifiutati senza toccare la destinazione
        Workspace target; target.memory.store('Q', 3);
        for (std::size_t cut = 0; cut < blob.size(); cut += 11) CHECK(!deserializeWorkspace(blob.data(), cut, target));
        CHECK(target.memory.recall('Q') == 3);
    }
    return testutil::finish("lists_csv_test");
}
