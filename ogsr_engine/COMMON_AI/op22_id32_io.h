#pragma once
// OP22 (id32): чтение/запись ID объектов в сохраняемых данных. Запись - всегда 4 байта; чтение - 2 или 4 байта по
// признаку формата: STATE/UPDATE серверных объектов - g_op22_state_id32 (ставит CSE_Abstract::Spawn_Read по флагу
// M_SPAWN_ID32 объекта; по умолчанию true), client_data - g_op22_cd_id32, реестры сейва - g_op22_alife_id32.
// «Нет объекта» в обоих форматах - 0xffff.
extern bool g_op22_state_id32;

template <class S>
inline u32 op22_read_id(S& s, bool id32)
{
    return id32 ? u32(s.r_u32()) : u32(s.r_u16());
}

template <class S>
inline void op22_write_id(S& s, u32 id)
{
    s.w_u32(id);
}

// вектор ID в раскладке save_data/load_data: u32 count, затем элементы
template <class S, class V>
inline void op22_read_ids(S& s, V& v, bool id32)
{
    v.clear();
    const u32 n = s.r_u32();
    v.reserve(n);
    for (u32 i = 0; i < n; ++i)
        v.push_back(op22_read_id(s, id32));
}

template <class S, class V>
inline void op22_write_ids(S& s, const V& v)
{
    s.w_u32(u32(v.size()));
    for (const auto id : v)
        s.w_u32(u32(id));
}

template <class S>
inline u32 op22_r_state_id(S& s) { return op22_read_id(s, g_op22_state_id32); }
