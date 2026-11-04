// OP22: переменные консоли движка ОП 2.2, которых нет в OGSR. Их читает и пишет окно опций ОП
// (ui_mm_opt_creator.script, ui_mm_opt_*.script, options_item в ui_mm_opt.xml) и скрипты ОП (autopause, ui_storage_viewer...).
// Здесь они только хранят значение и сохраняются в user.ltx: опции можно менять, значение не теряется.
// Механики за ними (липкий прицел, панель быстрых слотов, профили шрифтов, язык...) - отдельно (TODO, этап 3).
// Типы и значения по умолчанию - по user.ltx игры на движке ОП.
#include "stdafx.h"
#include "..\xr_3da\XR_IOConsole.h"
#include "..\xr_3da\xr_ioc_cmd.h"
#include "..\xr_3da\CustomHUD.h"
#include "ui_base.h"
#include "HUDManager.h"
#include "../xrCore/op22_skins.h"

namespace
{
Flags32 g_flags{};
u32 g_flag_bits = 0;

void add(IConsole_Command* c)
{
    if (Console->GetCommand(c->Name()))
    {
        Msg("~ [OP22] console: [%s] already exists in OGSR - skipped", c->Name());
        return; // своя переменная OGSR главнее
    }
    Console->AddCommand(c);
}

void flag(LPCSTR name, bool def)
{
    R_ASSERT2(g_flag_bits < 32, "OP22: too many flags");
    const u32 m = 1u << g_flag_bits++;
    g_flags.set(m, def);
    add(xr_new<CCC_Mask>(name, &g_flags, m));
}

void integer(LPCSTR name, int def, int mn, int mx)
{
    int* v = xr_new<int>(def);
    add(xr_new<CCC_Integer>(name, v, mn, mx));
}

void flt(LPCSTR name, float def, float mn, float mx)
{
    float* v = xr_new<float>(def);
    add(xr_new<CCC_Float>(name, v, mn, mx));
}

void str(LPCSTR name, LPCSTR def)
{
    constexpr int size = 256;
    char* v = xr_alloc<char>(size);
    xr_strcpy(v, size, def);
    add(xr_new<CCC_String>(name, v, size));
}

// список значений - ключи секции конфига (languages, lang_codes, lang_fonts, font_profiles); нет секции - одно значение def
void token(LPCSTR name, LPCSTR section, LPCSTR def)
{
    xr_vector<shared_str> names;
    if (pSettings && pSettings->section_exist(section))
    {
        CInifile::Sect& s = pSettings->r_section(section);
        for (const auto& it : s.Ordered_Data) // порядок как в конфиге
            names.push_back(it.first);
    }
    if (names.empty())
        names.push_back(def);
    xr_token* t = xr_alloc<xr_token>(names.size() + 1);
    u32* v = xr_new<u32>(0);
    for (u32 i = 0; i < names.size(); ++i)
    {
        t[i].name = xr_strdup(names[i].c_str());
        t[i].id = int(i);
        if (!xr_strcmp(names[i], def))
            *v = i;
    }
    t[names.size()].name = nullptr;
    t[names.size()].id = 0;
    add(xr_new<CCC_Token>(name, v, t));
}
// ползунок ОП, у которого в OGSR есть аналог: хранит своё значение и передаёт его переменной OGSR
// (гамма/яркость ОП -> шейдерная коррекция изображения ssfx_gamma/ssfx_exposure, pp_img_corrections)
class CCC_OP22Alias : public CCC_Float
{
    LPCSTR m_target;

public:
    CCC_OP22Alias(LPCSTR N, float* V, float mn, float mx, LPCSTR target) : CCC_Float(N, V, mn, mx), m_target(target) {}
    virtual void Execute(LPCSTR args)
    {
        CCC_Float::Execute(args);
        string256 cmd;
        xr_sprintf(cmd, "%s %f", m_target, *value);
        Console->Execute(cmd);
    }
};

void alias(LPCSTR name, float def, float mn, float mx, LPCSTR target)
{
    float* v = xr_new<float>(def);
    add(xr_new<CCC_OP22Alias>(name, v, mn, mx, target));
}
// профиль шрифтов ОП: значения - описания из [font_profiles] (default_fonts, bigger_fonts), ключ профиля - по описанию;
// смена профиля сразу перезагружает шрифты
xr_vector<std::pair<shared_str, shared_str>> g_font_profiles; // описание -> ключ
u32 g_font_profile = 0;

class CCC_OP22FontProfile : public CCC_Token
{
public:
    CCC_OP22FontProfile(LPCSTR N, u32* V, const xr_token* T) : CCC_Token(N, V, T) {}
    virtual void Execute(LPCSTR args)
    {
        const u32 old = *value;
        CCC_Token::Execute(args);
        if (old != *value && g_hud && UI() && UI()->Font())
            UI()->Font()->InitializeFonts();
    }
};

void font_profile()
{
    if (pSettings && pSettings->section_exist("font_profiles"))
        for (const auto& it : pSettings->r_section("font_profiles").Ordered_Data)
            g_font_profiles.emplace_back(it.second.size() ? it.second : it.first, it.first);
    if (g_font_profiles.empty())
        g_font_profiles.emplace_back("default_fonts", "default");
    xr_token* t = xr_alloc<xr_token>(g_font_profiles.size() + 1);
    for (u32 i = 0; i < g_font_profiles.size(); ++i)
    {
        t[i].name = xr_strdup(g_font_profiles[i].first.c_str());
        t[i].id = int(i);
    }
    t[g_font_profiles.size()].name = nullptr;
    t[g_font_profiles.size()].id = 0;
    add(xr_new<CCC_OP22FontProfile>("font_profile", &g_font_profile, t));
}
// скин ОП (g_current_hud / g_current_inventory): задаёт профиль ядру скинов, показывает действующий (с учётом
// профиля по умолчанию) - окно выбора HUD в опциях ОП отмечает верную строку. Смена - после перезапуска игры
class CCC_OP22Skin : public CCC_String
{
    int m_kind;

public:
    CCC_OP22Skin(LPCSTR N, LPSTR V, int size, int kind) : CCC_String(N, V, size), m_kind(kind) {}
    virtual void Execute(LPCSTR args)
    {
        CCC_String::Execute(args);
        op22_skins::set_current(m_kind, value);
    }
    virtual void Status(TStatus& S) { xr_strcpy(S, op22_skins::current(m_kind)); }
};

void skin(LPCSTR name, int kind)
{
    constexpr int size = 128;
    char* v = xr_alloc<char>(size);
    v[0] = 0;
    add(xr_new<CCC_OP22Skin>(name, v, size, kind));
}
} // namespace

LPCSTR op22_font_profile_key() { return g_font_profile < g_font_profiles.size() ? g_font_profiles[g_font_profile].second.c_str() : nullptr; }

void op22_register_console()
{
    extern int g_op22_pda_tab; // OP22: read by op22_skills_page.script
    add(xr_new<CCC_Integer>("op22_pda_tab", &g_op22_pda_tab, 0, 100000));
    // яркость окна опций ОП: гамма, яркость, контраст - через коррекцию изображения OGSR
    alias("rs_c_gamma", 1.f, 0.5f, 1.5f, "ssfx_gamma");
    alias("rs_c_brightness", 1.f, 0.5f, 1.5f, "ssfx_exposure");
    alias("rs_c_contrast", 1.f, 0.5f, 1.5f, "ssfx_contrast"); // ssfx_contrast - форк, рендер (pp_img_corrections.w)
    flag("r2_sun", true);           // в OGSR солнце не отключается
    flag("rs_refresh_60hz", false);
    flag("ext_animations", false); // _g.script ОП выключает при загрузке уровня

    // окно опций ОП: options_item
    token("renderer", "op22_no_section", "st_op22_renderer"); // в OGSR один рендер (DX11) - значение для показа
    font_profile();
    token("g_lang", "languages", "rus");
    token("g_langc", "lang_codes", "east_1251");
    token("g_langf", "lang_fonts", "east");
    integer("g_dialog_wnd_alpha", 255, 0, 255);
    integer("texture_lod", 0, 0, 4);
    integer("r__supersample", 1, 1, 8);
    flag("mm_music", true);
    flag("snd_level_emb_music", true);
    flag("snd_level_emb_sounds", true);
    flt("snd_mm_volume_music", 0.3f, 0.f, 1.f);

    // настройки игры ОП (ui_mm_opt_creator.script) и скрипты ОП
    flag("dialog_numbered", true);
    flag("g_show_relation_in_ui", true);
    flag("gp_deffered_reload", true);
    flag("gp_demand_reload", false);
    flag("gp_fixed_reload", true);
    flag("gp_open_scope", false);
    flag("gp_show_enemy_minimap_spot", true);
    flag("gp_show_modificators", true);
    flag("gp_sticky_scope", false);
    flag("hc_armed_custom", true);
    flag("hud_art_panel_visible", true);
    flag("hud_indicators_visible", true);
    flag("hud_min_crosshair", true);
    flag("hud_qs_panel_visible", true);
    flag("hud_qsp_empty_slot_visible", true);
    flag("hud_weapon", true);
    flag("ls_show_progress", true);
    flag("ui_allow_operations_tsb", false);
    flag("ui_show_conditions", true);
    flag("ui_show_extdesc", true);
    flag("ui_show_focused", true);
    flag("ui_show_selected", false);
    flag("ui_show_trade_sb", true);
    integer("g_fly_show_info_delay", 1000, 0, 100000);
    integer("hc_armed_scale_type", 1, 0, 10);
    integer("hud_qspanel_anim_time", 2000, 0, 8000);
    flt("ls_text_offset_x", 30.f, -2000.f, 2000.f);
    flt("ls_text_offset_y", 90.f, -2000.f, 2000.f);
    flt("r2_sun_near_border", 1.f, 0.f, 1.f);
    {
        extern float g_op22_hud_inertion;
        add(xr_new<CCC_Float>("op22_hud_inertion", &g_op22_hud_inertion, 0.f, 300.f));
    }
    str("bolt_visual", "bolt");
    str("hc_unarmed", "");
    skin("g_current_hud", op22_skins::eHud);
    skin("g_current_inventory", op22_skins::eInventory);
    str("buy_item", "nil");
}
