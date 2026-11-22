// Sezioni TLV del record del workspace (v1.15.0, record v11): l'elenco delle
// codifiche compilate in questa build. Ogni modalita' realizzata dopo la
// v1.15.0 aggiunge qui la propria sezione (id section::ModeBase + AppId),
// solo sulle schede che hanno la modalita' (FXAI_MODE_<NOME>): sulle altre la
// sezione resta sconosciuta e viene conservata intatta da serializeWorkspace.
#include "workspace.hpp"
#include "app_registry.hpp"
#include "features.hpp"

namespace fx {
namespace {
// ---- v1.16.0: eActivity (sezione 0x0102)
// u16 cartella, u16 ultimo file, u32 + memorie salvate, u32 + striscia salvata.
constexpr std::uint16_t EActivityId = section::ModeBase + static_cast<std::uint16_t>(app::AppId::EActivity);
void put16(std::vector<std::uint8_t>& o, std::size_t v) { o.push_back(static_cast<std::uint8_t>(v)); o.push_back(static_cast<std::uint8_t>(v >> 8)); }
void put32(std::vector<std::uint8_t>& o, std::size_t v) { put16(o, v & 0xFFFF); put16(o, (v >> 16) & 0xFFFF); }
bool eactPresent(const Workspace& ws) { return !ws.eactivity.empty(); }
void eactEncode(const Workspace& ws, std::vector<std::uint8_t>& out) {
    const EActivityState& e = ws.eactivity;
    put16(out, e.folder.size()); out.insert(out.end(), e.folder.begin(), e.folder.end());
    put16(out, e.lastFile.size()); out.insert(out.end(), e.lastFile.begin(), e.lastFile.end());
    put32(out, e.savedMemories.size()); out.insert(out.end(), e.savedMemories.begin(), e.savedMemories.end());
    put32(out, e.savedStrip.size()); out.insert(out.end(), e.savedStrip.begin(), e.savedStrip.end());
}
bool eactDecode(Workspace& ws, std::uint16_t, const std::uint8_t* d, std::size_t n) {
    std::size_t i = 0;
    auto take = [&](std::size_t k, std::vector<std::uint8_t>* v, std::string* s) {
        if (k > n - i) return false;
        if (v) v->assign(d + i, d + i + k);
        if (s) s->assign(reinterpret_cast<const char*>(d + i), k);
        i += k;
        return true;
    };
    auto len = [&](std::size_t bytes, std::size_t& out) {
        if (bytes > n - i) return false;
        out = 0;
        for (std::size_t k = 0; k < bytes; ++k) out |= static_cast<std::size_t>(d[i + k]) << (8 * k);
        i += bytes;
        return true;
    };
    EActivityState e;
    std::size_t k = 0;
    if (!len(2, k) || k > 64 || !take(k, nullptr, &e.folder)) return false;
    if (!len(2, k) || k > 64 || !take(k, nullptr, &e.lastFile)) return false;
    if (!len(4, k) || !take(k, &e.savedMemories, nullptr)) return false;
    if (!len(4, k) || !take(k, &e.savedStrip, nullptr)) return false;
    if (i != n) return false;
    // Un documento o una striscia erano aperti quando il record e' stato
    // salvato: il workspace contiene i loro dati; si rimettono quelli del
    // menu principale (prima la striscia, poi le memorie del documento).
    // Dati non validi qui non fanno perdere il resto del record.
    if (!e.savedStrip.empty()) (void)decodeParts(ws, e.savedStrip.data(), e.savedStrip.size());
    if (!e.savedMemories.empty()) (void)decodeParts(ws, e.savedMemories.data(), e.savedMemories.size());
    e.savedStrip.clear();
    e.savedMemories.clear();
    ws.eactivity = std::move(e);
    return true;
}
#if FXAI_BOARD_PICO_PLUS_2W
// ---- v1.17.0 (solo Pimoroni Pico Plus 2 W): le sezioni delle modalita' Base.
// Ognuna e' la codifica dello stato del nucleo della modalita' (versione 1);
// vuota (= valori iniziali) non viene scritta. La stessa codifica serve le
// strisce di eActivity (parti ModeBase + AppId) e i file "a stato" (.pcdyna...).
constexpr std::uint16_t modeId(app::AppId a) { return static_cast<std::uint16_t>(section::ModeBase + static_cast<std::uint16_t>(a)); }
#define FXAI_MODE_CODEC(NS, MEMBER, APP)                                                                                              \
    {modeId(app::AppId::APP), 1, [](const Workspace& ws) { NS::State d; NS::reset(d); return !(ws.MEMBER == d); },                  \
     [](const Workspace& ws, std::vector<std::uint8_t>& out) { NS::encode(ws.MEMBER, out); },                                        \
     [](Workspace& ws, std::uint16_t, const std::uint8_t* d, std::size_t n) { return NS::decode(ws.MEMBER, d, n); },                 \
     [](Workspace& ws) { NS::reset(ws.MEMBER); }}
const SectionCodec kCodecs[] = {
#if FXAI_MODE_EACTIVITY
    {EActivityId, 1, eactPresent, eactEncode, eactDecode},
#endif
#if FXAI_MODE_SPREADSHEET
    FXAI_MODE_CODEC(sheet, sheet, Spreadsheet),
#endif
#if FXAI_MODE_DYNAGRAPH
    FXAI_MODE_CODEC(dyna, dyna, DynaGraph),
#endif
#if FXAI_MODE_RECURSION
    FXAI_MODE_CODEC(rec, recursion, Recursion),
#endif
#if FXAI_MODE_CONICGRAPHS
    FXAI_MODE_CODEC(conic, conic, ConicGraphs),
#endif
#if FXAI_MODE_PROGRAM
    FXAI_MODE_CODEC(progmode, program, Program),
#endif
#if FXAI_MODE_FINANCIAL
    FXAI_MODE_CODEC(fin, financial, Financial),
#endif
#if FXAI_MODE_LINK
    FXAI_MODE_CODEC(link, link, Link),
#endif
#if FXAI_MODE_GEOMETRY
    FXAI_MODE_CODEC(geo, geometry, Geometry),
#endif
#if FXAI_MODE_PICTUREPLOT
    FXAI_MODE_CODEC(pplot, pictplot, PicturePlot),
#endif
#if FXAI_MODE_PYTHON
    FXAI_MODE_CODEC(py, python, Python),
#endif
    {0, 0, nullptr, nullptr, nullptr},   // segnaposto (non contato)
};
} // namespace

const SectionCodec* workspaceSectionCodecs(std::size_t& count) noexcept {
    // v1.16.0: eActivity (su entrambe le schede, se la modalita' e' compilata);
    // v1.17.0: le modalita' Base della Plus. Gli id sono in ordine crescente.
    count = sizeof kCodecs / sizeof kCodecs[0] - 1;
    return count ? kCodecs : nullptr;
}
#else   // Pico 2 W congelato: la tabella della v1.16.0
const SectionCodec kCodecs[] = {
    {EActivityId, 1, eactPresent, eactEncode, eactDecode},
};
} // namespace

const SectionCodec* workspaceSectionCodecs(std::size_t& count) noexcept {
    // v1.16.0: eActivity (su entrambe le schede, se la modalita' e' compilata).
    count = features::kEActivity ? sizeof kCodecs / sizeof kCodecs[0] : 0;
    return count ? kCodecs : nullptr;
}
#endif
#if FXAI_BOARD_PICO_PLUS_2W
const char* sectionModeName(std::uint16_t id) noexcept {
    if (id < section::ModeBase || id >= section::ModeBase + app::AppCount) return "?";
    return app::appInfo(static_cast<app::AppId>(id - section::ModeBase)).name;
}
#endif
} // namespace fx
