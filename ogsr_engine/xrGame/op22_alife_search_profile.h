#pragma once

#include <chrono>

// Sample 1/64 actual searches, not cooldown-only brain updates. Per-zone
// clocks run only in those samples; logs are limited to four per second.
template <typename Clock = std::chrono::steady_clock>
class Op22ALifeSearchProfile
{
    using Tick = typename Clock::time_point;
    bool sampled_{};
    unsigned id_{}, visited_{}, skipped_{}, enabled_calls_{}, suitable_calls_{};
    unsigned enabled_worst_id_{}, suitable_worst_id_{};
    shared_str name_;
    float enabled_ms_{}, suitable_ms_{}, enabled_worst_ms_{}, suitable_worst_ms_{};
    Tick started_{};

    static float ms(Tick from, Tick to)
    {
        return std::chrono::duration<float, std::milli>(to - from).count();
    }

public:
    Op22ALifeSearchProfile(const CSE_Abstract* object, bool enabled)
    {
        if (!enabled || !object)
            return;
        static thread_local unsigned searches{};
        sampled_ = (searches++ % 64) == 0;
        if (!sampled_)
            return;
        id_ = object->ID;
        const char* name = object->name_replace();
        name_ = name ? name : "";
        started_ = Clock::now();
    }

    Tick begin() const { return sampled_ ? Clock::now() : Tick{}; }
    void visit() { if (sampled_) ++visited_; }
    void skip() { if (sampled_) ++skipped_; }

    void enabled(unsigned id, Tick from)
    {
        if (!sampled_) return;
        ++enabled_calls_;
        const float elapsed = ms(from, Clock::now());
        enabled_ms_ += elapsed;
        if (elapsed > enabled_worst_ms_)
        {
            enabled_worst_ms_ = elapsed;
            enabled_worst_id_ = id;
        }
    }

    void suitable(unsigned id, Tick from)
    {
        if (!sampled_) return;
        ++suitable_calls_;
        const float elapsed = ms(from, Clock::now());
        suitable_ms_ += elapsed;
        if (elapsed > suitable_worst_ms_)
        {
            suitable_worst_ms_ = elapsed;
            suitable_worst_id_ = id;
        }
    }

    bool finish(unsigned selected, Tick before_register) const
    {
        if (!sampled_) return false;
        const auto now = Clock::now();
        static thread_local Tick window{};
        static thread_local unsigned emitted{};
        if (window == Tick{} || now - window >= std::chrono::seconds(1))
        {
            window = now;
            emitted = 0;
        }
        if (emitted++ >= 4) return false;
        Msg("[op22_alife_search] id=%u name=%s visited=%u skipped=%u enabled_calls=%u suitable_calls=%u enabled_ms=%.3f suitable_ms=%.3f enabled_worst=%u:%.3f suitable_worst=%u:%.3f register_ms=%.3f selected=%u total_ms=%.3f sampled=1/64",
            id_, name_.c_str() ? name_.c_str() : "", visited_, skipped_, enabled_calls_, suitable_calls_,
            enabled_ms_, suitable_ms_, enabled_worst_id_, enabled_worst_ms_, suitable_worst_id_, suitable_worst_ms_,
            ms(before_register, now), selected, ms(started_, now));
        return true;
    }
};
