#pragma once

#include "InfoPortionDefs.h"
#include <array>
#include <atomic>
#include <cstdint>

// Only actor-info reads inside one synchronous select_task() are memoized.
// Successful info mutations advance the epoch before script actions run.
inline std::atomic<std::uint64_t> g_op22_info_epoch{};

inline void op22_info_changed()
{
    g_op22_info_epoch.fetch_add(1, std::memory_order_release);
}

class Op22ALifeInfoScope
{
    struct Entry
    {
        shared_str key; // Own missing IDs too: interned addresses can be reused.
        std::uint64_t generation{};
        bool value{};
    };
    std::array<Entry, 128> entries_{};
    Op22ALifeInfoScope* previous_;
    const KNOWN_INFO_VECTOR* source_{};
    const void* data_{};
    std::size_t size_{};
    std::uint64_t epoch_{}, generation_{1};
    unsigned queries_{}, hits_{}, misses_{}, resets_{};
    inline static thread_local Op22ALifeInfoScope* current_{};

public:
    Op22ALifeInfoScope() : previous_(current_) { current_ = this; }
    ~Op22ALifeInfoScope() { current_ = previous_; }
    Op22ALifeInfoScope(const Op22ALifeInfoScope&) = delete;
    Op22ALifeInfoScope& operator=(const Op22ALifeInfoScope&) = delete;

    template <typename Fallback>
    static bool lookup(const KNOWN_INFO_VECTOR* source, const shared_str& key, Fallback fallback)
    {
        auto* scope = current_;
        if (!scope || !source)
            return fallback();
        ++scope->queries_;
        const auto epoch = g_op22_info_epoch.load(std::memory_order_acquire);
        if (scope->source_ != source || scope->data_ != source->data() ||
            scope->size_ != source->size() || scope->epoch_ != epoch)
        {
            scope->source_ = source;
            scope->data_ = source->data();
            scope->size_ = source->size();
            scope->epoch_ = epoch;
            ++scope->generation_;
            ++scope->resets_;
        }

        // Direct-mapped and bounded; a collision is just an uncached lookup.
        auto hash = reinterpret_cast<std::uintptr_t>(key.c_str());
        hash ^= hash >> 17;
        hash ^= hash >> 9;
        auto& entry = scope->entries_[(hash >> 4) & 127];
        if (entry.generation == scope->generation_ && entry.key == key)
        {
            ++scope->hits_;
            return entry.value;
        }
        ++scope->misses_;
        const bool value = fallback();
        entry.key = key;
        entry.value = value;
        entry.generation = scope->generation_;
        return value;
    }

    void report(unsigned id, bool sampled) const
    {
        if (sampled)
            Msg("[op22_alife_info] id=%u queries=%u hits=%u misses=%u resets=%u info_count=%u scope=one_search",
                id, queries_, hits_, misses_, resets_, unsigned(size_));
    }
};
