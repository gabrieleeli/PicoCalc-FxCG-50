#pragma once
// Modalita' e servizi compilati in questa build (v1.15.0, F1). Le definizioni
// FXAI_MODE_<NOME> e FXAI_SERVICE_<NOME> (0/1) arrivano da
// cmake/fxai_sources.cmake con il valore della scheda; un sorgente che le usa
// senza il build del progetto non compila, cosi' nessuna build puo' perdere
// una modalita' in silenzio.
#if !defined(FXAI_MODE_EACTIVITY) || !defined(FXAI_SERVICE_TYPEDFILES) || !defined(FXAI_SERVICE_INTERP)
#error "FXAI_MODE_* / FXAI_SERVICE_* non definite: compilare con cmake/fxai_sources.cmake"
#endif

namespace fx::features {
inline constexpr bool kEActivity = FXAI_MODE_EACTIVITY != 0;
inline constexpr bool kSpreadsheet = FXAI_MODE_SPREADSHEET != 0;
inline constexpr bool kDynaGraph = FXAI_MODE_DYNAGRAPH != 0;
inline constexpr bool kRecursion = FXAI_MODE_RECURSION != 0;
inline constexpr bool kConicGraphs = FXAI_MODE_CONICGRAPHS != 0;
inline constexpr bool kProgram = FXAI_MODE_PROGRAM != 0;
inline constexpr bool kFinancial = FXAI_MODE_FINANCIAL != 0;
inline constexpr bool kLink = FXAI_MODE_LINK != 0;
inline constexpr bool kGeometry = FXAI_MODE_GEOMETRY != 0;
inline constexpr bool kPicturePlot = FXAI_MODE_PICTUREPLOT != 0;
inline constexpr bool kPython = FXAI_MODE_PYTHON != 0;
inline constexpr bool kTypedFiles = FXAI_SERVICE_TYPEDFILES != 0;
inline constexpr bool kModeShell = FXAI_SERVICE_MODESHELL != 0;
inline constexpr bool kTextEdit = FXAI_SERVICE_TEXTEDIT != 0;
inline constexpr bool kStepTask = FXAI_SERVICE_STEPTASK != 0;
inline constexpr bool kGraph2D = FXAI_SERVICE_GRAPH2D != 0;
inline constexpr bool kBinding = FXAI_SERVICE_BINDING != 0;
inline constexpr bool kInterp = FXAI_SERVICE_INTERP != 0;
} // namespace fx::features
