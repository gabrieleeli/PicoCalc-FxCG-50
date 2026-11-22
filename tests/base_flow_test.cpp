// v1.17.0: flussi delle modalita' Base con il firmware vero (app_rig, solo
// Pimoroni Pico Plus 2 W): per ogni modalita' apri dal menu, usa, salva il file
// tipizzato, esci, riapri e riavvia (il record v12 conserva lo stato).
#include "app_rig.hpp"
#include "link_protocol.hpp"
#include "pict.hpp"
#include "storage.hpp"
#include "test_util.hpp"
#include "typed_files.hpp"
#include <cmath>

using namespace fx;
using namespace fx::app;
namespace pk = fx::pckey;

namespace {
template <class A> std::string screen(testrig::Rig& r) { A* a = r.active<A>(); return a ? a->screenName() : "-"; }
void reboot(std::unique_ptr<testrig::Rig>& r) {
    CHECK(r->fw.saveNow());
    r.reset();
    r = std::make_unique<testrig::Rig>();
}
bool fileExists(const std::string& path) {
    files::Entry e;
    return board::host::ramCard().stat(path, e) == files::Status::Ok && !e.directory;
}
double cell(Workspace& ws, int col, int row) {
    const sheet::Cell* c = ws.sheet.sheet.cell(sheet::Ref{col, row});
    return c ? c->value : NAN;
}
void ticks(testrig::Rig& r, int n, std::uint32_t ms = 30) {
    for (int i = 0; i < n; ++i) { board::host::advance(ms); if (r.fw.step()) { r.fw.draw(board::graphics()); board::present(); } }
}

void spreadsheet(std::unique_ptr<testrig::Rig>& r) {
    r->type("4");
    CHECK(r->fw.apps().activeId() == AppId::Spreadsheet && screen<SpreadsheetApp>(*r) == "grid");
    r->enter("2");                                                          // A1 = 2, il cursore scende in A2
    r->key(pk::Up); r->key(pk::Right);
    r->enter("=A1*3");                                                      // B1
    r->type("8"); r->key(pk::Enter);                                         // B2 = 8
    r->settle();
    CHECK(cell(r->ws(), 1, 1) == 6 && cell(r->ws(), 1, 2) == 8 && !r->error());
    r->key(pk::Left); r->key(pk::Up); r->key(pk::Up);                        // A1
    r->enter("5");
    r->settle();
    CHECK(cell(r->ws(), 1, 1) == 15);                                       // ricalcolo automatico
    // EDIT > COPY di B1, PASTE in C1: il riferimento relativo si sposta
    r->key(pk::Up); r->key(pk::Right);
    r->key(pk::F2); r->key(pk::F2);
    r->key(pk::Right);
    r->key(pk::F2); r->key(pk::F3);
    r->settle();
    CHECK(r->ws().sheet.sheet.cell(sheet::Ref{2, 1})->input == "=B1*3" && cell(r->ws(), 2, 1) == 45);
    // ciclo: Circular ERROR nella cella, nessun crash
    r->key(pk::Down); r->enter("=C3");                                     // C2 = C3
    r->enter("=C2");                                                        // C3 = C2
    r->settle();
    CHECK(r->ws().sheet.sheet.cell(sheet::Ref{2, 2})->error == sheet::CircularError);
    // LIST > COL->L: la colonna B in List 1
    r->key(pk::Left);
    r->key(pk::F4); r->key(pk::F1); r->enter("1");
    CHECK(r->ws().lists[0] == std::vector<double>({15, 8}));
    // FILE > SAVE·AS
    r->key(pk::F1); r->key(pk::F3); r->enter("CONTI");
    CHECK(!r->error() && fileExists("PICOCALC/SHEET/CONTI.pcsheet") && r->ws().sheet.file == "CONTI.pcsheet");
    r->fw.draw(board::graphics());
    r->home();
    r->type("4");
    CHECK(cell(r->ws(), 1, 1) == 15);
    // FILE > NEW, poi OPEN del file salvato
    r->key(pk::F1); r->key(pk::F1); r->key(pk::Enter);
    CHECK(r->ws().sheet.sheet.empty());
    r->key(pk::F1); r->key(pk::F2); r->key(pk::Enter);
    r->settle();
    CHECK(cell(r->ws(), 1, 1) == 15 && r->ws().sheet.sheet.cell(sheet::Ref{0, 1})->input == "5");
    r->home();
    reboot(r);
    r->type("4");
    r->settle();
    CHECK(cell(r->ws(), 1, 1) == 15 && r->ws().sheet.file == "CONTI.pcsheet");
    r->home();
}

void dynaGraph(std::unique_ptr<testrig::Rig>& r) {
    r->type("6");
    CHECK(r->fw.apps().activeId() == AppId::DynaGraph && screen<DynaGraphApp>(*r) == "main");
    r->key(pk::F1);                                                         // DYNA
    CHECK(screen<DynaGraphApp>(*r) == "anim");
    ticks(*r, 80);
    r->key(pk::F2);                                                         // FASTER
    CHECK(r->ws().dyna.speed == dyna::Speed::Fast);
    r->key(pk::Break);                                                      // AC ferma
    CHECK(screen<DynaGraphApp>(*r) == "stopped");
    r->key(pk::Esc);
    CHECK(screen<DynaGraphApp>(*r) == "main");
    // Pitch 0: Argument ERROR, nulla cambia
    r->key(pk::Down); r->key(pk::Down); r->key(pk::Down); r->key(pk::Down);   // Pitch
    r->enter("0");
    CHECK(r->error());
    r->closeError();
    CHECK(r->ws().modeVars.dynaPitch == 1);
    r->key(pk::Up); r->enter("1");                                          // Pitch di nuovo valido
    r->key(pk::Up); r->key(pk::Up); r->key(pk::Up); r->enter("2");         // Start = 2 dal modulo
    CHECK(r->ws().modeVars.dynaStart == 2);
    r->key(pk::F5); r->key(pk::F2); r->enter("ONDA");
    CHECK(fileExists("PICOCALC/DYNA/ONDA.pcdyna"));
    // Il file porta anche Start, End e Pitch: OPEN li rimette
    r->key(pk::Up); r->enter("3");                                          // Start = 3 dal modulo, non salvato
    CHECK(r->ws().modeVars.dynaStart == 3);
    r->key(pk::F5); r->key(pk::F1); r->key(pk::Enter);
    CHECK(!r->error() && r->ws().modeVars.dynaStart == 2 && r->ws().modeVars.dynaEnd == 5 &&
          r->ws().modeVars.dynaPitch == 1 && r->ws().dyna.speed == dyna::Speed::Fast);
    r->home();
    reboot(r);
    CHECK(r->ws().dyna.speed == dyna::Speed::Fast);
}

void recursion(std::unique_ptr<testrig::Rig>& r) {
    r->type("8");
    CHECK(r->fw.apps().activeId() == AppId::Recursion);
    r->key(pk::F2);                                                         // EDIT an+1
    r->type("2"); r->key(pk::F2); r->type("+1");                            // 2an+1
    r->key(pk::Enter);
    CHECK(r->ws().modeVars.recFormula == "2rec_an+1");
    r->ws().modeVars.recInitial[0] = 1;
    r->ws().modeVars.recEnd = 5;
    r->key(pk::F6);                                                         // TABLE
    r->settle();
    CHECK(screen<RecursionApp>(*r) == "table" && !r->error());
    r->key(pk::F5);                                                         // G-PLT
    CHECK(screen<RecursionApp>(*r) == "graph");
    r->key(pk::Esc);
    r->key(pk::F6);                                                         // WEB
    CHECK(screen<RecursionApp>(*r) == "web");
    r->key(pk::Esc); r->key(pk::Esc);
    // an+2: Fibonacci
    r->key(pk::F1);
    CHECK(r->ws().recursion.type == rec::Type::An2);
    r->key(pk::F2); r->key(pk::F3); r->type("+"); r->key(pk::F2); r->key(pk::Enter);
    CHECK(r->ws().recursion.an2 == "rec_an1+rec_an");
    r->key(pk::F3); r->key(pk::F2); r->enter("FIBO");
    CHECK(fileExists("PICOCALC/RECUR/FIBO.pcrecur"));
    // Il file porta anche Start, End, a0 e a1: OPEN li rimette
    r->ws().modeVars.recEnd = 9; r->ws().modeVars.recInitial[0] = 0; r->ws().recursion.type = rec::Type::An1;
    r->key(pk::F3); r->key(pk::F1); r->key(pk::Enter);
    CHECK(!r->error() && r->ws().modeVars.recEnd == 5 && r->ws().modeVars.recInitial[0] == 1 &&
          r->ws().recursion.type == rec::Type::An2 && r->ws().modeVars.recFormula == "2rec_an+1");
    r->home();
    reboot(r);
    CHECK(r->ws().recursion.type == rec::Type::An2 && r->ws().modeVars.recFormula == "2rec_an+1");
}

void conics(std::unique_ptr<testrig::Rig>& r) {
    r->type("9");
    CHECK(r->fw.apps().activeId() == AppId::ConicGraphs && screen<ConicGraphsApp>(*r) == "list");
    r->key(pk::Down); r->key(pk::Up);                                       // Circle (gia' selezionato)
    r->key(pk::Enter);
    CHECK(screen<ConicGraphsApp>(*r) == "coef" && r->ws().conic.model == conic::Model::Circle);
    r->key(pk::Down); r->key(pk::Down); r->enter("3");                      // R = 3
    r->key(pk::F6);                                                         // DRAW
    r->settle();
    CHECK(screen<ConicGraphsApp>(*r) == "graph" && r->ws().conic.coef[4] == 3);
    r->key(pk::F5);                                                         // G-SOLVE
    r->key(pk::Down); r->key(pk::Down); r->key(pk::Down); r->key(pk::Enter);   // RADIUS
    CHECK(!r->error());
    r->key(pk::F1);                                                         // TRACE
    CHECK(screen<ConicGraphsApp>(*r) == "trace");
    r->key(pk::Right);
    r->key(pk::Esc); r->key(pk::Esc); r->key(pk::Esc);
    r->key(pk::F5); r->key(pk::F2); r->enter("CERCHIO");
    CHECK(fileExists("PICOCALC/CONIC/CERCHIO.pcconic"));
    r->home();
    reboot(r);
    CHECK(r->ws().conic.model == conic::Model::Circle && r->ws().conic.coef[4] == 3);
}

void financial(std::unique_ptr<testrig::Rig>& r) {
    r->type("c");
    CHECK(r->fw.apps().activeId() == AppId::Financial && screen<FinancialApp>(*r) == "menu");
    r->key(pk::Down); r->key(pk::Enter);                                    // Compound Interest
    CHECK(screen<FinancialApp>(*r) == "compound");
    r->enter("12"); r->enter("5"); r->enter("-1000"); r->enter("0");       // EXE passa al campo seguente
    r->key(pk::Down); r->enter("1"); r->enter("1");                         // P/Y, C/Y
    r->key(pk::F5);                                                         // FV
    CHECK(!r->error() && std::abs(r->ws().modeVars.tvmFV - 1795.856326) < 1e-5);
    r->key(pk::F6);                                                         // -> Ans
    CHECK(std::abs(r->ws().memory.answer() - 1795.856326) < 1e-5);
    r->key(pk::F10);                                                        // SET UP: Payment BGN
    CHECK(r->ws().financial.begin);
    r->key(pk::Esc);
    r->key(pk::Down); r->key(pk::Enter);                                    // Cash Flow
    r->key(pk::F1);
    CHECK(!r->error());
    r->key(pk::Esc);
    r->key(pk::F5); r->key(pk::F2); r->enter("MUTUO");
    CHECK(fileExists("PICOCALC/FIN/MUTUO.pcfin"));
    r->home();
    reboot(r);
    CHECK(r->ws().financial.begin && std::abs(r->ws().modeVars.tvmFV - 1795.856326) < 1e-5);
}

void program(std::unique_ptr<testrig::Rig>& r) {
    r->type("b");
    CHECK(r->fw.apps().activeId() == AppId::Program && screen<ProgramApp>(*r) == "list");
    r->key(pk::F3); r->enter("HELLO");                                      // NEW
    CHECK(screen<ProgramApp>(*r) == "edit" && fileExists("PICOCALC/PROG/HELLO.pcprog"));
    r->key(pk::F1);                                                         // RUN
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "done");
    r->key(pk::Esc);
    // input da tastiera
    auto put = [&](const char* name, const std::string& text) {
        files::TypedStore ts(board::host::ramCard());
        CHECK(ts.save(files::FileType::Program, name, std::vector<std::uint8_t>(text.begin(), text.end())) == files::Status::Ok);
    };
    put("ASK.pcprog", "// pcprog: C++\n#include <iostream>\nint main() { int n; std::cin >> n; std::cout << n * 2; fx::setVar(\"B\", n); return 0; }\n");
    put("BAD.pcprog", "// pcprog: C\nint main() {\n    return y;\n}\n");
    put("LOOP.cpp", "int main() { while (true) { } }\n");
    r->home(); r->type("b");
    r->key(pk::Home);
    r->key(pk::Enter);                                                      // ASK (primo in ordine)
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "input");
    r->enter("21");
    r->settle();
    CHECK(screen<ProgramApp>(*r) == "done" && r->ws().memory.recall('B') == 21);
    r->key(pk::Esc);
    r->key(pk::Home); r->key(pk::Down); r->key(pk::Enter);                  // BAD: errore con riga e cursore
    CHECK(r->error() && r->errorDetail().find("riga 2") != std::string::npos);   // la riga del dialetto non conta
    r->closeError();
    CHECK(screen<ProgramApp>(*r) == "edit");
    r->key(pk::Esc);
    r->key(pk::End); r->key(pk::Enter);                                     // LOOP: AC = Break
    ticks(*r, 10);
    CHECK(screen<ProgramApp>(*r) == "run");
    r->key(pk::Break);
    CHECK(r->error() && r->errorTitle() == "Break");
    r->closeError();
    r->home();
    reboot(r);
    CHECK(r->ws().program.lastFile == "LOOP.cpp");
}

void linkMode(std::unique_ptr<testrig::Rig>& r) {
    link::LoopPort dev, pc;
    link::connect(dev, pc);
    board::host::setSerialPort(&dev);
    std::uint32_t now = 0;
    r->type("e");
    CHECK(r->fw.apps().activeId() == AppId::Link && screen<LinkApp>(*r) == "main");
    // SEND > ALL: il PC riceve il workspace
    r->key(pk::F1); r->key(pk::F1);
    CHECK(screen<LinkApp>(*r) == "transfer");
    link::Receiver rx(pc, [&now] { return now; });
    for (int k = 0; k < 400 && rx.status() == link::Status::Running; ++k) { ticks(*r, 1, 20); now += 20; step::Budget b; (void)rx.step(b); }
    ticks(*r, 3);
    std::vector<link::Entry> got;
    CHECK(rx.status() == link::Status::Done && link::decodePackage(rx.package().data(), rx.package().size(), got) && got.size() == 1);
    Workspace copy;
    CHECK(got[0].kind == link::Entry::Kind::Workspace && deserializeWorkspace(got[0].data.data(), got[0].data.size(), copy));
    CHECK(r->ws().link.sent == 1 && screen<LinkApp>(*r) == "main");
    // RECEIVE: il PC invia uno script; il pacchetto resta in PICOCALC/LINK, APPLY scrive il file
    r->key(pk::F2);
    std::vector<link::Entry> es(1);
    es[0].path = "PICOCALC/PY/HI.py";
    const std::string script = "print('hi')\n";
    es[0].data.assign(script.begin(), script.end());
    link::Sender tx(pc, [&now] { return now; }, link::encodePackage(es), "DAL PC");
    for (int k = 0; k < 400 && tx.status() == link::Status::Running; ++k) { ticks(*r, 1, 20); now += 20; step::Budget b; (void)tx.step(b); }
    ticks(*r, 3);
    CHECK(tx.status() == link::Status::Done && screen<LinkApp>(*r) == "contents" && fileExists("PICOCALC/LINK/DALPC.pclink"));
    r->key(pk::F1);                                                         // APPLY
    CHECK(fileExists("PICOCALC/PY/HI.py") && r->ws().link.received == 1);
    // AC durante l'attesa
    r->key(pk::F2);
    r->key(pk::Break);
    CHECK(r->error());
    r->closeError();
    CHECK(screen<LinkApp>(*r) == "main");
    r->home();
    board::host::setSerialPort(nullptr);
    r->type("e");
    r->key(pk::F2);
    CHECK(r->error() && r->errorDetail().find("USB seriale") != std::string::npos);
    r->closeError();
    r->home();
    reboot(r);
    CHECK(r->ws().link.sent == 1 && r->ws().link.received == 1);
}

void geometry(std::unique_ptr<testrig::Rig>& r) {
    r->type("h");
    CHECK(r->fw.apps().activeId() == AppId::Geometry && screen<GeometryApp>(*r) == "main");
    r->key(pk::F1); r->key(pk::F2);                                         // DRAW > SEGMENT
    CHECK(screen<GeometryApp>(*r) == "draw");
    r->key(pk::Enter);                                                      // (0, 0)
    for (int i = 0; i < 12; ++i) r->key(pk::Right);                         // passo 0.25 alla scala iniziale
    for (int i = 0; i < 16; ++i) r->key(pk::Up);
    r->key(pk::Enter);                                                      // (3, 4)
    CHECK(r->ws().geometry.objects.size() == 1 && geo::measure(r->ws().geometry.objects[0])[0] == "Lunghezza=5");
    r->key(pk::Esc);
    r->key(pk::F2);                                                         // SELECT vicino a (3, 4)
    r->key(pk::F4);                                                         // MEASURE
    CHECK(screen<GeometryApp>(*r) == "measure");
    r->key(pk::Esc);
    r->key(pk::F3);                                                         // MOVE
    r->key(pk::Right);
    r->key(pk::Enter);
    CHECK(r->ws().geometry.objects[0].xy[0] == 0.25);
    r->key(pk::F5); r->key(pk::F1);                                         // ZOOM+
    CHECK(r->ws().geometry.view.scale == 36);
    r->key(pk::F6); r->key(pk::F3); r->enter("SEGM");
    CHECK(fileExists("PICOCALC/GEOM/SEGM.pcgeo"));
    r->key(pk::F2); r->key(pk::Del);                                        // cancella l'oggetto selezionato
    CHECK(r->ws().geometry.objects.empty());
    r->key(pk::F6); r->key(pk::F2); r->key(pk::Enter);                       // OPEN
    CHECK(r->ws().geometry.objects.size() == 1);
    r->home();
    reboot(r);
    CHECK(r->ws().geometry.objects.size() == 1 && r->ws().geometry.file == "SEGM.pcgeo");
}

void picturePlot(std::unique_ptr<testrig::Rig>& r) {
    // un'immagine di sfondo in PICOCALC/PICT
    {
        std::vector<std::uint16_t> px(320 * 100, 0xFFFF);
        for (int x = 0; x < 320; ++x) px[50 * 320 + x] = 0x001F;
        pict::Image img;
        CHECK(pict::encode(px.data(), 320, 320, 100, img));
        const auto bytes = pict::serialize(img);
        (void)board::host::ramCard().makeDirs(files::PictFolder);
        CHECK(files::writeAtomic(board::host::ramCard(), "PICOCALC/PICT/PICT01.pcpict", bytes.data(), bytes.size()) == files::Status::Ok);
    }
    r->type("i");
    CHECK(r->fw.apps().activeId() == AppId::PicturePlot && screen<PicturePlotApp>(*r) == "plot");
    r->key(pk::F1); r->key(pk::F4);                                         // FILE > PICT
    CHECK(screen<PicturePlotApp>(*r) == "pict");
    r->key(pk::Enter);
    CHECK(r->ws().pictplot.background == "PICT01.pcpict" && !r->error());
    for (int k = 0; k < 3; ++k) {
        for (int i = 0; i < 10; ++i) r->key(pk::Right);
        for (int i = 0; i < 5; ++i) r->key(pk::Up);
        r->key(pk::F2);                                                     // PLOT
    }
    CHECK(r->ws().pictplot.points.size() == 3);
    r->key(pk::F4);                                                         // LIST
    CHECK(r->ws().lists[0].size() == 3 && r->ws().lists[1].size() == 3);
    r->key(pk::F5); r->key(pk::Enter);                                      // REG > ax+b
    CHECK(r->ws().pictplot.regression == 0 && !r->error());
    r->key(pk::F3);                                                         // UNDO
    CHECK(r->ws().pictplot.points.size() == 2);
    r->key(pk::F1); r->key(pk::F3); r->enter("MELA");
    CHECK(fileExists("PICOCALC/PPLOT/MELA.pcplot"));
    r->home();
    reboot(r);
    CHECK(r->ws().pictplot.points.size() == 2 && r->ws().pictplot.background == "PICT01.pcpict");
}

void python(std::unique_ptr<testrig::Rig>& r) {
    r->type("l");
    CHECK(r->fw.apps().activeId() == AppId::Python && screen<PythonApp>(*r) == "list");
    r->key(pk::F5);                                                         // SHELL
    CHECK(screen<PythonApp>(*r) == "shell");
    r->enter("6*7");
    r->settle();
    r->enter("for i in range(2):");
    r->enter("  print(i)");
    r->enter("");
    r->settle();
    r->enter("while True:");
    r->enter("  pass");
    r->enter("");
    ticks(*r, 5);
    CHECK(screen<PythonApp>(*r) == "run");
    r->key(pk::Break);
    r->settle();
    CHECK(screen<PythonApp>(*r) == "shell" && r->ws().python.history.size() >= 3);
    r->key(pk::Esc);                                                        // all'elenco
    r->key(pk::F3); r->enter("DEMO");                                       // NEW
    CHECK(screen<PythonApp>(*r) == "edit" && fileExists("PICOCALC/PY/DEMO.py"));
    r->key(pk::F1);                                                         // RUN
    r->settle();
    CHECK(screen<PythonApp>(*r) == "shell" && !r->error());
    r->home();
    CHECK(!py::Session::active());                                         // heap liberato uscendo
    reboot(r);
    CHECK(r->ws().python.lastScript == "DEMO.py");
}
} // namespace

int main() {
    board::init();
    storage::RecordStore(board::storageFlash()).eraseAll();
    auto r = std::make_unique<testrig::Rig>();
    spreadsheet(r);
    dynaGraph(r);
    recursion(r);
    conics(r);
    financial(r);
    program(r);
    linkMode(r);
    geometry(r);
    picturePlot(r);
    python(r);
    CHECK(r->fw.apps().activeId() == AppId::MainMenu);
    return testutil::finish("base_flow_test");
}
