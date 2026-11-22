// Lavoro a passi interrompibile con AC (v1.15.0, F6): vedi step_task.hpp.
#include "step_task.hpp"
#include <limits>

namespace fx::step {

Budget::Budget(std::size_t units, std::uint32_t sliceMs, const Clock* clock) noexcept
    : limit_(units), slice_(sliceMs), clock_(clock && *clock ? clock : nullptr) {
    if (clock_ && slice_ > 0) start_ = (*clock_)();
}

bool Budget::consume(std::size_t units) noexcept {
    used_ += units;
    return !exhausted();
}

bool Budget::exhausted() const noexcept {
    if (limit_ > 0 && used_ >= limit_) return true;
    if (timeUp_) return true;
    if (clock_ && slice_ > 0) {
        // Differenza senza segno: corretta anche quando l'orologio a 32 bit si azzera.
        try {
            timeUp_ = static_cast<std::uint32_t>((*clock_)() - start_) >= slice_;
        } catch (...) {
            timeUp_ = true;
        }
    }
    return timeUp_;
}

std::size_t Budget::remaining() const noexcept {
    if (limit_ == 0) return std::numeric_limits<std::size_t>::max();
    return used_ >= limit_ ? 0 : limit_ - used_;
}

void Runner::start(Task& task, Config config) {
    task_ = &task;
    config_ = config;
    state_ = State::Running;
    steps_ = 0;
}

bool Runner::tick(const Clock& clock) {
    if (state_ != State::Running || !task_) return false;
    Budget budget(config_.unitsPerStep, config_.sliceMs, &clock);
    try {
        ++steps_;
        if (task_->step(budget)) state_ = State::Done;
    } catch (...) {
        state_ = State::Failed;
        throw;
    }
    return true;
}

bool Runner::interrupt() {
    if (state_ != State::Running) return false;
    state_ = State::Break;
    if (task_) task_->cancelled();
    return true;
}

int Runner::percent() const {
    if (!task_) return -1;
    const std::size_t total = task_->total();
    if (total == 0) return -1;
    const std::size_t done = task_->done();
    if (done >= total) return 100;
    return static_cast<int>(done * 100 / total);
}

} // namespace fx::step
