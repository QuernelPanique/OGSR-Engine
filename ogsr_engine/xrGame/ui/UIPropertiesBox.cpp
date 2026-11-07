#include "stdafx.h"
#include "UIPropertiesBox.h"
#include "../hudmanager.h"
#include "../level.h"
#include "UIListBoxItem.h"
#include "UIXmlInit.h"
#include "UIStatic.h" // OP22

#define OFFSET_X (5)
#define OFFSET_Y (5)
#define FRAME_BORDER_WIDTH 20
#define FRAME_BORDER_HEIGHT 22

#define ITEM_HEIGHT (GetFont()->CurrentHeight() + 2.0f)

CUIPropertiesBox::CUIPropertiesBox()
{
    SetFont(HUD().Font().pFontArial14);
    m_UIListWnd.SetImmediateSelection(true);
}

CUIPropertiesBox::~CUIPropertiesBox()
{
    xr_delete(m_op22_title); // OP22
    xr_delete(m_op22_line);
}

void CUIPropertiesBox::op22_SetTitle(LPCSTR t) // OP22
{
    if (m_op22_title)
        m_op22_title->SetText(t ? t : "");
}

void CUIPropertiesBox::Init(float x, float y, float width, float height)
{
    inherited::Init(x, y, width, height);

    AttachChild(&m_UIListWnd);

    CUIXml xml_doc;
    xml_doc.Init(CONFIG_PATH, UI_PATH, "inventory_new.xml");

    LPCSTR t = xml_doc.Read("properties_box:texture", 0, "");
    R_ASSERT(t);
    InitTexture(t);

    CUIXmlInit::InitListBox(xml_doc, "properties_box:list", 0, &m_UIListWnd);
    // OP22: меню по макету
    m_op22_sel = xml_doc.ReadAttrib("properties_box:list", 0, "op22_sel", "");
    m_op22_min_w = xml_doc.ReadAttribFlt("properties_box", 0, "op22_min_w", 0.f);
    for (auto [node, st] : {std::pair<LPCSTR, CUIStatic**>{"properties_box:op22_title", &m_op22_title}, {"properties_box:op22_line", &m_op22_line}})
        if (!*st && xml_doc.NavigateToNode(node, 0))
        {
            *st = xr_new<CUIStatic>();
            CUIXmlInit::InitStatic(xml_doc, node, 0, *st);
        }

    m_UIListWnd.Init(OFFSET_X, OFFSET_Y, width - OFFSET_X * 2, height - OFFSET_Y * 2);
}

void CUIPropertiesBox::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    if (pWnd == &m_UIListWnd)
    {
        if (msg == LIST_ITEM_CLICKED)
        {
            GetMessageTarget()->SendMessage(this, PROPERTY_CLICKED, pData);
            Hide();
        }
    }
    inherited::SendMessage(pWnd, msg, pData);
}

bool CUIPropertiesBox::AddItem(const char* str, void* pData, u32 tag_value)
{
    CUIListBoxItem* itm = m_UIListWnd.AddItem(str);
    if (m_op22_sel.size()) // OP22: подсветка пункта по макету
        itm->InitTexture(m_op22_sel.c_str());
    itm->SetTAG(tag_value);
    itm->SetData(pData);

    return true;
}
void CUIPropertiesBox::RemoveItemByTAG(u32 tag) { m_UIListWnd.RemoveWindow(m_UIListWnd.GetItemByTAG(tag)); }

void CUIPropertiesBox::RemoveAll() { m_UIListWnd.Clear(); }

void CUIPropertiesBox::Show(const Frect& parent_rect, const Fvector2& point)
{
    Fvector2 prop_pos;
    Fvector2 prop_size = GetWndSize();

    if (point.x - prop_size.x > parent_rect.x1 && point.y + prop_size.y < parent_rect.y2)
    {
        prop_pos.set(point.x - prop_size.x, point.y);
    }
    else if (point.x - prop_size.x > parent_rect.x1 && point.y - prop_size.y > parent_rect.y1)
    {
        prop_pos.set(point.x - prop_size.x, point.y - prop_size.y);
    }
    else if (point.x + prop_size.x < parent_rect.x2 && point.y - prop_size.y > parent_rect.y1)
    {
        prop_pos.set(point.x, point.y - prop_size.y);
    }
    else
        prop_pos.set(point.x, point.y);

    SetWndPos(prop_pos);

    inherited::Show(true);
    inherited::Enable(true);

    ResetAll();

    GetParent()->SetMouseCapture(this, true);
    m_pOrignMouseCapturer = this;
    m_UIListWnd.Reset();
}

void CUIPropertiesBox::Hide()
{
    op22_SetTitle(""); // OP22: заголовок ставит окно перед показом
    CUIWindow::Show(false);
    CUIWindow::Enable(false);
    CUIWindow::Reset();

    if (GetParent()->GetMouseCapturer() == this)
        GetParent()->SetMouseCapture(this, false);
}

bool CUIPropertiesBox::OnMouse(float x, float y, EUIMessages mouse_action)
{
    bool cursor_on_box;

    if (x >= 0 && x < GetWidth() && y >= 0 && y < GetHeight())
        cursor_on_box = true;
    else
        cursor_on_box = false;

    if (mouse_action == WINDOW_LBUTTON_DOWN && !cursor_on_box)
    {
        Hide();
        return true;
    }

    return inherited::OnMouse(x, y, mouse_action);
}

void CUIPropertiesBox::AutoUpdateSize()
{
    SetHeight(m_UIListWnd.GetItemHeight() * m_UIListWnd.GetSize() + m_UIListWnd.GetVertIndent());
    m_UIListWnd.SetHeight(GetHeight());
    float f = float(m_UIListWnd.GetLongestLength() + m_UIListWnd.GetHorizIndent()) + 2;
    f = _max(f, m_op22_min_w); // OP22
    SetWidth(_max(20.0f, f));
    m_UIListWnd.SetWidth(_max(20.0f, f));
    m_UIListWnd.UpdateChildrenLenght();
}

CUIListBoxItem* CUIPropertiesBox::GetClickedItem() { return m_UIListWnd.GetSelectedItem(); }
void CUIPropertiesBox::Update() { inherited::Update(); }
void CUIPropertiesBox::Draw()
{
    inherited::Draw();
    Fvector2 p;
    GetAbsolutePos(p);
    if (m_op22_line) // OP22: линия над каждым пунктом, кроме первого
    {
        const float ih = m_UIListWnd.GetItemHeight();
        const Fvector2 lp = m_UIListWnd.GetWndPos();
        for (u32 i = 1; i < m_UIListWnd.GetSize(); ++i)
        {
            m_op22_line->SetWndPos(Fvector2().set(p.x, p.y + lp.y + float(i) * ih));
            m_op22_line->SetWidth(GetWidth());
            m_op22_line->Draw();
        }
    }
    if (m_op22_title && m_op22_title->GetText() && *m_op22_title->GetText()) // OP22: название предмета над рамкой
    {
        const Fvector2 tp = m_op22_title->GetWndPos(); // из разметки: смещение от левого верхнего угла меню (y < 0)
        const Fvector2 old = tp;
        m_op22_title->SetWndPos(Fvector2().set(p.x + tp.x, p.y + tp.y));
        m_op22_title->Draw();
        m_op22_title->SetWndPos(old);
    }
}

bool CUIPropertiesBox::OnKeyboard(int dik, EUIMessages keyboard_action)
{
    Hide();
    return true;
}
