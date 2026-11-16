#pragma once
// Motore della modalita' EQUATION (v1.12.0), senza interfaccia: coefficienti
// come espressioni del parser di Run-Matrix con la loro forma esatta, sistemi
// lineari (SIML, 2-6 incognite), equazioni polinomiali (POLY, grado 2-6) e
// SOLVER su qualsiasi variabile, come il capitolo 4 del manuale fx-CG50
// (pagine 4-1...4-5, 1-29, 1-33).
//
// Un solo parser e un solo valutatore: i coefficienti si leggono con
// parseExpression() e si valutano con evaluateAny() / evaluateExact(); le
// forme esatte (razionali, radicali quadratici, multipli di π) sono quelle di
// exact.hpp, estese ai complessi con una parte reale e una immaginaria esatte.
// Dove una forma esatta non esiste (o un passo la farebbe uscire dai limiti di
// exact::Value) si usa il calcolo in virgola mobile, come la fx-CG50 che mostra
// i decimali quando la forma naturale non e' possibile.
#include "expression.hpp"
#include "math_engine.hpp"
#include "number_format.hpp"
#include "value.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace fx::eq {

// ---------------------------------------------------------------- numeri
// Numero complesso esatto: parte reale e parte immaginaria di exact::Value.
struct ExactComplex {
    exact::Value re, im;
    bool isZero() const noexcept { return re.isZero() && im.isZero(); }
    bool isReal() const noexcept { return im.isZero(); }
    Complex toComplex() const noexcept { return {re.toDouble(), im.toDouble()}; }
    bool operator==(const ExactComplex& o) const noexcept { return re == o.re && im == o.im; }
    bool operator!=(const ExactComplex& o) const noexcept { return !(*this == o); }
    static ExactComplex real(const exact::Value& v) noexcept { ExactComplex c; c.re = v; return c; }
};
// Ogni operazione da' il risultato esatto oppure false (forma esatta impossibile).
bool add(const ExactComplex& a, const ExactComplex& b, ExactComplex& out) noexcept;
bool sub(const ExactComplex& a, const ExactComplex& b, ExactComplex& out) noexcept;
bool mul(const ExactComplex& a, const ExactComplex& b, ExactComplex& out) noexcept;
bool div(const ExactComplex& a, const ExactComplex& b, ExactComplex& out) noexcept;   // false anche per b == 0
bool neg(const ExactComplex& a, ExactComplex& out) noexcept;

// Un coefficiente o un risultato: valore numerico e, quando esiste, la forma
// esatta (hasExact). `decimal`: viene da un numero decimale digitato (la
// fx-CG50 mostra allora i decimali; S⇔D da' la forma esatta).
struct Number {
    Complex value{};
    ExactComplex exact{};
    bool hasExact{true};
    bool decimal{false};
    static Number fromExact(const ExactComplex& e, bool decimal = false) noexcept;
    static Number fromComplex(Complex z) noexcept;          // nessuna forma esatta
    static Number fromRational(const exact::Rational& r, bool decimal = false) noexcept;
    bool isZero() const noexcept { return hasExact ? exact.isZero() : value == Complex{}; }
    bool isReal() const noexcept { return hasExact ? exact.isReal() : value.imag() == 0; }
    bool operator==(const Number& o) const noexcept;
    bool operator!=(const Number& o) const noexcept { return !(*this == o); }
};

// Valore di un coefficiente digitato: un'espressione dello stesso parser di
// Run-Matrix (frazioni, radici, π, A-Z, Ans, i...). Nel modo Real un input
// con i si calcola in complesso, come nell'editor MAT/VCT. Deve dare un numero
// (TypeError altrimenti); SyntaxError / MathError / NonRealError come Run-Matrix.
Number evaluateNumber(const std::string& text, const EvalContext& context);
// Valore di un double letto da un record precedente: la forma esatta e' il
// decimale piu' corto che riproduce lo stesso double (nessuna perdita), segnata
// come decimale; altrimenti il solo valore.
Number fromStoredDouble(double value);
// Testo per l'editor (EDIT): forma esatta canonica ("1/3", "(1+sqrt(3)*i)/2"
// come "1/3", "sqrt(3)/2*i") oppure 15 cifre significative.
std::string editText(const Number& n);

// ---------------------------------------------------------------- SIML
// Sistema n x n con i coefficienti per righe [a b c ... | termine noto]
// (n righe di n+1 numeri). Gauss-Jordan esatto quando tutti i coefficienti
// hanno una forma esatta, altrimenti in virgola mobile con pivoting parziale.
struct SimulResult {
    enum class Kind : std::uint8_t { Unique, None, Infinite };
    Kind kind{Kind::Unique};
    int unknowns{0};
    std::vector<Number> solution;          // Unique: n valori
    // Infinite: per ogni incognita k, formula[k*(1+F) + 0] e' la costante e
    // formula[k*(1+F) + 1 + j] il coefficiente dell'incognita libera freeVars[j]
    // (F = freeVars.size()); le incognite libere hanno free[k] = true.
    std::vector<std::uint8_t> freeVars;
    std::vector<bool> free;
    std::vector<Number> formula;
    std::vector<Number> rref;              // forma ridotta n x (n+1) (SimRes con None / Infinite)
    bool exactPath{false};                 // calcolato in aritmetica esatta
};
// Lancia MathError ("Ma ERROR") per coefficienti non finiti o soluzioni fuori
// dall'intervallo di calcolo (|x| >= 1e100), "Underflow" per valori non nulli
// sotto 1e-99.
SimulResult solveSimultaneous(const std::vector<Number>& coefficients, int unknowns);

// ---------------------------------------------------------------- POLY
struct Root { Number value; std::uint8_t multiplicity{1}; };
// Tutte le radici distinte di a·x^n + ... = 0 (coefficienti dal grado piu'
// alto; a != 0, altrimenti Ma ERROR), con la molteplicita', nell'ordine della
// fx-CG50: reali decrescenti, poi complesse per parte reale decrescente e parte
// immaginaria positiva prima. Coefficienti razionali: scomposizione senza
// quadrati (Yun), radici razionali esatte, fattori di grado 2 (e biquadratici)
// risolti in forma esatta, gli altri fattori in virgola mobile.
std::vector<Root> solvePolynomial(const std::vector<Number>& coefficients);
// Radici mostrate con il Complex Mode: Real tiene solo quelle reali.
std::vector<Root> visibleRoots(const std::vector<Root>& roots, ComplexMode mode);

// ---------------------------------------------------------------- SOLVER
// Variabili di un'equazione del Solver, nell'ordine in cui compaiono (lettere
// A-Z, x = X; e ed i minuscole sono le costanti). Lancia SyntaxError per piu'
// di un "=" o un'espressione non valida.
std::vector<char> solverVariables(const std::string& equation);
struct SolverOutcome {
    double value{};
    double left{}, right{};
    bool retry{false};          // convergenza non sufficiente (fx-CG50 "Retry")
};
// Risolve l'equazione per `variable` partendo dal valore attuale della
// variabile (memoria A-Z del contesto) entro [lower, upper]: metodo di Newton
// e, se non converge, ricerca di un cambio di segno con bisezione. Errori del
// manuale: "No Variable", "Can't Solve!" (soluzione fuori dall'intervallo),
// "Time Out" (nessuna convergenza), Syntax ERROR (piu' di un "=").
SolverOutcome solveFor(const std::string& equation, char variable, double lower, double upper, const EvalContext& context);

// ---------------------------------------------------------------- visualizzazione
// Opzioni di visualizzazione di un numero (SET UP della fx-CG50).
struct View {
    ComplexMode complex{ComplexMode::Real};
    DisplayFormat display{};
    bool mixed{false};          // Frac Result ab/c
    bool natural{true};         // Input/Output Math
    bool fractionResults{true}; // forme esatte automatiche
    AngleMode angle{AngleMode::Radians};
    // 0 automatica, 1 decimale (S⇔D), 2 esatta richiesta (S⇔D su un decimale)
    std::uint8_t mode{0};
};
// True quando il numero si mostra nella sua forma esatta con queste opzioni.
bool showsExact(const Number& n, const View& view);
// True quando S⇔D cambia qualcosa (esiste una forma esatta diversa dal decimale).
bool hasOtherForm(const Number& n, const View& view);
// Testo lineare: "1/3", "-1/2+(√3/2)i" ("(−1+√3i)/2"), "1∠π", "0.5", "1.414213562".
// lineStyle: frazioni con ⌟ (output Line).
std::string numberText(const Number& n, const View& view, bool lineStyle = false);
// Albero 2D (math_renderer) con gli stessi modelli di Run-Matrix: frazioni
// sovrapposte, radicali, π, ×10^n, i, ∠.
ast::NodePtr numberTree(const Number& n, const View& view);
// Formula di "Infinitely Many Solutions" per l'incognita k: "Y=1.4-1.8i-(2+i)Z".
std::string formulaText(const SimulResult& r, int k, const char* const* names, const View& view);
ast::NodePtr formulaTree(const SimulResult& r, int k, const char* const* names, const View& view);

// ---------------------------------------------------------------- memorie (VARS › EQUATION)
// Matrici richiamate da Run-Matrix: SimRes (soluzioni n x 1, o la Rref con
// None / Infinite), SimCoef (n x (n+1)), PlyRes (radici m x 1), PlyCoef (1 x (g+1)).
CalcValue toMatrix(const std::vector<Number>& values, std::size_t rows, std::size_t cols);
} // namespace fx::eq
