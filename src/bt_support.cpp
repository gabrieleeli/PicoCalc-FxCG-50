#include "bt_support.hpp"

#include "board_config.hpp"

#if FXAI_ON_DEVICE && FXAI_ENABLE_BT
#include "btstack.h"
#endif

namespace fx::bt {
#if FXAI_ON_DEVICE && FXAI_ENABLE_BT
namespace {
bool g_started = false;
}
bool init() noexcept {
    if (g_started) return true;
    l2cap_init();
    sm_init();
    g_started = hci_power_control(HCI_POWER_ON) == 0;
    return g_started;
}
bool enabled() noexcept { return true; }
const char* state() noexcept {
    if (!g_started) return "non avviato";
    switch (hci_get_state()) {
    case HCI_STATE_WORKING: return "attivo";
    case HCI_STATE_INITIALIZING: return "inizializzazione";
    case HCI_STATE_OFF: return "spento";
    default: return "transizione";
    }
}
#else
bool init() noexcept { return false; }
bool enabled() noexcept { return false; }
const char* state() noexcept { return "disabilitato (FXAI_ENABLE_BT=OFF)"; }
#endif
} // namespace fx::bt
