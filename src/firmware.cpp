#include "firmware.hpp"
#include "apps.hpp"
#include "expression.hpp"
#include "features.hpp"
#include <cstdio>
#include <new>

namespace fx {
struct Firmware::Apps {
    explicit Apps(app::AppManager& m) : unavailable(m) {}
    app::LauncherApp launcher;
    app::UnavailableApp unavailable;
    app::RunMatrixApp runMatrix;
    app::SymbolsApp symbols;
    app::EquationApp equation;
    app::StatisticsApp statistics;
    app::GraphApp graph;
    app::TableApp table;
    app::SystemApp system;
    app::MemoryApp memory;
    app::ConversionApp conversion;
    app::AiChatApp aiChat;
    app::Graph3DApp graph3d;
#if FXAI_MODE_EACTIVITY
    app::EActivityApp eactivity;    // v1.16.0, su entrambe le schede
#endif
    // v1.17.0: modalita' Base della Pimoroni Pico Plus 2 W.
#if FXAI_MODE_SPREADSHEET
    app::SpreadsheetApp spreadsheet;
#endif
#if FXAI_MODE_DYNAGRAPH
    app::DynaGraphApp dyna;
#endif
#if FXAI_MODE_RECURSION
    app::RecursionApp recursion;
#endif
#if FXAI_MODE_CONICGRAPHS
    app::ConicGraphsApp conic;
#endif
#if FXAI_MODE_PROGRAM
    app::ProgramApp program;
#endif
#if FXAI_MODE_FINANCIAL
    app::FinancialApp financial;
#endif
#if FXAI_MODE_LINK
    app::LinkApp link;
#endif
#if FXAI_MODE_GEOMETRY
    app::GeometryApp geometry;
#endif
#if FXAI_MODE_PICTUREPLOT
    app::PicturePlotApp pictplot;
#endif
#if FXAI_MODE_PYTHON
    app::PythonApp python;
#endif
};

namespace { constexpr std::uint32_t AutosaveDelayMs = 2500; }

Firmware::Firmware(app::Platform& platform, storage::FlashDevice& flash, storage::FlashDevice* legacy)
    : platform_(platform), store_(flash), legacy_(legacy), manager_(ws_, platform), apps_(std::make_unique<Apps>(manager_)) {
    manager_.setLauncher(apps_->launcher);
    manager_.setUnavailable(apps_->unavailable);
    app::App* list[] = {&apps_->runMatrix, &apps_->symbols, &apps_->equation, &apps_->statistics, &apps_->graph,
                        &apps_->table, &apps_->system, &apps_->memory, &apps_->conversion, &apps_->aiChat,
                        &apps_->graph3d};
    for (app::App* a : list) {
        manager_.add(*a);
        apps_->launcher.setAvailability(static_cast<std::size_t>(a->id()), true);
    }
#if FXAI_MODE_EACTIVITY
    manager_.add(apps_->eactivity);
    apps_->launcher.setAvailability(static_cast<std::size_t>(app::AppId::EActivity), true);
#endif
#if FXAI_BOARD_PICO_PLUS_2W
    app::App* base[] = {
#if FXAI_MODE_SPREADSHEET
        &apps_->spreadsheet,
#endif
#if FXAI_MODE_DYNAGRAPH
        &apps_->dyna,
#endif
#if FXAI_MODE_RECURSION
        &apps_->recursion,
#endif
#if FXAI_MODE_CONICGRAPHS
        &apps_->conic,
#endif
#if FXAI_MODE_PROGRAM
        &apps_->program,
#endif
#if FXAI_MODE_FINANCIAL
        &apps_->financial,
#endif
#if FXAI_MODE_LINK
        &apps_->link,
#endif
#if FXAI_MODE_GEOMETRY
        &apps_->geometry,
#endif
#if FXAI_MODE_PICTUREPLOT
        &apps_->pictplot,
#endif
#if FXAI_MODE_PYTHON
        &apps_->python,
#endif
        nullptr};
    for (app::App* a : base) {
        if (!a) continue;
        manager_.add(*a);
        apps_->launcher.setAvailability(static_cast<std::size_t>(a->id()), true);
    }
#endif
}
Firmware::~Firmware() = default;

void Firmware::boot() {
    try {
        std::vector<std::uint8_t> blob;
        if (store_.load(blob)) {
            loaded_ = deserializeWorkspace(blob.data(), blob.size(), ws_);
        } else if (legacy_) {
            // No record yet: import the workspace of the older firmware (its store
            // is only read; the next autosave writes the record to the new store).
            storage::RecordStore old(*legacy_);
            if (old.load(blob) && deserializeWorkspace(blob.data(), blob.size(), ws_)) loaded_ = migrated_ = true;
        }
    } catch (const std::bad_alloc&) {
        // Record piu' grande della memoria libera (deserializeWorkspace lascia
        // ws_ intatto in caso di errore): si parte vuoti e i salvataggi sono
        // sospesi, cosi' i dati in flash restano leggibili con piu' memoria.
        loaded_ = migrated_ = false;
        storageLocked_ = true;
    }
    seedRandom(platform_.millis() * 2654435761u + 1u);
    platform_.setBacklight(ws_.settings.lcdBacklight, ws_.settings.kbdBacklight);
    manager_.start();
    if (migrated_) {
        manager_.context().markDirty();
        manager_.context().toast("Dati della v1.8.0 importati", 3000);
    }
    if (storageLocked_)
        manager_.showError("Memory ERROR: dati salvati troppo grandi per la memoria libera; salvataggio sospeso per non perderli");
#if FXAI_BOARD_PICO_PLUS_2W
    // v1.17.0 (record v12): sezioni di modalita' danneggiate isolate alla lettura.
    if (!ws_.damagedSections.empty() && !storageLocked_) {
        std::string m = "Data ERROR: dati danneggiati di";
        for (std::uint16_t id : ws_.damagedSections) { m += ' '; m += sectionModeName(id); }
        m += ": ripartono dai valori iniziali";
        manager_.showError(m);
        manager_.context().markDirty();
    }
#endif
    redraw_ = true;
}
void Firmware::notice(const std::string& message) { notice(message.c_str()); }
void Firmware::notice(const char* message) {
    manager_.showError(message);
    redraw_ = true;
}
void Firmware::rawKey(std::uint16_t raw) { key(decoder_.decode(raw)); }
void Firmware::key(const KeyEvent& ev) {
    if (ev.none()) return;
    manager_.key(ev);
    lastInput_ = platform_.millis();
    redraw_ = true;
}
bool Firmware::saveNow() {
    if (storageLocked_) return false;
    const auto blob = serializeWorkspace(ws_);
    if (!store_.save(blob)) return false;
    manager_.context().clearDirty();
    ++saves_;
    return true;
}
bool Firmware::step() {
    if (manager_.tick()) redraw_ = true;
    auto& ctx = manager_.context();
    // Leaving an app or a quiet period after changes triggers an autosave.
    const bool appChanged = manager_.activeId() != lastApp_;
    lastApp_ = manager_.activeId();
    if (storageLocked_ && ctx.dirty()) ctx.clearDirty();     // salvataggi sospesi (vedi boot())
    if (ctx.dirty() && (appChanged || platform_.millis() - lastInput_ > AutosaveDelayMs)) {
        // The autosave runs outside the AppManager: no exception may leave it
        // (an escaped std::bad_alloc would terminate the firmware).
        char msg[112];
        bool saved = false, retry = false;
        try {
            saved = saveNow();
            if (!saved) {
                // A record larger than the slot can never be saved: say why and how to fix it.
                const std::size_t room = store_.capacity();
                const std::size_t bytes = room ? serializeWorkspace(ws_).size() : 0;
                if (room == 0)   // region disabled at boot (flash chip smaller than the build's layout)
                    std::snprintf(msg, sizeof(msg), "Salvataggio non disponibile: area dati della flash disattivata");
                else if (bytes > room)
                    std::snprintf(msg, sizeof(msg), "Memoria flash piena: %u KB > %u KB (cancella memorie grafiche, SKETCH, chat)",
                                  static_cast<unsigned>((bytes + 1023) / 1024), static_cast<unsigned>(room / 1024));
                else
                    std::snprintf(msg, sizeof(msg), "Errore salvataggio flash");
            }
        } catch (const std::bad_alloc&) {
            // Running out of memory is temporary (a mode holding large buffers,
            // a long computation): the changes stay pending and are saved as
            // soon as the memory is back, instead of being lost at power-off.
            std::snprintf(msg, sizeof(msg), "Memory ERROR: salvataggio rimandato (memoria insufficiente)");
            retry = true;
        }
        if (!saved) {
            // A record that does not fit or a failing flash cannot be fixed by
            // trying again (and repeated erases would wear the flash): the next
            // change triggers a new attempt.
            if (retry) lastInput_ = platform_.millis();   // next attempt after AutosaveDelayMs
            else ctx.clearDirty();
            ctx.toast(static_cast<const char*>(msg), 4000);   // allocation-free
            redraw_ = true;
        }
    }
    const bool r = redraw_;
    return r;
}
void Firmware::draw(gfx::Canvas& canvas) {
    manager_.draw(canvas);
    redraw_ = false;
}
} // namespace fx
