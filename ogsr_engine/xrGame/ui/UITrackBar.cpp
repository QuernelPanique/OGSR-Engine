#include "StdAfx.h"

#include "UITrackBar.h"
#include "UIFrameLineWnd.h"
#include "UI3tButton.h"
#include "UITextureMaster.h"
#include "UIStatic.h" // OP22
#include "..\..\xr_3da\xr_input.h"

#define DEF_CONTROL_HEIGHT 21
#define FRAME_LINE_TEXTURE "ui_slider_e"
#define FRAME_LINE_TEXTURE_D "ui_slider_d"
#define SLIDER_TEXTURE "ui_slider_button"

CUITrackBar::CUITrackBar() : m_f_min(0), m_f_max(1), m_f_val(0), m_f_back_up(0), m_f_step(0.01f), m_b_is_float(true), m_b_invert(false)
{
    m_pFrameLine = xr_new<CUIFrameLineWnd>();
    AttachChild(m_pFrameLine);
    m_pFrameLine->SetAutoDelete(true);
    m_pFrameLine_d = xr_new<CUIFrameLineWnd>();
    m_pFrameLine_d->SetVisible(false);
    AttachChild(m_pFrameLine_d);
    m_pFrameLine_d->SetAutoDelete(true);
    m_pSlider = xr_new<CUI3tButton>();
    AttachChild(m_pSlider);
    m_pSlider->SetAutoDelete(true);
    //.	m_pSlider->SetOwner				(this);
}

bool CUITrackBar::OnMouse(float x, float y, EUIMessages mouse_action)
{
    CUIWindow::OnMouse(x, y, mouse_action);

    if (m_bCursorOverWindow)
    {
        if (pInput->iGetAsyncKeyState(MOUSE_1))
        {
            UpdatePosRelativeToMouse();
            return true;
        }
    }
    return false;
}

void CUITrackBar::Init(float x, float y, float width, float height)
{
    string128 buf;
    float item_height;
    float item_width;
    CUIWindow::Init(x, y, width, DEF_CONTROL_HEIGHT);

    item_height = CUITextureMaster::GetTextureHeight(strconcat(sizeof(buf), buf, FRAME_LINE_TEXTURE, "_b"));
    m_pFrameLine->Init(0, (height - item_height) / 2, width, item_height);
    m_pFrameLine->InitTexture(FRAME_LINE_TEXTURE);
    m_pFrameLine_d->Init(0, (height - item_height) / 2, width, item_height);
    m_pFrameLine_d->InitTexture(FRAME_LINE_TEXTURE_D);

    strconcat(sizeof(buf), buf, SLIDER_TEXTURE, "_e");
    item_width = CUITextureMaster::GetTextureWidth(buf);
    item_height = CUITextureMaster::GetTextureHeight(buf);

    // OP22: window keeps the texture width, else the value is centered on 3/4 of the plate on 16:9
    // item_width *= UI()->get_current_kx();

    m_pSlider->Init(0, (height - item_height) / 2, item_width, item_height);
    m_pSlider->InitTexture(SLIDER_TEXTURE);
}

void CUITrackBar::SetCurrentValue()
{
    if (m_b_is_float)
    {
        GetOptFloatValue(m_f_val, m_f_min, m_f_max);

        if (!fis_zero(m_f_min_xml) && m_f_min_xml > m_f_min)
            m_f_min = m_f_min_xml;

        if (!fis_zero(m_f_max_xml) && m_f_max_xml < m_f_max)
            m_f_max = m_f_max_xml;
    }
    else
    {
        GetOptIntegerValue(m_i_val, m_i_min, m_i_max);

        if (!fis_zero(m_f_min_xml) && m_f_min_xml > m_i_min)
            m_i_min = iFloor(m_f_min_xml);

        if (!fis_zero(m_f_max_xml) && m_f_max_xml < m_i_max)
            m_i_max = iFloor(m_f_max_xml);
    }

    UpdatePos();
}

//. #include "../HUDmanager.h"
void CUITrackBar::Draw()
{
    if (m_op22) // OP22: полоса и заливка под ползунком, значение справа
    {
        Frect r;
        GetAbsoluteRect(r);
        const float lh = m_op22_back->GetHeight();
        const float y = r.y1 + (r.height() - lh) / 2;
        const float knob_x = m_pSlider->GetWndPos().x;
        m_op22_back->SetWndPos(r.x1, y);
        m_op22_back->SetWidth(op22_track_w());
        m_op22_back->Draw();
        m_op22_fill->SetWndPos(r.x1, y);
        m_op22_fill->SetWidth(_max(0.f, knob_x));
        if (knob_x > 0.5f)
            m_op22_fill->Draw();
        m_op22_val->SetWndPos(r.x1 + op22_track_w(), r.y1);
        m_op22_val->SetHeight(r.height());
        m_op22_val->Draw();
    }
    CUIWindow::Draw();
}

void CUITrackBar::op22_setup(float val_w, float knob_w, float knob_h, float line_h, CGameFont* font, u32 color)
{
    m_op22 = true;
    m_op22_val_w = val_w;
    m_pFrameLine->Show(false);
    m_pFrameLine_d->Show(false);
    for (auto [st, tex] : {std::pair<CUIStatic**, LPCSTR>{&m_op22_back, "ui\\ct_tr_trk_back"}, {&m_op22_fill, "ui\\ct_tr_trk_fill"}, {&m_op22_val, nullptr}})
    {
        if (!*st)
            *st = xr_new<CUIStatic>();
        if (tex)
        {
            (*st)->InitTexture(tex);
            (*st)->SetStretchTexture(true);
            (*st)->SetWndSize(Fvector2().set(10.f, line_h));
        }
    }
    m_op22_val->SetWndSize(Fvector2().set(val_w, GetHeight()));
    m_op22_val->SetTextAlignment(CGameFont::alRight);
    m_op22_val->SetVTextAlignment(valCenter);
    if (font)
        m_op22_val->SetFont(font);
    m_op22_val->SetTextColor(color);
    m_pSlider->InitTexture("ui\\ct_tr_knob");
    m_pSlider->SetStretchTexture(true);
    m_pSlider->SetWidth(knob_w); // OP22: SetWidth/SetHeight - и фону кнопки (SetWndSize оставлял фону размер ui_slider_button 24x17, 08.11)
    m_pSlider->SetHeight(knob_h);
    m_pSlider->SetWndPos(Fvector2().set(0.f, (GetHeight() - knob_h) / 2)); // значение ставит SetCurrentValue: здесь ещё нет родителя
}

CUITrackBar::~CUITrackBar()
{
    xr_delete(m_op22_back); // OP22
    xr_delete(m_op22_fill);
    xr_delete(m_op22_val);
}

void CUITrackBar::SaveValue()
{
    CUIOptionsItem::SaveValue();

    string128 buf;
    if (m_b_is_float)
    {
        SaveOptFloatValue(m_f_val);
        if (m_f_step >= 1)
            sprintf_s(buf, "%2.0f", m_f_val);
        else if (m_f_step >= 0.1)
            sprintf_s(buf, "%3.1f", m_f_val);
        else
            sprintf_s(buf, "%4.2f", m_f_val);
    }
    else
    {
        SaveOptIntegerValue(m_i_val);
        sprintf_s(buf, "%d", m_i_val);
    }

    if (m_op22 && m_op22_fmt.size()) // OP22: вид значения по макету
    {
        const float v = m_b_is_float ? m_f_val : float(m_i_val);
        const float mn = m_b_is_float ? m_f_min : float(m_i_min), mx = m_b_is_float ? m_f_max : float(m_i_max);
        if (m_op22_fmt == "pct")
            sprintf_s(buf, "%d", iFloor((mx > mn ? (v - mn) / (mx - mn) : 0.f) * 100.f + 0.5f));
        else if (m_op22_fmt == "int")
            sprintf_s(buf, "%d", iFloor(v + 0.5f));
        else if (m_op22_fmt == "f1")
            sprintf_s(buf, "%.1f", v);
        else if (m_op22_fmt == "f2")
            sprintf_s(buf, "%.2f", v);
    }
    if (m_op22) // OP22: значение справа, не на ползунке
    {
        m_op22_val->SetText(buf);
        m_pSlider->SetText("");
    }
    else
        m_pSlider->SetText(buf);
    GetMessageTarget()->SendMessage(this, TRACKBAR_CHANGED);
}

float CUITrackBar::GetTrackValue()
{
    if (m_b_is_float)
    {
        return (m_f_val);
    }
    else
    {
        return float(m_i_val);
    }
}

bool CUITrackBar::IsChanged()
{
    if (m_b_is_float)
    {
        return !fsimilar(m_f_back_up, m_f_val);
    }
    else
    {
        return (m_i_back_up != m_i_val);
    }
}

void CUITrackBar::SetStep(float step)
{
    if (m_b_is_float)
        m_f_step = step;
    else
        m_i_step = iFloor(step);
}

void CUITrackBar::SeveBackUpValue()
{
    if (m_b_is_float)
        m_f_back_up = m_f_val;
    else
        m_i_back_up = m_i_val;
}

void CUITrackBar::Undo()
{
    if (m_b_is_float)
        m_f_val = m_f_back_up;
    else
        m_i_val = m_i_back_up;

    SaveValue();
    SetCurrentValue();
}

void CUITrackBar::Enable(bool status)
{
    m_bIsEnabled = status;
    m_pFrameLine->SetVisible(status);
    m_pSlider->Enable(status);
    m_pFrameLine_d->SetVisible(!status);
}

void CUITrackBar::UpdatePosRelativeToMouse()
{
    float _bkf = 0.0f;
    int _bki = 0;
    if (m_b_is_float)
    {
        _bkf = m_f_val;
    }
    else
    {
        _bki = m_i_val;
    }

    float btn_width = m_pSlider->GetWidth();
    float window_width = op22_track_w(); // OP22
    float fpos = cursor_pos.x;

    if (GetInvert())
        fpos = window_width - fpos;

    if (fpos < btn_width / 2)
        fpos = btn_width / 2;
    else if (fpos > window_width - btn_width / 2)
        fpos = window_width - btn_width / 2;

    float __fval;
    float __fmax = (m_b_is_float) ? m_f_max : (float)m_i_max;
    float __fmin = (m_b_is_float) ? m_f_min : (float)m_i_min;
    float __fstep = (m_b_is_float) ? m_f_step : (float)m_i_step;

    __fval = (__fmax - __fmin) * (fpos - btn_width / 2) / (window_width - btn_width) + __fmin;

    float _d = (__fval - __fmin);

    float _v = _d / __fstep;
    int _vi = iFloor(_v);
    float _vf = __fstep * _vi;

    if (_d - _vf > __fstep / 2.0f)
        _vf += __fstep;

    __fval = __fmin + _vf;

    clamp(__fval, __fmin, __fmax);

    if (m_b_is_float)
        m_f_val = __fval;
    else
        m_i_val = iFloor(__fval);

    bool b_ch = false;
    if (m_b_is_float)
    {
        b_ch = !fsimilar(_bkf, m_f_val);
    }
    else
    {
        b_ch = (_bki != m_i_val);
    }

    if (b_ch)
        GetMessageTarget()->SendMessage(this, BUTTON_CLICKED, NULL);

    UpdatePos();
}

void CUITrackBar::UpdatePos()
{
#ifdef DEBUG

    if (m_b_is_float)
        R_ASSERT2(m_f_val >= m_f_min && m_f_val <= m_f_max, "CUITrackBar::UpdatePos() - m_val >= m_min && m_val <= m_max");
    else
        R_ASSERT2(m_i_val >= m_i_min && m_i_val <= m_i_max, "CUITrackBar::UpdatePos() - m_val >= m_min && m_val <= m_max");

#endif

    float btn_width = m_pSlider->GetWidth();
    float window_width = op22_track_w(); // OP22
    float free_space = window_width - btn_width;
    Fvector2 pos = m_pSlider->GetWndPos();

    float __fval = (m_b_is_float) ? m_f_val : (float)m_i_val;
    float __fmax = (m_b_is_float) ? m_f_max : (float)m_i_max;
    float __fmin = (m_b_is_float) ? m_f_min : (float)m_i_min;

    pos.x = (__fval - __fmin) * free_space / (__fmax - __fmin);
    if (GetInvert())
        pos.x = free_space - pos.x;

    m_pSlider->SetWndPos(pos);
    SaveValue();
}

void CUITrackBar::OnMessage(const char* message)
{
    if (0 == xr_strcmp(message, "set_default_value"))
    {
        if (m_b_is_float)
            m_f_val = m_f_min + (m_f_max - m_f_min) / 2.0f;
        else
            m_i_val = m_i_min + iFloor((m_i_max - m_i_min) / 2.0f);

        UpdatePos();
    }
}

bool CUITrackBar::GetCheck()
{
    VERIFY(!m_b_is_float);
    return !!m_i_val;
}

void CUITrackBar::SetCheck(bool b)
{
    VERIFY(!m_b_is_float);
    m_i_val = (b) ? m_i_max : m_i_min;
}
