#pragma once

#include <algorithm>
#include <chrono>

// OP22 (28.11 perf): замеры участков кадра из главного и рабочих потоков.
enum class Op22HitchStage { gc, alife, destroy, spawn, tex, present_wait, count };

class Op22HitchTimer
{
    using Clock = std::chrono::steady_clock;
    const Op22HitchStage stage;
    const unsigned frame_tag;
    // OP22 (28.11 perf): имя ресурса действительно до выхода из области замера.
    const char* const texture_name;
    Clock::time_point started{};

public:
    explicit Op22HitchTimer(Op22HitchStage stage, bool enabled = true, const char* texture_name = nullptr);
    ~Op22HitchTimer();
    Op22HitchTimer(const Op22HitchTimer&) = delete;
    Op22HitchTimer& operator=(const Op22HitchTimer&) = delete;
};

extern int g_op22_hitch_log;
extern int g_op22_hitch_ms;

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
        float parallel_job_ms{};
        unsigned long long parallel_job_rva{};
        unsigned parallel_job_calls{}, parallel_jobs{};
        int gc_budget{}, precache{};
    } sample;

    // OP22 (28.11 perf): подробная статистика и журнал рывков включаются независимо.
    const bool detailed;
    const bool enabled;
    const Tick started;

    Op22FrameProfile(bool detailed, bool hitch) : detailed(detailed), enabled(detailed || hitch), started(now()) {}

    Tick now(bool measure = true) const { return enabled && measure ? Clock::now() : Tick{}; }
    float since(Tick from) const { return from != Tick{} ? milliseconds(Clock::now() - from) : 0.f; }

    static float milliseconds(Clock::duration time)
    {
        return std::chrono::duration<float, std::milli>(time).count();
    }

    void finish()
    {
        if (!detailed)
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

        Msg("[op22_stage] n=%u worst work=%.2f gap=%.2f update=%.2f camera=%.2f begin=%.2f draw=%.2f present=%.2f wait=%.2f; async sound=%.2f parallel=%.2f physics=%.2f gc=%.2f budget_us=%d; turn=%.1f precache=%d; job_rva=%llx job_ms=%.2f job_calls=%u jobs=%u",
            count, worst.total, worst.gap, worst.update, worst.camera, worst.begin, worst.draw, worst.present, worst.wait,
            worst.sound, worst.parallel, worst.physics, worst.gc, worst.gc_budget, worst.turn, worst.precache,
            worst.parallel_job_rva, worst.parallel_job_ms, worst.parallel_job_calls, worst.parallel_jobs);
        window = current;
        worst = {};
        count = 0;
    }
};
