#pragma once

#include <algorithm>
#include <chrono>

// Wall-clock timings, not GPU timestamp queries. The worker fields are read
// only after all futures for this frame have been joined by on_idle().
struct Op22FrameProfile
{
    using Clock = std::chrono::steady_clock;
    using Tick = Clock::time_point;

    struct Sample
    {
        float total{}, gap{}, update{}, camera{}, begin{}, draw{}, present{}, wait{};
        float sound{}, parallel{}, physics{}, gc{}, turn{};
        int gc_budget{}, precache{};
    } sample;

    const bool enabled;
    const Tick started;

    explicit Op22FrameProfile(bool active) : enabled(active), started(now()) {}

    Tick now() const { return enabled ? Clock::now() : Tick{}; }
    float since(Tick from) const { return enabled ? milliseconds(Clock::now() - from) : 0.f; }

    static float milliseconds(Clock::duration time)
    {
        return std::chrono::duration<float, std::milli>(time).count();
    }

    void finish()
    {
        if (!enabled)
            return;

        const auto current = Clock::now();
        static Tick window{}, previous_end{};
        static Sample worst{};
        static unsigned count{};
        const float idle = previous_end == Tick{} ? 0.f : milliseconds(started - previous_end);
        previous_end = current;
        if (window == Tick{} || idle > 1000.f)
        {
            window = started;
            worst = {};
            count = 0;
        }

        sample.total = milliseconds(current - started);
        sample.gap = std::max(0.f, idle);
        if (sample.total + sample.gap >= worst.total + worst.gap)
            worst = sample;
        ++count;

        if (milliseconds(current - window) < 1000.f)
            return;

        Msg("[op22_stage] n=%u worst work=%.2f gap=%.2f update=%.2f camera=%.2f begin=%.2f draw=%.2f present=%.2f wait=%.2f; async sound=%.2f parallel=%.2f physics=%.2f gc=%.2f budget_us=%d; turn=%.1f precache=%d",
            count, worst.total, worst.gap, worst.update, worst.camera, worst.begin, worst.draw, worst.present, worst.wait,
            worst.sound, worst.parallel, worst.physics, worst.gc, worst.gc_budget, worst.turn, worst.precache);
        window = current;
        worst = {};
        count = 0;
    }
};
