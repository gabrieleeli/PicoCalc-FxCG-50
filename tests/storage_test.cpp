// Flash record store (A/B slots, CRC, power-loss safety) and workspace serialisation.
#include "storage.hpp"
#include "workspace.hpp"
#include "test_util.hpp"
#include <cstring>

using storage::RamFlash;
using storage::RecordStore;

static std::vector<std::uint8_t> blob(std::size_t n, std::uint8_t seed) {
    std::vector<std::uint8_t> v(n);
    for (std::size_t i = 0; i < n; ++i) v[i] = static_cast<std::uint8_t>(seed + i * 7);
    return v;
}

int main() {
    CHECK(storage::crc32("123456789", 9) == 0xCBF43926u);   // standard CRC-32 check value
    {
        RamFlash f(64 * 1024);
        RecordStore s(f);
        std::vector<std::uint8_t> out;
        CHECK(!s.load(out));                                 // blank flash: nothing stored
        CHECK(s.capacity() == 32 * 1024 - 256);
        const auto a = blob(1000, 1), b = blob(5000, 2), c = blob(10, 3);
        CHECK(s.save(a)); CHECK(s.load(out) && out == a);
        CHECK(s.save(b)); CHECK(s.load(out) && out == b);
        CHECK(s.activeSlot() == 1);
        CHECK(s.save(c)); CHECK(s.load(out) && out == c && s.activeSlot() == 0);
        RecordStore fresh(f);                                 // "reboot"
        CHECK(fresh.load(out) && out == c && fresh.sequence() == 3);
        CHECK(!s.save(blob(s.capacity() + 1, 0)));           // too large is refused
        CHECK(s.save(std::vector<std::uint8_t>{}) && s.load(out) && out.empty());
    }
    // Power loss at every possible point of a save never loses the previous record.
    {
        const auto oldData = blob(3000, 9), newData = blob(7000, 4);
        int interrupted = 0;
        for (std::size_t budget = 0; budget < 20000; budget += 97) {
            RamFlash f(64 * 1024);
            { RecordStore s(f); CHECK(s.save(oldData)); }
            f.failAfter(budget);
            { RecordStore s(f); std::vector<std::uint8_t> tmp; (void)s.load(tmp); if (!s.save(newData)) ++interrupted; }
            f.powerOn();
            RecordStore after(f);
            std::vector<std::uint8_t> out;
            CHECK(after.load(out));
            CHECK(out == oldData || out == newData);
        }
        CHECK(interrupted > 0);
    }
    // Corruption of the newest record falls back to the older one.
    {
        RamFlash f(64 * 1024);
        RecordStore s(f);
        CHECK(s.save(blob(100, 1)));
        CHECK(s.save(blob(100, 2)));
        f.raw()[32 * 1024 + 256 + 10] ^= 0x01;               // flip a payload bit in slot 1
        RecordStore r(f); std::vector<std::uint8_t> out;
        CHECK(r.load(out) && out == blob(100, 1));
    }
    // Workspace round trip.
    {
        fx::Workspace ws;
        ws.settings.angle = fx::AngleMode::Radians;
        ws.settings.display = {fx::DisplayMode::Fix, 3};
        ws.settings.complex = fx::ComplexMode::Rectangular;
        ws.settings.wifiSsid = "rete"; ws.settings.relayHost = "relay.example.org"; ws.settings.relayToken = "tok";
        ws.memory.store('A', 12.5); ws.memory.store('Z', -1e-9); ws.memory.setAnswer(42);
        ws.history.push_back({"1+1", {2, 0}, {}, 0});
        ws.history.push_back({"sqrt(-4)", {0, 2}, {}, 0});
        ws.functions[1] = {"x^2-1", fx::GraphType::YGreater, false, 3};
        ws.view.xmin = -10; ws.view.xmax = 10;
        ws.lists[0] = {1, 2, 3.5}; ws.lists[5] = {9};
        ws.equation.unknowns = 3; ws.equation.simultaneous[4] = 7; ws.equation.degree = 4; ws.equation.polynomial[0] = 1;
        ws.chat.push_back({true, "ciao"}); ws.chat.push_back({false, "risposta"});
        const auto bytes = fx::serializeWorkspace(ws);
        fx::Workspace back;
        CHECK(fx::deserializeWorkspace(bytes.data(), bytes.size(), back));
        CHECK(back.settings.angle == fx::AngleMode::Radians && back.settings.display.digits == 3);
        CHECK(back.settings.complex == fx::ComplexMode::Rectangular && back.settings.relayHost == "relay.example.org");
        CHECK(back.memory.recall('A') == 12.5 && back.memory.recall('Z') == -1e-9 && back.memory.answer() == 42);
        CHECK(back.history.size() == 2 && back.history[1].value.imag() == 2);
        CHECK(back.functions[1].expr == "x^2-1" && back.functions[1].type == fx::GraphType::YGreater && !back.functions[1].selected);
        CHECK(back.view.xmin == -10 && back.lists[0].size() == 3 && back.lists[0][2] == 3.5 && back.lists[5][0] == 9);
        CHECK(back.equation.unknowns == 3 && back.equation.simultaneous[4] == 7 && back.equation.degree == 4);
        CHECK(back.chat.size() == 2 && back.chat[1].text == "risposta");
        // Truncated or corrupted data is rejected and leaves the target untouched.
        fx::Workspace target; target.memory.store('B', 5);
        for (std::size_t cut = 0; cut < bytes.size(); cut += 13) CHECK(!fx::deserializeWorkspace(bytes.data(), cut, target));
        auto bad = bytes; bad[0] ^= 0xFF;
        CHECK(!fx::deserializeWorkspace(bad.data(), bad.size(), target));
        CHECK(target.memory.recall('B') == 5);
    }
    return testutil::finish("storage_test");
}
