#pragma once

#include <chrono>

extern int g_op22_frame_stats;

// Copy identity before update(): a callback may unregister/delete its object.
// No object pointer is retained or read after the callback returns.
template <typename Clock = std::chrono::steady_clock>
class Op22ALifeDetail
{
    using Tick = typename Clock::time_point;
    bool enabled_;
    const char* kind_;
    unsigned id_{};
    shared_str name_, section_;
    Tick started_{};

public:
    Op22ALifeDetail(const char* kind, const CSE_Abstract* object, bool enabled)
        : enabled_(enabled && object), kind_(kind)
    {
        if (enabled_)
        {
            id_ = object->ID;
            const char* name = object->name_replace();
            name_ = name ? name : "";
            section_ = object->s_name;
            started_ = Clock::now();
        }
    }

    float elapsed() const
    {
        return enabled_ ? std::chrono::duration<float, std::milli>(Clock::now() - started_).count() : 0.f;
    }

    void finish(float select = -1.f, float task = -1.f, float move = -1.f) const
    {
        if (!enabled_)
            return;
        const auto now = Clock::now();
        const float total = std::chrono::duration<float, std::milli>(now - started_).count();
        if (total < 2.f)
            return;

        // A worker may change between frames; keep the limiter thread-local.
        static thread_local Tick window{};
        static thread_local unsigned emitted{};
        if (window == Tick{} || now - window >= std::chrono::seconds(1))
        {
            window = now;
            emitted = 0;
        }
        if (emitted++ >= 8)
            return;
        Msg("[op22_alife_object] kind=%s id=%u name=%s section=%s total=%.2f select=%.2f task=%.2f move=%.2f",
            kind_, id_, name_.c_str() ? name_.c_str() : "", section_.c_str() ? section_.c_str() : "", total, select, task, move);
    }
};
