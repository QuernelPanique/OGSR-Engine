#include "stdafx.h"
#include "MosquitoBald.h"
#include "RadioactiveZone.h"
#include "BreakableObject.h"

using namespace luabind;


void CMosquitoBald::script_register(lua_State* L)
{
    module(L)[class_<CMosquitoBald, CGameObject>("CMosquitoBald").def(constructor<>()),
              // OP22: class_registrator.script ОП 2.2 регистрирует скриптовые классы ZS_RADIO и O_BRKBL_S на эти
              // C++-классы (cs_register) - без экспорта регистрация молча не проходит и clsid.*_s пустые
              class_<CRadioactiveZone, CGameObject>("CRadioactiveZone").def(constructor<>()),
              class_<CBreakableObject, CGameObject>("CBreakableObject").def(constructor<>())];
}
