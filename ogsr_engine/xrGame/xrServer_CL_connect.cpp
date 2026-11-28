#include "stdafx.h"
#include "xrserver.h"
#include "xrmessages.h"
#include "hudmanager.h"
#include "xrserver_objects.h"
#include "Level.h"

// OP22 (28.11 perf): отмечаем ID перед рекурсией, сохраняя обработку циклов и порядок спавна.
#include <atomic>
#include <unordered_set>

extern std::atomic<bool> g_op22_connect_spawn_ids;

struct xrServer::ConnectSpawnIds
{
    const bool use_hash = g_op22_connect_spawn_ids.load(std::memory_order_relaxed);
    std::unordered_set<ALife::_OBJECT_ID> indexed;
    xr_vector<ALife::_OBJECT_ID> linear;

    explicit ConnectSpawnIds(size_t count)
    {
        if (use_hash)
            indexed.reserve(count);
    }

    bool visit(ALife::_OBJECT_ID id)
    {
        if (use_hash)
            return indexed.insert(id).second;
        if (std::find(linear.begin(), linear.end(), id) != linear.end())
            return false;
        linear.push_back(id);
        return true;
    }
};

void xrServer::Perform_connect_spawn(CSE_Abstract* E, xrClientData* CL, NET_Packet& P, ConnectSpawnIds& visited)
{
    if (!visited.visit(E->ID))
        return;

    if (E->net_Processed)
        return;
    if (E->s_flags.is(M_SPAWN_OBJECT_PHANTOM))
        return;

    // Connectivity order
    CSE_Abstract* Parent = ID_to_entity(E->ID_Parent);
    if (Parent)
        Perform_connect_spawn(Parent, CL, P, visited); // OP22 (28.11 perf): родитель использует тот же набор посещённых ID.

    // Process
    Flags16 save = E->s_flags;
    //-------------------------------------------------
    E->s_flags.set(M_SPAWN_UPDATE, TRUE);
    if (0 == E->owner)
    {
        // PROCESS NAME; Name this entity
        if (E->s_flags.is(M_SPAWN_OBJECT_ASPLAYER))
        {
            CL->owner = E;
            E->set_name_replace(*CL->name);
        }

        // Associate
        E->owner = CL;
        E->Spawn_Write(P, TRUE);
        E->UPDATE_Write(P);
    }
    else
    {
        // Just inform
        E->Spawn_Write(P, FALSE);
        E->UPDATE_Write(P);
    }
    //-----------------------------------------------------
    E->s_flags = save;
    SendTo(CL->ID, P, net_flags(TRUE, TRUE));
    E->net_Processed = TRUE;
}

void xrServer::SendConnectionData(IClient* _CL)
{
    ConnectSpawnIds visited(entities.size()); // OP22 (28.11 perf): отдельный набор на каждый вход.
    xrClientData* CL = (xrClientData*)_CL;
    NET_Packet P;
    u32 mode = net_flags(TRUE, TRUE);
    // Replicate current entities on to this client
    xrS_entities::iterator I = entities.begin(), E = entities.end();
    for (; I != E; ++I)
        I->second->net_Processed = FALSE;
    for (I = entities.begin(); I != E; ++I)
        Perform_connect_spawn(I->second, CL, P, visited); // OP22 (28.11 perf): порядок задаёт прежняя карта entities.

    // Send "finished" signal
    P.w_begin(M_SV_CONFIG_FINISHED);
    SendTo(CL->ID, P, mode);
};

void xrServer::OnCL_Connected(IClient* _CL)
{
    xrClientData* CL = (xrClientData*)_CL;
    CL->net_Accepted = TRUE;

    csPlayers.Enter();

    Export_game_type(CL);
    Perform_game_export();
    SendConnectionData(CL);

    //
    NET_Packet P;
    P.B.count = 0;
    P.w_clientID(CL->ID);
    P.r_pos = 0;
    ClientID clientID;
    clientID.set(0);
    game->AddDelayedEvent(P, GAME_EVENT_PLAYER_CONNECTED, 0, clientID);
    csPlayers.Leave();
    game->ProcessDelayedEvent();
}

void xrServer::SendConnectResult(IClient* CL, u8 res, u8 res1, const char* ResultStr)
{
    NET_Packet P;
    P.w_begin(M_CLIENT_CONNECT_RESULT);
    P.w_u8(res);
    P.w_u8(res1);
    P.w_stringZ(ResultStr);

    if (SV_Client && SV_Client == CL)
        P.w_u8(1);
    else
        P.w_u8(0);
    P.w_stringZ(Level().m_caServerOptions);

    SendTo(CL->ID, P);
};

void xrServer::OnBuildVersionRespond(IClient* CL, NET_Packet& P)
{
    u16 Type;
    P.r_begin(Type);
    u64 _our = 0;
    u64 _him = P.r_u64();

#ifdef DEBUG
    Msg("_our = %d", _our);
    Msg("_him = %d", _him);
#endif // DEBUG

    if (_our != _him)
    {
        SendConnectResult(CL, 0, 0, "Data verification failed. Cheater? [3]");
    }
    else
    {
        Check_BuildVersion_Success(CL);
    }
};

void xrServer::Check_BuildVersion_Success(IClient* CL)
{
    CL->flags.bVerified = TRUE;
    SendConnectResult(CL, 1, 0, "All Ok");
};