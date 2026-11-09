#pragma once
// Self-tests for the validation on the PicoCalc (SYSTEM > DIAG): the microSD
// card (detection, initialisation, mounting, write / read / verify / delete
// with timings) and GRAPH (sampling and G-Solve times of a fixed set of
// graphs). Every result is a line of text shown on the screen and printed on
// the USB serial port ("[DIAG] ...") so that it can be sent back as a report.
// The user's data is not touched: the card test uses its own folder
// PICOCALC/DIAG and removes it, the graph test its own functions.
#include "file_store.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace fx {
struct Workspace;
}

namespace fx::diag {
struct Line {
    bool ok;
    std::string text;
};
using Clock = std::function<std::uint32_t()>;   // milliseconds

std::vector<Line> testStorage(files::FileSystem* fs, const Clock& now);
std::vector<Line> testGraph(Workspace& ws, const Clock& now);
} // namespace fx::diag
