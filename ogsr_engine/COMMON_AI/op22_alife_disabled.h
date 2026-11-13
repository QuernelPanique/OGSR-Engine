#pragma once

#include <luabind/wrapper_base.hpp>
#include <luabind/detail/object_rep.hpp>

// Opt-in CP22 contract: enabled() first rejects a truthy instance.disabled.
// Inspect only the raw instance field. Missing/inherited fields fall back to
// enabled(); no Lua code/metamethod is called, and no value is cached.
inline bool op22_alife_raw_disabled(const luabind::wrap_base& wrapped)
{
    const auto& self = luabind::detail::wrap_access::ref(wrapped);
    lua_State* L = self.state();
    if (!L)
        return false;

    const int top = lua_gettop(L);
    self.get(L);
    bool disabled = false;
    if (lua_type(L, -1) == LUA_TUSERDATA)
    {
        const auto* object = static_cast<const luabind::detail::object_rep*>(lua_touserdata(L, -1));
        const auto& table = object->get_lua_table();
        if (table.is_valid())
        {
            table.get(L);
            if (lua_istable(L, -1))
            {
                lua_pushliteral(L, "disabled");
                lua_rawget(L, -2);
                disabled = lua_toboolean(L, -1) != 0;
            }
        }
    }
    lua_settop(L, top);
    return disabled;
}
