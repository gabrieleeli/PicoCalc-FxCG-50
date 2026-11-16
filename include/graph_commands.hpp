#pragma once
// Comandi grafici di Run-Matrix (fx-CG50: menu SKETCH di Run-Matrix e dei
// programmi, manuale cap. 5 e 8). Un comando non e' un'espressione: questo
// interprete riconosce la parola del comando, separa gli argomenti e valuta
// ciascuno con l'unico parser e l'unico valutatore (expression.hpp). Il
// risultato sono dati di GRAPH nel workspace — V-Window, elementi SKETCH e
// grafici dei comandi — che la modalita' GRAPH disegna con il suo motore e il
// suo renderer: Run-Matrix non disegna nulla.
//
// Comandi:
//   Cls                                   cancella SKETCH e grafici dei comandi
//   ViewWindow Xmin,Xmax,Xscl,Ymin,Ymax,Yscl[,Tmin,Tmax,Tptch]
//   Graph Y=f(x)   Graph r=f(θ)   Graph X=f(y)   Graph(X,Y)=(xt,yt)
//   Plot x,y   PlotOn x,y   PlotOff x,y   PlotChg x,y   Line   F-Line x1,y1,x2,y2
//   Circle x,y,r   Vertical x   Horizontal y   Text riga,colonna,"testo"
//   PxlOn riga,colonna   PxlOff riga,colonna   PxlChg riga,colonna
//   Tangent f(x),x   Normal f(x),x   Inverse f(x)
// v1.12.0:
//   Graph Y> Y< Y≥ Y≤ X> X< X≥ X≤ f    G∫dx f(x),a,b
//   SketchNormal | SketchThick | SketchBroken | SketchDot | SketchThin [comando]
//   StoPict n   RclPict n   BG-Pict n   RclCapt n
//   DrawGraph   DrawStat   DrawDyna   DrawR-Con   DrawR-Plt   DrawWeb   DrawFTG-Con   DrawFTG-Plt
//   G-SelOn n   G-SelOff n
// PxlTest( e' una funzione del valutatore (ServiceResolver::pixelTest).
// Riga e colonna sono pixel della schermata del grafico (1..282, 1..320).
// Ogni comando e' tutto o niente: errori di sintassi o di calcolo (Syntax
// ERROR, Math ERROR, Memory ERROR con 64 elementi SKETCH o 10 grafici) non
// cambiano i dati.
#include <string>

namespace fx {
struct Workspace;
}

namespace fx::graphcmd {

// Vero se il testo inizia con un comando grafico (grafia della fx-CG50, con o
// senza lo spazio dopo il nome: "Plot 1,2" e "Plot1,2").
bool isCommand(const std::string& text);
// Esegue il comando sui dati di GRAPH del workspace e chiede la schermata del
// grafico (GraphState::commandScreen). Lancia SyntaxError / MathError con la
// posizione nel testo.
void execute(Workspace& ws, const std::string& text);
// Cartella delle catture dello schermo di Run-Matrix (CAPTURE, RclCapt n).
constexpr const char* CaptureFolder = "PICOCALC/CAPT";
// Nomi dei comandi per il menu SKETCH di Run-Matrix (terminati da nullptr).
const char* const* menuNames() noexcept;

} // namespace fx::graphcmd
