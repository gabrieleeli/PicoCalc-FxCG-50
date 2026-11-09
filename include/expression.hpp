#pragma once
#include "exact.hpp"
#include "expression_ast.hpp"
#include "memory.hpp"
#include <complex>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>

namespace fx {
using Complex = std::complex<double>;

// Casio-style error classes. All are std::runtime_error/std::domain_error
// subclasses so existing callers that catch std::exception keep working.
struct SyntaxError : std::runtime_error { using std::runtime_error::runtime_error; };
struct MathError : std::domain_error { using std::domain_error::domain_error; };
struct NonRealError : MathError { using MathError::MathError; };

enum class ComplexMode : std::uint8_t { Real = 0, Rectangular = 1, Polar = 2 };

// Variables bound by the caller (graph X, polar theta, parametric T, surface Y).
struct Binding { const char* name{}; double value{}; };

// Read-only access to variables owned by other applications (V-Window,
// Table range, ...). The workspace implements it; names are lower-case and
// listed by systemVariableNames().
class NameResolver {
public:
    virtual ~NameResolver() = default;
    virtual bool resolve(const std::string& lowerName, double& out) const = 0;
};

// Graph functions referenced from an expression (fx-CG50 VARS > GRAPH):
// "@Y1" = Y1(x), "@Y1(@Y2)" = composite Y1(Y2(x)), "@Y1(A=2)" = Y1 with the
// coefficient A replaced by 2. The canonical text marks a reference with '@'
// (typed letters "Y1" still mean Y*1, as on the calculator). Kinds: Y (Y= and
// Y inequalities), R (r=), XT/YT (parametric), X (X= and X inequalities),
// index 1..20. The GRAPH/TABLE engine implements the resolver; elsewhere a
// reference is a Math ERROR.
class FunctionResolver {
public:
    virtual ~FunctionResolver() = default;
    // lowerName without '@' ("y3", "xt2"). Returns the parsed body and the name
    // of the variable bound by the positional argument ("x", "theta", "t", "y"),
    // or nullptr when the function is not defined.
    virtual const ast::Node* function(const std::string& lowerName, const char*& variable) const = 0;
};
constexpr int MaxFunctionReferenceDepth = 5;       // fx-CG50: a composite function has at most 5 functions
// True for a valid reference word without the '@' marker ("y1", "xt20", ...).
bool isFunctionReferenceName(const std::string& lowerName) noexcept;

// Facts about one evaluation that the caller may want to show.
struct EvalInfo {
    bool numerical{};      // an iterative numerical method (integral, d/dx, SOLVE) produced the value
};

struct EvalContext {
    static constexpr std::size_t MaxBindings = 4;
    const CalculatorMemory* memory{};
    Binding bindings[MaxBindings]{};
    std::uint8_t bindingCount{};
    ComplexMode complexMode{ComplexMode::Real};
    const NameResolver* names{};
    const FunctionResolver* functions{};
    EvalInfo* info{};
    EvalContext& bind(const char* name, double value);
};

// Shared variable names resolved through EvalContext::names
// ("xmin", "xmax", "xdot", ..., "tblstart", "list1".."list6"); nullptr-terminated.
const char* const* systemVariableNames() noexcept;

// Limits that keep parsing and evaluation within the 32 KB stack of the RP2350.
// Longer input raises "Memory ERROR", deeper nesting "Stack ERROR".
constexpr std::size_t MaxExpressionTokens = 320;   // lexer tokens per expression
constexpr int MaxExpressionNesting = 32;           // parentheses, signs, exponents, arguments

// Parses linear text into the canonical expression AST (Binary/Unary/Call/
// Group/Token nodes). Throws SyntaxError, or MathError for the limits above.
ast::NodePtr parseExpression(const std::string& expression);

// Stack bytes one evaluation may use before it stops with "Stack ERROR". This
// is a safety net behind the parser limits; the firmware sets it at build time
// (FXAI_EVAL_STACK_BUDGET) and tests may lower it.
void setEvalStackBudget(std::size_t bytes) noexcept;
std::size_t evalStackBudget() noexcept;

// Evaluates a parsed AST. evaluateReal() throws NonRealError when a real
// result does not exist; evaluateComplex() uses principal branches.
double evaluateReal(const ast::Node& parsed, const EvalContext& context);
Complex evaluateComplex(const ast::Node& parsed, const EvalContext& context);
// Evaluates according to context.complexMode.
Complex evaluateValue(const ast::Node& parsed, const EvalContext& context);

// Exact evaluation of the same AST with rationals, surds (a√b/c ± d√e/f) and
// rational multiples of π (see exact.hpp). Returns false when any step has no
// exact form (transcendental functions, numerical calculus, random numbers,
// variables holding approximate values...); it never throws and never
// approximates. decimalInput is set when the expression contains a plain
// decimal literal or a value that came from one (fx-CG50: such results are
// shown as decimals, S<->D converts them).
bool evaluateExact(const ast::Node& parsed, const EvalContext& context, exact::Value& out, bool* decimalInput = nullptr);

// Parsed-once expression for graphs, tables and solvers.
class CompiledExpression {
public:
    CompiledExpression() = default;
    explicit CompiledExpression(const std::string& text, const char* variable = "x");
    bool empty() const noexcept { return !root_; }
    const std::string& text() const noexcept { return text_; }
    // Throws on math errors; use tryEval() in sampling loops.
    double operator()(double v, const CalculatorMemory* memory = nullptr) const;
    bool tryEval(double v, double& out, const CalculatorMemory* memory = nullptr) const noexcept;
    double eval2(double a, const char* nameB, double b, const CalculatorMemory* memory = nullptr) const;
private:
    ast::NodePtr root_;
    std::string text_;
    const char* variable_{"x"};
};

// Backwards-compatible helpers (bind x, and y where given).
double evaluate(const std::string& expression, double x = 0.0);
double evaluate(const ast::Node& expression, double x = 0.0);  // editor tree or parsed tree
double evaluate(const std::string& expression, double x, double y);
double evaluate(const std::string& expression, const CalculatorMemory& memory, double x = 0.0, double y = 0.0);
double compose(const std::string& outer, const std::string& inner, double x);
// Numerical inverse on a caller-selected branch [xMin,xMax].
double inverseValue(const std::string& expression, double y, double xMin, double xMax, unsigned iterations = 100);

// Numerical analysis on compiled expressions (used by GRAPH G-Solve, EQUATION, etc.).
double integrateAdaptive(const CompiledExpression& f, double a, double b, double tolerance = 1e-10,
                         const CalculatorMemory* memory = nullptr);
double differentiate(const CompiledExpression& f, double x, const CalculatorMemory* memory = nullptr);
// The same numerical methods (adaptive Gauss-Kronrod G7/K15, Ridders) on any
// callable; the callable throws (MathError) where the function is undefined.
double integrateFunction(const std::function<double(double)>& f, double a, double b, double tolerance = 1e-10);
double differentiateFunction(const std::function<double(double)>& f, double x);

// Rewrites a linear expression token by token with the parser's own lexer:
// every identifier (lower case: "x", "a", "sin", "theta") is passed to `map`,
// which returns its replacement or an empty string to keep it. Used by GRAPH
// to copy a Modify expression with its coefficient values and to swap x and y
// when converting Y= <-> X=. Throws SyntaxError like the parser.
std::string rewriteIdentifiers(const std::string& expression, const std::function<std::string(const std::string&)>& map);

// True for function, constant and special names known to the parser
// (lower-case input). The editor uses it to merge typed letters into words.
bool isReservedWord(const std::string& lowerName);

// Seeds the RNG used by rand()/randint() (the firmware seeds it from the uptime counter).
void seedRandom(std::uint32_t seed) noexcept;
} // namespace fx
