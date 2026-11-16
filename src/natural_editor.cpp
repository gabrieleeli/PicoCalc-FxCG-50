#include "natural_editor.hpp"
#include "expression.hpp"
#include "math_renderer.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace fx {
namespace {
using NodePtr = std::shared_ptr<NaturalNode>;
using ast::Kind;
constexpr std::size_t MaxUndo = 32;

NodePtr node(Kind k, std::string text = {}) { return ast::make(k, std::move(text)); }
std::size_t countNodes(const NaturalNode& n) {
    std::size_t total = 1;
    for (const auto& c : n.children) if (c) total += countNodes(*c);
    return total;
}
// Number of nested template boxes (rows do not count).
std::size_t templateDepth(const NaturalNode& n) {
    std::size_t inner = 0;
    for (const auto& c : n.children) if (c) inner = std::max(inner, templateDepth(*c));
    return (n.kind == Kind::Row || n.kind == Kind::Token) ? inner : inner + 1;
}
NodePtr row() { return node(Kind::Row); }
NodePtr token(std::string t) { return node(Kind::Token, std::move(t)); }

bool alphaWord(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) if (!std::isalpha(c)) return false;
    return true;
}
bool alnumToken(const NaturalNode& n) {
    if (n.kind != Kind::Token || n.text.empty()) return false;
    for (unsigned char c : n.text) if (!(std::isalnum(c) || c == '.')) return false;
    return true;
}
const char* const kGreek[] = {"alpha","beta","gamma","delta","epsilon","zeta","eta","theta","iota","kappa",
    "lambda","mu","nu","xi","omicron","rho","sigma","tau","upsilon","phi","chi","psi","omega"};
std::string lowerCopy(std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; }
bool isWord(const std::string& s) {
    if (s.size() < 2) return false;
    const std::string low = lowerCopy(s);
    if (isReservedWord(low)) return true;
    for (const char* g : kGreek) if (low == g) return true;
    return false;
}
unsigned autoRootIndex(const std::string& s) {
    if (s == "sqrt") return 2;
    if (s == "cbrt") return 3;
    if (s.size() >= 5 && s.size() <= 6 && s.compare(0, 4, "root") == 0) {
        char* end = nullptr; const unsigned long n = std::strtoul(s.c_str() + 4, &end, 10);
        if (end && *end == '\0' && n >= 2 && n <= 99) return static_cast<unsigned>(n);
    }
    return 0;
}
// Trailing operand for ^ and fraction templates: a number/identifier run, a
// parenthesised group "( ... )" or a single template box.
std::pair<std::size_t, NodePtr> takeTrailingOperand(NaturalNode& line, std::size_t cursor) {
    if (cursor == 0) throw SyntaxError("Syntax ERROR: manca l'operando a sinistra");
    std::size_t begin = cursor;
    const NaturalNode& last = *line.children[cursor - 1];
    auto closing = [](const NaturalNode& n) { return n.kind == Kind::Token && (n.text == ")" || n.text == "]" || n.text == "}"); };
    auto opening = [](const NaturalNode& n) { return n.kind == Kind::Token && (n.text == "(" || n.text == "[" || n.text == "{"); };
    if (closing(last)) {
        // Gruppo tra parentesi, matrice [[...]] o lista {...} digitati: tutto il gruppo e' l'operando.
        int depth = 0;
        while (begin > 0) {
            const NaturalNode& n = *line.children[begin - 1];
            if (closing(n)) ++depth;
            if (opening(n)) --depth;
            --begin;
            if (depth == 0) break;
        }
        if (depth != 0) throw SyntaxError("Syntax ERROR: parentesi non bilanciate");
        // Include a function name directly before the parenthesis: sin(x)^2.
        while (begin > 0 && alnumToken(*line.children[begin - 1]) && alphaWord(line.children[begin - 1]->text)) --begin;
    } else if (last.kind == Kind::Token) {
        if (!alnumToken(last)) throw SyntaxError("Syntax ERROR: operando non valido prima del modello");
        // Come sulla fx-CG50 l'operando e' il valore subito prima del cursore: un
        // numero (cifre e punto) oppure una sola variabile o parola. G T x² eleva
        // al quadrato solo T (v1.12.0: prima "GT" diventava (GT)²).
        auto numeric = [](const NaturalNode& n) {
            for (unsigned char ch : n.text) if (!(std::isdigit(ch) || ch == '.')) return false;
            return true;
        };
        if (numeric(last)) { while (begin > 0 && alnumToken(*line.children[begin - 1]) && numeric(*line.children[begin - 1])) --begin; }
        else --begin;
        // Mat A, Vct B, List 12: il nome della memoria (un token o le lettere
        // digitate una per una) fa parte dell'operando.
        std::string word;
        std::size_t k = begin;
        while (k > 0 && word.size() < 4 && line.children[k - 1]->kind == Kind::Token && alphaWord(line.children[k - 1]->text)) {
            word.insert(0, line.children[k - 1]->text);
            --k;
            if (word == "Mat" || word == "Vct" || word == "List") { begin = k; break; }
        }
    } else {
        --begin;
    }
    auto operand = row();
    operand->children.assign(line.children.begin() + static_cast<std::ptrdiff_t>(begin), line.children.begin() + static_cast<std::ptrdiff_t>(cursor));
    line.children.erase(line.children.begin() + static_cast<std::ptrdiff_t>(begin), line.children.begin() + static_cast<std::ptrdiff_t>(cursor));
    return {begin, operand};
}
bool rowIsEmpty(const NaturalNode& n) { return n.kind == Kind::Row && n.children.empty(); }
bool boxIsEmpty(const NaturalNode& box) {
    for (const auto& c : box.children) if (c && !rowIsEmpty(*c)) return false;
    return true;
}
bool isBox(const NaturalNode& n) {
    if (n.kind == Kind::Token) return false;
    for (const auto& c : n.children) if (c && c->kind == Kind::Row) return true;
    return false;
}

// ---- structural importer -------------------------------------------------
std::size_t matchParen(const std::string& s, std::size_t open) {
    int depth = 0;
    for (std::size_t i = open; i < s.size(); ++i) {
        if (s[i] == '(') ++depth;
        else if (s[i] == ')') { if (--depth == 0) return i; }
    }
    return std::string::npos;
}
std::vector<std::string> splitArgs(const std::string& s) {
    std::vector<std::string> out; int depth = 0; std::size_t start = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '(') ++depth; else if (s[i] == ')') --depth;
        else if (s[i] == ',' && depth == 0) { out.push_back(s.substr(start, i - start)); start = i + 1; }
    }
    out.push_back(s.substr(start));
    return out;
}
void importInto(NaturalNode& target, const std::string& s, int depth, bool linear = false);
NodePtr importRow(const std::string& s, int depth) { auto r = row(); importInto(*r, s, depth + 1); return r; }
// "[[a,b][c,d]]" a partire da `open`: modello Matrix con una riga per cella.
// Restituisce l'indice dopo la quadra di chiusura, oppure npos quando il testo
// non e' una matrice rettangolare (allora si importa come token semplici).
std::size_t importMatrix(NaturalNode& target, const std::string& s, std::size_t open, int depth) {
    if (open + 1 >= s.size() || s[open] != '[' || s[open + 1] != '[') return std::string::npos;
    std::vector<std::string> cells;
    std::size_t cols = 0, i = open + 1;
    while (i < s.size() && s[i] == '[') {
        // Una riga: celle separate da virgole al livello 0, fino alla ']' corrispondente.
        int level = 0;
        std::size_t start = i + 1, j = i + 1, count = 0;
        for (; j < s.size(); ++j) {
            const char c = s[j];
            if (c == '(' || c == '[' || c == '{') ++level;
            else if (c == ')' || c == '}' || (c == ']' && level > 0)) --level;
            else if (c == ']' && level == 0) break;
            else if (c == ',' && level == 0) { cells.push_back(s.substr(start, j - start)); ++count; start = j + 1; }
        }
        if (j >= s.size()) return std::string::npos;
        cells.push_back(s.substr(start, j - start)); ++count;
        if (cols == 0) cols = count; else if (count != cols) return std::string::npos;
        i = j + 1;
    }
    if (i >= s.size() || s[i] != ']' || cells.empty() || cols > 99) return std::string::npos;
    auto m = node(Kind::Matrix, std::to_string(cols));
    for (const auto& c : cells) m->children.push_back(importRow(c, depth));
    target.children.push_back(m);
    return i + 1;
}
std::size_t utf8Len(unsigned char c) { return c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : 4; }
void appendToken(NaturalNode& r, std::string text) {
    r.children.push_back(token(std::move(text)));
    // Merge letters into known words (sin, pi, ans, ...).
    std::size_t begin = r.children.size(); std::string tail;
    while (begin > 0) { const auto& n = *r.children[begin - 1]; if (n.kind != Kind::Token || !alphaWord(n.text)) break; tail.insert(0, n.text); --begin; }
    if (tail.size() < 2) return;
    // Longest suffix (aligned with token boundaries) that is a known word.
    std::size_t pos = begin, offset = 0;
    while (pos < r.children.size()) {
        const std::string suffix = tail.substr(offset);
        if (isWord(suffix)) {
            if (r.children.size() - pos <= 1) return;
            r.children.erase(r.children.begin() + static_cast<std::ptrdiff_t>(pos), r.children.end());
            r.children.push_back(token(suffix));
            return;
        }
        offset += r.children[pos]->text.size(); ++pos;
    }
}
void importInto(NaturalNode& target, const std::string& s, int depth, bool linear) {
    if (depth > static_cast<int>(NaturalEditor::MaxTemplateDepth)) throw MathError("Stack ERROR: annidamento eccessivo");
    std::size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (std::isspace(c)) { ++i; continue; }
        // Simbolo ingegneristico "#k" (v1.11.0): un token, mostrato "k" / "µ".
        if (c == '#' && i + 1 < s.size() && std::strchr("kMGTmunpf", s[i + 1])) { target.children.push_back(token(s.substr(i, 2))); i += 2; continue; }
        // SimRes, SimCoef, PlyRes, PlyCoef (VARS › EQUATION, v1.12.0): un token ciascuno.
        if (c == 'S' || c == 'P') {
            bool done = false;
            for (int k = 0; k < 4 && !done; ++k) {
                const std::string word = equationMatrixName(k);
                if (s.compare(i, word.size(), word) != 0) continue;
                target.children.push_back(token(word)); i += word.size(); done = true;
            }
            if (done) continue;
        }
        // Mat / Vct / List (comandi OPTN) sono un token ciascuno, anche quando il
        // nome segue senza spazio ("MatA" = Mat A).
        if (c == 'M' || c == 'V' || c == 'L') {
            bool done = false;
            for (const char* word : {"Mat", "Vct", "List"}) {
                const std::size_t n = std::strlen(word);
                if (s.compare(i, n, word) != 0) continue;
                std::size_t e = i; while (e < s.size() && (std::isalnum(static_cast<unsigned char>(s[e])) || s[e] == '_')) ++e;
                const std::string rest = s.substr(i + n, e - i - n);
                const bool number = !rest.empty() && std::all_of(rest.begin(), rest.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
                if (!number && isReservedWord(lowerCopy(s.substr(i, e - i)))) break;   // nome di funzione, es. "mattolist"
                target.children.push_back(token(word)); i += n; done = true;
                break;
            }
            if (done) continue;
        }
        if (linear) {
            if (c == '@') {
                std::size_t e = i + 1;
                while (e < s.size() && std::isalpha(static_cast<unsigned char>(s[e]))) ++e;
                while (e < s.size() && std::isdigit(static_cast<unsigned char>(s[e]))) ++e;
                target.children.push_back(token(s.substr(i, e - i))); i = e; continue;
            }
            const std::size_t len = utf8Len(c);
            appendToken(target, s.substr(i, len));
            i += len;
            continue;
        }
        if (c == '[') {
            const std::size_t next = importMatrix(target, s, i, depth);
            if (next != std::string::npos) { i = next; continue; }
        }
        // "(a)/(b)" -> fraction, "(a)^(b)" -> power. "((a)/(b))" e' una frazione
        // che il serializzatore ha racchiuso perche' seguita da un fattore implicito.
        if (c == '(') {
            const std::size_t close = matchParen(s, i);
            if (close != std::string::npos && i + 1 < close && s[i + 1] == '(') {
                const std::size_t a = matchParen(s, i + 1);
                if (a != std::string::npos && a + 2 < close && s[a + 1] == '/' && s[a + 2] == '(' && matchParen(s, a + 2) == close - 1) {
                    auto box = node(Kind::Fraction);
                    box->children = {importRow(s.substr(i + 2, a - i - 2), depth), importRow(s.substr(a + 3, close - 1 - a - 3), depth)};
                    target.children.push_back(box); i = close + 1; continue;
                }
            }
            if (close != std::string::npos && close + 2 < s.size() && (s[close + 1] == '/' || s[close + 1] == '^') && s[close + 2] == '(') {
                const std::size_t close2 = matchParen(s, close + 2);
                if (close2 != std::string::npos) {
                    auto box = node(s[close + 1] == '/' ? Kind::Fraction : Kind::Power);
                    box->children = {importRow(s.substr(i + 1, close - i - 1), depth), importRow(s.substr(close + 3, close2 - close - 3), depth)};
                    target.children.push_back(box); i = close2 + 1; continue;
                }
            }
            appendToken(target, "("); ++i; continue;
        }
        if (c == '^') {
            ++i;
            std::size_t cur = target.children.size();
            auto operand = cur ? takeTrailingOperand(target, cur) : std::pair<std::size_t, NodePtr>{0, row()};
            auto box = node(Kind::Power);
            std::string exponent;
            if (i < s.size() && s[i] == '(') {
                const std::size_t close = matchParen(s, i);
                if (close == std::string::npos) { exponent = s.substr(i + 1); i = s.size(); }
                else { exponent = s.substr(i + 1, close - i - 1); i = close + 1; }
            } else {
                std::size_t b = i;
                if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
                while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '.')) ++i;
                exponent = s.substr(b, i - b);
            }
            box->children = {operand.second, importRow(exponent, depth)};
            target.children.insert(target.children.begin() + static_cast<std::ptrdiff_t>(operand.first), box);
            continue;
        }
        if (c == '@') {                 // graph function reference (@Y3, @Xt2): one atomic token
            std::size_t e = i + 1;
            while (e < s.size() && std::isalpha(static_cast<unsigned char>(s[e]))) ++e;
            while (e < s.size() && std::isdigit(static_cast<unsigned char>(s[e]))) ++e;
            target.children.push_back(token(s.substr(i, e - i))); i = e; continue;
        }
        if (c == '_') {                 // repeating digits of a decimal literal
            std::size_t e = i + 1; while (e < s.size() && std::isdigit(static_cast<unsigned char>(s[e]))) ++e;
            auto box = node(Kind::Call, "period");
            box->children = {importRow(s.substr(i + 1, e - i - 1), depth)};
            target.children.push_back(box); i = e; continue;
        }
        if (std::isalpha(c)) {
            std::size_t e = i; while (e < s.size() && (std::isalnum(static_cast<unsigned char>(s[e])) || s[e] == '_')) ++e;
            const std::string word = s.substr(i, e - i);
            const std::string low = lowerCopy(word);
            if (e < s.size() && s[e] == '(') {
                const std::size_t close = matchParen(s, e);
                const std::string inner = close == std::string::npos ? s.substr(e + 1) : s.substr(e + 1, close - e - 1);
                const std::size_t next = close == std::string::npos ? s.size() : close + 1;
                const auto args = splitArgs(inner);
                NodePtr box;
                if ((low == "sqrt" || low == "cbrt") && args.size() == 1) { box = node(Kind::Radical, low == "sqrt" ? "2" : "3"); box->children = {importRow(inner, depth)}; }
                else if (autoRootIndex(low) && args.size() == 1) { box = node(Kind::Radical, std::to_string(autoRootIndex(low))); box->children = {importRow(inner, depth)}; }
                else if (low == "root" && args.size() == 2) {
                    const long idx = std::strtol(args[0].c_str(), nullptr, 10);
                    if (idx >= 2 && idx <= 99 && std::to_string(idx) == args[0]) { box = node(Kind::Radical, args[0]); box->children = {importRow(args[1], depth)}; }
                } else if (low == "frac" && args.size() == 2) { box = node(Kind::Fraction); box->children = {importRow(args[0], depth), importRow(args[1], depth)}; }
                else if (low == "mixed" && args.size() == 3) { box = node(Kind::Call, "mixed"); box->children = {importRow(args[0], depth), importRow(args[1], depth), importRow(args[2], depth)}; }
                else if (low == "abs" && args.size() == 1) { box = node(Kind::Call, "abs"); box->children = {importRow(inner, depth)}; }
                else if (low == "log" && args.size() == 2) { box = node(Kind::Call, "log"); box->children = {importRow(args[0], depth), importRow(args[1], depth)}; }
                else if ((low == "integral" || low == "sum" || low == "prod") && args.size() == 3) { box = node(Kind::Calculus, low); box->children = {importRow(args[0], depth), importRow(args[1], depth), importRow(args[2], depth)}; }
                else if (low == "derivative" && args.size() == 2) { box = node(Kind::Calculus, "derivative"); box->children = {importRow(args[0], depth), importRow(args[1], depth)}; }
                if (box) { target.children.push_back(box); i = next; continue; }
            }
            for (char ch : word) appendToken(target, std::string(1, ch));
            i = e; continue;
        }
        const std::size_t len = utf8Len(c);
        appendToken(target, s.substr(i, len));
        i += len;
    }
}
} // namespace

ast::NodePtr cloneTree(const NaturalNode& n) {
    auto out = ast::make(n.kind, n.text);
    out->children.reserve(n.children.size());
    for (const auto& c : n.children) out->children.push_back(c ? cloneTree(*c) : nullptr);
    return out;
}

NaturalEditor::NaturalEditor() { clear(); undo_.clear(); }
void NaturalEditor::clear() {
    if (root_ && !root_->children.empty()) pushUndo();
    selecting_ = false; errorMark_ = Range{};
    root_ = row(); rowPath_.clear(); cursor_ = 0; error_.clear();
    autoRootParenPending_ = false; autoRootParenOpened_ = false;
}
bool NaturalEditor::empty() const noexcept { return !root_ || root_->children.empty(); }
void NaturalEditor::setExpression(const std::string& expression) {
    auto r = row();
    importInto(*r, expression, 0, linear_);
    selecting_ = false; errorMark_ = Range{};
    // Imported text may be slightly larger than the tree it was produced from.
    if (countNodes(*r) > 2 * MaxNodes) throw MathError("Memory ERROR: espressione troppo lunga");
    if (templateDepth(*r) > MaxTemplateDepth) throw MathError("Stack ERROR: annidamento eccessivo");
    root_ = r; rowPath_.clear(); cursor_ = root_->children.size(); error_.clear();
    autoRootParenPending_ = false; autoRootParenOpened_ = false;
}
void NaturalEditor::setTree(const NaturalNode& tree) {
    selecting_ = false; errorMark_ = Range{};
    root_ = cloneTree(tree);
    if (root_->kind != Kind::Row) { auto r = row(); r->children.push_back(root_); root_ = r; }
    rowPath_.clear(); cursor_ = root_->children.size(); error_.clear();
    autoRootParenPending_ = false; autoRootParenOpened_ = false;
}
NaturalNode& NaturalEditor::activeRow() {
    NaturalNode* n = root_.get();
    for (auto i : rowPath_) { if (i >= n->children.size()) throw std::logic_error("Percorso cursore non valido"); n = n->children[i].get(); }
    if (n->kind != Kind::Row) throw std::logic_error("Il cursore non punta a una riga");
    return *n;
}
const NaturalNode& NaturalEditor::activeRow() const { return const_cast<NaturalEditor*>(this)->activeRow(); }
const NaturalNode* NaturalEditor::activeRowPtr() const { return &activeRow(); }
NaturalNode* NaturalEditor::parentBox() {
    if (rowPath_.size() < 2) return nullptr;
    NaturalNode* n = root_.get();
    for (std::size_t i = 0; i + 1 < rowPath_.size(); ++i) n = n->children.at(rowPath_[i]).get();
    return n;
}
void NaturalEditor::checkLimits() {
    const std::size_t depth = templateDepth(*root_);
    if (depth <= MaxTemplateDepth && countNodes(*root_) <= MaxNodes) return;
    if (!undo_.empty()) {
        auto s = std::move(undo_.back()); undo_.pop_back();
        root_ = s.root; rowPath_ = s.path; cursor_ = s.cursor;
    }
    autoRootParenPending_ = false; autoRootParenOpened_ = false;
    throw MathError(depth > MaxTemplateDepth ? "Stack ERROR: troppi modelli annidati" : "Memory ERROR: espressione troppo lunga");
}
void NaturalEditor::pushUndo() {
    // Ogni modifica termina la selezione CLIP e toglie l'evidenziazione dell'errore.
    selecting_ = false;
    errorMark_ = Range{};
    if (!root_) return;
    undo_.push_back({cloneTree(*root_), rowPath_, cursor_});
    if (undo_.size() > MaxUndo) undo_.erase(undo_.begin());
    redo_.clear();
}
bool NaturalEditor::undo() {
    selecting_ = false; errorMark_ = Range{};
    if (undo_.empty()) return false;
    redo_.push_back({cloneTree(*root_), rowPath_, cursor_});
    auto s = std::move(undo_.back()); undo_.pop_back();
    root_ = s.root; rowPath_ = s.path; cursor_ = s.cursor;
    try { (void)activeRow(); } catch (...) { rowPath_.clear(); cursor_ = root_->children.size(); }
    return true;
}
bool NaturalEditor::redo() {
    selecting_ = false; errorMark_ = Range{};
    if (redo_.empty()) return false;
    undo_.push_back({cloneTree(*root_), rowPath_, cursor_});
    auto s = std::move(redo_.back()); redo_.pop_back();
    root_ = s.root; rowPath_ = s.path; cursor_ = s.cursor;
    try { (void)activeRow(); } catch (...) { rowPath_.clear(); cursor_ = root_->children.size(); }
    return true;
}
ast::NodePtr NaturalEditor::copyTree() const { return cloneTree(*root_); }
void NaturalEditor::paste(const NaturalNode& tree) {
    selecting_ = false;
    pushUndo();
    auto copy = cloneTree(tree);
    if (linear_) {
        // Modalita' Line: i modelli si incollano come testo lineare.
        auto flat = row();
        importInto(*flat, ast::serialize(*copy), 0, true);
        copy = flat;
    }
    auto& r = activeRow();
    std::vector<NodePtr> items;
    if (copy->kind == Kind::Row) items = copy->children; else items.push_back(copy);
    r.children.insert(r.children.begin() + static_cast<std::ptrdiff_t>(cursor_), items.begin(), items.end());
    cursor_ += items.size();
    checkLimits();
}

void NaturalEditor::insertNode(NodePtr n) {
    auto& r = activeRow();
    r.children.insert(r.children.begin() + static_cast<std::ptrdiff_t>(cursor_), std::move(n));
    ++cursor_;
    error_.clear();
}
void NaturalEditor::mergeWord() {
    auto& r = activeRow();
    // Collect alphabetic tokens immediately before the cursor.
    std::size_t begin = cursor_; std::string tail;
    while (begin > 0) {
        const auto& n = *r.children[begin - 1];
        if (n.kind != Kind::Token || !alphaWord(n.text)) break;
        tail.insert(0, n.text); --begin;
    }
    if (tail.size() < 2) return;
    std::size_t pos = begin, offset = 0;
    while (pos < cursor_) {
        const std::string suffix = tail.substr(offset);
        if (isWord(suffix)) {
            if (cursor_ - pos <= 1) return;   // already a single token
            r.children.erase(r.children.begin() + static_cast<std::ptrdiff_t>(pos), r.children.begin() + static_cast<std::ptrdiff_t>(cursor_));
            r.children.insert(r.children.begin() + static_cast<std::ptrdiff_t>(pos), token(suffix));
            cursor_ = pos + 1;
            return;
        }
        offset += r.children[pos]->text.size(); ++pos;
    }
}

void NaturalEditor::insert(char c) {
    if (c == '^') { insertPower(); return; }
    if (c == ' ') return;
    // An optional "(" after an auto-recognised root alias (sqrt(, cbrt(, root5( ) is absorbed;
    // the matching ")" leaves the radical, keeping it one VPAM node.
    if (c == '(' && autoRootParenPending_) { autoRootParenPending_ = false; autoRootParenOpened_ = true; return; }
    if (c == ')' && autoRootParenOpened_) {
        if (rowPath_.size() >= 2) { const auto at = rowPath_[rowPath_.size() - 2]; rowPath_.resize(rowPath_.size() - 2); cursor_ = at + 1; }
        autoRootParenOpened_ = false; error_.clear(); return;
    }
    autoRootParenPending_ = false;
    // '[' ']' delimitano le matrici e i suffissi di GRAPH ",[a,b]" / ",[A=...]",
    // '{' '}' le liste, '@' inizia un riferimento a una funzione di GRAPH; il
    // parser li rifiuta dove non sono validi.
    // '"' delimita il testo del comando grafico Text (v1.11.0).
    if (!(std::isalnum(static_cast<unsigned char>(c)) || std::strchr(".+-*/%!,()=<>~[]{}@\"", c) != nullptr))
        throw std::invalid_argument("Carattere non ammesso nell'espressione");
    pushUndo();
    argumentPending_ = false;
    overwriteAtCursor();
    insertNode(token(std::string(1, c)));
    if (std::isalnum(static_cast<unsigned char>(c)) && !linear_) {
        auto& r = activeRow();
        std::string tail; std::size_t begin = cursor_;
        while (begin > 0 && alnumToken(*r.children[begin - 1])) { tail.insert(0, r.children[begin - 1]->text); --begin; }
        // Auto templates: the alias must be the whole trailing word or follow a digit/operator.
        for (std::size_t cut = 0; cut < tail.size(); ++cut) {
            const std::string suffix = tail.substr(cut);
            const unsigned index = autoRootIndex(suffix);
            if (!index) continue;
            if (cut > 0 && std::isalpha(static_cast<unsigned char>(tail[cut - 1]))) continue;
            // Remove the alias characters (tokens may be merged words).
            std::size_t removeChars = suffix.size(), pos = cursor_;
            while (removeChars > 0 && pos > 0) { const std::size_t len = r.children[pos - 1]->text.size(); if (len > removeChars) break; removeChars -= len; --pos; }
            if (removeChars != 0) break;
            r.children.erase(r.children.begin() + static_cast<std::ptrdiff_t>(pos), r.children.begin() + static_cast<std::ptrdiff_t>(cursor_));
            cursor_ = pos;
            auto box = node(Kind::Radical, std::to_string(index));
            box->children = {row()};
            r.children.insert(r.children.begin() + static_cast<std::ptrdiff_t>(cursor_), box);
            rowPath_.push_back(cursor_); rowPath_.push_back(0); cursor_ = 0;
            autoRootParenPending_ = true; autoRootParenOpened_ = false;
            checkLimits();
            return;
        }
        if (std::isalpha(static_cast<unsigned char>(c))) mergeWord();
    } else if (std::isalpha(static_cast<unsigned char>(c))) {
        mergeWord();
    }
    checkLimits();
}
// Modalita' Line: un tasto di modello inserisce il suo testo lineare; il
// cursore finisce `cursorBack` token prima della fine (dentro "[[,][,]]").
void NaturalEditor::insertLinear(const std::string& text, std::size_t cursorBack) {
    autoRootParenPending_ = false;
    pushUndo();
    overwriteAtCursor();
    auto flat = row();
    importInto(*flat, text, 0, true);
    auto& r = activeRow();
    r.children.insert(r.children.begin() + static_cast<std::ptrdiff_t>(cursor_), flat->children.begin(), flat->children.end());
    cursor_ += flat->children.size() - std::min(cursorBack, flat->children.size());
    error_.clear();
    checkLimits();
}
void NaturalEditor::insertText(const std::string& text) {
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        const std::size_t len = utf8Len(c);
        if (len == 1) insert(static_cast<char>(c)); else insertSymbol(text.substr(i, len));
        i += len;
    }
}
void NaturalEditor::insertSymbol(const std::string& utf8) {
    if (utf8.empty()) return;
    pushUndo();
    autoRootParenPending_ = false;
    argumentPending_ = false;
    overwriteAtCursor();
    insertNode(token(utf8));
    checkLimits();
}
void NaturalEditor::overwriteAtCursor() {
    if (!overwrite()) return;
    auto& r = activeRow();
    if (cursor_ < r.children.size()) r.children.erase(r.children.begin() + static_cast<std::ptrdiff_t>(cursor_));
}
// SHIFT+INS: valore a destra del cursore. Un numero o un nome, un gruppo tra
// parentesi (con il nome della funzione che lo apre: sin(30)) o un modello.
// Le parentesi esterne di un gruppo si tolgono: il modello le sostituisce.
NaturalEditor::NodePtrAlias NaturalEditor::takeArgument() {
    auto& r = activeRow();
    if (cursor_ >= r.children.size()) return nullptr;
    std::size_t end = cursor_;
    auto opening = [](const NaturalNode& n) { return n.kind == Kind::Token && (n.text == "(" || n.text == "[" || n.text == "{"); };
    auto closing = [](const NaturalNode& n) { return n.kind == Kind::Token && (n.text == ")" || n.text == "]" || n.text == "}"); };
    bool group = false;
    const NaturalNode& first = *r.children[cursor_];
    if (opening(first) || (alnumToken(first) && alphaWord(first.text) && cursor_ + 1 < r.children.size() && opening(*r.children[cursor_ + 1]))) {
        std::size_t k = opening(first) ? cursor_ : cursor_ + 1;
        group = opening(first);
        int depth = 0;
        for (; k < r.children.size(); ++k) {
            if (opening(*r.children[k])) ++depth;
            else if (closing(*r.children[k]) && --depth == 0) break;
        }
        end = std::min(k + 1, r.children.size());          // la chiusa puo' mancare alla fine dell'input
    } else if (first.kind == Kind::Token) {
        if (!alnumToken(first)) return nullptr;
        while (end < r.children.size() && alnumToken(*r.children[end])) ++end;
    } else {
        end = cursor_ + 1;                                   // un modello intero
    }
    auto arg = row();
    std::size_t b = cursor_, e = end;
    if (group && e - b >= 2 && closing(*r.children[e - 1])) { ++b; --e; }
    else if (group && e - b >= 1) ++b;
    arg->children.assign(r.children.begin() + static_cast<std::ptrdiff_t>(b), r.children.begin() + static_cast<std::ptrdiff_t>(e));
    r.children.erase(r.children.begin() + static_cast<std::ptrdiff_t>(cursor_), r.children.begin() + static_cast<std::ptrdiff_t>(end));
    return arg;
}
void NaturalEditor::insertBox(NodePtr box, std::size_t focusChild) {
    if (linear_) throw std::invalid_argument("Modello non disponibile in modalit\xC3\xA0 Line");
    const bool argument = argumentPending_;
    pushUndo();
    autoRootParenPending_ = false;
    argumentPending_ = false;
    // SHIFT+INS: il valore a destra del cursore diventa l'argomento del modello.
    if (argument) if (auto arg = takeArgument()) box->children[focusChild] = arg;
    auto& r = activeRow();
    r.children.insert(r.children.begin() + static_cast<std::ptrdiff_t>(cursor_), box);
    rowPath_.push_back(cursor_); rowPath_.push_back(focusChild);
    cursor_ = box->children[focusChild]->children.size();
    error_.clear();
    checkLimits();
}
void NaturalEditor::insertFraction() {
    // Line (fx-CG50): il tasto frazione scrive ⌟ ("2⌟3"); la divisione resta "/".
    if (linear_) { insertLinear("\xE2\x8C\x9F"); return; }
    if (argumentPending_) {
        // SHIFT+INS: il valore a destra diventa il numeratore.
        auto f = node(Kind::Fraction); f->children = {row(), row()};
        insertBox(f, 0);
        (void)moveDown();
        return;
    }
    pushUndo();
    auto f = node(Kind::Fraction); f->children = {row(), row()};
    auto& r = activeRow();
    std::size_t at = cursor_;
    bool tookOperand = false;
    // Like the fx-CG50 fraction key: a preceding operand becomes the numerator
    // and the cursor moves to the denominator.
    if (cursor_ > 0) {
        const auto& prev = *r.children[cursor_ - 1];
        if (alnumToken(prev) || (prev.kind == Kind::Token && (prev.text == ")" || prev.text == "]" || prev.text == "}")) || isBox(prev)) {
            auto operand = takeTrailingOperand(r, cursor_); at = operand.first; f->children[0] = operand.second; tookOperand = true;
        }
    }
    r.children.insert(r.children.begin() + static_cast<std::ptrdiff_t>(at), f);
    rowPath_.push_back(at); rowPath_.push_back(tookOperand ? 1 : 0);
    cursor_ = 0;
    autoRootParenPending_ = false;
    checkLimits();
}
void NaturalEditor::insertMixedFraction() {
    // fx-CG50: in Line la frazione mista si scrive con due ⌟ (1⌟2⌟3).
    if (linear_) { insertLinear("\xE2\x8C\x9F"); return; }
    auto m = node(Kind::Call, "mixed"); m->children = {row(), row(), row()};
    insertBox(m, 0);
}
void NaturalEditor::insertPeriod() {
    // The period must follow the decimal digits of a number ("0.1", "2.").
    const auto& r = activeRow();
    bool point = false;
    for (std::size_t k = cursor_; k > 0; --k) {
        const auto& t = *r.children[k - 1];
        if (t.kind != Kind::Token || t.text.size() != 1) break;
        if (t.text[0] == '.') { point = true; break; }
        if (!std::isdigit(static_cast<unsigned char>(t.text[0]))) break;
    }
    if (!point) throw SyntaxError("Syntax ERROR: il periodo va dopo la parte decimale (es. 0.1)");
    if (linear_) { pushUndo(); insertNode(token("_")); return; }
    auto p = node(Kind::Call, "period"); p->children = {row()};
    insertBox(p, 0);
}
void NaturalEditor::insertRadical() { insertNthRoot(2); }
void NaturalEditor::insertRootTemplate() {
    if (linear_) { insertLinear("root("); return; }
    auto r = node(Kind::Call, "root"); r->children = {row(), row()};
    insertBox(r, 0);
}
void NaturalEditor::insertNthRoot(unsigned index) {
    if (index < 2 || index > 99) throw std::invalid_argument("Indice della radice: usare 2-99");
    if (linear_) { insertLinear(index == 2 ? "\xE2\x88\x9A(" : index == 3 ? "\xE2\x88\x9B(" : "root(" + std::to_string(index) + ","); return; }
    auto f = node(Kind::Radical, std::to_string(index)); f->children = {row()};
    insertBox(f, 0);
}
void NaturalEditor::insertPower() {
    if (linear_) { pushUndo(); autoRootParenPending_ = false; insertNode(token("^")); checkLimits(); return; }
    auto& r = activeRow();
    pushUndo();
    autoRootParenPending_ = false;
    const bool hasOperand = cursor_ > 0 && (alnumToken(*r.children[cursor_ - 1]) ||
        (r.children[cursor_ - 1]->kind == Kind::Token && (r.children[cursor_ - 1]->text == ")" || r.children[cursor_ - 1]->text == "]" ||
                                                          r.children[cursor_ - 1]->text == "}")) || isBox(*r.children[cursor_ - 1]));
    if (!hasOperand) {
        // Like the CG50 x^ key without a preceding value: empty base box, cursor in the base.
        auto p = node(Kind::Power); p->children = {row(), row()};
        r.children.insert(r.children.begin() + static_cast<std::ptrdiff_t>(cursor_), p);
        rowPath_.push_back(cursor_); rowPath_.push_back(0); cursor_ = 0;
        checkLimits();
        return;
    }
    auto operand = takeTrailingOperand(r, cursor_);
    auto p = node(Kind::Power); p->children = {operand.second, row()};
    r.children.insert(r.children.begin() + static_cast<std::ptrdiff_t>(operand.first), p);
    rowPath_.push_back(operand.first); rowPath_.push_back(1); cursor_ = 0;
    checkLimits();
}
void NaturalEditor::insertSquare() {
    if (linear_) { insertLinear("^2"); return; }
    insertPower();
    insertNode(token("2"));
    (void)leaveChild(true);
    checkLimits();
}
void NaturalEditor::insertSubscript() {
    if (linear_) throw std::invalid_argument("Modello non disponibile in modalit\xC3\xA0 Line");
    auto& r = activeRow();
    pushUndo();
    auto operand = takeTrailingOperand(r, cursor_);
    auto p = node(Kind::Subscript); p->children = {operand.second, row()};
    r.children.insert(r.children.begin() + static_cast<std::ptrdiff_t>(operand.first), p);
    rowPath_.push_back(operand.first); rowPath_.push_back(1); cursor_ = 0;
    checkLimits();
}
void NaturalEditor::insertGroup() { if (linear_) { insertLinear("("); return; } auto g = node(Kind::Group); g->children = {row()}; insertBox(g, 0); }
void NaturalEditor::insertAbs() { if (linear_) { insertLinear("abs("); return; } auto g = node(Kind::Call, "abs"); g->children = {row()}; insertBox(g, 0); }
void NaturalEditor::insertLogBase() { if (linear_) { insertLinear("log("); return; } auto g = node(Kind::Call, "log"); g->children = {row(), row()}; insertBox(g, 0); }
void NaturalEditor::insertExp() {
    if (linear_) { insertLinear("e^("); return; }
    auto p = node(Kind::Power); auto base = row(); base->children.push_back(token("e")); p->children = {base, row()}; insertBox(p, 1);
}
void NaturalEditor::insertTenPower() {
    if (linear_) { insertLinear("10^("); return; }
    auto p = node(Kind::Power); auto base = row(); base->children = {token("1"), token("0")}; p->children = {base, row()}; insertBox(p, 1);
}
void NaturalEditor::insertIntegral() { if (linear_) { insertLinear("integral("); return; } auto n = node(Kind::Calculus, "integral"); n->children = {row(), row(), row()}; insertBox(n, 0); }
void NaturalEditor::insertDerivative() { if (linear_) { insertLinear("derivative("); return; } auto n = node(Kind::Calculus, "derivative"); n->children = {row(), row()}; insertBox(n, 0); }
void NaturalEditor::insertSummation() { if (linear_) { insertLinear("sum("); return; } auto n = node(Kind::Calculus, "sum"); n->children = {row(), row(), row()}; insertBox(n, 0); }
void NaturalEditor::insertProduct() { if (linear_) { insertLinear("prod("); return; } auto n = node(Kind::Calculus, "prod"); n->children = {row(), row(), row()}; insertBox(n, 0); }
void NaturalEditor::insertMatrix(std::size_t rows, std::size_t cols) {
    if (rows < 1 || rows > 6 || cols < 1 || cols > 6) throw std::invalid_argument("Modello MAT/VCT: da 1x1 a 6x6");
    if (linear_) {
        std::string text = "[[";
        for (std::size_t r = 0; r < rows; ++r) {
            if (r) text += "[";
            for (std::size_t c = 1; c < cols; ++c) text += ",";
            text += "]";
        }
        text += "]";
        insertLinear(text, text.size() - 2);           // ogni carattere e' un token: cursore dopo "[["
        return;
    }
    auto m = node(Kind::Matrix, std::to_string(cols));
    for (std::size_t i = 0; i < rows * cols; ++i) m->children.push_back(row());
    insertBox(m, 0);
}
void NaturalEditor::setLinear(bool linear) {
    if (linear == linear_) return;
    overwrite_ = false; argumentPending_ = false;
    std::string text;
    try { text = toExpression(); } catch (...) { text.clear(); }
    linear_ = linear;
    selecting_ = false; errorMark_ = Range{};
    autoRootParenPending_ = autoRootParenOpened_ = false;
    try { setExpression(text); } catch (...) { root_ = row(); rowPath_.clear(); cursor_ = 0; }
}

bool NaturalEditor::enterChild(std::size_t boxIndex, std::size_t child, bool atEnd) {
    auto& r = activeRow();
    if (boxIndex >= r.children.size()) return false;
    auto box = r.children[boxIndex];
    if (child >= box->children.size() || !box->children[child] || box->children[child]->kind != Kind::Row) return false;
    rowPath_.push_back(boxIndex); rowPath_.push_back(child);
    cursor_ = atEnd ? box->children[child]->children.size() : 0;
    return true;
}
bool NaturalEditor::leaveChild(bool right) {
    if (rowPath_.size() < 2) return false;
    const auto boxIndex = rowPath_[rowPath_.size() - 2];
    rowPath_.resize(rowPath_.size() - 2);
    (void)activeRow();
    cursor_ = boxIndex + (right ? 1 : 0);
    autoRootParenOpened_ = false;
    return true;
}
bool NaturalEditor::moveLeft() {
    selecting_ = false;
    argumentPending_ = false;
    autoRootParenPending_ = false;
    auto& r = activeRow();
    if (cursor_ > 0) {
        const auto& prev = *r.children[cursor_ - 1];
        if (isBox(prev)) {
            // Enter the box from the right, into its last editable row.
            for (std::size_t k = prev.children.size(); k-- > 0;)
                if (prev.children[k] && prev.children[k]->kind == Kind::Row) return enterChild(cursor_ - 1, k, true);
        }
        --cursor_; return true;
    }
    // At the start of a child row: go to the previous field of multi-field boxes.
    if (rowPath_.size() >= 2) {
        NaturalNode* box = parentBox();
        const std::size_t slot = rowPath_.back();
        if (box && (box->kind == Kind::Calculus || box->kind == Kind::Call || box->kind == Kind::Matrix) && slot > 0) {
            rowPath_.back() = slot - 1; cursor_ = box->children[slot - 1]->children.size(); return true;
        }
    }
    return leaveChild(false);
}
bool NaturalEditor::moveRight() {
    selecting_ = false;
    argumentPending_ = false;
    autoRootParenPending_ = false;
    auto& r = activeRow();
    if (cursor_ < r.children.size()) {
        const auto& next = *r.children[cursor_];
        if (isBox(next)) {
            for (std::size_t k = 0; k < next.children.size(); ++k)
                if (next.children[k] && next.children[k]->kind == Kind::Row) return enterChild(cursor_, k, false);
        }
        ++cursor_; return true;
    }
    if (rowPath_.size() >= 2) {
        NaturalNode* box = parentBox();
        const std::size_t slot = rowPath_.back();
        if (box && (box->kind == Kind::Calculus || box->kind == Kind::Call || box->kind == Kind::Matrix) && slot + 1 < box->children.size()) {
            rowPath_.back() = slot + 1; cursor_ = 0; return true;
        }
    }
    return leaveChild(true);
}
void NaturalEditor::moveHome() { selecting_ = false; rowPath_.clear(); cursor_ = 0; autoRootParenPending_ = autoRootParenOpened_ = false; }
void NaturalEditor::moveEnd() { selecting_ = false; rowPath_.clear(); cursor_ = root_->children.size(); autoRootParenPending_ = autoRootParenOpened_ = false; }
bool NaturalEditor::moveUp() {
    selecting_ = false;
    // Innermost enclosing box that supports vertical movement wins.
    for (std::size_t depth = rowPath_.size(); depth >= 2; depth -= 2) {
        NaturalNode* parent = root_.get();
        for (std::size_t i = 0; i + 2 < depth; ++i) parent = parent->children.at(rowPath_[i]).get();
        const auto boxIndex = rowPath_[depth - 2];
        if (boxIndex >= parent->children.size()) return false;
        auto box = parent->children[boxIndex];
        const std::size_t slot = rowPath_[depth - 1];
        std::size_t target = slot;
        if (box->kind == Kind::Calculus || box->kind == Kind::Call) { if (slot > 0) target = slot - 1; }
        else if (box->kind == Kind::Matrix) { const std::size_t cols = ast::matrixColumns(*box); if (cols && slot >= cols) target = slot - cols; }
        else if ((box->kind == Kind::Fraction || box->kind == Kind::Power || box->kind == Kind::Subscript) && slot == 1) target = 0;
        if (target != slot && box->children[target]->kind == Kind::Row) {
            rowPath_.resize(depth); rowPath_[depth - 1] = target; cursor_ = box->children[target]->children.size(); return true;
        }
    }
    return false;
}
bool NaturalEditor::moveDown() {
    selecting_ = false;
    for (std::size_t depth = rowPath_.size(); depth >= 2; depth -= 2) {
        NaturalNode* parent = root_.get();
        for (std::size_t i = 0; i + 2 < depth; ++i) parent = parent->children.at(rowPath_[i]).get();
        const auto boxIndex = rowPath_[depth - 2];
        if (boxIndex >= parent->children.size()) return false;
        auto box = parent->children[boxIndex];
        const std::size_t slot = rowPath_[depth - 1];
        std::size_t target = slot;
        if (box->kind == Kind::Calculus || box->kind == Kind::Call) { if (slot + 1 < box->children.size()) target = slot + 1; }
        else if (box->kind == Kind::Matrix) { const std::size_t cols = ast::matrixColumns(*box); if (cols && slot + cols < box->children.size()) target = slot + cols; }
        else if ((box->kind == Kind::Fraction || box->kind == Kind::Power || box->kind == Kind::Subscript) && slot == 0) target = 1;
        if (target != slot && box->children[target]->kind == Kind::Row) {
            rowPath_.resize(depth); rowPath_[depth - 1] = target; cursor_ = box->children[target]->children.size(); return true;
        }
    }
    return false;
}
bool NaturalEditor::backspace() {
    autoRootParenPending_ = false;
    argumentPending_ = false;
    auto& r = activeRow();
    if (cursor_ == 0) {
        // At the start of an empty template: remove the whole template.
        NaturalNode* box = parentBox();
        if (box && rowPath_.size() >= 2) {
            NaturalNode* parent = root_.get();
            for (std::size_t i = 0; i + 2 < rowPath_.size(); ++i) parent = parent->children.at(rowPath_[i]).get();
            const auto boxIndex = rowPath_[rowPath_.size() - 2];
            auto boxPtr = parent->children[boxIndex];
            if (boxIsEmpty(*boxPtr) || (boxPtr->kind == Kind::Power && rowIsEmpty(*boxPtr->children[1]))) {
                pushUndo();
                // A power with an empty exponent collapses back to its base.
                std::vector<NodePtr> keep;
                if (boxPtr->kind == Kind::Power || boxPtr->kind == Kind::Subscript) keep = boxPtr->children[0]->children;
                rowPath_.resize(rowPath_.size() - 2);
                auto& pr = activeRow();
                pr.children.erase(pr.children.begin() + static_cast<std::ptrdiff_t>(boxIndex));
                pr.children.insert(pr.children.begin() + static_cast<std::ptrdiff_t>(boxIndex), keep.begin(), keep.end());
                cursor_ = boxIndex + keep.size();
                return true;
            }
        }
        return leaveChild(false);
    }
    const auto& prev = *r.children[cursor_ - 1];
    if (isBox(prev) && !boxIsEmpty(prev)) return moveLeft();   // enter instead of destroying content
    pushUndo();
    r.children.erase(r.children.begin() + static_cast<std::ptrdiff_t>(--cursor_));
    return true;
}
bool NaturalEditor::eraseForward() {
    auto& r = activeRow();
    if (cursor_ >= r.children.size()) return false;
    pushUndo();
    r.children.erase(r.children.begin() + static_cast<std::ptrdiff_t>(cursor_));
    return true;
}
void NaturalEditor::handle(const NaturalKeyEvent& e) {
    error_.clear();
    try {
        switch (e.key) {
        case NaturalKey::Left: moveLeft(); break;
        case NaturalKey::Right: moveRight(); break;
        case NaturalKey::Up: moveUp(); break;
        case NaturalKey::Down: moveDown(); break;
        case NaturalKey::Backspace: backspace(); break;
        case NaturalKey::Delete: eraseForward(); break;
        case NaturalKey::Enter: insertGroup(); break;
        case NaturalKey::Character: insert(e.character); break;
        }
    } catch (const std::exception& ex) { error_ = ex.what(); }
}
std::string NaturalEditor::toExpression() const { return ast::serialize(*root_); }

// ---- selezione CLIP ----------------------------------------------------------
namespace {
bool isOpening(const NaturalNode& n) { return n.kind == Kind::Token && (n.text == "(" || n.text == "[" || n.text == "{"); }
bool isClosing(const NaturalNode& n) { return n.kind == Kind::Token && (n.text == ")" || n.text == "]" || n.text == "}"); }
// Nomi di funzione a cui appartiene la parentesi che segue ("sin" "(").
bool ownsParenthesis(const NaturalNode& n) {
    if (n.kind != Kind::Token || n.text.size() < 2 || !alphaWord(n.text)) return false;
    return isReservedWord(lowerCopy(n.text));
}
} // namespace
void NaturalEditor::beginSelection() {
    autoRootParenPending_ = false;
    selecting_ = true;
    anchor_ = cursor_;
    errorMark_ = Range{};
}
bool NaturalEditor::selectLeft() {
    if (!selecting_) beginSelection();
    if (cursor_ > 0) { --cursor_; return true; }
    if (rowPath_.size() < 2) return false;
    // Oltre l'inizio di un campo di un modello: l'intero modello diventa l'unita'.
    const std::size_t box = rowPath_[rowPath_.size() - 2];
    rowPath_.resize(rowPath_.size() - 2);
    anchor_ = box + 1;
    cursor_ = box;
    return true;
}
bool NaturalEditor::selectRight() {
    if (!selecting_) beginSelection();
    const auto& r = activeRow();
    if (cursor_ < r.children.size()) { ++cursor_; return true; }
    if (rowPath_.size() < 2) return false;
    const std::size_t box = rowPath_[rowPath_.size() - 2];
    rowPath_.resize(rowPath_.size() - 2);
    anchor_ = box;
    cursor_ = box + 1;
    return true;
}
NaturalEditor::Range NaturalEditor::selection() const {
    Range out;
    if (!selecting_) return out;
    const auto& r = activeRow();
    out.row = &r;
    std::size_t b = std::min(anchor_, cursor_), e = std::max(anchor_, cursor_);
    e = std::min(e, r.children.size());
    b = std::min(b, e);
    // Bilancia le parentesi: una chiusa senza corrispondente riporta l'inizio
    // alla sua aperta (con il nome di funzione che la precede), un'aperta senza
    // corrispondente porta la fine alla sua chiusa. Si ripete finche' non cambia.
    for (bool changed = true; changed && b < e;) {
        changed = false;
        int depth = 0;
        for (std::size_t i = b; i < e; ++i) {
            if (isOpening(*r.children[i])) ++depth;
            else if (isClosing(*r.children[i])) {
                if (depth > 0) { --depth; continue; }
                // cerca l'aperta corrispondente prima di b (se manca, una chiusa isolata resta com'e')
                int d = 0; std::size_t k = b;
                bool found = false;
                while (k > 0) {
                    --k;
                    if (isClosing(*r.children[k])) ++d;
                    else if (isOpening(*r.children[k])) { if (d == 0) { found = true; break; } --d; }
                }
                if (found) { b = k; changed = true; }
                break;
            }
        }
        if (changed) continue;
        depth = 0;
        for (std::size_t i = e; i-- > b;) {
            if (isClosing(*r.children[i])) ++depth;
            else if (isOpening(*r.children[i])) {
                if (depth > 0) { --depth; continue; }
                int d = 0; std::size_t k = e;
                for (; k < r.children.size(); ++k) {
                    if (isOpening(*r.children[k])) ++d;
                    else if (isClosing(*r.children[k])) { if (d == 0) break; --d; }
                }
                if (k < r.children.size()) { e = k + 1; changed = true; }
                else { e = r.children.size(); }     // parentesi di chiusura omessa alla fine dell'input
                break;
            }
        }
    }
    // Una selezione che inizia con "(" comprende il nome di funzione a cui appartiene.
    while (b > 0 && b < e && isOpening(*r.children[b]) && ownsParenthesis(*r.children[b - 1])) --b;
    out.begin = b; out.end = e;
    return out;
}
ast::NodePtr NaturalEditor::copySelection() const {
    const Range s = selection();
    if (s.empty()) return nullptr;
    auto out = row();
    for (std::size_t i = s.begin; i < s.end; ++i) out->children.push_back(cloneTree(*s.row->children[i]));
    return out;
}
bool NaturalEditor::deleteSelection() {
    const Range s = selection();
    selecting_ = false;
    if (s.empty()) return false;
    pushUndo();
    auto& r = activeRow();
    r.children.erase(r.children.begin() + static_cast<std::ptrdiff_t>(s.begin), r.children.begin() + static_cast<std::ptrdiff_t>(s.end));
    cursor_ = s.begin;
    return true;
}
ast::NodePtr NaturalEditor::cutSelection() {
    auto copy = copySelection();
    (void)deleteSelection();
    return copy;
}

// ---- causa di un errore ------------------------------------------------------
bool NaturalEditor::markError(std::size_t position, std::size_t length) {
    selecting_ = false;
    errorMark_ = Range{};
    ast::SerializeMap map;
    std::string text;
    try { text = ast::serialize(*root_, &map); } catch (...) { return false; }
    if (position > text.size()) return false;
    const std::size_t end = std::min(text.size(), position + length);
    // Riga piu' interna il cui testo contiene tutto l'intervallo (la radice contiene tutto).
    const ast::SerializeMap::RowSpan* best = nullptr;
    for (const auto& span : map.rows) {
        if (span.begin <= position && end <= span.end && (!best || span.path.size() > best->path.size())) best = &span;
    }
    if (!best) return false;
    const std::size_t n = best->childStart.size();
    // Primo figlio toccato dall'intervallo e primo figlio dopo di esso.
    std::size_t b = 0;
    while (b + 1 < n && best->childStart[b + 1] <= position) ++b;
    if (n == 0 || position >= best->end) b = n;
    std::size_t e = b;
    while (e < n && best->childStart[e] < end) ++e;
    if (length == 0) e = b;
    rowPath_ = best->path;
    cursor_ = e;
    autoRootParenPending_ = autoRootParenOpened_ = false;
    if (e > b) errorMark_ = Range{best->row, b, e};
    return true;
}
std::vector<NaturalDrawOp> NaturalEditor::layout(int x, int baseline, int maxWidth) const {
    math::CursorTarget target{activeRowPtr(), cursor_};
    target.colorParens = true;
    target.cursorStyle = overwrite() ? 1 : argumentPending_ ? 2 : 0;
    const Range mark = selecting_ ? selection() : errorMark_;
    if (!mark.empty()) { target.markRow = mark.row; target.markBegin = mark.begin; target.markEnd = mark.end; target.markIsError = !selecting_; }
    auto l = math::layout(*root_, x, baseline, maxWidth, 220, math::Align::Left, &target);
    return std::move(l.ops);
}
} // namespace fx
