#pragma once
#include <cstddef>
#include <string>
#include <vector>
namespace fx { class NaturalEditor; struct GraphView; namespace pointa { struct HistoryEntry; } namespace g3d { struct View; } namespace diag { struct Result; } }
namespace ui {
void drawHome(unsigned selected=0);
void drawUnavailable(const char* appName,const char* message);
void drawCheatsheet(unsigned page, unsigned selected);
void drawGraph(const char* expression,const fx::GraphView& view);
void drawDisplayCalibration();
void drawStatus(const char* title,const char* subtitle);
void drawCalculator(const char* expression,const char* result,const char* mode,const char* message=nullptr);
void drawMatrixEditor(const char* name,const double* values,unsigned rows,unsigned cols);
void drawStatisticsEditor(const char* name,const double* values,unsigned count);
void drawNaturalEditor(const fx::NaturalEditor& editor,const char* message=nullptr);
void drawRunMatrix(const fx::NaturalEditor& editor,const char* result,const char* message,
                   const std::vector<fx::pointa::HistoryEntry>& history);
// v1.9.0
void drawGraph(const std::vector<std::string>& functions,const fx::GraphView& view,std::size_t traced);
void drawGraphFunctions(const std::vector<std::string>& functions,std::size_t selected,const char* message);
void drawGraph3D(const char* expression,const fx::g3d::View& view);
void drawSystemMenu(unsigned selected);
void drawMemory(unsigned page);
void drawDiagnostics(unsigned selected,const fx::diag::Result* last,bool running);
void drawToast(const char* message);
}
