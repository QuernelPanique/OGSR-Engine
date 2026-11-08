#include "stdafx.h"

#include "UIEditKeyBind.h"

#include <dinput.h>

#include "UIColorAnimatorWrapper.h"
#include "../xr_level_controller.h"
#include "../object_broker.h"
#include "UIStatic.h" // OP22

CUIEditKeyBind::CUIEditKeyBind(bool bPrim)
{
    m_bPrimary = bPrim;
    m_bEditMode = false;

    m_pAnimation = xr_new<CUIColorAnimatorWrapper>("ui_map_area_anim");
    m_pAnimation->Cyclic(true);
    m_bChanged = false;
    m_lines.SetTextComplexMode(false);
    m_keyboard = NULL;
    m_action = NULL;
}
CUIEditKeyBind::~CUIEditKeyBind()
{
    delete_data(m_pAnimation);
    xr_delete(m_op22_box); // OP22
}

u32 cut_string_by_length(CGameFont* pFont, LPCSTR src, LPSTR dst, u32 dst_size, float length)
{
    if (pFont->IsMultibyte())
    {
        u16 nPos = pFont->GetCutLengthPos(length, src);
        VERIFY(nPos < dst_size);
        strncpy(dst, src, nPos);
        dst[nPos] = '\0';
        return nPos;
    }
    else
    {
        float text_len = pFont->SizeOf_(src);
        UI()->ClientToScreenScaledWidth(text_len);
        VERIFY(xr_strlen(src) <= dst_size);
        strcpy(dst, src);

        while (text_len > length)
        {
            dst[xr_strlen(dst) - 1] = 0;
            VERIFY(xr_strlen(dst));
            text_len = pFont->SizeOf_(dst);
            UI()->ClientToScreenScaledWidth(text_len);
        }

        return xr_strlen(dst);
    }
}

void CUIEditKeyBind::op22_setup(LPCSTR box_tex, float pad, LPCSTR empty, u32 empty_clr, float box_h, float box_minw) // OP22
{
    m_op22_pad = pad;
    m_op22_box_h = box_h;
    m_op22_box_minw = box_minw;
    m_op22_empty = empty;
    m_op22_empty_clr = empty_clr;
    m_op22_clr = m_lines.GetTextColor();
    InitTexture("ui\\ct_tr_none");
    if (box_tex && *box_tex && !m_op22_box)
    {
        m_op22_box = xr_new<CUIStatic>();
        m_op22_box->InitTexture(box_tex);
        m_op22_box->SetStretchTexture(true);
    }
    m_textPos.x = pad;
    SetText(m_lines.GetText());
}

void CUIEditKeyBind::Draw() // OP22: рамка по ширине клавиши под текстом
{
    if (m_op22_box && !m_op22_is_empty)
    {
        float w = CUILinesOwner::GetFont()->SizeOf_(m_lines.GetText());
        UI()->ClientToScreenScaledWidth(w);
        Fvector2 p;
        GetAbsolutePos(p);
        const float bw = _max(m_op22_box_minw, w + 2.f * m_op22_pad);
        const float bh = m_op22_box_h > 0.f ? _min(m_op22_box_h, GetHeight()) : GetHeight();
        m_op22_box->SetWndPos(Fvector2().set(p.x, p.y + (GetHeight() - bh) / 2.f));
        m_op22_box->SetWndSize(Fvector2().set(bw, bh));
        m_op22_box->Draw();
        if (m_op22_box_minw > 0.f) // OP22: текст по центру плашки (на макете клавиша в центре рамки)
            m_textPos.x = (bw - w) / 2.f;
    }
    CUILabel::Draw();
}

void CUIEditKeyBind::SetText(const char* text)
{
    m_op22_is_empty = !text || 0 == xr_strlen(text) || (m_op22_empty.size() && !xr_strcmp(text, m_op22_empty.c_str()));
    if (m_op22_empty.size()) // OP22
        m_lines.SetTextColor(m_op22_is_empty ? m_op22_empty_clr : m_op22_clr);
    if (!text || 0 == xr_strlen(text))
        CUILabel::SetText(m_op22_empty.size() ? m_op22_empty.c_str() : "---");
    else
    {
        string256 buff;

        cut_string_by_length(CUILinesOwner::GetFont(), text, buff, sizeof(buff), GetWidth());

        CUILabel::SetText(buff);
    }
}

void CUIEditKeyBind::Init(float x, float y, float width, float height)
{
    CUILabel::Init(x, y, width, height);
    InitTexture("ui_options_string");
}

void CUIEditKeyBind::InitTexture(LPCSTR texture, bool bHorizontal) { CUILabel::InitTexture(texture, bHorizontal); }

void CUIEditKeyBind::OnFocusLost()
{
    CUILabel::OnFocusLost();
    m_bEditMode = false;
    m_lines.SetTextColor((subst_alpha(m_lines.GetTextColor(), color_get_A(0xffffffff))));
}

bool CUIEditKeyBind::OnMouseDown(int mouse_btn)
{
    if (m_bEditMode)
    {
        string64 message;

        m_keyboard = dik_to_ptr(mouse_btn, true);
        if (!m_keyboard)
            return true;
        SetText(m_keyboard->key_local_name.c_str());
        OnFocusLost();
        m_bChanged = true;

        strcpy_s(message, m_action->action_name);
        strcat_s(message, "=");
        strcat_s(message, m_keyboard->key_name);
        SendMessage2Group("key_binding", message);

        return true;
    }

    if (mouse_btn == MOUSE_1)
        m_bEditMode = m_bCursorOverWindow;

    return CUILabel::OnMouseDown(mouse_btn);
}

bool CUIEditKeyBind::OnKeyboard(int dik, EUIMessages keyboard_action)
{
    if (dik == MOUSE_1 || dik == MOUSE_2 || dik == MOUSE_3)
        return false;
    if (CUILabel::OnKeyboard(dik, keyboard_action))
        return true;

    string64 message;
    if (m_bEditMode)
    {
        if (dik == DIK_DELETE)
        {
            if (m_keyboard)
            {
                SetText("---");
                m_keyboard = nullptr;
                OnFocusLost();
                m_bChanged = true;
                return true;
            }
        }

        m_keyboard = dik_to_ptr(dik, true);
        if (!m_keyboard)
            return true;

        strcpy_s(message, m_action->action_name);
        strcat_s(message, "=");
        strcat_s(message, m_keyboard->key_name);
        SetText(m_keyboard->key_local_name.c_str());
        OnFocusLost();
        m_bChanged = true;
        SendMessage2Group("key_binding", message);
        return true;
    }
    return false;
}

void CUIEditKeyBind::Update()
{
    CUILabel::Update();

    m_bTextureAvailable = m_bCursorOverWindow;
    if (m_bEditMode)
    {
        m_pAnimation->Update();
        m_lines.SetTextColor((subst_alpha(m_lines.GetTextColor(), color_get_A(m_pAnimation->GetColor()))));
    }
}

void CUIEditKeyBind::Register(const char* entry, const char* group)
{
    CUIOptionsItem::Register(entry, group);
    m_action = action_name_to_ptr(entry);
    ASSERT_FMT(m_action, "Action [%s] not found. Group: [%s]", entry, group);
}

void CUIEditKeyBind::SetCurrentValue()
{
    _binding* pbinding = &g_key_bindings.at(m_action->id);

    int idx = (m_bPrimary) ? 0 : 1;
    m_keyboard = pbinding->m_keyboard[idx];

    if (m_keyboard)
        SetText(m_keyboard->key_local_name.c_str());
    else
        SetText(NULL);
}

void CUIEditKeyBind::SaveValue()
{
    CUIOptionsItem::SaveValue();

    BindAction2Key();
    m_bChanged = false;
}

#include "..\..\xr_3da\xr_ioconsole.h"
void CUIEditKeyBind::BindAction2Key()
{
    xr_string comm_unbind = (m_bPrimary) ? "unbind " : "unbind_sec ";
    comm_unbind += m_action->action_name;
    Console->Execute(comm_unbind.c_str());

    if (m_keyboard)
    {
        xr_string comm_bind = (m_bPrimary) ? "bind " : "bind_sec ";
        comm_bind += m_action->action_name;
        comm_bind += " ";
        comm_bind += m_keyboard->key_name;
        Console->Execute(comm_bind.c_str());
    }
}

bool CUIEditKeyBind::IsChanged() { return m_bChanged; }

void CUIEditKeyBind::OnMessage(const char* message)
{
    // message = "command=key"
    int eq = (int)strcspn(message, "=");

    if (!m_keyboard)
        return;

    if (0 != xr_strcmp(m_keyboard->key_name, message + eq + 1))
        return;

    string64 command;
    strcpy_s(command, message);
    command[eq] = 0;

    if (0 == xr_strcmp(m_action->action_name, command))
        return; // fuck

    SetText("---");
    m_keyboard = NULL;
}