//////////////////////////////////////////////////////////////////////
// UIPdaMsgListItem.cpp: элемент окна списка в основном
// экране для сообщений PDA
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "UIPdaMsgListItem.h"
#include "../Entity.h"
#include "../character_info.h"
#include "UIInventoryUtilities.h"
#include "xrUIXmlParser.h"
#include "UIXmlInit.h"
#include "uicoloranimatorwrapper.h"
#include "../object_broker.h"
#include "UILines.h" // OP22
#include "UIFrameWindow.h" // OP22
#include "../ui_base.h" // OP22

#define PDA_MSG_MAINGAME_CHAR "maingame_pda_msg.xml"

using namespace InventoryUtilities;

void CUIPdaMsgListItem::SetFont(CGameFont* pFont) { UIMsgText.SetFont(pFont); }

void CUIPdaMsgListItem::Init(float x, float y, float width, float height)
{
    CUIStatic::Init(x, y, width, height);

    CUIXml uiXml;
    bool xml_result = uiXml.Init(CONFIG_PATH, UI_PATH, PDA_MSG_MAINGAME_CHAR);
    R_ASSERT2(xml_result, "xml file not found");

    CUIXmlInit xml_init;
    if (uiXml.NavigateToNode("op22_back", 0)) // OP22: подложка сообщения по макету
    {
        if (uiXml.NavigateToNode("op22_back:frame", 0))
        {
            m_op22_frame = xr_new<CUIFrameWindow>();
            m_op22_frame->SetAutoDelete(true);
            AttachChild(m_op22_frame);
            xml_init.InitFrameWindow(uiXml, "op22_back:frame", 0, m_op22_frame);
        }
        else
        {
            CUIXmlInit::InitTexture(uiXml, "op22_back", 0, this);
            SetStretchTexture(true);
        }
        m_op22_pad = uiXml.ReadAttribFlt("op22_back", 0, "pad", 0.f);
        m_op22_gap = uiXml.ReadAttribFlt("op22_back", 0, "gap", 0.f);
        m_op22_time_color = uiXml.ReadAttrib("op22_back", 0, "time_color", "");
        m_op22_time_sep = uiXml.ReadAttrib("op22_back", 0, "time_sep", "");
        m_op22_recolor = uiXml.ReadAttrib("op22_back", 0, "recolor", "");
        m_op22_line_h = uiXml.ReadAttribFlt("op22_back", 0, "line_h", 0.f);
        m_op22_pad_b = uiXml.ReadAttribFlt("op22_back", 0, "pad_b", -1.f);
        m_op22 = true;
    }
    AttachChild(&UIIcon);
    xml_init.InitStatic(uiXml, "icon_static", 0, &UIIcon);

    AttachChild(&UIName);
    if (uiXml.NavigateToNode("name_static", 0))
        xml_init.InitStatic(uiXml, "name_static", 0, &UIName);
    else
    {
        UIName.Show(false);
        UIName.Enable(false);
    }
    AttachChild(&UIMsgText);
    xml_init.InitStatic(uiXml, "text_static", 0, &UIMsgText);
}

void CUIPdaMsgListItem::SetTextColor(u32 color) { UIMsgText.SetTextColor(color); }

// OP22: GAMMA 2.0 - «09:52 · » серым, отправитель цветом макета, строка заданной высоты
void CUIPdaMsgListItem::op22_format()
{
    xr_string t = UIMsgText.GetText() ? UIMsgText.GetText() : "";
    if (!m_op22_time_color.empty() && t.size() > 6 && isdigit((u8)t[0]) && isdigit((u8)t[1]) && t[2] == ':' && isdigit((u8)t[3]) && isdigit((u8)t[4]) && t[5] == ' ')
        t = "%c[" + m_op22_time_color + "]" + t.substr(0, 5) + (m_op22_time_sep.empty() ? " " : m_op22_time_sep) + "%c[default]" + t.substr(6);
    if (!m_op22_recolor.empty())
    {
        // пары имя=r,g,b,a через «;»
        xr_string rc = m_op22_recolor;
        size_t p = 0;
        while (p < rc.size())
        {
            size_t e = rc.find(';', p);
            if (e == xr_string::npos)
                e = rc.size();
            const xr_string pair = rc.substr(p, e - p);
            const size_t eq = pair.find('=');
            if (eq != xr_string::npos)
            {
                const xr_string from = "%c[" + pair.substr(0, eq) + "]", to = "%c[" + pair.substr(eq + 1) + "]";
                for (size_t q = t.find(from); q != xr_string::npos; q = t.find(from, q + to.size()))
                    t.replace(q, from.size(), to);
            }
            p = e + 1;
        }
    }
    UIMsgText.SetText(t.c_str());
    if (m_op22_line_h > 0.f && UIMsgText.GetFont())
    {
        float fh = UIMsgText.GetFont()->CurrentHeight_();
        UI()->ClientToScreenScaledHeight(fh);
        UIMsgText.m_pLines->op22_SetInterval(_max(0.f, m_op22_line_h - fh));
    }
}

void CUIPdaMsgListItem::SetColor(u32 color) { UIIcon.SetColor(color); }

void CUIPdaMsgListItem::InitCharacter(CInventoryOwner* pInvOwner)
{
    VERIFY(pInvOwner);

    string256 str;
    sprintf_s(str, "name: %s", pInvOwner->Name());
    UIName.SetText(str);

    UIIcon.InitTexture(pInvOwner->CharacterInfo().IconName().c_str());
    /*
        UIIcon.SetShader(GetCharIconsShader());
        UIIcon.GetUIStaticItem().SetOriginalRect(
                        float(pInvOwner->CharacterInfo().TradeIconX()*ICON_GRID_WIDTH),
                        float(pInvOwner->CharacterInfo().TradeIconY()*ICON_GRID_HEIGHT),
                        float(pInvOwner->CharacterInfo().TradeIconX()+CHAR_ICON_WIDTH*ICON_GRID_WIDTH),
                        float(pInvOwner->CharacterInfo().TradeIconY()+CHAR_ICON_HEIGHT*ICON_GRID_HEIGHT));
    */
}
