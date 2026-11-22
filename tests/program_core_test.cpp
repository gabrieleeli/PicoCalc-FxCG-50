// v1.15.0 (F9, solo Pimoroni Pico Plus 2 W): nucleo dell'interprete dei
// programmi in C, C++ e C# (decisione della v1.15.0): sintassi dei tre
// dialetti, macchina iterativa, controllo di flusso, funzioni e ricorsione a
// profondita' limitata, array, stringhe, I/O astratto e input in attesa,
// comandi grafici tramite graph_commands, fx::calc con le variabili legate,
// budget di passi e AC (StepTask), errori con riga e colonna.
#include "program.hpp"
#include "test_util.hpp"
#include "workspace.hpp"
#include <memory>

using namespace fx;
using namespace fx::prog;

namespace {
struct Run {
    std::unique_ptr<Workspace> ws = std::make_unique<Workspace>();
    WorkspaceHost host{*ws};
    std::string error;
    int line{0}, column{0}, exit{0};
    Machine::State state{};
};
Run run(const std::string& src, Dialect d, std::vector<std::string> input = {}, std::size_t maxSteps = 5000000) {
    Run r;
    r.host.input = std::move(input);
    try {
        auto prog = compile(src, d);
        Machine m(prog, r.host);
        m.runToEnd(maxSteps);
        r.state = m.state();
        r.exit = m.exitCode();
    } catch (const Error& e) {
        r.error = e.what(); r.line = e.line; r.column = e.column;
    } catch (const std::exception& e) {
        r.error = std::string("altro: ") + e.what();
    }
    return r;
}
std::string out(const std::string& src, Dialect d, std::vector<std::string> input = {}) {
    Run r = run(src, d, std::move(input));
    if (!r.error.empty()) return "ERR[" + r.error + "@" + std::to_string(r.line) + ":" + std::to_string(r.column) + "]";
    return r.host.output;
}
} // namespace

int main() {
    // ---------------------------------------------------------------- dialetti
    CHECK(dialectOf("HELLO.c") == Dialect::C && dialectOf("A.CPP") == Dialect::Cpp && dialectOf("P.cs") == Dialect::CSharp && dialectOf("X") == Dialect::Cpp);
    CHECK(std::string(dialectName(Dialect::CSharp)) == "C#");
    // ---------------------------------------------------------------- F9.1 C
    CHECK_EQ_STR(out("#include <stdio.h>\nint main(void) {\n  printf(\"Ciao %d %s!\\n\", 42, \"mondo\");\n  return 0;\n}\n", Dialect::C), "Ciao 42 mondo!\n");
    CHECK_EQ_STR(out("#include <math.h>\n#define N 5\nint fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); }\n"
                     "int main() { int i; for (i = 1; i <= N; i++) printf(\"%d! = %d\\n\", i, fact(i)); printf(\"%.3f %5.1f|%-4d|%x %c\\n\", sqrt(2.0), M_PI, 7, 255, 'A'); return 0; }",
                     Dialect::C), "1! = 1\n2! = 2\n3! = 6\n4! = 24\n5! = 120\n1.414   3.1|7   |ff A\n");
    // divisione intera e reale come in C
    CHECK_EQ_STR(out("int main(){ int a = 7 / 2; double b = 7 / 2; double c = 7 / 2.0; int d = -7 % 3; printf(\"%d %g %g %d\", a, b, c, d); return 0; }", Dialect::C), "3 3 3.5 -1");
    // stringhe di C
    CHECK_EQ_STR(out("#include <string.h>\nint main(){ char s[20] = \"ab\"; char t[10]; strcpy(t, \"cd\"); strcat(s, t); printf(\"%s %d %d %c\", s, (int)strlen(s), strcmp(s, \"abcd\"), s[2]); return 0; }", Dialect::C), "abcd 4 0 c");
    // scanf: l'input arriva a righe e la macchina aspetta
    {
        Run r;
        auto prog = compile("int main(){ int a, b; double x; scanf(\"%d %d\", &a, &b); scanf(\"%lf\", &x); printf(\"%d %.2f\", a + b, x * 2); return 0; }", Dialect::C);
        Machine m(prog, r.host);
        m.runToEnd();
        CHECK(m.state() == Machine::State::WaitingInput && r.host.output.empty());
        r.host.input = {"3 4"};
        m.runToEnd();
        CHECK(m.state() == Machine::State::WaitingInput);
        r.host.input = {"1.25"};
        m.runToEnd();
        CHECK(m.state() == Machine::State::Done && r.host.output == "7 2.50");
    }
    // controllo di flusso: while, do, for, break, continue, switch, goto
    CHECK_EQ_STR(out("int main(){ int i = 0, s = 0; while (1) { i++; if (i % 2 == 0) continue; if (i > 9) break; s += i; }\n"
                     "do { s--; } while (s > 20);\n"
                     "switch (s) { case 20: printf(\"venti\"); case 21: printf(\"+\"); break; default: printf(\"altro\"); }\n"
                     "int k = 0; again: k++; if (k < 3) goto again; printf(\" %d %d\", s, k); return 0; }", Dialect::C), "venti+ 20 3");
    // array 1D e 2D, inizializzazioni
    CHECK_EQ_STR(out("int main(){ int a[5] = {1, 2}; int m[2][3] = {{1,2,3},{4,5,6}}; int t = 0, i, j;\n"
                     "for (i = 0; i < 5; i++) t += a[i];\n for (i = 0; i < 2; i++) for (j = 0; j < 3; j++) t += m[i][j] * 10;\n"
                     "a[4] += 7; m[1][2]++; printf(\"%d %d %d\", t, a[4], m[1][2]); return 0; }", Dialect::C), "213 7 7");
    // ---------------------------------------------------------------- C++
    CHECK_EQ_STR(out("#include <iostream>\n#include <string>\nusing namespace std;\n"
                     "double area(double r) { return 3.14159 * r * r; }\n"
                     "int main() {\n  string nome = \"PicoCalc\";\n  cout << \"Ciao \" << nome << \"! \" << nome.size() << endl;\n"
                     "  cout << area(2) << \" \" << 10 / 4 << \" \" << 10 / 4.0 << \" \" << (3 > 2) << endl;\n  return 0;\n}", Dialect::Cpp),
                 "Ciao PicoCalc! 8\n12.5664 2 2.5 1\n");
    CHECK_EQ_STR(out("#include <iostream>\nint main(){ int n; std::string s; std::cin >> n; std::getline(std::cin, s); std::getline(std::cin, s);\n"
                     " auto x = n * 2; std::cout << x << \"|\" << s << \"|\" << std::to_string(1.5) << std::endl; }", Dialect::Cpp, {"21", "riga intera"}),
                 "42|riga intera|1.500000\n");
    CHECK_EQ_STR(out("#include <cmath>\n#include <iostream>\nint main(){ double v = std::sqrt(16.0) + std::pow(2, 10); int i = static_cast<int>(v);\n"
                     " std::cout << v << ' ' << i << ' ' << std::max(3, 9) << ' ' << int(2.9) << std::endl; return 0; }", Dialect::Cpp), "1028 1028 9 2\n");
    // ---------------------------------------------------------------- C#
    CHECK_EQ_STR(out("using System;\nnamespace Demo {\n  class Program {\n    static int Square(int x) { return x * x; }\n"
                     "    static void Main(string[] args) {\n      int[] a = new int[] { 3, 4 };\n      double r = Math.Sqrt(Square(a[0]) + Square(a[1]));\n"
                     "      Console.WriteLine(\"Ipotenusa: \" + r);\n      Console.WriteLine($\"{a.Length} valori, r = {r:F2}, ok = {r == 5}\");\n"
                     "      string nome = Console.ReadLine();\n      Console.Write(nome.ToUpper());\n    }\n  }\n}\n", Dialect::CSharp, {"pico"}),
                 "Ipotenusa: 5\n2 valori, r = 5.00, ok = True\nPICO");
    CHECK_EQ_STR(out("class P { static void Main() { double[,] m = new double[2, 2]; m[0, 1] = 1.5; var n = int.Parse(\"12\");\n"
                     " Console.WriteLine(m[0, 1] * n + 0.1); Console.WriteLine(Math.Round(2.5) + \" \" + 1.0 / 3); } }", Dialect::CSharp),
                 "18.1\n2 0.3333333333333333\n");
    // ---------------------------------------------------------------- F9.3 calcolatrice: comandi grafici e fx::calc
    {
        Run r = run("#include <iostream>\nint main() {\n  fx::viewWindow(-5, 5, 1, -4, 4, 1);\n  for (int i = -2; i <= 2; i++) fx::plot(i, i * i);\n"
                    "  fx::line(0, 0, 1, 1);\n  double raggio = 2;\n  double a = fx::calc(\"\xCF\x80raggio^2\");\n  fx::setVar('B', 7);\n"
                    "  std::cout << a << ' ' << fx::calc(\"B+raggio\") << ' ' << fx::getVar('B') << std::endl;\n  return 0;\n}", Dialect::Cpp);
        CHECK(r.error.empty());
        CHECK_EQ_STR(r.host.output, "12.5664 9 7\n");
        CHECK(r.host.commands.size() == 7 && r.host.commands[0] == "ViewWindow -5,5,1,-4,4,1" && r.host.commands[1] == "Plot -2,4");
        CHECK(r.ws->view.xmin == -5 && r.ws->view.ymax == 4);                         // eseguiti da graph_commands sul workspace
        CHECK(r.ws->memory.recall('B') == 7);
        CHECK(r.ws->graph.sketch.size() >= 6);
        // un comando non valido e' l'errore di graph_commands, con la riga del programma
        Run bad = run("int main() {\n  fx::viewWindow(5, -5, 1, -4, 4, 1);\n  return 0;\n}", Dialect::Cpp);
        CHECK(bad.error.find("ERROR") != std::string::npos);
    }
    // ---------------------------------------------------------------- F9.5 errori con riga e colonna
    {
        Run e1 = run("int main() {\n  int x = 3\n  return x;\n}", Dialect::C);
        CHECK(e1.error.find("Syntax ERROR") == 0 && e1.line == 3);
        Run e2 = run("int main() {\n  int a = 0;\n  return 5 / a;\n}", Dialect::C);
        CHECK(e2.error.find("Math ERROR") == 0 && e2.line == 3);
        Run e3 = run("int main() {\n  int a[3];\n  a[3] = 1;\n  return 0;\n}", Dialect::C);
        CHECK(e3.error.find("Dimension ERROR") == 0 && e3.line == 3);
        Run e4 = run("int main() { goto fine; return 0; }", Dialect::C);
        CHECK(e4.error.find("Go ERROR") == 0 && e4.line == 1 && e4.column > 10);
        Run e5 = run("int main() { return y; }", Dialect::C);
        CHECK(e5.error.find("non dichiarata") != std::string::npos && e5.column == 21);
        Run e6 = run("int f(int a) { return a; }\nint main() { return f(1, 2); }", Dialect::C);
        CHECK(e6.error.find("argomenti") != std::string::npos && e6.line == 2);
        Run e7 = run("int main() { int *p; return 0; }", Dialect::C);
        CHECK(e7.error.find("puntatori") != std::string::npos);
        Run e8 = run("void f() {}\n", Dialect::Cpp);
        CHECK(e8.error.find("main") != std::string::npos);
        Run e9 = run("class P { static void Main() { Console.WriteLine(\"x\" } }", Dialect::CSharp);
        CHECK(e9.error.find("Syntax ERROR") == 0);
        Run e10 = run("int main() { printf(\"%d %d\", 1); return 0; }", Dialect::C);
        CHECK(e10.error.find("Argument ERROR") == 0);
        Run e11 = run("/* aperto\nint main(){}", Dialect::C);
        CHECK(e11.error.find("commento") != std::string::npos && e11.line == 1);
    }
    // ---------------------------------------------------------------- F9.2/F9.4 limiti: profondita', memoria, passi, AC
    {
        Run deep = run("int f(int n) { return f(n + 1); }\nint main() { return f(0); }", Dialect::C);
        CHECK(deep.error.find("Nesting ERROR") == 0 && deep.line == 1);
        Run ok = run("int f(int n) { if (n == 0) return 0; return 1 + f(n - 1); }\nint main() { printf(\"%d\", f(60)); return 0; }", Dialect::C);
        CHECK(ok.error.empty() && ok.host.output == "60");
        Run mem = run("int main() { int i; for (i = 0; i < 100; i++) { int a[100000]; a[0] = i; } return 0; }", Dialect::C);
        CHECK(mem.error.empty());                                                     // gli array dei blocchi si liberano
        Run big = run("int main() { int a[100000]; int b[100000]; int c[100000]; return 0; }", Dialect::C);
        CHECK(big.error.find("Memory ERROR") == 0);
        Run loop = run("int main() { while (1) {} return 0; }", Dialect::C, {}, 100000);
        CHECK(loop.error.find("Time Out") == 0);
        std::string nested = "int main() { return ";
        for (int i = 0; i < 200; ++i) nested += "(";
        nested += "1";
        for (int i = 0; i < 200; ++i) nested += ")";
        nested += "; }";
        Run stack = run(nested, Dialect::C);
        CHECK(stack.error.find("Stack ERROR") == 0);
        // a passi con AC: Break
        Run r;
        auto prog = compile("int main() { long s = 0; for (long i = 0; i < 1000000; i++) s += i; printf(\"%ld\", s); return 0; }", Dialect::C);
        Machine m(prog, r.host);
        step::Runner runner;
        runner.start(m, step::Runner::Config{1000, 0});
        std::uint32_t now = 0;
        for (int i = 0; i < 10; ++i) runner.tick([&] { return now; });
        CHECK(runner.running() && m.executed() == 10000 && m.currentLine() == 1);
        CHECK(runner.interrupt() && runner.state() == step::State::Break && m.state() == Machine::State::Failed);
        CHECK(r.host.output.empty());
        // completato a passi
        Machine m2(prog, r.host);
        runner.start(m2, step::Runner::Config{50000, 0});
        while (runner.running()) runner.tick([&] { return now; });
        CHECK(runner.state() == step::State::Done && r.host.output == "499999500000");
        CHECK(codeSize(*prog) > 10);
    }
    // ---------------------------------------------------------------- exit code e exit()
    {
        Run r = run("int main() { return 3; }", Dialect::C);
        CHECK(r.error.empty() && r.exit == 3 && r.state == Machine::State::Done);
        Run e = run("#include <stdlib.h>\nint main() { printf(\"a\"); exit(4); printf(\"b\"); return 0; }", Dialect::C);
        CHECK(e.host.output == "a" && e.exit == 4);
    }
    return testutil::finish("program_core_test");
}
