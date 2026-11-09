#pragma once
// TABLE core (no drawing, no UI), shared by the TABLE mode and by GRAPH for
// GPH-CON / GPH-PLT:
//  - rows: values of the variable (x, θ or T) from the table range
//    (Start/End/Step) or from a list (Setup "Variable"), at most 999;
//  - columns: the selected functions of the table type (Y=, r= or Param,
//    one type per table, manual 5-34), with derivative columns when the Setup
//    item Derivative is On;
//  - every value comes from the GRAPH engine, i.e. the shared evaluator.
#include "graph_engine.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fx::table {
constexpr std::size_t MaxRows = 999;
constexpr std::size_t MaxColumns = 40;            // 20 functions, with derivatives

struct Column {
    enum class Kind : std::uint8_t { Value, Second, Derivative };   // Second = Yt of a parametric pair
    int plot{-1};                                  // index in GraphEngine::plots()
    Kind kind{Kind::Value};
    std::string title;                             // "Y1", "Y1'", "r2", "Xt3", "Yt3"
    std::uint8_t color{0};
};

bool tableType(GraphType t) noexcept;              // Y=, r=, Param
const char* variableName(GraphType funcType) noexcept;   // "X", "θ", "T"
const char* derivativeName(GraphType funcType) noexcept; // "dY/dX", "dr/dθ"

// Rows of a table range: Start, Start+Step, ... up to End (rounding tolerant).
// Errors: Step <= 0 or End < Start (Argument ERROR), more than MaxRows (Memory ERROR).
std::vector<double> rangeRows(double start, double end, double step);
// Rows from a list: empty list (Dimension ERROR), more than MaxRows (Memory ERROR).
std::vector<double> listRows(const std::vector<double>& list);

// Columns of the selected functions of type funcType. With derivative, a
// function with a graph range or an overwrite is an error (manual 5-34).
std::vector<Column> columns(const graph::GraphEngine& engine, GraphType funcType, bool derivative);

// Value of a column at the variable value v; false where it is not defined.
bool cell(const graph::GraphEngine& engine, const Column& c, double v, double& out) noexcept;

// Point (x, y) of the graph of a column at v, for GPH-CON / GPH-PLT:
// Y=: (v, Y(v)); r=: (r cos v, r sin v); Param: (Xt(v), Yt(v)).
bool point(const graph::GraphEngine& engine, const graph::Plot& p, double v, double& x, double& y) noexcept;

// Text of a cell: at most 6 characters including the sign (manual 5-33),
// e.g. "-2.625", "1.4142", "3.2E15", "ERROR".
std::string cellText(double v);
} // namespace fx::table
