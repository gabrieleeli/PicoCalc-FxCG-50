#pragma once
// .pcgraph: original, versioned text format of a complete GRAPH project
// (documented in docs/PCGRAPH_FORMAT.md). UTF-8 lines "key=value"; the first
// line is "PCGRAPH <major>", the last one "crc32=<8 hex digits>" (CRC-32 of
// every byte before it), so truncated or damaged files are rejected. Unknown
// keys are ignored (newer minor revisions), a newer major version is refused.
// The same line syntax encodes the graph memories G-Mem1..20 ("PCGMEM 1").
#include "graph_model.hpp"
#include "math_engine.hpp"
#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace fx::graphfile {

constexpr int FormatVersion = 1;
constexpr std::size_t MaxProjectBytes = 64 * 1024;

struct Project {
    std::array<GraphFunction, MaxGraphFunctions> functions{};
    ViewWindow view;
    AngleMode angle{AngleMode::Radians};
    GraphSettings settings;
    std::array<ViewWindow, VWindowMemories> vmem{};
    std::uint8_t vmemUsed{};
    std::vector<SketchItem> sketch;
    std::string overlay;                 // picture recalled behind the graphs
    double tableStart{-3}, tableEnd{3}, tableStep{1};
    bool tableFromList{false};                                         // TABLE Setup "Variable": range or list
    std::uint8_t tableList{0};                                         // List 1..6 (0-based)
    GraphType tableFunc{GraphType::Y};                                 // Table Func: Y=, r=, Param
    Project();
};

std::string encode(const Project& p);
// "" on success, otherwise the reason; `out` is only changed on success.
std::string decode(const std::string& text, Project& out);

// Graph memory: functions with type, style, colour, draw status and V-Window (manual 5-19).
std::string encodeMemory(const std::array<GraphFunction, MaxGraphFunctions>& functions, const ViewWindow& view);
std::string decodeMemory(const std::string& text, std::array<GraphFunction, MaxGraphFunctions>& functions, ViewWindow& view);

// Percent-escaping used for text values ('%', '|', control characters).
std::string escape(const std::string& s);
bool unescape(const std::string& s, std::string& out);
} // namespace fx::graphfile
