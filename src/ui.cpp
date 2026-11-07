#include "ui.hpp"
#include "point_a.hpp"
#include "board_port.hpp"
#include "memory.hpp"
#include "natural_editor.hpp"
#include "graph_renderer.hpp"
#include "widget_system.hpp"
#include "math_renderer.hpp"
#include "expression.hpp"
#include "cheatsheet.hpp"
#include <string>
#include <algorithm>
#include <cstdio>
namespace ui {
// CG50-inspired compact icon launcher, rearranged for the PicoCalc 320x320 panel.
// The layout calls the board abstraction; the ST7365P flush still needs hardware validation.
static constexpr unsigned short BG=0x0841, TOP=0x18E3, PANEL=0x2124, EDGE=0x4208, WHITE=0xFFFF, CYAN=0x07FF, GREEN=0x07E0, ORANGE=0xFD20, PURPLE=0xA81F;

static void drawMathEditor(const fx::NaturalEditor& editor,int x,int baseline,int maxWidth){
 auto layout=fx::math::layout(editor.root(),x,baseline,maxWidth,190);
 if(layout.clipped){board::text(x,baseline,"ESPRESSIONE FUORI AREA",ORANGE);return;}
 for(const auto& op:layout.ops){
  if(op.kind==fx::NaturalDrawOp::Kind::Text)board::text(op.x,op.y,op.text.c_str(),WHITE);
  else if(op.kind==fx::NaturalDrawOp::Kind::Line)board::graphics().line(op.x,op.y,op.x2,op.y2,WHITE);
 }
 // The editor owns cursor navigation; reuse only its cursor geometry while the
 // new renderer owns all expression geometry.
 const auto cursorOps=editor.layout(x,baseline,maxWidth);
 for(const auto& op:cursorOps)if(op.kind==fx::NaturalDrawOp::Kind::Cursor)
  board::graphics().line(op.x,op.y,op.x2,op.y2,CYAN);
}

// Casio fx-CG50 style launcher: each mode has its own colored icon tile,
// a compact pictogram, its keyboard shortcut, and a caption. The 320x320
// PicoCalc panel uses a 4x5 grid plus two utility tiles in the footer.
struct HomeTile {
 const char* label;
 const char* glyph;
 unsigned short color;
 const char* key;
};
static constexpr HomeTile HOME_TILES[20]={
 {"Run-Matrix","x+",0xE3A0,"1"}, {"Statistics","%",0x2E69,"2"},
 {"eActivity","eA",0xD0B2,"3"}, {"Spreadsheet","[]",0x65A0,"4"},
 {"Graph","xy",0x681F,"5"}, {"Dyna Graph","dyn",0x051F,"6"},
 {"Table","123",0x781F,"7"}, {"Recursion","a(n)",0x583F,"8"},
 {"Conic Graphs","c",0x35B7,"9"}, {"Equation","ax",0xFD20,"A"},
 {"Program","{ }",0xA6A0,"B"}, {"Financial","$",0xFFE0,"C"},
 {"E-CON4","EC",0x901F,"D"}, {"Link","<> ",0x8D5A,"E"},
 {"Memory","MEM",0x9CD3,"F"}, {"System","SYS",0x8D5A,"G"},
 {"Geometry","geo",0x6B7F,"H"}, {"Picture Plot","PLOT",0x26A8,"I"},
 {"3D Graph","3D",0x585F,"J"}, {"Conversion","<>",0xDDF7,"K"}
};
static void drawHomeTile(unsigned index,int x,int y,bool selected){
 const auto& t=HOME_TILES[index];
 const unsigned short border=selected?CYAN:EDGE;
 board::rect(x,y,76,46,PANEL,true);
 board::rect(x,y,76,46,border,false);
 board::rect(x+10,y+3,56,25,t.color,true);
 board::rect(x+10,y+3,56,25,WHITE,false);
 const int glyphX=x+38-static_cast<int>(std::char_traits<char>::length(t.glyph))*3;
 board::text(glyphX,y+12,t.glyph,WHITE);
 // The small upper-right key marker mirrors the original Casio icon badges.
 board::rect(x+58,y+4,7,8,0x4208,true);
 board::text(x+59,y+4,t.key,WHITE);
 const int labelX=x+38-static_cast<int>(std::char_traits<char>::length(t.label))*3;
 board::text(labelX,y+34,t.label,WHITE);
 if(selected) board::graphics().roundedRect(x-1,y-1,78,48,3,CYAN,false);
}
void drawHome(unsigned selected){
 board::clear(BG);
 board::rect(0,0,320,25,TOP,true);
 board::text(7,8,"PicoCalc FX",CYAN);
 board::text(226,8,"CG50 MODES",WHITE);
 for(unsigned i=0;i<20;++i){
  const int col=static_cast<int>(i%4),row=static_cast<int>(i/4);
  drawHomeTile(i,2+col*80,29+row*49,selected==i);
 }
 // Extra project-specific applications live below the Casio mode grid.
 const bool aiSelected=selected==20,helpSelected=selected==21;
 board::rect(3,277,153,39,PANEL,true);
 board::rect(3,277,153,39,aiSelected?CYAN:EDGE,false);
 board::rect(10,283,34,26,CYAN,true); board::text(19,292,"AI",WHITE);
 board::text(51,286,"AI CHAT",WHITE); board::text(51,300,"CLAUDE / GPT",CYAN);
 board::rect(164,277,153,39,PANEL,true);
 board::rect(164,277,153,39,helpSelected?CYAN:EDGE,false);
 board::rect(171,283,34,26,PURPLE,true); board::text(180,292,"?",WHITE);
 board::text(212,286,"SYMBOLS",WHITE); board::text(212,300,"CHEATSHEET",CYAN);
 board::present();
}

// Hardware bring-up screen: use this before judging color order, orientation or offsets.
void drawDisplayCalibration(){
 board::clear(0x0000);
 board::rect(0,0,320,24,0x2104,true);
 board::text(6,8,"LCD TEST 320x320",0xFFFF);
 const unsigned short bars[8]={0xF800,0x07E0,0x001F,0xFFFF,0xFFE0,0xF81F,0x07FF,0x0000};
 for(int i=0;i<8;++i) board::rect(8+i*38,38,34,34,bars[i],true);
 board::rect(7,82,306,230,0xFFFF,false);
 for(int x=16;x<320;x+=16) board::rect(x,90,1,214,0x4208,true);
 for(int y=90;y<312;y+=16) board::rect(8,y,304,1,0x4208,true);
 board::rect(8,90,8,8,0xF800,true);
 board::rect(304,90,8,8,0x07E0,true);
 board::rect(8,304,8,8,0x001F,true);
 board::rect(304,304,8,8,0xFFFF,true);
 board::text(18,112,"TOP LEFT",0xFFFF);
 board::text(18,128,"RGB565 COLOR BARS",0xFFFF);
 board::text(18,144,"GRID 16 PX",0xFFFF);
 board::text(18,290,"BOTTOM LEFT",0xFFFF);
 board::present();
}
void drawUnavailable(const char* appName,const char* message){
 board::clear(BG);board::rect(0,0,320,30,TOP,true);board::text(8,9,"PicoCalc FX / LAUNCHER",CYAN);
 board::rect(12,58,296,174,PANEL,true);board::rect(12,58,296,174,EDGE,false);
 board::text(24,82,appName?appName:"APP",CYAN);board::text(24,118,"MODULO NON DISPONIBILE",ORANGE);
 board::text(24,145,message?message:"Funzione non ancora collegata",WHITE);
 board::text(8,280,"ESC / MENU  TORNA AL LAUNCHER",CYAN);board::text(8,300,"EXE  TORNA AL LAUNCHER",WHITE);board::present();
}
void drawCheatsheet(unsigned page,unsigned selected){
 const unsigned pageCount=static_cast<unsigned>(fx::help::PageCount);
 if(pageCount==0) return;
 page%=pageCount;
 const auto& data=fx::help::pages[page];
 if(data.count==0) return;
 selected%=data.count;
 board::clear(BG);
 board::rect(0,0,320,28,TOP,true);
 board::text(8,9,"PicoCalc FX / GUIDA MATEMATICA",CYAN);
 board::text(8,36,data.title,WHITE);
 char pageLabel[20]; std::snprintf(pageLabel,sizeof(pageLabel),"PAGE %u/%u",page+1,pageCount);
 board::text(244,36,pageLabel,CYAN);
 for(unsigned i=0;i<data.count && i<6;++i){
  const int y=57+static_cast<int>(i)*27;
  if(i==selected) board::rect(7,y-3,306,24,0x2945,true);
  board::text(13,y,data.entries[i].symbol,i==selected?CYAN:ORANGE);
  board::text(122,y,data.entries[i].meaning,WHITE);
 }
 board::rect(7,224,306,48,PANEL,true); board::rect(7,224,306,48,EDGE,false);
 board::text(14,231,"ESEMPIO / NOTA",CYAN);
 board::text(14,246,data.entries[selected].example,WHITE);
 board::text(8,282,"SU/GIU: VOCE   SINISTRA/DESTRA: PAGINA",CYAN);
 board::text(8,299,"ESC: INDIETRO  |  RIFERIMENTO E SINTASSI",WHITE);
 board::present();
}

void drawStatus(const char*t,const char*s){board::clear(BG);board::rect(0,0,320,30,TOP,true);board::text(9,9,t,CYAN);board::text(10,42,s,WHITE);board::present();}
void drawCalculator(const char* expression,const char* result,const char* mode,const char* message){
 board::clear(BG);
 board::rect(0,0,320,28,TOP,true); board::text(8,9,"PicoCalc FX  /  CALC",CYAN); board::text(255,9,mode?mode:"DEG",WHITE);
 board::rect(7,38,306,76,PANEL,true); board::rect(7,38,306,76,EDGE,false);
 board::text(16,49,"Input",CYAN); std::string natural=fx::formatNatural(expression?expression:""); board::text(16,69,natural.c_str(),WHITE);
 board::text(16,91,message?message:"",ORANGE);
 board::rect(7,121,306,48,TOP,true); board::text(16,130,"Risultato",CYAN); board::text(16,149,result?result:"0",WHITE);
 board::text(8,180,"STORIA",CYAN); board::rect(7,194,306,42,PANEL,true); board::text(15,210,"Nessun calcolo precedente",WHITE);
 // Tasti funzione grafici: sei colonne compatte, con colori distinti.
 const char* keys[12]={"SHIFT","ALPHA","MENU","SETUP","DEL","AC","sin","cos","tan","^","( )","="};
 for(int i=0;i<12;++i){int col=i%4,row=i/4;int x=7+col*78,y=243+row*24;unsigned short c=(i==11)?GREEN:(i<6?EDGE:PANEL);board::rect(x,y,72,20,c,true);board::rect(x,y,72,20,WHITE,false);board::text(x+7,y+7,keys[i],WHITE);}
 board::rect(0,319,320,1,TOP,true);
 board::present();
}
void drawMatrixEditor(const char* name,const double* values,unsigned rows,unsigned cols){
 board::clear(BG);board::rect(0,0,320,30,TOP,true);board::text(8,9,"EDITOR MATRICE",CYAN);board::text(230,9,name?name:"MatA",WHITE);
 if(rows>4) rows=4;
 if(cols>4) cols=4;
 for(unsigned r=0;r<rows;++r)for(unsigned c=0;c<cols;++c){int x=12+static_cast<int>(c)*76,y=55+static_cast<int>(r)*48;board::rect(x,y,68,36,PANEL,true);board::rect(x,y,68,36,EDGE,false);char v[20];std::snprintf(v,sizeof(v),"%.5g",values?values[r*cols+c]:0.0);board::text(x+5,y+13,v,WHITE);}
 board::text(8,280,"F1 DIM  F2 MODIFICA  AC ESCI",CYAN);
 board::present();
}
void drawStatisticsEditor(const char* name,const double* values,unsigned count){
 board::clear(BG);board::rect(0,0,320,30,TOP,true);board::text(8,9,"EDITOR STATISTICO",CYAN);board::text(245,9,name?name:"List1",WHITE);
 if(count>12) count=12;
 for(unsigned i=0;i<count;++i){int col= i/6,row=i%6;int x=10+col*155,y=43+row*35;board::rect(x,y,145,29,PANEL,true);char v[28];std::snprintf(v,sizeof(v),"%u: %.8g",i+1,values?values[i]:0.0);board::text(x+6,y+10,v,WHITE);}
 board::text(8,280,"F1 INSERISCI  F2 ELIMINA  EXE OK",CYAN);
 board::present();
}
}

namespace ui {
void drawNaturalEditor(const fx::NaturalEditor& editor,const char* message){
 board::clear(BG); board::rect(0,0,320,28,TOP,true);
 board::text(8,9,"PicoCalc FX / NATURAL",CYAN);
 board::rect(7,38,306,220,PANEL,true); board::rect(7,38,306,220,EDGE,false);
 drawMathEditor(editor,16,130,286);
 board::text(10,270,message?message:editor.error().c_str(),ORANGE);
 board::text(10,294,"AUTO sqrt/cbrt/rootN  ^ POWER",CYAN);
 board::present();
}
}

namespace ui {
void drawRunMatrix(const fx::NaturalEditor& editor,const char* result,const char* message,
                   const std::vector<fx::pointa::HistoryEntry>& history){
 board::clear(BG); board::rect(0,0,320,28,TOP,true);
 board::text(8,9,"PicoCalc FX / RUN-MATRIX",CYAN);
 board::rect(7,38,306,150,PANEL,true); board::rect(7,38,306,150,EDGE,false);
 board::text(15,48,"Input Natural V.P.A.M.",CYAN);
 drawMathEditor(editor,16,132,286);
 board::rect(7,195,306,47,TOP,true); board::text(15,202,"Risultato",CYAN);
 board::text(15,222,result?result:"",WHITE);
 const char* status=(message&&*message)?message:editor.error().c_str();
 if(status&&*status) board::text(9,248,status,ORANGE);
 board::text(8,263,"ULTIMI CALCOLI  (UP/DOWN NELLA STORIA)",CYAN);
 // Keep the two previous entries visible, as on a replay/history strip.
 // The current result remains in its dedicated result panel above.
 const std::size_t end=history.size()>1?history.size()-1:0;
 const std::size_t begin=end>2?end-2:0;
 if(begin==end) board::text(10,280,"Nessun calcolo precedente",EDGE);
 else {
  for(std::size_t i=begin;i<end;++i){
   std::string line=fx::formatNatural(history[i].expression)+" = ";
   char value[32]; std::snprintf(value,sizeof(value),"%.8g",history[i].result); line+=value;
   if(line.size()>49) line=line.substr(0,46)+"...";
   board::text(10,278+static_cast<int>(i-begin)*15,line.c_str(),WHITE);
  }
 }
 board::text(8,309,"F1 FRAZ  F2 RAD  F3 POT  EXE CALC  ESC MENU",CYAN);
 board::present();
}
}

namespace ui {
void drawGraph(const char* expression,const fx::GraphView& view){
 board::clear(BG);board::rect(0,0,320,28,TOP,true);board::text(8,9,"PicoCalc FX / GRAPH",CYAN);
 board::text(8,31,expression?expression:"x^2",WHITE);
 fx::GraphRenderer::draw(board::graphics(),expression?expression:"x^2",view);
 board::text(8,278,"LEFT/RIGHT TRACE  UP/DOWN ZOOM",CYAN);
 if(view.trace){double xv=view.xmin+(view.xmax-view.xmin)*(std::clamp(view.cursorX,12,308)-12)/296.0;char coord[48];try{double yv=fx::evaluate(expression?expression:"x^2",xv);std::snprintf(coord,sizeof(coord),"x=%.3g  y=%.3g",xv,yv);}catch(...){std::snprintf(coord,sizeof(coord),"x=%.3g  y=--",xv);}board::text(8,290,coord,ORANGE);}
 else board::text(8,290,"AC RESET VIEW   ESC MENU",WHITE);
 board::present();
}
}
