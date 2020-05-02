#ifndef EV3PLOGGER_SCHEDULER_H
#define EV3PLOGGER_SCHEDULER_H

#include <chrono>
#include <functional>
#include <optional>
#include <thread>
#include <type_safe/strong_typedef.hpp>
#include <vector>

namespace ev3plotter {

struct priority
    : type_safe::strong_typedef<priority, int>
    , type_safe::strong_typedef_op::equality_comparison<priority>
    , type_safe::strong_typedef_op::relational_comparison<priority> {
    using strong_typedef::strong_typedef;
};

class Scheduler {
  public:
    using clock = std::chrono::steady_clock;
    Scheduler(std::function<void()> afterStepCallback = {}) : afterStepCallback_{std::move(afterStepCallback)} {

    }

    template <typename TFunc> void schedule(priority p, clock::duration after_this_time, TFunc&& f) {
        updates_.push_back(task
            {p, after_this_time == clock::duration::zero() ? clock::time_point{} : clock::now() + after_this_time, std::forward<TFunc>(f)});
    }

    template <typename TFunc> void schedule(priority p, TFunc&& f) {
        schedule(p, clock::duration::zero(), std::forward<TFunc>(f));
    }

    template <typename TFunc> void schedule(TFunc&& f) { schedule(priority{0}, clock::duration::zero(), std::forward<TFunc>(f)); }
    template <typename TFunc> void schedule(clock::duration after_this_time, TFunc&& f) {
        schedule({}, after_this_time, std::forward<TFunc>(f));
    }

    void run() {
        while (refresh()) {
            if (schedule_[0].when == clock::time_point{} || schedule_[0].when < clock::now()) {
                schedule_[0].callback();
            } else {
                std::this_thread::sleep_until(schedule_[0].when);
                schedule_[0].callback();
            }

            if (afterStepCallback_) {
                afterStepCallback_();
            }

            schedule_.erase(schedule_.begin());
        }

        schedule_.clear();
    }

  private:
    struct task {
        std::optional<priority> priority_;
        clock::time_point when;
        std::function<void()> callback;
    };

    struct compare_tasks {
        constexpr bool operator()(const task& a, const task& b) const noexcept {
            return a.when == b.when
                ? a.priority_ < b.priority_
                : a.when < b.when;
        }
    };

    bool refresh() {
        std::stable_sort(updates_.begin(), updates_.end(), compare_tasks{});
        const auto schedule_prev_size = schedule_.size();
        schedule_.insert(
            schedule_.end(),
            std::make_move_iterator(updates_.begin()),
            std::make_move_iterator(updates_.end()));
        updates_.clear();
        std::inplace_merge(schedule_.begin(), schedule_.begin() + static_cast<std::vector<task>::difference_type>(schedule_prev_size), schedule_.end(), compare_tasks{});

        return ! schedule_.empty();
    }

    std::function<void()> afterStepCallback_;
    std::vector<task> schedule_;
    std::vector<task> updates_;
};
} // namespace ev3plotter

#endif // EV3PLOGGER_SCHEDULER_H