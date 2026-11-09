#include "firmware.hpp"
#include "apps.hpp"
#include "expression.hpp"
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
}
Firmware::~Firmware() = default;

void Firmware::boot() {
    std::vector<std::uint8_t> blob;
    if (store_.load(blob)) {
        loaded_ = deserializeWorkspace(blob.data(), blob.size(), ws_);
    } else if (legacy_) {
        // No record yet: import the workspace of the older firmware (its store
        // is only read; the next autosave writes the record to the new store).
        storage::RecordStore old(*legacy_);
        if (old.load(blob) && deserializeWorkspace(blob.data(), blob.size(), ws_)) loaded_ = migrated_ = true;
    }
    seedRandom(platform_.millis() * 2654435761u + 1u);
    platform_.setBacklight(ws_.settings.lcdBacklight, ws_.settings.kbdBacklight);
    manager_.start();
    if (migrated_) {
        manager_.context().markDirty();
        manager_.context().toast("Dati della v1.8.0 importati", 3000);
    }
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
    if (ctx.dirty() && (appChanged || platform_.millis() - lastInput_ > AutosaveDelayMs)) {
        // The autosave runs outside the AppManager: no exception may leave it
        // (an escaped std::bad_alloc would terminate the firmware).
        char msg[112];
        bool saved = false;
        try {
            saved = saveNow();
            if (!saved) {
                // A record larger than the slot can never be saved: say why and how to fix it.
                const std::size_t bytes = serializeWorkspace(ws_).size(), room = store_.capacity();
                if (bytes > room)
                    std::snprintf(msg, sizeof(msg), "Memoria flash piena: %u KB > %u KB (cancella memorie grafiche, SKETCH, chat)",
                                  static_cast<unsigned>((bytes + 1023) / 1024), static_cast<unsigned>(room / 1024));
                else
                    std::snprintf(msg, sizeof(msg), "Errore salvataggio flash");
            }
        } catch (const std::bad_alloc&) {
            std::snprintf(msg, sizeof(msg), "Memory ERROR: salvataggio non riuscito (memoria insufficiente)");
        }
        if (!saved) {
            ctx.clearDirty();
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
