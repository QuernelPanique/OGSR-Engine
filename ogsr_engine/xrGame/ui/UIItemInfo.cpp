#include "stdafx.h"

#include "uiiteminfo.h"
#include "uistatic.h"
#include "UIXmlInit.h"

#include "UIListWnd.h"
#include "UIProgressBar.h"
#include "UIScrollView.h"

#include "../string_table.h"
#include "../Inventory_Item.h"
#include "UIInventoryUtilities.h"
#include "../PhysicsShellHolder.h"
#include "UIWpnParams.h"
#include "UIFrameWindow.h" // OP22
#include "UILines.h" // OP22: межстрочный интервал описания
#include "ui_af_params.h"
#include <format>

bool op22_item_shows_condition(CInventoryItem* itm); // OP22: UICellItem.cpp

CUIItemInfo::CUIItemInfo()
{
    UIItemImageSize.set(0.0f, 0.0f);
    UICondProgresBar = NULL;
    UICondition = NULL;
    UICost = NULL;
    UIWeight = NULL;
    UIItemImage = NULL;
    UIDesc = NULL;
    UIWpnParams = NULL;
    UIArtefactParams = NULL;
    UIName = NULL;
    m_pInvItem = NULL;
    m_b_force_drawing = false;
}

CUIItemInfo::~CUIItemInfo()
{
    if (m_op22_wpn_fixed && UIWpnParams) // OP22: окно параметров - ребёнок описания, удаляется здесь
        DetachChild(UIWpnParams);
    xr_delete(UIWpnParams);
    xr_delete(UIArtefactParams);
}

void CUIItemInfo::Init(LPCSTR xml_name)
{
    CUIXml uiXml;
    bool xml_result = uiXml.Init(CONFIG_PATH, UI_PATH, xml_name);
    R_ASSERT2(xml_result, "xml file not found");

    CUIXmlInit xml_init;

    if (uiXml.NavigateToNode("main_frame", 0))
    {
        Frect wnd_rect;
        wnd_rect.x1 = uiXml.ReadAttribFlt("main_frame", 0, "x", 0);
        wnd_rect.y1 = uiXml.ReadAttribFlt("main_frame", 0, "y", 0);

        wnd_rect.x2 = uiXml.ReadAttribFlt("main_frame", 0, "width", 0);
        wnd_rect.y2 = uiXml.ReadAttribFlt("main_frame", 0, "height", 0);

        inherited::Init(wnd_rect.x1, wnd_rect.y1, wnd_rect.x2, wnd_rect.y2);
        m_op22_autoh = !!uiXml.ReadAttribInt("main_frame", 0, "op22_autoh", 0); // OP22
        m_op22_max_h = wnd_rect.y2;
        m_op22_pad_b = uiXml.ReadAttribFlt("main_frame", 0, "op22_pad_b", 0.f);
    }

    // OP22: фон плавающего окна описания ОП 2.2 (inventory_item_fly.xml), первым ребёнком - под остальными
    if (uiXml.NavigateToNode("background_frame", 0))
    {
        CUIFrameWindow* back = xr_new<CUIFrameWindow>();
        back->SetAutoDelete(true);
        AttachChild(back);
        xml_init.InitFrameWindow(uiXml, "background_frame", 0, back);
        m_op22_back = back; // OP22
    }

    if (uiXml.NavigateToNode("static_name", 0))
    {
        UIName = xr_new<CUIStatic>();
        AttachChild(UIName);
        UIName->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "static_name", 0, UIName);
        m_op22_name_auto = !!uiXml.ReadAttribInt("static_name", 0, "op22_name_auto", 0); // OP22
        if (uiXml.ReadAttribInt("static_name", 0, "op22_name_auto", 0) == 2) // OP22: GAMMA 2.0 - панель описания
            m_op22_name_shift = 2;
        m_op22_name_h = UIName->GetHeight();
    }
    if (uiXml.NavigateToNode("static_weight", 0))
    {
        UIWeight = xr_new<CUIStatic>();
        AttachChild(UIWeight);
        UIWeight->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "static_weight", 0, UIWeight);
        m_op22_weight_fmt = !!uiXml.ReadAttribInt("static_weight", 0, "op22_fmt", 0); // OP22
    }

    if (uiXml.NavigateToNode("static_cost", 0))
    {
        UICost = xr_new<CUIStatic>();
        AttachChild(UICost);
        UICost->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "static_cost", 0, UICost);
        m_op22_cost_gap = uiXml.ReadAttribFlt("static_cost", 0, "op22_after_weight", -1.f); // OP22
    }

    if (uiXml.NavigateToNode("static_condition", 0))
    {
        UICondition = xr_new<CUIStatic>();
        AttachChild(UICondition);
        UICondition->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "static_condition", 0, UICondition);
    }

    if (uiXml.NavigateToNode("condition_progress", 0))
    {
        UICondProgresBar = xr_new<CUIProgressBar>();
        AttachChild(UICondProgresBar);
        UICondProgresBar->SetAutoDelete(true);
        xml_init.InitProgressBar(uiXml, "condition_progress", 0, UICondProgresBar);
        // OP22: op22_cond_auto="1" - шкала и подпись (узел op22_cond_lbl) только у предметов с состоянием, без них описание
        // поднимается на op22_cond_h (как на макете: у аптечки строки «Состояние» нет)
        m_op22_cond_auto = !!uiXml.ReadAttribInt("condition_progress", 0, "op22_cond_auto", 0);
        m_op22_cond_h = uiXml.ReadAttribFlt("condition_progress", 0, "op22_cond_h", 0.f);
    }
    if (uiXml.NavigateToNode("op22_cond_lbl", 0))
    {
        m_op22_cond_lbl = xr_new<CUIStatic>();
        AttachChild(m_op22_cond_lbl);
        m_op22_cond_lbl->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "op22_cond_lbl", 0, m_op22_cond_lbl);
    }

    if (uiXml.NavigateToNode("op22_cond_val", 0)) // OP22: GAMMA 2.0
    {
        m_op22_cond_val = xr_new<CUIStatic>();
        AttachChild(m_op22_cond_val);
        m_op22_cond_val->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "op22_cond_val", 0, m_op22_cond_val);
    }
    if (uiXml.NavigateToNode("descr_list", 0))
    {
        UIWpnParams = xr_new<CUIWpnParams>();
        UIWpnParams->InitFromXml(uiXml);
        m_op22_wpn_fixed = !!uiXml.ReadAttribInt("wpn_params", 0, "op22_fixed", 0); // OP22
        if (m_op22_wpn_fixed)
        {
            AttachChild(UIWpnParams);
            UIWpnParams->Show(false);
        }

        UIArtefactParams = xr_new<CUIArtefactParams>();
        UIArtefactParams->InitFromXml(uiXml);

        UIDesc = xr_new<CUIScrollView>();
        AttachChild(UIDesc);
        UIDesc->SetAutoDelete(true);
        xml_init.InitScrollView(uiXml, "descr_list", 0, UIDesc);
        m_op22_desc_y = UIDesc->GetWndPos().y; // OP22
        m_op22_desc_h = UIDesc->GetHeight();

        m_desc_info.bShowDescrText = !!uiXml.ReadAttribInt("descr_list", 0, "only_text_info", 1);
        m_op22_line_h = uiXml.ReadAttribFlt("descr_list", 0, "op22_line_h", 0.f); // OP22
        xml_init.InitFont(uiXml, "descr_list:font", 0, m_desc_info.uDescClr, m_desc_info.pDescFont);
    }

    if (uiXml.NavigateToNode("image_static", 0))
    {
        UIItemImage = xr_new<CUIStatic>();
        AttachChild(UIItemImage);
        UIItemImage->SetAutoDelete(true);
        xml_init.InitStatic(uiXml, "image_static", 0, UIItemImage);
        UIItemImage->TextureAvailable(true);

        UIItemImage->TextureOff();
        UIItemImage->ClipperOn();
        UIItemImageSize.set(UIItemImage->GetWidth(), UIItemImage->GetHeight());
        m_op22_fit = !!uiXml.ReadAttribInt("image_static", 0, "op22_fit", 0); // OP22
        m_op22_img_pos = UIItemImage->GetWndPos();
    }

    xml_init.InitAutoStaticGroup(uiXml, "auto", 0, this);
}

void CUIItemInfo::Init(float x, float y, float width, float height, LPCSTR xml_name)
{
    inherited::Init(x, y, width, height);
    Init(xml_name);
}

void CUIItemInfo::InitItem(CInventoryItem* pInvItem)
{
    m_pInvItem = pInvItem;
    if (!m_pInvItem)
        return;

    float op22_dy = 0.f; // OP22: на сколько имя выросло по тексту
    if (UIName)
    {
        UIName->SetText(pInvItem->Name());
        if (m_op22_name_auto)
        {
            CUIWindow* below[5] = {UIWeight, UICost, UICondProgresBar, m_op22_cond_lbl, m_op22_cond_val};
            if (!m_op22_base_ok)
            {
                for (int i = 0; i < 5; ++i)
                    if (below[i]) m_op22_base_y[i] = below[i]->GetWndPos().y;
                m_op22_base_ok = true;
            }
            UIName->SetHeight(m_op22_name_h);
            UIName->AdjustHeightToText();
            op22_dy = _max(0.f, UIName->GetHeight() - m_op22_name_h);
            UIName->SetHeight(m_op22_name_h + op22_dy);
            for (int i = 0; i < m_op22_name_shift; ++i)
                if (below[i]) below[i]->SetWndPos(Fvector2().set(below[i]->GetWndPos().x, m_op22_base_y[i] + op22_dy));
        }
    }
    if (UIWeight)
    {
        string256 str;
        sprintf_s(str, "%3.2f %s", pInvItem->Weight(), CStringTable().translate("st_kg").c_str()); // OP22: единица веса из строк (ОП - «кг»)
        if (m_op22_weight_fmt) // OP22: GAMMA 2.0 - «4,9 кг»: десятые, запятая, без лишних нулей
        {
            xr_string w = std::format("{:.1f}", pInvItem->Weight());
            if (w.size() > 2 && w.substr(w.size() - 2) == ".0")
                w.resize(w.size() - 2);
            for (auto& ch : w)
                if (ch == '.')
                    ch = ',';
            sprintf_s(str, "%s %s", w.c_str(), CStringTable().translate("st_kg").c_str());
        }
        UIWeight->SetText(str);
    }
    if (UICost)
    {
        static const char* StMoneyDescr = CStringTable().translate("ui_st_money_descr").c_str();  
        UICost->SetText(std::format("{} {}", InventoryUtilities::op22_group_digits(pInvItem->Cost()), StMoneyDescr).c_str()); // OP22: «2 800 р.»
        if (m_op22_cost_gap >= 0.f && UIWeight && UIWeight->GetFont()) // OP22: GAMMA 2.0 - цена за весом
        {
            float len = UIWeight->GetFont()->SizeOf_(UIWeight->GetText());
            UI()->ClientToScreenScaledWidth(len);
            UICost->SetWndPos(Fvector2().set(UIWeight->GetWndPos().x + UIWeight->GetTextX() + len + m_op22_cost_gap, UIWeight->GetWndPos().y));
        }
    }

    if (UICondProgresBar)
    {
        float cond = pInvItem->GetConditionToShow();
        const bool op22_show = !m_op22_cond_auto || op22_item_shows_condition(pInvItem); // OP22
        UICondProgresBar->Show(op22_show);
        UICondProgresBar->SetProgressPos(cond * 100.0f + 1.0f - EPS);
        if (m_op22_cond_lbl)
            m_op22_cond_lbl->Show(op22_show);
        if (m_op22_cond_val) // OP22: GAMMA 2.0
        {
            m_op22_cond_val->Show(op22_show);
            m_op22_cond_val->SetText(std::format("{}", iFloor(cond * 100.f + 0.5f)).c_str());
        }
        if (m_op22_cond_auto && UIDesc)
        {
            UIDesc->SetWndPos(Fvector2().set(UIDesc->GetWndPos().x, m_op22_desc_y + op22_dy - (op22_show ? 0.f : m_op22_cond_h)));
            UIDesc->SetHeight(m_op22_desc_h - op22_dy + (op22_show ? 0.f : m_op22_cond_h));
        }
    }
    else if (m_op22_name_auto && UIDesc) // OP22: без шкалы состояния описание тоже под именем
    {
        UIDesc->SetWndPos(Fvector2().set(UIDesc->GetWndPos().x, m_op22_desc_y + op22_dy));
        UIDesc->SetHeight(m_op22_desc_h - op22_dy);
    }

    if (UIDesc)
    {
        UIDesc->Clear();
        if (m_op22_wpn_fixed) // OP22: параметры оружия - только у оружия (TryAddWpnInfo покажет)
            UIWpnParams->Show(false);
        VERIFY(0 == UIDesc->GetSize());
        TryAddWpnInfo(pInvItem->object());
        TryAddArtefactInfo(pInvItem->object().cNameSect());
        TryAddCustomInfo(pInvItem->object());
        if (m_desc_info.bShowDescrText)
        {
            CUIStatic* pItem = xr_new<CUIStatic>();
            pItem->SetTextColor(m_desc_info.uDescClr);
            pItem->SetFont(m_desc_info.pDescFont);
            pItem->SetWidth(UIDesc->GetDesiredChildWidth());
            pItem->SetTextComplexMode(true);
            if (m_op22_line_h > 0.f && m_desc_info.pDescFont) // OP22: высота строки по макету
            {
                float fh = m_desc_info.pDescFont->CurrentHeight_();
                UI()->ClientToScreenScaledHeight(fh);
                pItem->m_pLines->op22_SetInterval(_max(0.f, m_op22_line_h - fh));
            }
            pItem->SetText(*pInvItem->ItemDescription());
            pItem->AdjustHeightToText();
            UIDesc->AddWindow(pItem, true);
        }
        UIDesc->ScrollToBegin();
        if (m_op22_autoh) // OP22: высота окна - по содержимому описания
        {
            float h = 0.f;
            for (u32 i = 0; i < UIDesc->GetSize(); ++i)
                h += UIDesc->GetItem(i)->GetHeight() + UIDesc->m_op22_vi();
            const float full = _min(m_op22_max_h, UIDesc->GetWndPos().y + h + m_op22_pad_b);
            const bool fits = UIDesc->GetWndPos().y + h + m_op22_pad_b <= m_op22_max_h + 0.5f;
            // OP22: всё влезло - список на полединицы выше содержимого: при равной высоте погрешность float давала
            // GetHeight() < высоты содержимого и CUIScrollView показывал полосу прокрутки (11.11, всплывающее описание)
            const float list_h = _max(0.f, full - UIDesc->GetWndPos().y - m_op22_pad_b) + (fits ? 0.5f : 0.f);
            UIDesc->SetHeight(list_h);
            SetHeight(full);
            if (m_op22_back)
                m_op22_back->SetHeight(full);
            // OP22: не влезло - полоса прокрутки рисуется внутри списка поверх правого края (числа шкал, текст): окно и список
            // шире на полосу, полоса встаёт справа от содержимого, раскладка содержимого прежняя
            if (m_op22_w0 < 0.f)
            {
                m_op22_w0 = GetWidth();
                m_op22_desc_w0 = UIDesc->GetWidth();
            }
            const bool overflow = !fits;
            const float sbw = overflow && UIDesc->ScrollBar() ? UIDesc->ScrollBar()->GetWidth() + 2.f : 0.f;
            UIDesc->SetWidth(m_op22_desc_w0 + sbw);
            if (UIDesc->ScrollBar()) // полоса - у нового правого края списка
                UIDesc->ScrollBar()->SetWndPos(Fvector2().set(UIDesc->GetWidth() - UIDesc->ScrollBar()->GetWidth(), UIDesc->ScrollBar()->GetWndPos().y));
            SetWidth(m_op22_w0 + sbw);
            if (m_op22_back)
                m_op22_back->SetWidth(m_op22_w0 + sbw);
        }
    }
    if (UIItemImage)
    {
        // Загружаем картинку
        pInvItem->m_icon_params.set_shader(UIItemImage);

        Frect rect = pInvItem->m_icon_params.original_rect();
        UIItemImage->TextureOn();
        UIItemImage->ClipperOn();
        Frect v_r = {0.0f, 0.0f, rect.width(), rect.height()};
        v_r.x2 *= UI()->get_current_kx();

        if (m_op22_fit && v_r.width() > 0 && v_r.height() > 0) // OP22: вписать в поле и поставить по центру
        {
            const float k = _min(UIItemImageSize.x / v_r.width(), UIItemImageSize.y / v_r.height());
            v_r.x2 *= k;
            v_r.y2 *= k;
            UIItemImage->GetUIStaticItem().SetRect(v_r);
            UIItemImage->SetWidth(v_r.width());
            UIItemImage->SetHeight(v_r.height());
            UIItemImage->SetWndPos(Fvector2().set(m_op22_img_pos.x + (UIItemImageSize.x - v_r.width()) / 2.f, m_op22_img_pos.y + (UIItemImageSize.y - v_r.height()) / 2.f));
        }
        else
        {
        UIItemImage->GetUIStaticItem().SetRect(v_r);
        UIItemImage->SetWidth(_min(v_r.width(), UIItemImageSize.x));
        UIItemImage->SetHeight(_min(v_r.height(), UIItemImageSize.y));
        }
    }
}

void CUIItemInfo::TryAddWpnInfo(CPhysicsShellHolder& obj)
{
    if (UIWpnParams->Check(obj) && !fis_zero(UIWpnParams->GetHeight())) // OP22: wpn_params высотой 0 - окно без параметров оружия (обыск, ящик)
    {
        UIWpnParams->SetInfo(obj);
        if (m_op22_wpn_fixed)
            UIWpnParams->Show(true);
        else
        UIDesc->AddWindow(UIWpnParams, false);
    }
}

void CUIItemInfo::TryAddArtefactInfo(const shared_str& af_section)
{
    if (UIArtefactParams->Check(af_section))
    {
        UIArtefactParams->SetInfo(af_section);
        UIDesc->AddWindow(UIArtefactParams, false);
    }
}

#include "script_game_object.h"

void CUIItemInfo::TryAddCustomInfo(CPhysicsShellHolder& obj)
{
    if (pSettings->line_exist("engine_callbacks", "ui_item_info_callback"))
    {
        const char* callback = pSettings->r_string("engine_callbacks", "ui_item_info_callback");
        if (luabind::functor<void> lua_function; ai().script_engine().functor(callback, lua_function))
        {
            lua_function(UIDesc, obj.lua_game_object());
        }
    }
}

void CUIItemInfo::Draw()
{
    if (m_pInvItem || m_b_force_drawing)
        inherited::Draw();
}
