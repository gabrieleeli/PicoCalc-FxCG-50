#include "run_matrix.hpp"
#include "cheatsheet.hpp"
#include <cstdio>
#include <exception>

namespace fx {
void RunMatrixController::evaluate() {
    try {
        expression_ = editor_.toExpression();
        if (expression_.empty()) {
            message_ = "Inserire un'espressione";
            result_.clear();
            return;
        }
        const double value = session_.execute(expression_);
        char buffer[48];
        std::snprintf(buffer, sizeof(buffer), "%.10g", value);
        result_ = buffer;
        message_.clear();
    } catch (const std::exception& error) {
        result_.clear();
        message_ = error.what();
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

void RunMatrixController::handle(const InputEvent& event) {
    if (event.action == InputAction::None) return;
    if (screen_ == AppScreen::Home) {
        constexpr unsigned Columns=4, GridCount=20;
        if(selected_>=GridCount){
            switch(event.action){
            case InputAction::Left: case InputAction::Right: selected_=selected_==20?21:20; return;
            case InputAction::Up: selected_=selected_==20?16:17; return;
            case InputAction::Down: return;
            case InputAction::Fraction: selected_=0; screen_=AppScreen::RunMatrix; break;
            case InputAction::Evaluate:
                if(selected_==21){screen_=AppScreen::Cheatsheet;cheatsheetPage_=0;cheatsheetSelection_=0;message_.clear();}
                else {screen_=AppScreen::Unavailable;message_="AI chat requires network/API configuration";}
                return;
            default:return;
            }
        } else {
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
            case InputAction::Fraction: selected_=0; screen_=AppScreen::RunMatrix; break;
            case InputAction::Evaluate:
                if(selected_==0){screen_=AppScreen::RunMatrix;editor_.clear();expression_.clear();result_.clear();message_.clear();}
                else if(selected_==4){screen_=AppScreen::Graph;message_.clear();}
                else {screen_=AppScreen::Unavailable;message_="Application is not implemented yet";}
                return;
            default:return;
            }
        }
        if(screen_==AppScreen::RunMatrix){editor_.clear();expression_.clear();result_.clear();message_.clear();}
        return;
    }
    if(event.action==InputAction::Escape){screen_=AppScreen::Home;return;}
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
    if(screen_==AppScreen::Graph){
        switch(event.action){
        case InputAction::Left: graphView_.cursorX=graphView_.cursorX>12?graphView_.cursorX-4:12;graphView_.trace=true;break;
        case InputAction::Right: graphView_.cursorX=graphView_.cursorX<308?graphView_.cursorX+4:308;graphView_.trace=true;break;
        case InputAction::Up: GraphRenderer::zoom(graphView_,0.8);break;
        case InputAction::Down: GraphRenderer::zoom(graphView_,1.25);break;
        case InputAction::Clear: graphView_=GraphView{};break;
        default:break;
        }
        return;
    }

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
    } catch (const std::exception& error) { message_ = error.what(); result_.clear(); }
    if (!editor_.error().empty()) message_ = editor_.error();
}
}
