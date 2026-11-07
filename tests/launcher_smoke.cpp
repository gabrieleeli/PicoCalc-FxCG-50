#include "run_matrix.hpp"
#include <cassert>
int main(){
 fx::RunMatrixController c;assert(c.screen()==fx::AppScreen::Home);assert(c.selection()==0);
 // The first Casio tile is RUN-MATRIX; the fifth is GRAPH.
 c.handle({fx::InputAction::Evaluate,0});assert(c.screen()==fx::AppScreen::RunMatrix);
 c.handle({fx::InputAction::Escape,0});assert(c.screen()==fx::AppScreen::Home);
 c.handle({fx::InputAction::Right,0});assert(c.selection()==1);
 c.handle({fx::InputAction::Evaluate,0});assert(c.screen()==fx::AppScreen::Unavailable);
 assert(c.selectedAppName()[0]=='S');
 c.handle({fx::InputAction::Escape,0});
 c.handle({fx::InputAction::Down,0});assert(c.selection()==5);
 c.handle({fx::InputAction::Left,0});assert(c.selection()==4);
 c.handle({fx::InputAction::Evaluate,0});assert(c.screen()==fx::AppScreen::Graph);
 c.handle({fx::InputAction::Escape,0});
 // Grid wraps vertically and opens the two footer applications.
 c.handle({fx::InputAction::Up,0});assert(c.selection()==0);
 c.handle({fx::InputAction::Up,0});assert(c.selection()==16);
 c.handle({fx::InputAction::Down,0});assert(c.selection()==20);
 assert(c.selectedAppName()[0]=='A');
 c.handle({fx::InputAction::Right,0});assert(c.selection()==21);
 c.handle({fx::InputAction::Evaluate,0});assert(c.screen()==fx::AppScreen::Cheatsheet);
 assert(c.cheatsheetPage()==0 && c.cheatsheetSelection()==0);
 c.handle({fx::InputAction::Down,0});assert(c.cheatsheetSelection()==1);
 c.handle({fx::InputAction::Right,0});assert(c.cheatsheetPage()==1 && c.cheatsheetSelection()==0);
 c.handle({fx::InputAction::Escape,0});assert(c.screen()==fx::AppScreen::Home);
 c.handle({fx::InputAction::Fraction,0});assert(c.screen()==fx::AppScreen::RunMatrix); // F1 shortcut
}
