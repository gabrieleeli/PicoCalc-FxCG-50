// v1.15.0 (F6): lavoro a passi interrompibile con AC, con orologio simulato:
// budget di unita' e di tempo, AC -> "Break", avanzamento, errori che lasciano
// il runner in Failed, e il calcolo progressivo del 3D Graph sul runner.
#include "app_rig.hpp"
#include "step_task.hpp"
#include "test_util.hpp"
#include <stdexcept>

using namespace fx;

namespace {
// Conta fino a `target`: ogni unita' fa avanzare l'orologio simulato di `cost` ms.
struct Counter final : step::Task {
    std::size_t target, value{0};
    std::uint32_t* clock;
    std::uint32_t cost;
    bool cancelledFlag{false};
    int throwAt{-1};
    Counter(std::size_t t, std::uint32_t* c, std::uint32_t ms) : target(t), clock(c), cost(ms) {}
    bool step(step::Budget& b) override {
        while (value < target) {
            if (static_cast<int>(value) == throwAt) throw std::runtime_error("Math ERROR: prova");
            ++value;
            *clock += cost;
            if (!b.consume()) break;
        }
        return value >= target;
    }
    std::size_t total() const override { return target; }
    std::size_t done() const override { return value; }
    void cancelled() override { cancelledFlag = true; }
};
} // namespace

int main() {
    std::uint32_t now = 1000;
    const step::Clock clock = [&now] { return now; };
    // ---------------------------------------------------------------- budget di unita'
    {
        Counter c(1000, &now, 0);
        step::Runner r;
        r.start(c, step::Runner::Config{100, 0});
        CHECK(r.running() && r.percent() == 0);
        CHECK(r.tick(clock) && c.value == 100 && r.percent() == 10);
        for (int i = 0; i < 8; ++i) r.tick(clock);
        CHECK(c.value == 900 && r.running());
        r.tick(clock);
        CHECK(c.value == 1000 && r.state() == step::State::Done && r.steps() == 10 && r.percent() == 100);
        CHECK(!r.tick(clock));                                  // finito: nulla da fare
    }
    // ---------------------------------------------------------------- budget di tempo (orologio simulato)
    {
        Counter c(1000, &now, 3);                               // 3 ms a unita'
        step::Runner r;
        r.start(c, step::Runner::Config{0, 20});                // 20 ms a passo
        r.tick(clock);
        CHECK(c.value == 7);                                    // 7 * 3 = 21 ms >= 20
        r.tick(clock);
        CHECK(c.value == 14);
        // l'orologio a 32 bit che ricomincia da zero non blocca il passo
        now = 0xFFFFFFF0u;
        r.tick(clock);
        CHECK(c.value == 21);
        // budget combinati: il primo limite raggiunto ferma il passo
        Counter d(1000, &now, 1);
        r.start(d, step::Runner::Config{5, 20});
        r.tick(clock);
        CHECK(d.value == 5);
        step::Budget b(0, 0, nullptr);                          // nessun limite
        CHECK(b.remaining() == static_cast<std::size_t>(-1) && b.consume(1000000) && !b.exhausted());
    }
    // ---------------------------------------------------------------- AC: Break
    {
        Counter c(1000, &now, 0);
        step::Runner r;
        r.start(c, step::Runner::Config{50, 0});
        r.tick(clock);
        CHECK(r.interrupt() && r.state() == step::State::Break && c.cancelledFlag);
        CHECK(!r.interrupt() && !r.tick(clock) && c.value == 50);   // fermo: i dati parziali restano
        CHECK(std::string(step::Runner::BreakMessage) == "Break");
        r.reset();
        CHECK(r.state() == step::State::Idle && r.percent() == -1);
    }
    // ---------------------------------------------------------------- errori: Failed e l'eccezione al chiamante
    {
        Counter c(1000, &now, 0);
        c.throwAt = 120;
        step::Runner r;
        r.start(c, step::Runner::Config{100, 0});
        r.tick(clock);
        bool thrown = false;
        try { r.tick(clock); } catch (const std::runtime_error& e) { thrown = std::string(e.what()) == "Math ERROR: prova"; }
        CHECK(thrown && r.state() == step::State::Failed && !r.running());
    }
    // ---------------------------------------------------------------- 3D Graph sul runner (F6.3)
    {
        board::init();
        testrig::Rig rig;
        auto& g3 = rig.ws().graph3d;
        for (std::size_t i = 0; i < g3d::Functions; ++i) {             // tre grafici: piu' passi da 600 punti
            g3.set.fns[i].kind = g3d::Kind::Z; g3.set.fns[i].expr[0] = "x^2-y^2+" + std::to_string(i); g3.set.fns[i].selected = true;
        }
        g3.set.window.xgrid = g3.set.window.ygrid = 50;
        rig.fw.apps().open(app::AppId::Graph3D);
        auto* app = rig.active<app::Graph3DApp>();
        CHECK(app != nullptr);
        rig.key(pckey::F6);                                     // DRAW: calcolo progressivo, 600 punti a passo
        CHECK(app->sampling());                                 // non finito dopo due passi
        rig.settle();
        CHECK(!app->sampling() && !rig.error());
        // AC durante il calcolo: interrotto, grafico parziale
        rig.key(pckey::Esc);
        g3.set.fns[0].expr[0] = "x^2-y^2+1";
        rig.key(pckey::F6);
        if (app->sampling()) {
            rig.key(pckey::Break);
            CHECK(!app->sampling() && !rig.error());
        }
    }
    return testutil::finish("step_task_test");
}
