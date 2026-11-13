#pragma once

#include <array>
#include <cstdint>
#include <cstring>

// Read the delegate's code identity without modifying its closure or invoking it.
// MSVC x64 stores a code pointer in this single-inheritance member pointer.
struct Op22DelegateIdentity : fastdelegate::DelegateMemento
{
    explicit Op22DelegateIdentity(const fastdelegate::DelegateMemento& value) : DelegateMemento(value) {}

    std::uintptr_t code() const
    {
        std::uintptr_t result{};
#ifdef _MSC_VER
        static_assert(sizeof(m_pFunction) == sizeof(result));
#endif
        std::memcpy(&result, &m_pFunction, sizeof(result));
        return result;
    }
};

struct Op22ParallelJobs
{
    struct Group
    {
        std::uintptr_t rva{};
        float ms{};
        unsigned calls{};
    };
    std::array<Group, 32> groups{};
    unsigned used{}, calls{};

    void record(std::uintptr_t code, std::uintptr_t module_begin, std::uintptr_t module_end, float ms)
    {
        const auto rva = code >= module_begin && code < module_end ? code - module_begin : 0;
        ++calls;
        for (unsigned i = 0; i < used; ++i)
        {
            if (groups[i].rva == rva)
            {
                groups[i].ms += ms;
                ++groups[i].calls;
                return;
            }
        }
        if (used < groups.size())
            groups[used++] = {rva, ms, 1};
    }

    Group worst() const
    {
        Group result{};
        for (unsigned i = 0; i < used; ++i)
            if (groups[i].ms > result.ms)
                result = groups[i];
        return result;
    }
};
