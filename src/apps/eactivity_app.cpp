// eActivity (v1.16.0, manuale fx-CG50 capitolo 10): controller sottile sopra il
// nucleo puro eactivity.hpp.
//  * elenco dei documenti di PICOCALC/EACT (cartelle fino a tre livelli, OPEN,
//    NEW, DELETE, SEARCH, MEMO con JUMP/EDIT/DETAIL/DELETE/DEL-ALL);
//  * workspace: righe di testo (255 byte, a capo sulle parole, espressioni Math
//    incorporate scritte con l'editor naturale, colori e marcatori, memo),
//    righe di calcolo valutate dal nucleo di Run-Matrix (evaluateEntry, forme
//    esatte, S<->D) con il ricalcolo a passi delle righe seguenti fino a una
//    riga di stop (StepTask, AC = Break), righe di stop, immagini (.pcpict di
//    PICT e CAPT) e strisce che aprono le altre modalita' con i propri dati
//    (sessione da striscia del Context, parti del workspace);
//  * memorie Mat/Vct/List separate per documento (parti Matrices e Lists).
// I documenti sono file tipizzati .pceact (typed_files.hpp, FILE_FORMATS.md);
// il documento aperto ha il limite limits::kEActivityDocumentBytes per scheda:
// oltre, "Memory ERROR" e il documento resta com'era.
#include "apps.hpp"
#include "eactivity.hpp"
#include "font_renderer.hpp"
#include "limits.hpp"
#include "math_renderer.hpp"
#include "mode_shell.hpp"
#include "pict.hpp"
#include "run_matrix.hpp"
#include "step_task.hpp"
#include "text_editor.hpp"
#include "typed_files.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace fx::app {
using namespace ui::theme;
using eact::Line;
using eact::LineKind;
using eact::StripKind;

// Le schermate d'ingresso delle strisce (Context::strip().entry) sono i tipi di striscia.
static_assert(stripentry::Run == static_cast<std::uint8_t>(StripKind::Run) && stripentry::Graph == static_cast<std::uint8_t>(StripKind::Graph) &&
              stripentry::GraphEditor == static_cast<std::uint8_t>(StripKind::GraphEditor) &&
              stripentry::TableEditor == static_cast<std::uint8_t>(StripKind::TableEditor) &&
              stripentry::StatGraph == static_cast<std::uint8_t>(StripKind::StatGraph) &&
              stripentry::ListEditor == static_cast<std::uint8_t>(StripKind::ListEditor) &&
              stripentry::Solver == static_cast<std::uint8_t>(StripKind::Solver) &&
              stripentry::MatrixEditor == static_cast<std::uint8_t>(StripKind::MatrixEditor) &&
              stripentry::VectorEditor == static_cast<std::uint8_t>(StripKind::VectorEditor) &&
              stripentry::SimulEquation == static_cast<std::uint8_t>(StripKind::SimulEquation) &&
              stripentry::PolyEquation == static_cast<std::uint8_t>(StripKind::PolyEquation) &&
              stripentry::Graph3D == static_cast<std::uint8_t>(StripKind::Graph3D) &&
              stripentry::Graph3DEditor == static_cast<std::uint8_t>(StripKind::Graph3DEditor),
              "stripentry e eact::StripKind devono coincidere");
static_assert(stripentry::MemoryLists >= eact::StripKindCount, "ingressi LIST e MAT/VCT fuori dai tipi di striscia");

namespace {
constexpr files::FileType kType = files::FileType::EActivity;
constexpr const char* kExtension = ".pceact";
constexpr int kLeft = 4;
constexpr int kWidth = ui::ScreenW - 14;                 // a destra la barra di scorrimento
constexpr int kTop = ui::ContentTop + 2, kBottom = ui::ContentBottom - 2;
constexpr int kFileRowH = 22;
constexpr std::size_t kMaxFolderDepth = 3;               // cartelle annidate (cap. 11, Appendice α)
constexpr int kMaxPicture = 320;                         // lato massimo di un'immagine inseribile

// Azioni delle barre F1-F6 (tabelle del guscio comune).
enum Act : int {
    AOpen = 0, ANew, ADelete, ASearch, AMemoFiles,
    AJump, AEditMemo, ADetail, AMemoDelete, ADelAll,
    ASave, ASaveAs, AOpt, ACapacity, ASize,
    AStrip, AToCalc, AToText, AChar, ACase,
    ATop, ABottom, APageUp, APageDown,
    ADelLine, AInsText, AInsCalc, AInsStop, AInsPicture,
    AMath, AMarker, ACharColor, AMatVct, AList,
    AMemoInsert, AMemoRemove, ACatalog, AView, ASet, ANotesInsert
};
// Modali: conferme e menu.
enum Tag : int {
    TagNone = 0, TagDeleteFile, TagDeleteLine, TagDeleteMemo, TagDeleteAllMemos, TagSave,
    TagStrip, TagSwitch, TagMarker, TagCharColor, TagMemoColor, TagFormat, TagLineStyle, TagLineColor, TagCalcView
};
enum Dlg : int { DlgNone = 0, DlgNew, DlgSaveAs, DlgSearch, DlgPicture };

const FKeyPage kFilePage{{{"SAVE", ASave}, {"SAVE\xC2\xB7" "AS", ASaveAs}, {"OPT", AOpt}, {"CAPACITY", ACapacity}, {}, {}}};
const FKeyMenu kFileMenu{&kFilePage, 1, nullptr};
const FKeyPage kStripFilePage{{{"SAVE", ASave}, {"SAVE\xC2\xB7" "AS", ASaveAs}, {"OPT", AOpt}, {"CAPACITY", ACapacity}, {"SIZE", ASize}, {}}};
const FKeyMenu kStripFileMenu{&kStripFilePage, 1, nullptr};
const FKeyPage kJumpPage{{{"TOP", ATop}, {"BOTTOM", ABottom}, {"PageUp", APageUp}, {"PageDown", APageDown}, {}, {}}};
const FKeyMenu kJumpMenu{&kJumpPage, 1, nullptr};
const FKeyPage kInsertPage{{{"TEXT", AInsText}, {"CALC", AInsCalc}, {"STOP", AInsStop}, {"PICTURE", AInsPicture}, {}, {}}};
const FKeyMenu kInsertMenu{&kInsertPage, 1, nullptr};
const FKeyPage kColorPage{{{"MARKER", AMarker}, {"CHAR", ACharColor}, {}, {}, {}, {}}};
const FKeyMenu kColorMenu{&kColorPage, 1, nullptr};
const FKeyPage kMemoPage{{{"INSERT", AMemoInsert}, {"DELETE", AMemoRemove}, {"Catalog", ACatalog}, {"VIEW", AView}, {}, {}}};
const FKeyMenu kMemoMenu{&kMemoPage, 1, nullptr};
#define FX_CASE "A\xE2\x87\x94" "a"
// Manuale 10-3: riga di testo (TEXT), di calcolo (CALC), di stop, immagine, striscia.
const FKeyPage kTextPages[] = {
    {{{"FILE", -1, &kFileMenu}, {"STRIP", AStrip}, {"TEXT", AToCalc}, {"CHAR", AChar}, {FX_CASE, ACase}, {}}},
    {{{"JUMP", -1, &kJumpMenu}, {"DEL-LINE", ADelLine}, {"INSERT", -1, &kInsertMenu}, {"MATH", AMath}, {"COLOR", -1, &kColorMenu}, {}}},
    {{{"'MAT/VCT", AMatVct}, {"'LIST", AList}, {"MEMO", -1, &kMemoMenu}, {}, {}, {}}}};
const FKeyMenu kTextMenu{kTextPages, 3, nullptr};
const FKeyPage kCalcPages[] = {
    {{{"FILE", -1, &kFileMenu}, {"STRIP", AStrip}, {"CALC", AToText}, {"MATH", AMath}, {"INSERT", -1, &kInsertMenu}, {}}},
    {{{"JUMP", -1, &kJumpMenu}, {"DEL-LINE", ADelLine}, {"'MAT/VCT", AMatVct}, {"'LIST", AList}, {"COLOR", -1, &kColorMenu}, {}}},
    {{{"MEMO", -1, &kMemoMenu}, {}, {}, {}, {}, {}}}};
const FKeyMenu kCalcMenu{kCalcPages, 3, nullptr};
const FKeyPage kStopPages[] = {
    {{{"FILE", -1, &kFileMenu}, {"STRIP", AStrip}, {"CALC", AToText}, {"MATH", AMath}, {"INSERT", -1, &kInsertMenu}, {}}},
    {{{"JUMP", -1, &kJumpMenu}, {"DEL-LINE", ADelLine}, {}, {}, {}, {}}}};
const FKeyMenu kStopMenu{kStopPages, 2, nullptr};
const FKeyPage kPicturePages[] = {
    {{{"FILE", -1, &kFileMenu}, {"STRIP", AStrip}, {}, {}, {}, {}}},
    {{{"JUMP", -1, &kJumpMenu}, {"DELETE", ADelLine}, {"INSERT", -1, &kInsertMenu}, {}, {}, {}}}};
const FKeyMenu kPictureMenu{kPicturePages, 2, nullptr};
const FKeyPage kStripPages[] = {
    {{{"FILE", -1, &kStripFileMenu}, {"STRIP", AStrip}, {}, {"CHAR", AChar}, {FX_CASE, ACase}, {}}},
    {{{"JUMP", -1, &kJumpMenu}, {"DEL-LINE", ADelLine}, {"INSERT", -1, &kInsertMenu}, {}, {}, {}}}};
const FKeyMenu kStripMenu{kStripPages, 2, nullptr};
// Notes (10-20): JUMP, DEL-LINE, INSERT, MATH, CHAR, A<->a.
const FKeyPage kNotesPage{{{"JUMP", -1, &kJumpMenu}, {"DEL-LINE", ADelLine}, {"INSERT", ANotesInsert}, {"MATH", AMath}, {"CHAR", AChar}, {FX_CASE, ACase}}};
const FKeyMenu kNotesMenu{&kNotesPage, 1, nullptr};
const FKeyPage kMarkPage{{{"SET", ASet}, {}, {}, {}, {}, {}}};
const FKeyMenu kMarkMenu{&kMarkPage, 1, nullptr};
// Elenco dei file (10-2) e dei memo.
const FKeyPage kFilesPage{{{"OPEN", AOpen}, {"NEW", ANew}, {"DELETE", ADelete}, {"SEARCH", ASearch}, {"MEMO", AMemoFiles}, {}}};
const FKeyMenu kFilesMenu{&kFilesPage, 1, nullptr};
const FKeyPage kMemoListPage{{{"JUMP", AJump}, {"EDIT", AEditMemo}, {"DETAIL", ADetail}, {"DELETE", AMemoDelete}, {"DEL-ALL", ADelAll}, {}}};
const FKeyMenu kMemoListMenu{&kMemoListPage, 1, nullptr};
const FKeyPage kCatalogPage{{{"JUMP", AJump}, {}, {}, {}, {}, {}}};
const FKeyMenu kCatalogMenu{&kCatalogPage, 1, nullptr};
#undef FX_CASE

std::uint16_t textColor(std::uint8_t c) {
    static const std::uint16_t k[eact::TextColorCount] = {Ink, gfx::rgb888To565(0, 40, 220), gfx::rgb888To565(215, 0, 20), gfx::rgb888To565(200, 0, 190),
                                                          gfx::rgb888To565(0, 145, 40), gfx::rgb888To565(0, 150, 180), gfx::rgb888To565(200, 155, 0)};
    return k[c < eact::TextColorCount ? c : 0];
}
std::uint16_t markerColor(std::uint8_t m) {
    static const std::uint16_t k[eact::MarkerCount] = {Bg, gfx::rgb888To565(255, 175, 240), gfx::rgb888To565(170, 240, 170),
                                                       gfx::rgb888To565(165, 235, 252), gfx::rgb888To565(255, 244, 130)};
    return k[m < eact::MarkerCount ? m : 0];
}
std::uint16_t borderInk(std::uint8_t c) {   // Line Color 1..8 (10-16)
    static const std::uint16_t k[8] = {Ink, gfx::rgb888To565(0, 40, 220), gfx::rgb888To565(215, 0, 20), gfx::rgb888To565(200, 0, 190),
                                       gfx::rgb888To565(0, 145, 40), gfx::rgb888To565(0, 150, 180), gfx::rgb888To565(230, 200, 0), Bg};
    return c >= 1 && c <= 8 ? k[c - 1] : Ink;
}
const char* const kColorNames[eact::TextColorCount] = {"Black", "Blue", "Red", "Magenta", "Green", "Cyan", "Yellow"};
const char* const kBorderNames[8] = {"Black", "Blue", "Red", "Magenta", "Green", "Cyan", "Yellow", "White"};
const char* const kStyleNames[3] = {"Normal", "Thick", "Thin"};

// CHAR (10-3): simboli matematici, speciali e lettere di altre lingue.
struct CharPage { const char* name; const char* chars; };
const CharPage kCharPages[] = {
    {"MATH", "≤≥≠±×÷√∛∞°∠πθ∑∫∂≈≡∈⊂∪∩→⇒⇔∀∃∆∇′″·¹²³⁻ⁿ"},
    {"SYMBOL", "'{}~\"#$%&@[]\\^_|<>!?;:`*+-=/()"},
    {"GREEK", "αβγδεζηθικλμνξοπρστυφχψωΓΔΘΛΞΠΣΦΨΩ"},
    {"LANG", "àáâäãåæçèéêëìíîïñòóôöõøùúûüýÿßÀÁÂÄÅÆÇÈÉÊËÌÍÎÏÑÒÓÔÖØÙÚÛÜ¿¡€£¥"},
};
constexpr int kCharPageCount = 4, kCharCols = 10;
std::uint32_t codepoint(const std::string& s) {
    const auto* b = reinterpret_cast<const unsigned char*>(s.data());
    if (s.empty()) return 0;
    if (b[0] < 0x80) return b[0];
    if ((b[0] & 0xE0) == 0xC0 && s.size() >= 2) return ((b[0] & 0x1Fu) << 6) | (b[1] & 0x3Fu);
    if ((b[0] & 0xF0) == 0xE0 && s.size() >= 3) return ((b[0] & 0x0Fu) << 12) | ((b[1] & 0x3Fu) << 6) | (b[2] & 0x3Fu);
    if (s.size() >= 4) return ((b[0] & 0x07u) << 18) | ((b[1] & 0x3Fu) << 12) | ((b[2] & 0x3Fu) << 6) | (b[3] & 0x3Fu);
    return 0;
}
std::vector<std::string> charList(int page) {
    std::vector<std::string> out;
    const std::string all = kCharPages[page % kCharPageCount].chars;
    for (std::size_t i = 0; i < all.size();) {
        const std::size_t j = eact::nextUnit(all, i);
        std::string ch = all.substr(i, j - i);
        if (font::hasGlyph(font::Face::Medium, codepoint(ch))) out.push_back(std::move(ch));   // nessun carattere senza glifo
        i = j;
    }
    return out;
}

int textWidth(const std::string& t, std::size_t a, std::size_t b) {
    if (b <= a) return 0;
    const std::string s = t.substr(a, b - a);
    return font::measure(font::Face::Medium, s.c_str()).width;
}
char swapCase(char c) {
    const auto u = static_cast<unsigned char>(c);
    if (std::isupper(u)) return static_cast<char>(std::tolower(u));
    if (std::islower(u)) return static_cast<char>(std::toupper(u));
    return c;
}
bool isIsland(const std::string& t, std::size_t pos) { return t.compare(pos, 3, eact::IslandStart) == 0; }
// Testo lineare di una riga di testo senza le isole (TEXT -> CALC).
std::string plainExpression(const std::string& t) {
    std::string out;
    for (std::size_t i = 0; i < t.size();) {
        std::string expr; std::size_t end;
        if (eact::islandAt(t, i, expr, end)) { out += expr; i = end; continue; }
        out += t[i++];
    }
    return out;
}
std::size_t depth(const std::string& folder) {
    if (folder.empty()) return 0;
    return static_cast<std::size_t>(std::count(folder.begin(), folder.end(), '/')) + 1;
}
std::string parentFolder(const std::string& folder) {
    const std::size_t s = folder.rfind('/');
    return s == std::string::npos ? std::string() : folder.substr(0, s);
}
bool validFolder(const std::string& folder) {
    if (folder.empty()) return true;
    if (depth(folder) > kMaxFolderDepth) return false;
    std::size_t a = 0;
    while (a <= folder.size()) {
        const std::size_t b = std::min(folder.find('/', a), folder.size());
        if (b == a || folder[a] == '.') return false;
        for (std::size_t i = a; i < b; ++i) if (!(std::isalnum(static_cast<unsigned char>(folder[i])) || folder[i] == '_' || folder[i] == '-')) return false;
        a = b + 1;
    }
    return true;
}
void check(files::Status st) {
    if (st != files::Status::Ok) throw MathError(std::string("Storage ERROR: ") + files::statusMessage(st));
}
const std::vector<std::uint8_t>& memoryParts() {
    static const std::vector<std::uint8_t> p = {part::Matrices, part::Lists};
    return p;
}
// Disegno di un'immagine ridotta a w x h (campionamento dei tratti RLE).
void drawPicture(gfx::Canvas& c, const pict::Image& img, int x, int y, int w, int h) {
    if (img.empty() || w <= 0 || h <= 0) return;
    (void)pict::forEachSpan(img, [&](int sx, int sy, int n, std::uint16_t col) {
        const int y0 = y + sy * h / img.height, y1 = y + (sy + 1) * h / img.height;
        const int x0 = x + sx * w / img.width, x1 = x + (sx + n) * w / img.width;
        if (y1 > y0 && x1 > x0) c.rect(x0, y0, x1 - x0, y1 - y0, col, true);
    });
}
} // namespace

// Una riga visiva di una riga di testo: pezzi di eact::wrap con l'altezza.
struct EActivityApp::Row {
    std::vector<eact::Piece> pieces;
    int ascent{13}, descent{3};
    int height() const noexcept { return ascent + descent + 2; }
    std::size_t from() const noexcept { return pieces.empty() ? 0 : pieces.front().from; }
    std::size_t to() const noexcept { return pieces.empty() ? 0 : pieces.back().to; }
};

// ================================================================ ciclo di vita
void EActivityApp::enter(Context& ctx) {
    picts_.clear(); pictBytes_ = 0;
    trees_.clear();
    shell_.closeModals();
    dialog_.close();
    memoWindow_ = false; charOpen_ = false;
    bar_ = nullptr;
    const Context::StripSession s = ctx.strip();
    if (s.active && s.owner == AppId::EActivity) {      // ritorno da una striscia (SHIFT ', MENU, EXIT)
        std::string error;
        try { returnFromStrip(ctx); }
        catch (const std::bad_alloc&) { ctx.endStrip(); throw; }
        catch (const std::exception& e) { error = e.what(); }
        ctx.endStrip();
        screen_ = open_ ? Screen::Doc : Screen::Files;
        if (open_) { enterLine(ctx); ensureVisible(ctx); }
        if (!error.empty()) throw MathError(error);
        if (s.switchRequested && open_ && current().kind == LineKind::Strip) showStripMenu(TagSwitch);   // SHIFT , (10-21)
        return;
    }
    if (open_) {                                         // dal menu principale o dal SET UP con il documento aperto
        resume(ctx);
        screen_ = Screen::Doc;
        enterLine(ctx);
        ensureVisible(ctx);
        return;
    }
    screen_ = Screen::Files;
    folder_ = validFolder(ctx.ws.eactivity.folder) ? ctx.ws.eactivity.folder : std::string();
    listFiles(ctx);
    for (std::size_t i = 0; i < items_.size(); ++i)
        if (!items_[i].folder && items_[i].file == ctx.ws.eactivity.lastFile) sel_ = static_cast<int>(i);
}

void EActivityApp::leave(Context& ctx) {
    runner_.reset();
    if (open_ && !frames_.empty()) leaveLine(ctx);
    markMode_ = 0;
    titleEdit_ = false;
    if (!open_) return;
    const Context::StripSession& s = ctx.strip();
    if (s.active && s.owner == AppId::EActivity) return;   // verso una striscia o LIST/MAT: restano le memorie del file
    suspend(ctx);
}

// Le memorie Mat/Vct/List del documento si scambiano con quelle del menu
// principale all'apertura e alla chiusura (10-9), e lasciando eActivity con il
// documento aperto (le altre modalita' vedono le memorie del menu principale).
void EActivityApp::suspend(Context& ctx) {
    if (suspended_) return;
    docMemories_ = encodeParts(ctx.ws, memoryParts());
    auto& saved = ctx.ws.eactivity.savedMemories;
    if (!saved.empty() && !decodeParts(ctx.ws, saved.data(), saved.size())) for (auto p : memoryParts()) resetPart(ctx.ws, p);
    saved.clear();
    suspended_ = true;
    ctx.markDirty();
}
void EActivityApp::resume(Context& ctx) {
    if (!suspended_) return;
    ctx.ws.eactivity.savedMemories = encodeParts(ctx.ws, memoryParts());
    if (!decodeParts(ctx.ws, docMemories_.data(), docMemories_.size())) for (auto p : memoryParts()) resetPart(ctx.ws, p);
    docMemories_.clear();
    docMemories_.shrink_to_fit();
    suspended_ = false;
    ctx.markDirty();
}

bool EActivityApp::tick(Context& ctx) {
    if (!runner_.running()) return false;
    const step::Clock clock = [&ctx] { return ctx.now(); };
    try {
        (void)runner_.tick(clock);
    } catch (...) {
        runner_.reset();
        throw;
    }
    if (!runner_.running()) runner_.reset();
    return true;
}

std::size_t EActivityApp::documentBytes() const {
    if (frames_.empty()) return 0;
    std::size_t n = eact::encodedSize(frames_.front().doc);
    if (frames_.size() > 1) {   // Notes aperta: il testo conta al posto dei dati della striscia
        const Frame& f = frames_.back();
        const int at = f.stripLine;
        const auto& lines = frames_.front().doc.lines;
        if (at >= 0 && at < static_cast<int>(lines.size())) n -= std::min(n, lines[static_cast<std::size_t>(at)].data.size());
        for (const Line& l : f.doc.lines) n += l.text.size() + 1;
    }
    return n;
}
bool EActivityApp::fits() const { return documentBytes() <= limits::kEActivityDocumentBytes; }
[[noreturn]] void EActivityApp::memoryError() {
    throw MathError("Memory ERROR: documento oltre " + std::to_string(limits::kEActivityDocumentBytes / 1024) + " KB");
}

// ================================================================ file
std::string EActivityApp::directory() const {
    const std::string base = files::fileType(kType).folder;
    return folder_.empty() ? base : files::join(base, folder_);
}
files::FileSystem& EActivityApp::store(Context& ctx) {
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) throw MathError("Storage ERROR: archivio non disponibile");
    return *fs;
}

void EActivityApp::listFiles(Context& ctx) {
    items_.clear();
    message_.clear();
    files::FileSystem* fs = userFiles(ctx);
    if (!fs) { message_ = "Archivio non disponibile"; return; }
    const files::Status st = fs->status();
    if (st != files::Status::Ok) { message_ = files::statusMessage(st); return; }
    const std::string dir = directory();
    (void)files::recover(*fs, dir);                       // salvataggi interrotti
    std::vector<files::Entry> entries;
    const files::Status ls = fs->list(dir, entries);
    if (ls != files::Status::Ok && ls != files::Status::NotFound) { message_ = files::statusMessage(ls); return; }
    for (const files::Entry& e : entries) {
        if (e.name.empty() || e.name[0] == '.') continue;
        if (e.directory) { items_.push_back({e.name, e.name, true, 0}); continue; }
        if (!files::hasExtension(e.name, kExtension)) continue;
        items_.push_back({files::stem(e.name), e.name, false, e.size});
    }
    std::sort(items_.begin(), items_.end(), [](const Item& a, const Item& b) {
        if (a.folder != b.folder) return a.folder;
        return a.name < b.name;
    });
    sel_ = std::clamp(sel_, 0, std::max(0, static_cast<int>(items_.size()) - 1));
}

eact::Document EActivityApp::readDocument(Context& ctx, const std::string& file) {
    files::FileSystem& fs = store(ctx);
    std::vector<std::uint8_t> raw, payload;
    const files::Status st = files::readAll(fs, files::join(directory(), file), raw, files::HeaderBytes + limits::kEActivityDocumentBytes);
    if (st == files::Status::TooLarge) memoryError();
    check(st);
    const files::Status d = files::decodeTypedFile(kType, raw, payload);
    if (d == files::Status::TooLarge) memoryError();
    if (d == files::Status::NotSupported) throw MathError("Data ERROR: formato di eActivity piu' recente");
    if (d != files::Status::Ok) throw MathError("Data ERROR: file danneggiato");
    if (payload.size() > limits::kEActivityDocumentBytes) memoryError();
    eact::Document doc;
    if (!eact::decode(payload.data(), payload.size(), doc)) throw MathError("Data ERROR: file danneggiato");
    return doc;
}
void EActivityApp::writeDocument(Context& ctx, const std::string& file, const eact::Document& doc) {
    const auto payload = eact::encode(doc);
    if (payload.size() > limits::kEActivityDocumentBytes) memoryError();
    files::FileSystem& fs = store(ctx);
    check(fs.status());
    check(fs.makeDirs(directory()));
    const auto raw = files::encodeTypedFile(kType, payload);
    check(files::writeAtomic(fs, files::join(directory(), file), raw.data(), raw.size()));
}

void EActivityApp::loadDocument(Context& ctx, eact::Document doc, const std::string& file) {
    if (doc.lines.empty()) doc.lines.push_back(Line::textLine());
    // Memorie del file (10-9): quelle del menu principale vanno nella sezione 0x0102.
    std::vector<std::uint8_t> main = encodeParts(ctx.ws, memoryParts());
    if (doc.memories.empty()) { for (auto p : memoryParts()) resetPart(ctx.ws, p); }
    else if (!decodeParts(ctx.ws, doc.memories.data(), doc.memories.size())) throw MathError("Data ERROR: memorie del file danneggiate");
    ctx.ws.eactivity.savedMemories = std::move(main);
    ctx.ws.eactivity.lastFile = file;
    ctx.ws.eactivity.folder = folder_;
    ctx.markDirty();
    frames_.clear();
    frames_.push_back(Frame{std::move(doc), 0, 0, -1});
    fileName_ = file;
    open_ = true; suspended_ = false; modified_ = false;
    lower_ = false; markMode_ = 0; island_ = false; titleEdit_ = false; fieldLine_ = false;
    stripLine_ = -1;
    screen_ = Screen::Doc;
    bar_ = nullptr;
    picts_.clear(); pictBytes_ = 0;
    enterLine(ctx);
}

void EActivityApp::openDocument(Context& ctx, const std::string& file) {
    eact::Document doc = readDocument(ctx, file);
    loadDocument(ctx, std::move(doc), file);
}

void EActivityApp::newDocument(Context& ctx, const std::string& stem) {
    if (!eact::validName(stem)) throw MathError("Invalid Name: nome di 1-8 caratteri A-Z 0-9 { } ' ~");
    eact::Document doc;
    doc.lines.push_back(Line::textLine());
    const std::string file = stem + kExtension;
    writeDocument(ctx, file, doc);                        // il file esiste subito nell'elenco
    loadDocument(ctx, std::move(doc), file);
}

void EActivityApp::saveDocument(Context& ctx, const std::string& file) {
    if (!open_ || frames_.empty()) return;
    while (frames_.size() > 1) closeNotes(ctx);
    leaveLine(ctx);
    eact::Document& d = frames_.front().doc;
    d.memories = encodeParts(ctx.ws, memoryParts());
    writeDocument(ctx, file, d);
    fileName_ = file;
    ctx.ws.eactivity.lastFile = file;
    ctx.markDirty();
    modified_ = false;
    ctx.toast("Salvato " + files::stem(file));
    enterLine(ctx);
}

void EActivityApp::closeDocument(Context& ctx) {
    runner_.reset();
    if (open_) {
        auto& saved = ctx.ws.eactivity.savedMemories;
        if (!suspended_ && !saved.empty() && !decodeParts(ctx.ws, saved.data(), saved.size()))
            for (auto p : memoryParts()) resetPart(ctx.ws, p);
        saved.clear();
        ctx.markDirty();
    }
    frames_.clear();
    docMemories_.clear();
    open_ = suspended_ = modified_ = false;
    island_ = titleEdit_ = fieldLine_ = false;
    markMode_ = 0;
    picts_.clear(); pictBytes_ = 0;
    screen_ = Screen::Files;
    bar_ = nullptr;
    listFiles(ctx);
    for (std::size_t i = 0; i < items_.size(); ++i) if (!items_[i].folder && items_[i].file == fileName_) sel_ = static_cast<int>(i);
}

void EActivityApp::exitDocument(Context& ctx) {
    leaveLine(ctx);
    if (modified_) { shell_.confirm.open("Salvare il file?  AC: torna", TagSave); return; }   // 10-23
    closeDocument(ctx);
}

std::vector<std::string> EActivityApp::fileStems() const {
    std::vector<std::string> out;
    for (const Item& it : items_) if (!it.folder) out.push_back(it.name);
    return out;
}
void EActivityApp::nameDialog(const char* title, const std::string& initial, int purpose) {
    dialog_.setNameRule([](char c) { return files::typedNameChar(kType, c); }, [](char c) { return files::typedNameCase(kType, c); }, 8);
    dialog_.setExtension(kExtension);
    if (purpose == DlgSearch) dialog_.openSearch(title);
    else {
        dialog_.setExisting(fileStems());
        dialog_.openName(title, initial);
    }
    dialogFor_ = purpose;
}

void EActivityApp::filesKey(Context& ctx, const KeyEvent& ev) {
    updateBar();
    const int n = static_cast<int>(items_.size());
    int a = shell_.fkeys.key(ev, false);
    if (a == FKeyBar::None && ev.is(Key::Exe)) a = AOpen;
    const Item* it = n > 0 && sel_ >= 0 && sel_ < n ? &items_[static_cast<std::size_t>(sel_)] : nullptr;
    switch (a) {
    case AOpen:
        if (!it) return;
        if (it->folder) {
            if (depth(folder_) >= kMaxFolderDepth) throw MathError("ERROR: Sub-folders cannot be displayed");
            folder_ = folder_.empty() ? it->file : folder_ + "/" + it->file;
            ctx.ws.eactivity.folder = folder_; ctx.markDirty();
            sel_ = 0; first_ = 0;
            listFiles(ctx);
            return;
        }
        openDocument(ctx, it->file);
        return;
    case ANew:
        listFiles(ctx);
        nameDialog("NEW", "", DlgNew);
        return;
    case ADelete:
        if (!it || it->folder) return;
        shell_.confirm.open("Delete eActivity?", TagDeleteFile);    // 10-5
        return;
    case ASearch: nameDialog("SEARCH", "", DlgSearch); return;
    case AMemoFiles: {
        if (!it || it->folder) throw MathError("No MEMO: scegliere un file");
        memoDoc_ = readDocument(ctx, it->file);
        memoFile_ = it->file;
        if (memoDoc_.memoCount() == 0) throw MathError("No MEMO: il file non ha memo");   // Appendice α
        memoReturn_ = Screen::Files;
        buildMemoList(memoDoc_);
        memoSel_ = 0;
        screen_ = Screen::MemoList;
        return;
    }
    default: break;
    }
    if (a != FKeyBar::None) return;
    switch (ev.key) {
    case Key::Up: if (n) sel_ = (sel_ + n - 1) % n; return;
    case Key::Down: if (n) sel_ = (sel_ + 1) % n; return;
    case Key::PageUp: sel_ = std::max(0, sel_ - 8); return;
    case Key::PageDown: if (n) sel_ = std::min(n - 1, sel_ + 8); return;
    case Key::Home: sel_ = 0; return;
    case Key::End: sel_ = std::max(0, n - 1); return;
    case Key::Exit:
        if (!folder_.empty()) {
            const std::string from = folder_.substr(folder_.rfind('/') == std::string::npos ? 0 : folder_.rfind('/') + 1);
            folder_ = parentFolder(folder_);
            ctx.ws.eactivity.folder = folder_; ctx.markDirty();
            listFiles(ctx);
            for (std::size_t i = 0; i < items_.size(); ++i) if (items_[i].folder && items_[i].file == from) sel_ = static_cast<int>(i);
            return;
        }
        ctx.home();
        return;
    default: return;
    }
}

void EActivityApp::dialogKey(Context& ctx, const KeyEvent& ev) {
    const int r = dialog_.key(ev);
    if (r == 0) return;
    const int purpose = dialogFor_;
    dialogFor_ = DlgNone;
    dialog_.close();
    if (r < 0) return;
    const std::string name = dialog_.name();
    switch (purpose) {
    case DlgNew: newDocument(ctx, name); return;
    case DlgSaveAs:
        if (!eact::validName(name)) throw MathError("Invalid Name: nome di 1-8 caratteri A-Z 0-9 { } ' ~");
        saveDocument(ctx, name + kExtension);
        return;
    case DlgSearch: {                                     // 10-5: dall'inizio del nome
        for (std::size_t i = 0; i < items_.size(); ++i)
            if (!items_[i].folder && items_[i].name.compare(0, name.size(), name) == 0) { sel_ = static_cast<int>(i); return; }
        throw MathError("Not Found: nessun file che inizia con " + name);
    }
    case DlgPicture: insertPicture(ctx, name); return;
    default: return;
    }
}

// ================================================================ memo
void EActivityApp::buildMemoList(const eact::Document& doc) {
    memos_.clear();
    for (std::size_t i = 0; i < doc.lines.size(); ++i)
        if (doc.lines[i].memo.present) memos_.push_back({static_cast<int>(i), doc.lines[i].memo.color, doc.lines[i].memo.text});
}
eact::Document& EActivityApp::memoDocument() { return memoReturn_ == Screen::Files ? memoDoc_ : frames_.front().doc; }

void EActivityApp::memoListKey(Context& ctx, const KeyEvent& ev) {
    updateBar();
    const int n = static_cast<int>(memos_.size());
    int a = shell_.fkeys.key(ev, false);
    if (a == FKeyBar::None && ev.is(Key::Exe)) a = AJump;
    if (n == 0) { screen_ = memoReturn_; return; }
    memoSel_ = std::clamp(memoSel_, 0, n - 1);
    const MemoRef& m = memos_[static_cast<std::size_t>(memoSel_)];
    switch (a) {
    case AJump:                                           // alla riga del memo, cursore al primo carattere
        if (memoReturn_ == Screen::Files) openDocument(ctx, memoFile_);
        screen_ = Screen::Doc;
        moveTo(ctx, m.line, false);
        return;
    case AEditMemo: case ADetail:
        memoLine_ = m.line;
        memoColor_ = m.color;
        (void)memoText_.setText(m.text);
        memoText_.bottom();
        if (a == AEditMemo) memoWindow_ = true;
        else { detailReturn_ = Screen::MemoList; screen_ = Screen::MemoDetail; }
        return;
    case AMemoDelete: memoLine_ = m.line; shell_.confirm.open("Eliminare il memo?", TagDeleteMemo); return;
    case ADelAll: shell_.confirm.open("Eliminare tutti i memo?", TagDeleteAllMemos); return;
    default: break;
    }
    if (a != FKeyBar::None) return;
    switch (ev.key) {
    case Key::Up: memoSel_ = (memoSel_ + n - 1) % n; return;
    case Key::Down: memoSel_ = (memoSel_ + 1) % n; return;
    case Key::Exit: case Key::AC: screen_ = memoReturn_; return;
    default: return;
    }
}

// Scrive il memo di memoLine_ nel documento (aperto o dall'elenco dei file).
void EActivityApp::storeMemo(Context& ctx, bool remove) {
    eact::Document& d = memoDocument();
    if (memoLine_ < 0 || memoLine_ >= static_cast<int>(d.lines.size())) return;
    Line& l = d.lines[static_cast<std::size_t>(memoLine_)];
    const eact::Memo before = l.memo;
    const std::string text = memoText_.text();
    if (!remove && !eact::validText(text)) throw MathError("Data ERROR: testo del memo non valido");
    if (remove || text.empty()) l.memo = eact::Memo{};
    else { l.memo.present = true; l.memo.color = memoColor_; l.memo.text = text.substr(0, eact::MaxMemoBytes); }
    if (memoReturn_ == Screen::Files) {
        try { writeDocument(ctx, memoFile_, d); } catch (...) { l.memo = before; throw; }
    } else {
        if (!fits()) { l.memo = before; memoryError(); }
        modified_ = true;
    }
    buildMemoList(d);
}

void EActivityApp::memoKey(Context& ctx, const KeyEvent& ev) {
    if (ev.is(Key::Exit)) { memoWindow_ = false; return; }
    if (ev.is(Key::Exe)) { storeMemo(ctx, false); memoWindow_ = false; return; }
    if (ev.is(Key::F3)) { memoWindow_ = false; detailReturn_ = screen_; screen_ = Screen::MemoDetail; return; }   // DETAIL
    if (ev.key >= Key::F1 && ev.key <= Key::F6) return;
    if (!memoText_.key(ev, &ctx.textClipboard) && memoText_.full()) ctx.toast("Memo pieno (255 byte)");
}
void EActivityApp::detailKey(Context& ctx, const KeyEvent& ev) {
    if (ev.is(Key::Exit)) { screen_ = detailReturn_; return; }
    if (ev.is(Key::Exe)) { storeMemo(ctx, false); screen_ = detailReturn_; return; }
    if (ev.key >= Key::F1 && ev.key <= Key::F6) return;
    if (!memoText_.key(ev, &ctx.textClipboard) && memoText_.full()) ctx.toast("Memo pieno (255 byte)");
}

// ================================================================ righe
EActivityApp::Frame& EActivityApp::frame() { return frames_.back(); }
eact::Document& EActivityApp::doc() { return frames_.back().doc; }
Line& EActivityApp::current() { Frame& f = frames_.back(); return f.doc.lines[static_cast<std::size_t>(f.cur)]; }

void EActivityApp::enterLine(Context& ctx) {
    if (frames_.empty()) return;
    Frame& f = frame();
    if (f.doc.lines.empty()) f.doc.lines.push_back(Line::textLine());
    f.cur = std::clamp(f.cur, 0, static_cast<int>(f.doc.lines.size()) - 1);
    fieldLine_ = false;
    island_ = false;
    titleEdit_ = false;
    Line& l = current();
    textPos_ = std::min(textPos_, l.text.size());
    while (textPos_ > 0 && textPos_ < l.text.size() && (static_cast<unsigned char>(l.text[textPos_]) & 0xC0) == 0x80) --textPos_;
    if (l.kind == LineKind::Calc && markMode_ == 0) {
        field_.start(ctx, std::string());
        if (field_.editor().linear()) field_.editor().setLinear(false);     // sempre Math (10-7)
        try { if (!l.text.empty()) field_.editor().setExpression(l.text); }
        catch (const std::bad_alloc&) { throw; }
        catch (...) { field_.editor().clear(); field_.editor().insertText(l.text); }
        fieldText_ = field_.empty() ? std::string() : field_.text();
        fieldLine_ = true;
    }
}

// Lasciando la riga: l'espressione modificata va nella riga (il risultato vecchio non vale piu').
void EActivityApp::leaveLine(Context& ctx) {
    if (frames_.empty()) return;
    if (island_) commitIsland(ctx, true);
    titleEdit_ = false;
    if (!fieldLine_) return;
    fieldLine_ = false;
    Line& l = current();
    if (l.kind != LineKind::Calc) return;
    const std::string text = field_.empty() ? std::string() : field_.text();
    if (text == fieldText_) return;
    const std::uint8_t a = l.attr.empty() ? 0 : l.attr[0];
    Line before = l;
    l.text = text;
    l.attr.assign(text.size(), a);
    l.result.reset();
    if (!fits()) { l = std::move(before); memoryError(); }
    modified_ = true;
}

void EActivityApp::moveTo(Context& ctx, int line, bool atEnd) {
    if (frames_.empty()) return;
    leaveLine(ctx);
    Frame& f = frame();
    f.cur = std::clamp(line, 0, static_cast<int>(f.doc.lines.size()) - 1);
    textPos_ = atEnd ? current().text.size() : 0;
    enterLine(ctx);
    if (atEnd) textPos_ = current().text.size();
    ensureVisible(ctx);
}

void EActivityApp::insertLine(Context& ctx, Line line, bool below) {
    leaveLine(ctx);
    Frame& f = frame();
    if (f.doc.lines.size() >= eact::MaxLines) memoryError();
    const int at = f.cur + (below ? 1 : 0);
    f.doc.lines.insert(f.doc.lines.begin() + at, std::move(line));
    if (!fits()) { f.doc.lines.erase(f.doc.lines.begin() + at); memoryError(); }
    f.cur = at;
    textPos_ = 0;
    modified_ = true;
    enterLine(ctx);
    ensureVisible(ctx);
}

void EActivityApp::deleteLine(Context& ctx) {
    Frame& f = frame();
    island_ = false; fieldLine_ = false; titleEdit_ = false;
    f.doc.lines.erase(f.doc.lines.begin() + f.cur);
    if (f.doc.lines.empty()) f.doc.lines.push_back(Line::textLine());
    f.cur = std::min(f.cur, static_cast<int>(f.doc.lines.size()) - 1);
    textPos_ = 0;
    modified_ = true;
    enterLine(ctx);
    ensureVisible(ctx);
}

// ---------------------------------------------------------------- testo
void EActivityApp::insertText(Context& ctx, const std::string& s) {
    Line& l = current();
    if (l.text.size() + s.size() > eact::MaxTextLineBytes) { ctx.toast("Riga piena (255 byte)"); return; }
    const std::uint8_t a = textPos_ > 0 && textPos_ <= l.attr.size() ? l.attr[textPos_ - 1] : 0;
    l.text.insert(textPos_, s);
    l.attr.insert(l.attr.begin() + static_cast<std::ptrdiff_t>(textPos_), s.size(), a);
    if (!fits()) {
        l.text.erase(textPos_, s.size());
        l.attr.erase(l.attr.begin() + static_cast<std::ptrdiff_t>(textPos_), l.attr.begin() + static_cast<std::ptrdiff_t>(textPos_ + s.size()));
        memoryError();
    }
    textPos_ += s.size();
    modified_ = true;
}
void EActivityApp::eraseText(std::size_t from, std::size_t to) {
    Line& l = current();
    l.text.erase(from, to - from);
    l.attr.erase(l.attr.begin() + static_cast<std::ptrdiff_t>(from), l.attr.begin() + static_cast<std::ptrdiff_t>(to));
    textPos_ = from;
    modified_ = true;
}

// Unisce la riga `index` con la seguente (righe di testo): una sola riga con memo (Appendice α).
void EActivityApp::joinLines(Context& ctx, int index) {
    Frame& f = frame();
    if (index < 0 || index + 1 >= static_cast<int>(f.doc.lines.size())) return;
    Line& a = f.doc.lines[static_cast<std::size_t>(index)];
    Line& b = f.doc.lines[static_cast<std::size_t>(index) + 1];
    if (a.kind != LineKind::Text || b.kind != LineKind::Text) return;
    if (a.memo.present && b.memo.present) throw MathError("ERROR: Only one memo allowed per line.");
    if (a.text.size() + b.text.size() > eact::MaxTextLineBytes) { ctx.toast("Riga piena (255 byte)"); return; }
    const std::size_t pos = a.text.size();
    a.text += b.text;
    a.attr.insert(a.attr.end(), b.attr.begin(), b.attr.end());
    if (!a.memo.present) a.memo = std::move(b.memo);
    f.doc.lines.erase(f.doc.lines.begin() + index + 1);
    f.cur = index;
    textPos_ = pos;
    modified_ = true;
    enterLine(ctx);
    ensureVisible(ctx);
}

void EActivityApp::splitLine(Context& ctx) {   // EXE: a capo (10-7)
    Frame& f = frame();
    if (f.doc.lines.size() >= eact::MaxLines) memoryError();
    Line& l = current();
    Line next = Line::textLine(l.text.substr(textPos_));
    next.attr.assign(l.attr.begin() + static_cast<std::ptrdiff_t>(textPos_), l.attr.end());
    const Line before = l;
    l.text.erase(textPos_);
    l.attr.resize(textPos_);
    f.doc.lines.insert(f.doc.lines.begin() + f.cur + 1, std::move(next));
    if (!fits()) { f.doc.lines.erase(f.doc.lines.begin() + f.cur + 1); current() = before; memoryError(); }
    modified_ = true;
    ++f.cur;
    textPos_ = 0;
    enterLine(ctx);
    ensureVisible(ctx);
}

std::vector<EActivityApp::Row> EActivityApp::textRows(const Line& l, bool current) {
    layoutCurrent_ = current;
    auto raw = eact::wrap(l.text, kWidth, &EActivityApp::measurePiece, this);
    layoutCurrent_ = false;
    const int fa = font::baseline(font::Face::Medium), fh = font::lineHeight(font::Face::Medium);
    std::vector<Row> rows(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        rows[i].pieces = std::move(raw[i]);
        rows[i].ascent = fa;
        rows[i].descent = fh - fa;
        for (const eact::Piece& p : rows[i].pieces) {
            if (!p.island) continue;
            const math::Size sz = islandSize(l.text, p.from, current);
            rows[i].ascent = std::max(rows[i].ascent, sz.baseline + 1);
            rows[i].descent = std::max(rows[i].descent, sz.height - sz.baseline + 1);
        }
    }
    if (rows.empty()) { rows.emplace_back(); rows[0].ascent = fa; rows[0].descent = fh - fa; }
    return rows;
}

math::Size EActivityApp::islandSize(const std::string& text, std::size_t from, bool current) {
    if (current && island_ && from == islandFrom_) return math::measure(field_.editor().root());
    std::string expr; std::size_t end;
    if (!eact::islandAt(text, from, expr, end)) return {8, 16, 13};
    return math::measure(*tree(expr));
}

int EActivityApp::measurePiece(const std::string& t, std::size_t a, std::size_t b, bool island, void* user) {
    auto* self = static_cast<EActivityApp*>(user);
    if (!island) return textWidth(t, a, b);
    return std::max(8, self->islandSize(t, a, self->layoutCurrent_).width + 2);
}

ast::NodePtr EActivityApp::tree(const std::string& expr) {
    for (const auto& t : trees_) if (t.first == expr) return t.second;
    if (trees_.size() >= 48) trees_.erase(trees_.begin());
    NaturalEditor tmp;
    try { if (!expr.empty()) tmp.setExpression(expr); }
    catch (const std::bad_alloc&) { throw; }
    catch (...) { tmp.clear(); tmp.insertText(expr); }
    trees_.emplace_back(expr, tmp.copyTree());
    return trees_.back().second;
}

// Posizione (riga visiva, x) del cursore in una riga di testo.
void EActivityApp::caretAt(const std::vector<Row>& rows, const std::string& text, std::size_t pos, int& row, int& x) const {
    row = static_cast<int>(rows.size()) - 1;
    x = kLeft;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const Row& rw = rows[r];
        const bool last = r + 1 == rows.size();
        if (rw.pieces.empty()) { if (last) { row = static_cast<int>(r); x = kLeft; return; } continue; }
        if (pos < rw.from() || (pos >= rw.to() && !last)) continue;
        row = static_cast<int>(r);
        int px = kLeft;
        for (const eact::Piece& p : rw.pieces) {
            if (pos < p.to || (&p == &rw.pieces.back())) {
                if (pos <= p.from) { x = px; return; }
                x = px + (p.island ? p.width : textWidth(text, p.from, std::min(pos, p.to)));
                return;
            }
            px += p.width;
        }
        x = px;
        return;
    }
}
std::size_t EActivityApp::positionAt(const Row& row, const std::string& text, int x) const {
    int px = kLeft;
    for (const eact::Piece& p : row.pieces) {
        if (p.island) {
            if (x < px + p.width / 2) return p.from;
            px += p.width;
            continue;
        }
        for (std::size_t i = p.from; i < p.to;) {
            const std::size_t j = eact::nextUnit(text, i);
            const int w = textWidth(text, i, j);
            if (x < px + w / 2) return i;
            px += w;
            i = j;
        }
    }
    return row.to();
}

void EActivityApp::visualMove(Context& ctx, int dir) {
    Line& l = current();
    const auto rows = textRows(l, true);
    int r, x;
    caretAt(rows, l.text, textPos_, r, x);
    const int target = r + dir;
    if (target < 0 || target >= static_cast<int>(rows.size())) {
        const int line = frame().cur + dir;
        if (line < 0 || line >= static_cast<int>(doc().lines.size())) return;
        moveTo(ctx, line, dir < 0);
        return;
    }
    textPos_ = positionAt(rows[static_cast<std::size_t>(target)], l.text, x);
    // la fine di una riga visiva che va a capo e' l'inizio della seguente: si resta su questa
    if (dir < 0 && target + 1 < static_cast<int>(rows.size()) && textPos_ == rows[static_cast<std::size_t>(target)].to())
        textPos_ = eact::prevUnit(l.text, textPos_);
}

void EActivityApp::clearRow(Context&) {   // AC: solo la riga visualizzata (10-7)
    Line& l = current();
    if (l.text.empty()) return;
    const auto rows = textRows(l, true);
    int r, x;
    caretAt(rows, l.text, textPos_, r, x);
    const Row& row = rows[static_cast<std::size_t>(r)];
    if (row.to() > row.from()) eraseText(row.from(), row.to());
}

void EActivityApp::textKey(Context& ctx, const KeyEvent& ev) {
    Line& l = current();
    Frame& f = frame();
    switch (ev.key) {
    case Key::Char:
        if (ev.ctrl) {
            if (ev.ch == 'c' || ev.ch == 'C') { ctx.textClipboard = l.text; ctx.toast("Copiato"); }
            else if ((ev.ch == 'v' || ev.ch == 'V') && !ctx.textClipboard.empty()) {
                std::string s = ctx.textClipboard.substr(0, ctx.textClipboard.find('\n'));
                if (eact::validText(s)) insertText(ctx, s);
            }
            return;
        }
        insertText(ctx, std::string(1, lower_ ? swapCase(ev.ch) : ev.ch));
        return;
    case Key::Store: insertText(ctx, "\xE2\x86\x92"); return;
    case Key::Exe: splitLine(ctx); return;
    case Key::AC: clearRow(ctx); return;
    case Key::Del:
        if (textPos_ == 0) { if (f.cur > 0) joinLines(ctx, f.cur - 1); return; }
        {
            const std::size_t p = eact::prevUnit(l.text, textPos_);
            if (isIsland(l.text, p)) { beginIsland(ctx, p, true); return; }   // DEL dopo un'espressione: si modifica dentro
            eraseText(p, textPos_);
        }
        return;
    case Key::DelForward:
        if (textPos_ >= l.text.size()) { joinLines(ctx, f.cur); return; }
        eraseText(textPos_, eact::nextUnit(l.text, textPos_));
        return;
    case Key::Left:
        if (textPos_ == 0) { if (f.cur > 0) moveTo(ctx, f.cur - 1, true); return; }
        {
            const std::size_t p = eact::prevUnit(l.text, textPos_);
            if (isIsland(l.text, p)) { beginIsland(ctx, p, true); return; }
            textPos_ = p;
        }
        return;
    case Key::Right:
        if (textPos_ >= l.text.size()) { if (f.cur + 1 < static_cast<int>(f.doc.lines.size())) moveTo(ctx, f.cur + 1, false); return; }
        if (isIsland(l.text, textPos_)) { beginIsland(ctx, textPos_, false); return; }
        textPos_ = eact::nextUnit(l.text, textPos_);
        return;
    case Key::Up: visualMove(ctx, -1); return;
    case Key::Down: visualMove(ctx, 1); return;
    case Key::Home: textPos_ = 0; return;
    case Key::End: textPos_ = l.text.size(); return;
    default: return;
    }
}

// ---------------------------------------------------------------- espressioni nel testo (§3.3)
void EActivityApp::beginIsland(Context& ctx, std::size_t from, bool atEnd) {
    Line& l = current();
    std::string expr; std::size_t end;
    if (!eact::islandAt(l.text, from, expr, end)) return;
    field_.start(ctx, std::string());
    if (field_.editor().linear()) field_.editor().setLinear(false);
    try { if (!expr.empty()) field_.editor().setExpression(expr); }
    catch (const std::bad_alloc&) { throw; }
    catch (...) { field_.editor().clear(); field_.editor().insertText(expr); }
    if (!atEnd) field_.editor().moveHome();
    islandFrom_ = from;
    islandTo_ = end;
    island_ = true;
}
void EActivityApp::insertIsland(Context& ctx) {
    Line& l = current();
    const std::string empty = eact::island(std::string());
    if (l.text.size() + empty.size() > eact::MaxTextLineBytes) { ctx.toast("Riga piena (255 byte)"); return; }
    const std::size_t at = textPos_;
    insertText(ctx, empty);
    beginIsland(ctx, at, true);
    KeyEvent math;
    math.key = Key::F4;                                   // menu MATH dei modelli (1-17)
    (void)field_.key(ctx, math);
}
// L'espressione modificata va subito nella riga (limite dei 255 byte e del file).
void EActivityApp::syncIsland(Context& ctx) {
    Line& l = current();
    const std::string repl = eact::island(field_.empty() ? std::string() : field_.text());
    const std::size_t old = islandTo_ - islandFrom_;
    if (l.text.size() - old + repl.size() > eact::MaxTextLineBytes) {
        (void)field_.editor().undo();
        ctx.toast("Riga piena (255 byte)");
        return;
    }
    if (l.text.compare(islandFrom_, old, repl) == 0) return;
    const std::uint8_t a = islandFrom_ < l.attr.size() ? l.attr[islandFrom_] : 0;
    const Line before = l;
    l.text.replace(islandFrom_, old, repl);
    l.attr.erase(l.attr.begin() + static_cast<std::ptrdiff_t>(islandFrom_), l.attr.begin() + static_cast<std::ptrdiff_t>(islandTo_));
    l.attr.insert(l.attr.begin() + static_cast<std::ptrdiff_t>(islandFrom_), repl.size(), a);
    if (!fits()) { l = before; (void)field_.editor().undo(); memoryError(); }
    islandTo_ = islandFrom_ + repl.size();
    modified_ = true;
}
void EActivityApp::commitIsland(Context& ctx, bool after) {
    if (!island_) return;
    island_ = false;
    try { syncIsland(ctx); } catch (...) {}
    if (field_.empty()) { eraseText(islandFrom_, islandTo_); return; }   // espressione vuota: tolta
    textPos_ = after ? islandTo_ : islandFrom_;
}
bool EActivityApp::fieldAtStart() const {
    const NaturalEditor& e = const_cast<MathField&>(field_).editor();
    return e.activeRowPtr() == &e.root() && e.cursorIndex() == 0;
}
bool EActivityApp::fieldAtEnd() const {
    const NaturalEditor& e = const_cast<MathField&>(field_).editor();
    return e.activeRowPtr() == &e.root() && e.cursorIndex() == e.root().children.size();
}
void EActivityApp::islandKey(Context& ctx, const KeyEvent& ev) {
    if (field_.menuOpen()) { (void)field_.key(ctx, ev); syncIsland(ctx); return; }
    switch (ev.key) {
    case Key::Exe: case Key::Exit: commitIsland(ctx, true); return;
    case Key::Left: if (fieldAtStart()) { commitIsland(ctx, false); return; } break;
    case Key::Right: if (fieldAtEnd()) { commitIsland(ctx, true); return; } break;
    case Key::Del: if (field_.empty()) { commitIsland(ctx, false); return; } break;
    case Key::Up: case Key::Down:
        if (!field_.key(ctx, ev)) { commitIsland(ctx, true); visualMove(ctx, ev.is(Key::Up) ? -1 : 1); }
        return;
    default: break;
    }
    if (field_.key(ctx, ev)) syncIsland(ctx);
}

// ---------------------------------------------------------------- calcoli
void EActivityApp::evaluateLine(Context& ctx, Line& l) {
    auto e = std::make_shared<HistoryEntry>();
    try {
        (void)evaluateEntry(ctx.ws, l.text, *e);
    } catch (const SyntaxError& err) {
        *e = HistoryEntry{}; e->expression = l.text; e->error = err.what();
    } catch (const MathError& err) {
        *e = HistoryEntry{}; e->expression = l.text; e->error = err.what();
    }
    l.result = std::move(e);
}

void EActivityApp::execute(Context& ctx) {   // EXE su una riga di calcolo (10-8)
    Frame& f = frame();
    const std::string text = field_.empty() ? std::string() : field_.text();
    if (text.empty()) return;
    Line& l = current();
    auto e = std::make_shared<HistoryEntry>();
    try {
        (void)evaluateEntry(ctx.ws, text, *e);
    } catch (const SyntaxError& err) { field_.markError(err.where); throw; }
    catch (const MathError& err) { field_.markError(err.where); throw; }
    const Line before = l;
    if (l.text != text) { const std::uint8_t a = l.attr.empty() ? 0 : l.attr[0]; l.text = text; l.attr.assign(text.size(), a); }
    l.result = std::move(e);
    fieldText_ = text;
    if (!fits()) { l = before; memoryError(); }
    modified_ = true;
    ctx.markDirty();
    const int done = f.cur;
    // il risultato e' nella riga; il cursore va alla riga seguente (una nuova riga di calcolo in fondo)
    if (done + 1 >= static_cast<int>(f.doc.lines.size())) insertLine(ctx, Line::calcLine(), true);
    else moveTo(ctx, done + 1, false);
    startRecalc(ctx, static_cast<std::size_t>(done) + 1);
}

void EActivityApp::startRecalc(Context& ctx, std::size_t from) {
    const auto& lines = doc().lines;
    std::size_t end = from;
    bool any = false;
    while (end < lines.size() && lines[end].kind != LineKind::Stop) { any = any || (lines[end].kind == LineKind::Calc && !lines[end].text.empty()); ++end; }
    if (!any) return;
    recalc_.app = this;
    recalc_.ctx = &ctx;
    recalc_.start = recalc_.next = from;
    recalc_.end = end;
    runner_.start(recalc_, step::Runner::Config{4, 20});
}

bool EActivityApp::Recalc::step(step::Budget& budget) {
    auto& lines = app->doc().lines;
    while (next < end && next < lines.size()) {
        Line& l = lines[next];
        if (l.kind == LineKind::Stop) { next = end; break; }
        if (l.kind == LineKind::Calc && !l.text.empty()) app->evaluateLine(*ctx, l);
        ++next;
        if (!budget.consume()) break;
    }
    if (next >= lines.size()) next = end;
    if (next >= end) { ctx->markDirty(); return true; }
    return false;
}

void EActivityApp::calcKey(Context& ctx, const KeyEvent& ev) {
    Frame& f = frame();
    switch (ev.key) {
    case Key::Exe: execute(ctx); return;
    case Key::Up: case Key::Down:
        if (!field_.key(ctx, ev)) moveTo(ctx, f.cur + (ev.is(Key::Up) ? -1 : 1), ev.is(Key::Up));
        return;
    case Key::Optn:                                       // S<->D e frazioni del risultato
        if (!current().result) return;
        shell_.showMenu("RISULTATO", {"S\xE2\x87\x94" "D", "a b/c \xE2\x87\x94 d/c"}, TagCalcView);
        return;
    default: break;
    }
    if (ev.key >= Key::F1 && ev.key <= Key::F6) return;   // la barra di eActivity, non i modelli
    (void)field_.key(ctx, ev);
}

// ---------------------------------------------------------------- strisce
void EActivityApp::stripKey(Context& ctx, const KeyEvent& ev) {
    Line& l = current();
    Frame& f = frame();
    switch (ev.key) {
    case Key::Exe: openStrip(ctx, f.cur); return;
    case Key::Up: moveTo(ctx, f.cur - 1, true); return;
    case Key::Down: moveTo(ctx, f.cur + 1, false); return;
    case Key::Char: {                                     // il primo carattere sostituisce il titolo (10-20)
        if (ev.ctrl) return;
        titleEdit_ = true;
        title_.clear();
        KeyEvent k = ev;
        if (lower_) k.ch = swapCase(k.ch);
        (void)title_.key(k);
        return;
    }
    case Key::Left: case Key::Right: case Key::Del:
        titleEdit_ = true;
        title_.set(l.text);
        (void)title_.key(ev);
        return;
    default: return;
    }
}
void EActivityApp::titleKey(Context& ctx, const KeyEvent& ev) {
    Line& l = current();
    switch (ev.key) {
    case Key::Exe: case Key::Up: case Key::Down: {
        std::string t = title_.text();
        while (t.size() > eact::MaxStripTitleBytes) t.erase(eact::prevUnit(t, t.size()));
        if (!eact::validText(t)) t.clear();
        if (t != l.text) {
            const std::string before = l.text;
            l.text = t;
            if (!fits()) { l.text = before; titleEdit_ = false; memoryError(); }
            modified_ = true;
        }
        titleEdit_ = false;
        if (!ev.is(Key::Exe)) moveTo(ctx, frame().cur + (ev.is(Key::Up) ? -1 : 1), ev.is(Key::Up));
        return;
    }
    case Key::Exit: titleEdit_ = false; return;           // titolo invariato
    case Key::AC: title_.clear(); return;
    case Key::Char: {
        KeyEvent k = ev;
        if (lower_ && !ev.ctrl) k.ch = swapCase(k.ch);
        (void)title_.key(k);
        return;
    }
    default: (void)title_.key(ev); return;
    }
}

void EActivityApp::showStripMenu(int tag) {
    std::vector<std::string> items;
    items.reserve(eact::StripKindCount);
    for (std::size_t k = 0; k < eact::StripKindCount; ++k) {
        const auto kind = static_cast<StripKind>(k);
        std::string s = eact::stripInfo(kind).name;
        if (!eact::stripAvailable(kind)) s += eact::stripInfo(kind).app == AppId::ECon4 ? "  (External)" : "  (non disponibile)";
        items.push_back(std::move(s));
    }
    shell_.showMenu(tag == TagSwitch ? "Cambia applicazione" : "STRIP", std::move(items), tag);
}

void EActivityApp::openStrip(Context& ctx, int index) {
    Line& l = doc().lines[static_cast<std::size_t>(index)];
    const auto kind = static_cast<StripKind>(l.strip);
    if (l.strip >= eact::StripKindCount || !eact::stripAvailable(kind))
        throw MathError(std::string("ERROR: striscia ") + (l.strip < eact::StripKindCount ? eact::stripInfo(kind).name : "sconosciuta") +
                        " non disponibile su questa scheda");
    if (kind == StripKind::Notes) { openNotes(ctx, index); return; }
    leaveLine(ctx);
    const eact::StripInfo& info = eact::stripInfo(kind);
    const auto parts = eact::stripParts(kind);
    auto& saved = ctx.ws.eactivity.savedStrip;
    saved = encodeParts(ctx.ws, parts);                   // dati del documento, ripresi al ritorno
    if (l.data.empty()) { for (auto p : parts) resetPart(ctx.ws, p); }
    else if (!decodeParts(ctx.ws, l.data.data(), l.data.size())) { saved.clear(); throw MathError("Data ERROR: dati della striscia danneggiati"); }
    stripLine_ = index;
    ctx.markDirty();
    try {
        ctx.beginStrip(AppId::EActivity, info.app, l.strip);
    } catch (...) {
        (void)decodeParts(ctx.ws, saved.data(), saved.size());
        saved.clear();
        stripLine_ = -1;
        throw;
    }
}

// Al ritorno: i dati della modalita' vanno nella striscia, quelli del documento tornano.
void EActivityApp::returnFromStrip(Context& ctx) {
    auto& saved = ctx.ws.eactivity.savedStrip;
    const int index = stripLine_;
    stripLine_ = -1;
    if (index == -2) { modified_ = true; return; }        // LIST e MAT/VCT: memorie del documento, nessuno scambio
    if (!open_ || frames_.empty() || index < 0 || index >= static_cast<int>(frames_.front().doc.lines.size()) ||
        frames_.front().doc.lines[static_cast<std::size_t>(index)].kind != LineKind::Strip) {
        if (!saved.empty()) (void)decodeParts(ctx.ws, saved.data(), saved.size());
        saved.clear();
        return;
    }
    while (frames_.size() > 1) frames_.pop_back();
    Line& l = frames_.front().doc.lines[static_cast<std::size_t>(index)];
    std::vector<std::uint8_t> data = encodeParts(ctx.ws, eact::stripParts(static_cast<StripKind>(l.strip)));
    bool fit = true;
    if (data != l.data) {
        std::vector<std::uint8_t> old = std::move(l.data);
        l.data = std::move(data);
        fit = fits();
        if (!fit) l.data = std::move(old);
        else modified_ = true;
    }
    if (!saved.empty() && !decodeParts(ctx.ws, saved.data(), saved.size())) for (auto p : eact::stripParts(static_cast<StripKind>(l.strip))) resetPart(ctx.ws, p);
    saved.clear();
    ctx.markDirty();
    frames_.front().cur = index;
    if (!fit) memoryError();
}

void EActivityApp::openMemoryEditor(Context& ctx, std::uint8_t entry) {   // 'MAT/VCT e 'LIST (10-9)
    leaveLine(ctx);
    stripLine_ = -2;
    try {
        ctx.beginStrip(AppId::EActivity, entry == stripentry::MemoryLists ? AppId::Statistics : AppId::RunMatrix, entry);
    } catch (...) { stripLine_ = -1; throw; }
}

void EActivityApp::openNotes(Context& ctx, int index) {
    leaveLine(ctx);
    const Line& l = doc().lines[static_cast<std::size_t>(index)];
    Frame notes;
    notes.stripLine = index;
    std::string text(l.data.begin(), l.data.end());
    std::size_t a = 0;
    for (;;) {
        const std::size_t b = std::min(text.find('\n', a), text.size());
        std::string row = text.substr(a, b - a);
        while (row.size() > eact::MaxTextLineBytes) {      // righe di Notes oltre 255 byte: divise
            std::size_t cut = eact::MaxTextLineBytes;
            while (cut > 0 && (static_cast<unsigned char>(row[cut]) & 0xC0) == 0x80) --cut;
            notes.doc.lines.push_back(Line::textLine(row.substr(0, cut)));
            row.erase(0, cut);
        }
        if (!eact::validText(row)) row = plainExpression(row);
        notes.doc.lines.push_back(Line::textLine(std::move(row)));
        if (b >= text.size()) break;
        a = b + 1;
    }
    frames_.push_back(std::move(notes));
    textPos_ = 0;
    bar_ = nullptr;
    enterLine(ctx);
    ensureVisible(ctx);
}
void EActivityApp::closeNotes(Context& ctx) {
    if (frames_.size() < 2) return;
    leaveLine(ctx);
    std::string text;
    for (std::size_t i = 0; i < doc().lines.size(); ++i) { if (i) text += '\n'; text += doc().lines[i].text; }
    const int index = frame().stripLine;
    Frame notes = std::move(frames_.back());
    frames_.pop_back();
    Line& l = doc().lines[static_cast<std::size_t>(index)];
    std::vector<std::uint8_t> data(text.begin(), text.end());
    if (data != l.data) {
        std::vector<std::uint8_t> old = std::move(l.data);
        l.data = std::move(data);
        if (!fits()) { l.data = std::move(old); frames_.push_back(std::move(notes)); enterLine(ctx); memoryError(); }
        modified_ = true;
    }
    frame().cur = index;
    bar_ = nullptr;
    enterLine(ctx);
    ensureVisible(ctx);
}

// ---------------------------------------------------------------- immagini (10-14...10-16)
const pict::Image* EActivityApp::picture(Context& ctx, const std::string& file) {
    for (const Pict& p : picts_) if (p.name == file) return p.ok ? &p.img : nullptr;
    Pict p;
    p.name = file;
    if (files::FileSystem* fs = userFiles(ctx)) {
        for (const char* dir : {files::PictFolder, files::CaptureFolder}) {
            std::vector<std::uint8_t> data;
            if (files::readAll(*fs, files::join(dir, file), data, pict::maxEncodedBytes() + 256) != files::Status::Ok) continue;
            if (pict::parse(data.data(), data.size(), p.img).empty()) { p.ok = true; break; }
        }
    }
    while (!picts_.empty() && pictBytes_ + p.img.rle.size() > pict::maxEncodedBytes()) {
        pictBytes_ -= picts_.front().img.rle.size();
        picts_.erase(picts_.begin());
    }
    pictBytes_ += p.img.rle.size();
    picts_.push_back(std::move(p));
    return picts_.back().ok ? &picts_.back().img : nullptr;
}
void EActivityApp::pictureSize(const pict::Image& img, int& w, int& h) {
    w = std::min<int>(img.width, kWidth - 6);
    h = img.width ? img.height * w / img.width : 0;
}
void EActivityApp::openPictureList(Context& ctx) {
    files::FileSystem& fs = store(ctx);
    check(fs.status());
    std::vector<std::string> names;
    for (const char* dir : {files::PictFolder, files::CaptureFolder}) {
        std::vector<files::Entry> entries;
        if (fs.list(dir, entries) != files::Status::Ok) continue;
        for (const files::Entry& e : entries)
            if (!e.directory && files::hasExtension(e.name, files::PictExtension) && files::validName(files::stem(e.name), 8)) names.push_back(e.name);
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    dialog_.setExtension("");
    dialog_.setEmptyText("Nessuna immagine in PICT o CAPT");
    dialog_.openList("PICTURE", std::move(names));
    dialogFor_ = DlgPicture;
}
void EActivityApp::insertPicture(Context& ctx, const std::string& file) {
    const pict::Image* img = picture(ctx, file);
    if (!img) throw MathError("Data ERROR: immagine " + file + " non leggibile");
    if (img->width > kMaxPicture || img->height > kMaxPicture) throw MathError("ERROR: Image wrong size for insertion.");
    Line l;
    l.kind = LineKind::Picture;
    l.text = file;
    insertLine(ctx, std::move(l));
}
void EActivityApp::openFormat() {
    const Line& l = current();
    shell_.showMenu("FORMAT", {std::string("Line Style: ") + kStyleNames[std::min<int>(l.borderStyle, 2)],
                               std::string("Line Color: ") + (l.borderColor == eact::BorderClear ? "Clear" : kBorderNames[std::clamp<int>(l.borderColor, 1, 8) - 1])},
                    TagFormat);
}

// ---------------------------------------------------------------- colori (10-10, 10-11)
void EActivityApp::startMark(Context& ctx, int mode, std::uint8_t value) {
    leaveLine(ctx);
    markMode_ = mode;
    markValue_ = value;
    markLine_ = frame().cur;
    if (current().kind == LineKind::Calc) textPos_ = 0;
    markPos_ = textPos_;
    fieldLine_ = false;
}
void EActivityApp::markRange(int& l0, std::size_t& p0, int& l1, std::size_t& p1) const {
    const Frame& f = frames_.back();
    l0 = markLine_; p0 = markPos_; l1 = f.cur; p1 = textPos_;
    if (l1 < l0 || (l1 == l0 && p1 < p0)) { std::swap(l0, l1); std::swap(p0, p1); }
}
std::uint8_t EActivityApp::shownAttr(int line, std::size_t byte, std::uint8_t stored) const {
    if (!markMode_) return stored;
    int l0, l1; std::size_t p0, p1;
    markRange(l0, p0, l1, p1);
    if (line < l0 || line > l1) return stored;
    const Line& l = frames_.back().doc.lines[static_cast<std::size_t>(line)];
    if (l.kind == LineKind::Text) {
        if ((line == l0 && byte < p0) || (line == l1 && byte >= p1)) return stored;
    }
    return markMode_ == 1 ? eact::attr(eact::attrColor(stored), markValue_) : eact::attr(markValue_, eact::attrMarker(stored));
}
void EActivityApp::applyMark(Context& ctx) {
    int l0, l1; std::size_t p0, p1;
    markRange(l0, p0, l1, p1);
    auto& lines = doc().lines;
    for (int i = l0; i <= l1; ++i) {
        Line& l = lines[static_cast<std::size_t>(i)];
        if (l.kind != LineKind::Text && l.kind != LineKind::Calc) continue;   // non sul risultato (10-10)
        l.attr.resize(l.text.size(), 0);
        for (std::size_t b = 0; b < l.text.size(); ++b) l.attr[b] = shownAttr(i, b, l.attr[b]);
    }
    markMode_ = 0;
    modified_ = true;
    enterLine(ctx);
}
void EActivityApp::markKey(Context& ctx, const KeyEvent& ev) {
    Frame& f = frame();
    Line& l = current();
    const bool calc = l.kind == LineKind::Calc;
    switch (ev.key) {
    case Key::F1: applyMark(ctx); return;                 // SET
    case Key::Exit: case Key::AC: markMode_ = 0; enterLine(ctx); return;
    case Key::Left:
        if (textPos_ == 0 || calc) { if (f.cur > 0) { --f.cur; textPos_ = current().text.size(); ensureVisible(ctx); } return; }
        textPos_ = eact::prevUnit(l.text, textPos_);
        return;
    case Key::Right:
        if (textPos_ >= l.text.size() || calc) {
            if (f.cur + 1 < static_cast<int>(f.doc.lines.size())) { ++f.cur; textPos_ = 0; ensureVisible(ctx); }
            else textPos_ = l.text.size();
            return;
        }
        textPos_ = eact::nextUnit(l.text, textPos_);
        return;
    case Key::Up: if (f.cur > 0) { --f.cur; textPos_ = std::min(textPos_, current().text.size()); ensureVisible(ctx); } return;
    case Key::Down: if (f.cur + 1 < static_cast<int>(f.doc.lines.size())) { ++f.cur; textPos_ = std::min(textPos_, current().text.size()); ensureVisible(ctx); } return;
    case Key::Home: textPos_ = 0; return;
    case Key::End: textPos_ = l.text.size(); return;
    default: return;
    }
}

// ---------------------------------------------------------------- tasti del workspace
void EActivityApp::page(Context& ctx, int dir) {
    Frame& f = frame();
    const int n = static_cast<int>(f.doc.lines.size());
    int h = 0, i = f.cur;
    while (i + dir >= 0 && i + dir < n) {
        h += lineHeight(ctx, i + dir, false);
        if (h > kBottom - kTop && i != f.cur) break;
        i += dir;
    }
    moveTo(ctx, i, false);
}

void EActivityApp::ensureVisible(Context& ctx) {
    if (frames_.empty()) return;
    Frame& f = frame();
    f.first = std::clamp(f.first, 0, std::max(0, static_cast<int>(f.doc.lines.size()) - 1));
    if (f.cur < f.first) { f.first = f.cur; return; }
    for (;;) {
        int h = 0;
        for (int i = f.first; i <= f.cur; ++i) h += lineHeight(ctx, i, i == f.cur);
        if (h <= kBottom - kTop || f.first >= f.cur) return;
        ++f.first;
    }
}

void EActivityApp::docAction(Context& ctx, int a) {
    Frame& f = frame();
    Line& l = current();
    switch (a) {
    case ASave: saveDocument(ctx, fileName_); return;
    case ASaveAs: leaveLine(ctx); listFiles(ctx); nameDialog("SAVE\xC2\xB7" "AS", files::stem(fileName_), DlgSaveAs); return;
    case AOpt: {                                          // differenza D2
        files::FileSystem& fs = store(ctx);
        check(fs.status());
        check(files::recover(fs, directory()));
        std::uint64_t free = 0;
        (void)fs.freeSpace(free);
        ctx.toast("OPT: archivio in ordine, " + std::to_string(free / 1024) + " KB liberi");
        return;
    }
    case ACapacity: leaveLine(ctx); screen_ = Screen::Capacity; return;
    case ASize: if (l.kind == LineKind::Strip) screen_ = Screen::StripSize; return;
    case AStrip: showStripMenu(TagStrip); return;
    case AToCalc: {                                       // TEXT: la riga di testo diventa di calcolo
        if (l.kind != LineKind::Text) return;
        leaveLine(ctx);
        const Line before = l;
        const std::string expr = plainExpression(l.text);
        const std::uint8_t at = l.attr.empty() ? 0 : l.attr[0];
        l.kind = LineKind::Calc;
        l.text = expr;
        l.attr.assign(expr.size(), at);
        l.result.reset();
        if (!fits()) { l = before; memoryError(); }
        modified_ = true;
        enterLine(ctx);
        return;
    }
    case AToText: {                                       // CALC: la riga di calcolo (o di stop) diventa di testo
        leaveLine(ctx);
        const Line before = l;
        const std::string t = l.kind == LineKind::Calc && !l.text.empty() ? eact::island(l.text) : std::string();
        if (t.size() > eact::MaxTextLineBytes) { ctx.toast("Espressione troppo lunga per una riga di testo"); return; }
        const std::uint8_t at = l.attr.empty() ? 0 : l.attr[0];
        l.kind = LineKind::Text;
        l.text = t;
        l.attr.assign(t.size(), at);
        l.result.reset();
        if (!fits()) { l = before; memoryError(); }
        textPos_ = l.text.size();
        modified_ = true;
        enterLine(ctx);
        return;
    }
    case AChar: charOpen_ = true; charSel_ = 0; return;
    case ACase: lower_ = !lower_; return;
    case ATop: moveTo(ctx, 0, false); return;
    case ABottom: moveTo(ctx, static_cast<int>(f.doc.lines.size()) - 1, true); return;
    case APageUp: page(ctx, -1); return;
    case APageDown: page(ctx, 1); return;
    case ADelLine:
        shell_.confirm.open(l.kind == LineKind::Picture ? "Eliminare l'immagine?" : l.kind == LineKind::Strip ? "Eliminare la striscia?" : "Eliminare la riga?",
                            TagDeleteLine);
        return;
    case AInsText: insertLine(ctx, Line::textLine()); return;
    case AInsCalc: insertLine(ctx, Line::calcLine()); return;
    case AInsStop: { Line s; s.kind = LineKind::Stop; insertLine(ctx, std::move(s)); return; }
    case AInsPicture: leaveLine(ctx); openPictureList(ctx); return;
    case ANotesInsert: insertLine(ctx, Line::textLine()); return;
    case AMath:
        if (l.kind == LineKind::Text) { insertIsland(ctx); return; }
        if (l.kind == LineKind::Stop) { l = Line::calcLine(); modified_ = true; enterLine(ctx); }
        if (current().kind == LineKind::Calc && fieldLine_) { KeyEvent m; m.key = Key::F4; (void)field_.key(ctx, m); }
        return;
    case AMarker: shell_.showMenu("MARKER", {"1: Magenta", "2: Green", "3: Cyan", "4: Yellow", "Clear"}, TagMarker); return;
    case ACharColor: {
        std::vector<std::string> items;
        for (std::uint8_t i = 0; i < eact::TextColorCount; ++i) items.push_back(std::to_string(i + 1) + ": " + kColorNames[i]);
        shell_.showMenu("CHAR", std::move(items), TagCharColor);
        return;
    }
    case AMatVct: openMemoryEditor(ctx, stripentry::MemoryMatrices); return;
    case AList: openMemoryEditor(ctx, stripentry::MemoryLists); return;
    case AMemoInsert: {
        if (l.kind != LineKind::Text && l.kind != LineKind::Calc) return;
        if (l.memo.present) throw MathError("ERROR: Only one memo allowed per line.");
        std::vector<std::string> items;
        for (std::uint8_t i = 0; i < eact::TextColorCount; ++i) items.push_back(std::to_string(i + 1) + ": " + kColorNames[i]);
        shell_.showMenu("MEMO", std::move(items), TagMemoColor);
        return;
    }
    case AMemoRemove:
        if (!l.memo.present) throw MathError("No MEMO: nessun memo su questa riga");
        memoReturn_ = Screen::Doc; memoLine_ = f.cur;
        shell_.confirm.open("Eliminare il memo?", TagDeleteMemo);
        return;
    case ACatalog:
        leaveLine(ctx);
        if (frames_.front().doc.memoCount() == 0) throw MathError("No MEMO: il file non ha memo");
        memoReturn_ = Screen::Doc;
        buildMemoList(frames_.front().doc);
        memoSel_ = 0;
        screen_ = Screen::MemoList;
        return;
    case AView:
        if (!l.memo.present) throw MathError("No MEMO: nessun memo su questa riga");
        memoReturn_ = Screen::Doc; memoLine_ = f.cur; memoColor_ = l.memo.color;
        (void)memoText_.setText(l.memo.text);
        memoText_.bottom();
        memoWindow_ = true;
        return;
    default: return;
    }
}

void EActivityApp::menuChoice(Context& ctx, int tag, int choice) {
    switch (tag) {
    case TagStrip: case TagSwitch: {
        const auto kind = static_cast<StripKind>(choice);
        if (!eact::stripAvailable(kind))
            throw MathError(std::string("ERROR: ") + eact::stripInfo(kind).name + " non disponibile su questa scheda");
        Line s;
        s.kind = LineKind::Strip;
        s.strip = static_cast<std::uint8_t>(choice);
        if (tag == TagStrip) {                            // sopra la riga, poi il titolo (10-18)
            insertLine(ctx, std::move(s));
            titleEdit_ = true;
            title_.clear();
        } else {                                          // SHIFT , : la nuova striscia si apre subito
            s.text = eact::stripInfo(kind).name;
            if (s.text.size() > eact::MaxStripTitleBytes) s.text.resize(eact::MaxStripTitleBytes);
            insertLine(ctx, std::move(s), true);
            openStrip(ctx, frame().cur);
        }
        return;
    }
    case TagMarker: startMark(ctx, 1, static_cast<std::uint8_t>(choice < 4 ? choice + 1 : 0)); return;
    case TagCharColor: startMark(ctx, 2, static_cast<std::uint8_t>(choice)); return;
    case TagMemoColor:
        memoReturn_ = Screen::Doc; memoLine_ = frame().cur; memoColor_ = static_cast<std::uint8_t>(choice);
        memoText_.clear();
        memoWindow_ = true;
        return;
    case TagFormat:
        if (choice == 0) shell_.showMenu("Line Style", {"1: Normal", "2: Thick", "3: Thin"}, TagLineStyle);
        else {
            std::vector<std::string> items;
            for (int i = 0; i < 8; ++i) items.push_back(std::to_string(i + 1) + ": " + kBorderNames[i]);
            items.emplace_back("Clear");
            shell_.showMenu("Line Color", std::move(items), TagLineColor);
        }
        return;
    case TagLineStyle: current().borderStyle = static_cast<std::uint8_t>(std::min(choice, 2)); modified_ = true; openFormat(); return;
    case TagLineColor:
        current().borderColor = choice < 8 ? static_cast<std::uint8_t>(choice + 1) : eact::BorderClear;
        modified_ = true;
        openFormat();
        return;
    case TagCalcView: {
        Line& l = current();
        if (!l.result) return;
        const bool ok = choice == 0 ? toggleExactDecimal(*l.result, ctx.ws.settings) : toggleMixedFraction(*l.result, ctx.ws.settings);
        if (!ok) ctx.toast("Nessuna altra forma del risultato");
        else modified_ = true;
        return;
    }
    default: return;
    }
}

void EActivityApp::confirmed(Context& ctx, int tag) {
    switch (tag) {
    case TagDeleteFile: {
        if (sel_ < 0 || sel_ >= static_cast<int>(items_.size()) || items_[static_cast<std::size_t>(sel_)].folder) return;
        check(store(ctx).remove(files::join(directory(), items_[static_cast<std::size_t>(sel_)].file)));
        listFiles(ctx);
        return;
    }
    case TagDeleteLine: deleteLine(ctx); return;
    case TagDeleteMemo:
        storeMemo(ctx, true);
        if (screen_ == Screen::MemoList && memos_.empty()) screen_ = memoReturn_;
        memoSel_ = std::min(memoSel_, std::max(0, static_cast<int>(memos_.size()) - 1));
        return;
    case TagDeleteAllMemos: {
        eact::Document& d = memoDocument();
        eact::Document before = d;
        for (Line& l : d.lines) l.memo = eact::Memo{};
        if (memoReturn_ == Screen::Files) {
            try { writeDocument(ctx, memoFile_, d); } catch (...) { d = std::move(before); throw; }
        } else modified_ = true;
        memos_.clear();
        screen_ = memoReturn_;
        return;
    }
    default: return;
    }
}

void EActivityApp::charKey(Context& ctx, const KeyEvent& ev) {
    const auto chars = charList(charPage_);
    const int n = static_cast<int>(chars.size());
    switch (ev.key) {
    case Key::F1: case Key::F2: case Key::F3: case Key::F4:
        charPage_ = static_cast<int>(ev.key) - static_cast<int>(Key::F1); charSel_ = 0; return;
    case Key::Left: if (n) charSel_ = (charSel_ + n - 1) % n; return;
    case Key::Right: if (n) charSel_ = (charSel_ + 1) % n; return;
    case Key::Up: if (n) charSel_ = (charSel_ - kCharCols + n * kCharCols) % n; return;
    case Key::Down: if (n) charSel_ = (charSel_ + kCharCols) % n; return;
    case Key::Exit: case Key::AC: charOpen_ = false; return;
    case Key::Exe: {
        charOpen_ = false;
        if (!n) return;
        const std::string ch = chars[static_cast<std::size_t>(std::clamp(charSel_, 0, n - 1))];
        if (screen_ != Screen::Doc || frames_.empty()) return;
        if (titleEdit_ || current().kind == LineKind::Strip) {
            if (!titleEdit_) { titleEdit_ = true; title_.set(current().text); }
            if (title_.text().size() + ch.size() <= eact::MaxStripTitleBytes) title_.insert(ch);
            return;
        }
        if (current().kind == LineKind::Text) insertText(ctx, ch);
        return;
    }
    default: return;
    }
}

void EActivityApp::updateBar() {
    const FKeyMenu* m = nullptr;
    switch (screen_) {
    case Screen::Files: m = &kFilesMenu; break;
    case Screen::MemoList: m = memoReturn_ == Screen::Files ? &kMemoListMenu : &kCatalogMenu; break;
    case Screen::Doc:
        if (frames_.empty()) break;
        if (markMode_) { m = &kMarkMenu; break; }
        if (frames_.size() > 1) { m = &kNotesMenu; break; }
        switch (current().kind) {
        case LineKind::Text: m = &kTextMenu; break;
        case LineKind::Calc: m = &kCalcMenu; break;
        case LineKind::Stop: m = &kStopMenu; break;
        case LineKind::Picture: m = &kPictureMenu; break;
        case LineKind::Strip: m = &kStripMenu; break;
        }
        break;
    default: break;
    }
    if (m != bar_) { bar_ = m; shell_.fkeys.set(m); }
}

void EActivityApp::docKey(Context& ctx, const KeyEvent& ev) {
    if (frames_.empty()) { screen_ = Screen::Files; return; }
    if (markMode_) { markKey(ctx, ev); return; }
    if (island_) { islandKey(ctx, ev); return; }
    if (titleEdit_) { titleKey(ctx, ev); return; }
    if (fieldLine_ && field_.menuOpen()) { (void)field_.key(ctx, ev); return; }
    const bool shift = ctx.shift();
    updateBar();
    const int a = shell_.fkeys.key(ev, false);
    if (a >= 0) {
        shell_.fkeys.set(bar_);                           // dopo un'azione la barra torna alla prima pagina
        docAction(ctx, a);
        return;
    }
    if (a == FKeyBar::Handled) return;
    Frame& f = frame();
    const LineKind kind = current().kind;
    if (shift && ev.is(Key::Up) && kind == LineKind::Picture) { openFormat(); return; }   // SHIFT ↑ = FORMAT (10-16)
    if (ev.is(Key::PageUp) || (shift && ev.is(Key::Up))) { page(ctx, -1); return; }       // 10-6
    if (ev.is(Key::PageDown) || (shift && ev.is(Key::Down))) { page(ctx, 1); return; }
    if (shift && ev.isChar('\'')) {                       // SHIFT ': eActivity <-> applicazione della striscia
        if (frames_.size() > 1) closeNotes(ctx);
        else if (kind == LineKind::Strip) openStrip(ctx, f.cur);
        return;
    }
    if (ev.is(Key::Exit)) {
        if (frames_.size() > 1) closeNotes(ctx);
        else exitDocument(ctx);
        return;
    }
    switch (kind) {
    case LineKind::Text: textKey(ctx, ev); return;
    case LineKind::Calc: calcKey(ctx, ev); return;
    case LineKind::Strip: stripKey(ctx, ev); return;
    case LineKind::Stop: case LineKind::Picture:
        switch (ev.key) {
        case Key::Up: moveTo(ctx, f.cur - 1, true); return;
        case Key::Down: moveTo(ctx, f.cur + 1, false); return;
        case Key::Del: case Key::DelForward: docAction(ctx, ADelLine); return;
        default: return;
        }
    }
}

void EActivityApp::key(Context& ctx, const KeyEvent& ev) {
    if (runner_.running()) {                              // ricalcolo in corso: solo AC (Break)
        if (ev.is(Key::AC)) {
            (void)runner_.interrupt();
            runner_.reset();
            ctx.markDirty();
            throw MathError(std::string(step::Runner::BreakMessage) + ": ricalcolo interrotto");
        }
        return;
    }
    if (charOpen_) { charKey(ctx, ev); return; }
    if (dialog_.active()) { dialogKey(ctx, ev); return; }
    if (memoWindow_) { memoKey(ctx, ev); return; }
    if (shell_.confirm.active() && shell_.confirm.tag() == TagSave) {   // Save? Si' / No / AC (10-23)
        switch (ev.key) {
        case Key::F1: case Key::Exe: shell_.confirm.close(); saveDocument(ctx, fileName_); closeDocument(ctx); return;
        case Key::F6: case Key::Exit: shell_.confirm.close(); closeDocument(ctx); return;
        case Key::AC: shell_.confirm.close(); return;
        default: return;
        }
    }
    const bool modal = shell_.modalKey(ev,
        [&](int tag, bool yes) { if (yes) confirmed(ctx, tag); },
        [&](int) {},
        [&](int tag, int choice) { menuChoice(ctx, tag, choice); });
    if (modal) return;
    switch (screen_) {
    case Screen::Files: filesKey(ctx, ev); return;
    case Screen::MemoList: memoListKey(ctx, ev); return;
    case Screen::MemoDetail: detailKey(ctx, ev); return;
    case Screen::Capacity: case Screen::StripSize:
        if (ev.is(Key::Exit) || ev.is(Key::AC) || ev.is(Key::Exe)) screen_ = Screen::Doc;
        return;
    case Screen::Doc: docKey(ctx, ev); return;
    }
}

// ================================================================ disegno
int EActivityApp::lineHeight(Context& ctx, int index, bool current) {
    const Line& l = frames_.back().doc.lines[static_cast<std::size_t>(index)];
    switch (l.kind) {
    case LineKind::Text: {
        int h = 0;
        for (const Row& r : textRows(l, current)) h += r.height();
        return std::max(h, 18);
    }
    case LineKind::Calc: {
        const math::Size e = current && fieldLine_ ? math::measure(field_.editor().root()) : math::measure(*tree(l.text));
        int h = std::max(18, e.height + 4);
        if (l.result) h += math::measure(*resultTree(*l.result, ctx.ws.settings)).height + 6;
        return h;
    }
    case LineKind::Stop: return 12;
    case LineKind::Picture: {
        const pict::Image* img = picture(ctx, l.text);
        if (!img) return 24;
        int w, h;
        pictureSize(*img, w, h);
        return h + 8;
    }
    case LineKind::Strip: return 24;
    }
    return 18;
}

void EActivityApp::drawText(Context& ctx, gfx::Canvas& c, int index, int y, bool current) {
    (void)ctx;
    const Line& l = frames_.back().doc.lines[static_cast<std::size_t>(index)];
    const auto rows = textRows(l, current);
    const int fa = font::baseline(font::Face::Medium);
    int caretRow = -1, caretX = 0;
    if (current && !island_) caretAt(rows, l.text, textPos_, caretRow, caretX);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const Row& row = rows[r];
        const int h = row.height(), base = y + 1 + row.ascent;
        int x = kLeft;
        for (const eact::Piece& p : row.pieces) {
            if (p.island) {
                const std::uint8_t a = shownAttr(index, p.from, p.from < l.attr.size() ? l.attr[p.from] : 0);
                if (eact::attrMarker(a)) c.rect(x, y, p.width, h, markerColor(eact::attrMarker(a)), true);
                if (current && island_ && p.from == islandFrom_) {
                    field_.draw(c, x + 1, base, kWidth - (x - kLeft), h);
                    c.setClip({0, kTop, ui::ScreenW, kBottom - kTop});
                } else {
                    std::string expr; std::size_t end;
                    if (eact::islandAt(l.text, p.from, expr, end)) {
                        const auto t = tree(expr);
                        const auto lay = math::layout(*t, x + 1, base, p.width + 4, h + 40);
                        math::draw(c, lay, textColor(eact::attrColor(a)), Cursor, Placeholder);
                    }
                }
                x += p.width;
                continue;
            }
            std::size_t i = p.from;
            while (i < p.to) {
                const std::uint8_t a = shownAttr(index, i, i < l.attr.size() ? l.attr[i] : 0);
                std::size_t j = eact::nextUnit(l.text, i);
                while (j < p.to && shownAttr(index, j, j < l.attr.size() ? l.attr[j] : 0) == a) j = eact::nextUnit(l.text, j);
                const std::string s = l.text.substr(i, j - i);
                const int w = font::measure(font::Face::Medium, s.c_str()).width;
                if (eact::attrMarker(a)) c.rect(x, y, w, h, markerColor(eact::attrMarker(a)), true);
                ui::text(c, x, base - fa, s.c_str(), textColor(eact::attrColor(a)));
                x += w;
                i = j;
            }
        }
        if (static_cast<int>(r) == caretRow) {
            const std::uint16_t col = markMode_ == 1 ? (markValue_ ? markerColor(markValue_) : Muted) : markMode_ == 2 ? textColor(markValue_) : Cursor;
            c.rect(caretX, y + 1, 2, h - 2, col == Bg ? Muted : col, true);
        }
        y += h;
    }
}

void EActivityApp::drawCalc(Context& ctx, gfx::Canvas& c, int index, int y, bool current) {
    const Line& l = frames_.back().doc.lines[static_cast<std::size_t>(index)];
    const std::uint8_t a = shownAttr(index, 0, l.attr.empty() ? 0 : l.attr[0]);
    const math::Size e = current && fieldLine_ ? math::measure(field_.editor().root()) : math::measure(*tree(l.text));
    const int eh = std::max(18, e.height + 4);
    if (eact::attrMarker(a)) c.rect(kLeft, y, std::min(kWidth, e.width + 4), eh, markerColor(eact::attrMarker(a)), true);
    const int base = y + 2 + e.baseline;
    if (current && fieldLine_) {
        field_.draw(c, kLeft, base, kWidth, eh);
        c.setClip({0, kTop, ui::ScreenW, kBottom - kTop});
    } else {
        const auto lay = math::layout(*tree(l.text), kLeft, base, kWidth, eh + 40);
        math::draw(c, lay, textColor(eact::attrColor(a)), Cursor, Placeholder);
        if (e.width > kWidth) ui::text(c, ui::ScreenW - 14, y + 2, "\xE2\x96\xB6", Accent, font::Face::Small);   // 10-7
        if (current && markMode_) c.rect(kLeft, y + eh - 2, std::min(kWidth, e.width), 2, Cursor, true);
    }
    if (l.result) {
        const auto rt = resultTree(*l.result, ctx.ws.settings);
        const math::Size rs = math::measure(*rt);
        const int ry = y + eh, rh = rs.height + 6;
        const bool failed = !l.result->error.empty() && l.result->error[0] != '=';
        const bool wide = rs.width > kWidth;
        const auto lay = wide ? math::layout(*rt, kLeft, ry + 2 + rs.baseline, kWidth, rh, math::Align::Left)
                              : math::layout(*rt, kLeft, ry + 2 + rs.baseline, kWidth, rh, math::Align::Right);
        math::draw(c, lay, failed ? Error : Ink, Cursor, Placeholder);
        if (wide) ui::text(c, ui::ScreenW - 14, ry + 2, "\xE2\x96\xB6", Accent, font::Face::Small);
        c.hLine(kLeft, ry + rh - 2, kWidth, Grid);
    }
}

void EActivityApp::drawLine(Context& ctx, gfx::Canvas& c, int index, int y, int h, bool current) {
    const Line& l = frames_.back().doc.lines[static_cast<std::size_t>(index)];
    switch (l.kind) {
    case LineKind::Text: drawText(ctx, c, index, y, current); return;
    case LineKind::Calc: drawCalc(ctx, c, index, y, current); return;
    case LineKind::Stop:
        if (current) c.rect(kLeft, y, kWidth, h, SelectSoft, true);
        c.dashedLine(kLeft, y + h / 2, kLeft + kWidth - 30, y + h / 2, Error, 5, 3);
        ui::text(c, kLeft + kWidth - 26, y, "STOP", Error, font::Face::Tiny);
        return;
    case LineKind::Picture: {
        const pict::Image* img = picture(ctx, l.text);
        if (!img) {
            c.rect(kLeft, y + 2, kWidth, h - 4, current ? Error : Faint, false);
            ui::textFit(c, kLeft + 6, y + 5, l.text + "  (immagine mancante)", kWidth - 12, Muted, font::Face::Small);
            return;
        }
        int w, ph;
        pictureSize(*img, w, ph);
        const int x = kLeft + 3, py = y + 4;
        drawPicture(c, *img, x, py, w, ph);
        if (l.borderColor != eact::BorderClear) {
            const std::uint16_t col = current ? Select : borderInk(l.borderColor);
            if (l.borderStyle == eact::BorderThin) c.dottedRect(x - 1, py - 1, w + 2, ph + 2, col);
            else {
                c.rect(x - 1, py - 1, w + 2, ph + 2, col, false);
                if (l.borderStyle == eact::BorderThick) c.rect(x - 2, py - 2, w + 4, ph + 4, col, false);
            }
        } else if (current) {
            c.rect(x - 2, py - 2, w + 4, ph + 4, Error, false);   // selezionata: bordo rosso (10-15)
        }
        return;
    }
    case LineKind::Strip: {
        const bool known = l.strip < eact::StripKindCount;
        const auto kind = static_cast<StripKind>(l.strip);
        const bool avail = known && eact::stripAvailable(kind);
        const std::uint16_t fill = current ? Select : SelectSoft, ink = current ? SelectInk : Ink;
        c.roundedRect(kLeft, y + 2, kWidth, h - 4, 3, fill, true);
        c.roundedRect(kLeft, y + 2, kWidth, h - 4, 3, current ? Select : Accent, false);
        const std::string name = known ? eact::stripInfo(kind).name : "Striscia " + std::to_string(l.strip);
        const int nw = ui::text(c, kLeft + 5, y + 7, name.c_str(), current ? SelectInk : Accent, font::Face::Small);
        const int tx = kLeft + 12 + std::max(nw, 70);
        if (current && titleEdit_) {
            c.rect(tx - 2, y + 4, kWidth - (tx - kLeft) - 2, h - 8, Bg, true);
            title_.draw(c, tx, y + 4, kWidth - (tx - kLeft) - 4, Ink);
        } else if (!avail) {
            ui::textFit(c, tx, y + 4, (l.text.empty() ? std::string() : l.text + "  ") + "non disponibile", kWidth - (tx - kLeft) - 4, current ? SelectInk : Muted);
        } else {
            ui::textFit(c, tx, y + 4, l.text, kWidth - (tx - kLeft) - 4, ink);
        }
        return;
    }
    }
}

void EActivityApp::drawDoc(Context& ctx, gfx::Canvas& c) {
    const bool notes = frames_.size() > 1;
    std::string title = notes ? "Notes" : "eActivity  " + files::stem(fileName_) + (modified_ ? "*" : "");
    if (lower_) title += "  a\xE2\x87\x94" "A";
    if (markMode_) title += markMode_ == 1 ? "  MARKER" : "  CHAR";
    ui::StatusInfo st = ctx.status(title.c_str());
    st.strip = st.strip || notes;                         // Notes e' aperta da una striscia (barra azzurra)
    ui::statusBar(c, st);
    Frame& f = frame();
    ensureVisible(ctx);
    const int n = static_cast<int>(f.doc.lines.size());
    int y = kTop, shown = 0;
    c.setClip({0, kTop, ui::ScreenW, kBottom - kTop});
    for (int i = f.first; i < n && y < kBottom; ++i) {
        const bool cur = i == f.cur;
        const int h = lineHeight(ctx, i, cur);
        drawLine(ctx, c, i, y, h, cur);
        y += h;
        ++shown;
    }
    c.resetClip();
    if (n > shown || f.first > 0) ui::scrollBar(c, ui::ScreenW - 5, kTop, kBottom - kTop, n, std::max(1, shown), f.first);   // 10-1
    // Icona del memo della riga (10-11), in alto a destra.
    const Line& l = current();
    if (l.memo.present) {
        const int mx = ui::ScreenW - 20, my = kTop + 1;
        c.rect(mx, my, 12, 11, textColor(l.memo.color), true);
        c.rect(mx, my, 12, 11, Ink, false);
        c.hLine(mx + 2, my + 3, 8, Bg);
        c.hLine(mx + 2, my + 6, 6, Bg);
    }
    if (island_) ui::softKeys(c, MathField::softKeys(ctx.shift()));
    else if (titleEdit_) { const char* k[6] = {}; ui::softKeys(c, k); }
    else { updateBar(); shell_.fkeys.draw(c, false); }
    if (field_.menuOpen()) field_.drawMenu(c);
}

void EActivityApp::drawFiles(Context& ctx, gfx::Canvas& c) {
    const std::string title = folder_.empty() ? std::string("eActivity") : "eActivity  /" + folder_;
    ctx.drawStatus(c, title.c_str());
    const int top = kTop, rows = (kBottom - top) / kFileRowH;
    const int n = static_cast<int>(items_.size());
    if (!message_.empty()) {
        ui::textCenter(c, ui::ScreenW / 2, 120, message_.c_str(), Error);
    } else if (n == 0) {
        ui::textCenter(c, ui::ScreenW / 2, 110, "Nessun file eActivity", Muted);
        ui::textCenter(c, ui::ScreenW / 2, 132, "F2 NEW: nuovo file", Muted, font::Face::Small);
    } else {
        if (sel_ < first_) first_ = sel_;
        if (sel_ >= first_ + rows) first_ = sel_ - rows + 1;
        for (int i = first_; i < n && i < first_ + rows; ++i) {
            const Item& it = items_[static_cast<std::size_t>(i)];
            const int y = top + (i - first_) * kFileRowH;
            const bool sel = i == sel_;
            if (sel) c.rect(0, y, ui::ScreenW - 6, kFileRowH, Select, true);
            const std::uint16_t ink = sel ? SelectInk : Ink;
            if (it.folder) {                              // icona della cartella
                c.rect(8, y + 7, 14, 10, sel ? SelectInk : Warn, true);
                c.rect(8, y + 5, 6, 3, sel ? SelectInk : Warn, true);
            } else {
                c.rect(9, y + 4, 11, 14, sel ? SelectInk : Accent, false);
                c.hLine(11, y + 8, 7, sel ? SelectInk : Accent);
                c.hLine(11, y + 11, 7, sel ? SelectInk : Accent);
            }
            ui::text(c, 30, y + 3, it.name.c_str(), ink);
            if (!it.folder) {
                char sz[24];
                std::snprintf(sz, sizeof(sz), "%u B", static_cast<unsigned>(it.size));
                ui::textRight(c, ui::ScreenW - 12, y + 5, sz, sel ? SelectInk : Muted, font::Face::Small);
            }
        }
        if (n > rows) ui::scrollBar(c, ui::ScreenW - 5, top, rows * kFileRowH, n, rows, first_);
    }
    updateBar();
    shell_.fkeys.draw(c, false);
}

void EActivityApp::drawMemoList(Context& ctx, gfx::Canvas& c) {
    const std::string title = "MEMO  " + (memoReturn_ == Screen::Files ? files::stem(memoFile_) : files::stem(fileName_));
    ctx.drawStatus(c, title.c_str());
    const int n = static_cast<int>(memos_.size()), rows = (kBottom - kTop) / kFileRowH;
    const int first = std::max(0, memoSel_ - rows + 1);
    for (int i = first; i < n && i < first + rows; ++i) {
        const MemoRef& m = memos_[static_cast<std::size_t>(i)];
        const int y = kTop + (i - first) * kFileRowH;
        const bool sel = i == memoSel_;
        if (sel) c.rect(0, y, ui::ScreenW - 6, kFileRowH, Select, true);
        c.rect(8, y + 5, 12, 11, textColor(m.color), true);
        c.rect(8, y + 5, 12, 11, sel ? SelectInk : Ink, false);
        ui::textFit(c, 28, y + 3, m.text.substr(0, m.text.find('\n')), ui::ScreenW - 80, sel ? SelectInk : Ink);
        const std::string ln = "riga " + std::to_string(m.line + 1);
        ui::textRight(c, ui::ScreenW - 10, y + 5, ln.c_str(), sel ? SelectInk : Muted, font::Face::Small);
    }
    if (n > rows) ui::scrollBar(c, ui::ScreenW - 5, kTop, rows * kFileRowH, n, rows, first);
    updateBar();
    shell_.fkeys.draw(c, false);
}

void EActivityApp::drawMemoWindow(gfx::Canvas& c) {
    const gfx::Rect box{24, 70, ui::ScreenW - 48, 160};
    ui::panel(c, box, Bg, Ink);
    c.rect(box.x + 1, box.y + 1, box.w - 2, 18, textColor(memoColor_), true);
    ui::text(c, box.x + 6, box.y + 3, "MEMO", memoColor_ == eact::TextYellow || memoColor_ == eact::TextCyan ? Ink : Bg, font::Face::Small);
    char used[24];
    std::snprintf(used, sizeof(used), "%u/255", static_cast<unsigned>(memoText_.bytes()));
    ui::textRight(c, box.x + box.w - 6, box.y + 4, used, memoColor_ == eact::TextYellow || memoColor_ == eact::TextCyan ? Ink : Bg, font::Face::Small);
    memoText_.setVisibleRows(8);
    memoText_.draw(c, {box.x + 6, box.y + 22, box.w - 12, box.h - 28}, Ink);
    const char* k[6] = {nullptr, nullptr, "DETAIL", nullptr, nullptr, nullptr};
    ui::softKeys(c, k);
}

void EActivityApp::drawChars(gfx::Canvas& c) {
    const auto chars = charList(charPage_);
    const int cell = 28, rows = (static_cast<int>(chars.size()) + kCharCols - 1) / kCharCols;
    const gfx::Rect box{(ui::ScreenW - kCharCols * cell - 12) / 2, 50, kCharCols * cell + 12, rows * 24 + 30};
    ui::panel(c, box, Bg, Ink);
    const std::string title = std::string("CHAR  ") + kCharPages[charPage_].name;
    ui::text(c, box.x + 6, box.y + 4, title.c_str(), Accent, font::Face::Small);
    for (int i = 0; i < static_cast<int>(chars.size()); ++i) {
        const int x = box.x + 6 + (i % kCharCols) * cell, y = box.y + 22 + (i / kCharCols) * 24;
        const bool sel = i == charSel_;
        if (sel) c.rect(x, y, cell - 2, 22, Select, true);
        ui::textCenter(c, x + cell / 2 - 1, y + 3, chars[static_cast<std::size_t>(i)].c_str(), sel ? SelectInk : Ink);
    }
    const char* k[6] = {"MATH", "SYMBOL", "GREEK", "LANG", nullptr, nullptr};
    ui::softKeys(c, k);
}

void EActivityApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    switch (screen_) {
    case Screen::Files: drawFiles(ctx, c); break;
    case Screen::MemoList: drawMemoList(ctx, c); break;
    case Screen::MemoDetail: {
        ctx.drawStatus(c, "MEMO  DETAIL");
        c.rect(0, kTop - 2, 6, kBottom - kTop + 4, textColor(memoColor_), true);
        memoText_.setVisibleRows((kBottom - kTop - 8) / ui::LineH);
        memoText_.draw(c, {10, kTop + 2, ui::ScreenW - 16, kBottom - kTop - 6}, Ink);
        const char* k[6] = {};
        ui::softKeys(c, k);
        break;
    }
    case Screen::Capacity: {                              // 10-23
        ctx.drawStatus(c, "CAPACITY");
        const std::size_t used = documentBytes(), cap = limits::kEActivityDocumentBytes;
        char line[64];
        std::snprintf(line, sizeof(line), "File: %u byte", static_cast<unsigned>(used));
        ui::text(c, 16, 70, line, Ink);
        std::snprintf(line, sizeof(line), "Rimanenti: %u byte", static_cast<unsigned>(used < cap ? cap - used : 0));
        ui::text(c, 16, 96, line, Ink);
        c.rect(16, 130, ui::ScreenW - 32, 16, Ink, false);
        c.rect(18, 132, static_cast<int>((ui::ScreenW - 36) * std::min(used, cap) / cap), 12, used * 10 > cap * 9 ? Error : Accent, true);
        std::snprintf(line, sizeof(line), "Limite del file su questa scheda: %u KB", static_cast<unsigned>(cap / 1024));
        ui::text(c, 16, 160, line, Muted, font::Face::Small);
        const char* k[6] = {};
        ui::softKeys(c, k);
        break;
    }
    case Screen::StripSize: {                             // 10-21
        ctx.drawStatus(c, "SIZE");
        const Line& l = current();
        const std::string name = l.strip < eact::StripKindCount ? eact::stripInfo(static_cast<StripKind>(l.strip)).name : "Striscia";
        ui::text(c, 16, 60, name.c_str(), Accent);
        ui::text(c, 16, 84, l.text.c_str(), Ink);
        char line[64];
        std::snprintf(line, sizeof(line), "Dati della striscia: %u byte", static_cast<unsigned>(l.data.size() + l.text.size() + 8));
        ui::text(c, 16, 116, line, Ink);
        const char* k[6] = {};
        ui::softKeys(c, k);
        break;
    }
    case Screen::Doc:
        if (frames_.empty()) { drawFiles(ctx, c); break; }
        drawDoc(ctx, c);
        break;
    }
    if (memoWindow_) drawMemoWindow(c);
    if (charOpen_) drawChars(c);
    shell_.drawModals(c);
    dialog_.draw(c);
    if (runner_.running()) {
        const std::string p = "Ricalcolo... " + std::to_string(std::max(0, runner_.percent())) + "%  AC: Break";
        ui::toast(c, p.c_str());
    }
}
} // namespace fx::app
