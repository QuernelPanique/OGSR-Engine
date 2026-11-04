///////////////////////////////////////////////////////////////
// ExoOutfit.h
// ExoOutfit - защитный костюм с усилением
///////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "exooutfit.h"

#include "Actor.h"
#include "actor_defs.h"
#include "Inventory.h"
#include "ai_space.h"
#include "script_engine.h"
#include "script_game_object.h"

CExoOutfit::CExoOutfit() {}

CExoOutfit::~CExoOutfit() {}

bool CExoOutfit::CanAttach(PIItem pIItem)
{
    luabind::functor<bool> f;
    if (pIItem && ai().script_engine().functor("op22_exo.can_attach", f))
        return f(lua_game_object(), pIItem->object().lua_game_object());
    return inherited::CanAttach(pIItem);
}

bool CExoOutfit::Attach(PIItem pIItem, bool b_send_event)
{
    luabind::functor<void> f;
    if (!pIItem || !ai().script_engine().functor("op22_exo.attach", f))
        return inherited::Attach(pIItem, b_send_event);
    f(lua_game_object(), pIItem->object().lua_game_object());
    if (b_send_event)
        pIItem->object().DestroyObject();
    return true;
}

void CExoOutfit::op22_update(u32 actor_state)
{
    const bool jump = (actor_state & mcJump) != 0;
    if (jump && !m_op22_in_jump)
        ++m_op22_jumps;
    m_op22_in_jump = jump;

    const float ms = Device.fTimeDelta * 1000.f;
    if (actor_state & mcAnyMove)
    {
        if (actor_state & mcSprint)
            m_op22_ms_sprint += ms;
        else
            m_op22_ms_move += ms;
    }

    if (Device.dwTimeGlobal < m_op22_time)
        return;
    m_op22_time = Device.dwTimeGlobal + 1000;

    luabind::functor<bool> f;
    if (ai().script_engine().functor("op22_exo.update", f))
        m_op22_powered = f(lua_game_object(), m_op22_ms_move, m_op22_ms_sprint, m_op22_jumps);
    m_op22_ms_move = m_op22_ms_sprint = 0.f;
    m_op22_jumps = 0;
}

bool op22_exo_unpowered()
{
    CActor* actor = Actor();
    if (!actor)
        return false;
    const auto* exo = smart_cast<CExoOutfit*>(actor->inventory().m_slots[OUTFIT_SLOT].m_pIItem);
    return exo && !exo->op22_powered();
}
