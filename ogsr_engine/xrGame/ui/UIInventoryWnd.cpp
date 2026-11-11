#include "stdafx.h"
#include "../ui_base.h" // OP22: UI() для привязки времени к деньгам
#include "UIInventoryWnd.h"

#include "xrUIXmlParser.h"
#include "UIXmlInit.h"
#include "../string_table.h"

#include "../actor.h"
#include "../uigamesp.h"
#include "../hudmanager.h"

#include "../CustomOutfit.h"
#include "../ai_space.h" // OP22
#include "../script_engine.h"
#include "../script_game_object.h"
#include "UIIconParams.h"

#include "../weapon.h"

#include "../eatable_item.h"
#include "../inventory.h"

#include "UIInventoryUtilities.h"
using namespace InventoryUtilities;

#include "../InfoPortion.h"
#include "../level.h"
#include "../game_base_space.h"
#include "../entitycondition.h"

#include "../game_cl_base.h"
#include "../ActorCondition.h"
#include "UIDragDropListEx.h"
#include "UIOutfitSlot.h"
#include "UI3tButton.h"
#include <format>
#include "../UICursor.h" // OP22: плавающее описание
extern void op22_prop_funcs_clear(); // OP22
#include "UIScrollView.h"
#include "UICellItem.h"
#include "../ui_base.h"
#include "..\..\xr_3da\xr_input.h"
#include "..\..\xr_3da\xr_ioconsole.h"

#define INVENTORY_ITEM_XML "inventory_item.xml"
#define INVENTORY_XML "inventory_new.xml"

CUIInventoryWnd* g_pInvWnd = NULL;

CUIInventoryWnd::CUIInventoryWnd()
    : m_pUIBagList(nullptr), m_pUIBeltList(nullptr), m_pUIPistolList(nullptr), m_pUIAutomaticList(nullptr), m_pUIKnifeList(nullptr), m_pUIHelmetList(nullptr),
      m_pUIBIODetList(nullptr), m_pUINightVisionList(nullptr), m_pUIDetectorList(nullptr), m_pUITorchList(nullptr), m_pUIBinocularList(nullptr), m_pUIOutfitList(nullptr)
{
    m_iCurrentActiveSlot = NO_ACTIVE_SLOT;
    UIRank = NULL;
    Init();
    SetCurrentItem(NULL);

    g_pInvWnd = this;
    m_b_need_reinit = false;
    m_b_need_update_stats = false;
    Hide();
}

void CUIInventoryWnd::Init()
{
    CUIXml uiXml;
    bool xml_result = uiXml.Init(CONFIG_PATH, UI_PATH, INVENTORY_XML);
    R_ASSERT3(xml_result, "file parsing error ", uiXml.m_xml_file_name);

    CUIXmlInit xml_init;

    xml_init.InitWindow(uiXml, "main", 0, this);

    AttachChild(&UIBeltSlots);
    xml_init.InitStatic(uiXml, "belt_slots", 0, &UIBeltSlots);

    AttachChild(&UIBack);
    xml_init.InitStatic(uiXml, "back", 0, &UIBack);

    AttachChild(&UIStaticBottom);
    xml_init.InitStatic(uiXml, "bottom_static", 0, &UIStaticBottom);

    AttachChild(&UIBagWnd);
    xml_init.InitStatic(uiXml, "bag_static", 0, &UIBagWnd);

    AttachChild(&UIMoneyWnd);
    xml_init.InitStatic(uiXml, "money_static", 0, &UIMoneyWnd);

    AttachChild(&UIDescrWnd);
    xml_init.InitStatic(uiXml, "descr_static", 0, &UIDescrWnd);

    UIDescrWnd.AttachChild(&UIItemInfo);
    UIItemInfo.Init(0, 0, UIDescrWnd.GetWidth(), UIDescrWnd.GetHeight(), INVENTORY_ITEM_XML);
    // OP22: описание предмета ОП 2.2 - плавающее окно у курсора (Ghidra: CUIInventoryWnd::Init создаёт CUIItemInfo из
    // inventory_item_fly.xml, Draw/Update рисуют его поверх окна, колесо мыши - в его описание); панель descr_static
    // ОП не заполняет. Окно со своей панелью описания (скин op22_port) помечено узлом op22_panel_info
    // OP22: панель описания (op22_panel_info) и плавающее окно вместе - атрибут fly="1" у op22_panel_info (окно по макету)
    const bool op22_panel = !!uiXml.NavigateToNode("op22_panel_info", 0);
    if (!op22_panel || uiXml.ReadAttribInt("op22_panel_info", 0, "fly", 0))
    {
        CUIXml fly_xml;
        if (fly_xml.Init(CONFIG_PATH, UI_PATH, "inventory_item_fly.xml"))
        {
            m_op22_fly = xr_new<CUIItemInfo>();
            m_op22_fly->Init("inventory_item_fly.xml");
            if (!op22_panel)
                UIItemInfo.Show(false);
        }
    }

    AttachChild(&UIPersonalWnd);
    xml_init.InitFrameWindow(uiXml, "character_frame_window", 0, &UIPersonalWnd);

    AttachChild(&UIProgressBack);
    xml_init.InitStatic(uiXml, "progress_background", 0, &UIProgressBack);

    UIProgressBack.AttachChild(&UIProgressBarHealth);
    xml_init.InitProgressBar(uiXml, "progress_bar_health", 0, &UIProgressBarHealth);

    UIProgressBack.AttachChild(&UIProgressBarPsyHealth);
    xml_init.InitProgressBar(uiXml, "progress_bar_psy", 0, &UIProgressBarPsyHealth);

    UIProgressBack.AttachChild(&UIProgressBarRadiation);
    xml_init.InitProgressBar(uiXml, "progress_bar_radiation", 0, &UIProgressBarRadiation);

    // OP22: окно по макету
    if (uiXml.NavigateToNode("progress_bar_satiety", 0))
    {
        m_op22_satiety = xr_new<CUIProgressBar>();
        m_op22_satiety->SetAutoDelete(true);
        UIProgressBack.AttachChild(m_op22_satiety);
        xml_init.InitProgressBar(uiXml, "progress_bar_satiety", 0, m_op22_satiety);
    }
    if (uiXml.NavigateToNode("progress_bar_thirst", 0))
    {
        m_op22_thirst = xr_new<CUIProgressBar>();
        m_op22_thirst->SetAutoDelete(true);
        UIProgressBack.AttachChild(m_op22_thirst);
        xml_init.InitProgressBar(uiXml, "progress_bar_thirst", 0, m_op22_thirst);
    }
    { // OP22: GAMMA 2.0 - шкалы выносливости, бодрости, кровотечения (дети progress_background, как сытость) и числа
        CUIProgressBar** bars[3] = {&m_op22_power, &m_op22_sleep, &m_op22_bleed};
        static const char* bar_nodes[3] = {"progress_bar_power", "progress_bar_sleep", "progress_bar_bleeding"};
        for (int i = 0; i < 3; ++i)
        {
            if (!uiXml.NavigateToNode(bar_nodes[i], 0))
                continue;
            *bars[i] = xr_new<CUIProgressBar>();
            (*bars[i])->SetAutoDelete(true);
            UIProgressBack.AttachChild(*bars[i]);
            xml_init.InitProgressBar(uiXml, bar_nodes[i], 0, *bars[i]);
        }
        static const char* val_nodes[5] = {"op22_power_val", "op22_satiety_val", "op22_thirst_val", "op22_sleep_val", "op22_bleeding_val"};
        for (int i = 0; i < 5; ++i)
        {
            if (!uiXml.NavigateToNode(val_nodes[i], 0))
                continue;
            m_op22_vals2[i] = xr_new<CUIStatic>();
            m_op22_vals2[i]->SetAutoDelete(true);
            AttachChild(m_op22_vals2[i]);
            xml_init.InitStatic(uiXml, val_nodes[i], 0, m_op22_vals2[i]);
        }
        if (pSettings->line_exist("main_ingame_indicators_thresholds", "wounds")) // как значок HUD: предел - последний порог
        {
            const char* rec = pSettings->r_string("main_ingame_indicators_thresholds", "wounds");
            string32 tmp;
            m_op22_bleed_max = _max(0.01f, (float)atof(_GetItem(rec, _GetItemCount(rec) - 1, tmp)));
        }
    }
    if (uiXml.NavigateToNode("op22_weight", 0))
    {
        m_op22_weight = xr_new<CUIStatic>();
        m_op22_weight->SetAutoDelete(true);
        AttachChild(m_op22_weight);
        xml_init.InitStatic(uiXml, "op22_weight", 0, m_op22_weight);
    }
    if (uiXml.NavigateToNode("op22_weight_bar", 0))
    {
        m_op22_weight_bar = xr_new<CUIProgressBar>();
        m_op22_weight_bar->SetAutoDelete(true);
        AttachChild(m_op22_weight_bar);
        xml_init.InitProgressBar(uiXml, "op22_weight_bar", 0, m_op22_weight_bar);
    }
    if (uiXml.NavigateToNode("op22_caption", 0))
    {
        m_op22_caption = xr_new<CUIStatic>();
        m_op22_caption->SetAutoDelete(true);
        AttachChild(m_op22_caption);
        xml_init.InitStatic(uiXml, "op22_caption", 0, m_op22_caption);
    }
    {
        static const char* g2_nodes[5] = {"op22_outfit_name", "op22_health_val", "op22_psy_val", "op22_radiation_val", "op22_belt_count"};
        for (int i = 0; i < 5; ++i)
        {
            if (!uiXml.NavigateToNode(g2_nodes[i], 0))
                continue;
            m_op22_g2[i] = xr_new<CUIStatic>();
            m_op22_g2[i]->SetAutoDelete(true);
            AttachChild(m_op22_g2[i]);
            xml_init.InitStatic(uiXml, g2_nodes[i], 0, m_op22_g2[i]);
        }
    }

    UIPersonalWnd.AttachChild(&UIStaticPersonal);
    xml_init.InitStatic(uiXml, "static_personal", 0, &UIStaticPersonal);
    //	UIStaticPersonal.Init				(1, UIPersonalWnd.GetHeight() - 175, 260, 260);

    AttachChild(&UIOutfitInfo);
    UIOutfitInfo.InitFromXml(uiXml);
    //.	xml_init.InitStatic					(uiXml, "outfit_info_window",0, &UIOutfitInfo);

    //Элементы автоматического добавления
    xml_init.InitAutoStatic(uiXml, "auto_static", this);

    m_pUIBagList = xr_new<CUIDragDropListEx>();
    UIBagWnd.AttachChild(m_pUIBagList);
    m_pUIBagList->SetAutoDelete(true);
    xml_init.InitDragDropListEx(uiXml, "dragdrop_bag", 0, m_pUIBagList);
    BindDragDropListEnents(m_pUIBagList);

    m_pUIBeltList = xr_new<CUIDragDropListEx>();
    AttachChild(m_pUIBeltList);
    m_pUIBeltList->SetAutoDelete(true);
    xml_init.InitDragDropListEx(uiXml, "dragdrop_belt", 0, m_pUIBeltList);
    BindDragDropListEnents(m_pUIBeltList);

    m_pUIOutfitList = xr_new<CUIOutfitDragDropList>();
    AttachChild(m_pUIOutfitList);
    if (uiXml.NavigateToNode("dragdrop_outfit:battery_icon", 0)) // OP22: панель аккумулятора
    {
        m_op22_bat_icon = xr_new<CUIStatic>();
        m_op22_bat_icon->SetAutoDelete(true);
        m_pUIOutfitList->AttachChild(m_op22_bat_icon);
        xml_init.InitWindow(uiXml, "dragdrop_outfit:battery_icon", 0, m_op22_bat_icon);
        m_op22_bat_icon->SetStretchTexture(true);
        m_op22_bat_text = xr_new<CUIStatic>();
        m_op22_bat_text->SetAutoDelete(true);
        m_pUIOutfitList->AttachChild(m_op22_bat_text);
        xml_init.InitStatic(uiXml, "dragdrop_outfit:battery_charge_text", 0, m_op22_bat_text);
        m_op22_bat_bar = xr_new<CUIProgressBar>();
        m_op22_bat_bar->SetAutoDelete(true);
        m_pUIOutfitList->AttachChild(m_op22_bat_bar);
        xml_init.InitProgressBar(uiXml, "dragdrop_outfit:battery_charge_progress", 0, m_op22_bat_bar);
        m_op22_bat_keep_icon = !!uiXml.ReadAttribInt("dragdrop_outfit:battery_icon", 0, "op22_keep_tex", 0); // OP22: GAMMA 2.0
        m_op22_bat_pct = !!uiXml.ReadAttribInt("dragdrop_outfit:battery_charge_text", 0, "op22_pct", 0);
        m_op22_bat_none = uiXml.ReadAttrib("dragdrop_outfit:battery_charge_text", 0, "op22_none", "");
        if (m_op22_bat_keep_icon)
            xml_init.InitStatic(uiXml, "dragdrop_outfit:battery_icon", 0, m_op22_bat_icon);
        if (uiXml.NavigateToNode("dragdrop_outfit:op22_battery_fill", 0))
        {
            m_op22_bat_fill = xr_new<CUIProgressBar>();
            m_op22_bat_fill->SetAutoDelete(true);
            m_pUIOutfitList->AttachChild(m_op22_bat_fill);
            xml_init.InitProgressBar(uiXml, "dragdrop_outfit:op22_battery_fill", 0, m_op22_bat_fill);
        }
    }
    m_pUIOutfitList->SetAutoDelete(true);
    xml_init.InitDragDropListEx(uiXml, "dragdrop_outfit", 0, m_pUIOutfitList);
    BindDragDropListEnents(m_pUIOutfitList);

    if (Core.Features.test(xrCore::Feature::ogse_new_slots))
    {
        m_pUIKnifeList = xr_new<CUIDragDropListEx>();
        AttachChild(m_pUIKnifeList);
        m_pUIKnifeList->SetAutoDelete(true);
        xml_init.InitDragDropListEx(uiXml, "dragdrop_knife", 0, m_pUIKnifeList);
        BindDragDropListEnents(m_pUIKnifeList);
    }

    m_pUIPistolList = xr_new<CUIDragDropListEx>();
    AttachChild(m_pUIPistolList);
    m_pUIPistolList->SetAutoDelete(true);
    xml_init.InitDragDropListEx(uiXml, "dragdrop_pistol", 0, m_pUIPistolList);
    BindDragDropListEnents(m_pUIPistolList);

    m_pUIAutomaticList = xr_new<CUIDragDropListEx>();
    AttachChild(m_pUIAutomaticList);
    m_pUIAutomaticList->SetAutoDelete(true);
    xml_init.InitDragDropListEx(uiXml, "dragdrop_automatic", 0, m_pUIAutomaticList);
    BindDragDropListEnents(m_pUIAutomaticList);

    if (Core.Features.test(xrCore::Feature::ogse_new_slots))
    {
        m_pUIBinocularList = xr_new<CUIDragDropListEx>();
        AttachChild(m_pUIBinocularList);
        m_pUIBinocularList->SetAutoDelete(true);
        xml_init.InitDragDropListEx(uiXml, "dragdrop_binocular", 0, m_pUIBinocularList);
        BindDragDropListEnents(m_pUIBinocularList);

        m_pUIHelmetList = xr_new<CUIDragDropListEx>();
        AttachChild(m_pUIHelmetList);
        m_pUIHelmetList->SetAutoDelete(true);
        xml_init.InitDragDropListEx(uiXml, "dragdrop_helmet", 0, m_pUIHelmetList);
        BindDragDropListEnents(m_pUIHelmetList);

        m_pUIBIODetList = xr_new<CUIDragDropListEx>();
        AttachChild(m_pUIBIODetList);
        m_pUIBIODetList->SetAutoDelete(true);
        xml_init.InitDragDropListEx(uiXml, "dragdrop_biodetector", 0, m_pUIBIODetList);
        BindDragDropListEnents(m_pUIBIODetList);

        m_pUINightVisionList = xr_new<CUIDragDropListEx>();
        AttachChild(m_pUINightVisionList);
        m_pUINightVisionList->SetAutoDelete(true);
        xml_init.InitDragDropListEx(uiXml, "dragdrop_nv", 0, m_pUINightVisionList);
        BindDragDropListEnents(m_pUINightVisionList);

        m_pUIDetectorList = xr_new<CUIDragDropListEx>();
        AttachChild(m_pUIDetectorList);
        m_pUIDetectorList->SetAutoDelete(true);
        xml_init.InitDragDropListEx(uiXml, "dragdrop_detector", 0, m_pUIDetectorList);
        BindDragDropListEnents(m_pUIDetectorList);

        m_pUITorchList = xr_new<CUIDragDropListEx>();
        AttachChild(m_pUITorchList);
        m_pUITorchList->SetAutoDelete(true);
        xml_init.InitDragDropListEx(uiXml, "dragdrop_torch", 0, m_pUITorchList);
        BindDragDropListEnents(m_pUITorchList);
    }

    for (u8 i = 0; i < SLOTS_TOTAL; i++)
        m_slots_array[i] = NULL;
    m_slots_array[OUTFIT_SLOT] = m_pUIOutfitList;
    if (Core.Features.test(xrCore::Feature::ogse_new_slots))
        m_slots_array[KNIFE_SLOT] = m_pUIKnifeList;
    m_slots_array[FIRST_WEAPON_SLOT] = m_pUIPistolList;
    m_slots_array[SECOND_WEAPON_SLOT] = m_pUIAutomaticList;
    if (Core.Features.test(xrCore::Feature::ogse_new_slots))
    {
        m_slots_array[APPARATUS_SLOT] = m_pUIBinocularList;
        m_slots_array[HELMET_SLOT] = m_pUIHelmetList;
        m_slots_array[BIODETECTOR_SLOT] = m_pUIBIODetList;
        m_slots_array[NIGHT_VISION_SLOT] = m_pUINightVisionList;
        m_slots_array[DETECTOR_SLOT] = m_pUIDetectorList;
        m_slots_array[TORCH_SLOT] = m_pUITorchList;
    }

    // OP22: слоты ОП 2.2 по узлам разметки (окно порта ОП 2 на OGSR их содержит); нет узла - нет списка
    if (!Core.Features.test(xrCore::Feature::ogse_new_slots))
    {
        static const std::pair<u8, const char*> op22_slots[] = {
            {KNIFE_SLOT, "dragdrop_knife"},           {APPARATUS_SLOT, "dragdrop_apparatus"}, {DETECTOR_SLOT, "dragdrop_detector_arts"},
            {TORCH_SLOT, "dragdrop_torch"},           {NIGHT_VISION_SLOT, "dragdrop_detector_anoms"}, // ОП: 11 - детектор аномалий
            {BIODETECTOR_SLOT, "dragdrop_pnv"},       // ОП: 12 - ПНВ
            {OP22_SHOTGUN_SLOT, "dragdrop_secondwpn"}, {OP22_SHOTGUN_SLOT, "dragdrop_shotgun"}, {OP22_BIODEV_SLOT, "dragdrop_biodev"},
            {GRENADE_SLOT, "dragdrop_grenade"}, {BOLT_SLOT, "dragdrop_bolt"}, {PDA_SLOT, "dragdrop_pda"}, // OP22: окно по макету
        };
        for (const auto& [slot, node] : op22_slots)
        {
            if (m_op22_lists[slot] || m_slots_array[slot] || !uiXml.NavigateToNode(node, 0))
                continue;
            CUIDragDropListEx* l = xr_new<CUIDragDropListEx>();
            AttachChild(l);
            l->SetAutoDelete(true);
            xml_init.InitDragDropListEx(uiXml, node, 0, l);
            BindDragDropListEnents(l);
            m_op22_lists[slot] = l;
            m_slots_array[slot] = l;
        }
    }

    // OP22: быстрые слоты ОП 2.2
    for (int i = 0; i < 5; ++i)
    {
        string32 node;
        xr_sprintf(node, "dragdrop_quick%d", i);
        if (!uiXml.NavigateToNode(node, 0))
            continue;
        CUIDragDropListEx* l = xr_new<CUIDragDropListEx>();
        AttachChild(l);
        l->SetAutoDelete(true);
        xml_init.InitDragDropListEx(uiXml, node, 0, l);
        l->SetAdjustCells(true); // предмет любого размера вписывается в ячейку (вылет на ноже 3x1 в сетке 2x2)
        BindDragDropListEnents(l);
        m_op22_quick[i] = l;
    }

    // pop-up menu
    AttachChild(&UIPropertiesBox);
    UIPropertiesBox.Init(0, 0, 300, 300);
    UIPropertiesBox.Hide();

    AttachChild(&UIStaticTime);
    xml_init.InitStatic(uiXml, "time_static", 0, &UIStaticTime);
    m_op22_time_gap = uiXml.ReadAttribFlt("time_static", 0, "op22_left_of_money", -1.f); // OP22

    UIStaticTime.AttachChild(&UIStaticTimeString);
    xml_init.InitStatic(uiXml, "time_static_str", 0, &UIStaticTimeString);

    UIExitButton = xr_new<CUI3tButton>();
    UIExitButton->SetAutoDelete(true);
    AttachChild(UIExitButton);
    xml_init.Init3tButton(uiXml, "exit_button", 0, UIExitButton);

    // Load sounds
    if (uiXml.NavigateToNode("action_sounds", 0))
    {
        XML_NODE* stored_root = uiXml.GetLocalRoot();
        uiXml.SetLocalRoot(uiXml.NavigateToNode("action_sounds", 0));

        create_ui_snd(sounds[eInvSndOpen], uiXml.Read("snd_open", 0, nullptr));
        create_ui_snd(sounds[eInvSndClose], uiXml.Read("snd_close", 0, nullptr));
        create_ui_snd(sounds[eInvItemToSlot], uiXml.Read("snd_item_to_slot", 0, nullptr));
        create_ui_snd(sounds[eInvItemToBelt], uiXml.Read("snd_item_to_belt", 0, nullptr));
        create_ui_snd(sounds[eInvItemToRuck], uiXml.Read("snd_item_to_ruck", 0, nullptr));
        create_ui_snd(sounds[eInvProperties], uiXml.Read("snd_properties", 0, nullptr));
        create_ui_snd(sounds[eInvDropItem], uiXml.Read("snd_drop_item", 0, nullptr));
        create_ui_snd(sounds[eInvAttachAddon], uiXml.Read("snd_attach_addon", 0, nullptr));
        create_ui_snd(sounds[eInvDetachAddon], uiXml.Read("snd_detach_addon", 0, nullptr));
        create_ui_snd(sounds[eInvItemUse], uiXml.Read("snd_item_use", 0, nullptr));
        create_ui_snd(sounds[eInvUnloadWpn], uiXml.Read("snd_unload_wpn", 0, nullptr));

        uiXml.SetLocalRoot(stored_root);
    }
}

EListType CUIInventoryWnd::GetType(CUIDragDropListEx* l)
{
    if (l == m_pUIBagList)
        return iwBag;
    if (l == m_pUIBeltList)
        return iwBelt;

    for (u8 i = 0; i < SLOTS_TOTAL; i++)
        if (m_slots_array[i] == l)
            return iwSlot;

    NODEFAULT;
#ifdef DEBUG
    return iwSlot;
#endif // DEBUG
}

void CUIInventoryWnd::PlaySnd(eInventorySndAction a)
{
    if (sounds[a]._handle())
        sounds[a].play(NULL, sm_2D);
}

CUIInventoryWnd::~CUIInventoryWnd()
{
    //.	ClearDragDrop(m_vDragDropItems);
    ClearAllLists();
    xr_delete(m_op22_fly); // OP22
}

bool CUIInventoryWnd::OnMouse(float x, float y, EUIMessages mouse_action)
{
    if (m_b_need_reinit)
        return true;

    if (mouse_action == WINDOW_RBUTTON_DOWN)
    {
        if (UIPropertiesBox.IsShown())
        {
            UIPropertiesBox.Hide();
            return true;
        }
    }

    // OP22: колесо мыши при открытом плавающем описании прокручивает его текст (как в ОП 2.2)
    if (m_op22_fly_on && m_op22_fly && m_op22_fly->UIDesc && (mouse_action == WINDOW_MOUSE_WHEEL_UP || mouse_action == WINDOW_MOUSE_WHEEL_DOWN))
    {
        CUIScrollView* d = m_op22_fly->UIDesc;
        const int step = 20;
        int p = d->GetCurrentScrollPos() + (mouse_action == WINDOW_MOUSE_WHEEL_UP ? -step : step);
        d->SetScrollPos(_max(d->GetMinScrollPos(), _min(d->GetMaxScrollPos(), p)));
        return true;
    }

    CUIWindow::OnMouse(x, y, mouse_action);

    return true; // always returns true, because ::StopAnyMove() == true;
}

void CUIInventoryWnd::Draw()
{
    CUIWindow::Draw();
    if (m_op22_fly && m_op22_fly_on) // OP22: плавающее описание поверх окна
        m_op22_fly->Draw();
}

// OP22: ячейка предмета под курсором (рюкзак, пояс, костюм, слоты)
CUICellItem* CUIInventoryWnd::op22_fly_cell_at_cursor()
{
    const Fvector2 c = GetUICursor()->GetCursorPosition();
    auto test = [&c](CUIDragDropListEx* l) -> CUICellItem* {
        if (!l || !l->IsShown())
            return nullptr;
        Frect lr;
        l->GetAbsoluteRect(lr);
        if (!lr.in(c.x, c.y))
            return nullptr;
        for (u32 i = 0, n = l->ItemsCount(); i < n; ++i)
        {
            CUICellItem* ci = l->GetItemIdx(i);
            Frect r;
            ci->GetAbsoluteRect(r);
            if (r.in(c.x, c.y))
                return ci;
        }
        return nullptr;
    };
    if (CUICellItem* ci = test(m_pUIBagList))
        return ci;
    if (CUICellItem* ci = test(m_pUIBeltList))
        return ci;
    for (CUIDragDropListEx* l : m_slots_array)
        if (CUICellItem* ci = test(l))
            return ci;
    return nullptr;
}

// OP22: плавающее описание: курсор над предметом дольше g_fly_show_info_delay мс - окно у курсора, не за краем экрана;
// меню предмета, зажатая кнопка (перетаскивание) - скрыто
void CUIInventoryWnd::op22_fly_update()
{
    if (!m_op22_fly)
        return;
    CUICellItem* ci = (UIPropertiesBox.IsShown() || pInput->iGetAsyncKeyState(MOUSE_1)) ? nullptr : op22_fly_cell_at_cursor();
    if (ci != m_op22_fly_cell)
    {
        m_op22_fly_cell = ci;
        m_op22_fly_t = Device.dwTimeGlobal;
        m_op22_fly_on = false;
        return;
    }
    if (!ci)
        return;
    if (!m_op22_fly_on)
    {
        int mn = 0, mx = 0;
        const int delay = Console ? Console->GetInteger("g_fly_show_info_delay", mn, mx) : 1000;
        PIItem it = (PIItem)ci->m_pData;
        if (!it || Device.dwTimeGlobal - m_op22_fly_t < u32(_max(delay, 0)))
            return;
        m_op22_fly->InitItem(it);
        m_op22_fly_on = true;
    }
    const Fvector2 c = GetUICursor()->GetCursorPosition();
    const Fvector2 sz = m_op22_fly->GetWndSize();
    constexpr float gap = 8.f; // OP22: отступ от краёв экрана, рамка не упирается в край
    float x = c.x + 16.f, y = c.y + 16.f;
    if (x + sz.x > 1024.f - gap) // виртуальный экран 1024x768
        x = c.x - 8.f - sz.x;
    if (y + sz.y > 768.f - gap)
        y = 768.f - gap - sz.y;
    m_op22_fly->SetWndPos(Fvector2().set(_max(x, gap), _max(y, gap)));
    m_op22_fly->Update();
}

void CUIInventoryWnd::Update()
{
    if (m_b_need_reinit)
        InitInventory();

    CEntityAlive* pEntityAlive = smart_cast<CEntityAlive*>(Level().CurrentEntity());

    if (pEntityAlive)
    {
        float v = pEntityAlive->conditions().GetHealth() * 100.0f;
        UIProgressBarHealth.SetProgressPos(v);

        v = pEntityAlive->conditions().GetPsyHealth() * 100.0f;
        UIProgressBarPsyHealth.SetProgressPos(v);

        v = pEntityAlive->conditions().GetRadiation() * 100.0f;
        UIProgressBarRadiation.SetProgressPos(v);

        // OP22: GAMMA 2.0 - числа шкал (целые проценты)
        const float g2_vals[3] = {pEntityAlive->conditions().GetHealth(), pEntityAlive->conditions().GetPsyHealth(), pEntityAlive->conditions().GetRadiation()};
        for (int i = 0; i < 3; ++i)
            if (m_op22_g2[1 + i])
                m_op22_g2[1 + i]->SetText(std::format("{}", iFloor(g2_vals[i] * 100.0f + 0.5f)).c_str());

        if (CActor* op22_a = smart_cast<CActor*>(pEntityAlive)) // OP22: сытость и жажда
        {
            if (m_op22_satiety)
                m_op22_satiety->SetProgressPos(op22_a->conditions().GetSatiety() * 100.0f);
            if (m_op22_thirst)
                m_op22_thirst->SetProgressPos(op22_a->conditions().GetThirst() * 100.0f);
            // OP22: GAMMA 2.0 - выносливость, бодрость, кровотечение и числа
            if (m_op22_sleep && Device.dwTimeGlobal >= m_op22_sleep_next)
            {
                m_op22_sleep_next = Device.dwTimeGlobal + 1000;
                luabind::functor<float> f;
                if (ai().script_engine().functor("op22_needs.sleep", f))
                {
                    const float sv = f();
                    m_op22_sleep_v = std::clamp(sv, 0.f, 1.f);
                }
            }
            const float bleed = std::clamp(op22_a->conditions().BleedingSpeed() / m_op22_bleed_max, 0.f, 1.f);
            if (m_op22_power)
                m_op22_power->SetProgressPos(op22_a->conditions().GetPower() * 100.0f);
            if (m_op22_sleep)
                m_op22_sleep->SetProgressPos(m_op22_sleep_v * 100.0f);
            if (m_op22_bleed)
                m_op22_bleed->SetProgressPos(bleed * 100.0f);
            const float v2[5] = {op22_a->conditions().GetPower(), op22_a->conditions().GetSatiety(), op22_a->conditions().GetThirst(), m_op22_sleep_v, bleed};
            for (int i = 0; i < 5; ++i)
                if (m_op22_vals2[i])
                    m_op22_vals2[i]->SetText(std::format("{}", iFloor(v2[i] * 100.0f + 0.5f)).c_str());
        }

        CInventoryOwner* pOurInvOwner = smart_cast<CInventoryOwner*>(pEntityAlive);

        // update money
        static const char* StMoneyDescr = CStringTable().translate("ui_st_money_descr").c_str();
        UIMoneyWnd.SetText(std::format("{} {}", InventoryUtilities::op22_group_digits(pOurInvOwner->get_money()), StMoneyDescr).c_str()); // OP22: «18 420 р.»
        if (m_op22_time_gap >= 0.f && UIMoneyWnd.GetFont()) // OP22: GAMMA 2.0 - время вплотную слева от суммы (деньги выровнены вправо)
        {
            float len = UIMoneyWnd.GetFont()->SizeOf_(UIMoneyWnd.GetText());
            UI()->ClientToScreenScaledWidth(len);
            const float right = UIMoneyWnd.GetWndPos().x + UIMoneyWnd.GetWidth() - len - m_op22_time_gap;
            UIStaticTime.SetWndPos(Fvector2().set(right - UIStaticTime.GetWidth(), UIStaticTime.GetWndPos().y));
        }
 
        if (m_b_need_update_stats)
        {
            // update outfit parameters
            CCustomOutfit* outfit = smart_cast<CCustomOutfit*>(pOurInvOwner->inventory().m_slots[OUTFIT_SLOT].m_pIItem);
            UIOutfitInfo.Update(outfit);

            m_b_need_update_stats = false;
        }

        if (m_op22_g2[0] || m_op22_g2[4]) // OP22: GAMMA 2.0 - имя костюма и «5 / 14» пояса
        {
            CInventory& g2_inv = pOurInvOwner->inventory();
            if (m_op22_g2[0])
            {
                PIItem g2_outfit = g2_inv.m_slots[OUTFIT_SLOT].m_pIItem;
                m_op22_g2[0]->SetText(g2_outfit ? g2_outfit->Name() : "");
            }
            if (m_op22_g2[4])
                m_op22_g2[4]->SetText(std::format("{} / {}", g2_inv.m_belt.size(), g2_inv.BeltSlotsCount()).c_str());
        }

        if (m_op22_bat_icon && Device.dwTimeGlobal >= m_op22_bat_next)
        {
            m_op22_bat_next = Device.dwTimeGlobal + 500;
            op22_update_battery();
        }
    }

    UIStaticTimeString.SetText(*InventoryUtilities::GetGameTimeAsString(InventoryUtilities::etpTimeToMinutes));

    CUIWindow::Update();
    op22_fly_update(); // OP22
}

void CUIInventoryWnd::Show()
{
    InitInventory();
    inherited::Show();

    if (Core.Features.test(xrCore::Feature::more_hide_weapon))
        Actor()->SetWeaponHideState(INV_STATE_INV_WND, true);

    SendInfoToActor("ui_inventory");

    Update();
    PlaySnd(eInvSndOpen);

    m_b_need_update_stats = true;

    if (Core.Features.test(xrCore::Feature::engine_ammo_repacker) && !Core.Features.test(xrCore::Feature::hard_ammo_reload))
        if (auto pActor = Actor())
            pActor->RepackAmmo();
}

// OP22: секция и заряд - из op22_exo.script; у костюма без аккумуляторов панели нет
void CUIInventoryWnd::op22_update_battery()
{
    auto* outfit = Actor() ? smart_cast<CCustomOutfit*>(Actor()->inventory().m_slots[OUTFIT_SLOT].m_pIItem) : nullptr;
    LPCSTR sect = "-";
    float charge = 0.f;
    if (outfit)
    {
        luabind::functor<LPCSTR> f_sect;
        luabind::functor<float> f_charge;
        if (ai().script_engine().functor("op22_exo.panel_section", f_sect) && ai().script_engine().functor("op22_exo.panel_charge", f_charge))
        {
            sect = f_sect(outfit->lua_game_object());
            charge = f_charge(outfit->lua_game_object());
        }
    }
    const bool exo = sect && xr_strcmp(sect, "-") != 0;
    const bool has_battery = exo && sect[0];
    m_op22_bat_icon->Show(m_op22_bat_keep_icon ? exo : has_battery);
    m_op22_bat_text->Show(exo);
    m_op22_bat_bar->Show(exo);
    if (m_op22_bat_fill)
        m_op22_bat_fill->Show(exo);
    if (!exo)
        return;
    m_op22_bat_bar->SetProgressPos(charge * 100.f);
    if (m_op22_bat_fill)
        m_op22_bat_fill->SetProgressPos(has_battery ? charge * 100.f : 0.f);
    if (m_op22_bat_pct) // OP22: GAMMA 2.0 - заряд числом
        m_op22_bat_text->SetText(has_battery ? std::format("{} %", iFloor(charge * 100.f + 0.5f)).c_str() : m_op22_bat_none.c_str());
    if (m_op22_bat_keep_icon)
        return;
    const shared_str battery(sect);
    if (has_battery && m_op22_bat_sect != battery)
    {
        m_op22_bat_sect = battery;
        CIconParams icon(battery);
        icon.set_shader(m_op22_bat_icon);
    }
}

void CUIInventoryWnd::Hide()
{
    PlaySnd(eInvSndClose);
    inherited::Hide();
    op22_prop_funcs_clear(); // OP22: функции меню предмета держат ссылки на Lua
    m_op22_fly_on = false; // OP22: ячейки удаляются ниже (ClearAllLists)
    m_op22_fly_cell = nullptr;

    SendInfoToActor("ui_inventory_hide");
    ClearAllLists();

    //достать вещь в активный слот
    CActor* pActor = smart_cast<CActor*>(Level().CurrentEntity());
    if (pActor && m_iCurrentActiveSlot != NO_ACTIVE_SLOT && pActor->inventory().m_slots[m_iCurrentActiveSlot].m_pIItem)
    {
        pActor->inventory().Activate(m_iCurrentActiveSlot);
        m_iCurrentActiveSlot = NO_ACTIVE_SLOT;
    }

    if (Core.Features.test(xrCore::Feature::more_hide_weapon))
        if (pActor)
            pActor->SetWeaponHideState(INV_STATE_INV_WND, false);

    HideSlotsHighlight();
}

void CUIInventoryWnd::HideSlotsHighlight()
{
    m_pUIBeltList->enable_highlight(false);
    for (const auto& DdList : m_slots_array)
        if (DdList)
            DdList->enable_highlight(false);
}

void CUIInventoryWnd::ShowSlotsHighlight(PIItem InvItem)
{
    if (InvItem->m_flags.test(CInventoryItem::Fbelt) && !Actor()->inventory().InBelt(InvItem))
        m_pUIBeltList->enable_highlight(true);

    for (const u8 slot : InvItem->GetSlots())
        if (auto DdList = m_slots_array[slot]; DdList && (!Actor()->inventory().InSlot(InvItem) || InvItem->GetSlot() != slot))
            DdList->enable_highlight(true);
}

void CUIInventoryWnd::AttachAddon(PIItem item_to_upgrade)
{
    PlaySnd(eInvAttachAddon);
    R_ASSERT(item_to_upgrade);

    item_to_upgrade->Attach(CurrentIItem(), true);

    SetCurrentItem(nullptr);
}

void CUIInventoryWnd::DetachAddon(const char* addon_name)
{
    PlaySnd(eInvDetachAddon);

    CurrentIItem()->Detach(addon_name, true);
}

void CUIInventoryWnd::SendEvent_ActivateSlot(PIItem pItem)
{
    NET_Packet P;
    pItem->object().u_EventGen(P, GEG_PLAYER_ACTIVATE_SLOT, pItem->object().H_Parent()->ID());
    P.w_u32(pItem->GetSlot());
    pItem->object().u_EventSend(P);
}

void CUIInventoryWnd::SendEvent_Item2Slot(PIItem pItem)
{
    NET_Packet P;
    pItem->object().u_EventGen(P, GEG_PLAYER_ITEM2SLOT, pItem->object().H_Parent()->ID());
    P.w_u16(pItem->object().ID());
    pItem->object().u_EventSend(P);
    g_pInvWnd->PlaySnd(eInvItemToSlot);
    m_b_need_update_stats = true;
};

void CUIInventoryWnd::SendEvent_Item2Belt(PIItem pItem)
{
    NET_Packet P;
    pItem->object().u_EventGen(P, GEG_PLAYER_ITEM2BELT, pItem->object().H_Parent()->ID());
    P.w_u16(pItem->object().ID());
    pItem->object().u_EventSend(P);
    g_pInvWnd->PlaySnd(eInvItemToBelt);
    m_b_need_update_stats = true;
};

void CUIInventoryWnd::SendEvent_Item2Ruck(PIItem pItem)
{
    NET_Packet P;
    pItem->object().u_EventGen(P, GEG_PLAYER_ITEM2RUCK, pItem->object().H_Parent()->ID());
    P.w_u16(pItem->object().ID());
    pItem->object().u_EventSend(P);
    g_pInvWnd->PlaySnd(eInvItemToRuck);
    m_b_need_update_stats = true;
};

void CUIInventoryWnd::SendEvent_Item_Drop(PIItem pItem)
{
    pItem->SetDropManual(TRUE);

    g_pInvWnd->PlaySnd(eInvDropItem);
    m_b_need_update_stats = true;
};

void CUIInventoryWnd::SendEvent_Item_Eat(PIItem pItem)
{
    R_ASSERT(pItem->m_pCurrentInventory == m_pInv);
    NET_Packet P;
    pItem->object().u_EventGen(P, GEG_PLAYER_ITEM_EAT, pItem->object().H_Parent()->ID());
    P.w_u16(pItem->object().ID());
    pItem->object().u_EventSend(P);
};

void CUIInventoryWnd::BindDragDropListEnents(CUIDragDropListEx* lst)
{
    lst->m_f_item_drop = fastdelegate::MakeDelegate(this, &CUIInventoryWnd::OnItemDropOP22); // OP22
    lst->m_f_item_start_drag = fastdelegate::MakeDelegate(this, &CUIInventoryWnd::OnItemStartDrag);
    lst->m_f_item_db_click = fastdelegate::MakeDelegate(this, &CUIInventoryWnd::OnItemDbClick);
    lst->m_f_item_selected = fastdelegate::MakeDelegate(this, &CUIInventoryWnd::OnItemSelectedOP22); // OP22
    lst->m_f_item_rbutton_click = fastdelegate::MakeDelegate(this, &CUIInventoryWnd::OnItemRButtonClick);
}

#include "../xr_level_controller.h"
#include <dinput.h>

bool CUIInventoryWnd::OnKeyboard(int dik, EUIMessages keyboard_action)
{
    if (m_b_need_reinit)
        return true;

    if (UIPropertiesBox.GetVisible())
        UIPropertiesBox.OnKeyboard(dik, keyboard_action);

    if (is_binded(kDROP, dik))
    {
        if (WINDOW_KEY_PRESSED == keyboard_action)
            DropCurrentItem(false);
        return true;
    }

    if (WINDOW_KEY_PRESSED == keyboard_action)
    {
#ifdef DEBUG
        if (DIK_NUMPAD7 == dik && CurrentIItem())
        {
            CurrentIItem()->ChangeCondition(-0.05f);
            UIItemInfo.InitItem(CurrentIItem());
        }
        else if (DIK_NUMPAD8 == dik && CurrentIItem())
        {
            CurrentIItem()->ChangeCondition(0.05f);
            UIItemInfo.InitItem(CurrentIItem());
        }
#endif
    }
    if (inherited::OnKeyboard(dik, keyboard_action))
        return true;

    return false;
}

void CUIInventoryWnd::UpdateOutfit()
{
    if (dont_update_belt_flag)
    { //Чтобы арты не перемещались в рюкзак, при смене костюма
        dont_update_belt_flag = false;
        return;
    }

    auto& inv = Actor()->inventory();
    const u32 new_slots_count = inv.BeltSlotsCount();
    m_pUIBeltList->SetCellsAvailable(new_slots_count);

    auto& l_blist = inv.m_belt;
    bool modified{};
    while (l_blist.size() > new_slots_count)
    {
        inv.Ruck(l_blist.back());
        modified = true;
    }

    if (modified)
    {
        extern void update_inventory_window(); //некрасиво, зато просто
        update_inventory_window();
    }
}
