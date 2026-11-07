// file:		UIustomSpin.cpp
// description:	base class for CSpinNum & CSpinText
// created:		15.06.2005
// author:		Serge Vynnychenko
// mail:		narrator@gsc-game.kiev.ua
//
// copyright 2005 GSC Game World

#include "StdAfx.h"
#include "UI3tButton.h"
#include "UIStatic.h" // OP22
#include "UIFrameLineWnd.h"
#include "UILines.h"
#include "UICustomSpin.h"

#define SPIN_HEIGHT 22
#define BTN_SIZE 11

CUICustomSpin::CUICustomSpin()
{
    m_pFrameLine = xr_new<CUIFrameLineWnd>();
    m_pBtnUp = xr_new<CUI3tButton>();
    m_pBtnDown = xr_new<CUI3tButton>();
    m_pLines = xr_new<CUILines>();

    m_pFrameLine->SetAutoDelete(true);
    m_pBtnUp->SetAutoDelete(true);
    m_pBtnDown->SetAutoDelete(true);

    AttachChild(m_pFrameLine);
    AttachChild(m_pBtnUp);
    AttachChild(m_pBtnDown);
    m_pLines->SetTextAlignment(CGameFont::alLeft);
    m_pLines->SetVTextAlignment(valCenter);
    m_pLines->SetFont(UI()->Font()->pFontLetterica16Russian);
    m_pLines->SetTextColor(color_argb(255, 235, 219, 185));

    m_time_begin = 0;
    m_p_delay = 500;
    m_u_delay = 0;

    m_textColor[0] = color_argb(255, 235, 219, 185);
    m_textColor[1] = color_argb(255, 100, 100, 100);
}

CUICustomSpin::~CUICustomSpin()
{
    xr_delete(m_pLines);
    xr_delete(m_op22_bg); // OP22
}

void CUICustomSpin::Init(float x, float y, float width, float height)
{
    if (m_op22_tex.size()) // OP22: вид по макету
    {
        CUIWindow::Init(x, y, width, height);
        m_pFrameLine->Init(0, 0, width, height);
        m_pFrameLine->InitTexture("ui_spiner");
        m_pFrameLine->Show(false);
        if (!m_op22_bg)
            m_op22_bg = xr_new<CUIStatic>();
        m_op22_bg->InitTexture(m_op22_tex.c_str());
        m_op22_bg->SetStretchTexture(true);
        const float bw = height * 0.75f;
        m_pBtnUp->Init(width - bw, 0, bw, height / 2);
        m_pBtnUp->InitTexture("ui\\ct_tr_none");
        m_pBtnDown->Init(width - bw, height / 2, bw, height / 2);
        m_pBtnDown->InitTexture("ui\\ct_tr_none");
        m_pLines->Init(0, 0, width - bw - 6, height);
        m_pLines->SetVTextAlignment(valCenter);
        return;
    }
    CUIWindow::Init(x, y, width, SPIN_HEIGHT);
    m_pFrameLine->Init(0, 0, width, SPIN_HEIGHT);
    m_pFrameLine->InitTexture("ui_spiner");
    m_pBtnUp->Init(width - BTN_SIZE - 1, 0, BTN_SIZE, BTN_SIZE);
    m_pBtnUp->InitTexture("ui_spiner_button_t");
    m_pBtnDown->Init(width - BTN_SIZE - 1, BTN_SIZE + 1, BTN_SIZE, BTN_SIZE);
    m_pBtnDown->InitTexture("ui_spiner_button_b");

    m_pLines->Init(0, 0, width - BTN_SIZE - 10, SPIN_HEIGHT);
}

void CUICustomSpin::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    if (BUTTON_CLICKED == msg)
    {
        if (m_pBtnUp == pWnd)
        {
            OnBtnUpClick();
        }
        else if (m_pBtnDown == pWnd)
        {
            OnBtnDownClick();
        }
    }
}

void CUICustomSpin::Enable(bool status)
{
    CUIWindow::Enable(status);
    m_pBtnDown->Enable(status);
    m_pBtnUp->Enable(status);

    if (!status)
        m_pLines->SetTextColor(m_textColor[0]); // enabled color
    else
        m_pLines->SetTextColor(m_textColor[1]); // disabled color
}

void CUICustomSpin::OnBtnUpClick() { GetMessageTarget()->SendMessage(this, BUTTON_CLICKED); }

void CUICustomSpin::OnBtnDownClick() { GetMessageTarget()->SendMessage(this, BUTTON_CLICKED); }

void CUICustomSpin::Draw()
{
    Fvector2 pos;
    GetAbsolutePos(pos);
    if (m_op22_bg) // OP22: фон под текстом и стрелками
    {
        m_op22_bg->SetWndRect(pos.x, pos.y, GetWidth(), GetHeight());
        m_op22_bg->Draw();
    }
    CUIWindow::Draw();
    m_pLines->Draw(pos.x + (m_op22_bg ? 8.f : 3.f), pos.y);
}

void CUICustomSpin::Update()
{
    CUIWindow::Update();
    if (!m_pBtnUp->CursorOverWindow())
        m_pBtnUp->SetButtonMode(CUIButton::BUTTON_NORMAL);
    if (!m_pBtnDown->CursorOverWindow())
        m_pBtnDown->SetButtonMode(CUIButton::BUTTON_NORMAL);

    if (CUIButton::BUTTON_PUSHED == m_pBtnUp->GetButtonsState() && m_pBtnUp->CursorOverWindow())
    {
        if (m_time_begin < Device.dwTimeContinual - m_p_delay)
        {
            m_time_begin = Device.dwTimeContinual;
            float tmp = float(m_u_delay);
            float step = powf(tmp, 0.7f);
            while (tmp > 0)
            {
                IncVal();
                tmp -= step;
            };

            m_u_delay += 50;

            if (m_p_delay > 50)
                m_p_delay -= 50;
        }
    }
    else if (CUIButton::BUTTON_PUSHED == m_pBtnDown->GetButtonsState() && m_pBtnDown->CursorOverWindow())
    {
        if (m_time_begin < Device.dwTimeContinual - m_p_delay)
        {
            m_time_begin = Device.dwTimeContinual;
            float tmp = float(m_u_delay);
            float step = powf(tmp, 0.7f);
            while (tmp > 0)
            {
                DecVal();
                tmp -= step;
            };

            m_u_delay += 50;

            if (m_p_delay > 50)
                m_p_delay -= 50;
        }
    }
    else
    {
        m_p_delay = 500;
        m_u_delay = 0;
        m_time_begin = 0;
    }

    if (IsEnabled())
    {
        m_pBtnUp->Enable(CanPressUp());
        m_pBtnDown->Enable(CanPressDown());
        m_pLines->SetTextColor(m_textColor[0]);
    }
    else
    {
        m_pBtnUp->Enable(false);
        m_pBtnDown->Enable(false);
        m_pLines->SetTextColor(m_textColor[1]);
    }
}

LPCSTR CUICustomSpin::GetText() { return m_pLines->GetText(); }

void CUICustomSpin::SetTextColor(u32 color) { m_textColor[0] = color; }

void CUICustomSpin::SetTextColorD(u32 color) { m_textColor[1] = color; }