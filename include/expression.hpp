#pragma once
#include <string>
#include "memory.hpp"
#include "expression_ast.hpp"
namespace fx {
ast::NodePtr parseExpression(const std::string& expression);
double evaluate(const std::string& expression,double x=0.0);
// Evaluates the exact same VPAM AST produced by NaturalEditor.
double evaluate(const ast::Node& expression,double x=0.0);
double evaluate(const std::string& expression,double x,double y);
double evaluate(const std::string& expression,const CalculatorMemory& memory,double x=0.0,double y=0.0);
double compose(const std::string& outer,const std::string& inner,double x);
// Numerical inverse on a caller-selected branch [xMin,xMax].
double inverseValue(const std::string& expression,double y,double xMin,double xMax,unsigned iterations=100);
}
