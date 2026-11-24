// Parser di C# (v1.20.0): token -> albero comune (program_ast.hpp).
//
// I tipi di C# possono essere usati prima di essere dichiarati: dichiarazioni
// ed espressioni si distinguono con un'analisi di prova ("tipo seguito da un
// nome") che torna indietro se non riesce. Le regole dei generici seguono la
// disambiguazione della norma (dopo "<...>" un "(", ")", ".", ","...).
// Le stringhe interpolate $"..{x,8:F2}.." diventano un nodo Interp con le
// parti di testo e le espressioni dei buchi (analizzate con lo stesso lexer).
#include "program_ast.hpp"
#include <algorithm>

namespace fx::prog::ast {
namespace {

const char* const kCsKeywords[] = {"abstract", "as", "base", "bool", "break", "byte", "case", "catch", "char", "checked", "class",
    "const", "continue", "decimal", "default", "delegate", "do", "double", "else", "enum", "event", "explicit", "extern", "false",
    "finally", "fixed", "float", "for", "foreach", "goto", "if", "implicit", "in", "int", "interface", "internal", "is", "lock",
    "long", "namespace", "new", "null", "object", "operator", "out", "override", "params", "private", "protected", "public",
    "readonly", "ref", "return", "sbyte", "sealed", "short", "sizeof", "stackalloc", "static", "string", "struct", "switch",
    "this", "throw", "true", "try", "typeof", "uint", "ulong", "unchecked", "unsafe", "ushort", "using", "virtual", "void",
    "volatile", "while", nullptr};
const char* const kCsPredefined[] = {"bool", "byte", "sbyte", "char", "decimal", "double", "float", "int", "uint", "long", "ulong",
    "object", "short", "ushort", "string", "void", "nint", "nuint", "dynamic", nullptr};

class CsParser {
public:
    CsParser(const Lexed& lx, Unit& unit, ParseContext& ctx) : lx_(lx), toks_(lx.tokens), unit_(unit), ctx_(ctx), syms_(*ctx.syms), pool_(*ctx.pool) {}
    void run() {
        Node* root = pool_.node(K::Unit, cur().pos);
        unit_.root = root;
        while (isWord("using") && !isP(peek(), LParen) && !(peek().t == T::Ident && isWord(peek(), "var"))) usingDirective(root->list);
        // istruzioni di primo livello (C# 9): diventano Program.Main(string[] args); le
        // funzioni locali di primo livello diventano metodi static di Program
        Node* top = pool_.node(K::Class, cur().pos);
        top->sym = syms_.intern("Program");
        top->mods = MStatic | MInternal;
        Node* body = pool_.node(K::Block, cur().pos);
        while (!atEnd()) {
            if (isWord("using") && !isP(peek(), LParen) && !isWord(peek(), "var") && !isWord(peek(), "static")) { usingDirective(root->list); continue; }
            if (isWord("namespace")) { root->list.push_back(namespaceDecl()); continue; }
            if (startsTypeDecl()) { root->list.push_back(typeDecl()); continue; }
            classStack_.push_back(top);
            const bool saveStatic = staticCtx_;
            staticCtx_ = true;
            Node* st = statement();
            staticCtx_ = saveStatic;
            classStack_.pop_back();
            if (st->k != K::Empty || st->ival != 1) body->list.push_back(st);
        }
        if (!body->list.empty() || !top->list.empty()) {
            if (!body->list.empty()) {
                Node* m = pool_.node(K::Func, body->list.front()->pos);
                m->sym = syms_.intern("Main");
                m->type = pool_.type(TF::Named, m->pos);
                m->type->name = syms_.intern("void");
                m->type->builtin = true;
                m->mods = MStatic;
                Node* prm = pool_.node(K::Param, m->pos);
                prm->sym = syms_.intern("args");
                TypeNode* st = pool_.type(TF::Named, m->pos);
                st->name = syms_.intern("string");
                st->builtin = true;
                prm->type = pool_.type(TF::Array, m->pos);
                prm->type->inner = st;
                m->list.push_back(prm);
                m->b = body;
                top->list.push_back(m);
            }
            root->list.push_back(top);
        }
    }

    // ------------------------------------------------------------ token
    const Token& cur() const { return toks_[p_]; }
    const Token& peek(std::size_t k = 1) const { return toks_[std::min(p_ + k, toks_.size() - 1)]; }
    bool atEnd() const { return cur().t == T::End; }
    bool isP(P p) const { return cur().t == T::Punct && cur().p == p; }
    static bool isP(const Token& t, P p) { return t.t == T::Punct && t.p == p; }
    bool isWord(const char* w) const { return cur().t == T::Ident && syms_[cur().sym] == w; }
    bool isWord(const Token& t, const char* w) const { return t.t == T::Ident && syms_[t.sym] == w; }
    bool accept(P p) { if (isP(p)) { ++p_; return true; } return false; }
    bool acceptWord(const char* w) { if (isWord(w)) { ++p_; return true; } return false; }
    void expect(P p) {
        if (accept(p)) return;
        if (p == Gt && isP(Shr)) { splits_.push_back({p_, Shr}); toks_[p_].p = Gt; return; }
        fail(std::string("atteso '") + punctText(p) + "'" + near(), cur().pos);
    }
    std::string near() const {
        const Token& t = cur();
        if (t.t == T::End) return " alla fine del file";
        if (t.t == T::Ident) return " prima di '" + syms_[t.sym] + "'";
        if (t.t == T::Punct) return std::string(" prima di '") + punctText(t.p) + "'";
        return "";
    }
    bool keyword(Sym s) const { for (const char* const* k = kCsKeywords; *k; ++k) if (syms_[s] == *k) return true; return false; }
    bool predefined(const Token& t) const {
        if (t.t != T::Ident) return false;
        for (const char* const* k = kCsPredefined; *k; ++k) if (syms_[t.sym] == *k) return true;
        return false;
    }
    Sym ident(const char* what = "un nome") {
        if (cur().t != T::Ident || keyword(cur().sym)) fail(std::string("atteso ") + what + near(), cur().pos);
        return toks_[p_++].sym;
    }
    [[noreturn]] void error(const std::string& m) const { fail(m, cur().pos); }
    struct Depth {
        explicit Depth(CsParser& c) : c_(c) {
            if (++c_.depth_ > ParseDepth) fail("Stack ERROR: annidamento troppo profondo nel sorgente", c_.cur().pos);
            if (++c_.polls_ % 2048 == 0 && c_.ctx_.poll) c_.ctx_.poll();
        }
        ~Depth() { --c_.depth_; }
        CsParser& c_;
    };

    // ------------------------------------------------------------ dichiarazioni
    void usingDirective(std::vector<Node*>& out) {
        Node* u = pool_.node(K::Using, cur().pos);
        ++p_;
        if (isWord("static")) fail("using static non supportato", cur().pos);
        if (cur().t == T::Ident && isP(peek(), Assign)) fail("alias di using non supportati", cur().pos);
        std::string path = syms_[ident()];
        while (accept(Dot)) path += "." + syms_[ident()];
        u->op = 1;
        u->sym = syms_.intern(path);
        expect(Semi);
        out.push_back(u);
        unit_.usings.push_back(path);
    }
    Node* namespaceDecl() {
        Node* ns = pool_.node(K::Namespace, cur().pos);
        ++p_;
        std::string path = syms_[ident()];
        while (accept(Dot)) path += "." + syms_[ident()];
        ns->sym = syms_.intern(path);
        if (accept(Semi)) {                              // namespace di file (C# 10)
            while (!atEnd()) {
                if (isWord("using")) { usingDirective(ns->list); continue; }
                ns->list.push_back(typeDecl());
            }
            return ns;
        }
        expect(LBrace);
        while (!isP(RBrace)) {
            if (atEnd()) fail("manca '}' del namespace", ns->pos);
            if (isWord("using")) { usingDirective(ns->list); continue; }
            if (isWord("namespace")) { ns->list.push_back(namespaceDecl()); continue; }
            ns->list.push_back(typeDecl());
        }
        ++p_;
        return ns;
    }
    std::uint32_t modifiers() {
        std::uint32_t m = 0;
        while (cur().t == T::Ident) {
            const std::string& w = syms_[cur().sym];
            if (w == "public") m |= MPublic; else if (w == "private") m |= MPrivate; else if (w == "protected") m |= MProtected;
            else if (w == "internal") m |= MInternal; else if (w == "static") m |= MStatic; else if (w == "abstract") m |= MAbstract;
            else if (w == "virtual") m |= MVirtual; else if (w == "override") m |= MOverride; else if (w == "sealed") m |= MSealed;
            else if (w == "readonly") m |= MReadonly; else if (w == "const") m |= MConst; else if (w == "partial") m |= MPartial;
            else if (w == "new" && !isP(peek(), LParen) && !isP(peek(), LBracket) && !isP(peek(), LBrace)) m |= MNew;
            else if (w == "extern") m |= MExtern; else if (w == "volatile") m |= MVolatile;
            else if (w == "unsafe") error("unsafe non supportato");
            else if (w == "async") error("async/await non supportati");
            else break;
            ++p_;
        }
        if (isP(LBracket)) error("attributi [..] non supportati");
        return m;
    }
    bool startsTypeDecl() const {
        std::size_t q = p_;
        while (toks_[q].t == T::Ident) {
            const std::string& w = syms_[toks_[q].sym];
            if (w == "class" || w == "struct" || w == "interface" || w == "enum" || w == "record" || w == "delegate") return true;
            if (w == "public" || w == "private" || w == "protected" || w == "internal" || w == "static" || w == "abstract" ||
                w == "sealed" || w == "partial" || w == "readonly" || w == "unsafe") { ++q; continue; }
            return false;
        }
        return isP(toks_[q], LBracket);                  // attributi
    }
    Node* typeDecl() {
        Depth guard(*this);
        const Pos at = cur().pos;
        const std::uint32_t mods = modifiers();
        const std::string kw = cur().t == T::Ident ? syms_[cur().sym] : "";
        if (kw == "record") error("record non supportati: usa class");
        if (kw == "delegate") error("delegate dell'utente non supportati: usa Func<...> o Action<...>");
        if (kw != "class" && kw != "struct" && kw != "interface" && kw != "enum") error("attesa una classe, struct, interface o enum" + near());
        ++p_;
        Node* c = pool_.node(K::Class, at);
        c->mods = mods;
        c->op = kw == "class" ? 0 : kw == "struct" ? 1 : kw == "interface" ? 3 : 4;
        c->sym = ident("il nome del tipo");
        if (isP(Lt)) c->targs = typeParams();
        if (accept(Colon)) {
            if (c->op == 4) { c->type = type(); }
            else do { Node* b = pool_.node(K::BaseSpec, cur().pos); b->type = type(); c->list2.push_back(b); } while (accept(Comma));
        }
        constraints();
        expect(LBrace);
        const Sym saveClass = className_;
        className_ = c->sym;
        classStack_.push_back(c);
        if (c->op == 4) {
            while (!isP(RBrace)) {
                Node* e = pool_.node(K::Enumerator, cur().pos);
                e->sym = ident("il nome di un valore dell'enum");
                if (accept(Assign)) e->a = expression();
                c->list.push_back(e);
                if (!accept(Comma)) break;
            }
            expect(RBrace);
        } else {
            while (!isP(RBrace)) {
                if (atEnd()) fail("manca '}' alla fine di " + syms_[c->sym], at);
                member(c);
            }
            ++p_;
        }
        className_ = saveClass;
        classStack_.pop_back();
        accept(Semi);
        return generic(c);
    }
    // Classe o metodo generico: lo stesso nodo Template dei template del C++
    // (istanze per argomenti di tipo; i vincoli where sono controllati dall'uso).
    Node* generic(Node* d) {
        if (d->targs.empty()) return d;
        Node* t = pool_.node(K::Template, d->pos);
        for (TypeNode* tp : d->targs) {
            Node* p = pool_.node(K::TParam, tp->pos);
            p->sym = tp->name;
            t->list.push_back(p);
        }
        d->targs.clear();
        t->a = d;
        return t;
    }
    std::vector<TypeNode*> typeParams() {
        std::vector<TypeNode*> ps;
        expect(Lt);
        do {
            if (isWord("in") || isWord("out")) ++p_;
            TypeNode* t = pool_.type(TF::Named, cur().pos);
            t->name = ident("il nome del parametro di tipo");
            ps.push_back(t);
        } while (accept(Comma));
        expect(Gt);
        return ps;
    }
    void constraints() {
        while (isWord("where")) {
            ++p_;
            (void)ident();
            expect(Colon);
            do {
                if (acceptWord("class") || acceptWord("struct") || acceptWord("unmanaged") || acceptWord("notnull")) { accept(Question); continue; }
                if (isWord("new") && isP(peek(), LParen)) { p_ += 2; expect(RParen); continue; }
                (void)type();
            } while (accept(Comma));
        }
    }
    void member(Node* cls) {
        Depth guard(*this);
        const Pos at = cur().pos;
        if (accept(Semi)) return;
        if (startsTypeDecl()) { cls->list.push_back(typeDecl()); return; }
        const std::uint32_t mods = modifiers();
        if (isWord("event")) error("event non supportati");
        if (isWord("delegate")) error("delegate dell'utente non supportati: usa Func<...> o Action<...>");
        if (isWord("implicit") || isWord("explicit")) error("conversioni dell'utente (implicit/explicit operator) non supportate");
        // costruttore / costruttore statico / distruttore
        if (cur().t == T::Ident && cur().sym == cls->sym && isP(peek(), LParen)) {
            ++p_;
            Node* f = pool_.node(K::Func, at);
            f->sym = cls->sym;
            f->op = 1;
            f->mods = mods;
            f->list = parameters();
            if (accept(Colon)) {
                Node* mi = pool_.node(K::MemberInit, cur().pos);
                if (acceptWord("this")) { mi->sym = syms_.intern("this"); f->mods |= MThisCall; }
                else if (acceptWord("base")) { mi->sym = syms_.intern("base"); f->mods |= MBaseCall; }
                else error("attesi this(...) o base(...)");
                mi->list = arguments();
                f->list2.push_back(mi);
            }
            f->b = methodBody(f);
            cls->list.push_back(f);
            return;
        }
        if (isP(Tilde)) error("i finalizzatori (~Classe) non sono supportati");
        // tipo del membro
        TypeNode* t = type();
        if (isWord("operator")) {
            ++p_;
            Node* f = pool_.node(K::Func, at);
            std::string op;
            if (cur().t == T::Punct) {
                op = punctText(cur().p);
                ++p_;
                if (op == ">" && isP(Gt)) error("operatore non supportato");
            } else if (isWord("true") || isWord("false")) error("operator true/false non supportati");
            else error("atteso un operatore");
            f->sym = syms_.intern("operator" + op);
            f->type = t;
            f->mods = mods | MStatic;
            f->list = parameters();
            f->b = methodBody(f);
            cls->list.push_back(f);
            return;
        }
        if (isWord("this") && isP(peek(), LBracket)) {
            // indicizzatore
            ++p_;
            Node* pr = pool_.node(K::Property, at);
            pr->sym = syms_.intern("this");
            pr->type = t;
            pr->mods = mods;
            pr->op = 2;
            expect(LBracket);
            while (!isP(RBracket)) { pr->list.push_back(parameter()); if (!accept(Comma)) break; }
            expect(RBracket);
            accessors(pr);
            cls->list.push_back(pr);
            return;
        }
        const Sym name = ident("il nome di un membro");
        if (isP(LParen) || isP(Lt)) {
            Node* f = pool_.node(K::Func, at);
            f->sym = name;
            f->type = t;
            f->mods = mods;
            if (isP(Lt)) f->targs = typeParams();
            f->list = parameters();
            constraints();
            f->b = methodBody(f);
            if (!f->b && !(mods & (MAbstract | MExtern)) && cls->op != 3) error("metodo senza corpo: manca { ... }");
            cls->list.push_back(generic(f));
            return;
        }
        if (isP(LBrace) || isP(FatArrow)) {
            Node* pr = pool_.node(K::Property, at);
            pr->sym = name;
            pr->type = t;
            pr->mods = mods;
            if (accept(FatArrow)) {
                Node* ret = pool_.node(K::Return, cur().pos);
                ret->a = expression();
                Node* b = pool_.node(K::Block, ret->pos);
                b->list.push_back(ret);
                pr->a = b;
                pr->ival = 1;                             // ha get
                expect(Semi);
            } else {
                accessors(pr);
                if (accept(Assign)) { pr->c = variableInit(); expect(Semi); }
            }
            cls->list.push_back(pr);
            return;
        }
        // campi: Tipo a = 1, b;
        Node* f = pool_.node(K::Field, at);
        f->mods = mods;
        Sym n = name;
        while (true) {
            Node* d = pool_.node(K::Declarator, cur().pos);
            d->sym = n;
            d->type = t;
            d->mods = mods;
            if (accept(Assign)) { d->op = 1; d->a = variableInit(); }
            f->list.push_back(d);
            if (!accept(Comma)) break;
            n = ident();
        }
        expect(Semi);
        cls->list.push_back(f);
    }
    // { get; set; } / { get { ... } private set { ... } } / { get => x; }
    void accessors(Node* pr) {
        expect(LBrace);
        bool autoProp = true;
        while (!isP(RBrace)) {
            std::uint32_t am = 0;
            while (isWord("private") || isWord("protected") || isWord("internal") || isWord("public")) {
                am |= isWord("private") ? MPrivate : isWord("protected") ? MProtected : isWord("internal") ? MInternal : MPublic;
                ++p_;
            }
            const bool get = isWord("get");
            const bool set = isWord("set") || isWord("init");
            if (!get && !set) error("attesi get o set");
            const Pos at = cur().pos;
            ++p_;
            Node* body = nullptr;
            if (accept(Semi)) {}
            else if (accept(FatArrow)) {
                autoProp = false;
                Node* e = expression();
                expect(Semi);
                body = pool_.node(K::Block, at);
                if (get) { Node* r = pool_.node(K::Return, at); r->a = e; body->list.push_back(r); }
                else { Node* s = pool_.node(K::ExprStmt, at); s->a = e; body->list.push_back(s); }
            } else { autoProp = false; body = block(); }
            if (get) { pr->a = body ? body : pool_.node(K::Empty, at); pr->ival |= 1; }
            else { pr->b = body ? body : pool_.node(K::Empty, at); pr->ival |= 2; pr->dval = static_cast<double>(am); }
        }
        expect(RBrace);
        if (autoProp && pr->op != 2) pr->op = 1;
        else if (pr->op != 2) pr->op = 0;
        // corpi vuoti delle proprieta' automatiche
        if (pr->op == 1) {
            if (pr->a && pr->a->k == K::Empty) pr->a = nullptr;
            if (pr->b && pr->b->k == K::Empty) pr->b = nullptr;
        }
    }
    Node* methodBody(Node* f) {
        if (accept(Semi)) return nullptr;
        struct StaticCtx { bool& r; bool s; ~StaticCtx() { r = s; } } guard{staticCtx_, staticCtx_};
        staticCtx_ = (f->mods & MStatic) != 0;
        if (accept(FatArrow)) {
            Node* b = pool_.node(K::Block, cur().pos);
            Node* e = expression();
            expect(Semi);
            Node* r = pool_.node(K::Return, e->pos);
            r->a = e;
            r->op = 1;                                   // corpo d'espressione: anche void
            b->list.push_back(r);
            (void)f;
            return b;
        }
        return block();
    }
    Node* parameter() {
        Node* prm = pool_.node(K::Param, cur().pos);
        while (true) {
            if (acceptWord("ref")) { prm->mods |= MRef; continue; }
            if (acceptWord("out")) { prm->mods |= MOut; continue; }
            if (acceptWord("in")) { prm->mods |= MIn; continue; }
            if (acceptWord("params")) { prm->mods |= MParams; continue; }
            if (isWord("this")) error("metodi di estensione non supportati");
            break;
        }
        if (isP(LBracket)) error("attributi [..] non supportati");
        prm->type = type();
        prm->sym = ident("il nome del parametro");
        if (accept(Assign)) prm->a = expression();
        return prm;
    }
    std::vector<Node*> parameters() {
        std::vector<Node*> ps;
        expect(LParen);
        while (!isP(RParen)) { ps.push_back(parameter()); if (!accept(Comma)) break; }
        expect(RParen);
        return ps;
    }

    // ------------------------------------------------------------ tipi
    TypeNode* type() {
        Depth guard(*this);
        TypeNode* t = pool_.type(TF::Named, cur().pos);
        if (cur().t != T::Ident) error("atteso un tipo" + near());
        if (predefined(cur())) {
            const std::string w = syms_[cur().sym];
            if (w == "decimal") error("decimal non supportato: usa double");
            if (w == "dynamic") error("dynamic non supportato");
            if (w == "nint" || w == "nuint") error("nint/nuint non supportati");
            t->name = cur().sym;
            t->builtin = true;
            ++p_;
        } else {
            if (isWord("var") && !isP(peek(), Dot)) { t->f = TF::Auto; t->name = cur().sym; ++p_; return t; }
            Sym s = ident("un tipo");
            while (true) {
                if (isP(Lt)) t->args = typeArgs();
                if (isP(Dot) && peek().t == T::Ident) {
                    if (!t->args.empty()) {
                        // Esterno<T>.Interno: tipo annidato di un generico
                        TypeNode* outer = pool_.type(TF::Named, t->pos);
                        *outer = *t;
                        outer->name = s;
                        t = pool_.type(TF::Named, t->pos);
                        t->inner = outer;
                        ++p_;
                        s = ident();
                        continue;
                    }
                    t->qual.push_back(s);
                    ++p_;
                    s = ident();
                    continue;
                }
                break;
            }
            t->name = s;
        }
        // nullable e array
        while (true) {
            if (isP(Question) && !questionIsTernary()) {
                ++p_;
                TypeNode* n = pool_.type(TF::Nullable, t->pos);
                n->inner = t;
                t = n;
                continue;
            }
            if (isP(LBracket) && (isP(peek(), RBracket) || isP(peek(), Comma))) {
                ++p_;
                std::uint8_t rank = 1;
                while (accept(Comma)) ++rank;
                expect(RBracket);
                TypeNode* a = pool_.type(TF::Array, t->pos);
                a->inner = t;
                a->rank = rank;
                t = a;
                continue;
            }
            break;
        }
        return t;
    }
    bool questionIsTernary() const {
        // "T? x" (nullable) contro "a ? b : c"
        const Token& n = peek();
        return !(n.t == T::Ident || isP(n, RParen) || isP(n, Comma) || isP(n, Gt) || isP(n, LBracket) || isP(n, Semi) || isP(n, Assign) || isP(n, RBracket));
    }
    std::vector<TypeNode*> typeArgs() {
        std::vector<TypeNode*> args;
        expect(Lt);
        do args.push_back(type()); while (accept(Comma));
        if (isP(Shr)) { splits_.push_back({p_, Shr}); toks_[p_].p = Gt; return args; }
        expect(Gt);
        return args;
    }
    // Analisi di prova: il token corrente inizia "Tipo nome"?
    bool tryTypeThenName(TypeNode** out = nullptr) {
        const std::size_t save = p_;
        const std::size_t mark = splits_.size();
        try {
            TypeNode* t = type();
            if (cur().t == T::Ident && !keyword(cur().sym)) {
                if (out) { *out = t; return true; }
                p_ = save;
                restoreSplits(mark);
                return true;
            }
        } catch (const Error&) {}
        p_ = save;
        restoreSplits(mark);
        return false;
    }
    // I '>>' divisi durante una prova fallita tornano com'erano.
    void restoreSplits(std::size_t mark) {
        while (splits_.size() > mark) { toks_[splits_.back().first].p = splits_.back().second; splits_.pop_back(); }
    }
    // "<" dopo un nome apre argomenti di tipo (regola di disambiguazione)?
    bool genericArgsAhead() const {
        int depth = 0;
        std::size_t q = p_;
        for (; q < toks_.size() && toks_[q].t != T::End; ++q) {
            const Token& t = toks_[q];
            if (isP(t, Lt)) { ++depth; continue; }
            if (isP(t, Gt)) { if (--depth == 0) break; continue; }
            if (isP(t, Shr)) { depth -= 2; if (depth <= 0) break; continue; }
            if (t.t == T::Ident || isP(t, Comma) || isP(t, Dot) || isP(t, LBracket) || isP(t, RBracket) || isP(t, Question)) continue;
            return false;
        }
        if (q >= toks_.size() || toks_[q].t == T::End) return false;
        const Token& n = toks_[q + 1];
        return isP(n, LParen) || isP(n, RParen) || isP(n, RBracket) || isP(n, Colon) || isP(n, Semi) || isP(n, Comma) ||
               isP(n, Dot) || isP(n, Question) || isP(n, EqEq) || isP(n, Ne) || isP(n, LBrace) || isP(n, RBrace);
    }

    // ------------------------------------------------------------ istruzioni
    Node* block() {
        Depth guard(*this);
        Node* b = pool_.node(K::Block, cur().pos);
        expect(LBrace);
        while (!isP(RBrace)) {
            if (atEnd()) fail("manca '}' alla fine del blocco", b->pos);
            b->list.push_back(statement());
        }
        ++p_;
        return b;
    }
    Node* variableInit() {
        if (isP(LBrace)) return arrayInit();
        return expression();
    }
    Node* arrayInit() {
        Depth guard(*this);
        Node* n = pool_.node(K::InitList, cur().pos);
        expect(LBrace);
        while (!isP(RBrace)) {
            n->list.push_back(isP(LBrace) ? arrayInit() : expression());
            if (!accept(Comma)) break;
        }
        expect(RBrace);
        return n;
    }
    Node* localDecl(TypeNode* t, Pos at, std::uint32_t mods) {
        Node* v = pool_.node(K::VarDecl, at);
        v->mods = mods;
        do {
            Node* d = pool_.node(K::Declarator, cur().pos);
            d->sym = ident("il nome della variabile");
            d->type = t;
            d->mods = mods;
            if (isP(LParen)) error("funzioni locali non supportate: dichiarale come metodi della classe");
            if (accept(Assign)) { d->op = 1; d->a = variableInit(); }
            v->list.push_back(d);
        } while (accept(Comma));
        return v;
    }
    Node* statement() {
        Depth guard(*this);
        const Pos at = cur().pos;
        if (isP(LBrace)) return block();
        if (accept(Semi)) return pool_.node(K::Empty, at);
        if (cur().t == T::Ident) {
            const std::string w = syms_[cur().sym];
            if (w == "if") {
                ++p_;
                Node* n = pool_.node(K::If, at);
                expect(LParen); n->a = expression(); expect(RParen);
                n->b = statement();
                if (acceptWord("else")) n->c = statement();
                return n;
            }
            if (w == "while") { ++p_; Node* n = pool_.node(K::While, at); expect(LParen); n->a = expression(); expect(RParen); n->b = statement(); return n; }
            if (w == "do") { ++p_; Node* n = pool_.node(K::DoWhile, at); n->b = statement(); if (!acceptWord("while")) error("atteso while"); expect(LParen); n->a = expression(); expect(RParen); expect(Semi); return n; }
            if (w == "for") return forStmt();
            if (w == "foreach") {
                ++p_;
                Node* n = pool_.node(K::ForEach, at);
                expect(LParen);
                Node* d = pool_.node(K::Declarator, cur().pos);
                if (acceptWord("ref")) error("foreach con ref non supportato");
                d->type = type();
                d->sym = ident("il nome della variabile del foreach");
                n->a = d;
                if (!acceptWord("in")) error("atteso in nel foreach");
                n->b = expression();
                expect(RParen);
                n->c = statement();
                return n;
            }
            if (w == "switch") {
                ++p_;
                Node* n = pool_.node(K::Switch, at);
                expect(LParen); n->a = expression(); expect(RParen);
                Node* body = pool_.node(K::Block, cur().pos);
                expect(LBrace);
                while (!isP(RBrace)) {
                    if (atEnd()) fail("manca '}' dello switch", at);
                    if (isWord("case")) {
                        Node* c = pool_.node(K::Case, cur().pos);
                        ++p_;
                        if (predefined(cur()) && peek().t == T::Ident) error("pattern nei case non supportati");
                        c->a = expression();
                        if (isWord("when")) error("case ... when non supportato");
                        expect(Colon);
                        body->list.push_back(c);
                        continue;
                    }
                    if (isWord("default") && isP(peek(), Colon)) { body->list.push_back(pool_.node(K::Case, cur().pos)); p_ += 2; continue; }
                    body->list.push_back(statement());
                }
                ++p_;
                n->b = body;
                n->op = 1;                                // regole di C# (niente fall-through)
                return n;
            }
            if (w == "break") { ++p_; expect(Semi); return pool_.node(K::Break, at); }
            if (w == "continue") { ++p_; expect(Semi); return pool_.node(K::Continue, at); }
            if (w == "return") { ++p_; Node* n = pool_.node(K::Return, at); if (!isP(Semi)) n->a = expression(); expect(Semi); return n; }
            if (w == "goto") {
                ++p_;
                if (isWord("case") || isWord("default")) error("goto case/default non supportato");
                Node* n = pool_.node(K::Goto, at); n->sym = ident(); expect(Semi); return n;
            }
            if (w == "throw") { ++p_; Node* n = pool_.node(K::Throw, at); if (!isP(Semi)) n->a = expression(); expect(Semi); return n; }
            if (w == "try") return tryStmt();
            if (w == "using" && (isP(peek(), LParen) || isWord(peek(), "var") || peek().t == T::Ident)) {
                ++p_;
                Node* n = pool_.node(K::UsingStmt, at);
                if (accept(LParen)) {
                    TypeNode* t = nullptr;
                    if (tryTypeThenName(&t)) n->a = localDecl(t, cur().pos, 0);
                    else n->a = expression();
                    expect(RParen);
                    n->b = statement();
                } else {
                    TypeNode* t = type();
                    n->a = localDecl(t, cur().pos, 0);
                    expect(Semi);
                    n->op = 1;                            // using var: fino alla fine del blocco
                }
                return n;
            }
            if (w == "checked" || w == "unchecked") {
                if (isP(peek(), LBrace)) { if (w == "checked") error("checked non supportato"); ++p_; return block(); }
            }
            if (w == "lock" || w == "fixed" || w == "unsafe" || w == "yield" || w == "await") error(w + " non supportato");
            if (w == "const") {
                ++p_;
                TypeNode* t = type();
                Node* v = localDecl(t, at, MConst);
                expect(Semi);
                return v;
            }
            if (isP(peek(), Colon) && !keyword(cur().sym)) {
                Node* n = pool_.node(K::Label, at);
                n->sym = cur().sym;
                p_ += 2;
                n->a = statement();
                return n;
            }
            // funzione locale static: static int F(int x) => ...;
            if (w == "static") {
                const std::size_t save = p_;
                ++p_;
                TypeNode* t = nullptr;
                if (tryTypeThenName(&t) && isP(peek(), LParen)) return localFunction(t, at, true);
                p_ = save;
            }
            // dichiarazione locale?
            TypeNode* t = nullptr;
            if (tryTypeThenName(&t)) {
                if (isP(peek(), LParen)) return localFunction(t, at, false);
                Node* v = localDecl(t, at, 0);
                expect(Semi);
                return v;
            }
        }
        Node* e = pool_.node(K::ExprStmt, at);
        e->a = expression();
        expect(Semi);
        return e;
    }
    // Funzione locale: diventa un metodo (privato) della classe che la contiene,
    // static se lo e' il metodo; usa i campi e i parametri propri, non le
    // variabili locali del metodo (passale come parametri).
    Node* localFunction(TypeNode* t, Pos at, bool isStatic) {
        if (classStack_.empty()) error("funzione locale fuori da una classe");
        Node* f = pool_.node(K::Func, at);
        f->sym = ident("il nome della funzione locale");
        f->type = t;
        f->mods = MPrivate | ((isStatic || staticCtx_) ? MStatic : 0u);
        f->list = parameters();
        f->b = methodBody(f);
        if (!f->b) error("funzione locale senza corpo");
        classStack_.back()->list.push_back(f);
        Node* e = pool_.node(K::Empty, at);
        e->ival = 1;                                     // segnaposto della funzione locale
        return e;
    }
    Node* forStmt() {
        const Pos at = cur().pos;
        ++p_;
        Node* n = pool_.node(K::For, at);
        expect(LParen);
        if (!accept(Semi)) {
            TypeNode* t = nullptr;
            if (tryTypeThenName(&t)) n->a = localDecl(t, cur().pos, 0);
            else {
                Node* e = pool_.node(K::ExprStmt, cur().pos);
                e->a = expression();
                while (accept(Comma)) { Node* c = pool_.node(K::Comma, cur().pos); c->a = e->a; c->b = expression(); e->a = c; }
                n->a = e;
            }
            expect(Semi);
        }
        if (!isP(Semi)) n->b = expression();
        expect(Semi);
        if (!isP(RParen)) {
            n->c = expression();
            while (accept(Comma)) { Node* c = pool_.node(K::Comma, cur().pos); c->a = n->c; c->b = expression(); n->c = c; }
        }
        expect(RParen);
        n->d = statement();
        return n;
    }
    Node* tryStmt() {
        const Pos at = cur().pos;
        ++p_;
        Node* n = pool_.node(K::Try, at);
        n->a = block();
        while (isWord("catch")) {
            Node* c = pool_.node(K::Catch, cur().pos);
            ++p_;
            if (accept(LParen)) {
                c->type = type();
                if (cur().t == T::Ident && !keyword(cur().sym)) c->sym = ident();
                expect(RParen);
            }
            if (acceptWord("when")) {
                // filtro dell'eccezione: catch (E e) when (condizione)
                expect(LParen);
                c->b = expression();
                expect(RParen);
            }
            c->a = block();
            n->list.push_back(c);
        }
        if (acceptWord("finally")) n->b = block();
        if (n->list.empty() && !n->b) error("try senza catch ne' finally");
        return n;
    }

    // ------------------------------------------------------------ espressioni
    Node* expression() {
        Depth guard(*this);
        // lambda: x => ..., (a, b) => ..., () => ...
        if (cur().t == T::Ident && isP(peek(), FatArrow) && !keyword(cur().sym)) return lambda();
        if (isP(LParen) && lambdaAhead()) return lambda();
        Node* l = conditional();
        if (cur().t == T::Punct) {
            const P p = cur().p;
            if (p == Assign || p == PlusEq || p == MinusEq || p == StarEq || p == SlashEq || p == PercentEq || p == AmpEq ||
                p == PipeEq || p == CaretEq || p == ShlEq || p == ShrEq || p == QQEq) {
                Node* a = pool_.node(K::Assign, cur().pos);
                a->op = p;
                ++p_;
                a->a = l;
                a->b = expression();
                return a;
            }
        }
        return l;
    }
    bool lambdaAhead() const {
        int depth = 0;
        for (std::size_t q = p_; q < toks_.size() && toks_[q].t != T::End; ++q) {
            if (isP(toks_[q], LParen)) ++depth;
            else if (isP(toks_[q], RParen)) { if (--depth == 0) return isP(toks_[q + 1], FatArrow); }
            else if (isP(toks_[q], Semi) || isP(toks_[q], LBrace)) return false;
        }
        return false;
    }
    Node* lambda() {
        Node* n = pool_.node(K::Lambda, cur().pos);
        if (cur().t == T::Ident) {
            Node* prm = pool_.node(K::Param, cur().pos);
            prm->sym = ident();
            n->list.push_back(prm);
        } else {
            expect(LParen);
            while (!isP(RParen)) {
                Node* prm = pool_.node(K::Param, cur().pos);
                if (cur().t == T::Ident && (isP(peek(), Comma) || isP(peek(), RParen))) prm->sym = ident();
                else { prm->type = type(); prm->sym = ident(); }
                n->list.push_back(prm);
                if (!accept(Comma)) break;
            }
            expect(RParen);
        }
        expect(FatArrow);
        if (isP(LBrace)) n->b = block();
        else {
            Node* b = pool_.node(K::Block, cur().pos);
            Node* r = pool_.node(K::Return, cur().pos);
            r->a = expression();
            r->op = 1;
            b->list.push_back(r);
            n->b = b;
            n->op = 1;                                    // corpo d'espressione
        }
        n->flags = 2;                                     // C#: cattura per riferimento
        return n;
    }
    Node* conditional() {
        Depth guard(*this);
        Node* c = nullCoalesce();
        if (isP(Question)) {
            Node* n = pool_.node(K::Cond, cur().pos);
            ++p_;
            n->a = c;
            n->b = expression();
            expect(Colon);
            n->c = expression();
            return n;
        }
        return c;
    }
    Node* nullCoalesce() {
        Node* l = binary(1);
        if (isP(QQ)) {
            Node* n = pool_.node(K::Binary, cur().pos);
            n->op = QQ;
            ++p_;
            n->a = l;
            n->b = nullCoalesce();
            return n;
        }
        return l;
    }
    static int precOf(P p) {
        switch (p) {
        case OrOr: return 1; case AndAnd: return 2; case Pipe: return 3; case Caret: return 4; case Amp: return 5;
        case EqEq: case Ne: return 6; case Lt: case Gt: case Le: case Ge: return 7; case Shl: case Shr: return 8;
        case Plus: case Minus: return 9; case Star: case Slash: case Percent: return 10;
        default: return -1;
        }
    }
    Node* binary(int minPrec) {
        Node* l = unary();
        while (true) {
            // is / as (precedenza relazionale)
            if (minPrec <= 7 && (isWord("is") || isWord("as"))) {
                const bool is = isWord("is");
                Node* n = pool_.node(is ? K::Is : K::As, cur().pos);
                ++p_;
                n->a = l;
                if (is && isWord("not")) error("pattern 'is not' non supportato");
                if (is && isWord("null")) { ++p_; Node* nl = pool_.node(K::NullLit, n->pos); Node* b = pool_.node(K::Binary, n->pos); b->op = EqEq; b->a = l; b->b = nl; l = b; continue; }
                n->type = type();
                if (is && cur().t == T::Ident && !keyword(cur().sym)) n->sym = ident();
                l = n;
                continue;
            }
            if (cur().t != T::Punct) break;
            const P p = cur().p;
            const int pr = precOf(p);
            if (pr < 0 || pr < minPrec) break;
            Node* n = pool_.node(K::Binary, cur().pos);
            ++p_;
            n->op = p;
            n->a = l;
            Depth guard(*this);
            n->b = binary(pr + 1);
            l = n;
        }
        return l;
    }
    bool castAhead() const {
        // (Tipo)x: tipo predefinito, o nome (anche generico/array) seguito da un'espressione
        const Token& n = peek();
        if (n.t != T::Ident) return false;
        if (predefined(n)) {
            // (int)x, (double[])...; (int) da solo non esiste
            return true;
        }
        int depth = 0;
        std::size_t q = p_ + 1;
        for (; q < toks_.size() && toks_[q].t != T::End; ++q) {
            const Token& t = toks_[q];
            if (isP(t, LParen)) return false;
            if (isP(t, Lt)) ++depth;
            else if (isP(t, Gt)) --depth;
            else if (isP(t, RParen)) break;
            else if (!(t.t == T::Ident || isP(t, Dot) || isP(t, Comma) || isP(t, LBracket) || isP(t, RBracket) || isP(t, Question) || isP(t, Shr))) return false;
        }
        if (depth > 0) return false;
        const Token& after = toks_[q + 1];
        return (after.t == T::Ident && !isWord(after, "is") && !isWord(after, "as")) || after.t == T::Int || after.t == T::Float ||
               after.t == T::Str || after.t == T::Char || after.t == T::Interp || isP(after, LParen) || isP(after, Bang) || isP(after, Tilde);
    }
    Node* unary() {
        Depth guard(*this);
        const Pos at = cur().pos;
        if (cur().t == T::Punct) {
            switch (cur().p) {
            case PlusPlus: { ++p_; Node* n = pool_.node(K::PreInc, at); n->a = unary(); return n; }
            case MinusMinus: { ++p_; Node* n = pool_.node(K::PreDec, at); n->a = unary(); return n; }
            case Plus: case Minus: case Bang: case Tilde: { Node* n = pool_.node(K::Unary, at); n->op = cur().p; ++p_; n->a = unary(); return n; }
            case LParen:
                if (castAhead()) {
                    ++p_;
                    Node* n = pool_.node(K::Cast, at);
                    n->type = type();
                    expect(RParen);
                    n->a = unary();
                    return n;
                }
                break;
            case Amp: case Star: error("puntatori non supportati in C#");
            default: break;
            }
        }
        if (isWord("await")) error("async/await non supportati");
        return postfix(primary());
    }
    std::vector<Node*> arguments() {
        std::vector<Node*> args;
        expect(LParen);
        while (!isP(RParen)) {
            std::uint32_t m = 0;
            if (acceptWord("ref")) m = MRef;
            else if (acceptWord("out")) {
                m = MOut;
                // out var x / out int x: dichiarazione nell'argomento
                if ((isWord("var") || predefined(cur()) || (cur().t == T::Ident && peek().t == T::Ident)) && peek().t == T::Ident) {
                    Node* d = pool_.node(K::Declarator, cur().pos);
                    d->type = type();
                    d->sym = ident();
                    d->mods = MOut;
                    args.push_back(d);
                    if (!accept(Comma)) break;
                    continue;
                }
            } else if (acceptWord("in")) m = MIn;
            if (cur().t == T::Ident && isP(peek(), Colon) && !isP(peek(2), Colon)) error("argomenti con nome non supportati");
            Node* a = expression();
            a->mods |= m;
            args.push_back(a);
            if (!accept(Comma)) break;
        }
        expect(RParen);
        return args;
    }
    Node* primary() {
        Depth guard(*this);
        const Token& t = cur();
        const Pos at = t.pos;
        switch (t.t) {
        case T::Int: { Node* n = pool_.node(K::IntLit, at); n->ival = t.i; n->op = t.flags; ++p_; return n; }
        case T::Float: { Node* n = pool_.node(K::FloatLit, at); n->dval = t.d; n->op = t.flags; ++p_; return n; }
        case T::Char: { Node* n = pool_.node(K::CharLit, at); n->ival = t.i; ++p_; return n; }
        case T::Str: {
            Node* n = pool_.node(K::StrLit, at);
            unit_.strings.push_back(lx_.strings[t.str]);
            n->str = static_cast<std::uint32_t>(unit_.strings.size() - 1);
            n->flags = 1;
            ++p_;
            return n;
        }
        case T::Interp: return interpolation();
        case T::Punct:
            if (t.p == LParen) { ++p_; Node* e = expression(); expect(RParen); return e; }
            break;
        case T::Ident: {
            const std::string w = syms_[t.sym];
            if (w == "true" || w == "false") { Node* n = pool_.node(K::BoolLit, at); n->ival = w == "true"; ++p_; return n; }
            if (w == "null") { ++p_; return pool_.node(K::NullLit, at); }
            if (w == "this") { ++p_; return pool_.node(K::This, at); }
            if (w == "base") { ++p_; return pool_.node(K::Base, at); }
            if (w == "new") return newExpr();
            if (w == "default") {
                ++p_;
                Node* n = pool_.node(K::Default, at);
                if (accept(LParen)) { n->type = type(); expect(RParen); }
                return n;
            }
            if (w == "sizeof" || w == "typeof") {
                // sizeof(int) = 4; typeof(T) -> System.Type (op = 1)
                ++p_;
                Node* n = pool_.node(K::SizeofType, at);
                n->op = w == "typeof" ? 1 : 0;
                expect(LParen);
                n->type = type();
                expect(RParen);
                return n;
            }
            if (w == "nameof") {
                // nameof(x), nameof(p.Campo), nameof(Classe.Metodo): l'ultimo nome, come testo
                ++p_;
                expect(LParen);
                Sym last = 0;
                int depth = 0;
                while (depth > 0 || !isP(RParen)) {
                    if (atEnd()) error("manca ')' di nameof");
                    if (isP(LParen) || isP(Lt)) ++depth;
                    else if (isP(RParen) || isP(Gt)) --depth;
                    else if (cur().t == T::Ident && depth == 0) last = cur().sym;
                    ++p_;
                }
                expect(RParen);
                if (!last) error("nameof vuole un nome");
                Node* n = pool_.node(K::StrLit, at);
                unit_.strings.push_back(syms_[last]);
                n->str = static_cast<std::uint32_t>(unit_.strings.size() - 1);
                n->flags = 1;
                return n;
            }
            if (w == "stackalloc" || w == "delegate" || w == "checked" || w == "unchecked")
                error(w + " non supportato");
            if (w == "throw") { ++p_; Node* n = pool_.node(K::ThrowExpr, at); n->a = expression(); return n; }
            if (predefined(t)) {
                // int.Parse, string.Join, double.MaxValue...
                Node* n = pool_.node(K::TypeExpr, at);
                n->type = pool_.type(TF::Named, at);
                n->type->name = t.sym;
                n->type->builtin = true;
                ++p_;
                return n;
            }
            if (keyword(t.sym)) error("'" + w + "' non atteso qui");
            Node* n = pool_.node(K::Name, at);
            n->sym = t.sym;
            ++p_;
            if (isP(Lt) && genericArgsAhead()) {
                const std::size_t save = p_;
                try { n->targs = typeArgs(); }
                catch (const Error&) { p_ = save; n->targs.clear(); }
            }
            return n;
        }
        default: break;
        }
        error("attesa un'espressione" + near());
    }
    Node* newExpr() {
        const Pos at = cur().pos;
        ++p_;
        if (isP(LBracket)) {
            // new[] { ... }
            ++p_;
            expect(RBracket);
            Node* n = pool_.node(K::NewArray, at);
            n->a = arrayInit();
            return n;
        }
        if (isP(LBrace)) error("tipi anonimi (new { ... }) non supportati");
        // tipo senza le dimensioni dell'array
        TypeNode* t = pool_.type(TF::Named, cur().pos);
        {
            if (predefined(cur())) { t->name = cur().sym; t->builtin = true; ++p_; }
            else {
                Sym s = ident("un tipo");
                while (true) {
                    if (isP(Lt)) t->args = typeArgs();
                    if (isP(Dot) && peek().t == T::Ident) { t->qual.push_back(s); ++p_; s = ident(); continue; }
                    break;
                }
                t->name = s;
            }
            if (isP(Question)) { ++p_; TypeNode* nn = pool_.type(TF::Nullable, t->pos); nn->inner = t; t = nn; }
        }
        if (isP(LBracket)) {
            Node* n = pool_.node(K::NewArray, at);
            ++p_;
            if (isP(RBracket) || isP(Comma)) {
                // new int[] {..} / new int[,] {..}
                std::uint8_t rank = 1;
                while (accept(Comma)) ++rank;
                expect(RBracket);
                n->ival = rank;
                while (isP(LBracket)) {                   // new int[][] {..}
                    ++p_;
                    std::uint8_t r2 = 1;
                    while (accept(Comma)) ++r2;
                    expect(RBracket);
                    TypeNode* a = pool_.type(TF::Array, t->pos); a->inner = t; a->rank = r2; t = a;
                }
                n->type = t;
                n->a = arrayInit();
                return n;
            }
            n->list.push_back(expression());
            while (accept(Comma)) n->list.push_back(expression());
            expect(RBracket);
            n->ival = static_cast<std::int64_t>(n->list.size());
            // jagged: new int[3][]
            while (isP(LBracket)) {
                ++p_;
                std::uint8_t r2 = 1;
                while (accept(Comma)) ++r2;
                expect(RBracket);
                TypeNode* a = pool_.type(TF::Array, t->pos); a->inner = t; a->rank = r2; t = a;
            }
            n->type = t;
            if (isP(LBrace)) n->a = arrayInit();
            return n;
        }
        Node* n = pool_.node(K::New, at);
        n->type = t;
        if (isP(LParen)) n->list = arguments();
        else if (!isP(LBrace)) error("atteso ( dopo il tipo di new");
        if (isP(LBrace)) {
            // inizializzatore di oggetto o di collezione
            Node* oi = pool_.node(K::ObjInit, cur().pos);
            oi->a = n;
            ++p_;
            while (!isP(RBrace)) {
                if (cur().t == T::Ident && isP(peek(), Assign)) {
                    Node* m = pool_.node(K::Label, cur().pos);
                    m->sym = ident();
                    ++p_;
                    m->a = isP(LBrace) ? arrayInit() : expression();
                    oi->list.push_back(m);
                } else if (isP(LBracket)) {
                    Node* m = pool_.node(K::Index, cur().pos);
                    ++p_;
                    m->list.push_back(expression());
                    expect(RBracket);
                    expect(Assign);
                    m->b = expression();
                    oi->list.push_back(m);
                } else if (isP(LBrace)) {
                    oi->list.push_back(arrayInit());      // { chiave, valore }
                } else {
                    oi->list.push_back(expression());
                }
                if (!accept(Comma)) break;
            }
            expect(RBrace);
            return oi;
        }
        return n;
    }
    Node* interpolation() {
        const Token& t = cur();
        const Pos at = t.pos;
        const bool verbatim = (t.flags & 1) != 0;
        const std::string raw = lx_.strings[t.str];
        ++p_;
        Node* n = pool_.node(K::Interp, at);
        std::string text;
        auto flush = [&] {
            if (text.empty()) return;
            Node* s = pool_.node(K::StrLit, at);
            unit_.strings.push_back(text);
            s->str = static_cast<std::uint32_t>(unit_.strings.size() - 1);
            s->flags = 1;
            n->list.push_back(s);
            text.clear();
        };
        for (std::size_t i = 0; i < raw.size(); ++i) {
            const char c = raw[i];
            if (c == '{' && i + 1 < raw.size() && raw[i + 1] == '{') { text += '{'; ++i; continue; }
            if (c == '}' && i + 1 < raw.size() && raw[i + 1] == '}') { text += '}'; ++i; continue; }
            if (verbatim && c == '"' && i + 1 < raw.size() && raw[i + 1] == '"') { text += '"'; ++i; continue; }
            if (!verbatim && c == '\\' && i + 1 < raw.size()) {
                const char e = raw[++i];
                switch (e) {
                case 'n': text += '\n'; break; case 't': text += '\t'; break; case 'r': text += '\r'; break;
                case '\\': text += '\\'; break; case '"': text += '"'; break; case '\'': text += '\''; break; case '0': text += '\0'; break;
                default: fail(std::string("sequenza di escape \\") + e + " non valida", at);
                }
                continue;
            }
            if (c == '{') {
                flush();
                // buco: espressione[,allineamento][:formato]
                int depth = 0;
                std::size_t j = i + 1;
                bool inStr = false;
                std::size_t colon = std::string::npos, comma = std::string::npos;
                for (; j < raw.size(); ++j) {
                    const char d = raw[j];
                    if (inStr) { if (d == '\\') ++j; else if (d == '"') inStr = false; continue; }
                    if (d == '"') { inStr = true; continue; }
                    if (d == '(' || d == '[' || d == '{') ++depth;
                    else if (d == ')' || d == ']') --depth;
                    else if (d == '}') { if (depth == 0) break; --depth; }
                    else if (d == ':' && depth == 0 && colon == std::string::npos) colon = j;
                    else if (d == ',' && depth == 0 && colon == std::string::npos && comma == std::string::npos) comma = j;
                    else if (d == '?' && depth == 0) { /* ?: dentro un buco richiede parentesi */ }
                }
                if (j >= raw.size()) fail("'{' non chiusa nella stringa interpolata", at);
                const std::size_t exprEnd = comma != std::string::npos ? comma : colon != std::string::npos ? colon : j;
                const std::string expr = raw.substr(i + 1, exprEnd - i - 1);
                Node* hole = pool_.node(K::Unary, at);       // contenitore: a = espressione
                hole->op = 0;
                hole->a = subExpression(expr, at);
                if (comma != std::string::npos) {
                    const std::size_t aEnd = colon != std::string::npos ? colon : j;
                    hole->ival = std::strtol(raw.substr(comma + 1, aEnd - comma - 1).c_str(), nullptr, 10);
                }
                if (colon != std::string::npos) {
                    unit_.strings.push_back(raw.substr(colon + 1, j - colon - 1));
                    hole->str = static_cast<std::uint32_t>(unit_.strings.size());   // +1: 0 = nessun formato
                }
                n->list.push_back(hole);
                i = j;
                continue;
            }
            text += c;
        }
        flush();
        return n;
    }
    Node* subExpression(const std::string& src, Pos at) {
        Lexed lx;
        LexOptions lo;
        lo.dialect = Dialect::CSharp;
        lo.file = at.file;
        lex(src, lo, syms_, lx);
        for (Token& tk : lx.tokens) { tk.pos.line = at.line; tk.pos.col = static_cast<std::uint16_t>(at.col + tk.pos.col); }
        if (lx.tokens.size() <= 1) fail("buco vuoto nella stringa interpolata", at);
        CsParser sub(lx, unit_, ctx_);
        sub.depth_ = depth_;
        Node* e = sub.expression();
        if (!sub.atEnd()) fail("espressione non valida nella stringa interpolata", at);
        return e;
    }
    Node* postfix(Node* e) {
        while (true) {
            Depth guard(*this);
            const Pos at = cur().pos;
            if (isP(LParen)) {
                Node* c = pool_.node(K::Call, at);
                c->a = e;
                c->list = arguments();
                e = c;
                continue;
            }
            if (isP(LBracket)) {
                ++p_;
                Node* ix = pool_.node(K::Index, at);
                ix->a = e;
                ix->list.push_back(expression());
                while (accept(Comma)) ix->list.push_back(expression());
                expect(RBracket);
                e = ix;
                continue;
            }
            if (isP(Dot)) {
                ++p_;
                Node* m = pool_.node(K::Member, at);
                m->a = e;
                m->sym = ident("il nome di un membro");
                if (isP(Lt) && genericArgsAhead()) {
                    const std::size_t save = p_;
                    try { m->targs = typeArgs(); } catch (const Error&) { p_ = save; m->targs.clear(); }
                }
                e = m;
                continue;
            }
            if (isP(QDot)) {
                ++p_;
                Node* m = pool_.node(K::NullCond, at);
                m->a = e;
                m->sym = ident("il nome di un membro");
                e = m;
                continue;
            }
            if (isP(PlusPlus)) { ++p_; Node* n = pool_.node(K::PostInc, at); n->a = e; e = n; continue; }
            if (isP(MinusMinus)) { ++p_; Node* n = pool_.node(K::PostDec, at); n->a = e; e = n; continue; }
            if (isP(Bang) && (isP(peek(), Dot) || isP(peek(), RParen) || isP(peek(), Semi))) { ++p_; continue; }   // x! (null-forgiving)
            return e;
        }
    }

    const Lexed& lx_;
    std::vector<Token> toks_;
    Unit& unit_;
    ParseContext& ctx_;
    Symbols& syms_;
    NodePool& pool_;
    std::size_t p_{0};
    int depth_{0};
    std::size_t polls_{0};
    Sym className_{0};
    std::vector<Node*> classStack_;                     // classi aperte (funzioni locali)
    bool staticCtx_{false};                             // corpo di un metodo static
    std::vector<std::pair<std::size_t, P>> splits_;
};

} // namespace

void parseCSharp(const Lexed& lexed, Unit& unit, ParseContext& ctx) {
    CsParser p(lexed, unit, ctx);
    p.run();
}

} // namespace fx::prog::ast
