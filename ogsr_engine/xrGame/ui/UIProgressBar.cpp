#include "stdafx.h"
#include "uiprogressbar.h"

CUIProgressBar::CUIProgressBar()
{
    m_MinPos = 1.0f;
    m_MaxPos = 1.0f + EPS;

    Enable(false);

    m_bBackgroundPresent = false;
    m_bUseColor = false;
    m_bUseGradient = true;

    AttachChild(&m_UIBackgroundItem);
    AttachChild(&m_UIProgressItem);
    m_ProgressPos.x = 0.0f;
    m_ProgressPos.y = 0.0f;
    m_inertion = 0.0f;
    m_last_render_frame = u32(-1);
    m_orient_mode = om_horz;
    m_animated = !Core.Features.test(xrCore::Feature::no_progress_bar_animation);
}

CUIProgressBar::~CUIProgressBar()
{
    for (auto& c : m_op22_cap)
        xr_delete(c);
}

void CUIProgressBar::op22_caps(LPCSTR tex, const Fvector3& g)
{
    m_op22_cap_g = g;
    static constexpr LPCSTR sfx[3]{"_l", "_m", "_r"};
    for (int i = 0; i < 3; ++i)
    {
        string256 t;
        strconcat(sizeof(t), t, tex, sfx[i]);
        xr_delete(m_op22_cap[i]);
        m_op22_cap[i] = xr_new<CUIStatic>(); // не ребёнок: рисует Draw шкалы
        m_op22_cap[i]->InitTexture(t);
        m_op22_cap[i]->SetStretchTexture(true);
    }
}

void CUIProgressBar::InitProgressBar(Fvector2 pos, Fvector2 size, EOrientMode mode)
{
    m_orient_mode = mode;
    SetWndPos(pos);
    SetWndSize(size);
    UpdateProgressBar();
}

void CUIProgressBar::UpdateProgressBar()
{
    if (fsimilar(m_MaxPos, m_MinPos))
        m_MaxPos += EPS;

    float progressbar_unit = 1 / (m_MaxPos - m_MinPos);

    float fCurrentLength = m_ProgressPos.x * progressbar_unit;
    if (m_op22_steps > 0) // OP22
        fCurrentLength = std::round(fCurrentLength * m_op22_steps) / float(m_op22_steps);

    switch (m_orient_mode)
    {
    case om_horz:
    case om_back:
    case om_fromcenter: m_CurrentLength = GetWidth() * fCurrentLength; break;
    case om_vert:
    case om_down:
    case om_vfromcenter: m_CurrentLength = GetHeight() * fCurrentLength; break;
    default: m_CurrentLength = 0.0f;
    }

    if (m_bUseColor)
    {
        if (m_bUseGradient)
        {
            Fcolor curr;
            curr.lerp(m_minColor, m_middleColor, m_maxColor, fCurrentLength);
            m_UIProgressItem.SetTextureColor(curr.get());
        }
        else
            m_UIProgressItem.SetTextureColor(m_maxColor.get());
    }
}

void CUIProgressBar::SetProgressPos(float _Pos)
{
    m_ProgressPos.y = _Pos;
    clamp(m_ProgressPos.y, m_MinPos, m_MaxPos);
    if (!m_animated)
        m_ProgressPos.x = m_ProgressPos.y;
    UpdateProgressBar();
}

float _sign(const float& v) { return (v > 0.0f) ? +1.0f : -1.0f; }
void CUIProgressBar::Update()
{
    inherited::Update();
    if (!fsimilar(m_ProgressPos.x, m_ProgressPos.y))
    {
        if (fsimilar(m_MaxPos, m_MinPos))
            m_MaxPos += EPS; // hack ^(
        float _diff = m_ProgressPos.y - m_ProgressPos.x;

        float _length = (m_MaxPos - m_MinPos);
        float _val = _length * (1.0f - m_inertion) * Device.fTimeDelta;

        _val = _min(_abs(_val), _abs(_diff));
        _val *= _sign(_diff);
        m_ProgressPos.x += _val;
        UpdateProgressBar();
    }
}

void CUIProgressBar::Draw()
{
    Frect rect;
    GetAbsoluteRect(rect);

    if (m_bBackgroundPresent)
    {
        UI()->PushScissor(rect);
        m_UIBackgroundItem.Draw();
        UI()->PopScissor();
    }

    if (m_op22_cap[0] && m_orient_mode == om_horz) // OP22: заливка из трёх частей
    {
        if (m_CurrentLength > 0.01f)
        {
            const Fvector3& g = m_op22_cap_g; // поле x, поле y, край
            const float fw = _max(m_CurrentLength, 2.f * g.z), x = rect.left, y = rect.top, h = GetHeight();
            m_op22_cap[0]->SetWndRect(Frect().set(x - g.x, y - g.y, x + g.z, y + h + g.y));
            m_op22_cap[1]->SetWndRect(Frect().set(x + g.z, y - g.y, x + fw - g.z, y + h + g.y));
            m_op22_cap[2]->SetWndRect(Frect().set(x + fw - g.z, y - g.y, x + fw + g.x, y + h + g.y));
            for (auto c : m_op22_cap)
                c->Draw();
        }
        m_last_render_frame = Device.dwFrame;
        return;
    }

    Frect progress_rect;

    switch (m_orient_mode)
    {
    case om_horz: progress_rect.set(0, 0, m_CurrentLength, GetHeight()); break;
    case om_vert: progress_rect.set(0, GetHeight() - m_CurrentLength, GetWidth(), GetHeight()); break;
    case om_back: progress_rect.set(GetWidth() - m_CurrentLength * 1.01f, 0, GetWidth(), GetHeight()); break;
    case om_down: progress_rect.set(0, 0, GetWidth(), m_CurrentLength); break;
    case om_fromcenter: {
        const float center = GetWidth() / 2.f;
        progress_rect.set(center - m_CurrentLength, 0, center + m_CurrentLength, GetHeight());
        break;
    }
    case om_vfromcenter: {
        const float center = GetHeight() / 2.f;
        progress_rect.set(0, center - m_CurrentLength, GetWidth(), center + m_CurrentLength);
        break;
    }
    default: NODEFAULT; break;
    }

    if (m_CurrentLength > 0)
    {
        Fvector2 pos = m_UIProgressItem.GetWndPos();
        progress_rect.add(rect.left + pos.x, rect.top + pos.y);

        UI()->PushScissor(progress_rect);
        m_UIProgressItem.Draw();
        UI()->PopScissor();
    }
    m_last_render_frame = Device.dwFrame;
}
