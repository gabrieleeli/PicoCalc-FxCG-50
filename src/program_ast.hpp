#pragma once
// Token, lexer e albero comune dei linguaggi dei programmi (v1.20.0).
// Un solo lexer per C, C++, C# e Java (parole chiave per dialetto), il
// preprocessore di C e C++, un solo albero (nodi "larghi": pochi tipi di
// struttura, tanti generi) prodotto dai parser di ogni famiglia e letto
// dall'analisi comune. I nodi stanno in un NodePool e si liberano tutti
// insieme (nessuna distruzione ricorsiva).
#include "program_internal.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace fx::prog::ast {

using Sym = std::uint32_t;
class Symbols {
public:
    Symbols() { (void)intern(""); }
    Sym intern(const std::string& s);
    const std::string& operator[](Sym s) const { return names_[s]; }
    std::size_t size() const noexcept { return names_.size(); }
private:
    std::vector<std::string> names_;
    std::unordered_map<std::string, Sym> map_;
};

// ---------------------------------------------------------------- token
enum class T : std::uint8_t { End, Ident, Int, Float, Char, Str, Punct, Interp };
enum P : std::uint8_t {
    PNone, LParen, RParen, LBrace, RBrace, LBracket, RBracket, Semi, Comma, Dot, Arrow, Scope, Question, Colon,
    Plus, Minus, Star, Slash, Percent, Amp, Pipe, Caret, Tilde, Bang, Assign, Lt, Gt, Le, Ge, EqEq, Ne, AndAnd, OrOr,
    PlusPlus, MinusMinus, Shl, Shr, UShr, PlusEq, MinusEq, StarEq, SlashEq, PercentEq, AmpEq, PipeEq, CaretEq,
    ShlEq, ShrEq, UShrEq, Ellipsis, Hash, HashHash, QQ, QQEq, QDot, FatArrow, At, ArrowStar, DotStar, PCount
};
const char* punctText(P p) noexcept;
struct Pos {
    std::uint16_t file{0};
    std::uint16_t col{0};
    std::uint32_t line{0};
};
struct Token {
    T t{T::End};
    std::uint8_t flags{0};       // Int: 1 unsigned, 2 long, 4 long long; Float: 1 float; Str: 1 verbatim, 2 wide/char16
    P p{PNone};
    Pos pos;
    Sym sym{0};                  // Ident
    std::uint32_t str{0};        // Str/Interp: indice in Lexed::strings
    std::int64_t i{0};
    double d{0};
    bool spaceBefore{false};
    bool lineStart{false};
};

// Errore di analisi con la posizione (Syntax ERROR...). I nomi dei file
// vengono da setFailFiles() (impostato da compile()).
[[noreturn]] void fail(const std::string& msg, Pos pos, const std::vector<std::string>* files = nullptr);
void setFailFiles(const std::vector<std::string>* files);

struct Lexed {
    std::vector<Token> tokens;
    std::vector<std::string> strings;                    // letterali (byte UTF-8, escape gia' risolti)
};
// Opzioni del lexer per un dialetto.
struct LexOptions {
    Dialect dialect{Dialect::Cpp};
    std::uint16_t file{0};
};
// Analisi lessicale di un file (senza preprocessore). Lancia Error.
void lex(const std::string& source, const LexOptions& opt, Symbols& syms, Lexed& out);

// ---------------------------------------------------------------- preprocessore (C/C++)
struct PreContext {
    Dialect dialect{Dialect::Cpp};
    Symbols* syms{nullptr};
    std::vector<std::string>* files{nullptr};            // nomi dei file (indice = Pos::file)
    // Testo di un'inclusione: `quoted` per "x.h"; `from` = file che include.
    // Restituisce false se non trovato. `canonical` = percorso usato come nome.
    std::function<bool(const std::string& name, bool quoted, const std::string& from, std::string& text, std::string& canonical, bool& standard)> include;
    std::function<void()> poll;                          // AC e fetta di tempo durante l'analisi
    std::vector<std::string> standardHeaders;            // intestazioni standard incluse (per i nomi della libreria)
    std::size_t maxTokens{2000000};
};
// Preprocessa `source` (file `fileIndex`): include, macro, condizioni.
void preprocess(const std::string& source, std::uint16_t fileIndex, PreContext& ctx, Lexed& out);

// ---------------------------------------------------------------- albero
enum class K : std::uint8_t {
    // espressioni
    IntLit, FloatLit, CharLit, StrLit, BoolLit, NullLit, Name, Scope, Member, Arrow, Index, Call, Unary,
    PreInc, PreDec, PostInc, PostDec, Binary, Assign, Cond, Cast, SizeofType, SizeofExpr, New, NewArray,
    Delete, This, Base, Lambda, InitList, Is, As, Interp, Comma, AddrOf, Deref, Default, NullCond,
    TypeExpr, ThrowExpr, ObjInit,
    // istruzioni
    Block, ExprStmt, VarDecl, If, While, DoWhile, For, ForEach, Switch, Case, Break, Continue, Return,
    Goto, Label, Try, Catch, Throw, Empty, UsingStmt, LocalFunc,
    // dichiarazioni
    Declarator, Param, Func, Class, BaseSpec, Field, Property, Enum, Enumerator, Typedef, Namespace, Using,
    Template, TParam, MemberInit, Friend, Unit, Binding,
};
enum Mod : std::uint32_t {
    MStatic = 1u << 0, MConst = 1u << 1, MExtern = 1u << 2, MInline = 1u << 3, MVirtual = 1u << 4, MOverride = 1u << 5,
    MAbstract = 1u << 6, MSealed = 1u << 7, MPublic = 1u << 8, MPrivate = 1u << 9, MProtected = 1u << 10,
    MInternal = 1u << 11, MReadonly = 1u << 12, MExplicit = 1u << 13, MFriend = 1u << 14, MPartial = 1u << 15,
    MNew = 1u << 16, MRef = 1u << 17, MOut = 1u << 18, MParams = 1u << 19, MConstexpr = 1u << 20,
    MPure = 1u << 21, MDefault = 1u << 22, MDeleted = 1u << 23, MConstMethod = 1u << 24, MThisCall = 1u << 25,
    MBaseCall = 1u << 26, MFinal = 1u << 27, MSynchronized = 1u << 28, MDefaultMethod = 1u << 29, MVolatile = 1u << 30,
    MIn = 1u << 31,
};
struct TypeNode;
struct Node {
    K k{K::Empty};
    std::uint8_t op{0};          // operatore (P) o genere (classe/struct/interface/enum/union; foreach ref...)
    std::uint32_t mods{0};
    Pos pos;
    Sym sym{0};
    std::uint32_t str{0};
    std::int64_t ival{0};
    double dval{0};
    TypeNode* type{nullptr};
    TypeNode* type2{nullptr};    // Func: classe/namespace del nome (A::f); Is: tipo del modello
    Node* a{nullptr};
    Node* b{nullptr};
    Node* c{nullptr};
    Node* d{nullptr};
    std::vector<Node*> list;
    std::vector<Node*> list2;
    std::vector<TypeNode*> targs;
    std::uint8_t flags{0};       // StrLit: 0 C (array di char), 1 C#/Java; Lambda: cattura predefinita
};
enum class TF : std::uint8_t { Named, Pointer, Reference, Array, Func, Nullable, Auto };
struct TypeNode {
    TF f{TF::Named};
    Pos pos;
    Sym name{0};                 // Named: ultimo componente o tipo base ("int", "unsigned long")
    std::vector<Sym> qual;       // prefissi ("std", "System.Collections.Generic" a pezzi)
    bool global{false};          // ::nome
    bool isConst{false};
    bool builtin{false};         // tipo primitivo
    std::vector<TypeNode*> args; // argomenti generici o parametri di Func
    TypeNode* inner{nullptr};    // Pointer/Reference/Array/Nullable: elemento; Func: ritorno
    Node* size{nullptr};         // Array del C: dimensione (nullptr = [])
    std::uint8_t rank{1};        // Array di C#: 2 = [,]
};

class NodePool {
public:
    Node* node(K k, Pos pos) { nodes_.push_back(std::make_unique<Node>()); Node* n = nodes_.back().get(); n->k = k; n->pos = pos; return n; }
    TypeNode* type(TF f, Pos pos) { types_.push_back(std::make_unique<TypeNode>()); TypeNode* t = types_.back().get(); t->f = f; t->pos = pos; return t; }
    std::size_t count() const noexcept { return nodes_.size() + types_.size(); }
private:
    std::vector<std::unique_ptr<Node>> nodes_;
    std::vector<std::unique_ptr<TypeNode>> types_;
};

// Un'unita' di traduzione analizzata.
struct Unit {
    Dialect dialect{Dialect::Cpp};
    std::uint16_t file{0};
    std::int32_t library{-1};
    Node* root{nullptr};                                 // K::Unit: list = dichiarazioni
    std::vector<std::string> strings;                    // letterali
    std::vector<std::string> standardHeaders;            // C/C++: intestazioni standard incluse
    std::vector<std::string> usings;                     // C#: using
};
struct ParseContext {
    Symbols* syms{nullptr};
    NodePool* pool{nullptr};
    std::vector<std::string>* files{nullptr};
    std::function<void()> poll;
};
// Profondita' massima delle funzioni ricorsive del parser e dell'analisi
// (circa cinque per livello di parentesi: ~50 livelli di annidamento).
constexpr int ParseDepth = 256;
// Parser per famiglia (lanciano Error con la posizione).
void parseCFamily(const Lexed& lexed, Unit& unit, ParseContext& ctx);   // C e C++
void parseCSharp(const Lexed& lexed, Unit& unit, ParseContext& ctx);
void parseJava(const Lexed& lexed, Unit& unit, ParseContext& ctx);       // v1.21.0

} // namespace fx::prog::ast
