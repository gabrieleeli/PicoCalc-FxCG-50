// v1.20.0: il nuovo nucleo dell'interprete, una prova per voce delle tabelle
// LK/LC/LP/LS di docs/reports/ANALISI_FONTI_v1.20.0.md (sottoinsiemi di C,
// C++ e C#): tipi e aritmetica con le regole di ogni linguaggio, puntatori
// controllati, struct, classi, ereditarieta', template e generici,
// eccezioni, librerie standard, file confinati, errori con riga e colonna,
// GC e limiti. I risultati attesi sono quelli dei compilatori reali (gcc,
// g++, dotnet) per gli stessi programmi.
#include "file_store.hpp"
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
    std::size_t gcCycles{0}, openFiles{0};
};
files::RamFileSystem& card() { static files::RamFileSystem fs; return fs; }
Run run(const std::string& src, Dialect d, std::vector<std::string> input = {}, std::size_t maxSteps = 20000000, Limits* limits = nullptr) {
    Run r;
    // "\x04" come ultima riga = Ctrl+D nella console: fine dell'input (EOF)
    if (!input.empty() && input.back() == "\x04") { input.pop_back(); r.host.endOfInput = true; }
    r.host.input = std::move(input);
    r.host.fs = &card();
    r.host.folder = "PICOCALC/CODE/TEST";
    (void)card().makeDirs(r.host.folder);
    try {
        auto prog = compile(src, d);
        std::unique_ptr<Machine> m = limits ? std::make_unique<Machine>(prog, r.host, *limits) : std::make_unique<Machine>(prog, r.host);
        try {
            m->runToEnd(maxSteps);
        } catch (...) {
            r.state = m->state();
            r.openFiles = m->openFiles();
            throw;
        }
        r.state = m->state();
        r.exit = m->exitCode();
        r.gcCycles = m->gcCycles();
        r.openFiles = m->openFiles();
    } catch (const Error& e) {
        r.error = e.what(); r.line = e.line; r.column = e.column;
    } catch (const std::exception& e) {
        r.error = std::string("altro: ") + e.what();
    }
    return r;
}
std::string out(const std::string& src, Dialect d, std::vector<std::string> input = {}) {
    Run r = run(src, d, std::move(input));
    if (!r.error.empty()) return r.host.output + "ERR[" + r.error + "@" + std::to_string(r.line) + ":" + std::to_string(r.column) + "]";
    return r.host.output;
}
std::string c(const std::string& src, std::vector<std::string> in = {}) { return out(src, Dialect::C, std::move(in)); }
std::string cpp(const std::string& src, std::vector<std::string> in = {}) { return out(src, Dialect::Cpp, std::move(in)); }
std::string cs(const std::string& src, std::vector<std::string> in = {}) { return out(src, Dialect::CSharp, std::move(in)); }
std::string err(const std::string& src, Dialect d) { Run r = run(src, d); return r.error + "@" + std::to_string(r.line); }

// ================================================================ C
void testC() {
    // LC-01 main con argc/argv e codice d'uscita
    CHECK_EQ_STR(c("int main(int argc, char* argv[]) { printf(\"%d\", argc); return 0; }"), "1");
    CHECK(run("int main(void) { return 7; }", Dialect::C).exit == 7);
    // LC-02 interi a 8/16/32/64 bit, LP64, avvolgimento, promozioni
    CHECK_EQ_STR(c(R"(int main() { char ch = 127; ch++; unsigned char u = 250; u += 10; short s = 32767; s++;
        int i = 2147483647; i++; long l = 9223372036854775807L; l++; unsigned int ui = 0; ui--;
        printf("%d %d %d %d %ld %u %zu %zu", ch, u, s, i, l, ui, sizeof(long), sizeof(int)); return 0; })"),
                 "-128 4 -32768 -2147483648 -9223372036854775808 4294967295 8 4");
    CHECK_EQ_STR(c("int main() { int a = -7; printf(\"%d %d %d %d\", a / 2, a % 2, 7 / -2, 7 % -2); return 0; }"), "-3 -1 -3 1");
    CHECK_EQ_STR(c("#include <stdint.h>\nint main() { uint8_t b = 255; int8_t sb = -1; uint16_t w = 65535; int64_t big = 1LL << 40; printf(\"%d %d %d %lld\", b, sb, w, big); return 0; }"), "255 -1 65535 1099511627776");
    CHECK_EQ_STR(c("int main() { unsigned u = 3; int i = -1; printf(\"%d\", i < u); return 0; }"), "0");   // conversione all'unsigned
    // LC-03 float e double
    CHECK_EQ_STR(c("int main() { float f = 0.1f; double d = 0.1; printf(\"%.10f %.10f %d\", f, d, f == d); return 0; }"), "0.1000000015 0.1000000000 0");
    CHECK_EQ_STR(c("int main() { double x = 1e308; x = x * 10; printf(\"%f %f\", x, -x); return 0; }"), "inf -inf");
    // LC-04 bool
    CHECK_EQ_STR(c("#include <stdbool.h>\nint main() { bool b = 5; printf(\"%d %d\", b, !b); return 0; }"), "1 0");
    // LC-05 letterali
    CHECK_EQ_STR(c("int main() { printf(\"%d %d %d %u %s\", 0x1F, 017, 'A', 3000000000u, \"ab\" \"cd\"); return 0; }"), "31 15 65 3000000000 abcd");
    // LC-06 sizeof
    CHECK_EQ_STR(c("struct P { char c; int i; double d; }; int main() { int a[10]; printf(\"%zu %zu %zu %zu\", sizeof(struct P), sizeof a, sizeof(char), sizeof(double*)); return 0; }"), "16 40 1 8");
    // LC-07 switch e goto
    CHECK_EQ_STR(c("int main() { int s = 0; for (int i = 0; i < 5; i++) switch (i) { case 0: case 1: s += 1; break; case 3: s += 10; default: s += 100; } printf(\"%d\", s); return 0; }"), "312");
    // LC-08 array: inizializzatori designati, 3D, VLA, limiti controllati, parametri
    CHECK_EQ_STR(c(R"(int somma(int v[], int n) { int s = 0; for (int i = 0; i < n; i++) s += v[i]; return s; }
        int main() { int a[6] = {[2] = 5, [4] = 7}; int m[2][2][2] = {{{1,2},{3,4}},{{5,6},{7,8}}}; int n = 4; int vla[n];
        for (int i = 0; i < n; i++) vla[i] = i * i; printf("%d %d %d %d", a[2] + a[4], m[1][0][1], somma(vla, n), a[0]); return 0; })"), "12 6 14 0");
    CHECK(err("int main() {\n  int a[5];\n  for (int i = 0; i <= 5; i++) a[i] = i;\n  return 0;\n}", Dialect::C).find("Dimension ERROR") == 0);
    // LC-09 stringhe del C: letterali in sola lettura
    CHECK(err("int main() {\n  char* s = \"ciao\";\n  s[0] = 'C';\n  return 0;\n}", Dialect::C).find("@3") != std::string::npos);
    // LC-10 struct annidate, copia, ritorno per valore, typedef
    CHECK_EQ_STR(c(R"(typedef struct { int x, y; } Pt; typedef struct { Pt a, b; char nome[8]; } Seg;
        Seg sposta(Seg s, int d) { s.a.x += d; s.b.x += d; return s; }
        int main() { Seg s = {{1, 2}, {3, 4}, "seg"}; Seg t = sposta(s, 10); Seg u = t; u.a.y = 99;
        printf("%d %d %d %d %s", s.a.x, t.a.x, t.a.y, u.a.y, t.nome); return 0; })"), "1 11 2 99 seg");
    CHECK_EQ_STR(c("struct S { int v[3]; }; int main() { struct S a = {{1,2,3}}, b; b = a; b.v[0] = 9; printf(\"%d %d\", a.v[0], b.v[0]); return 0; }"), "1 9");
    // LC-11 enum
    CHECK_EQ_STR(c("typedef enum { NORD, EST = 5, SUD } Dir; int main() { Dir d = SUD; printf(\"%d %d\", d, NORD + EST); return 0; }"), "6 5");
    // LC-12 puntatori controllati
    CHECK_EQ_STR(c(R"(void scambia(int* a, int* b) { int t = *a; *a = *b; *b = t; }
        int main() { int x = 1, y = 2; scambia(&x, &y); int v[4] = {10, 20, 30, 40}; int* p = v; p += 2; int** pp = &p;
        printf("%d %d %d %d %ld %d", x, y, *p, **pp, p - v, *(v + 3)); return 0; })"), "2 1 30 30 2 40");
    CHECK(err("int main() {\n  int* p = 0;\n  return *p;\n}", Dialect::C).find("Pointer ERROR") == 0);
    CHECK(err("#include <stdlib.h>\nint main() {\n  int* p = malloc(4 * sizeof(int));\n  free(p);\n  p[0] = 1;\n  return 0;\n}", Dialect::C).find("@5") != std::string::npos);
    CHECK(err("int* f() { int x = 5; return &x; }\nint main() {\n  int* p = f();\n  return *p;\n}", Dialect::C).find("Pointer ERROR") == 0);
    CHECK(err("#include <stdlib.h>\nint main() {\n  int* p = malloc(8);\n  free(p);\n  free(p);\n  return 0;\n}", Dialect::C).find("doppio free") != std::string::npos);
    // LC-13 puntatori a funzione
    CHECK_EQ_STR(c(R"(int piu(int a, int b) { return a + b; } int per(int a, int b) { return a * b; }
        typedef int (*Op)(int, int);
        int main() { Op ops[2] = {piu, per}; int (*f)(int, int) = &per; printf("%d %d %d", ops[0](3, 4), ops[1](3, 4), (*f)(5, 6)); return 0; })"), "7 12 30");
    // LC-14 globali, static locali, const
    CHECK_EQ_STR(c("int g = 5; const int K = 3; int conta(void) { static int n; return ++n; } int main() { conta(); conta(); printf(\"%d %d %d\", conta(), g * K, g); return 0; }"), "3 15 5");
    // LC-15 preprocessore
    CHECK_EQ_STR(c(R"(#define QUAD(x) ((x) * (x))
        #define DEBUG 1
        #if DEBUG && defined(QUAD)
        #define MSG "si"
        #else
        #define MSG "no"
        #endif
        #ifndef MANCA
        #define V 2
        #endif
        int main() { printf("%d %s %d %d", QUAD(1 + 2), MSG, V, __LINE__); return 0; })"), "9 si 2 11");
    // LC-16 printf e scanf
    CHECK_EQ_STR(c("int main() { printf(\"[%5d|%-5d|%05d|%+d|%x|%X|%o|%#x|%e|%G|%c|%%|%.2s|%*d]\", 42, 42, 42, 42, 255, 255, 8, 255, 1234.5, 0.00001, 'z', \"abc\", 4, 7); return 0; }"),
                 "[   42|42   |00042|+42|ff|FF|10|0xff|1.234500e+03|1E-05|z|%|ab|   7]");
    CHECK_EQ_STR(c("int main() { int a, n; char w[10]; double d; n = scanf(\"%d %s %lf\", &a, w, &d); printf(\"%d %d %s %.1f\", n, a, w, d); return 0; }", {"5 parola 2.5"}), "3 5 parola 2.5");
    CHECK_EQ_STR(c("int main() { char buf[20]; fgets(buf, 20, stdin); printf(\"[%s]\", buf); int c = getchar(); printf(\"%d\", c == 'x'); return 0; }", {"una riga", "x"}), "[una riga\n]1");
    CHECK_EQ_STR(c("int main() { int h, m; char sep; sscanf(\"12:34\", \"%d%c%d\", &h, &sep, &m); char cls[8]; sscanf(\"abc123\", \"%[a-z]\", cls); printf(\"%d %c %d %s\", h, sep, m, cls); return 0; }"), "12 : 34 abc");
    // LC-17 file confinati e atomici
    CHECK_EQ_STR(c(R"(#include <stdio.h>
        int main() { FILE* f = fopen("dati.txt", "w"); fprintf(f, "%d %d\n", 3, 4); fputs("fine\n", f); fclose(f);
        f = fopen("dati.txt", "r"); int a, b; char w[10]; fscanf(f, "%d %d %s", &a, &b, w); fclose(f);
        f = fopen("dati.txt", "a"); fputc('!', f); fclose(f);
        f = fopen("dati.txt", "r"); int n = 0, ch; while ((ch = fgetc(f)) != EOF) n++; fclose(f);
        printf("%d %s %d %d", a + b, w, n, fopen("../fuori.txt", "w") == NULL); return 0; })"), "7 fine 10 1");
    CHECK_EQ_STR(c("#include <stdio.h>\nint main() { FILE* f = fopen(\"/assoluto.txt\", \"r\"); if (!f) perror(\"apertura\"); return 0; }").substr(0, 9), "apertura:");
    // LC-18 stdlib: heap controllato, conversioni, rand della norma, qsort, bsearch
    CHECK_EQ_STR(c(R"(#include <stdlib.h>
        int cmp(const void* a, const void* b) { return *(const int*)a - *(const int*)b; }
        int main() { int* v = malloc(5 * sizeof(int)); for (int i = 0; i < 5; i++) v[i] = (i * 7) % 5; qsort(v, 5, sizeof(int), cmp);
        int key = 3; int* hit = bsearch(&key, v, 5, sizeof(int), cmp); int trovato = *hit; v = realloc(v, 10 * sizeof(int)); v[9] = 9;
        char* end; long x = strtol("123abc", &end, 10); printf("%d%d%d%d%d %d %d %ld %s %d %.1f", v[0], v[1], v[2], v[3], v[4], trovato, v[9], x, end, atoi(" 42"), atof("2.5"));
        free(v); return 0; })"), "01234 3 9 123 abc 42 2.5");
    CHECK_EQ_STR(c("#include <stdlib.h>\nint main() { srand(1); int a = rand(); int b = rand(); srand(1); printf(\"%d %d %d %d\", a, b, rand() == a, RAND_MAX); return 0; }"), "16838 5758 1 32767");
    // LC-19 string.h con i limiti
    CHECK_EQ_STR(c(R"(#include <string.h>
        int main() { char a[20] = "cane"; char b[20]; strcpy(b, a); strcat(b, "gatto"); char* p = strstr(b, "gat");
        char t[] = "a,b,,c"; int n = 0; for (char* k = strtok(t, ","); k; k = strtok(NULL, ",")) n++;
        int z[4]; memset(z, 0, sizeof z); z[1] = 5; int w[4]; memcpy(w, z, sizeof z);
        printf("%s %zu %s %d %d %d %d %c", b, strlen(b), p, strcmp("abc", "abd") < 0, n, w[1], strncmp("abcd", "abce", 3), *strrchr("a.b.c", '.')); return 0; })"),
                 "canegatto 9 gatto 1 3 5 0 .");
    CHECK(err("#include <string.h>\nint main() {\n  char s[4];\n  strcpy(s, \"troppo lungo\");\n  return 0;\n}", Dialect::C).find("Dimension ERROR") == 0);
    // LC-20 ctype, math, time, limits
    CHECK_EQ_STR(c(R"(#include <ctype.h>
        #include <math.h>
        #include <limits.h>
        int main() { printf("%d %d %c %c %.4f %.4f %.1f %d %d %.3f %d", isdigit('5') != 0, isalpha('5') != 0, toupper('q'), tolower('Q'),
                     sqrt(2), pow(2, 0.5), fabs(-3.5), INT_MAX == 2147483647, isnan(NAN) != 0, hypot(3, 4) + cbrt(8) + log2(8) - 10, (int)floor(-2.5)); return 0; })"),
                 "1 0 Q q 1.4142 1.4142 3.5 1 1 0.000 -3");
    // LC-22 non supportati: errori chiari
    CHECK(err("union U { int a; float b; };\nint main() { return 0; }", Dialect::C).find("union") != std::string::npos);
    CHECK(err("int somma(int n, ...) { return n; }\nint main() { return 0; }", Dialect::C).find("variadic") != std::string::npos ||
          err("int somma(int n, ...) { return n; }\nint main() { return 0; }", Dialect::C).find("...") != std::string::npos);
    CHECK(err("#include <pthread.h>\nint main() { return 0; }", Dialect::C).find("non supportata") != std::string::npos);
    CHECK(err("int main() {\n  char b[10];\n  gets(b);\n  return 0;\n}", Dialect::C).find("fgets") != std::string::npos);
}

// ================================================================ C++
void testCpp() {
    // LP-02/03 classi, accesso, costruttori, distruttori in ordine inverso
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        struct T { std::string n; T(std::string s) : n(s) { std::cout << "+" << n; } ~T() { std::cout << "-" << n; } };
        int main() { T a("a"); { T b("b"); T c("c"); } T* d = new T("d"); delete d; std::cout << "|"; return 0; })"), "+a+b+c-c-b+d-d|-a");
    CHECK(err("class C { int x; };\nint main() {\n  C c;\n  return c.x;\n}", Dialect::Cpp).find("@4") != std::string::npos);   // privato
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        class V { public: int x, y; V() : V(0, 0) {} V(int a, int b) : x(a), y(b) {} V(const V& o) : x(o.x + 100), y(o.y) {}
        V& operator=(const V& o) { x = o.x + 1000; y = o.y; return *this; } };
        int main() { V a(1, 2); V b = a; V c; c = a; std::cout << b.x << " " << c.x << " " << V().y; return 0; })"), "101 1001 0");
    // LP-04 new/delete controllati
    CHECK(err("int main() {\n  int* p = new int(3);\n  delete p;\n  delete p;\n  return 0;\n}", Dialect::Cpp).find("@4") != std::string::npos);
    CHECK_EQ_STR(cpp("#include <iostream>\nint main() { int* v = new int[5]; for (int i = 0; i < 5; i++) v[i] = i; int s = 0; for (int i = 0; i < 5; i++) s += v[i]; delete[] v; std::cout << s; return 0; }"), "10");
    // LP-05 ereditarieta', virtual, astratte, dynamic_cast, slicing
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        struct Animale { virtual ~Animale() {} virtual std::string verso() const = 0; std::string chi() const { return "animale"; } };
        struct Cane : Animale { std::string verso() const override { return "bau"; } };
        struct Gatto : Animale { std::string verso() const override { return "miao"; } int vite = 7; };
        int main() { Animale* a[2] = {new Cane(), new Gatto()}; for (Animale* x : a) std::cout << x->verso() << " ";
        Gatto* g = dynamic_cast<Gatto*>(a[1]); Gatto* n = dynamic_cast<Gatto*>(a[0]); std::cout << g->vite << " " << (n == nullptr) << " " << a[0]->chi();
        for (Animale* x : a) delete x; return 0; })"), "bau miao 7 1 animale");
    CHECK(err("struct A { virtual void f() = 0; };\nint main() {\n  A a;\n  return 0;\n}", Dialect::Cpp).find("astratta") != std::string::npos);
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        struct B { virtual std::string f() { return "B"; } }; struct D : B { std::string f() override { return "D"; } };
        int main() { D d; B b = d; B& r = d; std::cout << b.f() << r.f() << d.B::f(); return 0; })"), "BDB");
    // LP-06 overload e operatori
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        int f(int) { return 1; } int f(double) { return 2; } int f(const char*) { return 3; } int g(int a, int b = 10) { return a + b; }
        struct C { double re, im; C operator+(const C& o) const { return {re + o.re, im + o.im}; } bool operator==(const C& o) const { return re == o.re && im == o.im; } };
        std::ostream& operator<<(std::ostream& os, const C& c) { return os << c.re << "+" << c.im << "i"; }
        int main() { C a{1, 2}, b{3, 4}; std::cout << f(1) << f(1.5) << f("x") << g(1) << " " << (a + b) << " " << (a == a); return 0; })"), "12311 4+6i 1");
    // LP-07 riferimenti
    CHECK_EQ_STR(cpp("#include <iostream>\nvoid inc(int& x) { x++; } int& primo(int* v) { return v[0]; } int main() { int a = 1; inc(a); int& r = a; r *= 10; int v[2] = {5, 6}; primo(v) = 50; std::cout << a << v[0]; return 0; }"), "2050");
    // LP-08 namespace
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        namespace geo { namespace piano { int area(int a, int b) { return a * b; } } const double PI = 3.14; }
        namespace geo { int doppia(int x) { return 2 * x; } }
        using namespace geo;
        int main() { std::cout << piano::area(2, 3) << doppia(4) << " " << geo::PI; return 0; })"), "68 3.14");
    // LP-09 template di funzione e di classe
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        template <typename T> T maxi(T a, T b) { return a > b ? a : b; }
        template <class T, int N> struct Pila { T v[N]; int n = 0; void push(T x) { v[n++] = x; } T pop() { return v[--n]; } int cap() const { return N; } };
        int main() { Pila<std::string, 4> p; p.push("a"); p.push("b"); std::cout << maxi(3, 9) << maxi<double>(2.5, 1) << p.pop() << p.cap(); return 0; })"), "92.5b4");
    // LP-10 auto, range-for, binding strutturati, lambda con catture, enum class
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        #include <vector>
        #include <map>
        #include <algorithm>
        enum class Col { Rosso, Verde };
        int main() { std::vector<int> v{3, 1, 2}; int k = 10; auto add = [k](int x) { return x + k; }; int tot = 0; auto acc = [&tot](int x) { tot += x; };
        for (auto& x : v) x *= 2; for (int x : v) acc(x); std::map<std::string, int> m{{"a", 1}, {"b", 2}}; std::string s;
        for (auto [key, val] : m) s += key + std::to_string(val); Col c = Col::Verde;
        std::sort(v.begin(), v.end(), [](int a, int b) { return a > b; });
        std::cout << add(5) << " " << tot << " " << s << " " << (c == Col::Verde) << " " << v[0]; return 0; })"), "15 12 a1b2 1 6");
    // LP-11 std::string
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        #include <string>
        int main() { std::string s = "Ciao Mondo"; s.replace(0, 4, "Salve"); s.insert(0, ">"); s += "!"; std::string t = s.substr(1, 5);
        std::cout << s << " " << t << " " << s.find('M') << " " << (s.find("zz") == std::string::npos) << " " << s.size() << " " << s.back() << " "
                  << std::stoi("42") + 1 << " " << std::to_string(7) << " " << (t < s) << " " << s.at(1); return 0; })"), ">Salve Mondo! Salve 7 1 13 ! 43 7 0 S");
    // LP-12 iostream e iomanip
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        #include <iomanip>
        int main() { std::cout << std::setw(6) << 42 << "|" << std::left << std::setw(6) << "ab" << "|" << std::right << std::setfill('0') << std::setw(5) << 7
                  << "|" << std::fixed << std::setprecision(2) << 3.14159 << "|" << std::scientific << 1234.5 << "|" << std::defaultfloat << 0.1 + 0.2
                  << "|" << std::boolalpha << true << "|" << std::hex << 255 << std::dec << "|" << 1.0 / 3 << std::endl; return 0; })"),
                 "    42|ab    |00007|3.14|1.23e+03|0.3|true|ff|0.33\n");   // la precisione 2 resta anche con defaultfloat
    CHECK_EQ_STR(cpp("#include <iostream>\nint main() { int a; double b; std::string w; char ch; std::cin >> a >> b >> w >> ch; std::cout << a * 2 << b << w << ch; return 0; }", {"3 1.5", "ciao x"}), "61.5ciaox");
    CHECK_EQ_STR(cpp("#include <iostream>\nint main() { int x, s = 0; while (std::cin >> x) s += x; std::cout << s; return 0; }", {"1 2 3", "4", "\x04"}), "10");
    CHECK_EQ_STR(c("int main() { int x, s = 0, n = 0; while (scanf(\"%d\", &x) == 1) { s += x; n++; } printf(\"%d %d %d\", s, n, getchar() == EOF); return 0; }", {"5 6", "\x04"}), "11 2 1");
    CHECK_EQ_STR(cs("using System; class P { static void Main() { string l; int n = 0; while ((l = Console.ReadLine()) != null) n += l.Length; Console.Write(n); } }", {"abc", "de", "\x04"}), "5");
    CHECK(run("#include <iostream>\nint main() { int x, s = 0; while (std::cin >> x) s += x; std::cout << s; return 0; }", Dialect::Cpp, {"1 2"}).state == Machine::State::WaitingInput);
    // LP-13 contenitori
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        #include <vector>
        #include <map>
        #include <set>
        #include <unordered_map>
        #include <stack>
        #include <queue>
        #include <deque>
        #include <list>
        int main() { std::vector<std::vector<int>> g(3, std::vector<int>(2, 1)); g[1][1] = 5; std::vector<int> v = {1, 2, 3, 4};
        v.erase(v.begin() + 1); v.insert(v.begin(), 9); v.pop_back(); std::map<int, std::string> m; m[3] = "c"; m[1] = "a"; m.insert({2, "b"});
        std::string ms; for (const auto& p : m) ms += p.second; std::set<std::string> s{"pera", "mela", "pera"};
        std::unordered_map<std::string, int> u; u["x"] = 1; u["y"]++; std::stack<int> st; st.push(1); st.push(2); std::queue<int> q; q.push(7); q.push(8);
        std::deque<int> d; d.push_front(1); d.push_back(2); std::list<int> l{3, 1, 2}; l.sort();
        std::cout << g[1][1] << g.size() << " " << v[0] << v[1] << v[2] << v.size() << " " << ms << " " << s.size() << *s.begin() << " " << u["y"] << u.count("z")
                  << " " << st.top() << q.front() << d.front() << d.back() << l.front() << " " << m.count(2) << (m.find(5) == m.end()); return 0; })"),
                 "53 9133 abc 2mela 10 27121 11");
    CHECK(err("#include <vector>\nint main() {\n  std::vector<int> v(2);\n  return v[5];\n}", Dialect::Cpp).find("@4") != std::string::npos);
    // LP-14 algorithm e numeric
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        #include <vector>
        #include <algorithm>
        #include <numeric>
        int main() { std::vector<int> v{5, 3, 8, 1}; std::sort(v.begin(), v.end()); std::reverse(v.begin(), v.end()); auto it = std::find(v.begin(), v.end(), 3);
        std::vector<int> w(4); std::fill(w.begin(), w.end(), 2); std::iota(w.begin(), w.end(), 10); int a = 1, b = 2; std::swap(a, b);
        std::cout << v[0] << " " << (it - v.begin()) << " " << std::count(v.begin(), v.end(), 8) << " " << *std::min_element(v.begin(), v.end())
                  << " " << std::accumulate(v.begin(), v.end(), 0) << " " << w[3] << " " << a << b << " " << std::max(3, 7) << std::min(2.5, 1.5)
                  << " " << std::binary_search(w.begin(), w.end(), 12); return 0; })"), "8 2 1 1 17 13 21 71.5 1");
    // LP-15 eccezioni
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        #include <stdexcept>
        struct MiaErr : std::runtime_error { int code; MiaErr(int c) : std::runtime_error("mia"), code(c) {} };
        int dividi(int a, int b) { if (b == 0) throw std::invalid_argument("divisione per zero"); return a / b; }
        int main() { try { dividi(1, 0); } catch (const std::invalid_argument& e) { std::cout << e.what() << "|"; }
        try { throw MiaErr(42); } catch (std::exception& e) { std::cout << e.what() << "|"; }
        try { throw 5; } catch (int x) { std::cout << x << "|"; }
        try { try { throw std::out_of_range("x"); } catch (...) { std::cout << "rilancio|"; throw; } } catch (const std::logic_error& e) { std::cout << "logic " << e.what(); }
        return 0; })"), "divisione per zero|mia|5|rilancio|logic x");
    CHECK(err("#include <stdexcept>\nint main() {\n  throw std::runtime_error(\"boom\");\n}", Dialect::Cpp).find("runtime_error: boom") != std::string::npos);
    // LP-16 fstream e sstream
    CHECK_EQ_STR(cpp(R"(#include <fstream>
        #include <sstream>
        #include <iostream>
        int main() { std::ofstream o("prova.csv"); o << "a,1\nb,2\n"; o.close(); std::ifstream in("prova.csv"); std::string line; int tot = 0;
        while (std::getline(in, line)) { std::stringstream ss(line); std::string k, n; std::getline(ss, k, ','); std::getline(ss, n, ','); tot += std::stoi(n); }
        std::ostringstream os; os << tot << "-" << 2.5; std::ifstream none("manca.txt"); std::cout << os.str() << " " << none.is_open() << (bool)none; return 0; })"), "3-2.5 00");
    // LP-17/18 librerie del C con std:: e fx
    CHECK_EQ_STR(cpp("#include <cstdio>\n#include <cmath>\n#include <cstring>\nint main() { std::printf(\"%.2f %zu\", std::sqrt(2.0), std::strlen(\"abc\")); fx::setVar('C', 3); printf(\" %g\", fx::getVar('C')); return 0; }"), "1.41 3 3");
    // LP-19 non supportati
    CHECK(err("struct A {}; struct B {};\nstruct C : A, B {};\nint main() { return 0; }", Dialect::Cpp).find("multipla") != std::string::npos);
    CHECK(err("#include <memory>\nint main() { return 0; }", Dialect::Cpp).find("non supportata") != std::string::npos);
    // regressioni trovate con le librerie di libraries/ (v1.20.0)
    // un operator<< dell'utente raggiungibile con un costruttore di conversione non batte cout << double
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        struct F { long n; F(long a = 0) : n(a) {} F operator/(const F& o) const { return F(n * 10 + o.n); } };
        std::ostream& operator<<(std::ostream& os, const F& f) { return os << "F" << f.n; }
        int main() { double x = 1; std::cout << x / 2 << " " << F(3) / F(4) << " " << 7 / 2; return 0; })"), "0.5 F34 3");
    // nomi dei parametri della definizione diversi da quelli del prototipo
    CHECK_EQ_STR(cpp("#include <iostream>\nint somma(int primo, int secondo);\nint somma(int a, int b) { return a + b; }\nint main() { std::cout << somma(2, 3); return 0; }"), "5");
    // costruttori e distruttori fuori dalla classe (anche in un namespace e nei template)
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        #include <vector>
        namespace geo { class P { public: P(int a); ~P(); int get() const; private: int a_; }; }
        geo::P::P(int a) : a_(a) { std::cout << "+"; }
        geo::P::~P() { std::cout << "-"; }
        int geo::P::get() const { return a_; }
        class Q { public: explicit Q(std::vector<double> c); Q doppio() const; double s() const; private: std::vector<double> c_; };
        Q::Q(std::vector<double> c) : c_(c) {}
        Q Q::doppio() const { std::vector<double> d; for (double x : c_) d.push_back(2 * x); return Q(d); }
        double Q::s() const { double t = 0; for (double x : c_) t += x; return t; }
        template <typename T> class W { public: W(T v); T v_; };
        template <typename T> W<T>::W(T v) : v_(v) {}
        int main() { { geo::P p(3); std::cout << p.get(); } Q q({1, 2, 3.5}); W<int> w(9); std::cout << q.doppio().s() << w.v_; return 0; })"), "+3-139");
}

// ================================================================ C#
void testCs() {
    // LS-01 using, namespace, Main, istruzioni di primo livello
    CHECK_EQ_STR(cs("using System;\nnamespace A.B { class P { static int Main(string[] args) { Console.Write(args.Length); return 3; } } }"), "0");
    CHECK_EQ_STR(cs("using System;\nConsole.WriteLine(\"primo livello\");\nint x = 2;\nConsole.Write(x * 3);"), "primo livello\n6");
    // LS-02 tipi e conversioni
    CHECK_EQ_STR(cs(R"(using System; class P { static void Main() { int i = int.MaxValue; i++; long l = i; byte b = 255; b++; double d = 7 / 2; double e = 7 / 2.0;
        char c = 'a'; c++; uint u = 0; u--; Console.Write($"{i} {l} {b} {d} {e} {c} {u} {(int)3.9} {(int)-3.9} {sizeof(long)}"); } })"), "-2147483648 -2147483648 0 3 3.5 b 4294967295 3 -3 8");
    CHECK(err("class P { static void Main() {\n  int x = 2.5;\n} }", Dialect::CSharp).find("@2") != std::string::npos);   // double -> int solo con cast
    // LS-03 nullable, ??, ?.
    CHECK_EQ_STR(cs(R"(using System; class N { public N next; public int v = 4; } class P { static void Main() { int? a = null; int b = a ?? 5; a ??= 7; N n = new N();
        Console.Write($"{b} {a} {a.HasValue} {n.next?.v} {n?.v} {(n.next == null)}"); } })"), "5 7 True  4 True");
    // LS-04 array, rettangolari, frastagliati, Array
    CHECK_EQ_STR(cs(R"(using System; class P { static void Main() { int[] a = {5, 2, 8}; Array.Sort(a); int[,] m = new int[2, 3]; m[1, 2] = 9; int[][] j = new int[2][];
        j[0] = new int[] {1}; j[1] = new int[3]; Array.Reverse(a); Console.Write($"{a[0]}{a[2]} {m.GetLength(1)} {m[1,2]} {j[1].Length} {m.Rank} {Array.IndexOf(a, 5)}"); } })"), "82 3 9 3 2 1");
    CHECK_EQ_STR(cs("using System; class P { static void Main() { try { int[] a = new int[2]; a[2] = 1; } catch (IndexOutOfRangeException e) { Console.Write(e.Message); } } }"), "Index was outside the bounds of the array.");
    // LS-05 classi: static, readonly, const, ref, out, params, costruttori, static ctor
    CHECK_EQ_STR(cs(R"(using System; class C { public const int K = 2; static int creati; public readonly int id; static C() { creati = 100; }
        public C() : this(1) {} public C(int i) { id = i; creati++; } public static int Creati => creati; }
        class P { static void Doppio(ref int x) { x *= 2; } static void Div(int a, int b, out int q, out int r) { q = a / b; r = a % b; }
        static int Somma(params int[] v) { int s = 0; foreach (var x in v) s += x; return s; } static int Def(int a, int b = 5) => a + b;
        static void Main() { var a = new C(); var b = new C(7); int x = 3; Doppio(ref x); Div(17, 5, out int q, out var r);
        Console.Write($"{a.id} {b.id} {C.Creati} {C.K} {x} {q}{r} {Somma()} {Somma(1, 2, 3)} {Def(1)}"); } })"), "1 7 102 2 6 32 0 6 6");
    // LS-06/07 proprieta' e indicizzatori, struct per valore
    CHECK_EQ_STR(cs(R"(using System; struct V { public int X; public V(int x) { X = x; } } class Vett { int[] d = new int[3]; public int this[int i] { get => d[i]; set => d[i] = value * 10; }
        public string Nome { get; private set; } = "v"; public int Len => d.Length; }
        class P { static void Main() { V a = new V(1); V b = a; b.X = 9; var t = new Vett(); t[1] = 4; Console.Write($"{a.X} {b.X} {t[1]} {t.Nome} {t.Len}"); } })"), "1 9 40 v 3");
    // LS-08/09 ereditarieta', interfacce, abstract, override, base, is/as, ToString
    CHECK_EQ_STR(cs(R"~(using System; interface IF { double Area(); } abstract class F : IF { public abstract double Area(); public virtual string Nome() => "forma";
        public override string ToString() => Nome() + ":" + Area(); }
        class Q : F { double l; public Q(double l) { this.l = l; } public override double Area() => l * l; public override string Nome() => "q(" + base.Nome() + ")"; }
        class P { static void Main() { F f = new Q(3); IF i = f; object o = f; Console.Write($"{f} {i.Area()} {o is Q} {(o as string) == null} {(o is Q q2 ? q2.Area() : -1)}"); } })~"), "q(forma):9 9 True True 9");
    // LS-10 enum (nomi nella stampa), switch su stringhe e enum
    CHECK_EQ_STR(cs(R"(using System; enum G { Lun, Mar = 5, Mer } class P { static string T(string s) { switch (s) { case "a": return "A"; case "b": case "c": return "BC"; default: return "?"; } }
        static void Main() { G g = G.Mer; switch (g) { case G.Mer: Console.Write("mer "); break; default: break; } Console.Write($"{g} {(int)g} {T("c")}{T("z")} {G.Lun}"); } })"), "mer Mer 6 BC? Lun");
    // LS-11 stringhe
    CHECK_EQ_STR(cs(R"(using System; class P { static void Main() { string s = "  Ciao Mondo  "; string t = s.Trim(); string[] p = "a,b,,c".Split(',');
        Console.Write($"{t.ToUpper()} {t.Substring(5)} {t.IndexOf("Mo")} {t.Replace("o", "0")} {p.Length} {string.Join("-", p)} {t.StartsWith("Ci")} {t.Contains("x")}");
        Console.Write($" {"ab".PadLeft(4, '*')} {string.IsNullOrEmpty("")} {t[0]} {t.Length} {"b".CompareTo("a")} {string.Format("{0:F1}|{1,4}", 2.25, 7)}"); } })"),
                 "CIAO MONDO Mondo 5 Cia0 M0nd0 4 a-b--c True False **ab True C 10 1 2.3|   7");
    CHECK_EQ_STR(cs("using System; using System.Text; class P { static void Main() { var sb = new StringBuilder(); sb.Append(1).Append(\"x\").AppendLine(\"y\"); sb.Insert(0, \">\"); sb[1] = '9'; Console.Write(sb.ToString() + sb.Length); } }"), ">9xy\n5");
    // LS-12 formati numerici e double come .NET
    CHECK_EQ_STR(cs(R"(using System; class P { static void Main() { double d = 1234.5678; Console.Write($"{d:F2} {d:N1} {d:0.0} {42:D5} {255:X} {0.5:P0} {1e20} {0.1 + 0.2} {1.0 / 3} {100.0} {-0.0} {double.NaN} {1e-7}"); } })"),
                 "1234.57 1,234.6 1234.6 00042 FF 50 % 1E+20 0.30000000000000004 0.3333333333333333 100 -0 NaN 1E-07");
    // LS-13 Console
    CHECK_EQ_STR(cs("using System; class P { static void Main() { string n = Console.ReadLine(); int k = int.Parse(Console.ReadLine()); Console.WriteLine(\"{0} {1}\", n, k * 2); } }", {"pico", "21"}), "pico 42\n");
    // LS-14 Math, Convert, Parse, TryParse, Random con il seme di .NET
    CHECK_EQ_STR(cs(R"(using System; class P { static void Main() { Console.Write($"{Math.Round(2.5)} {Math.Round(3.5)} {Math.Round(2.345, 2)} {Math.Floor(-1.5)} {Math.Max(3, 9)} {Math.Abs(-4)} ");
        Console.Write($"{Convert.ToInt32(4.5)} {Convert.ToInt32("17")} {double.Parse("2.5") * 2} {int.TryParse("x", out int bad)} {bad} {Math.Sign(-3.2)} {Math.Clamp(15, 0, 10)} ");
        var r = new Random(42); Console.Write($"{r.Next(100)} {r.Next(100)} {r.Next(100)} {r.NextDouble():F4}"); } })"), "2 4 2.35 -2 9 4 4 17 5 False 0 -1 10 66 14 12 0.5228");
    CHECK_EQ_STR(cs("using System; class P { static void Main() { try { int.Parse(\"abc\"); } catch (FormatException e) { Console.Write(e.Message); } } }"), "The input string 'abc' was not in a correct format.");
    // LS-15 collezioni e generici dell'utente
    CHECK_EQ_STR(cs(R"(using System; using System.Collections.Generic;
        class Box<T> { public T V; public Box(T v) { V = v; } public T Get() => V; }
        class P { static T Max<T>(T a, T b) where T : IComparable<T> => a.CompareTo(b) >= 0 ? a : b;
        static void Main() { var l = new List<int> { 5, 1, 4 }; l.Add(2); l.Sort(); l.Insert(0, 9); l.Remove(4); var d = new Dictionary<string, int>();
        d["a"] = 1; d.Add("b", 2); d["a"] += 10; var hs = new HashSet<int> { 1, 2, 2 }; var q = new Queue<string>(); q.Enqueue("x"); q.Enqueue("y");
        var st = new Stack<int>(); st.Push(1); st.Push(2); string ks = ""; foreach (var kv in d) ks += kv.Key + kv.Value;
        Console.Write($"{string.Join(",", l)} {l.Count} {l.Contains(5)} {l.IndexOf(5)} {ks} {d.ContainsKey("c")} {d.TryGetValue("b", out int bv)}{bv} {hs.Count} ");
        Console.Write($"{q.Dequeue()}{q.Count} {st.Pop()}{st.Peek()} {new Box<string>("bx").Get()} {Max(3, 8)} {Max("a", "b")} {l.Find(x => x > 4)} {l.Exists(x => x > 100)}"); } })"),
                 "9,1,2,5 4 True 3 a11b2 False True2 2 x1 21 bx 8 b 9 False");
    CHECK_EQ_STR(cs("using System; using System.Collections.Generic; class P { static void Main() { var d = new Dictionary<string,int>(); try { int x = d[\"k\"]; } catch (KeyNotFoundException e) { Console.Write(e.Message); } } }"),
                 "The given key 'k' was not present in the dictionary.");
    // LS-16 lambda e delegati con chiusure
    CHECK_EQ_STR(cs(R"(using System; using System.Collections.Generic; class P { static void Main() { int c = 0; Action inc = () => c++; inc(); inc(); Func<int, int, int> s = (a, b) => a + b;
        Predicate<int> pari = x => x % 2 == 0; var l = new List<int> { 3, 1, 2 }; l.Sort((a, b) => b.CompareTo(a)); l.ForEach(x => c += x);
        Func<int, Func<int, int>> addN = n => x => x + n; Console.Write($"{c} {s(2, 3)} {pari(4)} {l[0]} {addN(10)(5)}"); } })"), "8 5 True 3 15");
    // LS-17 eccezioni
    CHECK_EQ_STR(cs(R"(using System; class Mia : Exception { public int Code; public Mia(string m, int c) : base(m) { Code = c; } }
        class P { static int F(int x) { try { if (x > 0) throw new Mia("mia", x); return 0; } finally { Console.Write("[f]"); } }
        static void Main() { try { F(3); } catch (Mia e) when (e.Code > 1) { Console.Write(e.Message + e.Code); }
        try { object o = "s"; int i = (int)o; } catch (InvalidCastException) { Console.Write(" cast"); }
        try { int z = 0; int r = 5 / z; } catch (DivideByZeroException e) { Console.Write(" " + e.GetType().Name); } catch (Exception) { Console.Write(" altro"); }
        try { throw new InvalidOperationException("op"); } catch (Exception e) { Console.Write(" " + e.Message); } } })").substr(0, 16), "[f]mia3 cast Div");
    CHECK(err("using System; class P { static void Main() {\n  throw new ArgumentException(\"male\");\n} }", Dialect::CSharp).find("ArgumentException: male") != std::string::npos);
    // LS-18 file confinati
    CHECK_EQ_STR(cs(R"(using System; using System.IO; class P { static void Main() { File.WriteAllText("f.txt", "uno\ndue\n"); File.AppendAllText("f.txt", "tre");
        string[] r = File.ReadAllLines("f.txt"); using (var w = new StreamWriter("g.txt")) { w.WriteLine("x"); w.Write(5); } using (var rd = new StreamReader("g.txt")) { Console.Write(rd.ReadLine() + rd.ReadLine()); }
        Console.Write($" {r.Length} {r[2]} {File.Exists("f.txt")} {File.Exists("no.txt")}"); try { File.ReadAllText("../x.txt"); } catch (UnauthorizedAccessException) { Console.Write(" negato"); }
        try { File.ReadAllText("manca.txt"); } catch (FileNotFoundException) { Console.Write(" manca"); } } })"), "x5 3 tre True False negato manca");
    // LS-20 Fx
    CHECK_EQ_STR(cs("class P { static void Main() { Fx.SetVar('D', 4); System.Console.Write(Fx.GetVar('D') + Fx.Calc(\"D^2\")); } }"), "20");
    // LS-21 operatori dell'utente
    CHECK_EQ_STR(cs(R"(using System; struct C { public double R, I; public C(double r, double i) { R = r; I = i; } public static C operator +(C a, C b) => new C(a.R + b.R, a.I + b.I);
        public static bool operator ==(C a, C b) => a.R == b.R && a.I == b.I; public static bool operator !=(C a, C b) => !(a == b); public override string ToString() => $"{R}+{I}i"; }
        class P { static void Main() { var a = new C(1, 2); Console.Write($"{a + a} {a == new C(1, 2)}"); } })"), "2+4i True");
    // LS-22 non supportati: errori chiari
    CHECK(err("using System.Linq; using System.Collections.Generic; class P { static void Main() {\n  var l = new List<int>();\n  var s = l.Sum();\n} }", Dialect::CSharp).find("@3") != std::string::npos);
    CHECK(err("class P { static async void Main() { } }", Dialect::CSharp).find("async") != std::string::npos);
    // regressioni trovate con le librerie di libraries/ (v1.20.0)
    CHECK_EQ_STR(cs(R"(using System; using System.Collections.Generic;
        class R { public double A { get; } public int N { get; } = 5; public static int K { get; } public R(double a) { A = a; N = N + 1; } static R() { K = 9; } }
        class P { static double Somma(IList<double> x) { double s = 0; foreach (double v in x) s += v; return s; } static int Conta(IEnumerable<int> e) { int n = 0; foreach (int v in e) n++; return n; }
        static void Main() { var r = new R(2.5); Console.Write($"{r.A} {r.N} {R.K} {Somma(new List<double> { 1, 2.5 })} {Conta(new List<int> { 4, 5, 6 })}");
        try { throw new ArgumentOutOfRangeException("periodi", "servono periodi positivi"); } catch (ArgumentOutOfRangeException e) { Console.Write(" " + e.ParamName + "|" + e.Message); }
        try { throw new ArgumentNullException("nome"); } catch (ArgumentException e) { Console.Write(" " + e.Message); }
        try { throw new ArgumentException("valore errato", "x"); } catch (ArgumentException e) { Console.Write(" " + e.ParamName + "|" + e.Message); } } })"),
                 "2.5 6 9 3.5 3 periodi|servono periodi positivi (Parameter 'periodi') Value cannot be null. (Parameter 'nome') x|valore errato (Parameter 'x')");
}

// ================================================================ libreria fx: statistiche, distribuzioni, finanza (FX-07, FX-08)
void testFx() {
    // C: FxStat1 da un array, quartili della fx-CG50 (mediane delle meta')
    CHECK_EQ_STR(c(R"(#include <stdio.h>
        #include <fx.h>
        int main(void) { double d[] = {2, 4, 4, 5, 7, 9}; FxStat1 s = fx_stat1var_data(d, 6);
        printf("%g %.6g %g %g %.6f %.6f %g %g %g %g %g", s.n, s.mean, s.sum, s.sumSq, s.popSd, s.sampleSd, s.min, s.q1, s.median, s.q3, s.max);
        double x[] = {1, 2, 3, 4, 5}, y[] = {2.1, 3.9, 6.2, 7.8, 10.1}; FxReg r = fx_regression_data(0, x, y, 5);
        printf(" | %.4f %.4f %.5f", r.a, r.b, r.r); return 0; })"),
                 "6 5.16667 31 191 2.266912 2.483277 2 4 4.5 7 9 | 1.9900 0.0500 0.99865");
    // C++: dalle liste della calcolatrice (List 1, List 2) e dai vector; distribuzioni e TVM
    CHECK_EQ_STR(cpp(R"(#include <iostream>
        #include <iomanip>
        #include <vector>
        int main() { fx::setList(1, std::vector<double>{1, 2, 3, 4, 5}); fx::setList(2, std::vector<double>{2.1, 3.9, 6.2, 7.8, 10.1});
        fx::Stat2 s = fx::stat2Var(1, 2); auto r = fx::regression(11, 1, 2); auto q = fx::stat1Var(std::vector<double>{3, 1, 2});
        std::cout << std::fixed << std::setprecision(4) << s.meanY << " " << s.sumXY << " " << r.a << " " << r.b << " " << q.median << " "
                  << fx::normCD(-1.96, 1.96) << " " << fx::invNorm(0.975) << " " << fx::binomPD(3, 10, 0.5) << " " << fx::poissonCD(2, 3) << " "
                  << fx::tvmFV(10, 5, -1000, 0) << " " << fx::tvmPMT(12, 12, 10000, 0, 12, 12) << " " << fx::npv(5, std::vector<double>{-1000, 300, 400, 500})
                  << " " << fx::irr(std::vector<double>{-1000, 300, 400, 500}); return 0; })"),
                 "6.0200 110.2000 0.0500 1.9900 2.0000 0.9500 1.9600 0.1172 0.4232 1628.8946 -888.4879 80.4449 8.8963");
    // C#: Fx.Stat1 e Fx.Reg con le proprieta' in PascalCase, errori come FxException
    CHECK_EQ_STR(cs(R"(using System; class P { static void Main() { Fx.Stat1 s = Fx.Stat1Var(new double[] { 2, 4, 4, 5, 7, 9 }); var r = Fx.Regression(0, new double[] { 1, 2, 3 }, new double[] { 2, 4, 6 });
        Console.Write($"{s.N} {s.Q1} {s.Median} {s.Q3} {r.A} {r.B} {Fx.NormPD(0):F6} {Fx.TvmN(5, -1000, 0, 1628.894627):F3} {Fx.Days(1, 1, 2026, 12, 31, 2026)}");
        try { Fx.Stat1Var(new double[0]); } catch (FxException e) { Console.Write(" " + e.Message); }
        try { Fx.SetColor("Viola"); } catch (FxException) { Console.Write(" colore"); } Fx.SetColor("Red"); } })"),
                 "6 4 4.5 7 2 0 0.398942 10.000 364 Dimension ERROR: nessun dato colore");
    // Ans e i comandi grafici nuovi (PxlChg, DrawGraph) passano dal motore dei comandi grafici
    Run g = run("#include <fx.h>\nint main(void) { fx_pxlchg(10, 20); fx_drawgraph(); fx_setcolor(\"green\"); fx_pause(); return (int)fx_getans(); }", Dialect::C);
    CHECK(g.error.empty() && g.host.commands.size() == 2 && g.host.commands[0] == "PxlChg 10,20" && g.host.commands[1] == "DrawGraph");
    CHECK(g.host.workspace().graph.settings.plotColor == 2 && g.host.output == "- Disp -\n");
}

// ================================================================ nucleo comune
void testCore() {
    // LK-05 divisione per zero, LK-06 errori con riga e colonna
    {
        const std::string e0 = err("int main() {\n  int z = 0;\n  return 10 / z;\n}", Dialect::C);
        CHECK(e0.find("Math ERROR") == 0 && e0.size() > 2 && e0.substr(e0.size() - 2) == "@3");
    }
    // ricorsione profonda (cornici della macchina, nessuna ricorsione nativa)
    CHECK_EQ_STR(c("int f(int n) { return n == 0 ? 0 : 1 + f(n - 1); } int main() { printf(\"%d\", f(1500)); return 0; }"), "1500");
    // limiti: memoria con l'errore della fx-CG50, tempo, profondita'
    {
        Limits lim = defaultLimits(false);
        lim.heapBytes = 64 * 1024;
        Run r = run("#include <stdlib.h>\nint main() { for (int i = 0; ; i++) { int* p = malloc(1000); if (!p) { printf(\"NULL %d\", i > 10); return 0; } } }", Dialect::C, {}, 20000000, &lim);
        CHECK(r.error.empty() && r.host.output == "NULL 1");
        Run e = run("#include <vector>\nint main() { std::vector<int> v; for (;;) v.push_back(1); }", Dialect::Cpp, {}, 50000000, &lim);
        CHECK(e.error.find("bad_alloc") != std::string::npos || e.error.find("Memory ERROR") != std::string::npos);
        Run s = run("using System.Collections.Generic; class P { static void Main() { var l = new List<string>(); try { for (;;) l.Add(\"xxxxxxxxxxxxxxxx\"); } catch (System.OutOfMemoryException) { System.Console.Write(\"oom\"); } } }",
                    Dialect::CSharp, {}, 50000000, &lim);
        CHECK(s.host.output == "oom");
    }
    // GC: gli oggetti irraggiungibili si liberano (il ciclo non finisce la memoria)
    {
        Limits lim = defaultLimits(false);
        lim.heapBytes = 128 * 1024;
        Run g = run("using System.Collections.Generic; class P { static void Main() { long t = 0; for (int i = 0; i < 20000; i++) { var l = new List<int> { i, i }; t += l.Count; } System.Console.Write(t); } }",
                    Dialect::CSharp, {}, 50000000, &lim);
        CHECK(g.error.empty() && g.host.output == "40000" && g.gcCycles > 0);
    }
    // file chiusi dopo un errore
    {
        Run f = run("#include <stdio.h>\nint main() {\n  FILE* f = fopen(\"aperto.txt\", \"w\");\n  fprintf(f, \"x\");\n  int z = 0;\n  return 1 / z;\n}", Dialect::C);
        CHECK(f.error.find("Math ERROR") == 0 && f.openFiles == 0);
    }
    // eccezione non catturata di C# con la posizione
    {
        Run e = run("using System;\nclass P {\n  static void Main() {\n    object o = null;\n    Console.Write(o.ToString());\n  }\n}", Dialect::CSharp);
        CHECK(e.error.find("NullReferenceException") != std::string::npos && e.line == 5);
    }
    // v1.20.0, trovati dai fuzzer: "(" di C# senza ")" fino alla fine (lettura oltre
    // i token), spazio dei nomi come operando, std::string lunga distrutta a fine blocco
    CHECK(err("class P { static void Main() { var x = (a.b", Dialect::CSharp).find("Syntax ERROR") == 0);
    CHECK(err("#include <iostream>\nint main() { std << 1; return 0; }", Dialect::Cpp).find("operando che non e' un valore@2") != std::string::npos);
    // ... e negli altri usi come valore (fuzz_cpp sul codice finale: std[1][1] = 5)
    for (const char* e : {"std[1]", "std[1] = 5", "std++", "++std", "std += 1", "sizeof(std)", "&std", "int a = std"})
        CHECK(err(std::string("#include <iostream>\nint main() { ") + e + "; return 0; }", Dialect::Cpp).find("Syntax ERROR") == 0);
    CHECK_EQ_STR(cpp("#include <string>\n#include <iostream>\nint main() { for (int i = 0; i < 3; ++i) { std::string s(200, 'x'); s += \"y\"; } std::cout << \"ok\"; return 0; }"), "ok");
    // stringhe UTF-8 nei sorgenti e nell'output
    CHECK_EQ_STR(c("int main() { printf(\"perché %s\", \"π\"); return 0; }"), "perché π");
    CHECK_EQ_STR(cs("class P { static void Main() { string s = \"àè\"; System.Console.Write(s.Length + s.ToUpper()); } }"), "2ÀÈ");
}

// ================================================================ debugger
#define CHECK_EQ(a, b) do { const long long va_ = (a), vb_ = (b); \
    ::testutil::report(va_ == vb_, #a " == " #b, __FILE__, __LINE__, "(" + std::to_string(va_) + " vs " + std::to_string(vb_) + ")"); } while (0)
// DB-01..DB-07: prima fermata, punti d'interruzione all'ingresso della riga,
// STEP dentro/sopra/fuori, pila delle chiamate, variabili e figli, AC in pausa.
struct Dbg {
    std::unique_ptr<Workspace> ws = std::make_unique<Workspace>();
    WorkspaceHost host{*ws};
    std::unique_ptr<Machine> m;
    Dbg(const std::string& name, const std::string& src, Dialect d, const std::vector<int>& bps = {}) {
        host.fs = &card();
        host.folder = "PICOCALC/CODE/TEST";
        Project p;
        p.dialect = d;
        p.files.push_back({name, src, -1});
        p.entry = name;
        m = std::make_unique<Machine>(compile(p), host);
        m->setDebug(true);
        std::vector<std::pair<std::string, int>> b;
        for (int l : bps) b.emplace_back(name, l);
        m->setBreakpoints(b);
    }
    // esegue fino alla prossima pausa (o alla fine): la riga dove si e' fermato
    int go() {
        for (int k = 0; k < 1000; ++k) {
            step::Budget b(100000, 0, nullptr);
            if (m->step(b)) return -1;                       // finito
            if (m->state() == Machine::State::Paused) return m->currentLine();
        }
        return -2;
    }
    int next(StepMode mode) { m->resume(mode); return go(); }
    std::string var(const std::string& n) const {
        for (const VarInfo& v : m->variables(0)) if (v.name == n) return v.value;
        return "<assente>";
    }
};
void testDebugger() {
    const std::string fact =
        "#include <stdio.h>\n"            // 1
        "\n"                               // 2
        "int main(void) {\n"               // 3
        "    int f = 1;\n"                 // 4
        "    for (int i = 1; i <= 5; i++) f *= i;\n"   // 5
        "    printf(\"%d\\n\", f);\n"      // 6
        "    printf(\"Ciao\\n\");\n"       // 7
        "    return 0;\n"                  // 8
        "}\n";                             // 9
    // DB-01 la prima fermata e' la prima istruzione di main, prima di eseguirla
    {
        Dbg d("Fatt.c", fact, Dialect::C);
        CHECK_EQ(d.go(), 4);
        CHECK_EQ_STR(d.m->currentFile(), "Fatt.c");
        CHECK(d.host.output.empty());
        const auto st = d.m->callStack();
        CHECK(st.size() == 1 && st[0].function == "main" && st[0].line == 4 && st[0].file == "Fatt.c");
    }
    // DB-02 punto d'interruzione: si ferma sulla riga 6 PRIMA di eseguirla, con
    // f gia' calcolato; il ciclo su una sola riga non si ferma a ogni giro
    {
        Dbg d("Fatt.c", fact, Dialect::C, {5, 6});
        CHECK_EQ(d.go(), 4);
        CHECK_EQ(d.next(StepMode::Run), 5);                  // ingresso nella riga 5
        CHECK_EQ_STR(d.var("f"), "1");
        CHECK_EQ(d.next(StepMode::Run), 6);                  // non di nuovo la 5 (stessa riga)
        CHECK_EQ_STR(d.var("f"), "120");
        CHECK_EQ_STR(d.var("i"), "<assente>");               // fuori dal for
        CHECK(d.host.output.empty());                        // printf non ancora eseguita
        CHECK_EQ(d.next(StepMode::Run), -1);
        CHECK_EQ_STR(d.host.output, "120\nCiao\n");
    }
    // DB-03 ciclo su piu' righe: il punto nel corpo scatta a ogni giro
    {
        const std::string loop = "int main() {\n  int s = 0;\n  for (int i = 0; i < 3; i++) {\n    s += i;\n  }\n  return s;\n}\n";
        Dbg d("Ciclo.c", loop, Dialect::C, {4});
        CHECK_EQ(d.go(), 2);
        CHECK_EQ(d.next(StepMode::Run), 4);
        CHECK_EQ_STR(d.var("i"), "0");
        CHECK_EQ(d.next(StepMode::Run), 4);
        CHECK_EQ_STR(d.var("i"), "1");
        CHECK_EQ(d.next(StepMode::Run), 4);
        CHECK_EQ_STR(d.var("i"), "2");
        CHECK_EQ_STR(d.var("s"), "1");
        CHECK_EQ(d.next(StepMode::Run), -1);
        CHECK_EQ(d.m->exitCode(), 3);
    }
    // DB-04 STEP dentro, sopra e fuori con la pila delle chiamate
    {
        const std::string calls =
            "int sq(int x) {\n"            // 1
            "    int r = x * x;\n"         // 2
            "    return r;\n"              // 3
            "}\n"                          // 4
            "int main() {\n"               // 5
            "    int a = sq(3);\n"         // 6
            "    int b = sq(4);\n"         // 7
            "    return a + b;\n"          // 8
            "}\n";
        Dbg d("Sq.c", calls, Dialect::C);
        CHECK_EQ(d.go(), 6);
        CHECK_EQ(d.next(StepMode::Into), 2);                 // dentro sq
        auto st = d.m->callStack();
        CHECK(st.size() == 2 && st[0].function == "sq" && st[0].line == 2 && st[1].function == "main" && st[1].line == 6);
        CHECK_EQ_STR(d.var("x"), "3");
        CHECK_EQ(d.next(StepMode::Out), 6);                  // di nuovo in main, sulla riga della chiamata
        CHECK(d.m->callStack().size() == 1);
        CHECK_EQ(d.next(StepMode::Over), 7);                 // sopra la seconda chiamata
        CHECK_EQ_STR(d.var("a"), "9");
        CHECK_EQ(d.next(StepMode::Over), 8);
        CHECK_EQ_STR(d.var("b"), "16");
        CHECK_EQ(d.next(StepMode::Into), -1);
        CHECK_EQ(d.m->exitCode(), 25);
    }
    // DB-05 ricorsione: il punto sulla stessa riga scatta in ogni cornice
    {
        const std::string rec = "int f(int n) {\n  if (n == 0) return 0;\n  return n + f(n - 1);\n}\nint main() { return f(3); }\n";
        Dbg d("Rec.c", rec, Dialect::C, {3});
        CHECK_EQ(d.go(), 5);
        int hits = 0;
        while (d.next(StepMode::Run) == 3) ++hits;
        CHECK_EQ(hits, 3);
        CHECK_EQ(d.m->exitCode(), 6);
    }
    // DB-06 variabili e figli: struct/oggetti espandibili, globali con ::
    {
        const std::string cpp = "#include <vector>\nint g = 5;\nstruct P { int x, y; };\nint main() {\n  P p{1, 2};\n  std::vector<int> v{7, 8, 9};\n  return p.x + v[2] + g;\n}\n";
        Dbg d("V.cpp", cpp, Dialect::Cpp, {7});
        CHECK_EQ(d.go(), 5);
        CHECK_EQ(d.next(StepMode::Run), 7);
        bool sawP = false, sawV = false, sawG = false;
        for (const VarInfo& vi : d.m->variables(0)) {
            if (vi.name == "p" && vi.expandable) {
                const auto ch = d.m->children(vi.handle);
                sawP = ch.size() == 2 && ch[0].name == "x" && ch[0].value == "1" && ch[1].name == "y" && ch[1].value == "2";
            }
            if (vi.name == "v" && vi.expandable) {
                const auto ch = d.m->children(vi.handle);
                sawV = ch.size() == 3 && ch[2].name == "[2]" && ch[2].value == "9";
            }
            if (vi.name == "::g") sawG = vi.value == "5";
        }
        CHECK(sawP && sawV && sawG);
        const std::string cs = "using System;\nclass Q {\n  static void Main() {\n    var l = new System.Collections.Generic.List<string> { \"a\" };\n    Console.Write(l.Count);\n  }\n}\n";
        Dbg e("Q.cs", cs, Dialect::CSharp, {5});
        CHECK_EQ(e.go(), 4);
        CHECK_EQ(e.next(StepMode::Run), 5);
        CHECK(e.host.output.empty());
        const auto st = e.m->callStack();
        CHECK(st.size() == 1 && st[0].line == 5 && st[0].file == "Q.cs");
        CHECK_EQ(e.next(StepMode::Run), -1);
        CHECK_EQ_STR(e.host.output, "1");
    }
    // DB-07 AC in pausa: Break, nessun file aperto, niente altro output
    {
        const std::string f = "#include <stdio.h>\nint main() {\n  FILE* o = fopen(\"dbg.txt\", \"w\");\n  fprintf(o, \"x\");\n  printf(\"dopo\");\n  fclose(o);\n  return 0;\n}\n";
        Dbg d("F.c", f, Dialect::C, {5});
        CHECK_EQ(d.go(), 3);
        CHECK_EQ(d.next(StepMode::Run), 5);
        CHECK(d.m->openFiles() == 1);
        d.m->cancelled();
        CHECK(d.m->state() == Machine::State::Failed);
        CHECK(d.m->openFiles() == 0);
        CHECK(d.host.output.empty());
    }
}
} // namespace

int main() {
    testC();
    testCpp();
    testCs();
    testFx();
    testCore();
    testDebugger();
    return testutil::finish("program_lang_test");
}
