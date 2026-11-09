#include "run_matrix.hpp"
#include "board_config.hpp"
#include "cheatsheet.hpp"
#include "limits.hpp"
#include <cstdio>
#include <cstring>
#include <exception>
#include <new>

namespace fx {
namespace {
// FNV-1a, used for window snapshot stamps.
struct Hash {
    std::uint32_t h{2166136261u};
    void bytes(const void* p, std::size_t n) noexcept {
        const auto* c = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) { h ^= c[i]; h *= 16777619u; }
    }
    template <class T> void value(const T& v) noexcept { bytes(&v, sizeof v); }
    void text(const std::string& s) noexcept { bytes(s.data(), s.size()); value(s.size()); }
};
}

namespace {
constexpr std::size_t kMessageCapacity = 96;
}

RunMatrixController::RunMatrixController() {
    // Reserve once so that every later status/result update reuses the buffer.
    message_.reserve(kMessageCapacity);
    result_.reserve(kMessageCapacity);
}

void RunMatrixController::setMessage(const char* text) noexcept {
    if (!text) text = "";
    const std::size_t n = std::min(std::strlen(text), message_.capacity());
    message_.assign(text, n); // n <= capacity: no reallocation, cannot throw
}

void RunMatrixController::evaluate() {
    try {
        expression_ = editor_.toExpression();
        if (expression_.empty()) {
            setMessage("Inserire un'espressione");
            result_.clear();
            return;
        }
        const double value = session_.execute(expression_);
        char buffer[48];
        std::snprintf(buffer, sizeof(buffer), "%.10g", value);
        result_ = buffer;
        message_.clear();
    } catch (const std::bad_alloc&) {
        // RunMatrixSession::execute has the strong guarantee: history and
        // Ans are unchanged, the editor still holds the expression.
        result_.clear();
        setMessage(limits::kOutOfMemoryMessage);
    } catch (const std::exception& error) {
        result_.clear();
        setMessage(error.what());
    }
}

const char* RunMatrixController::selectedAppName() const noexcept {
    static constexpr const char* names[22]={
        "RUN-MATRIX","STATISTICS","eACTIVITY","SPREADSHEET","GRAPH",
        "DYNA GRAPH","TABLE","RECURSION","CONIC GRAPHS","EQUATION",
        "PROGRAM","FINANCIAL","E-CON4","LINK","MEMORY","SYSTEM",
        "GEOMETRY","PICTURE PLOT","3D GRAPH","CONVERSION","AI CHAT",
        "SYMBOL CHEATSHEET"
    };
    return names[selected_<22?selected_:0];
}

void RunMatrixController::reportOutOfMemory() {
    setMessage(limits::kOutOfMemoryMessage);
    std::snprintf(toast_, sizeof toast_, "%s", limits::kOutOfMemoryMessage);
    diagPending_ = false;
}

void RunMatrixController::reportError(const char* text) {
    setMessage(text ? text : "Errore");
    std::snprintf(toast_, sizeof toast_, "%s", text ? text : "Errore");
}

std::uint32_t RunMatrixController::stamp(AppScreen screen) const noexcept {
    Hash h;
    h.value(screen);
    switch (screen) {
    case AppScreen::Home: h.value(selected_); break;
    case AppScreen::Graph:
        for (const auto& f : graphFunctions_) h.text(f);
        h.value(graphView_.xmin); h.value(graphView_.xmax); h.value(graphView_.ymin); h.value(graphView_.ymax);
        h.value(graphView_.cursorX); h.value(graphView_.trace); h.value(graphSelected_);
        break;
    case AppScreen::Graph3D:
        h.text(graph3dExpression_);
        h.value(graph3dView_.yawDeg); h.value(graph3dView_.pitchDeg); h.value(graph3dView_.zoom);
        break;
    case AppScreen::System: h.value(systemSelection_); break;
    default:
        h.bytes(message_.data(), message_.size()); // other screens: redraw (stamp rarely equal)
        h.value(reinterpret_cast<std::uintptr_t>(this));
        break;
    }
    return h.h;
}

void RunMatrixController::runPendingDiagnostic() {
    if (!diagPending_) return;
    diagPending_ = false;
    lastDiag_ = diag::run(static_cast<diag::Test>(diagSelection_ % diag::kTestCount));
    haveDiag_ = true;
}

void RunMatrixController::openRunMatrix() {
    screen_ = AppScreen::RunMatrix;
    editor_.clear();
    expression_.clear();
    result_.clear();
    message_.clear();
}

void RunMatrixController::handleHome(const InputEvent& event) {
    constexpr unsigned Columns=4, GridCount=20;
    if(selected_>=GridCount){
        switch(event.action){
        case InputAction::Left: case InputAction::Right: selected_=selected_==20?21:20; return;
        case InputAction::Up: selected_=selected_==20?16:17; return;
        case InputAction::Down: return;
        case InputAction::Fraction: selected_=0; openRunMatrix(); return;
        case InputAction::Evaluate:
            if(selected_==21){screen_=AppScreen::Cheatsheet;cheatsheetPage_=0;cheatsheetSelection_=0;message_.clear();}
            else {screen_=AppScreen::Unavailable;setMessage("AI chat requires network/API configuration");}
            return;
        default:return;
        }
    }
    const unsigned row=selected_/Columns, col=selected_%Columns;
    switch(event.action){
    case InputAction::Left: selected_=row*Columns+(col+Columns-1)%Columns; return;
    case InputAction::Right: selected_=row*Columns+(col+1)%Columns; return;
    case InputAction::Up:
        if(row==0) selected_=16+col;
        else selected_=(row-1)*Columns+col;
        return;
    case InputAction::Down:
        if(row==4) selected_=(col==0?20:21);
        else selected_=(row+1)*Columns+col;
        return;
    case InputAction::Fraction: selected_=0; openRunMatrix(); return;
    case InputAction::Evaluate:
        if(selected_==0) openRunMatrix();
        else if(selected_==4){screen_=AppScreen::Graph;message_.clear();}
        else if(selected_==15){screen_=AppScreen::System;systemSelection_=0;message_.clear();}
        else if(selected_==18){
            if(FXAI_ENABLE_3D){screen_=AppScreen::Graph3D;message_.clear();}
            else {screen_=AppScreen::Unavailable;setMessage("3D disabilitato (FXAI_ENABLE_3D=OFF)");}
        }
        else {screen_=AppScreen::Unavailable;setMessage("Application is not implemented yet");}
        return;
    default:return;
    }
}

void RunMatrixController::handleGraph(const InputEvent& event) {
    switch(event.action){
    case InputAction::Left: graphView_.cursorX=graphView_.cursorX>12?graphView_.cursorX-4:12;graphView_.trace=true;break;
    case InputAction::Right: graphView_.cursorX=graphView_.cursorX<308?graphView_.cursorX+4:308;graphView_.trace=true;break;
    case InputAction::Up: GraphRenderer::zoom(graphView_,0.8);break;
    case InputAction::Down: GraphRenderer::zoom(graphView_,1.25);break;
    case InputAction::Clear: graphView_=GraphView{};break;
    case InputAction::Fraction: graphSelected_=(graphSelected_+1)%graphFunctions_.size();graphView_.trace=true;break; // trace next Yn
    case InputAction::Evaluate: screen_=AppScreen::GraphFunctions;message_.clear();break;
    default:break;
    }
}

void RunMatrixController::handleGraphFunctions(const InputEvent& event) {
    const std::size_t maxFunctions = limits::graphMaxFunctions();
    const std::size_t rows = graphFunctions_.size() + (graphFunctions_.size() < maxFunctions ? 1 : 0);
    if (graphSelected_ >= rows) graphSelected_ = rows - 1;
    message_.clear();
    switch(event.action){
    case InputAction::Up: graphSelected_ = (graphSelected_ + rows - 1) % rows; break;
    case InputAction::Down: graphSelected_ = (graphSelected_ + 1) % rows; break;
    case InputAction::Character:
        if (graphSelected_ == graphFunctions_.size()) {
            if (graphFunctions_.size() >= maxFunctions) { setMessage("Numero massimo di funzioni raggiunto"); break; }
            graphFunctions_.emplace_back(); // may throw std::bad_alloc: nothing else changed yet
        }
        graphFunctions_[graphSelected_].push_back(event.character);
        break;
    case InputAction::Backspace:
        if (graphSelected_ < graphFunctions_.size() && !graphFunctions_[graphSelected_].empty())
            graphFunctions_[graphSelected_].pop_back();
        break;
    case InputAction::Clear:
        if (graphSelected_ < graphFunctions_.size()) graphFunctions_[graphSelected_].clear();
        break;
    case InputAction::Delete:
        if (graphSelected_ < graphFunctions_.size()) {
            if (graphFunctions_.size() > 1) graphFunctions_.erase(graphFunctions_.begin() + static_cast<std::ptrdiff_t>(graphSelected_));
            else graphFunctions_.front().clear();
            if (graphSelected_ >= graphFunctions_.size()) graphSelected_ = graphFunctions_.size() - 1;
        }
        break;
    case InputAction::Evaluate:
        if (graphSelected_ >= graphFunctions_.size()) graphSelected_ = 0;
        screen_ = AppScreen::Graph;
        break;
    default: break;
    }
}

void RunMatrixController::handleGraph3D(const InputEvent& event) {
    switch(event.action){
    case InputAction::Left: g3d::rotate(graph3dView_,-15.0F,0.0F); break;
    case InputAction::Right: g3d::rotate(graph3dView_,15.0F,0.0F); break;
    case InputAction::Up: g3d::rotate(graph3dView_,0.0F,10.0F); break;
    case InputAction::Down: g3d::rotate(graph3dView_,0.0F,-10.0F); break;
    case InputAction::Fraction: graph3dView_.zoom = graph3dView_.zoom < 4.0F ? graph3dView_.zoom * 1.25F : graph3dView_.zoom; break;
    case InputAction::Radical: graph3dView_.zoom = graph3dView_.zoom > 0.3F ? graph3dView_.zoom / 1.25F : graph3dView_.zoom; break;
    case InputAction::Clear: graph3dView_ = g3d::View{}; break;
    case InputAction::Character: graph3dExpression_.push_back(event.character); break;
    case InputAction::Backspace: if(!graph3dExpression_.empty()) graph3dExpression_.pop_back(); break;
    case InputAction::Delete: graph3dExpression_.clear(); break;
    default: break;
    }
}

void RunMatrixController::handleSystem(const InputEvent& event) {
    if (screen_ == AppScreen::System) {
        switch(event.action){
        case InputAction::Up: systemSelection_ = (systemSelection_ + kSystemEntries - 1) % kSystemEntries; break;
        case InputAction::Down: systemSelection_ = (systemSelection_ + 1) % kSystemEntries; break;
        case InputAction::Evaluate:
            if (systemSelection_ == 0) { screen_ = AppScreen::Memory; memoryPage_ = 0; }
            else if (systemSelection_ == 1) { screen_ = AppScreen::Memory; memoryPage_ = 1; }
            else { screen_ = AppScreen::Diagnostics; }
            break;
        default: break;
        }
        return;
    }
    if (screen_ == AppScreen::Memory) {
        if (event.action == InputAction::Left || event.action == InputAction::Right)
            memoryPage_ = (memoryPage_ + 1) % kMemoryPages;
        return;
    }
    // Diagnostics
    switch(event.action){
    case InputAction::Up: diagSelection_ = static_cast<unsigned>((diagSelection_ + diag::kTestCount - 1) % diag::kTestCount); break;
    case InputAction::Down: diagSelection_ = static_cast<unsigned>((diagSelection_ + 1) % diag::kTestCount); break;
    case InputAction::Evaluate: diagPending_ = true; break; // executed by the main loop after a "running" frame
    default: break;
    }
}

void RunMatrixController::handle(const InputEvent& event) {
    if (event.action == InputAction::None) return;
    if (screen_ == AppScreen::Home) { handleHome(event); return; }
    if(event.action==InputAction::Escape){
        // Sub-screens go back to their parent; applications return to the launcher.
        if(screen_==AppScreen::Memory||screen_==AppScreen::Diagnostics) screen_=AppScreen::System;
        else if(screen_==AppScreen::GraphFunctions) screen_=AppScreen::Graph;
        else screen_=AppScreen::Home;
        diagPending_=false;
        return;
    }
    if(screen_==AppScreen::Unavailable){if(event.action==InputAction::Evaluate||event.action==InputAction::Clear)screen_=AppScreen::Home;return;}
    if(screen_==AppScreen::Cheatsheet){
        const unsigned count=static_cast<unsigned>(help::PageCount);
        const unsigned entries=help::pages[cheatsheetPage_].count;
        switch(event.action){
        case InputAction::Left: cheatsheetPage_=(cheatsheetPage_+count-1)%count;cheatsheetSelection_=0;break;
        case InputAction::Right: cheatsheetPage_=(cheatsheetPage_+1)%count;cheatsheetSelection_=0;break;
        case InputAction::Up: if(entries) cheatsheetSelection_=(cheatsheetSelection_+entries-1)%entries;break;
        case InputAction::Down: if(entries) cheatsheetSelection_=(cheatsheetSelection_+1)%entries;break;
        case InputAction::Clear: cheatsheetSelection_=0;break;
        default: break;
        }
        return;
    }
    if(screen_==AppScreen::Graph){handleGraph(event);return;}
    if(screen_==AppScreen::GraphFunctions){
        try { handleGraphFunctions(event); }
        catch (const std::bad_alloc&) { setMessage(limits::kOutOfMemoryMessage); }
        return;
    }
    if(screen_==AppScreen::Graph3D){handleGraph3D(event);return;}
    if(screen_==AppScreen::System||screen_==AppScreen::Memory||screen_==AppScreen::Diagnostics){handleSystem(event);return;}

    editor_.clearError();
    if (event.action != InputAction::Evaluate) message_.clear();
    try {
    switch (event.action) {
    case InputAction::Escape: screen_ = AppScreen::Home; break;
    case InputAction::Character: editor_.insert(event.character); result_.clear(); message_.clear(); break;
    case InputAction::Left: editor_.handle({NaturalKey::Left, 0}); break;
    case InputAction::Right: editor_.handle({NaturalKey::Right, 0}); break;
    case InputAction::Up: editor_.handle({NaturalKey::Up, 0}); break;
    case InputAction::Down: editor_.handle({NaturalKey::Down, 0}); break;
    case InputAction::Backspace: editor_.handle({NaturalKey::Backspace, 0}); result_.clear(); break;
    case InputAction::Delete: editor_.handle({NaturalKey::Delete, 0}); result_.clear(); break;
    case InputAction::Fraction: editor_.insertFraction(); result_.clear(); break;
    case InputAction::Radical: editor_.insertRadical(); result_.clear(); break;
    case InputAction::Power: editor_.insertPower(); result_.clear(); break;
    case InputAction::Subscript: editor_.insertSubscript(); result_.clear(); break;
    case InputAction::Group: editor_.insertGroup(); result_.clear(); break;
    case InputAction::Evaluate: evaluate(); break;
    case InputAction::Clear: editor_.clear(); expression_.clear(); result_.clear(); message_.clear(); break;
    case InputAction::None: break;
    }
    } catch (const std::bad_alloc&) { setMessage(limits::kOutOfMemoryMessage); result_.clear(); }
    catch (const std::exception& error) { setMessage(error.what()); result_.clear(); }
    if (!editor_.error().empty()) setMessage(editor_.error());
}
}
