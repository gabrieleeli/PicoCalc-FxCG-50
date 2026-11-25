#pragma once
// Analisi e generazione delle istruzioni (v1.20.0): interno del compilatore,
// diviso in program_compile.cpp (tipi, namespace, classi, template, funzioni),
// program_compile_stmt.cpp (istruzioni) e program_compile_expr.cpp
// (espressioni, chiamate, overload, conversioni). Lo stesso analizzatore
// serve C, C++ e C# (e Java dalla v1.21.0): le differenze di linguaggio sono
// regole (conversioni ammesse, promozioni, ricerca dei nomi), non codice
// separato. Ricorsione sull'albero limitata dalla profondita' del parser.
#include "program_ast.hpp"
#include "program_natives.hpp"
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace fx::prog {

struct TemplateDef {
    ast::Node* node{nullptr};                 // K::Template
    std::string qualified;                    // nome con i namespace
    std::vector<std::string> nsPath;          // namespace della dichiarazione
    ClassInfo* owner{nullptr};                // template membro
    Dialect lang{Dialect::Cpp};
    std::int32_t library{-1};
    std::uint16_t unitIndex{0};
    std::map<std::string, ClassInfo*> classes;     // istanze di classe per argomenti
    std::map<std::string, std::vector<Function*>> functions;   // istanze di funzione
    std::vector<ast::Node*> outOfClass;       // definizioni fuori dalla classe (Pila<T>::f)
};
struct GlobalVar {
    std::string name;
    TypeP type{nullptr};
    std::int32_t slot{-1};
    bool isConst{false};
    bool hasConstValue{false};
    Value constValue;                         // costanti di compilazione (enum, const int)
};
struct OverloadSet;
// Voce di un namespace (o della radice).
struct NsEntry {
    enum Kind : std::uint8_t { None, Class, Template, Functions, Global, Typedef, Namespace, EnumConst, NativeType } kind{None};
    ClassInfo* cls{nullptr};
    TemplateDef* tmpl{nullptr};
    std::vector<Function*> fns;
    std::vector<TemplateDef*> fnTemplates;
    GlobalVar* global{nullptr};
    TypeP type{nullptr};
    struct Namespace* ns{nullptr};
    std::int64_t enumValue{0};
    const NativeTypeEntry* native{nullptr};
};
struct Namespace {
    std::string name;                         // qualificato ("geo::piano", "Demo.Utili")
    Namespace* parent{nullptr};
    std::unordered_map<std::string, NsEntry> members;
    std::vector<Namespace*> usings;           // using namespace
    std::vector<std::unique_ptr<Namespace>> children;
};

// Variabile locale durante la compilazione.
struct LocalVar {
    std::string name;
    TypeP type{nullptr};
    std::int32_t slot{-1};
    bool boxed{false};                        // catturata da una lambda di C# (Box)
    bool isRef{false};                        // riferimento C++ / ref C#: lo slot contiene un puntatore
    bool isConst{false};
    bool needsKill{false};                    // oggetto per valore da liberare all'uscita
    bool needsDtor{false};                    // C++: distruttore all'uscita
    bool moved{false};                        // restituito per valore (nessun distruttore)
    std::int32_t infoIndex{-1};               // in Function::locals
    std::int32_t global{-1};                  // static locale: slot globale (alias)
    bool hasConstValue{false};
    Value constValue;
};
struct SwitchBuild {
    bool strings{false};
    std::vector<std::pair<std::int64_t, std::int32_t>> cases;
    std::vector<std::pair<std::string, std::int32_t>> strCases;
    std::int32_t defaultTarget{-1};
};
struct BlockScope {
    std::vector<LocalVar> vars;
    std::int32_t cleanupStart{-1};            // prima istruzione coperta dal gestore dei distruttori
    bool transparent{false};
};
// Destinazione di break/continue.
struct LoopCtx {
    std::vector<std::int32_t> breaks, continues;
    std::size_t scopeDepth{0};                // scope fuori dal ciclo
    std::int32_t finallyDepth{0};
    bool isSwitch{false};
    ast::Sym label{0};                        // Java: etichetta del ciclo
};
struct FinallyCtx {
    ast::Node* block{nullptr};                // corpo del finally da copiare all'uscita
    std::size_t scopeDepth{0};
};

// Descrizione di un'espressione compilata (valore o luogo).
struct EV {
    enum Cat : std::uint8_t { RValue, Local, LocalBox, Global, Field, Elem, Elem2, Ptr, Prop, Indexer, Cap, Void, TypeName, FuncGroup, NsName } cat{RValue};
    TypeP type{nullptr};
    std::int32_t slot{0};                     // Local/Global/Field/Cap
    std::int32_t components{0};               // valori sulla pila per il luogo
    PropertyInfo* prop{nullptr};
    Function* getter{nullptr};
    Function* setter{nullptr};
    bool lvalue{false};
    bool temp{false};                         // oggetto nuovo (nessuna copia necessaria)
    bool isConst{false};
    bool isNull{false};                       // letterale null/NULL/nullptr
    bool isStringLit{false};
    bool hasConst{false};                     // costante di compilazione
    Value constValue;
    // gruppo di funzioni (nome di funzione non chiamato): candidati
    std::vector<Function*> fns;
    std::vector<std::int32_t> natives;
    std::vector<TemplateDef*> templates;
    bool hasReceiver{false};                  // metodo con il ricevente sulla pila
    bool superCall{false};                    // base.f() / Base::f(): chiamata non virtuale
    ClassInfo* typeCls{nullptr};              // TypeName: classe
    Namespace* ns{nullptr};                   // NsName
    LocalVar* local{nullptr};
};

// Valori sulla pila che descrivono un luogo (oggetto, indice...).
inline std::int32_t componentsOf(const EV& e) {
    switch (e.cat) {
    case EV::Field: case EV::Ptr: return 1;
    case EV::Elem: return 2;
    case EV::Elem2: return 3;
    case EV::Prop: return e.prop && e.prop->isStatic ? 0 : 1;
    case EV::Indexer: return e.components;
    default: return 0;
    }
}

class Compiler {
public:
    Compiler(Program& prog, std::vector<ast::Unit>& units, ast::Symbols& syms, ast::NodePool& pool, std::function<void()> poll);
    void run();

    // ------------------------------------------------------------ tipi (program_compile.cpp)
    TypeP intern(const TypeRef& t);
    TypeP prim(TK k);
    TypeP ptrTo(TypeP t);
    TypeP refTo(TypeP t);
    TypeP arrayOf(TypeP elem, std::int32_t size, std::uint8_t rank = 1);
    TypeP nativeOf(NK nk, std::vector<TypeP> args);
    TypeP funcOf(TypeP ret, std::vector<TypeP> params);
    TypeP nullableOf(TypeP t);
    TypeP classType(ClassInfo* c);
    TypeP stringType(Dialect d);
    TypeP charType(Dialect d) { return d == Dialect::C || d == Dialect::Cpp ? tChar8_ : tChar16_; }
    TypeP boolResult(Dialect d) { return d == Dialect::C ? tI32_ : tBool_; }
    TypeP sizeType() { return tU64_; }
    TypeP resolveType(ast::TypeNode* t, bool allowVoid = true);
    std::int64_t sizeOf(TypeP t, ast::Pos at);
    std::int64_t alignOf(TypeP t);
    bool isValueType(TypeP t) const;
    bool needsDtor(TypeP t) const;
    TypeP stripRef(TypeP t) const { return t && t->k == TK::Ref ? t->elem : t; }
    std::string typeName(TypeP t) const;

    // ------------------------------------------------------------ errori
    [[noreturn]] void error(const std::string& msg, ast::Pos at) const;
    [[noreturn]] void error(const std::string& msg, const ast::Node* n) const { error(msg, n ? n->pos : ast::Pos{}); }
    const std::string& name(ast::Sym s) const { return syms_[s]; }

    // ------------------------------------------------------------ dichiarazioni (program_compile.cpp)
    void declareUnit(ast::Unit& u, std::uint16_t unitIndex);
    void declareIn(Namespace* ns, ast::Node* n, ClassInfo* owner);
    ClassInfo* declareClass(Namespace* ns, ast::Node* n, ClassInfo* outer);
    void completeClass(ClassInfo* c);
    void layoutClass(ClassInfo* c);
    Function* declareFunction(Namespace* ns, ast::Node* n, ClassInfo* owner, bool friendFn = false);
    void compileBodies();
    void compileFunctionBody(Function* f, ast::Node* body, ast::Node* decl);
    ClassInfo* instantiateClass(TemplateDef* t, const std::vector<TypeP>& args, ast::Pos at);
    Function* instantiateFunction(TemplateDef* t, const std::vector<TypeP>& args, ast::Pos at);
    Namespace* namespaceFor(Namespace* from, const std::string& qualified, bool create);
    NsEntry* lookupScoped(Namespace* ns, const std::string& n);
    NsEntry* lookupGlobalName(const std::string& n, Namespace* from, ClassInfo* cls);
    ClassInfo* nativeClass(const std::string& qualified, Dialect d);
    ClassInfo* exceptionClass(Dialect d, const std::string& name);
    void setupNativeClasses();
    void setupEntry();
    std::string qualify(Namespace* ns, const std::string& n) const;

    // ------------------------------------------------------------ funzioni (program_compile_stmt.cpp)
    void beginFunction(Function* f);
    void endFunction();
    void statement(ast::Node* n);
    void block(ast::Node* n, bool newScope = true);
    void declareLocalVars(ast::Node* n);
    LocalVar& addLocal(const std::string& nm, TypeP t, ast::Pos at, bool boxed = false);
    LocalVar* findLocal(const std::string& nm, std::size_t* depth = nullptr);
    void pushScope(bool transparent = false);
    void popScope(bool emitCleanup = true);
    void emitScopeCleanup(BlockScope& s, const LocalVar* except = nullptr);
    void emitExitTo(std::size_t depth, std::int32_t finallyDepth);
    void ifStmt(ast::Node* n);
    void loopStmt(ast::Node* n);
    void forEachStmt(ast::Node* n);
    void switchStmt(ast::Node* n);
    void tryStmt(ast::Node* n);
    void returnStmt(ast::Node* n);
    void usingStmt(ast::Node* n);
    void initVariable(LocalVar& v, ast::Node* decl, ast::Pos at);
    void defaultInit(TypeP t, ast::Pos at);
    void zeroValue(TypeP t, ast::Pos at);
    void constructObject(ClassInfo* c, const std::vector<ast::Node*>& args, ast::Pos at, bool onStack);
    void initFromList(TypeP t, ast::Node* list, ast::Pos at);

    // ------------------------------------------------------------ espressioni (program_compile_expr.cpp)
    EV expr(ast::Node* n);
    TypeP rvalue(ast::Node* n, TypeP want = nullptr, bool explicitCast = false);
    void discard(ast::Node* n);
    void load(EV& e, ast::Pos at);
    void store(EV& e, bool keep, ast::Pos at);
    void convert(TypeP from, TypeP to, ast::Pos at, bool explicitCast = false, const EV* src = nullptr);
    int convCost(TypeP from, TypeP to, const EV* src) const;   // 0 esatta, 1 promozione, 2 conversione, -1 impossibile
    TypeP arithmetic(TypeP a, TypeP b, ast::Pos at);
    TypeP condition(ast::Node* n);
    EV name(ast::Node* n);
    EV member(ast::Node* n, bool arrow);
    EV scope(ast::Node* n);
    EV index(ast::Node* n);
    EV call(ast::Node* n);
    EV binary(ast::Node* n);
    EV unary(ast::Node* n);
    EV assign(ast::Node* n);
    EV incdec(ast::Node* n, bool pre, bool dec);
    EV cast(ast::Node* n);
    EV newExpr(ast::Node* n);
    EV lambda(ast::Node* n, TypeP want);
    EV interpolation(ast::Node* n);
    EV objInit(ast::Node* n);
    EV callFunction(Function* f, const std::vector<ast::Node*>& args, ast::Pos at, bool receiverOnStack, bool virtualCall, ClassInfo* via = nullptr);
    EV callNative(std::int32_t idx, const std::vector<ast::Node*>& args, ast::Pos at, bool receiverOnStack, TypeP receiver);
    EV callOverloads(const std::vector<Function*>& fns, const std::vector<std::int32_t>& natives, const std::vector<TemplateDef*>& tmpls,
                     const std::vector<ast::Node*>& args, const std::vector<TypeP>& targs, ast::Pos at, bool receiverOnStack, TypeP receiver,
                     bool superCall, const std::string& what);
    EV callValue(EV fnValue, const std::vector<ast::Node*>& args, ast::Pos at);
    bool operatorCall(ast::P op, ast::Node* a, ast::Node* b, ast::Pos at, EV& out);
    EV methodOn(EV& recv, const std::string& m, ast::Node* call, const std::vector<ast::Node*>* args, ast::Pos at);
    std::vector<TypeP> argTypesFor(const std::vector<ast::Node*>& args);
    TypeP parseSig(const char* sig, std::vector<TypeP>& params, bool& variadic, TypeP receiver, Dialect d, std::vector<bool>* refs = nullptr);
    void emitToString(TypeP t, ast::Pos at);
    void emitStringConcat(ast::Node* n);
    EV memberOfType(EV& base, TypeP t, const std::string& m, ast::Node* n);
    EV fieldOf(TypeP objType, const FieldInfo* f, ast::Pos at);
    const FieldInfo* findField(ClassInfo* c, const std::string& n, ClassInfo** owner = nullptr);
    PropertyInfo* findProperty(ClassInfo* c, const std::string& n, bool indexer = false);
    std::vector<Function*> findMethods(ClassInfo* c, const std::string& n, bool* found = nullptr);
    std::vector<TemplateDef*> methodTemplates(ClassInfo* c, const std::string& n);
    bool canAccess(Access a, ClassInfo* owner) const;
    void checkAccess(Access a, ClassInfo* owner, const std::string& what, ast::Pos at) const;
    void derefPointerValue(EV& e, ast::Pos at);
    EV addressOf(ast::Node* n);
    void pushInt(std::int64_t v, VK k);
    void pushDouble(double d, VK k);
    void pushString(const std::string& s, Dialect d, bool cppValue);
    bool constInt(ast::Node* n, std::int64_t& out);
    void copyIfLvalue(EV& e, TypeP t, ast::Pos at);

    // ------------------------------------------------------------ emissione
    std::int32_t emit(Op op, std::int32_t a = 0, std::uint8_t b = 0, std::uint16_t c = 0);
    std::int32_t here() const;
    void patch(std::int32_t at, std::int32_t target);
    void setPos(ast::Pos p) { pos_ = p; }
    void markStatement();
    std::int32_t constIndex(std::int64_t v);
    std::int32_t constDouble(double v);
    std::int32_t constString(const std::string& bytes, bool wide);
    std::int32_t selector(const std::string& n, std::size_t arity);
    Function* newFunction(const std::string& nm, Dialect d, ClassInfo* owner);
    // C/C++: una funzione della libreria (sqrt, sin...) usata come puntatore a funzione
    bool constFromExpr(ast::Node* n, std::int64_t& out);
    TypeP lambdaType(ast::Node* lambda);
    Function* nativeWrapper(const std::vector<std::int32_t>& natives, ast::Pos at);
    ClassInfo* newClass(const std::string& nm, Dialect d);

    // ------------------------------------------------------------ stato
    Program& prog_;
    std::vector<ast::Unit>& units_;
    ast::Symbols& syms_;
    ast::NodePool& pool_;
    std::function<void()> poll_;
    std::unique_ptr<Namespace> root_;
    std::map<std::string, TypeP> typeIndex_;
    TypeP tVoid_{}, tBool_{}, tChar8_{}, tChar16_{}, tI8_{}, tU8_{}, tI16_{}, tU16_{}, tI32_{}, tU32_{}, tI64_{}, tU64_{}, tF32_{}, tF64_{};
    TypeP tNull_{}, tDyn_{}, tCppStr_{}, tCsStr_{}, tVoidPtr_{}, tCStr_{}, tObject_{};
    TypeP tAnyIter_{};                                  // firme native: un iteratore o un puntatore qualsiasi
    TypeP tArg_[3] = {};                       // firme native: "arg0"... = tipo dell'argomento
    TypeP tElem_[3] = {};                      // firme native: "E0"... = tipo dell'elemento dell'argomento
    TypeP substMarkers(TypeP t, const std::vector<EV>& probes);
    std::vector<std::unique_ptr<TemplateDef>> templates_;
    std::vector<std::unique_ptr<GlobalVar>> globals_;
    struct Pending { Function* fn; ast::Node* decl; ast::Node* body; Namespace* ns; Dialect lang; std::uint16_t unit; std::vector<std::pair<std::string, TypeP>> tbind; };
    std::vector<Pending> pending_;
    std::vector<std::pair<ast::Node*, GlobalVar*>> globalInits_;
    struct GlobalInit { ast::Node* decl; GlobalVar* g; Namespace* ns; Dialect lang; std::uint16_t unit; ClassInfo* cls; };
    std::vector<GlobalInit> inits_;
    std::vector<ClassInfo*> classOrder_;
    std::map<std::string, ClassInfo*> nativeClasses_;
    std::unordered_map<ClassInfo*, ast::Node*> classNodes_;
    std::unordered_map<ClassInfo*, Namespace*> classNs_;
    std::unordered_map<ClassInfo*, Namespace*> classScope_;   // namespace dei membri statici e dei tipi annidati
    std::unordered_map<Function*, ast::Node*> fnDecls_;
    // contesto corrente
    Dialect lang_{Dialect::Cpp};
    Namespace* ns_{nullptr};
    ClassInfo* cls_{nullptr};
    std::uint16_t unit_{0};
    std::int32_t library_{-1};
    std::vector<std::pair<std::string, TypeP>> tbind_;        // parametri di template legati
    std::vector<std::string> headers_;                        // intestazioni standard dell'unita'
    std::vector<std::string> usings_;                         // using di C#
    Function* fn_{nullptr};
    std::vector<BlockScope> scopes_;
    std::vector<LoopCtx> loops_;
    std::vector<FinallyCtx> finallies_;
    std::map<std::string, std::int32_t> labels_;
    std::vector<std::pair<std::int32_t, std::pair<std::string, ast::Pos>>> gotos_;
    std::vector<std::int32_t> freeSlots_;
    std::int32_t retSlot_{-1};
    ast::Pos pos_;
    bool stmtStart_{false};
    int depth_{0};
    // lambda in compilazione: catture
    struct LambdaCtx {
        Function* fn{nullptr};
        std::vector<std::pair<std::string, std::int32_t>> captured;   // nome -> indice di cattura
        std::vector<EV> sources;                                      // come caricare il valore nel creatore
        std::vector<bool> byRef;
        std::vector<BlockScope> outerScopes;                               // scope della funzione che la contiene
        Function* outerFn{nullptr};
        int defaultCapture{0};
        std::vector<std::string> explicitRef, explicitVal;
        bool captureThis{false};
        ClassInfo* outerCls{nullptr};
    };
    std::vector<LambdaCtx*> lambdas_;
    // seconda passata delle dichiarazioni (funzioni, variabili, typedef, using)
    struct LaterDecl { ast::Node* node; Namespace* ns; ClassInfo* owner; Dialect lang; std::uint16_t unit; std::int32_t library; };
    std::vector<LaterDecl> laterDecls_;
    struct OutOfClass { TemplateDef* tmpl; Namespace* ns; };
    std::vector<OutOfClass> pendingOutOfClass_;
    std::unordered_map<ClassInfo*, std::vector<ast::Node*>> partialParts_;
    struct UnitCtx { Dialect lang; std::uint16_t unit; std::int32_t library; Namespace* ns; };
    std::unordered_map<ClassInfo*, UnitCtx> classUnit_;
    std::vector<std::pair<Function*, ast::Node*>> outOfClassBodies_;
    std::set<ClassInfo*> completing_;
    std::unordered_map<ClassInfo*, std::vector<std::pair<std::string, TypeP>>> classTbind_;
    std::unordered_map<ClassInfo*, std::vector<ast::Node*>> fieldInits_;
    std::set<ClassInfo*> privateBase_;
    std::unordered_map<ClassInfo*, std::set<Function*>> friendFns_;
    std::unordered_map<ClassInfo*, std::set<std::string>> friendNames_;
    std::set<Function*> deletedFns_, defaultedFns_;
    std::vector<Function*> calledUndefined_;
    int instantiations_{0};
    bool initPending_{false};
    bool pendingBody(Function* f) const;
    void ensureFieldInit(ClassInfo* c);
    void emitFieldInits(ClassInfo* c, ast::Node* ctorDecl);
    void collectCaptures(ast::Node* body);
    std::uint32_t lastLine(ast::Node* n) const;
    void emitCtorPrologue(Function* f, ast::Node* decl);
    EV exprType(ast::Node* n);
    void nativeBaseCtor(ClassInfo* base, ast::Node* mi);
    void constructNative(TypeP t, const std::vector<ast::Node*>& args, ast::Pos at);
    std::vector<std::int32_t> nativeCtors(TypeP t);
    void listToNative(TypeP t, ast::Node* list, ast::Pos at);
    void fillArray(TypeP t, ast::Node* list);
    void bindingDecl(ast::Node* n);
    void bindNames(ast::Node* n, TypeP t, std::int32_t slot, bool byRef);
    void staticLocal(ast::Node* d, TypeP t, const std::string& nm);
    bool jumpStatement(ast::Node* s) const;
    bool defaultConstructible(TypeP t);                 // C++: un valore "zero" si puo' creare senza argomenti
    bool hasCleanups() const;
    void disposeValue(TypeP t, ast::Pos at);
    EV addressOfEV(EV& e, ast::Pos at);
    std::string ownerKey(TypeP t) const;
    std::vector<std::int32_t> nativeFunctions(const std::string& name, bool qualifiedStd);
    std::vector<std::int32_t> nativeMembers(const std::string& owner, const std::string& name, std::uint8_t needFlags, std::uint8_t avoidFlags);
    bool headerVisible(const char* scope) const;
    bool nativeConstant(const std::string& name, EV& out, ast::Pos at);
    std::vector<SwitchBuild*> switchCases_;
    ast::Sym pendingLabel_{0};
    std::vector<std::int32_t> catchSlots_;
    std::unordered_map<Namespace*, std::vector<std::string>> localTypedefs_;
    std::vector<std::pair<std::int32_t, TypeP>> usingVars_;
    bool popScopeKeep_{false};
    struct StaticLocal { std::string name; GlobalVar* g; std::size_t depth; };
    std::vector<StaticLocal> staticLocals_;
    TypeP lambdaReturnType_{nullptr};
    std::unordered_map<const ast::Node*, EV> probeCache_;
    bool probing_{false};
    std::int32_t convTemp_{-1};
    TypeP convTempType_{nullptr};
    bool implicitThis_{false};
    std::string staticClass_;
    TemplateDef* templateName_{nullptr};
    std::uint32_t lambdaCount_{0};
    std::unordered_map<std::int32_t, Function*> nativeWrappers_;   // nativo -> funzione che lo chiama
    EV assignImpl(ast::Node* n, bool needValue);
    EV nullCond(ast::Node* n);
    EV conditional(ast::Node* n);
    EV isExpr(ast::Node* n);
    bool captureName(const std::string& nm, ast::Pos at, EV& out);
    EV captureThis(ast::Pos at);
    GlobalVar* globalFor(std::int32_t slot);
    EV entryValue(NsEntry* ent, const std::string& nm, ast::Node* n);
    std::string scopePath(const ast::Node* n) const;
    EV staticMember(EV& left, const std::string& member, ast::Node* n);
    std::pair<std::string, std::string> csTypeNames(TypeP t);    // Name e FullName di System.Type
    EV nativeMember(const std::vector<std::int32_t>& nat, TypeP t, const std::string& m, ast::Node* n);
    std::string headerHint(const std::string& nm) const;
    EV construct(TypeP t, const std::vector<ast::Node*>& args, ast::Pos at, bool braces);
    void emitArrayCtorLoop(ClassInfo* c, ast::Pos at);
    bool deduce(ast::TypeNode* pt, TypeP at, std::vector<std::pair<std::string, TypeP>>& bind, const std::vector<std::string>& names);
    void emitArg(ast::Node* arg, const EV& probe, TypeP param, bool byRef, ast::Pos at);
    void emitVarArg(ast::Node* arg, ast::Pos at);
    EV callBuiltinOnStack(const char* owner, const char* name, const char* sig, int argc, TypeP t);
    void processLater(const LaterDecl& d);
    void declareVariables(Namespace* ns, ast::Node* n, ClassInfo* owner);
    void declareProperty(ClassInfo* c, ast::Node* n);
    void declareEnum(ClassInfo* c, ast::Node* n);
    void finishClasses();
    void buildVtable(ClassInfo* c);
    void enterUnitContext(Dialect d, std::uint16_t unit, std::int32_t library, Namespace* ns, ClassInfo* cls);
    std::string templateKey(const std::vector<TypeP>& args) const;
    std::vector<Function*> attachOutOfClass(ClassInfo* c, ast::Node* f, Namespace* ns);
    bool sameParams(const Function* f, const std::vector<TypeP>& ps) const;
    std::vector<TypeP> paramTypes(ast::Node* fn);
    std::set<std::string> csCaptured_;                        // nomi catturati dalle lambda (C#: in una Box)
    std::size_t polls_{0};
};

inline void dupComponents(Compiler& c, const EV& e) {
    const std::int32_t n = componentsOf(e);
    if (n > 0) c.emit(Op::Dup, 0, static_cast<std::uint8_t>(n));
}

// Nomi semplici dei file e del dialetto di un'unita'.
Dialect unitDialect(const std::string& file) noexcept;

} // namespace fx::prog
