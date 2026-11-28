// Copyright (c) 2003 Daniel Wallin and Arvid Norberg

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
// ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
// ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
// OR OTHER DEALINGS IN THE SOFTWARE.
#include "stdafx.h"

#include <luabind/detail/pcall.hpp>
#include <luabind/error.hpp>

namespace luabind
{
	// OP22 (28.11 perf): переиспользуем обработчик ошибок в пределах одной VM.
	int op22_lua_pcall_cache = 1;
	static unsigned op22_nofatal_count = 0; // OP22
	unsigned get_op22_nofatal_count() { return op22_nofatal_count; }
}

namespace luabind { namespace detail
{
	int pcall(lua_State *L, int nargs, int nresults)
	{
		pcall_callback_fun e = get_pcall_callback();
		int en = 0;
		if ( e )
		{
			int base = lua_gettop(L) - nargs;
			if (op22_lua_pcall_cache)
			{
				// OP22 (28.11 perf): реестр общий для корутин и очищается при lua_close.
				static char error_handler_key;
				lua_pushlightuserdata(L, &error_handler_key);
				lua_rawget(L, LUA_REGISTRYINDEX);
				if (lua_tocfunction(L, -1) != e)
				{
					lua_pop(L, 1);
					lua_pushcfunction(L, e);
					lua_pushlightuserdata(L, &error_handler_key);
					lua_pushvalue(L, -2);
					lua_rawset(L, LUA_REGISTRYINDEX);
				}
			}
			else
				lua_pushcfunction(L, e);
			lua_insert(L, base);  // push pcall_callback under chunk and args
			en = base;
  		}
		int result = lua_pcall(L, nargs, nresults, en);
		if ( en )
			lua_remove(L, en);  // remove pcall_callback
		// OP22: «не падать» - ошибка уже напечатана обработчиком (lua_pcall_failed); убираем сообщение,
		// отдаём nil вместо результатов и успех: вызывающий код luabind не дойдёт до std::terminate
		if (result != 0 && get_op22_nofatal())
		{
			++op22_nofatal_count;
			lua_pop(L, 1);
			for (int i = 0; i < nresults; ++i)
				lua_pushnil(L);
			return 0;
		}
		return result;
	}

	int resume_impl(lua_State *L, int nargs, int)
	{
		return lua_resume(L, nargs);
	}

}}
