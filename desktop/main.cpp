#include <SDL.h>
#include <SDL_ttf.h>
#include "expression.hpp"
#include "math_engine.hpp"
#include "graph.hpp"
#include "editors.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int W=1120,H=780, SX=70,SY=76,SW=480,SH=480;
const SDL_Color body{34,39,43,255}, body2{52,58,62,255}, edge{15,18,20,255};
const SDL_Color lcd{225,235,224,255}, ink{35,49,45,255}, muted{100,119,108,255};
const SDL_Color blue{30,102,151,255}, orange{224,133,39,255}, white{245,247,244,255};
SDL_Window* win=nullptr; SDL_Renderer* ren=nullptr; TTF_Font *font=nullptr,*small=nullptr;
fx::CalculatorMemory memory; fx::AngleMode angle=fx::AngleMode::Degrees;
fx::StatisticsEditor stats; fx::MatrixEditor matrices; fx::Spreadsheet sheet;
std::string page="MENU", input, output="Pronto", status="F1 menu  F2 inserisci  EXE esegui  ESC indietro";
bool naturalDisplay=true;
std::string graphExpr="sin(x)"; double graphXmin=-6.3,graphXmax=6.3;
int selected=0, histPos=0; std::vector<std::pair<std::string,std::string>> history;
bool typing=false, running=true;
std::vector<std::string> apps={"RUN-MAT  Calcolo","GRAPH  Grafici 2D","TABLE  Tabella valori","EQUA  Equazioni","INEQ  Disequazioni","STAT  Statistica","MATRIX  Matrici","VECTOR  Vettori","COMPLEX  Complessi","BASE-N  Basi numeriche","SPREAD  Foglio elettronico","AI  Assistente (relay da configurare)","TOOLS  Altre funzioni"};
void color(SDL_Color c){SDL_SetRenderDrawColor(ren,c.r,c.g,c.b,c.a);} 
void rect(int x,int y,int w,int h,SDL_Color c,bool fill=true){color(c);SDL_Rect r{x,y,w,h}; if(fill)SDL_RenderFillRect(ren,&r);else SDL_RenderDrawRect(ren,&r);}
void line(int x1,int y1,int x2,int y2,SDL_Color c){color(c);SDL_RenderDrawLine(ren,x1,y1,x2,y2);}
void txt(const std::string&s,int x,int y,SDL_Color c=ink,bool tiny=false){if(s.empty())return; SDL_Surface* surf=TTF_RenderUTF8_Blended(tiny?small:font,s.c_str(),c);if(!surf)return;SDL_Texture*t=SDL_CreateTextureFromSurface(ren,surf);SDL_Rect d{x,y,surf->w,surf->h};SDL_RenderCopy(ren,t,nullptr,&d);SDL_DestroyTexture(t);SDL_FreeSurface(surf);}
void wrapped(const std::string&s,int x,int y,int maxw,int lineh=21){std::istringstream in(s);std::string word,cur;while(in>>word){std::string test=cur.empty()?word:cur+" "+word;int tw=0;TTF_SizeUTF8(small,test.c_str(),&tw,nullptr);if(tw>maxw&&!cur.empty()){txt(cur,x,y,ink,true);y+=lineh;cur=word;}else cur=test;}if(!cur.empty())txt(cur,x,y,ink,true);}
std::string fmt(double v){std::ostringstream o;o<<std::setprecision(11)<<v;return o.str();}
void lcdFrame(){rect(28,30,564,572,edge);rect(34,36,552,560,body2);rect(SX-8,SY-8,SW+16,SH+16,{8,12,13,255});rect(SX,SY,SW,SH,lcd);}
void topbar(const std::string& title){rect(SX,SY,SW,30,{194,211,196,255});txt("fx-CG50",SX+10,SY+6,ink);txt(title,SX+120,SY+6,ink);txt(angle==fx::AngleMode::Degrees?"DEG":angle==fx::AngleMode::Radians?"RAD":"GRA",SX+420,SY+6,ink,true);line(SX,SY+30,SX+SW,SY+30,muted);}
void panelText(int y,const std::string&s){wrapped(s,SX+12,y,SW-24,20);}
void drawMenu(){topbar("MENU");txt("MENU PRINCIPALE",SX+18,SY+44,ink);for(size_t i=0;i<apps.size();++i){int col=i%3,row=i/3,x=SX+12+col*154,y=SY+78+row*83;SDL_Color cc=(int(i)==selected)?blue:SDL_Color{207,220,207,255};rect(x,y,144,70,cc);rect(x,y,144,70,muted,false);txt(std::to_string(i+1),x+7,y+7,(int(i)==selected)?white:blue);wrapped(apps[i],x+28,y+9,108,17);}txt("F1 MENU   INVIO APRI   FRECCE SELEZIONA",SX+12,SY+445,ink,true);}
void drawCalc(){topbar("RUN-MAT");txt(naturalDisplay?"Natural V.P.A.M.":"Lineare",SX+12,SY+42,muted,true);rect(SX+10,SY+66,SW-20,84,{239,245,236,255});txt((naturalDisplay?fx::formatNatural(input):input).substr(0,48),SX+20,SY+82,ink);txt(output,SX+20,SY+116,blue);txt("Cronologia",SX+12,SY+164,muted,true);int y=SY+187;for(int i=std::max(0,(int)history.size()-5);i<(int)history.size();++i){txt(history[i].first,SX+14,y,muted,true);txt(history[i].second,SX+250,y,ink,true);y+=24;}txt("Inserisci espressione e premi INVIO",SX+12,SY+420,ink,true);txt("sin cos tan asin acos atan sinh cosh sqrt ln log exp fact nCr nPr",SX+12,SY+445,muted,true);}
void drawGraph(){topbar("GRAPH");txt("Y1 = "+graphExpr,SX+12,SY+40,ink);int gx=SX+35,gy=SY+88,gw=SW-70,gh=300;rect(gx,gy,gw,gh,{242,247,239,255});for(int i=0;i<=10;i++){int x=gx+i*gw/10;line(x,gy,x,gy+gh,{210,222,211,255});}for(int i=0;i<=8;i++){int y=gy+i*gh/8;line(gx,y,gx+gw,y,{210,222,211,255});}line(gx,gy+gh/2,gx+gw,gy+gh/2,muted);line(gx+gw/2,gy,gx+gw/2,gy+gh,muted);try{auto pts=fx::sample2D(graphExpr,graphXmin,graphXmax,-5,5,gw,gh);for(size_t i=1;i<pts.size();++i)line(gx+pts[i-1].x,gy+pts[i-1].y,gx+pts[i].x,gy+pts[i].y,blue);}catch(...){}txt("F2 cambia funzione   F3 zoom   EXE aggiorna",SX+12,SY+420,ink,true);}
void drawGeneric(){topbar(page);txt("PICO CALC / FX-CG50  -  MODALITA' DESKTOP",SX+12,SY+44,muted,true);panelText(SY+86,output);panelText(SY+150,"Comandi disponibili: inserisci i valori richiesti nella riga in basso e premi INVIO. ESC torna al menu.");panelText(SY+230,"Questo ambiente usa il motore matematico C++ del progetto. Le voci contrassegnate come prototipo o non implementate sono indicate nel README.");panelText(SY+330,"Input: "+input);}
void draw(){color({17,21,24,255});SDL_RenderClear(ren);rect(12,14,600,610,body);rect(22,22,580,594,edge,false);lcdFrame();if(page=="MENU")drawMenu();else if(page=="RUN-MAT")drawCalc();else if(page=="GRAPH")drawGraph();else drawGeneric();
// physical-style PicoCalc keyboard
const char* rows[]={"ESC  F1   F2   F3   F4   F5   F6   F7   F8","1    2    3    4    5    6    7    8    9    0","Q    W    E    R    T    Y    U    I    O    P","A    S    D    F    G    H    J    K    L","Z    X    C    V    B    N    M    ,    .","SHIFT  CTRL  ALT       SPACE       BACKSPACE      ENTER"};
for(int r=0;r<6;++r){std::istringstream ss(rows[r]);std::string key;int col=0;while(ss>>key){int x=640+col*48,y=86+r*72,w=(key=="SPACE"?150:key=="BACKSPACE"?110:key=="ENTER"?95:43);rect(x,y,w,48,{43,49,54,255});rect(x,y,w,48,{92,101,106,255},false);txt(key,x+5,y+15,white,true);col+=(w>60? (w/48):1);}}
rect(640,560,450,88,{30,35,39,255});txt("TASTIERA PC / MAPPATURA",654,574,white);txt("F1 menu | F2 scrivi | F3 grafico | F4 tools",654,600,white,true);txt("F5 DEG/RAD/GRA | F6 Natural/Lineare | ESC indietro",654,622,white,true);SDL_RenderPresent(ren);}
void enterApp(){switch(selected){case 0:page="RUN-MAT";input.clear();output="Pronto";break;case 1:page="GRAPH";output="Grafico di Y1";break;case 2:page="TABLE";output="Tabella: scrivi f(x)";break;case 3:page="EQUA";output="Inserisci a,b,c separati da virgola per ax²+bx+c=0";break;case 4:page="INEQ";output="Inserisci a,b,c,relazione (0:< 1:<= 2:> 3:>=)";break;case 5:page="STAT";output="STAT: inserisci numeri separati da spazi; calcola media, sd, mediana, quartili";break;case 6:page="MATRIX";output="MATRIX: usa comandi det, inv, add, mul. Matrici fino a 4x4 nel core.";break;case 7:page="VECTOR";output="VECTOR: inserisci vettori come liste separate da virgola";break;case 8:page="COMPLEX";output="COMPLEX: primitive complesse disponibili nel motore; UI aritmetica completa da integrare.";break;case 9:page="BASE-N";output="BASE-N: inserisci numero e base, es. 255 16";break;case 10:page="SPREAD";output="SPREAD: foglio 45x5 disponibile nel core; formule/celle non ancora integrate.";break;case 11:page="AI";output="AI non collegata: configura un relay HTTPS e chiavi API esterne. Nessuna chiave viene salvata qui.";break;default:page="TOOLS";output="Altre funzioni: frazioni, percentuali, coordinate, fattori primi, distribuzioni, derivate, integrali.";break;}}
std::vector<double> numbers(const std::string&s){std::vector<double>v;std::stringstream ss(s);std::string t;while(ss>>t){if(!t.empty()&&t.back()==',')t.pop_back();if(!t.empty())v.push_back(std::stod(t));}return v;}
void execute(){try{if(page=="RUN-MAT"){double v=fx::evaluate(input,memory);memory.setAnswer(v);output=fmt(v);history.push_back({fx::formatNatural(input),output});}
else if(page=="GRAPH"){graphExpr=input.empty()?graphExpr:input;output="Y1="+graphExpr;}
else if(page=="TABLE"){auto v=fx::evaluate(input.empty()?"x":input,2);output="f(2) = "+fmt(v)+". Usa x come variabile.";}
else if(page=="EQUA"){auto v=numbers(input);if(v.size()!=3)throw std::runtime_error("Servono a b c (separati da spazi)");auto q=fx::solveQuadratic(v[0],v[1],v[2]);if(q.kind==fx::QuadraticResult::TwoReal)output="x1="+fmt(q.x1)+"  x2="+fmt(q.x2);else if(q.kind==fx::QuadraticResult::Double)output="x="+fmt(q.x1);else if(q.kind==fx::QuadraticResult::Complex)output="x="+fmt(q.real)+" ± "+fmt(q.imag)+"i";else if(q.kind==fx::QuadraticResult::Linear)output="x="+fmt(q.x1);else output="Nessuna soluzione unica / identita'.";}
else if(page=="INEQ"){auto v=numbers(input);if(v.size()!=4)throw std::runtime_error("Servono a b c rel(0..3)");auto z=fx::solvePolynomialInequality(v[0],v[1],v[2],static_cast<fx::InequalityRelation>((int)v[3]));output="Intervalli soluzione: "+std::to_string(z.size());for(auto&i:z)output+="  ["+fmt(i.lower)+", "+fmt(i.upper)+"]";}
else if(page=="STAT"){auto v=numbers(input);if(v.empty())throw std::runtime_error("Inserisci almeno un dato");auto d=fx::describe(v);output="n="+std::to_string(d.count)+" media="+fmt(d.mean)+" mediana="+fmt(d.median)+" sd camp.="+fmt(d.sample_sd)+" Q1="+fmt(d.q1)+" Q3="+fmt(d.q3);}
else if(page=="BASE-N"){auto v=numbers(input);if(v.size()!=2)throw std::runtime_error("Inserisci valore decimale e base (2..36)");if(v[0]<0||v[1]<2||v[1]>36)throw std::runtime_error("Valore o base fuori intervallo");output=fx::toBase((uint64_t)v[0],(unsigned)v[1]);}
else if(page=="TOOLS"){output="Esempi parser: 5!, log(8,2), root(27,3), percent(250), recip(4), sin(30), sqrt(81), max(2,7). Le altre API statistiche sono nel core C++.";}
else output="Questa schermata è un pannello informativo: funzione non ancora collegata alla UI.";
}catch(const std::exception&e){output=std::string("ERRORE: ")+e.what();}}
void insertKey(SDL_Keycode k){if(k==SDLK_BACKSPACE){if(!input.empty())input.pop_back();return;}if(k==SDLK_RETURN||k==SDLK_KP_ENTER){if(page=="MENU")enterApp();else execute();return;}if(k==SDLK_ESCAPE){if(typing){typing=false;SDL_StopTextInput();}else{page="MENU";input.clear();}return;}if(k==SDLK_F1){page="MENU";return;}if(k==SDLK_F2){typing=true;SDL_StartTextInput();return;}if(k==SDLK_F3){page="GRAPH";return;}if(k==SDLK_F4){page="TOOLS";return;}if(page=="MENU"){if(k==SDLK_LEFT)selected=(selected+apps.size()-1)%apps.size();if(k==SDLK_RIGHT)selected=(selected+1)%apps.size();if(k==SDLK_UP)selected=(selected+apps.size()-3)%apps.size();if(k==SDLK_DOWN)selected=(selected+3)%apps.size();if(k==SDLK_SPACE)enterApp();return;}if(k==SDLK_F5){angle=angle==fx::AngleMode::Degrees?fx::AngleMode::Radians:angle==fx::AngleMode::Radians?fx::AngleMode::Gradians:fx::AngleMode::Degrees;fx::setAngleMode(angle);return;}if(k==SDLK_F6){naturalDisplay=!naturalDisplay;return;}if(k==SDLK_DELETE||k==SDLK_KP_PERIOD){input.clear();output="Pronto";return;} }
}
int main(int argc,char**argv){(void)argc;(void)argv;if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0){std::cerr<<SDL_GetError()<<"\n";return 1;}if(TTF_Init()!=0){std::cerr<<TTF_GetError()<<"\n";return 1;}win=SDL_CreateWindow("PicoCalc FX-CG50 Desktop Emulator",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,W,H,SDL_WINDOW_RESIZABLE);ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!ren)ren=SDL_CreateRenderer(win,-1,SDL_RENDERER_SOFTWARE);font=TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans.ttf",16);if(!font)font=TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",16);small=TTF_OpenFont("/usr/share/fonts/TTF/DejaVuSans.ttf",12);if(!small)small=TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",12);if(!font||!small){std::cerr<<"Installa ttf-dejavu\n";return 1;}fx::setAngleMode(angle);SDL_StartTextInput();while(running){SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_QUIT)running=false;else if(e.type==SDL_KEYDOWN)insertKey(e.key.keysym.sym);else if(e.type==SDL_TEXTINPUT&&typing){input+=e.text.text;}}draw();SDL_Delay(16);}SDL_StopTextInput();TTF_CloseFont(font);TTF_CloseFont(small);SDL_DestroyRenderer(ren);SDL_DestroyWindow(win);TTF_Quit();SDL_Quit();return 0;}
