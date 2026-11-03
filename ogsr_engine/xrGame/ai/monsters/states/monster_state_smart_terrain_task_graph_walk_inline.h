#pragma once

#define TEMPLATE_SPECIALIZATION template <typename _Object>

#define CStateMonsterSmartTerrainTaskGraphWalkAbstract CStateMonsterSmartTerrainTaskGraphWalk<_Object>

TEMPLATE_SPECIALIZATION
void CStateMonsterSmartTerrainTaskGraphWalkAbstract::initialize()
{
    inherited::initialize();

    CSE_ALifeMonsterAbstract* monster = smart_cast<CSE_ALifeMonsterAbstract*>(ai().alife().objects().object(object->ID()));
    VERIFY(monster);
    VERIFY(monster->m_smart_terrain_id != 0xffff);

    // get task
    CALifeSmartTerrainTask* task = monster->brain().smart_terrain().task(monster);
    VERIFY(task);
    if (task)
        m_task_gvid = task->game_vertex_id(); // OP22
}

TEMPLATE_SPECIALIZATION
bool CStateMonsterSmartTerrainTaskGraphWalkAbstract::check_start_conditions()
{
    CSE_ALifeMonsterAbstract* monster = smart_cast<CSE_ALifeMonsterAbstract*>(ai().alife().objects().object(object->ID()));
    VERIFY(monster);

    if (monster->m_smart_terrain_id == 0xffff)
        return false;

    CALifeSmartTerrainTask* task = monster->brain().smart_terrain().task(monster);
    VERIFY3(task, "Smart terrain selected, but task was not set for monster ", *object->cName());
    if (!task) // OP22
        return false;
    m_task_gvid = task->game_vertex_id();
    if (object->ai_location().game_vertex_id() == m_task_gvid)
        return false;

    return true;
}

TEMPLATE_SPECIALIZATION
bool CStateMonsterSmartTerrainTaskGraphWalkAbstract::check_completion()
{
    // if we get to the graph point - work complete
    if (object->ai_location().game_vertex_id() == m_task_gvid) // OP22
        return true;
    return false;
}

TEMPLATE_SPECIALIZATION
void CStateMonsterSmartTerrainTaskGraphWalkAbstract::execute()
{
    object->set_action(ACT_WALK_FWD);
    object->set_state_sound(MonsterSound::eMonsterSoundIdle);

    object->path().detour_graph_points(m_task_gvid); // OP22
}

#undef TEMPLATE_SPECIALIZATION
#undef CStateMonsterSmartTerrainTaskGraphWalkAbstract
