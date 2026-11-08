#pragma once
#include "expression_ast.hpp"
#include "memory.hpp"
#include <complex>
#include <cstddef>
#include <cstdint>
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

struct EvalContext {
    static constexpr std::size_t MaxBindings = 4;
    const CalculatorMemory* memory{};
    Binding bindings[MaxBindings]{};
    std::uint8_t bindingCount{};
    ComplexMode complexMode{ComplexMode::Real};
    EvalContext& bind(const char* name, double value);
};

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

// True for function, constant and special names known to the parser
// (lower-case input). The editor uses it to merge typed letters into words.
bool isReservedWord(const std::string& lowerName);

// Seeds the RNG used by rand()/randint() (the firmware seeds it from the uptime counter).
void seedRandom(std::uint32_t seed) noexcept;
} // namespace fx
