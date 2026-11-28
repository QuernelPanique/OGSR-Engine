////////////////////////////////////////////////////////////////////////////
//	Module 		: space_restriction_manager.cpp
//	Created 	: 17.08.2004
//  Modified 	: 27.08.2004
//	Author		: Dmitriy Iassenev
//	Description : Space restriction manager
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "space_restriction.h"
#include "restriction_space.h"
#include "space_restriction_manager.h"
#include "space_restriction_bridge.h"
#include "object_broker.h"
#include <algorithm>
#include <string_view>

const u32 time_to_delete = 300000;
// OP22 (28.11 perf): пакетная смена глобальных ограничителей.
int g_op22_restriction_batch = 1;

struct CSpaceRestrictionManager::CClientRestriction
{
    CRestrictionPtr m_restriction;
    shared_str m_base_out_restrictions;
    shared_str m_base_in_restrictions;
};

CSpaceRestrictionManager::CSpaceRestrictionManager() { m_clients = xr_new<CLIENT_RESTRICTIONS>(); }

CSpaceRestrictionManager::~CSpaceRestrictionManager()
{
    xr_delete(m_clients);
    delete_data(m_space_restrictions);
}

void show_restriction(const shared_str& restrictions)
{
    string256 temp;
    for (int i = 0, n = _GetItemCount(*restrictions); i < n; ++i)
        Msg("     %s", _GetItem(*restrictions, i, temp));
}

typedef intrusive_ptr<CSpaceRestriction, RestrictionSpace::CTimeIntrusiveBase> CRestrictionPtr;
void show_restriction(const CRestrictionPtr& restriction)
{
    Msg("out");
    show_restriction(restriction->out_restrictions());
    Msg("in");
    show_restriction(restriction->in_restrictions());
}

void CSpaceRestrictionManager::clear()
{
    m_clients->clear();
    delete_data(m_space_restrictions);

    CSpaceRestrictionHolder::clear();
}

void CSpaceRestrictionManager::remove_border(ALife::_OBJECT_ID id)
{
    CRestrictionPtr client_restriction = restriction(id);
    if (client_restriction)
        client_restriction->remove_border();
}

shared_str CSpaceRestrictionManager::in_restrictions(ALife::_OBJECT_ID id)
{
    CRestrictionPtr client_restriction = restriction(id);
    if (client_restriction)
        return (client_restriction->in_restrictions());
    return ("");
}

shared_str CSpaceRestrictionManager::out_restrictions(ALife::_OBJECT_ID id)
{
    CRestrictionPtr client_restriction = restriction(id);
    if (client_restriction)
        return (client_restriction->out_restrictions());
    return ("");
}

shared_str CSpaceRestrictionManager::base_in_restrictions(ALife::_OBJECT_ID id)
{
    CLIENT_RESTRICTIONS::iterator I = m_clients->find(id);
    VERIFY(m_clients->end() != I);
    return ((*I).second.m_base_in_restrictions);
}

shared_str CSpaceRestrictionManager::base_out_restrictions(ALife::_OBJECT_ID id)
{
    CLIENT_RESTRICTIONS::iterator I = m_clients->find(id);
    VERIFY(m_clients->end() != I);
    return ((*I).second.m_base_out_restrictions);
}

/*IC*/ CSpaceRestrictionManager::CRestrictionPtr CSpaceRestrictionManager::restriction(ALife::_OBJECT_ID id)
{
    CLIENT_RESTRICTIONS::iterator I = m_clients->find(id);
    VERIFY(m_clients->end() != I);
    return ((*I).second.m_restriction);
}

// OP22 (28.11 perf): правило срока жизни одинаково для очистки и поиска в пакете.
static bool expired_restriction(CSpaceRestriction* restriction)
{
    return restriction->released() && Device.dwTimeGlobal >= restriction->m_last_time_dec + time_to_delete;
}

IC void CSpaceRestrictionManager::collect_garbage()
{
    std::erase_if(m_space_restrictions, [](auto& pair) {
        if (expired_restriction(pair.second))
        {
            xr_delete(pair.second);
            return true;
        }
        return false;
    });
}

void CSpaceRestrictionManager::restrict(ALife::_OBJECT_ID id, shared_str out_restrictors, shared_str in_restrictors)
{
    shared_str merged_out_restrictions = out_restrictors;
    shared_str merged_in_restrictions = in_restrictors;
    shared_str _default_out_restrictions = default_out_restrictions();
    shared_str _default_in_restrictions = default_in_restrictions();

    difference_restrictions(_default_out_restrictions, merged_in_restrictions);
    difference_restrictions(_default_in_restrictions, merged_out_restrictions);

    join_restrictions(merged_out_restrictions, _default_out_restrictions);
    join_restrictions(merged_in_restrictions, _default_in_restrictions);

    // OP22 (28.11 perf): одиночная смена по-прежнему сразу очищает кэш.
    set_client_restriction(id, out_restrictors, in_restrictors, merged_out_restrictions, merged_in_restrictions);
    collect_garbage();
}

void CSpaceRestrictionManager::set_client_restriction(ALife::_OBJECT_ID id, shared_str base_out, shared_str base_in, shared_str merged_out, shared_str merged_in,
                                                      bool expire_on_lookup)
{
    CLIENT_RESTRICTIONS::iterator I = m_clients->find(id);
    VERIFY2((m_clients->end() == I) || !(*I).second.m_restriction || !(*I).second.m_restriction->applied(), "Restriction cannot be changed since its border is still applied!");
    (*m_clients)[id].m_restriction = restriction(merged_out, merged_in, expire_on_lookup);
    (*m_clients)[id].m_base_out_restrictions = base_out;
    (*m_clients)[id].m_base_in_restrictions = base_in;
}

void CSpaceRestrictionManager::unrestrict(ALife::_OBJECT_ID id)
{
    CLIENT_RESTRICTIONS::iterator I = m_clients->find(id);
    VERIFY(I != m_clients->end());
    m_clients->erase(I);
    collect_garbage();
}

bool CSpaceRestrictionManager::accessible(ALife::_OBJECT_ID id, const Fsphere& sphere)
{
    CRestrictionPtr client_restriction = restriction(id);
    if (client_restriction)
        return (client_restriction->accessible(sphere));
    return (true);
}

bool CSpaceRestrictionManager::accessible(ALife::_OBJECT_ID id, u32 level_vertex_id, float radius)
{
    CRestrictionPtr client_restriction = restriction(id);
    if (client_restriction)
        return (client_restriction->accessible(level_vertex_id, radius));
    return (true);
}

CSpaceRestrictionManager::CRestrictionPtr CSpaceRestrictionManager::restriction(shared_str out_restrictors, shared_str in_restrictors, bool expire_on_lookup)
{
    string4096 m_temp;
    if (!xr_strlen(out_restrictors) && !xr_strlen(in_restrictors))
        return (0);

    out_restrictors = normalize_string(out_restrictors);
    in_restrictors = normalize_string(in_restrictors);

    strconcat(sizeof(m_temp), m_temp, *out_restrictors, "\x01", *in_restrictors);
    shared_str space_restrictions = m_temp;

    const auto I = m_space_restrictions.find(space_restrictions);
    if (I != m_space_restrictions.end())
    {
        // OP22 (28.11 perf): не оживляем запись, которую уже удалил бы проход после предыдущего клиента.
        if (!expire_on_lookup || !expired_restriction(I->second))
            return I->second;
        xr_delete(I->second);
        m_space_restrictions.erase(I);
    }

    CSpaceRestriction* client_restriction = xr_new<CSpaceRestriction>(this, out_restrictors, in_restrictors);
    m_space_restrictions.insert(std::make_pair(space_restrictions, client_restriction));
    return (client_restriction);
}

u32 CSpaceRestrictionManager::accessible_nearest(ALife::_OBJECT_ID id, const Fvector& position, Fvector& result)
{
    CRestrictionPtr client_restriction = restriction(id);
    VERIFY(client_restriction);
    return (client_restriction->accessible_nearest(position, result));
}

/*IC*/ bool CSpaceRestrictionManager::restriction_presented(shared_str restrictions, shared_str restriction) const
{
    string4096 m_temp;
    for (u32 i = 0, n = _GetItemCount(*restrictions); i < n; ++i)
        if (!xr_strcmp(restriction, _GetItem(*restrictions, i, m_temp)))
            return true;
    return false;
}

IC void CSpaceRestrictionManager::join_restrictions(shared_str& restrictions, shared_str update)
{
    string4096 m_temp1;
    string4096 m_temp2;
    strcpy_s(m_temp2, *restrictions);
    for (u32 i = 0, n = _GetItemCount(*update), count = xr_strlen(m_temp2); i < n; ++i)
        if (!restriction_presented(m_temp2, _GetItem(*update, i, m_temp1)))
        {
            if (count)
                strcat_s(m_temp2, ",");
            strcat_s(m_temp2, m_temp1);
            ++count;
        }
    restrictions = shared_str(m_temp2);
}

IC void CSpaceRestrictionManager::difference_restrictions(shared_str& restrictions, shared_str update)
{
    string4096 m_temp1;
    string4096 m_temp2;
    strcpy_s(m_temp2, "");
    for (u32 i = 0, n = _GetItemCount(*restrictions), count = 0; i < n; ++i)
        if (!restriction_presented(update, _GetItem(*restrictions, i, m_temp1)))
        {
            if (count)
                strcat_s(m_temp2, ",");
            strcat_s(m_temp2, m_temp1);
            ++count;
        }
    restrictions = shared_str(m_temp2);
}

void CSpaceRestrictionManager::add_restrictions(ALife::_OBJECT_ID id, shared_str add_out_restrictions, shared_str add_in_restrictions)
{
    CRestrictionPtr _client_restriction = restriction(id);
    if (!_client_restriction)
    {
        restrict(id, add_out_restrictions, add_in_restrictions);
        return;
    }

    VERIFY(!_client_restriction->applied());

    CClientRestriction& client_restriction = (*m_clients)[id];

    shared_str new_out_restrictions = client_restriction.m_base_out_restrictions;
    shared_str new_in_restrictions = client_restriction.m_base_in_restrictions;

    join_restrictions(new_out_restrictions, add_out_restrictions);
    join_restrictions(new_in_restrictions, add_in_restrictions);

    restrict(id, new_out_restrictions, new_in_restrictions);
}

void CSpaceRestrictionManager::remove_restrictions(ALife::_OBJECT_ID id, shared_str remove_out_restrictions, shared_str remove_in_restrictions)
{
    CRestrictionPtr _client_restriction = restriction(id);
    if (!_client_restriction)
        return;

    VERIFY(!_client_restriction->applied());

    CClientRestriction& client_restriction = (*m_clients)[id];

    shared_str new_out_restrictions = client_restriction.m_base_out_restrictions;
    shared_str new_in_restrictions = client_restriction.m_base_in_restrictions;

    difference_restrictions(new_out_restrictions, remove_out_restrictions);
    difference_restrictions(new_in_restrictions, remove_in_restrictions);

    restrict(id, new_out_restrictions, new_in_restrictions);
}

void CSpaceRestrictionManager::change_restrictions(ALife::_OBJECT_ID id, shared_str add_out_restrictions, shared_str add_in_restrictions, shared_str remove_out_restrictions,
                                                   shared_str remove_in_restrictions)
{
    CRestrictionPtr _client_restriction = restriction(id);
    if (!_client_restriction)
    {
        restrict(id, add_out_restrictions, add_in_restrictions);
        return;
    }

    VERIFY(!_client_restriction->applied());

    CClientRestriction& client_restriction = (*m_clients)[id];

    shared_str new_out_restrictions = client_restriction.m_base_out_restrictions;
    shared_str new_in_restrictions = client_restriction.m_base_in_restrictions;

    difference_restrictions(new_out_restrictions, remove_out_restrictions);
    difference_restrictions(new_in_restrictions, remove_in_restrictions);

    join_restrictions(new_out_restrictions, add_out_restrictions);
    join_restrictions(new_in_restrictions, add_in_restrictions);

    restrict(id, new_out_restrictions, new_in_restrictions);
}

void CSpaceRestrictionManager::on_default_restrictions_changed()
{
    // OP22 (28.11 perf): обычные списки разбираем один раз, мусор собираем после всех клиентов.
    if (g_op22_restriction_batch && !m_clients->empty())
    {
        struct ParsedList
        {
            xr_vector<std::string_view> names;
            bool canonical = true;
        };
        string_unordered_map<shared_str, ParsedList> parsed;
        const auto parse = [&parsed](const shared_str& list) -> const ParsedList& {
            auto [it, inserted] = parsed.try_emplace(list);
            auto& result = it->second;
            if (!inserted || list.empty())
                return result;

            std::string_view remaining = it->first;
            for (;;)
            {
                const auto separator = remaining.find(',');
                const auto name = remaining.substr(0, separator);
                if (name.empty() || name.size() >= sizeof(string4096) || std::ranges::any_of(name, [](unsigned char c) { return c <= ' '; }))
                {
                    result.canonical = false;
                    break;
                }
                if (std::ranges::find(result.names, name) == result.names.end())
                    result.names.push_back(name);
                if (separator == std::string_view::npos)
                    break;
                remaining.remove_prefix(separator + 1);
            }
            return result;
        };

        const auto merge = [this, &parse](shared_str base, shared_str defaults, shared_str opposite) {
            const auto& base_list = parse(base);
            const auto& default_list = parse(defaults);
            const auto& opposite_list = parse(opposite);
            if (!base_list.canonical || !default_list.canonical || !opposite_list.canonical)
            {
                difference_restrictions(defaults, opposite);
                join_restrictions(base, defaults);
                return base;
            }

            string4096 merged;
            strcpy_s(merged, *base);
            for (const auto& name : default_list.names)
            {
                if (std::ranges::find(opposite_list.names, name) != opposite_list.names.end() ||
                    std::ranges::find(base_list.names, name) != base_list.names.end())
                    continue;
                if (*merged)
                    strcat_s(merged, ",");
                strncat_s(merged, name.data(), name.size());
            }
            return shared_str(merged);
        };

        const shared_str default_out = default_out_restrictions();
        const shared_str default_in = default_in_restrictions();
        bool expire_on_lookup = false;
        for (const auto& [id, client] : *m_clients)
        {
            const auto& base_out = client.m_base_out_restrictions;
            const auto& base_in = client.m_base_in_restrictions;
            const auto merged_out = merge(base_out, default_out, base_in);
            const auto merged_in = merge(base_in, default_in, base_out);
            set_client_restriction(id, base_out, base_in, merged_out, merged_in, expire_on_lookup);
            expire_on_lookup = true;
        }
        collect_garbage();
        return;
    }

    CLIENT_RESTRICTIONS::const_iterator I = m_clients->begin();
    CLIENT_RESTRICTIONS::const_iterator E = m_clients->end();
    for (; I != E; ++I)
        restrict((*I).first, (*I).second.m_base_out_restrictions, (*I).second.m_base_in_restrictions);
}
