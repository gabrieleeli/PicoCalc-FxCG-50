#pragma once
#include <cstddef>
#include <cstdint>

namespace fx::help {
struct Entry { const char* symbol; const char* meaning; const char* example; };
struct Page { const char* title; const Entry* entries; std::uint8_t count; };

inline constexpr Entry arithmetic[] = {
 {"+  -", "Somma e differenza", "8 + 3 = 11"},
 {"*  /", "Prodotto e divisione", "8 * 3 = 24"},
 {"%", "Resto della divisione", "17 % 5 = 2"},
 {"!", "Fattoriale di un intero", "4! = 24"},
 {"( )", "Raggruppa le operazioni", "(2 + 3) * 4 = 20"},
 {"Ans", "Ultimo risultato calcolato", "Ans * 2"},
};
inline constexpr Entry powers_roots[] = {
 {"^", "Eleva alla potenza", "2^3 = 8"},
 {"x^2", "Quadrato di un numero", "5^2 = 25"},
 {"x^(-1)", "Reciproco di x", "4^(-1) = 0.25"},
 {"sqrt(x)  √", "Radice quadrata", "sqrt(25) = 5"},
 {"cbrt(x)  ∛", "Radice cubica", "cbrt(27) = 3"},
 {"rootN(x)", "Radice di indice N", "root3(27) = 3"},
};
inline constexpr Entry greek[] = {
 {"α β γ δ", "Prime lettere greche", "alpha + beta"},
 {"ε ζ η θ", "Epsilon, zeta, eta, theta", "theta spesso indica un angolo"},
 {"ι κ λ μ", "Iota, kappa, lambda, mu", "lambda indica spesso una lunghezza d'onda"},
 {"ν ξ ο π", "Nu, xi, omicron, pi", "pi = 3.14159..."},
 {"ρ σ τ υ", "Rho, sigma, tau, upsilon", "sigma indica spesso la deviazione"},
 {"φ χ ψ ω", "Phi, chi, psi, omega", "omega indica spesso una pulsazione"},
};
inline constexpr Entry greek_upper[] = {
 {"Α Β Γ Δ", "Alfa, beta, gamma, delta", "Alpha, Beta, Gamma, Delta"},
 {"Ε Ζ Η Θ", "Epsilon, zeta, eta, theta", "Theta può indicare un angolo"},
 {"Ι Κ Λ Μ", "Iota, kappa, lambda, mu", "Lambda è spesso un parametro"},
 {"Ν Ξ Ο Π", "Nu, xi, omicron, pi", "Pi maiuscola può indicare un prodotto"},
 {"Ρ Σ Τ Υ", "Rho, sigma, tau, upsilon", "Sigma maiuscola indica una somma"},
 {"Φ Χ Ψ Ω", "Phi, chi, psi, omega", "Omega può indicare una frequenza"},
};
inline constexpr Entry constants[] = {
 {"pi", "Rapporto circonferenza/diametro", "pi ~= 3.14159265"},
 {"e", "Base dei logaritmi naturali", "e ~= 2.71828183"},
 {"tau", "Giro completo: due pi greco", "tau = 2*pi"},
 {"phi", "Sezione aurea", "phi ~= 1.61803399"},
 {"sqrt2", "Radice di due", "sqrt2 ~= 1.41421356"},
 {"eulerGamma", "Costante di Eulero-Mascheroni", "eulerGamma ~= 0.57721"},
};
inline constexpr Entry functions[] = {
 {"sin  cos  tan", "Funzioni trigonometriche", "sin(30) in modalita DEG"},
 {"asin  acos  atan", "Trigonometriche inverse", "asin(0.5)"},
 {"sinh  cosh  tanh", "Funzioni iperboliche", "sinh(1)"},
 {"ln(x)", "Logaritmo in base e", "ln(e) = 1"},
 {"log(x)", "Logaritmo in base 10", "log(100) = 2"},
 {"exp(x)", "Esponenziale: e elevato a x", "exp(1) = e"},
};
inline constexpr Entry advanced_functions[] = {
 {"log(a,b)", "Logaritmo di b in base a", "log(2,8) = 3"},
 {"root(n,x)", "Radice n-esima di x", "root(5,32) = 2"},
 {"abs(x)", "Valore assoluto", "abs(-7) = 7"},
 {"floor / ceil", "Arrotonda per difetto/eccesso", "floor(3.8) = 3"},
 {"round(x)", "Arrotonda all'intero", "round(3.6) = 4"},
 {"min / max", "Minimo e massimo", "max(3,8) = 8"},
};
inline constexpr Entry calculus[] = {
 {"d/dx", "Derivata: tasso istantaneo", "d/dx(x^2) = 2x"},
 {"integral", "Integrale: accumulo/area", "integral(x^2,0,3) = 9"},
 {"sum", "Somma di termini indicizzati", "sum(k,1,5) = 15"},
 {"product", "Prodotto di una sequenza", "1 * 2 * 3 * 4 = 24"},
 {"lim", "Valore a cui tende una funzione", "lim x->0 sin(x)/x"},
 {"infinity", "Quantita senza limite superiore", "x -> infinity"},
};
inline constexpr Entry combinatorics[] = {
 {"n!", "Permutazioni di n elementi", "5! = 120"},
 {"nCr", "Combinazioni senza ordine", "5 nCr 2 = 10"},
 {"nPr", "Disposizioni con ordine", "5 nPr 2 = 20"},
 {"rand", "Numero casuale", "rand(0,1)"},
 {"%", "Percentuale su cento", "25% = 0.25"},
 {"recip(x)", "Inverso moltiplicativo", "recip(4) = 0.25"},
};
inline constexpr Entry sets_logic[] = {
 {"in", "Appartenenza a un insieme", "x in R"},
 {"subset", "Inclusione tra insiemi", "A subset B"},
 {"union", "Unione di due insiemi", "A union B"},
 {"intersection", "Elementi comuni", "A intersection B"},
 {"AND OR NOT", "Operatori della logica", "P AND Q"},
 {"=>  <=>", "Implica / se e solo se", "P => Q"},
};
inline constexpr Entry statistics[] = {
 {"mean", "Media aritmetica", "(2 + 4 + 6) / 3 = 4"},
 {"sigma", "Deviazione standard", "Misura la dispersione"},
 {"variance", "Varianza: scarto quadratico", "variance = sigma^2"},
 {"median", "Valore centrale ordinato", "1, 3, 9 -> 3"},
 {"nCr", "Numero di combinazioni", "5 nCr 2 = 10"},
 {"nPr", "Numero di disposizioni", "5 nPr 2 = 20"},
};
inline constexpr Entry graphing[] = {
 {"x  y", "Coordinate cartesiane", "Punto (2, 3)"},
 {"f(x)", "Valore della funzione in x", "f(x) = x^2"},
 {"m", "Coefficiente angolare", "y = m*x + q"},
 {"q", "Intercetta sull'asse y", "y = 2*x + 1"},
 {"r  theta", "Coordinate polari", "(r, theta)"},
 {"t", "Parametro delle curve", "x(t), y(t)"},
};
inline constexpr Entry complex_matrix[] = {
 {"i", "Unita' immaginaria: i^2 = -1", "3 + 2*i"},
 {"Re(z)", "Parte reale di z", "Re(3 + 2*i) = 3"},
 {"Im(z)", "Parte immaginaria di z", "Im(3 + 2*i) = 2"},
 {"det(A)", "Determinante di una matrice", "det(A)"},
 {"A^T", "Matrice trasposta", "Righe e colonne si scambiano"},
 {"|v|", "Modulo/lunghezza del vettore", "|(3,4)| = 5"},
};
inline constexpr Entry units[] = {
 {"deg", "Angoli in gradi: giro 360", "sin(90) = 1 in DEG"},
 {"rad", "Angoli in radianti: giro 2*pi", "sin(pi/2) = 1 in RAD"},
 {"gra", "Angoli in gradi centesimali", "100 gra = 90 gradi"},
 {"m  cm  km", "Unita' di lunghezza", "1 km = 1000 m"},
 {"s  min  h", "Unita' di tempo", "1 h = 3600 s"},
 {"kg  g", "Unita' di massa", "1 kg = 1000 g"},
};
inline constexpr Entry notation[] = {
 {"~=", "Approssimativamente uguale", "pi ~= 3.14159"},
 {"+/-", "Piu' o meno: due possibilita'", "x = 3 +/- 1"},
 {"proportional", "Proporzionalita' tra grandezze", "y proportional x"},
 {"!", "Fattoriale", "4! = 24"},
 {"%", "Percentuale: parte su cento", "25% = 0.25"},
 {"|x|", "Distanza da zero, sempre positiva", "abs(-4) = 4"},
};
inline constexpr Entry display_formats[] = {
 {"Norm 1", "Formato decimale automatico", "Mostra numeri di uso comune"},
 {"Norm 2", "Formato con soglia piu' bassa", "Mostra piu' spesso notazione E"},
 {"Fix", "Numero fisso di decimali", "Fix 3: 1.23456 -> 1.235"},
 {"Sci", "Notazione scientifica", "12300 -> 1.23 x 10^4"},
 {"Eng", "Esponente multiplo di tre", "12000 -> 12 x 10^3"},
 {"Frac", "Visualizzazione frazionaria", "0.75 -> 3/4"},
};
inline constexpr Entry run_matrix_v17[] = {
 {"=  ≠  <  >", "Confronti: 1 se vero, 0 se falso", "3 > 2 = 1"},
 {"≤  ≥", "Minore o uguale, maggiore o uguale", "2 <= 2 = 1"},
 {"and or xor not", "Operatori logici (OPTN LOGIC)", "1 and 0 = 0"},
 {"Int÷  Rmdr", "Quoziente e resto interi (OPTN CALC)", "107 Rmdr 7 = 2"},
 {"a b/c", "Frazione mista (SHIFT F1), SHIFT F5 la converte", "2 1/3 = 7/3"},
 {"0.1(6)", "Numero periodico (MATH periodo)", "0.1(6) = 1/6"},
};
inline constexpr Page pages[] = {
 {"ARITMETICA", arithmetic, 6}, {"POTENZE E RADICI", powers_roots, 6},
 {"LETTERE GRECHE", greek, 6}, {"COSTANTI MATEMATICHE", constants, 6},
 {"FUNZIONI", functions, 6}, {"CALCOLO", calculus, 6},
 {"INSIEMI E LOGICA", sets_logic, 6}, {"STATISTICA", statistics, 6},
 {"GRAFICI", graphing, 6}, {"COMPLESSI / MATRICI", complex_matrix, 6},
 {"ALTRE FUNZIONI", advanced_functions, 6}, {"COMBINATORIA", combinatorics, 6},
 {"UNITA' DI MISURA", units, 6}, {"NOTAZIONE", notation, 6},
 {"GRECHE MAIUSCOLE", greek_upper, 6}, {"FORMATI RISULTATO", display_formats, 6},
 {"CONFRONTI E FRAZIONI", run_matrix_v17, 6},
};
inline constexpr std::size_t PageCount = sizeof(pages) / sizeof(pages[0]);
} // namespace fx::help
