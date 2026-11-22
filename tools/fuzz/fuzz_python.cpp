// v1.17.0 — Fuzzing della modalita' Python: sorgenti qualsiasi eseguiti da
// MicroPython (porta embed) nella coroutine della sessione, a passi con un
// budget e poi interrotti (KeyboardInterrupt). Cerca crash nella colla
// (cambio di pila, I/O, input(), casioplot): MicroPython stesso e' codice di terzi.
#include "mem_pools.hpp"
#include "python_port.hpp"
#include <string>
using namespace fx;
namespace {
struct NullCanvas final : py::Canvas {
    void setPixel(int, int, std::uint8_t, std::uint8_t, std::uint8_t) override {}
    bool getPixel(int, int, std::uint8_t&, std::uint8_t&, std::uint8_t&) override { return false; }
    void drawString(int, int, const std::string&, std::uint8_t, std::uint8_t, std::uint8_t, bool) override {}
    void clear() override {}
    void show() override {}
};
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* d, size_t n) {
    static NullCanvas canvas;
    static std::uint32_t now = 0;
    static py::Session* s = nullptr;
    if (!s) s = new py::Session([] { return now; }, &canvas);
    const std::string src(reinterpret_cast<const char*>(d), n);
    if (src.find('\0') != std::string::npos) return 0;
    s->clearConsole();
    if (n && (d[0] & 1)) s->runLine(src); else s->runScript(src);
    for (int k = 0; k < 40 && s->running(); ++k) {
        if (s->waitingInput()) s->provideInput("1");
        step::Budget b(20000, 0, nullptr);
        (void)s->step(b);
        now += 30;
        if (k == 30) s->interrupt();
    }
    for (int k = 0; k < 200 && s->running(); ++k) { s->interrupt(); step::Budget b(20000, 0, nullptr); (void)s->step(b); }
    return 0;
}
