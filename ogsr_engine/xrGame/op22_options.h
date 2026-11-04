// OP22: чтение настроек ОП 2.2 из консоли (op22_console.cpp). Нет переменной - значение по умолчанию.
#pragma once
#include "../xr_3da/XR_IOConsole.h"

inline bool op22_opt(LPCSTR name, bool def)
{
    return Console && Console->GetCommand(name) ? !!Console->GetBool(name) : def;
}
