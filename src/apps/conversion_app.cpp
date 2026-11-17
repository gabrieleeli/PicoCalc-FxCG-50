// CONVERSION (v1.14.0): convertitore completo.
//  - Grandezze: l'unica tabella di units.hpp (77 categorie, 771 unita', prefissi
//    SI e IEC, conversioni non lineari e derivate) - la stessa di Run-Matrix.
//  - Formati: unit_formats.hpp (basi, IEEE-754, romani, date, colori...).
//  - Valute: tassi inseriti dall'utente e salvati nel workspace (record v10).
// Il valore e' un'espressione (A-Z, Ans); il risultato va in Ans e, quando le
// unita' sono esatte, anche in forma esatta.
#include "apps.hpp"
#include "exact.hpp"
#include "expression.hpp"
#include "natural_editor.hpp"
#include "number_format.hpp"
#include "unit_formats.hpp"
#include "units.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace fx::app {
using namespace ui::theme;

namespace {
enum Kind : unsigned { Quantity = 0, Format = 1, Money = 2 };
struct Currency { const char* code; const char* name; };
const Currency kCurrencies[Settings::Conversion::Currencies] = {
    {"EUR", "euro"}, {"USD", "dollaro USA"}, {"GBP", "sterlina britannica"}, {"CHF", "franco svizzero"},
    {"JPY", "yen giapponese"}, {"CNY", "yuan cinese"}, {"CAD", "dollaro canadese"}, {"AUD", "dollaro australiano"},
    {"SEK", "corona svedese"}, {"NOK", "corona norvegese"}, {"DKK", "corona danese"}, {"PLN", "zloty polacco"},
    {"CZK", "corona ceca"}, {"HUF", "fiorino ungherese"}, {"INR", "rupia indiana"}, {"BRL", "real brasiliano"},
};
constexpr std::uint16_t code(unsigned kind, std::size_t index) { return static_cast<std::uint16_t>((kind << 8) | index); }
unsigned kindOf(std::uint16_t c) { return c >> 8; }
std::size_t indexOf(std::uint16_t c) { return c & 0xFF; }
bool validEntry(std::uint16_t c) {
    switch (kindOf(c)) {
    case Quantity: return indexOf(c) < units::categoryCount();
    case Format: return indexOf(c) < unitfmt::converterCount();
    case Money: return indexOf(c) == 0;
    default: return false;
    }
}
const char* entryName(std::uint16_t c) {
    switch (kindOf(c)) {
    case Quantity: return units::category(indexOf(c)).name;
    case Format: return unitfmt::converter(indexOf(c)).name;
    default: return "Valute";
    }
}
units::Group entryGroup(std::uint16_t c) {
    switch (kindOf(c)) {
    case Quantity: return units::category(indexOf(c)).group;
    case Format: return unitfmt::converter(indexOf(c)).group;
    default: return units::Group::Currency;
    }
}
std::size_t firstUnit(std::uint16_t c) { std::size_t f = 0, n = 0; units::unitsOf(indexOf(c), f, n); return f; }
std::size_t unitTotal(std::uint16_t c) { std::size_t f = 0, n = 0; units::unitsOf(indexOf(c), f, n); return n; }
units::Ref ref(std::uint16_t c, std::size_t i, int prefix) { return {&units::unit(firstUnit(c) + i), prefix}; }
std::string lower(const std::string& s) { std::string o = s; for (char& ch : o) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); return o; }
// La voce corrisponde alla ricerca: nome della voce, di un'unita' o di una rappresentazione.
bool entryMatches(std::uint16_t c, const std::string& q) {
    if (units::matchesText(entryName(c), q)) return true;
    if (kindOf(c) == Quantity) {
        const std::size_t f = firstUnit(c), n = unitTotal(c);
        for (std::size_t i = f; i < f + n; ++i) if (units::matches(units::unit(i), q)) return true;
    } else if (kindOf(c) == Format) {
        const auto& cv = unitfmt::converter(indexOf(c));
        for (const char* const* r = cv.reps; *r; ++r) if (units::matchesText(*r, q)) return true;
    } else {
        for (const auto& cur : kCurrencies) if (units::matchesText(cur.code, q) || units::matchesText(cur.name, q)) return true;
    }
    return false;
}
// Testo da copiare per un numero: accettato dal parser (niente "E").
std::string numberExpression(double v) {
    char b[48];
    if (v == 0 || (std::fabs(v) >= 1e-4 && std::fabs(v) < 1e15)) { std::snprintf(b, sizeof b, "%.15g", v); return b; }
    const int e = static_cast<int>(std::floor(std::log10(std::fabs(v))));
    std::snprintf(b, sizeof b, "%.14g*10^(%d)", v / std::pow(10.0, e), e);
    return b;
}
bool plainNumber(const std::string& s, double& out) {
    if (s.empty() || s.find_first_not_of("0123456789+-.eE") != std::string::npos) return false;
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return end && !*end && std::isfinite(out);
}
} // namespace

// ------------------------------------------------------------------ stato
void ConversionApp::enter(Context& ctx) {
    if (ctx.resuming() && screen_ != Screen::Menu) return;
    screen_ = Screen::Menu; query_.clear(); sel_ = 0; first_ = 0; editing_ = false;
    buildMenu(ctx);
}

void ConversionApp::buildMenu(Context& ctx) {
    rows_.clear();
    const auto& cv = ctx.ws.settings.conversion;
    std::vector<std::uint16_t> all;
    for (std::size_t g = 0; g < static_cast<std::size_t>(units::Group::Count); ++g) {
        const auto group = static_cast<units::Group>(g);
        for (std::size_t i = 0; i < units::categoryCount(); ++i) if (units::category(i).group == group) all.push_back(code(Quantity, i));
        for (std::size_t i = 0; i < unitfmt::converterCount(); ++i) if (unitfmt::converter(i).group == group) all.push_back(code(Format, i));
        if (group == units::Group::Currency) all.push_back(code(Money, 0));
    }
    if (!query_.empty()) {
        const std::string q = lower(query_);
        rows_.push_back({0, -1, "Risultati della ricerca"});
        for (std::uint16_t c : all) if (entryMatches(c, q)) rows_.push_back({c, -1, nullptr});
    } else {
        bool header = false;
        for (std::uint16_t f : cv.favorites) if (validEntry(f)) {
            if (!header) { rows_.push_back({0, -1, "Preferiti"}); header = true; }
            rows_.push_back({f, -1, nullptr});
        }
        header = false;
        for (std::size_t i = 0; i < cv.recent.size(); ++i) if (validEntry(cv.recent[i].entry)) {
            if (!header) { rows_.push_back({0, -1, "Recenti"}); header = true; }
            rows_.push_back({cv.recent[i].entry, static_cast<std::int16_t>(i), nullptr});
        }
        units::Group last = units::Group::Count;
        for (std::uint16_t c : all) {
            if (entryGroup(c) != last) { last = entryGroup(c); rows_.push_back({0, -1, units::groupName(last)}); }
            rows_.push_back({c, -1, nullptr});
        }
    }
    sel_ = std::clamp(sel_, 0, std::max(0, static_cast<int>(rows_.size()) - 1));
    while (sel_ + 1 < static_cast<int>(rows_.size()) && rows_[static_cast<std::size_t>(sel_)].header) ++sel_;
}

void ConversionApp::open(Context& ctx, std::uint16_t c) {
    if (!validEntry(c)) throw MathError("Argument ERROR: voce sconosciuta");
    entry_ = c; screen_ = Screen::Convert; field_ = 0; editing_ = false;
    from_ = 0; to_ = 1; fromPrefix_ = toPrefix_ = -1;
    result_.clear(); exact_.clear(); copyText_.clear(); query_.clear();
    if (kindOf(c) == Quantity) {
        // Da: l'unita' di riferimento della categoria se c'e', A: la successiva.
        const units::Ref si = units::find(indexOf(c), units::category(indexOf(c)).si);
        if (si) { from_ = static_cast<std::uint16_t>(units::indexOf(*si.unit) - firstUnit(c)); to_ = static_cast<std::uint16_t>(from_ + 1u < unitTotal(c) ? from_ + 1u : 0u); }
        if (unitTotal(c) < 2) to_ = 0;
        value_ = "1";
    } else if (kindOf(c) == Format) {
        const auto& cv = unitfmt::converter(indexOf(c));
        formatParam_ = cv.paramDefault;
        value_.clear();
        if (unitfmt::repCount(cv) < 2) to_ = 0;
    } else value_ = "1";
    (void)ctx;
}

std::size_t ConversionApp::optionCount() const {
    switch (kindOf(entry_)) {
    case Quantity: return unitTotal(entry_);
    case Format: return unitfmt::repCount(unitfmt::converter(indexOf(entry_)));
    default: return Settings::Conversion::Currencies;
    }
}
std::string ConversionApp::optionName(std::size_t i, bool withLabel) const {
    switch (kindOf(entry_)) {
    case Quantity: {
        const units::Unit& u = units::unit(firstUnit(entry_) + i);
        std::string s = u.name;
        if (withLabel) s += std::string("  ") + u.label + (u.has(units::flag::Approx) ? " \xE2\x89\x88" : "") + (u.has(units::flag::Historic) ? " (storica)" : "");
        return s;
    }
    case Format: return unitfmt::converter(indexOf(entry_)).reps[i];
    default: return withLabel ? std::string(kCurrencies[i].code) + "  " + kCurrencies[i].name : kCurrencies[i].code;
    }
}
std::string ConversionApp::fromName() const {
    return kindOf(entry_) == Quantity ? units::symbol(ref(entry_, from_, fromPrefix_)) : optionName(from_, false);
}
std::string ConversionApp::toName() const {
    return kindOf(entry_) == Quantity ? units::symbol(ref(entry_, to_, toPrefix_)) : optionName(to_, false);
}
bool ConversionApp::hasParam() const {
    if (kindOf(entry_) == Quantity) return units::category(indexOf(entry_)).param != units::Param::None;
    return kindOf(entry_) == Format && unitfmt::converter(indexOf(entry_)).paramName != nullptr;
}
std::string ConversionApp::paramText(Context& ctx) const {
    if (kindOf(entry_) == Format) return fmt(ctx, formatParam_);
    const auto& cv = ctx.ws.settings.conversion;
    return fmt(ctx, units::category(indexOf(entry_)).param == units::Param::Dpi ? cv.dpi : cv.molarMass);
}

double ConversionApp::convertValue(Context& ctx, double v, std::size_t from, int fromPrefix, std::size_t to, int toPrefix) const {
    const auto& cv = ctx.ws.settings.conversion;
    if (kindOf(entry_) == Money) {
        const double a = from == 0 ? 1 : cv.rates[from], b = to == 0 ? 1 : cv.rates[to];
        if (!(a > 0)) throw MathError(std::string("Argument ERROR: tasso di ") + kCurrencies[from].code + " non impostato (F4 TASSI)");
        if (!(b > 0)) throw MathError(std::string("Argument ERROR: tasso di ") + kCurrencies[to].code + " non impostato (F4 TASSI)");
        const double r = v / a * b;
        if (!std::isfinite(r)) throw MathError("Math ERROR: risultato fuori intervallo");
        return r;
    }
    units::Params p;
    p.dpi = cv.dpi; p.molarMass = cv.molarMass;
    return units::convert(v, ref(entry_, from, fromPrefix), ref(entry_, to, toPrefix), p);
}

void ConversionApp::convert(Context& ctx) {
    result_.clear(); exact_.clear(); copyText_.clear();
    if (kindOf(entry_) == Format) {
        unitfmt::Options o;
        o.param = formatParam_; o.angle = ctx.ws.settings.angle; o.display = ctx.ws.settings.display;
        result_ = unitfmt::convert(indexOf(entry_), from_, to_, value_, o);
        double v = 0;
        if (plainNumber(result_, v)) { ctx.ws.memory.setAnswer(v); copyText_ = numberExpression(v); }
        else copyText_ = "\"" + result_ + "\"";
        remember(ctx);
        return;
    }
    const double v = evalNumber(ctx, value_);
    const double r = convertValue(ctx, v, from_, fromPrefix_, to_, toPrefix_);
    ctx.ws.memory.setAnswer(r);
    result_ = fmt(ctx, r);
    copyText_ = numberExpression(r);
    if (kindOf(entry_) == Quantity) {
        // Forma esatta quando le unita' sono esatte e il valore e' esatto (1/3, 2π...).
        try {
            EvalContext ec; ec.memory = &ctx.ws.memory;
            exact::Value in, out;
            bool decimal = false;
            if (evaluateExact(*parseExpression(value_), ec, in, &decimal) &&
                units::convertExact(in, ref(entry_, from_, fromPrefix_), ref(entry_, to_, toPrefix_), out) && exact::fitsRelaxed(out)) {
                const std::string t = exact::toText(out, ctx.ws.settings.mixedFractions);
                ctx.ws.memory.setAnswerExact(r, out, decimal);
                copyText_ = exact::toExpression(out);
                if (t != result_) exact_ = t;
            }
        } catch (const std::exception&) {}
    }
    remember(ctx);
}

void ConversionApp::remember(Context& ctx) {
    auto& rec = ctx.ws.settings.conversion.recent;
    Settings::Conversion::Recent e;
    e.entry = entry_; e.from = from_; e.to = to_;
    e.fromPrefix = static_cast<std::int8_t>(fromPrefix_); e.toPrefix = static_cast<std::int8_t>(toPrefix_);
    e.value = value_.substr(0, Settings::Conversion::MaxValue);
    rec.erase(std::remove_if(rec.begin(), rec.end(), [&](const Settings::Conversion::Recent& r) {
        return r.entry == e.entry && r.from == e.from && r.to == e.to && r.value == e.value && r.fromPrefix == e.fromPrefix && r.toPrefix == e.toPrefix;
    }), rec.end());
    rec.insert(rec.begin(), e);
    if (rec.size() > Settings::Conversion::MaxRecent) rec.resize(Settings::Conversion::MaxRecent);
    ctx.markDirty();
}

void ConversionApp::swap() {
    std::swap(from_, to_);
    std::swap(fromPrefix_, toPrefix_);
    if (kindOf(entry_) == Format && !unitfmt::readable(unitfmt::converter(indexOf(entry_)), from_)) std::swap(from_, to_), std::swap(fromPrefix_, toPrefix_);
    else if (!result_.empty() && kindOf(entry_) == Format) value_ = result_;
}

void ConversionApp::showAll(Context& ctx) {
    all_.clear();
    all_.title = (kindOf(entry_) == Format ? value_ : value_ + " " + fromName()) + " =";
    unitfmt::Options o;
    o.param = formatParam_; o.angle = ctx.ws.settings.angle; o.display = ctx.ws.settings.display;
    double v = 0;
    if (kindOf(entry_) != Format) v = evalNumber(ctx, value_);
    for (std::size_t i = 0; i < optionCount(); ++i) {
        std::string text;
        try {
            if (kindOf(entry_) == Format) text = unitfmt::convert(indexOf(entry_), from_, i, value_, o);
            else text = fmt(ctx, convertValue(ctx, v, from_, fromPrefix_, i, -1));
        } catch (const std::exception& e) {
            const char* w = e.what();
            const char* colon = std::strchr(w, ':');
            text = colon ? std::string(w, colon) : "ERROR";
        }
        all_.add(optionName(i, false), text);
    }
    screen_ = Screen::All;
}

void ConversionApp::copy(Context& ctx) {
    if (copyText_.empty()) convert(ctx);
    NaturalEditor ed;
    ed.setExpression(copyText_);
    ctx.clipboard = ed.copyTree();
    ctx.toast("Copiato (Ctrl+V per incollare, risultato in Ans)");
}

void ConversionApp::buildPick() {
    pick_.clear();
    const std::string q = lower(query_);
    if (screen_ == Screen::Prefix) {
        const units::Unit& u = units::unit(firstUnit(entry_) + (pickTo_ ? to_ : from_));
        if (q.empty()) pick_.push_back(0xFFFF);
        for (std::size_t p = 0; p < units::prefixCount(); ++p)
            if (units::prefixAllowed(u, p) && (q.empty() || units::matchesText(units::prefix(p).name, q) || query_ == units::prefix(p).sym)) pick_.push_back(static_cast<std::uint16_t>(p));
    } else {
        // Prima il simbolo identico ("eV" prima di "meV"), poi le altre corrispondenze.
        for (int pass = 0; pass < 2; ++pass)
            for (std::size_t i = 0; i < optionCount(); ++i) {
                if (kindOf(entry_) == Format && !pickTo_ && !unitfmt::readable(unitfmt::converter(indexOf(entry_)), i)) continue;
                const bool same = !query_.empty() && optionName(i, false) == query_;
                if (pass == 0 ? same : !same && (q.empty() || units::matchesText(optionName(i, true).c_str(), q))) pick_.push_back(static_cast<std::uint16_t>(i));
            }
    }
    sel_ = 0; first_ = 0;
}

// ------------------------------------------------------------------ tasti
void ConversionApp::key(Context& ctx, const KeyEvent& ev) {
    switch (screen_) {
    case Screen::Menu: menuKey(ctx, ev); return;
    case Screen::Convert: convertKey(ctx, ev); return;
    case Screen::All:
        if (ev.is(Key::Exit) || ev.is(Key::F6) || ev.is(Key::Exe)) { screen_ = Screen::Convert; return; }
        (void)all_.key(ev);
        return;
    case Screen::Pick: case Screen::Prefix: pickKey(ctx, ev); return;
    case Screen::Rates: ratesKey(ctx, ev); return;
    }
}

void ConversionApp::menuKey(Context& ctx, const KeyEvent& ev) {
    const int n = static_cast<int>(rows_.size());
    auto move = [&](int step) {
        if (n == 0) return;
        int s = sel_;
        for (int k = 0; k < n; ++k) {
            s = (s + step + n) % n;
            if (!rows_[static_cast<std::size_t>(s)].header) { sel_ = s; return; }
        }
    };
    switch (ev.key) {
    case Key::Up: move(-1); return;
    case Key::Down: move(1); return;
    case Key::PageUp: for (int k = 0; k < 10; ++k) move(-1); return;
    case Key::PageDown: for (int k = 0; k < 10; ++k) move(1); return;
    case Key::Exe: case Key::Right: case Key::F1: {
        if (sel_ >= n || rows_[static_cast<std::size_t>(sel_)].header) return;
        const Row r = rows_[static_cast<std::size_t>(sel_)];
        open(ctx, r.code);
        const auto& rec = ctx.ws.settings.conversion.recent;
        if (r.recent >= 0 && static_cast<std::size_t>(r.recent) < rec.size()) {
            const auto e = rec[static_cast<std::size_t>(r.recent)];
            if (e.from < optionCount() && e.to < optionCount()) {
                from_ = e.from; to_ = e.to; value_ = e.value;
                const auto prefixOk = [&](int p, std::size_t u) { return p < 0 || (kindOf(entry_) == Quantity && units::prefixAllowed(units::unit(firstUnit(entry_) + u), static_cast<std::size_t>(p))); };
                fromPrefix_ = prefixOk(e.fromPrefix, from_) ? e.fromPrefix : -1;
                toPrefix_ = prefixOk(e.toPrefix, to_) ? e.toPrefix : -1;
                convert(ctx);
            }
        }
        return;
    }
    case Key::F2: {                                   // preferito sì/no
        if (sel_ >= n || rows_[static_cast<std::size_t>(sel_)].header) return;
        auto& fav = ctx.ws.settings.conversion.favorites;
        const std::uint16_t c = rows_[static_cast<std::size_t>(sel_)].code;
        const auto it = std::find(fav.begin(), fav.end(), c);
        if (it != fav.end()) { fav.erase(it); ctx.toast("Tolto dai preferiti"); }
        else if (fav.size() < Settings::Conversion::MaxFavorites) { fav.push_back(c); ctx.toast("Aggiunto ai preferiti"); }
        else ctx.toast("Preferiti pieni (16)");
        ctx.markDirty();
        buildMenu(ctx);
        return;
    }
    case Key::Del: if (!query_.empty()) { query_.pop_back(); sel_ = 0; buildMenu(ctx); } return;
    case Key::AC: query_.clear(); sel_ = 0; buildMenu(ctx); return;
    case Key::Exit: case Key::F6:
        if (!query_.empty()) { query_.clear(); sel_ = 0; buildMenu(ctx); return; }
        ctx.home();
        return;
    case Key::Char:
        if (ev.ctrl || static_cast<unsigned char>(ev.ch) < 0x20 || query_.size() >= 16) return;
        query_ += ev.ch; sel_ = 0; first_ = 0; buildMenu(ctx);
        return;
    default: return;
    }
}

void ConversionApp::convertKey(Context& ctx, const KeyEvent& ev) {
    const int fields = hasParam() ? 4 : 3;
    if (editing_) {
        auto commit = [&]() {
            const std::string t = editor_.text();
            if (field_ == 3) {
                const double p = evalNumber(ctx, t);
                if (kindOf(entry_) == Format) {
                    const auto& cv = unitfmt::converter(indexOf(entry_));
                    if (!(p >= cv.paramMin && p <= cv.paramMax)) throw MathError(std::string("Argument ERROR: ") + cv.paramName + " fuori intervallo");
                    formatParam_ = p;
                } else {
                    if (!(p > 0 && p <= 1e5)) throw MathError("Argument ERROR: parametro > 0");
                    auto& cv = ctx.ws.settings.conversion;
                    (units::category(indexOf(entry_)).param == units::Param::Dpi ? cv.dpi : cv.molarMass) = p;
                    ctx.markDirty();
                }
            } else {
                if (kindOf(entry_) != Format) (void)evalNumber(ctx, t);   // Syntax/Math ERROR subito
                value_ = t;
            }
            editing_ = false;
        };
        if (ev.is(Key::Exe)) { const int f = field_; commit(); if (f == 0 || !result_.empty()) convert(ctx); return; }
        if (ev.is(Key::Exit)) { editing_ = false; return; }
        if (ev.is(Key::Up) || ev.is(Key::Down)) { commit(); field_ = (field_ + (ev.is(Key::Up) ? fields - 1 : 1)) % fields; return; }
        (void)editor_.key(ev);
        return;
    }
    const std::size_t count = optionCount();
    switch (ev.key) {
    case Key::Up: field_ = (field_ + fields - 1) % fields; return;
    case Key::Down: field_ = (field_ + 1) % fields; return;
    case Key::Left: case Key::Right:
        if (field_ == 1 || field_ == 2) {
            std::uint16_t& u = field_ == 1 ? from_ : to_;
            for (std::size_t k = 0; k < count; ++k) {
                u = static_cast<std::uint16_t>((u + (ev.is(Key::Left) ? count - 1 : 1)) % count);
                if (field_ == 2 || kindOf(entry_) != Format || unitfmt::readable(unitfmt::converter(indexOf(entry_)), u)) break;
            }
            (field_ == 1 ? fromPrefix_ : toPrefix_) = -1;
            result_.clear(); exact_.clear();
        }
        return;
    case Key::Exe:
        if (field_ == 1 || field_ == 2) { pickTo_ = field_ == 2; query_.clear(); screen_ = Screen::Pick; buildPick(); return; }
        editing_ = true; editor_.set(field_ == 3 ? paramText(ctx) : value_);
        return;
    case Key::AC: if (field_ == 0) { value_.clear(); result_.clear(); exact_.clear(); } return;
    case Key::F1: convert(ctx); return;
    case Key::F2: swap(); if (!value_.empty()) convert(ctx); return;
    case Key::F3: showAll(ctx); return;
    case Key::F4:
        if (kindOf(entry_) == Money) { screen_ = Screen::Rates; sel_ = 1; first_ = 0; editing_ = false; return; }
        if (kindOf(entry_) == Quantity) {
            pickTo_ = field_ == 2;
            const units::Unit& u = units::unit(firstUnit(entry_) + (pickTo_ ? to_ : from_));
            bool any = false;
            for (std::size_t p = 0; p < units::prefixCount(); ++p) any = any || units::prefixAllowed(u, p);
            if (!any) { ctx.toast(std::string("[") + u.name + "] non ammette prefissi"); return; }
            query_.clear(); screen_ = Screen::Prefix; buildPick();
        }
        return;
    case Key::F5: copy(ctx); return;
    case Key::Exit: case Key::F6: screen_ = Screen::Menu; query_.clear(); buildMenu(ctx); return;
    case Key::Char:
        if (ev.ctrl) { if ((ev.ch == 'c' || ev.ch == 'C') && !result_.empty()) copy(ctx); return; }
        if (field_ == 1 || field_ == 2) {           // lettere su Da/A: elenco filtrato
            pickTo_ = field_ == 2; query_ = std::string(1, ev.ch); screen_ = Screen::Pick; buildPick();
            return;
        }
        editing_ = true; editor_.clear(); (void)editor_.key(ev);
        return;
    default: return;
    }
}

void ConversionApp::pickKey(Context& ctx, const KeyEvent& ev) {
    const int n = static_cast<int>(pick_.size());
    switch (ev.key) {
    case Key::Up: if (n) sel_ = (sel_ + n - 1) % n; return;
    case Key::Down: if (n) sel_ = (sel_ + 1) % n; return;
    case Key::PageUp: sel_ = std::max(0, sel_ - 10); return;
    case Key::PageDown: sel_ = std::max(0, std::min(n - 1, sel_ + 10)); return;
    case Key::Exe: case Key::F1: {
        if (sel_ >= n) return;
        const std::uint16_t v = pick_[static_cast<std::size_t>(sel_)];
        if (screen_ == Screen::Prefix) (pickTo_ ? toPrefix_ : fromPrefix_) = v == 0xFFFF ? -1 : v;
        else { (pickTo_ ? to_ : from_) = v; (pickTo_ ? toPrefix_ : fromPrefix_) = -1; }
        screen_ = Screen::Convert; query_.clear(); result_.clear(); exact_.clear();
        if (!value_.empty()) { try { convert(ctx); } catch (const std::exception&) { result_.clear(); throw; } }
        return;
    }
    case Key::Del: if (!query_.empty()) { query_.pop_back(); buildPick(); } return;
    case Key::Exit: case Key::F6:
        if (!query_.empty()) { query_.clear(); buildPick(); return; }
        screen_ = Screen::Convert;
        return;
    case Key::Char:
        if (ev.ctrl || static_cast<unsigned char>(ev.ch) < 0x20 || query_.size() >= 16) return;
        query_ += ev.ch; buildPick();
        return;
    default: return;
    }
}

void ConversionApp::ratesKey(Context& ctx, const KeyEvent& ev) {
    auto& rates = ctx.ws.settings.conversion.rates;
    const int n = static_cast<int>(Settings::Conversion::Currencies);
    if (editing_) {
        if (ev.is(Key::Exe)) {
            const std::string t = editor_.text();
            const double v = t.empty() ? 0 : evalNumber(ctx, t);
            if (!(v >= 0) || !std::isfinite(v)) throw MathError("Argument ERROR: tasso >= 0 (0 = non impostato)");
            rates[static_cast<std::size_t>(sel_)] = v;
            editing_ = false; ctx.markDirty();
            sel_ = std::min(sel_ + 1, n - 1);
            return;
        }
        if (ev.is(Key::Exit)) { editing_ = false; return; }
        (void)editor_.key(ev);
        return;
    }
    switch (ev.key) {
    case Key::Up: sel_ = std::max(1, sel_ - 1); return;
    case Key::Down: sel_ = std::min(n - 1, sel_ + 1); return;
    case Key::Exe: editing_ = true; editor_.set(rates[static_cast<std::size_t>(sel_)] > 0 ? numberExpression(rates[static_cast<std::size_t>(sel_)]) : ""); return;
    case Key::Del: case Key::AC: rates[static_cast<std::size_t>(sel_)] = 0; ctx.markDirty(); return;
    case Key::Exit: case Key::F6: screen_ = Screen::Convert; result_.clear(); return;
    case Key::Char:
        if (ev.ctrl) return;
        editing_ = true; editor_.clear(); (void)editor_.key(ev);
        return;
    default: return;
    }
}

// ------------------------------------------------------------------ disegno
void ConversionApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "CONVERSION");
    switch (screen_) {
    case Screen::Menu: drawMenu(ctx, c); return;
    case Screen::Convert: drawConvert(ctx, c); return;
    case Screen::All: {
        all_.draw(c, ui::ContentTop + 2, ui::ContentBottom - 2);
        const char* k[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, "EXIT"};
        ui::softKeys(c, k);
        return;
    }
    case Screen::Pick: case Screen::Prefix: drawPick(ctx, c); return;
    case Screen::Rates: drawRates(ctx, c); return;
    }
}

void ConversionApp::drawMenu(Context& ctx, gfx::Canvas& c) {
    int y = ui::ContentTop + 2;
    c.rect(0, y, ui::ScreenW, 20, SelectSoft, true);
    if (query_.empty()) ui::text(c, 6, y + 2, "Scrivi per cercare (grandezza o unita')", Muted, font::Face::Small);
    else { ui::textFit(c, 6, y + 2, "Cerca: " + query_ + "_", ui::ScreenW - 80, Ink); }
    char cnt[24];
    std::size_t entries = 0;
    for (const Row& r : rows_) if (!r.header) ++entries;
    std::snprintf(cnt, sizeof cnt, "%u voci", static_cast<unsigned>(entries));
    ui::textRight(c, ui::ScreenW - 6, y + 4, cnt, Muted, font::Face::Small);
    y += 22;
    const int rowH = 18, visible = (ui::ContentBottom - 2 - y) / rowH, n = static_cast<int>(rows_.size());
    if (sel_ < first_) first_ = sel_;
    if (sel_ >= first_ + visible) first_ = sel_ - visible + 1;
    first_ = std::clamp(first_, 0, std::max(0, n - visible));
    if (first_ > 0 && sel_ == first_ && rows_[static_cast<std::size_t>(first_ - 1)].header) --first_;   // mostra il titolo del gruppo
    const auto& rec = ctx.ws.settings.conversion.recent;
    const auto& fav = ctx.ws.settings.conversion.favorites;
    for (int i = 0; i < visible && first_ + i < n; ++i) {
        const Row& r = rows_[static_cast<std::size_t>(first_ + i)];
        const int ry = y + i * rowH;
        if (r.header) {
            c.rect(0, ry, ui::ScreenW, rowH, StatusBg, true);
            ui::text(c, 6, ry + 3, r.header, Accent, font::Face::Small);
            continue;
        }
        const bool selected = first_ + i == sel_;
        if (selected) c.rect(2, ry, ui::ScreenW - 8, rowH, Select, true);
        const std::uint16_t ink = selected ? SelectInk : Ink;
        std::string label = entryName(r.code);
        if (r.recent >= 0 && static_cast<std::size_t>(r.recent) < rec.size()) {
            const auto& e = rec[static_cast<std::size_t>(r.recent)];
            label += ": " + e.value;
        }
        const bool star = std::find(fav.begin(), fav.end(), r.code) != fav.end();
        ui::textFit(c, 10, ry + 1, (star ? "* " : "") + label, ui::ScreenW - 24, ink);
    }
    ui::scrollBar(c, ui::ScreenW - 4, y, visible * rowH, n, visible, first_);
    const char* k[6] = {"APRI", "PREF", nullptr, nullptr, nullptr, "EXIT"};
    ui::softKeys(c, k);
}

void ConversionApp::drawConvert(Context& ctx, gfx::Canvas& c) {
    int y = ui::ContentTop + 2;
    c.rect(0, y, ui::ScreenW, 22, SelectSoft, true);
    ui::textFit(c, 6, y + 3, entryName(entry_), ui::ScreenW - 100, Ink);
    ui::textRight(c, ui::ScreenW - 6, y + 6, units::groupName(entryGroup(entry_)), Muted, font::Face::Small);
    y += 26;
    const char* labels[4] = {"Valore", "Da", "A", nullptr};
    std::string param;
    if (hasParam()) {
        labels[3] = kindOf(entry_) == Format ? unitfmt::converter(indexOf(entry_)).paramName
                  : units::category(indexOf(entry_)).param == units::Param::Dpi ? "DPI (px)" : "M g/mol";
        param = paramText(ctx);
    }
    const int fields = hasParam() ? 4 : 3;
    for (int f = 0; f < fields; ++f) {
        const int fy = y + f * 22;
        const bool sel = f == field_;
        if (sel) c.rect(2, fy, ui::ScreenW - 4, 21, editing_ ? Highlight : Select, true);
        const std::uint16_t ink = sel && !editing_ ? SelectInk : Ink;
        ui::text(c, 8, fy + 3, labels[f], sel && !editing_ ? SelectInk : Muted, font::Face::Small);
        if (sel && editing_) { editor_.draw(c, 92, fy + 2, ui::ScreenW - 100, Ink); continue; }
        std::string v;
        if (f == 0) v = value_.empty() ? "(scrivi il valore)" : value_;
        else if (f == 3) v = param;
        else {
            const std::size_t o = f == 1 ? from_ : to_;
            v = (f == 1 ? fromName() : toName());
            if (kindOf(entry_) == Quantity) v += std::string("  ") + units::unit(firstUnit(entry_) + o).label;
            else if (kindOf(entry_) == Money) v += std::string("  ") + kCurrencies[o].name;
            v = "\xE2\x97\x80 " + v;
        }
        ui::textFit(c, 92, fy + 2, v, ui::ScreenW - 100, ink);
    }
    y += fields * 22 + 6;
    // Risultato
    ui::panel(c, {4, y, ui::ScreenW - 8, 64}, result_.empty() ? StatusBg : SelectSoft, result_.empty() ? Faint : Accent);
    if (result_.empty()) ui::text(c, 12, y + 8, "EXE o F1: converti", Muted);
    else {
        std::string lines[2];
        const std::string text = "= " + result_ + (kindOf(entry_) == Format ? "" : " " + toName());
        const std::size_t nl = ui::wrapText(text, 37, lines, 2);
        for (std::size_t i = 0; i < nl; ++i) ui::textFit(c, 12, y + 6 + static_cast<int>(i) * 17, lines[i], ui::ScreenW - 24, Ink);
        if (!exact_.empty()) ui::textFit(c, 12, y + 42, "esatto: " + exact_, ui::ScreenW - 24, Accent, font::Face::Small);
        else ui::text(c, 12, y + 44, kindOf(entry_) == Format ? "F5 COPY: negli appunti" : "Risultato in Ans", Muted, font::Face::Small);
        if (kindOf(entry_) == Format && indexOf(entry_) == unitfmt::colorConverter()) {
            unitfmt::Options o;
            std::uint16_t rgb = 0;
            if (unitfmt::colorOf(indexOf(entry_), from_, value_, o, rgb)) {
                c.rect(ui::ScreenW - 56, y + 8, 44, 48, rgb, true);
                c.rect(ui::ScreenW - 56, y + 8, 44, 48, Ink, false);
            }
        }
    }
    y += 70;
    // Note: costanti, formula, fonte del fattore, approssimazione.
    std::string info[4];
    std::size_t ni = 0;
    if (kindOf(entry_) == Quantity) {
        const units::Category& cat = units::category(indexOf(entry_));
        if (cat.note) info[ni++] = cat.note;
        for (const units::Unit* u : {&units::unit(firstUnit(entry_) + from_), &units::unit(firstUnit(entry_) + to_)}) {
            if (ni >= 4) break;
            std::string s = std::string("[") + u->name + "] " + units::sourceName(u->source());
            if (u->kind() == units::Kind::Function) s += ": " + std::string(units::functionFormula(*u));
            if (u->has(units::flag::Approx)) s = "\xE2\x89\x88 APPROSSIMATO " + s;
            if (ni == 0 || info[ni - 1] != s) info[ni++] = s;
        }
    } else if (kindOf(entry_) == Format) {
        if (const char* note = unitfmt::converter(indexOf(entry_)).note) info[ni++] = note;
    } else info[ni++] = "Tassi inseriti dall'utente (F4 TASSI): unita' per 1 EUR";
    int iy = y;
    for (std::size_t i = 0; i < ni && iy < ui::ContentBottom - 14; ++i) {
        std::string lines[2];
        const std::size_t nl = ui::wrapText(info[i], 51, lines, 2);
        for (std::size_t k = 0; k < nl && iy < ui::ContentBottom - 14; ++k, iy += 13) ui::text(c, 8, iy, lines[k].c_str(), Muted, font::Face::Small);
    }
    const char* f4 = kindOf(entry_) == Quantity ? "PREFIX" : kindOf(entry_) == Money ? "TASSI" : nullptr;
    const char* k[6] = {"CONVERT", "SWAP\xE2\x87\x84", "ALL", f4, "COPY", "EXIT"};
    ui::softKeys(c, k);
}

void ConversionApp::drawPick(Context& ctx, gfx::Canvas& c) {
    (void)ctx;
    int y = ui::ContentTop + 2;
    c.rect(0, y, ui::ScreenW, 20, SelectSoft, true);
    const std::string title = std::string(screen_ == Screen::Prefix ? "Prefisso" : pickTo_ ? "A" : "Da") + (query_.empty() ? "" : "  cerca: " + query_ + "_");
    ui::textFit(c, 6, y + 2, title, ui::ScreenW - 12, Ink);
    y += 22;
    const int rowH = 18, visible = (ui::ContentBottom - 2 - y) / rowH, n = static_cast<int>(pick_.size());
    if (sel_ < first_) first_ = sel_;
    if (sel_ >= first_ + visible) first_ = sel_ - visible + 1;
    for (int i = 0; i < visible && first_ + i < n; ++i) {
        const std::uint16_t v = pick_[static_cast<std::size_t>(first_ + i)];
        const int ry = y + i * rowH;
        const bool selected = first_ + i == sel_;
        if (selected) c.rect(2, ry, ui::ScreenW - 8, rowH, Select, true);
        const std::uint16_t ink = selected ? SelectInk : Ink;
        std::string left, right;
        if (screen_ == Screen::Prefix) {
            if (v == 0xFFFF) left = "(nessuno)";
            else {
                const units::Prefix& p = units::prefix(v);
                char b[24]; std::snprintf(b, sizeof b, "%s%d", p.iec ? "2^" : "1E", p.exp);
                left = p.sym; right = std::string(p.name) + "  " + b;
            }
        } else if (kindOf(entry_) == Format) left = optionName(v, false);
        else {
            left = optionName(v, false);
            right = kindOf(entry_) == Quantity ? optionName(v, true).substr(left.size() + 2) : kCurrencies[v].name;
        }
        ui::textFit(c, 8, ry + 1, left, 104, selected ? SelectInk : Accent);
        ui::textFit(c, 116, ry + 3, right, ui::ScreenW - 124, ink, font::Face::Small);
    }
    ui::scrollBar(c, ui::ScreenW - 4, y, visible * rowH, n, visible, first_);
    const char* k[6] = {"SCEGLI", nullptr, nullptr, nullptr, nullptr, "EXIT"};
    ui::softKeys(c, k);
}

void ConversionApp::drawRates(Context& ctx, gfx::Canvas& c) {
    int y = ui::ContentTop + 2;
    c.rect(0, y, ui::ScreenW, 20, SelectSoft, true);
    ui::text(c, 6, y + 2, "Tassi: unita' di valuta per 1 EUR", Ink);
    y += 22;
    const auto& rates = ctx.ws.settings.conversion.rates;
    const int rowH = 16, n = static_cast<int>(Settings::Conversion::Currencies);
    for (int i = 0; i < n; ++i) {
        const int ry = y + i * rowH;
        const bool selected = i == sel_;
        if (selected) c.rect(2, ry, ui::ScreenW - 4, rowH, editing_ ? Highlight : Select, true);
        const std::uint16_t ink = selected && !editing_ ? SelectInk : Ink;
        ui::text(c, 8, ry + 2, kCurrencies[i].code, ink, font::Face::Small);
        ui::text(c, 40, ry + 2, kCurrencies[i].name, selected && !editing_ ? SelectInk : Muted, font::Face::Small);
        if (selected && editing_) { editor_.draw(c, 170, ry, ui::ScreenW - 176, Ink); continue; }
        const std::string v = i == 0 ? "1" : rates[static_cast<std::size_t>(i)] > 0 ? fmt(ctx, rates[static_cast<std::size_t>(i)]) : "-";
        ui::textRight(c, ui::ScreenW - 8, ry + 2, v.c_str(), ink, font::Face::Small);
    }
    ui::text(c, 8, ui::ContentBottom - 14, "EXE: modifica   DEL: azzera   nessuna rete richiesta", Muted, font::Face::Small);
    const char* k[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, "EXIT"};
    ui::softKeys(c, k);
}
} // namespace fx::app
