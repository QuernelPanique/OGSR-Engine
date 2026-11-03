#include "stdafx.h"
#include "UIPropertiesBox.h"

using namespace luabind;

// OP22: методы меню ОП 2.2 (ui/op22_ui_dragdrop_script.cpp)
extern void op22_pb_InsertItem(CUIPropertiesBox* box, int pos, LPCSTR str, const luabind::functor<void>& f);
extern void op22_pb_RemoveItemByTAG(CUIPropertiesBox* box, u32 tag);


void CUIPropertiesBox::script_register(lua_State* L)
{
    module(L)[class_<CUIPropertiesBox, CUIFrameWindow>("CUIPropertiesBox")
                  .def(constructor<>())
                  //		.def("AddItem",					&CUIPropertiesBox::AddItem)
                  .def("RemoveItem", &CUIPropertiesBox::RemoveItemByTAG)
                  .def("RemoveAll", &CUIPropertiesBox::RemoveAll)
                  .def("Show", (void(CUIPropertiesBox::*)(int, int)) & CUIPropertiesBox::Show)
                  .def("Hide", &CUIPropertiesBox::Hide)
                  //		.def("GetClickedIndex",		&CUIPropertiesBox::GetClickedIndex)
                  .def("AutoUpdateSize", &CUIPropertiesBox::AutoUpdateSize)
                  .def("AddItem", &CUIPropertiesBox::AddItem_script)
                  .def("InsertItem", &op22_pb_InsertItem)          // OP22
                  .def("RemoveItemByTAG", &op22_pb_RemoveItemByTAG) // OP22
              //		.def("",					&CUIPropertiesBox::)
    ];
}
