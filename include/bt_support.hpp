#pragma once
// Optional Bluetooth LE (FXAI_ENABLE_BT, default OFF). Never required to build:
// with the flag off these functions are stubs and pico_btstack is not linked.
namespace fx::bt {
bool init() noexcept;          // powers the HCI on after cyw43_arch_init()
bool enabled() noexcept;       // compiled in
const char* state() noexcept;  // human-readable HCI state
}
