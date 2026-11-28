////////////////////////////////////////////////////////////////////////////
//	Module 		: level_graph.cpp
//	Created 	: 02.10.2001
//  Modified 	: 11.11.2003
//	Author		: Oles Shihkovtsov, Dmitriy Iassenev
//	Description : Level graph
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "level_graph.h"
#include <cmath>
#include <limits>

// OP22 (28.11 perf): поиск по кольцам с исходной метрикой контура.
int g_op22_nearest_vertex_fast = 1;

CLevelGraph::CLevelGraph(LPCSTR fName)
{
    sh_debug->create("editor\\wire"); // sh_debug->create("debug\\ai_nodes", "$null");

    m_reader = FS.r_open(fName);

    // m_header & data
    m_header = (CHeader*)m_reader->pointer();
    ASSERT_XRAI_VERSION_MATCH(header().version(), "Level graph");
    m_reader->advance(sizeof(CHeader));
    m_nodes = xr_new<CVertexStorage>(m_reader, header().vertex_count(), static_cast<xrAI_Versions>(header().version()));
    m_row_length = iFloor((header().box().max.z - header().box().min.z) / header().cell_size() + EPS_L + 1.5f);
    m_column_length = iFloor((header().box().max.x - header().box().min.x) / header().cell_size() + EPS_L + 1.5f);
    m_access_mask.assign(header().vertex_count(), true);
    unpack_xz(vertex_position(header().box().max), m_max_x, m_max_z);

#ifdef DEBUG
    m_current_level_id = -1;
    m_current_actual = false;
    m_current_center = Fvector().set(flt_max, flt_max, flt_max);
    m_current_radius = Fvector().set(flt_max, flt_max, flt_max);
#endif
}

CLevelGraph::~CLevelGraph() 
{ 
    xr_delete(m_nodes);
    FS.r_close(m_reader);
}

u32 CLevelGraph::nearest_vertex_id(const Fvector& position) const
{
    CLevelGraph::CPosition _node_position;
    vertex_position(_node_position, position);
    // OP22 (28.11 perf): ограничиваем число колец, чтобы на пустой сетке сохранить цену полного поиска.
    if (g_op22_nearest_vertex_fast)
    {
        const u32 id = nearest_vertex_id_in_grid(position);
        if (valid_vertex_id(id))
            return id;
    }

    float min_dist = flt_max;
    u32 selected;
    set_invalid_vertex(selected);
    for (u32 i = 0; i < header().vertex_count(); ++i)
    {
        float dist = distance(i, position);
        if (dist < min_dist)
        {
            min_dist = dist;
            selected = i;
        }
    }

    VERIFY(valid_vertex_id(selected));
    return (selected);
}

// OP22 (28.11 perf): просматриваем все этажи каждой клетки, при равенстве сохраняем первый ID.
u32 CLevelGraph::nearest_vertex_id_in_grid(const Fvector& position) const
{
    const auto begin = m_nodes->begin(), end = m_nodes->end();
    const float cell = header().cell_size();
    constexpr int world_limit_in_cells = 1 << 17;
    constexpr u32 axis_limit = 2 * world_limit_in_cells;
    if (begin == end || !m_row_length || m_row_length > axis_limit + 1 || !(cell >= 1.f / 1024 && cell <= 1024.f))
        return u32(-1);

    const auto& origin = header().box().min;
    const u32 last_row = (end - 1)->position().xz() / m_row_length;
    if (last_row > axis_limit)
        return u32(-1);
    const int last_x = static_cast<int>(last_row);
    const int last_z = static_cast<int>(m_row_length - 1);
    const float scale = cell * world_limit_in_cells;
    const auto world = [cell](int index, float offset) { return float(index) * cell + offset; };
    // Оставляем запас на округление при распаковке и интерполяции ребра в float.
    if (!(std::abs(origin.x) <= scale && std::abs(origin.z) <= scale &&
          std::abs(world(last_x, origin.x)) <= scale && std::abs(world(last_z, origin.z)) <= scale &&
          std::abs(position.x) <= scale && std::abs(position.z) <= scale))
        return u32(-1);

    const int center_x = std::clamp(iFloor((position.x - origin.x) / cell + .5f), 0, last_x);
    const int center_z = std::clamp(iFloor((position.z - origin.z) / cell + .5f), 0, last_z);
    u32 selected = u32(-1);
    float best = flt_max;
    const auto visit = [&](int x, int low_z, int high_z) {
        const u64 low = static_cast<u64>(x) * m_row_length + static_cast<u64>(low_z);
        const u64 high = static_cast<u64>(x) * m_row_length + static_cast<u64>(high_z);
        if (low > (std::numeric_limits<u32>::max)())
            return;
        auto node = std::lower_bound(begin, end, static_cast<u32>(low), &vertex::predicate2);
        for (; node != end && node->position().xz() <= high; ++node)
        {
            const u32 id = vertex_id(node);
            const float candidate = distance(position, node);
            if (candidate < best || (candidate == best && id < selected && valid_vertex_id(selected)))
            {
                best = candidate;
                selected = id;
            }
        }
    };

    for (int ring = 0; ring <= 16; ++ring)
    {
        const int left = center_x - ring, right = center_x + ring;
        const int bottom = center_z - ring, top = center_z + ring;
        for (int x = std::max(0, left); x <= std::min(last_x, right); ++x)
        {
            if (x == left || x == right)
                visit(x, std::max(0, bottom), std::min(last_z, top));
            else
            {
                if (bottom >= 0)
                    visit(x, bottom, bottom);
                if (top <= last_z)
                    visit(x, top, top);
            }
        }

        // Проекция контура меняет только Y. Для ребра берём полклетки и ещё клетку на округление.
        double gap = std::numeric_limits<double>::infinity();
        if (left > 0)
            gap = std::min(gap, double(position.x) - world(left - 1, origin.x));
        if (right < last_x)
            gap = std::min(gap, world(right + 1, origin.x) - double(position.x));
        if (bottom > 0)
            gap = std::min(gap, double(position.z) - world(bottom - 1, origin.z));
        if (top < last_z)
            gap = std::min(gap, world(top + 1, origin.z) - double(position.z));
        const double lower = (gap - 1.5 * cell) * .5;
        if (valid_vertex_id(selected) && lower > 0 && lower * lower > best)
            return selected;
    }
    return u32(-1);
}

u32 CLevelGraph::vertex_id(u32 current_node_id, const Fvector& position) const
{
    START_PROFILE("Level_Graph::find vertex")
    Device.Statistic->AI_Node.Begin();

    if (valid_vertex_position(position))
    {
        // so, our position is inside the level graph bounding box
        if (valid_vertex_id(current_node_id) && inside(vertex(current_node_id), position))
        {
            // so, our node corresponds to the position
            Device.Statistic->AI_Node.End();
            return (current_node_id);
        }

        // so, our node doesn't correspond to the position
        // try to search it with O(logN) time algorithm
        u32 _vertex_id = vertex_id(position);
        if (valid_vertex_id(_vertex_id))
        {
            // so, there is a node which corresponds with x and z to the position
            bool ok = true;
            if (valid_vertex_id(current_node_id))
            {
                float y0 = vertex_plane_y(current_node_id, position.x, position.z);
                float y1 = vertex_plane_y(_vertex_id, position.x, position.z);
                bool over0 = position.y > y0;
                bool over1 = position.y > y1;
                float y_dist0 = position.y - y0;
                float y_dist1 = position.y - y1;
                if (over0)
                {
                    if (over1)
                    {
                        if (y_dist1 - y_dist0 > 1.f)
                            ok = false;
                        else
                            ok = true;
                    }
                    else
                    {
                        ok = false;
                    }
                }
                else
                {
                    if (over1)
                    {
                        ok = true;
                    }
                    else
                    {
                        ok = false;
                    }
                }
            }
            if (ok)
            {
                Device.Statistic->AI_Node.End();
                return (_vertex_id);
            }
        }
    }

    if (!valid_vertex_id(current_node_id))
    {
        // so, we do not have a correct current node
        // performing very slow full search
        const u32 id = nearest_vertex_id(position);
        VERIFY(valid_vertex_id(id));
        Device.Statistic->AI_Node.End();
        return (id);
    }

    // so, our position is outside the level graph bounding box
    // or
    // there is no node for the current position
    // try to search the nearest one iteratively
    SContour _contour;
    Fvector point;
    u32 best_vertex_id = current_node_id;
    contour(_contour, current_node_id);
    nearest(point, position, _contour);
    float best_distance_sqr = position.distance_to_sqr(point);
    const_iterator i, e;
    begin(current_node_id, i, e);
    for (; i != e; ++i)
    {
        u32 level_vertex_id = value(current_node_id, i);
        if (!valid_vertex_id(level_vertex_id))
            continue;

        contour(_contour, level_vertex_id);
        nearest(point, position, _contour);
        float distance_sqr = position.distance_to_sqr(point);
        if (best_distance_sqr > distance_sqr)
        {
            best_distance_sqr = distance_sqr;
            best_vertex_id = level_vertex_id;
        }
    }

    Device.Statistic->AI_Node.End();
    return (best_vertex_id);

    STOP_PROFILE
}

u32 CLevelGraph::vertex_id(const Fvector& position) const
{
    CPosition _vertex_position = vertex_position(position);
    CVertex* B = m_nodes->begin();
    CVertex* E = m_nodes->end();
    CVertex* I = std::lower_bound(B, E, _vertex_position.xz());
    if ((I == E) || ((*I).position().xz() != _vertex_position.xz()))
        return (u32(-1));

    u32 best_vertex_id = u32(I - B);
    float y = vertex_plane_y(best_vertex_id, position.x, position.z);
    for (++I; I != E; ++I)
    {
        if ((*I).position().xz() != _vertex_position.xz())
            break;

        u32 new_vertex_id = u32(I - B);
        float _y = vertex_plane_y(new_vertex_id, position.x, position.z);
        if (y <= position.y)
        {
            // so, current node is under the specified position
            if (_y <= position.y)
            {
                // so, new node is under the specified position
                if (position.y - _y < position.y - y)
                {
                    // so, new node is closer to the specified position
                    y = _y;
                    best_vertex_id = new_vertex_id;
                }
            }
        }
        else
            // so, current node is over the specified position
            if (_y <= position.y)
            {
                // so, new node is under the specified position
                y = _y;
                best_vertex_id = new_vertex_id;
            }
            else
                // so, new node is over the specified position
                if (_y - position.y < y - position.y)
                {
                    // so, new node is closer to the specified position
                    y = _y;
                    best_vertex_id = new_vertex_id;
                }
    }

    return (best_vertex_id);
}