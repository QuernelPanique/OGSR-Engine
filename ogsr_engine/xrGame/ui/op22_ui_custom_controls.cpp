// OP22: контролы окна опций ОП 2.2 (ui_mm_opt_creator.script, ui_mm_opt_huds.script, ui_mm_opt_crosshairs.script,
// ui_matras.script): CUITrackBarCustom и CUISpinTextCustom (op22_ui_custom_controls.h). В отличие от CUITrackBar/CUISpinText
// они не связаны с консольной переменной - значение задаёт и читает скрипт. Плюс CScriptXmlInit:InitTrackBarCustom/InitSpinTextCustom.
#include "StdAfx.h"
#include "op22_ui_custom_controls.h"
#include "UIXmlInit.h"
#include "../ScriptXMLInit.h"
#include "UITextureMaster.h"
#include "../../xrCore/op22_skins.h"

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

// OP22: скины интерфейса ОП для скриптов (ui_mm_opt_huds.script, ui_mm_opt_creator.script): get_multihuds(),
// get_multiinvs() -> CMultiUIConfigs (profiles, current, enabled, ReloadUITex); профиль - CUIConfigProfile
struct op22_ui_profile
{
    int kind{};
    shared_str name;

    LPCSTR get_name() const { return name.c_str(); }
    LPCSTR get_folder() const { return name.c_str(); }
    xr_string text() const
    {
        xr_string t;
        op22_skins::read_text(kind, name.c_str(), "description.txt", t);
        return t;
    }
    LPCSTR get_short_description() const
    {
        static xr_string r;
        r = text();
        const size_t e = r.find_first_of("\r\n");
        if (e != xr_string::npos)
            r.resize(e);
        while (!r.empty() && (r.back() == ' ' || r.back() == '\t'))
            r.pop_back();
        if (r.empty())
            r = name.c_str();
        return r.c_str();
    }
    LPCSTR get_full_description() const
    {
        static xr_string r;
        r = text();
        r.erase(std::remove(r.begin(), r.end(), '\r'), r.end());
        return r.c_str();
    }
    // превью - первая .dds в корне профиля: op22skin\<hud|inv>\<профиль>\<файл> (ядро скинов найдёт файл)
    LPCSTR get_preview_texture() const
    {
        static string_path r;
        r[0] = 0;
        FS_FileSet fs;
        FS.file_list(fs, op22_skins::alias(kind), FS_ListFiles, "*.dds");
        xr_string pfx = name.c_str();
        pfx += "\\";
        for (const auto& f : fs)
        {
            xr_string n = f.name;
            if (n.size() <= pfx.size() || _strnicmp(n.c_str(), pfx.c_str(), pfx.size()))
                continue;
            n = n.substr(pfx.size());
            if (n.find('\\') != xr_string::npos || n.size() <= 4) // только корень профиля
                continue;
            n.resize(n.size() - 4);
            xr_sprintf(r, "op22skin\\%s\\%s\\%s", kind ? "inv" : "hud", name.c_str(), n.c_str());
            break;
        }
        return r;
    }
};

struct op22_multi_ui
{
    int kind{};
    bool enabled() const
    {
        xr_vector<shared_str> p;
        op22_skins::profiles(kind, p);
        return !p.empty();
    }
    luabind::object profiles() const
    {
        lua_State* L = ai().script_engine().lua();
        luabind::object t = luabind::newtable(L);
        xr_vector<shared_str> p;
        op22_skins::profiles(kind, p);
        int i = 1;
        for (const auto& n : p)
            t[i++] = op22_ui_profile{kind, n};
        return t;
    }
    op22_ui_profile current() const { return op22_ui_profile{kind, op22_skins::current(kind)}; }
    // текстуры уже загруженных окон меняются после перезапуска; описание текстур - сразу
    void ReloadUITex(LPCSTR file)
    {
        if (file && *file && xr_strcmp(file, "dummy.xml"))
            CUITextureMaster::ParseShTexInfo(file);
    }
    void reload() {}
};

static op22_multi_ui op22_get_multihuds() { return op22_multi_ui{op22_skins::eHud}; }
static op22_multi_ui op22_get_multiinvs() { return op22_multi_ui{op22_skins::eInventory}; }

void op22_register_skins(lua_State* L)
{
    module(L)[class_<op22_ui_profile>("CUIConfigProfile")
                  .def("get_name", &op22_ui_profile::get_name)
                  .def("get_folder", &op22_ui_profile::get_folder)
                  .def("get_short_description", &op22_ui_profile::get_short_description)
                  .def("get_full_description", &op22_ui_profile::get_full_description)
                  .def("get_preview_texture", &op22_ui_profile::get_preview_texture),
              class_<op22_multi_ui>("CMultiUIConfigs")
                  .def("enabled", &op22_multi_ui::enabled)
                  .def("profiles", &op22_multi_ui::profiles)
                  .def("current", &op22_multi_ui::current)
                  .def("ReloadUITex", &op22_multi_ui::ReloadUITex)
                  .def("reload", &op22_multi_ui::reload),
              def("get_multihuds", &op22_get_multihuds), def("get_multiinvs", &op22_get_multiinvs)];
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
    op22_register_skins(L); // OP22: скины ОП
}

