#include "stdafx.h"
#include "xrServer_Objects_ALife_All.h"
#include "level.h"
#include "game_cl_base.h"
#include "net_queue.h"
#include "ai_space.h"
#include "game_level_cross_table.h"
#include "level_graph.h"
#include "client_spawn_manager.h"
#include "..\xr_3da\xr_object.h"
#include "..\xr_3da\IGame_Persistent.h"
#include "../xr_3da/op22_frame_profile.h"

// OP22 (28.11 perf): проверяем ожидающего родителя по ID, сохраняя FIFO очереди.
#include <atomic>
extern std::atomic<bool> g_op22_spawn_parent_index;

bool CLevel::has_pending_spawn(u32 id)
{
    if (!g_op22_spawn_parent_index.load(std::memory_order_relaxed))
    {
        m_pending_spawn_ids.reset();
        return std::any_of(game_spawn_queue.begin(), game_spawn_queue.end(), [id](const CSE_Abstract* entity) { return entity->ID == id; });
    }

    if (!m_pending_spawn_ids)
    {
        m_pending_spawn_ids.emplace();
        for (const CSE_Abstract* entity : game_spawn_queue)
            ++(*m_pending_spawn_ids)[entity->ID];
    }
    return m_pending_spawn_ids->contains(id);
}

// OP22 (28.11 perf): снимаем ровно одну запись при извлечении или отмене спавна.
void CLevel::forget_pending_spawn(u32 id)
{
    if (!m_pending_spawn_ids)
        return;

    const auto it = m_pending_spawn_ids->find(id);
    VERIFY(it != m_pending_spawn_ids->end());
    if (--it->second == 0)
        m_pending_spawn_ids->erase(it);
}

void CLevel::cl_Process_Spawn(NET_Packet& P)
{
    // Begin analysis
    shared_str s_name;
    P.r_stringZ(s_name);

    // Create DC (xrSE)
    CSE_Abstract* E = F_entity_Create(*s_name);
    R_ASSERT2(E, *s_name);

    E->Spawn_Read(P);
    if (E->s_flags.is(M_SPAWN_UPDATE))
        E->UPDATE_Read(P);
    //-------------------------------------------------
    //	Msg ("M_SPAWN - %s[%d][%x] - %d", *s_name,  E->ID, E,E->ID_Parent);
    //-------------------------------------------------
    // force object to be local for server client
    {
        E->s_flags.set(M_SPAWN_OBJECT_LOCAL, TRUE);
    };

    if (std::find(m_just_destroyed.begin(), m_just_destroyed.end(), E->ID) != m_just_destroyed.end())
    {
        Msg("* [%s]: skip just destroyed [%s] ID: [%u] ID_Parent: [%u]", __FUNCTION__, E->name_replace(), E->ID, E->ID_Parent);
        m_just_destroyed.erase(std::remove(m_just_destroyed.begin(), m_just_destroyed.end(), E->ID), m_just_destroyed.end());
        F_entity_Destroy(E);
        return;
    }

    if (Device.dwPrecacheFrame == 0)
    {
        const CSE_ALifeMonsterAbstract* monster = smart_cast<CSE_ALifeMonsterAbstract*>(E);
        const CSE_ALifeTraderAbstract* trader = smart_cast<CSE_ALifeTraderAbstract*>(E);
        // OP22 (28.11 perf): прежнее условие отсрочки с быстрым поиском родителя.
        const bool postpone = ((trader || monster) && !is_removing_objects()) || has_pending_spawn(E->ID_Parent);
        if (postpone)
        {
            //Msg( "* [%s]: delay spawn ID[%d] ID_Parent[%d] name_replace[%s]", __FUNCTION__, E->ID, E->ID_Parent, E->name_replace() );
            game_spawn_queue.push_back(E);
            if (m_pending_spawn_ids) // OP22 (28.11 perf): добавляем ID только для отложенного спавна.
                ++(*m_pending_spawn_ids)[E->ID];
            return;
        }
    }

    /*
    game_spawn_queue.push_back(E);
    if (g_bDebugEvents)		ProcessGameSpawns();
    /*/
    g_sv_Spawn(E);

    F_entity_Destroy(E);
    //*/
};

void CLevel::g_cl_Spawn(LPCSTR name, u8 rp, u16 flags, Fvector pos)
{
    // Create
    CSE_Abstract* E = F_entity_Create(name);
    VERIFY(E);

    // Fill
    E->s_name = name;
    E->set_name_replace("");
    E->s_gameid = u8(GameID());
    E->s_RP = rp;
    E->ID = 0xffff;
    E->ID_Parent = 0xffff;
    E->ID_Phantom = 0xffff;
    E->s_flags.assign(flags);
    E->RespawnTime = 0;
    E->o_Position = pos;

    // Send
    NET_Packet P;
    E->Spawn_Write(P, TRUE);
    Send(P, net_flags(TRUE));

    // Destroy
    F_entity_Destroy(E);
}

#ifdef DEBUG
extern Flags32 psAI_Flags;
#include "ai_debug.h"
#endif // DEBUG

void CLevel::g_sv_Spawn(CSE_Abstract* E)
{
    //-----------------------------------------------------------------
    //	CTimer		T(false);

#ifdef DEBUG
    Msg("* CLIENT: Spawn: %s, ID=%d", *E->s_name, E->ID);
#endif

    auto obj = Objects.net_Find(E->ID);
    if (obj && obj->getDestroy())
    {
        Msg("[%s]: %s[%u] already net_Spawn'ed, call ProcessDestroyQueue()", __FUNCTION__, obj->cName().c_str(), obj->ID());
        Objects.ProcessDestroyQueue();
    }

    // Client spawn
    //	T.Start		();
    CObject* O = Objects.Create(*E->s_name);
    // Msg				("--spawn--CREATE: %f ms",1000.f*T.GetAsync());

    //	T.Start		();
    if (0 == O || (!O->net_Spawn(E)))
    {
        O->setDestroy(TRUE);
        O->net_Destroy();
        client_spawn_manager().clear(O->ID());
        Objects.Destroy(O);
        Msg("! Failed to spawn entity '%s'", *E->s_name);
    }
    else
    {
        client_spawn_manager().callback(O);
        // Msg			("--spawn--SPAWN: %f ms",1000.f*T.GetAsync());
        if ((E->s_flags.is(M_SPAWN_OBJECT_LOCAL)) && (E->s_flags.is(M_SPAWN_OBJECT_ASPLAYER)))
        {
            if (CurrentEntity() != NULL)
            {
                CGameObject* pGO = smart_cast<CGameObject*>(CurrentEntity());
                if (pGO)
                    pGO->On_B_NotCurrentEntity();
            }
            SetEntity(O);
            SetControlEntity(O);
        }

        if (0xffff != E->ID_Parent)
        {
            NET_Packet GEN;
            GEN.write_start();
            GEN.read_start();
            GEN.w_id(O->ID()); // OP22 (id32)
            cl_Process_Event(E->ID_Parent, GE_OWNERSHIP_TAKE, GEN);
        }
    }
    //---------------------------------------------------------
    Game().OnSpawn(O);
    //---------------------------------------------------------
}

CSE_Abstract* CLevel::spawn_item(LPCSTR section, const Fvector& position, u32 level_vertex_id, u32 parent_id, bool return_item)
{
    CSE_Abstract* abstract = F_entity_Create(section);
    R_ASSERT3(abstract, "Cannot find item with section", section);
    CSE_ALifeDynamicObject* dynamic_object = smart_cast<CSE_ALifeDynamicObject*>(abstract);
    if (dynamic_object && ai().get_level_graph())
    {
        dynamic_object->m_tNodeID = level_vertex_id;
        if (ai().level_graph().valid_vertex_id(level_vertex_id) && ai().get_game_graph() && ai().get_cross_table())
            dynamic_object->m_tGraphID = ai().cross_table().vertex(level_vertex_id).game_vertex_id();
    }

    //оружие спавним с полным магазинои
    CSE_ALifeItemWeapon* weapon = smart_cast<CSE_ALifeItemWeapon*>(abstract);
    if (weapon)
        weapon->a_elapsed = weapon->get_ammo_magsize();

    // Fill
    abstract->s_name = section;
    abstract->set_name_replace(section);
    abstract->s_gameid = u8(GameID());
    abstract->o_Position = position;
    abstract->s_RP = 0xff;
    abstract->ID = 0xffff;
    abstract->ID_Parent = parent_id;
    abstract->ID_Phantom = 0xffff;
    abstract->s_flags.assign(M_SPAWN_OBJECT_LOCAL);
    abstract->RespawnTime = 0;

    if (!return_item)
    {
        NET_Packet P;
        abstract->Spawn_Write(P, TRUE);
        Send(P, net_flags(TRUE));
        F_entity_Destroy(abstract);
        return (0);
    }
    else
        return (abstract);
}


void CLevel::ProcessGameSpawns()
{
    // OP22 (28.11 perf): учитываем спавн из очереди, включая обработку детей.
    const Op22HitchTimer timer(Op22HitchStage::spawn, !game_spawn_queue.empty());
    CSE_Abstract* trader = nullptr;
    while (!game_spawn_queue.empty())
    {
        CSE_Abstract* E = game_spawn_queue.front();
        game_spawn_queue.pop_front();
        forget_pending_spawn(E->ID); // OP22 (28.11 perf): извлечённый NPC больше не считается ожидающим.
        //Msg( "* [%s]: delayed spawn dwFrame[%u] ID[%d] ID_Parent[%d] name_replace[%s]", __FUNCTION__, Device.dwFrame, E->ID, E->ID_Parent, E->name_replace() );
        g_sv_Spawn(E);
        if (smart_cast<CSE_ALifeMonsterAbstract*>(E) || smart_cast<CSE_ALifeTraderAbstract*>(E))
        {
            trader = E;
            break;
        }
        F_entity_Destroy(E);
    }

    if (trader)
    {
        for (auto& E : game_spawn_queue)
        {
            if (E->ID_Parent == trader->ID)
            {
                // Msg( "* [%s]: delayed spawn dwFrame[%u] trader[%d] ID[%d] ID_Parent[%d] name_replace[%s]", __FUNCTION__, Device.dwFrame, trader->ID, E->ID, E->ID_Parent, E->name_replace() );
                g_sv_Spawn(E);
            }
        }
        
        std::erase_if(game_spawn_queue, [&](auto& E) {
            if (E->ID_Parent == trader->ID)
            {
                forget_pending_spawn(E->ID); // OP22 (28.11 perf): дети остаются в очереди до завершения семейного спавна.
                F_entity_Destroy(E);
                return true;
            }
            return false;
        });

        F_entity_Destroy(trader);
    }
}

void CLevel::ProcessGameSpawnsDestroy(u32 dest, u16 type, NET_Packet& P) // OP22 (id32)
{
    if (type != GE_DESTROY)
        return;

    game_spawn_queue.erase(std::remove_if(game_spawn_queue.begin(), game_spawn_queue.end(),
                                          [&](auto& E) {
                                                  if (E->ID == dest || E->ID_Parent == dest)
                                                  {
                                                      // Msg( "* [CLevel::ProcessGameSpawnsDestroy]: delayed spawn GE_DESTROY dest[%d] ID[%d] ID_Parent[%d] name_replace[%s]", dest,
                                                      // E->ID, E->ID_Parent, E->name_replace() );
                                                      forget_pending_spawn(E->ID); // OP22 (28.11 perf): отмена убирает ID из индекса.
                                                      F_entity_Destroy(E);
                                                      return true;
                                                  }
                                              return false;
                                          }),
                           game_spawn_queue.end());
}
