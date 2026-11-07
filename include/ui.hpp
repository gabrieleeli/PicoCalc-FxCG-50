#pragma once
#include <vector>
namespace fx { class NaturalEditor; struct GraphView; namespace pointa { struct HistoryEntry; } }
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
}
