#include "stdafx.h"
#include "../ExoOutfit.h" // OP22
#include "../op22_options.h" // OP22
#include "../ai_space.h" // OP22
#include "../script_engine.h" // OP22
#include "../Inventory.h" // OP22
#include "../xr_level_controller.h" // OP22
#include "UIFrameWindow.h" // OP22

#include "UIMainIngameWnd.h"
#include "UIMessagesWindow.h"
#include "../UIZoneMap.h"

#include <dinput.h>
#include "../actor.h"
#include "../HUDManager.h"
#include "../PDA.h"
#include "../character_info.h"
#include "../inventory.h"
#include "../UIGameSP.h"
#include "../weaponmagazined.h"
#include "../missile.h"
#include "../Grenade.h"
#include "../xrServer_objects_ALife.h"
#include "../alife_simulator.h"
#include "../alife_object_registry.h"
#include "../game_cl_base.h"
#include "../level.h"
#include "../seniority_hierarchy_holder.h"

#include "../date_time.h"
#include "../xrServer_Objects_ALife_Monsters.h"
#include "../../xr_3da/LightAnimLibrary.h"

#include "UIInventoryUtilities.h"

#include "UIXmlInit.h"
#include "UIPdaMsgListItem.h"
#include "../alife_registry_wrappers.h"
#include "../actorcondition.h"

#include "../string_table.h"
#include "clsid_game.h"
#include "UIArtefactPanel.h"
#include "UIMap.h"

#ifdef DEBUG
#include "../attachable_item.h"
#include "..\..\xr_3da\xr_input.h"
#endif

#include "UIScrollView.h"
#include "map_hint.h"
#include "UIColorAnimatorWrapper.h"
#include "../game_news.h"
#include "../xr_3da/xr_input.h"

using namespace InventoryUtilities;

#define DEFAULT_MAP_SCALE 1.f
#define MAININGAME_XML "maingame.xml"

static CUIMainIngameWnd* GetMainIngameWindow()
{
    if (g_hud)
    {
        CUI* pUI = g_hud->GetUI();
        if (pUI)
            return pUI->UIMainIngameWnd;
    }
    return nullptr;
}

static CUIStatic* warn_icon_list[8]{};

// alpet: для возможности внешнего контроля иконок (используется в NLC6 вместо типичных индикаторов). Никак не влияет на игру для остальных модов.
static bool external_icon_ctrl = false;

// позволяет расцветить иконку или изменить её размер
static bool SetupGameIcon(CUIMainIngameWnd::EWarningIcons icon, u32 cl, float width, float height)
{
    auto window = GetMainIngameWindow();
    if (!window)
    {
        Msg("!![SetupGameIcon] failed due GetMainIngameWindow() returned NULL");
        return false;
    }

    R_ASSERT(icon > 0 && icon < std::size(warn_icon_list), "!!Invalid first arg for setup_game_icon!");

    CUIStatic* sIcon = warn_icon_list[icon];

    if (width > 0 && height > 0)
    {
        sIcon->SetWidth(width);
        sIcon->SetHeight(height);
        sIcon->SetStretchTexture(cl > 0);
    }
    else
        window->SetWarningIconColor(icon, cl);

    external_icon_ctrl = true;
    return true;
}

CUIMainIngameWnd::CUIMainIngameWnd()
{
    m_pActor = NULL;
    m_pWeapon = NULL;
    m_pGrenade = NULL;
    m_pItem = NULL;
    UIZoneMap = xr_new<CUIZoneMap>();
    m_pPickUpItem = NULL;

    warn_icon_list[ewiWeaponJammed] = &UIWeaponJammedIcon;
    warn_icon_list[ewiRadiation] = &UIRadiaitionIcon;
    warn_icon_list[ewiWound] = &UIWoundIcon;
    warn_icon_list[ewiStarvation] = &UIStarvationIcon;
    warn_icon_list[ewiPsyHealth] = &UIPsyHealthIcon;
    warn_icon_list[ewiInvincible] = &UIInvincibleIcon;
    warn_icon_list[ewiThirst] = &UIThirstIcon;
}

#include "UIProgressShape.h"
extern CUIProgressShape* g_MissileForceShape;

CUIMainIngameWnd::~CUIMainIngameWnd()
{
    DestroyFlashingIcons();
    xr_delete(UIZoneMap);
    if (m_artefactPanel)
        xr_delete(m_artefactPanel);
    HUD_SOUND::DestroySound(m_contactSnd);
    xr_delete(g_MissileForceShape);
}

void CUIMainIngameWnd::Init()
{
    CUIXml uiXml;
    uiXml.Init(CONFIG_PATH, UI_PATH, MAININGAME_XML);

    CUIXmlInit xml_init;
    CUIWindow::Init(0, 0, UI_BASE_WIDTH, UI_BASE_HEIGHT);

    Enable(false);

    AttachChild(&UIStaticHealth);
    xml_init.InitStatic(uiXml, "static_health", 0, &UIStaticHealth);

    AttachChild(&UIStaticArmor);
    xml_init.InitStatic(uiXml, "static_armor", 0, &UIStaticArmor);

    AttachChild(&UIWeaponBack);
    xml_init.InitStatic(uiXml, "static_weapon", 0, &UIWeaponBack);

    UIWeaponBack.AttachChild(&UIWeaponSignAmmo);
    xml_init.InitStatic(uiXml, "static_ammo", 0, &UIWeaponSignAmmo);
    UIWeaponSignAmmo.SetElipsis(CUIStatic::eepEnd, 2);

    // OP22: GAMMA 2.0 - патроны в магазине и остаток отдельно («22» и «/ 90»), узлы - дети static_weapon
    for (auto [node, st] : {std::pair<LPCSTR, CUIStatic**>{"op22_ammo_mag", &m_op22_ammo_mag}, {"op22_ammo_rest", &m_op22_ammo_rest}})
    {
        if (!uiXml.NavigateToNode(node, 0))
            continue;
        *st = xr_new<CUIStatic>();
        (*st)->SetAutoDelete(true);
        UIWeaponBack.AttachChild(*st);
        xml_init.InitStatic(uiXml, node, 0, *st);
    }

    UIWeaponBack.AttachChild(&UIWeaponIcon);
    xml_init.InitStatic(uiXml, "static_wpn_icon", 0, &UIWeaponIcon);
    UIWeaponIcon.SetShader(GetEquipmentIconsShader());
    UIWeaponIcon_rect = UIWeaponIcon.GetWndRect();
    //---------------------------------------------------------
    AttachChild(&UIPickUpItemIcon);
    xml_init.InitStatic(uiXml, "pick_up_item", 0, &UIPickUpItemIcon);
    UIPickUpItemIcon.SetShader(GetEquipmentIconsShader());
    //	UIPickUpItemIcon.ClipperOn	();
    UIPickUpItemIcon.Show(false);

    m_iPickUpItemIconWidth = UIPickUpItemIcon.GetWidth();
    m_iPickUpItemIconHeight = UIPickUpItemIcon.GetHeight();
    m_iPickUpItemIconX = UIPickUpItemIcon.GetWndRect().left;
    m_iPickUpItemIconY = UIPickUpItemIcon.GetWndRect().top;
    //---------------------------------------------------------

    UIWeaponIcon.Enable(false);

    //индикаторы
    UIZoneMap->Init();
    UIZoneMap->SetScale(DEFAULT_MAP_SCALE);

    xml_init.InitStatic(uiXml, "static_pda_online", 0, &UIPdaOnline);
    UIZoneMap->Background()->AttachChild(&UIPdaOnline);

    //Полоса прогресса здоровья
    UIStaticHealth.AttachChild(&UIHealthBar);
    //.	xml_init.InitAutoStaticGroup(uiXml,"static_health", &UIStaticHealth);
    xml_init.InitProgressBar(uiXml, "progress_bar_health", 0, &UIHealthBar);

    //Полоса прогресса армора
    UIStaticArmor.AttachChild(&UIArmorBar);
    //.	xml_init.InitAutoStaticGroup(uiXml,"static_armor", &UIStaticArmor);
    xml_init.InitProgressBar(uiXml, "progress_bar_armor", 0, &UIArmorBar);

    // Подсказки, которые возникают при наведении прицела на объект
    AttachChild(&UIStaticQuickHelp);
    xml_init.InitStatic(uiXml, "quick_info", 0, &UIStaticQuickHelp);

    uiXml.SetLocalRoot(uiXml.GetRoot());

    m_UIIcons = xr_new<CUIScrollView>();
    m_UIIcons->SetAutoDelete(true);
    xml_init.InitScrollView(uiXml, "icons_scroll_view", 0, m_UIIcons);
    AttachChild(m_UIIcons);
    m_op22_icons_horz = !!uiXml.ReadAttribInt("icons_scroll_view", 0, "op22_horz", 0); // OP22
    m_op22_icons_gap = uiXml.ReadAttribFlt("icons_scroll_view", 0, "op22_gap", 0.f);

    // Загружаем иконки
    xml_init.InitStatic(uiXml, "starvation_static", 0, &UIStarvationIcon);
    UIStarvationIcon.Show(false);

    xml_init.InitStatic(uiXml, "psy_health_static", 0, &UIPsyHealthIcon);
    UIPsyHealthIcon.Show(false);

    xml_init.InitStatic(uiXml, "weapon_jammed_static", 0, &UIWeaponJammedIcon);
    UIWeaponJammedIcon.Show(false);

    // OP22: заряд экзоскелета; в HUD порта узла нет - размер от значка оружия, текстура ОП
    if (uiXml.NavigateToNode("exo_charge_static", 0))
        xml_init.InitStatic(uiXml, "exo_charge_static", 0, &UIExoChargeIcon);
    else
    {
        xml_init.InitStatic(uiXml, "weapon_jammed_static", 0, &UIExoChargeIcon);
        UIExoChargeIcon.InitTexture("ui_hud_icon_exo_charge");
    }
    UIExoChargeIcon.Show(false);
    if (pSettings->line_exist("main_ingame_indicators_thresholds", "exo_charge"))
    {
        const char* rec = pSettings->r_string("main_ingame_indicators_thresholds", "exo_charge");
        string32 tmp;
        m_op22_exo_min = (float)atof(_GetItem(rec, 0, tmp));
        m_op22_exo_max = (float)atof(_GetItem(rec, _GetItemCount(rec) - 1, tmp));
    }

    xml_init.InitStatic(uiXml, "radiation_static", 0, &UIRadiaitionIcon);
    UIRadiaitionIcon.Show(false);

    xml_init.InitStatic(uiXml, "wound_static", 0, &UIWoundIcon);
    UIWoundIcon.Show(false);

    xml_init.InitStatic(uiXml, "invincible_static", 0, &UIInvincibleIcon);
    UIInvincibleIcon.Show(false);

    if (Core.Features.test(xrCore::Feature::actor_thirst))
    {
        xml_init.InitStatic(uiXml, "thirst_static", 0, &UIThirstIcon);
        UIThirstIcon.Show(false);
    }

    constexpr const char* warningStrings[] = {
        "jammed",     "radiation", "wounds", "starvation",
        "fatigue", // PsyHealth ???
        "invincible", // Not used
        "thirst",
    };

    // Загружаем пороговые значения для индикаторов
    EWarningIcons i = ewiWeaponJammed;
    while (i <= (Core.Features.test(xrCore::Feature::actor_thirst) ? ewiThirst : ewiInvincible))
    {
        // Читаем данные порогов для каждого индикатора
        const char* cfgRecord = pSettings->r_string("main_ingame_indicators_thresholds", warningStrings[static_cast<int>(i) - 1]);
        u32 count = _GetItemCount(cfgRecord);

        char singleThreshold[8];
        float f = 0;
        for (u32 k = 0; k < count; ++k)
        {
            _GetItem(cfgRecord, k, singleThreshold);
            sscanf(singleThreshold, "%f", &f);

            m_Thresholds[i].push_back(f);
        }

        i = static_cast<EWarningIcons>(i + 1);

        if (i == ewiInvincible)
            i = static_cast<EWarningIcons>(i + 1);
    }

    // Flashing icons initialize
    uiXml.SetLocalRoot(uiXml.NavigateToNode("flashing_icons"));
    InitFlashingIcons(&uiXml);

    uiXml.SetLocalRoot(uiXml.GetRoot());

    AttachChild(&UICarPanel);
    xml_init.InitWindow(uiXml, "car_panel", 0, &UICarPanel);

    AttachChild(&UIMotionIcon);
    UIMotionIcon.Init();

    op22_qs_init(uiXml); // OP22

    if (uiXml.NavigateToNode("artefact_panel"))
    {
        m_artefactPanel = xr_new<CUIArtefactPanel>();
        m_artefactPanel->InitFromXML(uiXml, "artefact_panel", 0);
        this->AttachChild(m_artefactPanel);
    }

    HUD_SOUND::LoadSound("maingame_ui", "snd_new_contact", m_contactSnd, SOUND_TYPE_IDLE);
}

float UIStaticDiskIO_start_time = 0.0f;

void CUIMainIngameWnd::Draw()
{
    if (!m_pActor)
        return;

    UIMotionIcon.SetNoise((s16)(0xffff & iFloor(m_pActor->m_snd_noise * 100.0f)));
    CUIWindow::Draw();
    UIZoneMap->Render();

    RenderQuickInfos();
}

void CUIMainIngameWnd::SetAmmoIcon(const shared_str& sect_name)
{
    if (!sect_name.size())
    {
        UIWeaponIcon.Show(false);
        return;
    };

    UIWeaponIcon.Show(true);
    // properties used by inventory menu
    CIconParams icon_params(sect_name);

    icon_params.set_shader(&UIWeaponIcon);

    float iGridWidth = icon_params.grid_width;

    float w = std::clamp(iGridWidth, 1.f, 2.f) * INV_GRID_WIDTH;
    float h = INV_GRID_HEIGHT;
    w *= UI()->get_current_kx();

    // OP22: как в ОП 2.2 - иконка вписывается в static_wpn_icon разметки HUD (пропорции, по центру); OGSR ставил
    // размер ячейки инвентаря (50 на клетку) - в скинах ОП с маленьким окном (gamma_*: 46x34) иконка вылезала
    const float rw = UIWeaponIcon_rect.width(), rh = UIWeaponIcon_rect.height();
    if (rw > 1.f && rh > 1.f)
    {
        const float k = _min(rw / w, rh / h);
        w *= k;
        h *= k;
        UIWeaponIcon.SetStretchTexture(true);
        UIWeaponIcon.SetWndPos(UIWeaponIcon_rect.x1 + (rw - w) / 2.f, UIWeaponIcon_rect.y1 + (rh - h) / 2.f);
        UIWeaponIcon.SetWidth(w);
        UIWeaponIcon.SetHeight(h);
        return;
    }

    float x = UIWeaponIcon_rect.x1;
    if (iGridWidth < 2.f)
        x += w / 2.0f;

    UIWeaponIcon.SetWndPos(x, UIWeaponIcon_rect.y1);

    UIWeaponIcon.SetWidth(w);
    UIWeaponIcon.SetHeight(h);
};

// OP22: панель быстрых слотов: ячейка с рамкой, иконка, клавиша и количество. Секции слотов - op22_quick.script
void CUIMainIngameWnd::op22_qs_init(CUIXml& xml)
{
    constexpr LPCSTR P = "quickslots_panel";
    if (!xml.NavigateToNode(P, 0))
        return;
    m_op22_qs_cell.set(xml.ReadAttribFlt(P, 0, "cell_width", 26.f), xml.ReadAttribFlt(P, 0, "cell_height", 34.f));
    const float indent = xml.ReadAttribFlt(P, 0, "indent", 2.f);
    m_op22_qs_icon_k = xml.ReadAttribFlt(P, 0, "op22_icon_k", 1.f);
    const shared_str tex = xml.Read("quickslots_panel:slot:texture", 0, "");

    auto label = [&](LPCSTR path, CUIWindow* parent, float def_x, float def_y) {
        CUIStatic* s = xr_new<CUIStatic>();
        s->SetAutoDelete(true);
        parent->AttachChild(s);
        s->Init(xml.ReadAttribFlt(path, 0, "x", def_x), xml.ReadAttribFlt(path, 0, "y", def_y), xml.ReadAttribFlt(path, 0, "width", 20.f),
                xml.ReadAttribFlt(path, 0, "height", 10.f));
        s->SetFont(UI()->Font()->pFontSmall);
        s->SetTextColor(0xffc8c8c8);
        // OP22: GAMMA 2.0 - шрифт, цвет и выравнивание подписи из <text> узла (как у статиков), без узла - как раньше
        string256 tp;
        strconcat(sizeof(tp), tp, path, ":text");
        if (xml.NavigateToNode(tp, 0))
        {
            u32 color = 0;
            CGameFont* fnt = nullptr;
            CUIXmlInit::InitFont(xml, tp, 0, color, fnt);
            if (fnt)
                s->SetFont(fnt);
            if (color)
                s->SetTextColor(color);
            LPCSTR al = xml.ReadAttrib(tp, 0, "align", "l");
            s->SetTextAlignment(!xr_strcmp(al, "r") ? CGameFont::alRight : (!xr_strcmp(al, "c") ? CGameFont::alCenter : CGameFont::alLeft));
        }
        return s;
    };

    m_op22_qs_wnd = xr_new<CUIWindow>();
    m_op22_qs_wnd->SetAutoDelete(true);
    AttachChild(m_op22_qs_wnd);
    m_op22_qs_wnd->SetWndPos(Fvector2().set(xml.ReadAttribFlt(P, 0, "x", 0.f), xml.ReadAttribFlt(P, 0, "y", 0.f)));
    m_op22_qs_wnd->SetWndSize(Fvector2().set((m_op22_qs_cell.x + indent) * 5, m_op22_qs_cell.y));

    string_path probe;
    xr_sprintf(probe, "%s_lt.dds", tex.size() ? tex.c_str() : "");
    const bool frame = tex.size() && (strstr(tex.c_str(), "ct_gamma_qs") || FS.exist("$game_textures$", probe)); // рамка из 9 текстур
    for (int i = 0; i < 5; ++i)
    {
        Op22QuickSlot& q = m_op22_qs[i];
        const float x = (m_op22_qs_cell.x + indent) * i;
        if (frame)
        {
            auto* f = xr_new<CUIFrameWindow>();
            f->SetAutoDelete(true);
            m_op22_qs_wnd->AttachChild(f);
            f->Init(tex.c_str(), x, 0.f, m_op22_qs_cell.x, m_op22_qs_cell.y);
            q.back = f;
        }
        else
        {
            auto* s = xr_new<CUIStatic>();
            s->SetAutoDelete(true);
            m_op22_qs_wnd->AttachChild(s);
            s->Init(x, 0.f, m_op22_qs_cell.x, m_op22_qs_cell.y);
            if (tex.size())
            {
                s->InitTexture(tex.c_str());
                s->SetStretchTexture(true);
            }
            q.back = s;
        }
        q.icon = xr_new<CUIStatic>();
        q.icon->SetAutoDelete(true);
        q.back->AttachChild(q.icon);
        q.icon->Init(0.f, 0.f, m_op22_qs_cell.x, m_op22_qs_cell.y);
        q.icon->SetStretchTexture(true);
        q.key = label("quickslots_panel:slot:label_key", q.back, 2.f, 1.f);
        q.count = label("quickslots_panel:slot:label_count", q.back, 16.f, 24.f);
    }
}

void CUIMainIngameWnd::op22_qs_update()
{
    if (!m_op22_qs_wnd || !m_pActor || Device.dwTimeGlobal < m_op22_qs_time)
        return;
    m_op22_qs_time = Device.dwTimeGlobal + 200;

    luabind::functor<LPCSTR> f;
    const bool panel = op22_opt("hud_qs_panel_visible", true) && ai().script_engine().functor("op22_quick.section", f);
    m_op22_qs_wnd->Show(panel);
    if (!panel)
        return;
    const bool show_empty = op22_opt("hud_qsp_empty_slot_visible", true);
    CInventory& inv = m_pActor->inventory();
    for (int i = 0; i < 5; ++i)
    {
        Op22QuickSlot& q = m_op22_qs[i];
        LPCSTR sect = f(i);
        PIItem first = nullptr;
        u32 cnt = 0;
        if (sect && sect[0])
        {
            for (const TIItemContainer* list : {&inv.m_ruck, &inv.m_belt})
                for (PIItem it : *list)
                    if (it->object().cNameSect() == sect)
                    {
                        if (!first)
                            first = it;
                        ++cnt;
                    }
        }
        q.back->Show(first || show_empty);
        q.icon->Show(first != nullptr);
        q.count->Show(first != nullptr);
        if (first)
        {
            first->m_icon_params.set_shader(q.icon);
            const Frect r = first->m_icon_params.original_rect();
            q.icon->TextureOn();
            q.icon->ClipperOn();
            Frect v = {0.f, 0.f, r.width(), r.height()};
            v.x2 *= UI()->get_current_kx();
            q.icon->GetUIStaticItem().SetRect(v);
            // иконка вписывается в ячейку с сохранением пропорций
            const float k = m_op22_qs_icon_k * _min(m_op22_qs_cell.x / _max(v.width(), 1.f), m_op22_qs_cell.y / _max(v.height(), 1.f)); // OP22: op22_icon_k - доля ячейки (GAMMA 2.0)
            const Fvector2 sz{v.width() * k, v.height() * k};
            q.icon->SetWndPos(Fvector2().set((m_op22_qs_cell.x - sz.x) / 2, (m_op22_qs_cell.y - sz.y) / 2));
            q.icon->SetWndSize(sz);
            string16 t;
            xr_sprintf(t, "%u", cnt);
            q.count->SetText(t);
        }
        LPCSTR kn = dik_to_keyname(get_action_dik(EGameActions(kUSE_QUICK_SLOT0 + i)));
        LPCSTR kshort = kn && kn[0] == 'k' ? kn + 1 : kn; // kINSERT -> INSERT
        // OP22: GAMMA 2.0 - короткие имена клавиш, как на макете (INS, DEL, PGUP...)
        static const std::pair<LPCSTR, LPCSTR> abbr[] = {{"INSERT", "INS"}, {"DELETE", "DEL"}, {"PGUP", "PGUP"}, {"PRIOR", "PGUP"}, {"NEXT", "PGDN"}, {"PGDN", "PGDN"}};
        if (kshort)
            for (const auto& [from, to] : abbr)
                if (!xr_strcmp(kshort, from))
                    kshort = to;
        q.key->SetText(kshort);
    }
}

// OP22: GAMMA 2.0 - значки состояния (и окна скриптов в том же списке) строкой слева направо с зазором
void CUIMainIngameWnd::op22_icons_layout()
{
    if (!m_op22_icons_horz || !m_UIIcons)
        return;
    float x = 0.f;
    for (CUIWindow* w : m_UIIcons->op22_pad()->GetChildWndList())
    {
        if (!w->IsShown())
            continue;
        w->SetWndPos(Fvector2().set(x, 0.f));
        x += w->GetWidth() + m_op22_icons_gap;
    }
}

void CUIMainIngameWnd::Update()
{
    op22_qs_update(); // OP22
    op22_icons_layout();
    m_pActor = smart_cast<CActor*>(Level().CurrentViewEntity());
    if (!m_pActor)
    {
        m_pItem = NULL;
        m_pWeapon = NULL;
        m_pGrenade = NULL;
        CUIWindow::Update();
        return;
    }

    if (!(Device.dwFrame % 30))
    {
        string256 text_str;
        CPda* _pda = m_pActor->GetPDA();
        u32 _cn = 0;
        if (_pda && 0 != (_cn = _pda->ActiveContactsNum()))
        {
            sprintf_s(text_str, "%d", _cn);
            UIPdaOnline.SetText(text_str);
        }
        else
        {
            UIPdaOnline.SetText("");
        }
    };

    if (!(Device.dwFrame % 5))
    {
        if (!(Device.dwFrame % 30))
        {
            bool b_God = GodMode();
            if (b_God)
                SetWarningIconColor(ewiInvincible, 0xffffffff);
            else if (!external_icon_ctrl)
                TurnOffWarningIcon(ewiInvincible);
        }

        // Armor indicator stuff
        PIItem pItem = m_pActor->inventory().ItemFromSlot(OUTFIT_SLOT);
        if (pItem)
        {
            UIArmorBar.Show(true);
            UIStaticArmor.Show(true);
            UIArmorBar.SetProgressPos(pItem->GetCondition() * 100);
        }
        else
        {
            UIArmorBar.Show(false);
            UIStaticArmor.Show(false);
        }

        UpdateActiveItemInfo();

        EWarningIcons i = ewiWeaponJammed;
        while (!external_icon_ctrl && i <= (Core.Features.test(xrCore::Feature::actor_thirst) ? ewiThirst : ewiInvincible))
        {
            float value{};
            switch (i)
            {
                // radiation
            case ewiRadiation: value = m_pActor->conditions().GetRadiation(); break;
            case ewiWound: value = m_pActor->conditions().BleedingSpeed(); break;
            case ewiWeaponJammed:
                if (m_pWeapon)
                    value = 1 - m_pWeapon->GetConditionToShow();
                break;
            case ewiStarvation: value = 1 - m_pActor->conditions().GetSatiety(); break;
            case ewiThirst: value = 1 - m_pActor->conditions().GetThirst(); break;
            case ewiPsyHealth: value = 1 - m_pActor->conditions().GetPsyHealth(); break;
            default: R_ASSERT(!"Unknown type of warning icon");
            }

            // Сначала проверяем на точное соответсвие
            auto rit = std::find(m_Thresholds[i].rbegin(), m_Thresholds[i].rend(), value);

            // Если его нет, то берем последнее меньшее значение ()
            if (rit == m_Thresholds[i].rend())
                rit = std::find_if(m_Thresholds[i].rbegin(), m_Thresholds[i].rend(), std::bind(std::less<float>(), std::placeholders::_1, value));

            if (rit != m_Thresholds[i].rend())
            {
                // Минимальное и максимальное значения границы
                const float min = m_Thresholds[i].front();
                const float max = m_Thresholds[i].back();

                const float v = *rit;
                const float t = std::clamp((v - min) / (max - min), 0.f, 1.f);

                static const auto min_clr{READ_IF_EXISTS(pSettings, r_fvector4, "warning_icon_color", "min", (Fvector4{255.f, 0.f, 255.f, 0.f}))};
                static const auto max_clr{READ_IF_EXISTS(pSettings, r_fvector4, "warning_icon_color", "max", (Fvector4{255.f, 255.f, 0.f, 0.f}))};

                auto to_u = [](const float x) { return static_cast<u32>(std::lround(std::clamp(x, 0.f, 255.f))); };

                SetWarningIconColor(i,
                                    color_argb(to_u(std::lerp(min_clr.x, max_clr.x, t)), to_u(std::lerp(min_clr.y, max_clr.y, t)), to_u(std::lerp(min_clr.z, max_clr.z, t)),
                                               to_u(std::lerp(min_clr.w, max_clr.w, t))));
            }
            else
                TurnOffWarningIcon(i);

            i = (EWarningIcons)(i + 1);

            if (i == ewiInvincible)
                i = (EWarningIcons)(i + 1);
        }

        // OP22: заряд экзоскелета, пороги exo_charge как в ОП (значение - разряд)
        if (!external_icon_ctrl)
        {
            auto* exo = smart_cast<CExoOutfit*>(m_pActor->inventory().m_slots[OUTFIT_SLOT].m_pIItem);
            const float v = exo && exo->op22_charge() >= 0.f ? 1.f - exo->op22_charge() : -1.f;
            if (v >= m_op22_exo_min)
            {
                const float t = std::clamp((v - m_op22_exo_min) / (m_op22_exo_max - m_op22_exo_min), 0.f, 1.f);
                static const auto min_clr{READ_IF_EXISTS(pSettings, r_fvector4, "warning_icon_color", "min", (Fvector4{255.f, 0.f, 255.f, 0.f}))};
                static const auto max_clr{READ_IF_EXISTS(pSettings, r_fvector4, "warning_icon_color", "max", (Fvector4{255.f, 255.f, 0.f, 0.f}))};
                auto to_u = [](const float x) { return static_cast<u32>(std::lround(std::clamp(x, 0.f, 255.f))); };
                SetWarningIconColor(&UIExoChargeIcon, color_argb(to_u(std::lerp(min_clr.x, max_clr.x, t)), to_u(std::lerp(min_clr.y, max_clr.y, t)), to_u(std::lerp(min_clr.z, max_clr.z, t)), to_u(std::lerp(min_clr.w, max_clr.w, t))));
            }
            else
                SetWarningIconColor(&UIExoChargeIcon, 0x00ffffff);
        }
    }

    // health&armor
    UIHealthBar.SetProgressPos(m_pActor->GetfHealth() * 100.0f);
    UIMotionIcon.SetPower(m_pActor->conditions().GetPower() * 100.0f);

    UIZoneMap->UpdateRadar(Device.vCameraPosition);
    float h, p;
    Device.vCameraDirection.getHP(h, p);
    UIZoneMap->SetHeading(-h);

    UpdatePickUpItem();
    CUIWindow::Update();
}

bool CUIMainIngameWnd::OnKeyboardPress(int dik)
{
    const bool shift = pInput->iGetAsyncKeyState(DIK_LSHIFT) || pInput->iGetAsyncKeyState(DIK_RSHIFT);
    const auto bind = get_binded_action(dik);

    if (bind == kHIDEHUD)
    {
        if (shift)
        {
            UIZoneMap->ZoomOut();
        }
        else
        {
            HUD().GetUI()->HideGameIndicators();
            HUD().GetUI()->hud_disabled_by_user = true;
        }
        return true;
    }
    else if (bind == kSHOWHUD)
    {
        if (shift)
        {
            UIZoneMap->ZoomIn();
        }
        else
        {
            HUD().GetUI()->ShowGameIndicators();
            HUD().GetUI()->hud_disabled_by_user = false;
        }
        return true;
    }

    return false;
}

void CUIMainIngameWnd::RenderQuickInfos()
{
    if (!m_pActor)
        return;

    static CGameObject* pObject = NULL;
    LPCSTR actor_action = m_pActor->GetDefaultActionForObject();
    UIStaticQuickHelp.Show(NULL != actor_action);

    if (NULL != actor_action)
    {
        if (_stricmp(actor_action, UIStaticQuickHelp.GetText()))
            UIStaticQuickHelp.SetTextST(actor_action);
    }

    if (pObject != m_pActor->ObjectWeLookingAt())
    {
        UIStaticQuickHelp.SetTextST(actor_action);
        UIStaticQuickHelp.ResetClrAnimation();
        pObject = m_pActor->ObjectWeLookingAt();
    }
}

void CUIMainIngameWnd::ReceiveNews(GAME_NEWS_DATA* news)
{
    VERIFY(news->texture_name.size());

    HUD().GetUI()->m_pMessagesWnd->AddIconedPdaMessage(*(news->texture_name), news->tex_rect, news->SingleLineText(), news->show_time);
}

void CUIMainIngameWnd::SetWarningIconColor(CUIStatic* s, const u32 cl)
{
    int bOn = (cl >> 24);
    bool bIsShown = s->IsShown();

    if (bOn)
        s->SetColor(cl);

    if (bOn && !bIsShown)
    {
        m_UIIcons->AddWindow(s, false);
        s->Show(true);
    }

    if (!bOn && bIsShown)
    {
        m_UIIcons->RemoveWindow(s);
        s->Show(false);
    }
}

void CUIMainIngameWnd::SetWarningIconColor(EWarningIcons icon, const u32 cl)
{
    bool bMagicFlag = true;

    // Задаем цвет требуемой иконки
    switch (icon)
    {
    case ewiAll: bMagicFlag = false;
    case ewiWeaponJammed:
        SetWarningIconColor(&UIWeaponJammedIcon, cl);
        if (bMagicFlag)
            break;
    case ewiRadiation:
        SetWarningIconColor(&UIRadiaitionIcon, cl);
        if (bMagicFlag)
            break;
    case ewiWound:
        SetWarningIconColor(&UIWoundIcon, cl);
        if (bMagicFlag)
            break;
    case ewiStarvation:
        SetWarningIconColor(&UIStarvationIcon, cl);
        if (bMagicFlag)
            break;
    case ewiThirst:
        SetWarningIconColor(&UIThirstIcon, cl);
        if (bMagicFlag)
            break;
    case ewiPsyHealth:
        SetWarningIconColor(&UIPsyHealthIcon, cl);
        if (bMagicFlag)
            break;
    case ewiInvincible:
        SetWarningIconColor(&UIInvincibleIcon, cl);
        if (bMagicFlag)
            break;
        break;

    default: R_ASSERT(!"Unknown warning icon type");
    }
}

void CUIMainIngameWnd::TurnOffWarningIcon(EWarningIcons icon) { SetWarningIconColor(icon, 0x00ffffff); }

void CUIMainIngameWnd::SetFlashIconState_(EFlashingIcons type, bool enable)
{
    // Включаем анимацию требуемой иконки
    FlashingIcons_it icon = m_FlashingIcons.find(type);
    R_ASSERT2(icon != m_FlashingIcons.end(), "Flashing icon with this type not existed");
    icon->second->Show(enable);
}

void CUIMainIngameWnd::InitFlashingIcons(CUIXml* node)
{
    const char* const flashingIconNodeName = "flashing_icon";
    int staticsCount = node->GetNodesNum("", 0, flashingIconNodeName);

    CUIXmlInit xml_init;
    CUIStatic* pIcon = NULL;
    // Пробегаемся по всем нодам и инициализируем из них статики
    for (int i = 0; i < staticsCount; ++i)
    {
        pIcon = xr_new<CUIStatic>();
        xml_init.InitStatic(*node, flashingIconNodeName, i, pIcon);
        shared_str iconType = node->ReadAttrib(flashingIconNodeName, i, "type", "none");

        // Теперь запоминаем иконку и ее тип
        EFlashingIcons type = efiPdaTask;

        if (iconType == "pda")
            type = efiPdaTask;
        else if (iconType == "mail")
            type = efiMail;
        else
            R_ASSERT(!"Unknown type of mainingame flashing icon");

        R_ASSERT2(m_FlashingIcons.find(type) == m_FlashingIcons.end(), "Flashing icon with this type already exists");

        CUIStatic*& val = m_FlashingIcons[type];
        val = pIcon;

        AttachChild(pIcon);
        pIcon->Show(false);
    }
}

void CUIMainIngameWnd::DestroyFlashingIcons()
{
    for (FlashingIcons_it it = m_FlashingIcons.begin(); it != m_FlashingIcons.end(); ++it)
    {
        DetachChild(it->second);
        xr_delete(it->second);
    }

    m_FlashingIcons.clear();
}

void CUIMainIngameWnd::UpdateFlashingIcons()
{
    for (FlashingIcons_it it = m_FlashingIcons.begin(); it != m_FlashingIcons.end(); ++it)
    {
        it->second->Update();
    }
}

void CUIMainIngameWnd::AnimateContacts(bool b_snd)
{
    UIPdaOnline.ResetClrAnimation();

    if (b_snd)
        HUD_SOUND::PlaySound(m_contactSnd, Fvector().set(0, 0, 0), 0, true);
}

// OP22: the item under the crosshair is set in CActor::UpdateCL and destroyed at the end of the same Objects.Update, then the
// HUD reads it in this frame (autotest 07.11: call through a freed vtable, rip = 1)
void CUIMainIngameWnd::op22_net_relcase(CObject* object)
{
    if (m_pPickUpItem && smart_cast<CInventoryItem*>(object) == m_pPickUpItem)
        SetPickUpItem(nullptr);
}

void CUIMainIngameWnd::SetPickUpItem(CInventoryItem* PickUpItem)
{
    //	m_pPickUpItem = PickUpItem;
    if (m_pPickUpItem != PickUpItem)
    {
        m_pPickUpItem = PickUpItem;
        UIPickUpItemIcon.Show(false);
        UIPickUpItemIcon.DetachAll();
    }
};

#include "UICellCustomItems.h"
#include "../game_object_space.h"
#include "../script_callback_ex.h"
#include "../script_game_object.h"
#include "../Actor.h"

typedef CUIWeaponCellItem::eAddonType eAddonType;

CUIStatic* init_addon(CUIWeaponCellItem* cell_item, LPCSTR sect, float scale, float scale_x, eAddonType idx)
{
    CUIStatic* addon = xr_new<CUIStatic>();
    addon->SetAutoDelete(true);

    auto pos = cell_item->get_addon_offset(idx);
    pos.x *= scale * scale_x;
    pos.y *= scale;

    CIconParams params(sect);
    Frect rect = params.original_rect();
    params.set_shader(addon);
    addon->SetWndRect(pos.x, pos.y, rect.width() * scale * scale_x, rect.height() * scale);
    addon->SetColor(color_rgba(255, 255, 255, 192));

    return addon;
}

void CUIMainIngameWnd::UpdatePickUpItem()
{
    if (!m_pPickUpItem || !Level().CurrentViewEntity() || Level().CurrentViewEntity()->CLS_ID != CLSID_OBJECT_ACTOR)
    {
        if (UIPickUpItemIcon.IsShown())
        {
            UIPickUpItemIcon.Show(false);
        }

        return;
    };
    if (UIPickUpItemIcon.IsShown())
        return;

    // properties used by inventory menu
    CIconParams& params = m_pPickUpItem->m_icon_params;
    Frect rect = params.original_rect();

    float scale_x = m_iPickUpItemIconWidth / rect.width();

    float scale_y = m_iPickUpItemIconHeight / rect.height();

    scale_x = (scale_x > 1) ? 1.0f : scale_x;
    scale_y = (scale_y > 1) ? 1.0f : scale_y;

    float scale = scale_x < scale_y ? scale_x : scale_y;

    params.set_shader(&UIPickUpItemIcon);

    UIPickUpItemIcon.SetWidth(rect.width() * scale * UI()->get_current_kx());
    UIPickUpItemIcon.SetHeight(rect.height() * scale);

    UIPickUpItemIcon.SetWndPos(m_iPickUpItemIconX + (m_iPickUpItemIconWidth - UIPickUpItemIcon.GetWidth()) / 2,
                               m_iPickUpItemIconY + (m_iPickUpItemIconHeight - UIPickUpItemIcon.GetHeight()) / 2);

    UIPickUpItemIcon.SetColor(color_rgba(255, 255, 255, 192));
    if (auto wpn = m_pPickUpItem->cast_weapon())
    {
        CUIWeaponCellItem cell_item{wpn};

        if (wpn->SilencerAttachable() && wpn->IsSilencerAttached())
        {
            auto sil = init_addon(&cell_item, *wpn->GetSilencerName(), scale, UI()->get_current_kx(), eAddonType::eSilencer);
            UIPickUpItemIcon.AttachChild(sil);
        }

        if (wpn->ScopeAttachable() && wpn->IsScopeAttached())
        {
            auto scope = init_addon(&cell_item, *wpn->GetScopeName(), scale, UI()->get_current_kx(), eAddonType::eScope);
            UIPickUpItemIcon.AttachChild(scope);
        }

        if (wpn->GrenadeLauncherAttachable() && wpn->IsGrenadeLauncherAttached())
        {
            auto launcher = init_addon(&cell_item, *wpn->GetGrenadeLauncherName(), scale, UI()->get_current_kx(), eAddonType::eLauncher);
            UIPickUpItemIcon.AttachChild(launcher);
        }
    }

    // Real Wolf: Колбек для скриптового добавления своих иконок. 10.08.2014.
    g_actor->callback(GameObject::eUIPickUpItemShowing)(m_pPickUpItem->object().lua_game_object(), &UIPickUpItemIcon);

    UIPickUpItemIcon.Show(true);
};

void CUIMainIngameWnd::UpdateActiveItemInfo()
{
    PIItem item = m_pActor->inventory().ActiveItem();
    if (item && item->NeedBriefInfo())
    {
        xr_string str_name;
        xr_string icon_sect_name;
        xr_string str_count;
        item->GetBriefInfo(str_name, icon_sect_name, str_count);

        UIWeaponSignAmmo.Show(true);
        UIWeaponBack.SetText(str_name.c_str());
        UIWeaponSignAmmo.SetText(str_count.c_str());
        if (m_op22_ammo_mag || m_op22_ammo_rest) // OP22: «22/90» -> «22» и «/ 90»
        {
            const size_t sl = str_count.find('/');
            const xr_string mag = sl == xr_string::npos ? str_count : str_count.substr(0, sl);
            const xr_string rest = sl == xr_string::npos ? xr_string() : "/ " + str_count.substr(sl + 1);
            if (m_op22_ammo_mag)
            {
                m_op22_ammo_mag->SetText(mag.c_str());
                m_op22_ammo_mag->Show(true);
            }
            if (m_op22_ammo_rest)
            {
                m_op22_ammo_rest->SetText(rest.c_str());
                m_op22_ammo_rest->Show(true);
            }
        }
        SetAmmoIcon(icon_sect_name.c_str());

        //-------------------
        m_pWeapon = smart_cast<CWeapon*>(item);
    }
    else
    {
        UIWeaponIcon.Show(false);
        UIWeaponSignAmmo.Show(false);
        if (m_op22_ammo_mag)
            m_op22_ammo_mag->Show(false);
        if (m_op22_ammo_rest)
            m_op22_ammo_rest->Show(false);
        UIWeaponBack.SetText("");
        m_pWeapon = item ? smart_cast<CWeapon*>(item) : nullptr;
    }
}

void CUIMainIngameWnd::OnConnected() { UIZoneMap->SetupCurrentMap(); }

void CUIMainIngameWnd::reset_ui()
{
    m_pActor = NULL;
    m_pWeapon = NULL;
    m_pGrenade = NULL;
    m_pItem = NULL;
    m_pPickUpItem = NULL;
    UIMotionIcon.ResetVisibility();
}

using namespace luabind::detail;

template <typename T>
bool test_push_window(lua_State* L, CUIWindow* wnd)
{
    T* derived = smart_cast<T*>(wnd);
    if (derived)
    {
        convert_to_lua<T*>(L, derived);
        return true;
    }
    return false;
}

void GetStaticRaw(CUIMainIngameWnd* wnd, lua_State* L)
{
    // wnd->GetChildWndList();
    shared_str name = lua_tostring(L, 2);
    CUIWindow* child = wnd->FindChild(name, 2);
    if (!child)
    {
        CUIStatic* src = wnd->GetUIZoneMap()->Background();
        child = src->FindChild(name, 5);

        if (!child)
        {
            src = wnd->GetUIZoneMap()->ClipFrame();
            child = src->FindChild(name, 5);
        }
        if (!child)
        {
            src = wnd->GetUIZoneMap()->Compass();
            child = src->FindChild(name, 5);
        }
    }

    if (child)
    {
        // if (test_push_window<CUIMotionIcon>  (L, child)) return;
        if (test_push_window<CUIProgressBar>(L, child))
            return;
        if (test_push_window<CUIStatic>(L, child))
            return;
        if (test_push_window<CUIWindow>(L, child))
            return;
    }
    lua_pushnil(L);
}

using namespace luabind;


void CUIMainIngameWnd::script_register(lua_State* L)
{
    module(L)[(
        class_<CUIMainIngameWnd, CUIWindow>("CUIMainIngameWnd")
            .def("GetStatic", &GetStaticRaw, raw<2>()),

        def("get_main_window", &GetMainIngameWindow), // get_mainingame_window better??
        def("setup_game_icon", &SetupGameIcon)
    )];
}
