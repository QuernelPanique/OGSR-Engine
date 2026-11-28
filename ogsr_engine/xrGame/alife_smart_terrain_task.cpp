////////////////////////////////////////////////////////////////////////////
//	Module 		: alife_smart_terrain_task.cpp
//	Created 	: 20.09.2005
//  Modified 	: 20.09.2005
//	Author		: Dmitriy Iassenev
//	Description : ALife smart terrain task
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "alife_smart_terrain_task.h"
#include "ai_space.h"
#include "patrol_path_storage.h"
#include "patrol_path.h"
#include "patrol_point.h"

#ifdef DEBUG
#include "level_graph.h"
#include "game_graph.h"
#endif

// OP22 (28.11 perf): кэшируем поиск точки, сохраняя вызовы task() из офлайн A-Life.
int g_op22_patrol_point_cache = 1;

void CALifeSmartTerrainTask::setup_patrol_point(const shared_str& patrol_path_name, const u32& patrol_point_index)
{
    VERIFY(!m_patrol_point);

    if (g_op22_patrol_point_cache)
    {
        m_patrol_point = ai().patrol_paths().task_point(patrol_path_name, patrol_point_index);
        VERIFY(m_patrol_point);
        return;
    }

    const CPatrolPath* patrol_path = ai().patrol_paths().path(patrol_path_name);
    VERIFY(patrol_path);

    m_patrol_point = &patrol_path->vertex(patrol_point_index)->data();
    VERIFY(m_patrol_point);
}

GameGraph::_GRAPH_ID CALifeSmartTerrainTask::game_vertex_id() const
{
    VERIFY3(ai().game_graph().valid_vertex_id(patrol_point().game_vertex_id()), *m_patrol_path_name, *m_patrol_point->name());
    return (patrol_point().game_vertex_id());
}

u32 CALifeSmartTerrainTask::level_vertex_id() const
{
    VERIFY3(ai().game_graph().valid_vertex_id(patrol_point().game_vertex_id()), *m_patrol_path_name, *m_patrol_point->name());
    return (patrol_point().level_vertex_id());
}

Fvector CALifeSmartTerrainTask::position() const
{
    return (patrol_point().position());
}

CPatrolPoint* CALifeSmartTerrainTask::point_raw()
{
    return const_cast<CPatrolPoint*>(m_patrol_point);
}
