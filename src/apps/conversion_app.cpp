// CONVERSION: metric conversions (length, area, volume, time, speed, mass,
// pressure, energy, power, temperature) using point_a::convert.
#include "apps.hpp"
#include "point_a.hpp"

namespace fx::app {
using namespace ui::theme;
using fx::pointa::Unit;

namespace {
struct Category { const char* name; std::vector<std::pair<const char*, Unit>> units; };
const std::vector<Category>& categories() {
    static const std::vector<Category> c = {
        {"Lunghezza", {{"m", Unit::Meter}, {"km", Unit::Kilometer}, {"cm", Unit::Centimeter}, {"mm", Unit::Millimeter}, {"in", Unit::Inch}, {"ft", Unit::Foot}, {"mile", Unit::Mile}}},
        {"Area", {{"m\xC2\xB2", Unit::SquareMeter}, {"cm\xC2\xB2", Unit::SquareCentimeter}, {"ha", Unit::Hectare}, {"acre", Unit::Acre}}},
        {"Volume", {{"m\xC2\xB3", Unit::CubicMeter}, {"L", Unit::Liter}, {"mL", Unit::Milliliter}, {"gal(US)", Unit::GallonUS}}},
        {"Tempo", {{"s", Unit::Second}, {"min", Unit::Minute}, {"h", Unit::Hour}, {"day", Unit::Day}}},
        {"Velocita'", {{"m/s", Unit::MeterPerSecond}, {"km/h", Unit::KilometerPerHour}, {"mile/h", Unit::MilePerHour}}},
        {"Massa", {{"kg", Unit::Kilogram}, {"g", Unit::Gram}, {"mg", Unit::Milligram}, {"lb", Unit::Pound}}},
        {"Pressione", {{"Pa", Unit::Pascal}, {"kPa", Unit::Kilopascal}, {"bar", Unit::Bar}, {"atm", Unit::Atmosphere}}},
        {"Energia", {{"J", Unit::Joule}, {"kJ", Unit::Kilojoule}, {"cal", Unit::Calorie}, {"kcal", Unit::Kilocalorie}}},
        {"Potenza", {{"W", Unit::Watt}, {"kW", Unit::Kilowatt}, {"hp", Unit::Horsepower}}},
        {"Temperatura", {{"\xC2\xB0" "C", Unit::Celsius}, {"\xC2\xB0" "F", Unit::Fahrenheit}, {"K", Unit::Kelvin}}},
    };
    return c;
}
std::vector<std::string> names(const Category& c) { std::vector<std::string> v; for (const auto& u : c.units) v.emplace_back(u.first); return v; }
} // namespace

void ConversionApp::enter(Context&) {
    std::vector<std::string> cats; for (const auto& c : categories()) cats.emplace_back(c.name);
    form_.fields = {{"Grandezza", cats[0], FormField::Type::Choice, cats, 0}, {"Da", "", FormField::Type::Choice, {}, 0}, {"A", "", FormField::Type::Choice, {}, 1}, {"Valore", "1", FormField::Type::Number, {}, 0}};
    rebuildUnits();
    form_.selected = 3; result_.clear();
}
void ConversionApp::rebuildUnits() {
    const auto& cat = categories()[static_cast<std::size_t>(form_.fields[0].choice)];
    for (int f = 1; f <= 2; ++f) {
        auto& fld = form_.fields[static_cast<std::size_t>(f)];
        fld.choices = names(cat);
        fld.choice = std::min(fld.choice, static_cast<int>(fld.choices.size()) - 1);
        fld.text = fld.choices[static_cast<std::size_t>(fld.choice)];
    }
}
void ConversionApp::key(Context& ctx, const KeyEvent& ev) {
    if (!form_.editing() && (ev.is(Key::Exit))) { ctx.home(); return; }
    if (!form_.editing() && (ev.is(Key::F1) || ev.is(Key::F6))) {
        const auto& cat = categories()[static_cast<std::size_t>(form_.fields[0].choice)];
        const double v = form_.number(ctx, 3);
        const Unit a = cat.units[static_cast<std::size_t>(form_.fields[1].choice)].second, b = cat.units[static_cast<std::size_t>(form_.fields[2].choice)].second;
        const double r = pointa::convert(v, a, b);
        ctx.ws.memory.setAnswer(r);
        result_ = fmt(ctx, v) + " " + form_.fields[1].text + " = " + fmt(ctx, r) + " " + form_.fields[2].text;
        return;
    }
    const int before = form_.fields[0].choice;
    (void)form_.key(ctx, ev);
    if (form_.fields[0].choice != before) { form_.fields[1].choice = 0; form_.fields[2].choice = 1; rebuildUnits(); result_.clear(); }
}
void ConversionApp::draw(Context& ctx, gfx::Canvas& c) {
    ui::clear(c);
    ctx.drawStatus(c, "CONVERSION");
    const int top = ui::ContentTop + 2;
    form_.draw(c, top + 4, top + 4 + 4 * 20);
    if (!result_.empty()) {
        ui::panel(c, {6, top + 100, ui::ScreenW - 12, 40}, SelectSoft, Accent);
        ui::textFit(c, 14, top + 112, result_, ui::ScreenW - 28, Ink);
        ui::text(c, 14, top + 146, "Risultato copiato in Ans", Muted, font::Face::Small);
    }
    ui::text(c, 8, ui::ContentBottom - 40, "\xE2\x97\x80\xE2\x96\xB6 cambia unita'   EXE modifica valore", Muted, font::Face::Small);
    const char* k[6] = {"CONVERT", nullptr, nullptr, nullptr, nullptr, "CONVERT"};
    ui::softKeys(c, k);
}
} // namespace fx::app
