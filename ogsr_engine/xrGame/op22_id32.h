#pragma once
#include "op22_id32_io.h"
// OP22 (id32): чтение ID объекта из client_data (сохранённые данные клиентских объектов: память NPC, инвентарь, ...).
// Старые сейвы - 2 байта, client_data нового движка - 4 байта (флаг M_SPAWN_CD_ID32 объекта). Флаг текущего чтения
// ставит CGameObject::net_Spawn на время net_Load. Запись - всегда NET_Packet::w_id (4 байта).
extern bool g_op22_cd_id32;

inline u32 op22_r_cd_id(IReader& r) { return g_op22_cd_id32 ? r.r_u32() : u32(r.r_u16()); }

// OP22 (id32): реестры ALife в сейве (инфопоршни, отношения, контакты, задания, метки, ...): ID 4 байта с версии сейва
// 11. Чтение: ставит CALifeSimulatorHeader::load по версии; запись: CALifeSimulatorHeader::save - 11 и 4 байта
// только если в игре есть ID >= 0xfffe, иначе версия 10 и сейв побайтно как раньше.
extern bool g_op22_alife_id32;

inline u32 op22_r_reg_id(IReader& r) { return g_op22_alife_id32 ? r.r_u32() : u32(r.r_u16()); }
inline void op22_w_reg_id(IWriter& w, u32 id) { op22_w_id_fmt(w, id, g_op22_alife_id32); }

// поправка структур, прочитанных из старого сейва как есть (u16 + выравнивание): старшие 16 бит - мусор
template <typename T>
inline void op22_fix_old_ids(T&) {}
