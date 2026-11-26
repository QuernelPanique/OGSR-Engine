////////////////////////////////////////////////////////////////////////////
//	Module 		: id_generator.h
//	Created 	: 28.08.2003
//  Modified 	: 28.08.2003
//	Author		: Dmitriy Iassenev and Oles' Shyshkovtsov
//	Description : ID generation class template
////////////////////////////////////////////////////////////////////////////

#pragma once

template <typename TIME_ID, typename TYPE_ID, typename VALUE_ID, typename BLOCK_ID, typename CHUNK_ID, VALUE_ID tMinValue, VALUE_ID tMaxValue, CHUNK_ID tBlockSize,
          VALUE_ID tInvalidValueID = tMaxValue, TIME_ID tStartTime = 0>
class CID_Generator
{
private:
    struct SID_Block
    {
        CHUNK_ID m_tCount;
        TIME_ID m_tTimeID;
        TYPE_ID m_tpIDs[tBlockSize];

        IC SID_Block() : m_tCount(0) {}

        IC bool operator<(const SID_Block& b) const { return (m_tCount && ((m_tTimeID < b.m_tTimeID) || !b.m_tCount)); }
    };

private:
    enum
    {
        m_tBlockCount = u32(tMaxValue - tMinValue) / tBlockSize + 1,
    };

private:
    u32 m_available_count;
    SID_Block m_tppBlocks[m_tBlockCount];

private:
    IC BLOCK_ID tfGetBlockByValue(VALUE_ID tValueID)
    {
        BLOCK_ID l_tBlockID = BLOCK_ID((tValueID - tMinValue) / tBlockSize);
        R_ASSERT2(l_tBlockID < m_tBlockCount, "Requesting ID is invalid!");
        return (l_tBlockID);
    }

    IC VALUE_ID tfGetFromBlock(SID_Block& l_tID_Block, VALUE_ID tValueID)
    {
        VERIFY(l_tID_Block.m_tCount);
        BLOCK_ID l_tBlockID = BLOCK_ID(&l_tID_Block - m_tppBlocks);

        if (l_tID_Block.m_tCount == 1)
        {
            --m_available_count;
            VERIFY(m_available_count >= 0);
        }

        if (tInvalidValueID == tValueID)
            return (VALUE_ID(l_tID_Block.m_tpIDs[--l_tID_Block.m_tCount]) + l_tBlockID * tBlockSize + tMinValue);

        TYPE_ID* l_tpBlockID = std::find(l_tID_Block.m_tpIDs, l_tID_Block.m_tpIDs + l_tID_Block.m_tCount, TYPE_ID((tValueID - tMinValue) % tBlockSize));
        R_ASSERT2(l_tID_Block.m_tpIDs + l_tID_Block.m_tCount != l_tpBlockID, "Requesting ID has already been used!");
        *l_tpBlockID = *(l_tID_Block.m_tpIDs + --l_tID_Block.m_tCount);
        return (tValueID);
    }

public:
    IC CID_Generator()
    {
        m_available_count = 0;
        for (VALUE_ID i = tMinValue;; ++i)
        {
            vfFreeID(i, tStartTime);
            if (i >= tMaxValue)
                break;
        }
        VERIFY(m_available_count == m_tBlockCount);
        for (u32 j = 0; j < m_tBlockCount; ++j)
            std::reverse(m_tppBlocks[j].m_tpIDs, m_tppBlocks[j].m_tpIDs + m_tppBlocks[j].m_tCount);
    }

    IC VALUE_ID tfGetID(VALUE_ID tValueID = tInvalidValueID)
    {
        if (tInvalidValueID != tValueID)
            return (tfGetFromBlock(m_tppBlocks[tfGetBlockByValue(tValueID)], tValueID));

        R_ASSERT2(m_available_count, "Not enough IDs");
        SID_Block* I = std::min_element(m_tppBlocks, m_tppBlocks + m_tBlockCount);
        VERIFY(I != m_tppBlocks + m_tBlockCount);
        return (tfGetFromBlock(*I, tValueID));
    }

    IC void vfFreeID(VALUE_ID tValueID, TIME_ID tTimeID)
    {
        BLOCK_ID l_tBlockID = tfGetBlockByValue(tValueID);
        SID_Block& l_tID_Block = m_tppBlocks[l_tBlockID];

        VERIFY(l_tID_Block.m_tCount < tBlockSize);

        if (!l_tID_Block.m_tCount)
        {
            ++m_available_count;
            VERIFY(m_available_count <= m_tBlockCount);
        }

#ifdef DEBUG
        TYPE_ID* l_tpBlockID = std::find(l_tID_Block.m_tpIDs, l_tID_Block.m_tpIDs + l_tID_Block.m_tCount, TYPE_ID((tValueID - tMinValue) % tBlockSize));
        VERIFY(l_tpBlockID == l_tID_Block.m_tpIDs + l_tID_Block.m_tCount);
#endif
        l_tID_Block.m_tpIDs[l_tID_Block.m_tCount++] = TYPE_ID((tValueID - tMinValue) % tBlockSize);
        l_tID_Block.m_tTimeID = tTimeID;
    }
};

// OP22 (id32): генератор 32-битных ID объектов. Как старый CID_Generator: сначала свежие ID по возрастанию,
// освобождённые - повторно только когда свежие кончились (старейшие первыми). 0xffff («нет объекта») и 0xfffe
// не выдаются никогда. Предел - OP22_ID_LIMIT (16,7 млн; таблица объектов клиента растёт по наибольшему ID).
// op22_id_test_start: тест - свежие ID с этого номера (70000 - все новые объекты сразу выше старого предела).
extern int g_op22_id_test_start; // консоль op22_id_test_start (xrServer.cpp), 0 - выключено
extern bool g_op22_ids_wide; // op22_id32_io.h

class CID_Generator32
{
public:
    static constexpr u32 OP22_ID_LIMIT = 0x00FFFFFE;
    static constexpr u32 invalid_id = 0xffff;

private:
    xr_vector<u8> m_used;
    xr_deque<u32> m_freed;
    u32 m_next_fresh{};
    u32 m_used_count{};
    u32 m_max_used{}; // наибольший выданный ID (для перебора объектов в скриптах)

    // младшая половина 0xfffe/0xffff не выдаётся: в 16-битных полях ОП (STATE, Lua) она означает «нет объекта»
    static bool reserved(u32 id) { return (id & 0xffff) >= 0xfffe; }

    void mark(u32 id)
    {
        if (id >= m_used.size())
            m_used.resize(std::max<size_t>(id + 1, m_used.size() * 3 / 2 + 4096), 0);
        R_ASSERT2(!m_used[id], "Requesting ID has already been used!");
        m_used[id] = 1;
        ++m_used_count;
        if (id > m_max_used)
            m_max_used = id;
        if (id >= 0xfffe)
            g_op22_ids_wide = true; // с этого момента client_data и реестры сейва пишутся с 4-байтными ID
    }

public:
    CID_Generator32()
    {
        m_next_fresh = u32(std::max(0, g_op22_id_test_start));
        g_op22_ids_wide = false; // новая игра / загрузка: снова по фактическим ID
    }

    u32 tfGetID(u32 id = invalid_id)
    {
        if (id != invalid_id)
        {
            R_ASSERT2(id < OP22_ID_LIMIT && !reserved(id), "Requesting ID is invalid!");
            mark(id);
            return id;
        }
        while (m_next_fresh < OP22_ID_LIMIT)
        {
            const u32 n = m_next_fresh++;
            if (!reserved(n) && (n >= m_used.size() || !m_used[n]))
            {
                mark(n);
                return n;
            }
        }
        while (!m_freed.empty())
        {
            const u32 n = m_freed.front();
            m_freed.pop_front();
            if (!m_used[n])
            {
                mark(n);
                return n;
            }
        }
        R_ASSERT2(false, "Not enough IDs");
        return invalid_id;
    }

    void vfFreeID(u32 id, u32 /*time*/)
    {
        if (id >= m_used.size() || !m_used[id]) // старый генератор это молча терпел (VERIFY только в Debug)
        {
            Msg("! [OP22 id32] free of unused id %u", id);
            return;
        }
        m_used[id] = 0;
        --m_used_count;
        if (id < m_next_fresh)
            m_freed.push_back(id);
    }

    u32 used_count() const { return m_used_count; }
    u32 max_used() const { return m_max_used; }
    u32 next_fresh() const { return m_next_fresh; }
};
