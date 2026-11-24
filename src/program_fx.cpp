// Libreria fx (v1.20.0): gli stessi servizi della calcolatrice in tutti i
// linguaggi (docs/reports/ANALISI_FONTI_v1.20.0.md §4.5): C fx_…, C++ fx::…,
// C# Fx.…, Java Fx.… (v1.21.0). Espressioni con le variabili del programma
// legate, A-Z, liste, matrici, comandi grafici e testo della fx-CG50.
// WorkspaceHost collega il workspace: graph_commands, parser unico, memorie.
#include "program_lib.hpp"
#include "binding.hpp"
#include "exact.hpp"
#include "expression.hpp"
#include "graph_commands.hpp"
#include "workspace.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace fx::prog {
using namespace lib;

namespace {
std::string num(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.15g", v);
    return buf;
}
// Errore di un servizio della calcolatrice: fatale in C/C++, FxException in C#/Java.
[[noreturn]] void serviceError(Vm& vm, const std::string& msg) {
    const Dialect d = vm.lang();
    if (d == Dialect::CSharp || d == Dialect::Java) vm.raise("FxException", msg);
    vm.fatal(msg.find("ERROR") != std::string::npos ? msg : "Math ERROR: " + msg);
}
template <class F> auto guard(Vm& vm, F&& f) -> decltype(f()) {
    try {
        return f();
    } catch (ProgramThrow&) {
        throw;
    } catch (ProgramAbort&) {
        throw;
    } catch (ProgramExit&) {
        throw;
    } catch (Error&) {
        throw;
    } catch (const std::bad_alloc&) {
        throw;
    } catch (const std::exception& e) {
        serviceError(vm, e.what());
    }
}
char letterOf(Vm& vm, const Value& v) {
    char c = 0;
    if (v.k == VK::C8 || v.k == VK::C16 || v.k == VK::I32) c = static_cast<char>(v.i);
    else if (v.holdsObj()) { const std::string s = vm.utf8(v); if (s.size() == 1) c = s[0]; }
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (c < 'A' || c > 'Z') serviceError(vm, "Argument ERROR: attesa una lettera da A a Z");
    return c;
}
std::string textArg(Vm& vm, const NativeCall& c, int i) {
    if (c.args[i].isNull()) serviceError(vm, "Argument ERROR: attesa una stringa");
    return S(vm, c, i);
}
// Variabili numeriche visibili nella funzione che chiama (e i globali).
std::vector<std::pair<std::string, double>> visible(Vm& vm) {
    std::vector<std::pair<std::string, double>> out;
    auto add = [&](const std::string& n, const Value& v) {
        if (n.empty() || n[0] == '#' || !isNum(v.k)) return;
        for (const auto& e : out) if (e.first == n) return;
        out.emplace_back(n, v.num());
    };
    if (!vm.frames.empty()) {
        const Frame& fr = vm.frames.back();
        const std::int32_t pc = fr.pc > 0 ? fr.pc - 1 : 0;
        for (std::size_t i = fr.fn->locals.size(); i-- > 0;) {
            const LocalInfo& l = fr.fn->locals[i];
            if (pc < l.from || pc >= l.to) continue;
            const std::size_t at = fr.base + static_cast<std::size_t>(l.slot);
            if (at >= vm.stack.size()) continue;
            Value v = vm.stack[at];
            if (l.boxed && v.holdsObj() && v.o->kind == OK::Box) v = static_cast<BoxObj*>(v.o)->v;
            add(l.name, v);
        }
    }
    const Program& p = vm.program();
    if (vm.globals) for (std::size_t i = 0; i < p.globalNames.size() && i < vm.globals->count; ++i) add(p.globalNames[i], vm.globals->get(static_cast<std::uint32_t>(i)));
    return out;
}
Value real(double d) { return Value::f64(d); }

// ---------------------------------------------------------------- espressioni e variabili
void fxCalc(Vm& vm, NativeCall& c) {
    const std::string e = textArg(vm, c, 0);
    const auto loc = visible(vm);
    c.ret = real(guard(vm, [&] { return vm.host().calc(e, loc); }));
}
void fxCalcExact(Vm& vm, NativeCall& c) {
    const std::string e = textArg(vm, c, 0);
    const auto loc = visible(vm);
    c.ret = str(vm, guard(vm, [&] { return vm.host().calcExact(e, loc); }));
}
void fxGetVar(Vm& vm, NativeCall& c) { const char l = letterOf(vm, c.args[0]); c.ret = real(guard(vm, [&] { return vm.host().getVariable(l); })); }
void fxSetVar(Vm& vm, NativeCall& c) { const char l = letterOf(vm, c.args[0]); const double v = D(c, 1); guard(vm, [&] { vm.host().setVariable(l, v); return 0; }); }
// ---------------------------------------------------------------- liste
void fxGetList(Vm& vm, NativeCall& c) {
    const int l = static_cast<int>(I(c, 0)), i = static_cast<int>(I(c, 1));
    c.ret = real(guard(vm, [&] { return vm.host().getListItem(l, i); }));
}
void fxSetList(Vm& vm, NativeCall& c) {
    const int l = static_cast<int>(I(c, 0)), i = static_cast<int>(I(c, 1));
    const double v = D(c, 2);
    guard(vm, [&] { vm.host().setListItem(l, i, v); return 0; });
}
void fxListSize(Vm& vm, NativeCall& c) { const int l = static_cast<int>(I(c, 0)); c.ret = Value::i32(guard(vm, [&] { return vm.host().listSize(l); })); }
std::vector<double> listValues(Vm& vm, int l) { return guard(vm, [&] { return vm.host().getList(l); }); }
void fxGetListVec(Vm& vm, NativeCall& c) {
    const std::vector<double> v = listValues(vm, static_cast<int>(I(c, 0)));
    VecObj* out = vm.newVec(c.type && c.type->k == TK::Native ? c.type : nullptr);
    vm.root(out);
    for (double d : v) out->items.push_back(Value::f64(d));
    c.ret = Value::ref(out);
}
void fxGetListArr(Vm& vm, NativeCall& c) {
    const std::vector<double> v = listValues(vm, static_cast<int>(I(c, 0)));
    ArrayObj* a = vm.newArray(vm.program().prim[static_cast<int>(TK::F64)], static_cast<std::uint32_t>(v.size()));
    a->type = c.type;
    for (std::size_t i = 0; i < v.size(); ++i) a->set(static_cast<std::uint32_t>(i), Value::f64(v[i]));
    c.ret = Value::ref(a);
}
std::vector<double> numbersOf(Vm& vm, const Value& v) {
    std::vector<double> out;
    Obj* o = objArg(vm, v);
    if (o->kind == OK::Vec) for (const Value& x : static_cast<VecObj*>(o)->items) out.push_back(x.num());
    else if (o->kind == OK::Array) { ArrayObj* a = static_cast<ArrayObj*>(o); for (std::uint32_t i = 0; i < a->count; ++i) out.push_back(a->get(i).num()); }
    else serviceError(vm, "Argument ERROR: attesa una lista di numeri");
    return out;
}
void fxSetListAll(Vm& vm, NativeCall& c) {
    const int l = static_cast<int>(I(c, 0));
    const std::vector<double> v = numbersOf(vm, c.args[1]);
    guard(vm, [&] { vm.host().setList(l, v); return 0; });
}
void fxGetListBuf(Vm& vm, NativeCall& c) {
    // C: fx_getlist_all(lista, buf, n) -> elementi copiati
    const std::vector<double> v = listValues(vm, static_cast<int>(I(c, 0)));
    const std::int64_t n = I(c, 2);
    Value p = c.args[1];
    if (p.k == VK::Ref) p = Value::ptr(p.o, 0);
    if (p.k != VK::Ptr || p.f == 2 || !p.o || p.o->kind != OK::Array) serviceError(vm, "Argument ERROR: serve un array di double");
    ArrayObj* a = static_cast<ArrayObj*>(p.o);
    const std::uint32_t at = p.idx == Whole ? 0 : p.idx;
    std::int64_t k = 0;
    for (; k < n && static_cast<std::size_t>(k) < v.size(); ++k) {
        if (at + static_cast<std::uint32_t>(k) >= a->count) vm.fault(FaultKind::Index, "fx_getlist_all: oltre la fine dell'array");
        a->set(at + static_cast<std::uint32_t>(k), Value::f64(v[static_cast<std::size_t>(k)]));
    }
    c.ret = Value::i32(k);
}
// ---------------------------------------------------------------- matrici
struct Mat { std::vector<double> cells; int rows{0}, cols{0}; };
Mat matOf(Vm& vm, const Value& name) {
    const char l = letterOf(vm, name);
    Mat m;
    const bool ok = guard(vm, [&] { return vm.host().getMatrix(l, m.cells, m.rows, m.cols); });
    if (!ok) serviceError(vm, std::string("Memory ERROR: Mat ") + l + " non definita");
    return m;
}
void fxGetMat(Vm& vm, NativeCall& c) {
    const Mat m = matOf(vm, c.args[0]);
    const std::int64_t r = I(c, 1), k = I(c, 2);
    if (r < 1 || r > m.rows || k < 1 || k > m.cols) serviceError(vm, "Dimension ERROR: elemento della matrice");
    c.ret = real(m.cells[static_cast<std::size_t>((r - 1) * m.cols + (k - 1))]);
}
void fxSetMat(Vm& vm, NativeCall& c) {
    const char l = letterOf(vm, c.args[0]);
    Mat m = matOf(vm, c.args[0]);
    const std::int64_t r = I(c, 1), k = I(c, 2);
    if (r < 1 || r > m.rows || k < 1 || k > m.cols) serviceError(vm, "Dimension ERROR: elemento della matrice");
    m.cells[static_cast<std::size_t>((r - 1) * m.cols + (k - 1))] = D(c, 3);
    guard(vm, [&] { vm.host().setMatrix(l, m.cells, m.rows, m.cols); return 0; });
}
void fxMatRows(Vm& vm, NativeCall& c) { c.ret = Value::i32(matOf(vm, c.args[0]).rows); }
void fxMatCols(Vm& vm, NativeCall& c) { c.ret = Value::i32(matOf(vm, c.args[0]).cols); }
void fxGetMatrixVec(Vm& vm, NativeCall& c) {
    const Mat m = matOf(vm, c.args[0]);
    TypeP outer = c.type && c.type->k == TK::Native ? c.type : nullptr;
    TypeP inner = outer && !outer->args.empty() ? outer->args[0] : nullptr;
    VecObj* out = vm.newVec(outer);
    vm.root(out);
    for (int r = 0; r < m.rows; ++r) {
        VecObj* row = vm.newVec(inner);
        out->items.push_back(Value::ref(row));
        for (int k = 0; k < m.cols; ++k) row->items.push_back(Value::f64(m.cells[static_cast<std::size_t>(r * m.cols + k)]));
    }
    c.ret = Value::ref(out);
}
void fxGetMatrixArr(Vm& vm, NativeCall& c) {
    const Mat m = matOf(vm, c.args[0]);
    ArrayObj* a = vm.newArray(vm.program().prim[static_cast<int>(TK::F64)], static_cast<std::uint32_t>(m.rows * m.cols));
    a->type = c.type;
    a->rank = 2;
    a->rows = static_cast<std::uint32_t>(m.rows);
    a->cols = static_cast<std::uint32_t>(m.cols);
    for (std::size_t i = 0; i < m.cells.size(); ++i) a->set(static_cast<std::uint32_t>(i), Value::f64(m.cells[i]));
    c.ret = Value::ref(a);
}
void fxSetMatrix(Vm& vm, NativeCall& c) {
    const char l = letterOf(vm, c.args[0]);
    Mat m;
    Obj* o = objArg(vm, c.args[1]);
    if (o->kind == OK::Array && static_cast<ArrayObj*>(o)->rank == 2) {
        ArrayObj* a = static_cast<ArrayObj*>(o);
        m.rows = static_cast<int>(a->rows);
        m.cols = static_cast<int>(a->cols);
        for (std::uint32_t i = 0; i < a->count; ++i) m.cells.push_back(a->get(i).num());
    } else if (o->kind == OK::Vec || o->kind == OK::Array) {
        // vector<vector<double>> / double[][]
        std::vector<Value> rows;
        if (o->kind == OK::Vec) rows.assign(static_cast<VecObj*>(o)->items.begin(), static_cast<VecObj*>(o)->items.end());
        else { ArrayObj* a = static_cast<ArrayObj*>(o); for (std::uint32_t i = 0; i < a->count; ++i) rows.push_back(a->get(i)); }
        m.rows = static_cast<int>(rows.size());
        for (const Value& r : rows) {
            const std::vector<double> v = numbersOf(vm, r);
            if (m.cols == 0) m.cols = static_cast<int>(v.size());
            if (static_cast<int>(v.size()) != m.cols) serviceError(vm, "Dimension ERROR: righe di lunghezza diversa");
            m.cells.insert(m.cells.end(), v.begin(), v.end());
        }
    } else serviceError(vm, "Argument ERROR: attesa una matrice");
    if (m.rows <= 0 || m.cols <= 0) serviceError(vm, "Dimension ERROR: matrice vuota");
    guard(vm, [&] { vm.host().setMatrix(l, m.cells, m.rows, m.cols); return 0; });
}
// ---------------------------------------------------------------- grafici e testo
void cmd(Vm& vm, const std::string& s) { guard(vm, [&] { vm.host().graphics(s); return 0; }); }
void fxCls(Vm& vm, NativeCall&) { cmd(vm, "Cls"); }
void fxViewWindow(Vm& vm, NativeCall& c) { cmd(vm, "ViewWindow " + num(D(c, 0)) + "," + num(D(c, 1)) + "," + num(D(c, 2)) + "," + num(D(c, 3)) + "," + num(D(c, 4)) + "," + num(D(c, 5))); }
void fxPlot(Vm& vm, NativeCall& c) { cmd(vm, "Plot " + num(D(c, 0)) + "," + num(D(c, 1))); }
void fxLine(Vm& vm, NativeCall& c) { cmd(vm, "F-Line " + num(D(c, 0)) + "," + num(D(c, 1)) + "," + num(D(c, 2)) + "," + num(D(c, 3))); }
void fxCircle(Vm& vm, NativeCall& c) { cmd(vm, "Circle " + num(D(c, 0)) + "," + num(D(c, 1)) + "," + num(D(c, 2))); }
void fxText(Vm& vm, NativeCall& c) {
    const std::string s = textArg(vm, c, 2);
    if (s.find('"') != std::string::npos) serviceError(vm, "Argument ERROR: il testo non puo' contenere virgolette");
    cmd(vm, "Text " + num(D(c, 0)) + "," + num(D(c, 1)) + ",\"" + s + "\"");
}
void fxGraphY(Vm& vm, NativeCall& c) { cmd(vm, "Graph Y=" + textArg(vm, c, 0)); }
void fxVertical(Vm& vm, NativeCall& c) { cmd(vm, "Vertical " + num(D(c, 0))); }
void fxHorizontal(Vm& vm, NativeCall& c) { cmd(vm, "Horizontal " + num(D(c, 0))); }
void fxPixelOn(Vm& vm, NativeCall& c) { cmd(vm, "PxlOn " + num(D(c, 0)) + "," + num(D(c, 1))); }
void fxPixelOff(Vm& vm, NativeCall& c) { cmd(vm, "PxlOff " + num(D(c, 0)) + "," + num(D(c, 1))); }
void fxPixelTest(Vm& vm, NativeCall& c) { const int r = static_cast<int>(I(c, 0)), k = static_cast<int>(I(c, 1)); c.ret = Value::i32(guard(vm, [&] { return vm.host().pixelTest(r, k); })); }
void fxCommand(Vm& vm, NativeCall& c) { cmd(vm, textArg(vm, c, 0)); }
void fxLocate(Vm& vm, NativeCall& c) {
    const int col = static_cast<int>(I(c, 0)), row = static_cast<int>(I(c, 1));
    const Value& v = c.args[2];
    const std::string t = v.holdsObj() ? S(vm, c, 2) : vm.format(v, vm.lang() == Dialect::C ? Dialect::Cpp : vm.lang());
    guard(vm, [&] { vm.host().locate(col, row, t); return 0; });
}
void fxClrText(Vm& vm, NativeCall&) { vm.host().clearText(); }
void fxGetKey(Vm& vm, NativeCall& c) { c.ret = Value::i32(vm.host().getKey()); }
void fxWaitKey(Vm& vm, NativeCall& c) {
    for (;;) {
        const int k = vm.host().getKey();
        if (k) { c.ret = Value::i32(k); return; }
        vm.yieldNow();
        if (vm.abortRequested) throw ProgramAbort{};
    }
}

constexpr std::uint8_t St = NF::Static;
#define FXC(name, sig, fn) {L::C, "", "", "fx_" name, sig, fn, St}
#define FXP(name, sig, fn) {L::Cpp, "", "", "fx::" name, sig, fn, St}
#define FXS(name, sig, fn) {L::Cs, "Fx", "Fx", name, sig, fn, St}
#define FXJ(name, sig, fn) {L::Java, "Fx", "Fx", name, sig, fn, St}
const NativeEntry kFx[] = {
    // C
    FXC("calc", "double(cstr)", fxCalc), FXC("calc_exact", "charptr(cstr)", fxCalcExact),
    FXC("getvar", "double(int)", fxGetVar), FXC("getvar", "double(cstr)", fxGetVar), FXC("setvar", "void(int,double)", fxSetVar), FXC("setvar", "void(cstr,double)", fxSetVar),
    FXC("getlist", "double(int,int)", fxGetList), FXC("setlist", "void(int,int,double)", fxSetList), FXC("listsize", "int(int)", fxListSize),
    FXC("getlist_all", "int(int,any,int)", fxGetListBuf),
    FXC("getmat", "double(int,int,int)", fxGetMat), FXC("setmat", "void(int,int,int,double)", fxSetMat),
    FXC("matrows", "int(int)", fxMatRows), FXC("matcols", "int(int)", fxMatCols),
    FXC("cls", "void()", fxCls), FXC("viewwindow", "void(double,double,double,double,double,double)", fxViewWindow),
    FXC("plot", "void(double,double)", fxPlot), FXC("line", "void(double,double,double,double)", fxLine), FXC("circle", "void(double,double,double)", fxCircle),
    FXC("text", "void(double,double,cstr)", fxText), FXC("graphy", "void(cstr)", fxGraphY), FXC("vertical", "void(double)", fxVertical),
    FXC("horizontal", "void(double)", fxHorizontal), FXC("pxlon", "void(int,int)", fxPixelOn), FXC("pxloff", "void(int,int)", fxPixelOff),
    FXC("pxltest", "int(int,int)", fxPixelTest), FXC("command", "void(cstr)", fxCommand),
    FXC("locate", "void(int,int,any)", fxLocate), FXC("clrtext", "void()", fxClrText), FXC("getkey", "int()", fxGetKey), FXC("waitkey", "int()", fxWaitKey),
    // C++
    FXP("calc", "double(string)", fxCalc), FXP("calcExact", "string(string)", fxCalcExact),
    FXP("getVar", "double(char)", fxGetVar), FXP("getVar", "double(string)", fxGetVar), FXP("setVar", "void(char,double)", fxSetVar), FXP("setVar", "void(string,double)", fxSetVar),
    FXP("getList", "double(int,int)", fxGetList), FXP("getList", "vector<double>(int)", fxGetListVec), FXP("setList", "void(int,int,double)", fxSetList),
    FXP("setList", "void(int,any)", fxSetListAll), FXP("listSize", "int(int)", fxListSize),
    FXP("getMat", "double(char,int,int)", fxGetMat), FXP("setMat", "void(char,int,int,double)", fxSetMat),
    FXP("matRows", "int(char)", fxMatRows), FXP("matCols", "int(char)", fxMatCols),
    FXP("getMatrix", "vector<vector<double>>(char)", fxGetMatrixVec), FXP("setMatrix", "void(char,any)", fxSetMatrix),
    FXP("cls", "void()", fxCls), FXP("viewWindow", "void(double,double,double,double,double,double)", fxViewWindow),
    FXP("plot", "void(double,double)", fxPlot), FXP("line", "void(double,double,double,double)", fxLine), FXP("circle", "void(double,double,double)", fxCircle),
    FXP("text", "void(double,double,string)", fxText), FXP("graphY", "void(string)", fxGraphY), FXP("vertical", "void(double)", fxVertical),
    FXP("horizontal", "void(double)", fxHorizontal), FXP("pxlOn", "void(int,int)", fxPixelOn), FXP("pxlOff", "void(int,int)", fxPixelOff),
    FXP("pxlTest", "int(int,int)", fxPixelTest), FXP("command", "void(string)", fxCommand),
    FXP("locate", "void(int,int,any)", fxLocate), FXP("clrText", "void()", fxClrText), FXP("getKey", "int()", fxGetKey), FXP("waitKey", "int()", fxWaitKey),
    // C#
    FXS("Calc", "double(string)", fxCalc), FXS("CalcExact", "string(string)", fxCalcExact),
    FXS("GetVar", "double(char)", fxGetVar), FXS("GetVar", "double(string)", fxGetVar), FXS("SetVar", "void(char,double)", fxSetVar), FXS("SetVar", "void(string,double)", fxSetVar),
    FXS("GetList", "double(int,int)", fxGetList), FXS("GetList", "array<double>(int)", fxGetListArr), FXS("SetList", "void(int,int,double)", fxSetList),
    FXS("SetList", "void(int,any)", fxSetListAll), FXS("ListSize", "int(int)", fxListSize),
    FXS("GetMat", "double(char,int,int)", fxGetMat), FXS("SetMat", "void(char,int,int,double)", fxSetMat),
    FXS("MatRows", "int(char)", fxMatRows), FXS("MatCols", "int(char)", fxMatCols),
    FXS("GetMatrix", "array2<double>(char)", fxGetMatrixArr), FXS("SetMatrix", "void(char,any)", fxSetMatrix),
    FXS("Cls", "void()", fxCls), FXS("ViewWindow", "void(double,double,double,double,double,double)", fxViewWindow),
    FXS("Plot", "void(double,double)", fxPlot), FXS("Line", "void(double,double,double,double)", fxLine), FXS("Circle", "void(double,double,double)", fxCircle),
    FXS("Text", "void(double,double,string)", fxText), FXS("GraphY", "void(string)", fxGraphY), FXS("Vertical", "void(double)", fxVertical),
    FXS("Horizontal", "void(double)", fxHorizontal), FXS("PxlOn", "void(int,int)", fxPixelOn), FXS("PxlOff", "void(int,int)", fxPixelOff),
    FXS("PxlTest", "int(int,int)", fxPixelTest), FXS("Command", "void(string)", fxCommand),
    FXS("Locate", "void(int,int,any)", fxLocate), FXS("ClrText", "void()", fxClrText), FXS("GetKey", "int()", fxGetKey), FXS("WaitKey", "int()", fxWaitKey),
    // Java (v1.21.0)
    FXJ("calc", "double(string)", fxCalc), FXJ("calcExact", "string(string)", fxCalcExact),
    FXJ("getVar", "double(char)", fxGetVar), FXJ("getVar", "double(string)", fxGetVar), FXJ("setVar", "void(char,double)", fxSetVar), FXJ("setVar", "void(string,double)", fxSetVar),
    FXJ("getList", "double(int,int)", fxGetList), FXJ("getList", "array<double>(int)", fxGetListArr), FXJ("setList", "void(int,int,double)", fxSetList),
    FXJ("setList", "void(int,any)", fxSetListAll), FXJ("listSize", "int(int)", fxListSize),
    FXJ("getMat", "double(char,int,int)", fxGetMat), FXJ("setMat", "void(char,int,int,double)", fxSetMat),
    FXJ("matRows", "int(char)", fxMatRows), FXJ("matCols", "int(char)", fxMatCols), FXJ("setMatrix", "void(char,any)", fxSetMatrix),
    FXJ("cls", "void()", fxCls), FXJ("viewWindow", "void(double,double,double,double,double,double)", fxViewWindow),
    FXJ("plot", "void(double,double)", fxPlot), FXJ("line", "void(double,double,double,double)", fxLine), FXJ("circle", "void(double,double,double)", fxCircle),
    FXJ("text", "void(double,double,string)", fxText), FXJ("graphY", "void(string)", fxGraphY), FXJ("vertical", "void(double)", fxVertical),
    FXJ("horizontal", "void(double)", fxHorizontal), FXJ("command", "void(string)", fxCommand),
    FXJ("locate", "void(int,int,any)", fxLocate), FXJ("clrText", "void()", fxClrText), FXJ("getKey", "int()", fxGetKey), FXJ("waitKey", "int()", fxWaitKey),
};
#undef FXC
#undef FXP
#undef FXS
#undef FXJ
} // namespace

const NativeEntry* libFxEntries(std::size_t& n) { n = sizeof(kFx) / sizeof(kFx[0]); return kFx; }

// ================================================================ WorkspaceHost
bool WorkspaceHost::readLine(std::string& out) {
    if (input.empty()) return false;
    out = input.front();
    input.erase(input.begin());
    return true;
}
void WorkspaceHost::graphics(const std::string& command) {
    commands.push_back(command);
    graphcmd::execute(ws_, command);                            // lo stesso interprete dei comandi grafici di Run-Matrix
}
namespace {
ast::NodePtr parseBound(const std::string& expression, const std::vector<std::pair<std::string, double>>& locals, bind::BindingResolver& resolver, std::vector<std::string>& names) {
    for (const auto& l : locals) {
        names.push_back(l.first);
        std::string lower = l.first;
        for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        resolver.bind("var_" + lower, l.second);
    }
    ParseOptions opt;
    opt.boundNames = &names;
    return parseExpression(expression, opt);
}
std::size_t listIndex(int list) {
    if (list < 1 || list > 26) throw MathError("Argument ERROR: List 1...26");
    return static_cast<std::size_t>(list - 1);
}
} // namespace
double WorkspaceHost::calc(const std::string& expression, const std::vector<std::pair<std::string, double>>& locals) {
    std::vector<std::string> names;
    bind::BindingResolver resolver(&ws_, &ws_);
    const auto tree = parseBound(expression, locals, resolver, names);
    return evaluateReal(*tree, resolver.context(ws_.evalContext()));
}
std::string WorkspaceHost::calcExact(const std::string& expression, const std::vector<std::pair<std::string, double>>& locals) {
    std::vector<std::string> names;
    bind::BindingResolver resolver(&ws_, &ws_);
    const auto tree = parseBound(expression, locals, resolver, names);
    exact::Value v;
    if (evaluateExact(*tree, resolver.context(ws_.evalContext()), v)) return exact::toText(v, false);
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.10g", evaluateReal(*tree, resolver.context(ws_.evalContext())));
    return buf;
}
double WorkspaceHost::getVariable(char letter) { return ws_.memory.recall(letter); }
void WorkspaceHost::setVariable(char letter, double value) { ws_.memory.store(letter, value); }
double WorkspaceHost::getListItem(int list, int index) {
    const auto& l = ws_.lists[listIndex(list)];
    if (index < 1 || static_cast<std::size_t>(index) > l.size()) throw DimensionError("Dimension ERROR: elemento della lista");
    return l[static_cast<std::size_t>(index - 1)];
}
void WorkspaceHost::setListItem(int list, int index, double value) {
    const std::size_t li = listIndex(list);
    std::vector<double> l = ws_.lists[li];
    if (index < 1 || static_cast<std::size_t>(index) > l.size() + 1) throw DimensionError("Dimension ERROR: elemento della lista");
    if (static_cast<std::size_t>(index) == l.size() + 1) l.push_back(value); else l[static_cast<std::size_t>(index - 1)] = value;
    ws_.setList(li, std::move(l));
}
int WorkspaceHost::listSize(int list) { return static_cast<int>(ws_.lists[listIndex(list)].size()); }
std::vector<double> WorkspaceHost::getList(int list) { return ws_.lists[listIndex(list)]; }
void WorkspaceHost::setList(int list, const std::vector<double>& values) { ws_.setList(listIndex(list), values); }
bool WorkspaceHost::getMatrix(char name, std::vector<double>& cells, int& rows, int& cols) {
    rows = cols = 0;
    if (name < 'A' || name > 'Z') throw MathError("Argument ERROR: Mat A...Z");
    const CalcValue& m = ws_.matrices.mat[static_cast<std::size_t>(name - 'A')];
    if (m.empty()) return false;
    if (m.complex()) throw MathError("Non-Real ERROR: matrice con elementi complessi");
    cells = m.re;
    rows = m.rows;
    cols = m.cols;
    return true;
}
void WorkspaceHost::setMatrix(char name, const std::vector<double>& cells, int rows, int cols) {
    if (name < 'A' || name > 'Z') throw MathError("Argument ERROR: Mat A...Z");
    CalcValue v = CalcValue::matrix(static_cast<std::size_t>(rows), static_cast<std::size_t>(cols));
    v.re = cells;
    ws_.storeMatrix(static_cast<std::size_t>(name - 'A'), std::move(v));
}

} // namespace fx::prog
