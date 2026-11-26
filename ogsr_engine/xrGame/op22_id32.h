#pragma once
// OP22 (id32): чтение ID объекта из client_data (сохранённые данные клиентских объектов: память NPC, инвентарь, ...).
// Старые сейвы - 2 байта, client_data нового движка - 4 байта (флаг M_SPAWN_CD_ID32 объекта). Флаг текущего чтения
// ставит CGameObject::net_Spawn на время net_Load. Запись - всегда NET_Packet::w_id (4 байта).
extern bool g_op22_cd_id32;

inline u32 op22_r_cd_id(IReader& r) { return g_op22_cd_id32 ? r.r_u32() : u32(r.r_u16()); }
