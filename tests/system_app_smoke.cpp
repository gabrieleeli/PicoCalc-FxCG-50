// Controller flows added in v1.9.0: System -> Memoria / Limiti / Test,
// diagnostics execution, Graph Y= list (multiple simultaneous functions),
// 3D graph controls, memory report formatting.
#include "board_config.hpp"
#include "diagnostics.hpp"
#include "limits.hpp"
#include "mem_pools.hpp"
#include "run_matrix.hpp"
#include "system_info.hpp"
#include "test_support.hpp"

#include <cstring>

namespace {
void press(fx::RunMatrixController& c, fx::InputAction a, char ch = 0) { c.handle({a, ch}); }
void select(fx::RunMatrixController& c, unsigned index) {
    while (c.selection() != index) {
        const unsigned row = c.selection() / 4, target = index / 4;
        if (row < target) press(c, fx::InputAction::Down);
        else if (row > target) press(c, fx::InputAction::Up);
        else press(c, fx::InputAction::Right);
    }
}
} // namespace

int main() {
    mem::testing::simulateSram(256u * 1024u);
    mem::testing::simulatePsram(board::config::kHasPsram ? (8u << 20) : 0u);
    {
        fx::RunMatrixController c;
        // --- SYSTEM tile (G, index 15) ---------------------------------------
        select(c, 15);
        FX_CHECK(std::strcmp(c.selectedAppName(), "SYSTEM") == 0);
        press(c, fx::InputAction::Evaluate);
        FX_CHECK(c.screen() == fx::AppScreen::System && c.systemSelection() == 0);
        press(c, fx::InputAction::Evaluate);
        FX_CHECK(c.screen() == fx::AppScreen::Memory && c.memoryPage() == 0);
        press(c, fx::InputAction::Right);
        FX_CHECK(c.memoryPage() == 1);
        press(c, fx::InputAction::Escape);               // back to the parent, not the launcher
        FX_CHECK(c.screen() == fx::AppScreen::System);
        press(c, fx::InputAction::Down);
        press(c, fx::InputAction::Down);
        press(c, fx::InputAction::Evaluate);
        FX_CHECK(c.screen() == fx::AppScreen::Diagnostics && !c.lastDiagnostic());
        for (int i = 0; i < 2; ++i) press(c, fx::InputAction::Down); // AllocatorStress
        FX_CHECK(c.diagnosticSelection() == static_cast<unsigned>(fx::diag::Test::AllocatorStress));
        press(c, fx::InputAction::Evaluate);
        FX_CHECK(c.diagnosticPending());                 // runs after a "running" frame
        c.runPendingDiagnostic();
        FX_CHECK(!c.diagnosticPending() && c.lastDiagnostic() && c.lastDiagnostic()->passed);
        FX_CHECK(c.lastDiagnostic()->lineCount >= 2);
        press(c, fx::InputAction::Escape);
        FX_CHECK(c.screen() == fx::AppScreen::System);
        press(c, fx::InputAction::Escape);
        FX_CHECK(c.screen() == fx::AppScreen::Home && c.selection() == 15);

        // --- GRAPH: Y= list with several simultaneous functions --------------------
        select(c, 4);
        press(c, fx::InputAction::Evaluate);
        FX_CHECK(c.screen() == fx::AppScreen::Graph && c.graphFunctions().size() == 1 && c.graphExpression() == "x^2");
        press(c, fx::InputAction::Evaluate);              // EXE: Y= editor
        FX_CHECK(c.screen() == fx::AppScreen::GraphFunctions);
        press(c, fx::InputAction::Down);                  // "+ new function" row
        for (char ch : std::string("sin(x)")) press(c, fx::InputAction::Character, ch);
        press(c, fx::InputAction::Down);
        for (char ch : std::string("x/3")) press(c, fx::InputAction::Character, ch);
        FX_CHECK(c.graphFunctions().size() == 3 && c.graphFunctions()[1] == "sin(x)" && c.graphFunctions()[2] == "x/3");
        press(c, fx::InputAction::Backspace);
        FX_CHECK(c.graphFunctions()[2] == "x/");
        press(c, fx::InputAction::Delete);                // remove Y3
        FX_CHECK(c.graphFunctions().size() == 2);
        press(c, fx::InputAction::Evaluate);              // draw all
        FX_CHECK(c.screen() == fx::AppScreen::Graph);
        const std::size_t traced = c.graphSelected();      // last row edited in Y=
        press(c, fx::InputAction::Fraction);              // F1: trace the next function
        FX_CHECK(c.graphSelected() == (traced + 1) % c.graphFunctions().size());
        const auto before = c.stamp(fx::AppScreen::Graph);
        press(c, fx::InputAction::Right);
        FX_CHECK(c.stamp(fx::AppScreen::Graph) != before);
        press(c, fx::InputAction::Escape);
        FX_CHECK(c.screen() == fx::AppScreen::Home);

        // The Y= list respects the per-board function cap.
        select(c, 4);
        press(c, fx::InputAction::Evaluate);
        press(c, fx::InputAction::Evaluate);
        const std::size_t cap = fx::limits::graphMaxFunctions();
        for (std::size_t i = 0; i < cap + 3; ++i) {
            while (c.graphSelected() != c.graphFunctions().size() && c.graphFunctions().size() < cap)
                press(c, fx::InputAction::Down);
            press(c, fx::InputAction::Character, 'x');
        }
        FX_CHECK(c.graphFunctions().size() == cap);
        press(c, fx::InputAction::Escape);
        press(c, fx::InputAction::Escape);

        // --- 3D GRAPH tile (J, index 18) ------------------------------------------
        select(c, 18);
        press(c, fx::InputAction::Evaluate);
        FX_CHECK(c.screen() == (FXAI_ENABLE_3D ? fx::AppScreen::Graph3D : fx::AppScreen::Unavailable));
        if (FXAI_ENABLE_3D) {
            const float yaw = c.graph3dView().yawDeg;
            press(c, fx::InputAction::Right);
            FX_CHECK(c.graph3dView().yawDeg != yaw);
            press(c, fx::InputAction::Fraction);
            FX_CHECK(c.graph3dView().zoom > 1.0F);
            press(c, fx::InputAction::Delete);
            for (char ch : std::string("x*y")) press(c, fx::InputAction::Character, ch);
            FX_CHECK(c.graph3dExpression() == "x*y");
            press(c, fx::InputAction::Clear);
            FX_CHECK(c.graph3dView().zoom == 1.0F);
        }
        press(c, fx::InputAction::Escape);
        FX_CHECK(c.screen() == fx::AppScreen::Home);

        // Errors surface as a one-shot toast.
        c.reportOutOfMemory();
        FX_CHECK(c.hasToast() && std::strcmp(c.toast(), fx::limits::kOutOfMemoryMessage) == 0);
        c.clearToast();
        FX_CHECK(!c.hasToast());
    }

    // --- Memory report -----------------------------------------------------------
    const auto report = fx::sysinfo::collect();
    FX_CHECK(report.sram.available && report.psram.available == board::config::kHasPsram);
    char lines[24][fx::sysinfo::kLineChars];
    const std::size_t n = fx::sysinfo::formatMemory(report, lines, 24);
    FX_CHECK(n >= 10);
    bool sawSram = false, sawPsram = false, sawFlash = false, sawStack = false;
    for (std::size_t i = 0; i < n; ++i) {
        sawSram |= std::strncmp(lines[i], "SRAM", 4) == 0;
        sawPsram |= std::strncmp(lines[i], "PSRAM", 5) == 0;
        sawFlash |= std::strncmp(lines[i], "FLASH", 5) == 0;
        sawStack |= std::strncmp(lines[i], "STACK", 5) == 0;
    }
    FX_CHECK(sawSram && sawPsram && sawFlash && sawStack);
    char size[16];
    fx::sysinfo::formatSize(size, sizeof size, 8u * 1024u * 1024u);
    FX_CHECK(std::strcmp(size, "8.0M") == 0);
    fx::sysinfo::formatSize(size, sizeof size, 512u);
    FX_CHECK(std::strcmp(size, "512") == 0);

    // Allocator stress kernel used by the on-device test (both policies).
    const auto st = fx::diag::allocatorStress(1234u, 20000u, 4096u);
    FX_CHECK(st.corruptions == 0 && st.allocations > 1000);
    FX_CHECK(mem::checkIntegrity());
    // Hardware tests report themselves as device-only on the host.
    const auto wifi = fx::diag::run(fx::diag::Test::WiFi);
    FX_CHECK(wifi.ran && !wifi.passed && wifi.lineCount == 1);
    for (std::size_t i = 0; i < fx::diag::kTestCount; ++i)
        FX_CHECK(std::strlen(fx::diag::name(static_cast<fx::diag::Test>(i))) > 3);
    mem::testing::reset();
    std::puts("System menu, diagnostics, multi-graph and 3D controller flows: PASS");
    return 0;
}
