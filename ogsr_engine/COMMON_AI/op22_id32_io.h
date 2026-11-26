#pragma once
// OP22 (id32): ID объектов в сохраняемых данных - формат совместим с ОП, пока все ID < 0xfffe.
//  * STATE и заголовок объекта (их разбирает и Lua: m_netpk ОП): поля ID всегда 16 бит на старых местах (младшая
//    половина); полные значения собираются при записи и дописываются хвостом после блока STATE в пакете спавна
//    (CSE_Abstract::Spawn_Write/Spawn_Read), только если есть ID > 0xffff. При чтении STATE без хвоста (правка живого
//    объекта из скрипта) полный ID сохраняется из памяти, если младшая половина совпала.
//  * client_data и реестры сейва: 4 байта только после того, как в игре появился ID >= 0xfffe (g_op22_ids_wide);
//    признак формата - бит M_SPAWN_CD_ID32 объекта и версия сейва 11.
// Генератор не выдаёт ID с младшей половиной 0xfffe/0xffff: «нет объекта» (0xffff) в младшей половине однозначен.
extern bool g_op22_ids_wide; // в игре есть ID >= 0xfffe (ставит генератор)
extern xr_vector<u32>* g_op22_id_wcollect; // сбор полных ID при записи STATE (Spawn_Write)
extern xr_vector<u32*>* g_op22_id_rcollect; // сбор полей ID при чтении STATE (Spawn_Read)

constexpr u32 OP22_ID32_TAIL_MAGIC = 0x32334449; // 'ID32'

template <class S>
inline void op22_w_id16(S& s, u32 id)
{
    s.w_u16(u16(id & 0xffff));
    if (g_op22_id_wcollect)
        g_op22_id_wcollect->push_back(id);
}

template <class S>
inline void op22_r_id16(S& s, u32& id)
{
    const u32 lo = s.r_u16();
    // чтение из пакета спавна (идёт сбор: полный ID придёт хвостом) - всегда младшая половина; правка живого объекта
    // скриптом (без хвоста) - если младшая половина совпала, полный ID остаётся из памяти
    if (g_op22_id_rcollect || (id & 0xffff) != lo)
        id = lo;
    if (g_op22_id_rcollect)
        g_op22_id_rcollect->push_back(&id);
}

// вектор ID в раскладке save_data(xr_vector<u16>): u32 count, затем по 2 байта
template <class S, class V>
inline void op22_w_ids16(S& s, const V& v)
{
    s.w_u32(u32(v.size()));
    for (const auto id : v)
        op22_w_id16(s, u32(id));
}

template <class S, class V>
inline void op22_r_ids16(S& s, V& v)
{
    const u32 n = s.r_u32();
    v.assign(n, u32(0xffff));
    for (u32 i = 0; i < n; ++i)
        v[i] = s.r_u16();
    if (g_op22_id_rcollect)
        for (u32 i = 0; i < n; ++i)
            g_op22_id_rcollect->push_back(&v[i]);
}

// client_data и реестры: 2 или 4 байта по признаку формата
template <class S>
inline void op22_w_id_fmt(S& s, u32 id, bool wide)
{
    if (wide)
        s.w_u32(id);
    else
        s.w_u16(u16(id & 0xffff));
}

template <class S>
inline u32 op22_r_id_fmt(S& s, bool wide)
{
    return wide ? u32(s.r_u32()) : u32(s.r_u16());
}
