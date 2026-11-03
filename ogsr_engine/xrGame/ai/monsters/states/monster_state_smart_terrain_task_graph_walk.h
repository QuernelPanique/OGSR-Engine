#pragma once
#include "../state.h"
#include "game_graph_space.h" // OP22: m_task_gvid

template <typename _Object>
class CStateMonsterSmartTerrainTaskGraphWalk : public CStateMove<_Object>
{
    typedef CStateMove<_Object> inherited;
    using inherited::inherited::object;

    // OP22: храним вершину графа, а не указатель: задание создаёт скрипт (CALifeSmartTerrainTask(...) в
    // se_smart_terrain:task), им владеет Lua - после сборки мусора указатель висячий (вылет в execute)
    GameGraph::_GRAPH_ID m_task_gvid{GameGraph::_GRAPH_ID(-1)};

public:
    CStateMonsterSmartTerrainTaskGraphWalk(_Object* obj) : inherited(obj) {}
    virtual void initialize();
    virtual void execute();
    virtual bool check_start_conditions();
    virtual bool check_completion();
    virtual void remove_links(CObject* object) { inherited::remove_links(object); }
};

#include "monster_state_smart_terrain_task_graph_walk_inline.h"