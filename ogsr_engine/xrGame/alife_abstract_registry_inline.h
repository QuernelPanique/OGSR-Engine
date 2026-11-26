////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_abstract_registry_inline.h
//	Created 	: 30.06.2004
//  Modified 	: 30.06.2004
//	Author		: Dmitriy Iassenev
//	Description : ALife abstract registry inline functions
////////////////////////////////////////////////////////////////////////////

#pragma once
#include "alife_space.h" // OP22 (id32): ALife::_OBJECT_ID

#include "op22_id32.h"

#define TEMPLATE_SPECIALIZATION template <typename _index_type, typename _data_type>
#define CSALifeAbstractRegistry CALifeAbstractRegistry<_index_type, _data_type>

TEMPLATE_SPECIALIZATION
IC CSALifeAbstractRegistry::CALifeAbstractRegistry() {}

TEMPLATE_SPECIALIZATION
CSALifeAbstractRegistry::~CALifeAbstractRegistry() { delete_data(m_objects); }

// OP22 (id32): реестры по ID объекта (ALife::_OBJECT_ID = u32): ключ 2 байта (сейв 10) или 4 (сейв 11, есть ID >= 0xfffe);
// после чтения сейва 10 - поправка значений (op22_fix_old_ids). Раскладка как у общего save_data(map): u32 count, {ключ, значение}.
TEMPLATE_SPECIALIZATION
void CSALifeAbstractRegistry::save(IWriter& memory_stream)
{
    if constexpr (std::is_same_v<_index_type, ALife::_OBJECT_ID>)
    {
        memory_stream.w_u32((u32)m_objects.size());
        for (auto& it : m_objects)
        {
            op22_w_reg_id(memory_stream, it.first); // 2 или 4 байта - по формату сейва
            save_data(it.second, memory_stream);
        }
    }
    else
        save_data(m_objects, memory_stream);
}

TEMPLATE_SPECIALIZATION
void CSALifeAbstractRegistry ::load(IReader& file_stream)
{
    if constexpr (std::is_same_v<_index_type, ALife::_OBJECT_ID>)
    {
        m_objects.clear();
        const u32 count = file_stream.r_u32();
        for (u32 i = 0; i < count; ++i)
        {
            const ALife::_OBJECT_ID key = op22_r_reg_id(file_stream);
            _data_type value{};
            load_data(value, file_stream);
            if (!g_op22_alife_id32)
                op22_fix_old_ids(value);
            m_objects.insert(std::make_pair(key, value));
        }
    }
    else
        load_data(m_objects, file_stream);
}

TEMPLATE_SPECIALIZATION
IC const typename CSALifeAbstractRegistry::OBJECT_REGISTRY& CSALifeAbstractRegistry::objects() const { return (m_objects); }

TEMPLATE_SPECIALIZATION
IC typename CSALifeAbstractRegistry::OBJECT_REGISTRY& CSALifeAbstractRegistry::objects() { return m_objects; }

TEMPLATE_SPECIALIZATION
IC void CSALifeAbstractRegistry::add(const _index_type& index, _data_type& data, bool no_assert)
{
    const_iterator I = objects().find(index);
    if (I != objects().end())
    {
        THROW2(no_assert, "Specified object has been already found in the specified registry!");
        return;
    }
    m_objects.insert(std::make_pair(index, data));
}

TEMPLATE_SPECIALIZATION
IC void CSALifeAbstractRegistry::remove(const _index_type& index, bool no_assert)
{
    iterator I = m_objects.find(index);
    if (I == objects().end())
    {
        THROW2(no_assert, "Specified object hasn't been found in the specified registry!");
        return;
    }
    m_objects.erase(I);
}

TEMPLATE_SPECIALIZATION
IC _data_type* CSALifeAbstractRegistry::object(const _index_type& index, bool no_assert)
{
    iterator I = m_objects.find(index);
    if (I == objects().end())
    {
        THROW2(no_assert, "Specified object hasn't been found in the specified registry!");
        return (0);
    }
    return (&(*I).second);
}

#undef TEMPLATE_SPECIALIZATION