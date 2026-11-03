// OP22: контролы окна опций ОП 2.2 (ui_mm_opt_creator.script, ui_mm_opt_huds.script, ui_mm_opt_crosshairs.script,
// ui_matras.script): CUITrackBarCustom и CUISpinTextCustom (op22_ui_custom_controls.h). В отличие от CUITrackBar/CUISpinText
// они не связаны с консольной переменной - значение задаёт и читает скрипт. Плюс CScriptXmlInit:InitTrackBarCustom/InitSpinTextCustom.
#include "StdAfx.h"
#include "op22_ui_custom_controls.h"
#include "UIXmlInit.h"
#include "../ScriptXMLInit.h"

using namespace luabind;

static void attach(CUIWindow* w, CUIWindow* parent)
{
    w->SetAutoDelete(true);
    if (parent)
        parent->AttachChild(w);
}

CUITrackBarCustom* op22_xml_InitTrackBarCustom(CScriptXmlInit* self, LPCSTR path, CUIWindow* parent)
{
    CUITrackBarCustom* w = xr_new<CUITrackBarCustom>();
    CUIXmlInit::InitTrackBar(self->op22_xml(), path, 0, w);
    w->Normalize();
    attach(w, parent);
    return w;
}

CUISpinTextCustom* op22_xml_InitSpinTextCustom(CScriptXmlInit* self, LPCSTR path, CUIWindow* parent)
{
    CUISpinTextCustom* w = xr_new<CUISpinTextCustom>();
    CUIXmlInit::InitSpin(self->op22_xml(), path, 0, w);
    attach(w, parent);
    return w;
}

// вызывается из CUI3tButton::script_register (UIButton_script.cpp) - после регистрации CUITrackBar/CUISpinText
void op22_register_custom_controls(lua_State* L)
{
    module(L)[class_<CUITrackBarCustom, CUITrackBar>("CUITrackBarCustom")
                  .def(constructor<>())
                  .def("SetValue", &CUITrackBarCustom::SetValue)
                  .def("GetValue", &CUITrackBarCustom::GetValue)
                  .def("SetStep", &CUITrackBarCustom::SetStepCustom)
                  .def("SetType", &CUITrackBarCustom::SetTypeCustom)
                  .def("SetMaxValue", &CUITrackBarCustom::SetMaxValue)
                  .def("SetMinValue", &CUITrackBarCustom::SetMinValue)
                  .def("ShowDynamicValue", &CUITrackBarCustom::ShowDynamicValue),

              class_<CUISpinTextCustom, CUISpinText>("CUISpinTextCustom")
                  .def(constructor<>())
                  .def("AddItem", &CUISpinTextCustom::AddItem)
                  .def("GetSelectedId", &CUISpinTextCustom::GetSelectedId)
                  .def("SetSelectedId", &CUISpinTextCustom::SetSelectedId)
                  .def("GetSelectedText", &CUISpinTextCustom::GetSelectedText)
                  .def("SetSelectedText", &CUISpinTextCustom::SetSelectedText)
                  .def("GetIdByIndex", &CUISpinTextCustom::GetIdByIndex)
                  .def("GetTextByIndex", &CUISpinTextCustom::GetTextByIndex)
                  .def("SetSpinButtonCallback", &CUISpinTextCustom::SetSpinButtonCallback)];
}

