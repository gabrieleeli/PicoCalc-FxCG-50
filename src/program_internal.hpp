#pragma once
// Bytecode dell'interprete dei programmi (v1.15.0, F9): condiviso dal
// compilatore (program_lang.cpp) e dalla macchina (program_vm.cpp).
#include "program.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace fx::prog {

enum class Type : std::uint8_t { Void, Int, Char, Double, Bool, String, Auto, Array };

enum class Op : std::uint8_t {
    PushI, PushD, PushS, PushB, PushVoid,            // a: indice nella tabella delle costanti (PushB: valore)
    LoadG, StoreG, LoadL, StoreL,                    // a: slot; Store* toglie il valore dalla pila
    LoadIdx, StoreIdx,                               // [arr idx] -> v ; [arr idx v] -> (niente)
    LoadIdx2, StoreIdx2,                             // [arr r c] -> v ; [arr r c v]
    IncL, IncG,                                      // a: slot, b: bit0 = post, bit1 = decremento; spinge il valore
    IncIdx, IncIdx2,                                 // [arr idx] / [arr r c] -> valore
    CompIdx, CompIdx2,                               // b: operatore; [arr idx rhs] -> nuovo valore
    Conv,                                            // b: Type
    Add, Sub, Mul, Div, Mod, Neg, Not, BitNot, BitAnd, BitOr, BitXor, Shl, Shr,
    Eq, Ne, Lt, Le, Gt, Ge,
    Jmp, Jz, Jnz, JzKeep, JnzKeep,                   // a: destinazione
    Pop, Dup,
    Call,                                            // a: funzione, b: argomenti
    Ret,                                             // b: 1 se c'e' un valore
    Builtin,                                         // a: funzione predefinita, b: argomenti
    NewArr,                                          // a: Type degli elementi, b: dimensioni (1 o 2); [n] o [r c]
    ArrLit,                                          // a: Type, b: elementi: [v1..vn] -> array
    ReadTok, ReadLine, ReadChar,                     // input (la macchina aspetta se non c'e')
    Switch,                                          // a: indice della tabella dei casi; [v] -> salto
    Halt,
};

// Funzioni predefinite (Builtin).
enum class Fn : std::uint8_t {
    Print, Printf, Puts, Putchar, PrintLine, Format,  // Print: valori come li stampa il linguaggio
    Sin, Cos, Tan, Asin, Acos, Atan, Atan2, Sqrt, Pow, Exp, Log, Log10, Fabs, Abs, Floor, Ceil, Round, Fmod, Min, Max, Sinh, Cosh, Tanh, Trunc,
    Strlen, Strcmp, Strcat, Substr, IndexOf, ToUpper, ToLower,
    ToInt, ToDouble, ToStr, ParseInt, ParseDouble,
    Rand, Srand, Exit,
    FxCalc, FxGetVar, FxSetVar, FxCls, FxViewWindow, FxPlot, FxLine, FxCircle, FxText, FxGraphY, FxVertical, FxHorizontal,
    FxLocate, FxClrText, FxGetKey, FxCommand,
    Count
};

struct Instr {
    Op op;
    std::uint8_t b{0};
    std::int32_t a{0};
};
struct Where { std::int32_t line{0}, column{0}; };

struct Function {
    std::string name;
    std::int32_t entry{-1};
    std::vector<Type> params;
    std::vector<std::string> localNames;             // slot -> nome (fx::calc lega le variabili numeriche)
    std::vector<Type> localTypes;
    Type ret{Type::Void};
    int line{0}, column{0};
};
struct SwitchTable {
    std::vector<std::pair<std::int64_t, std::int32_t>> cases;
    std::int32_t fallback{-1};                       // default (o fine dello switch)
};

class Program {
public:
    Dialect dialect{Dialect::Cpp};
    std::vector<Instr> code;
    std::vector<Where> where;
    std::vector<std::int64_t> ints;
    std::vector<double> doubles;
    std::vector<std::string> strings;
    std::vector<Function> functions;
    std::vector<std::string> globalNames;
    std::vector<Type> globalTypes;
    std::vector<SwitchTable> switches;
    std::int32_t init{0};                            // codice delle variabili globali, poi Call main e Halt
    std::int32_t mainFunction{-1};
};

} // namespace fx::prog
