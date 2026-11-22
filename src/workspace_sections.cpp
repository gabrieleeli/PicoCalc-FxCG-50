// Sezioni TLV del record del workspace (v1.15.0, record v11): l'elenco delle
// codifiche compilate in questa build. Ogni modalita' realizzata dopo la
// v1.15.0 aggiunge qui la propria sezione (id section::ModeBase + AppId),
// solo sulle schede che hanno la modalita' (FXAI_MODE_<NOME>): sulle altre la
// sezione resta sconosciuta e viene conservata intatta da serializeWorkspace.
#include "workspace.hpp"
#include "features.hpp"

namespace fx {
const SectionCodec* workspaceSectionCodecs(std::size_t& count) noexcept {
    // v1.15.0: nessuna modalita' nuova e' ancora disponibile, quindi nessuna sezione.
    count = 0;
    return nullptr;
}
} // namespace fx
