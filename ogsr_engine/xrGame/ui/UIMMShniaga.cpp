#include "StdAfx.h"

#include "UIMMShniaga.h"
#include "UIStatic.h"
#include "UIScrollView.h"
#include "UIXmlInit.h"
#include "MMsound.h"
#include "../hudmanager.h"
#include "../game_base_space.h"
#include "../level.h"
#include "../object_broker.h"
#include "../Actor.h"
#include "../saved_game_wrapper.h"
#include "../UICursor.h" // OP22
#include "../UIFrameRect.h" // OP22

extern string_path g_last_saved_game;

CUIMMShniaga::CUIMMShniaga()
{
    m_sound = xr_new<CMMSound>();

    m_view = xr_new<CUIScrollView>();
    AttachChild(m_view);
    m_shniaga = xr_new<CUIStatic>();
    AttachChild(m_shniaga);
    m_magnifier = xr_new<CUIStatic>();
    m_shniaga->AttachChild(m_magnifier);
    m_magnifier->SetPPMode();
    m_gratings[0] = xr_new<CUIStatic>();
    m_shniaga->AttachChild(m_gratings[0]);
    m_gratings[1] = xr_new<CUIStatic>();
    m_shniaga->AttachChild(m_gratings[1]);
    m_anims[0] = xr_new<CUIStatic>();
    m_shniaga->AttachChild(m_anims[0]);
    m_anims[1] = xr_new<CUIStatic>();
    m_shniaga->AttachChild(m_anims[1]);

    m_mag_pos = 0;

    m_selected = NULL;

    m_start_time = 0;
    m_origin = 0;
    m_destination = 0;
    m_run_time = 0;

    m_flags.zero();

    m_selected_btn = -1;
    m_page = -1;
}

CUIMMShniaga::~CUIMMShniaga()
{
    xr_delete(m_magnifier);
    xr_delete(m_op22_box); // OP22
    xr_delete(m_shniaga);
    xr_delete(m_anims[0]);
    xr_delete(m_anims[1]);
    xr_delete(m_gratings[0]);
    xr_delete(m_gratings[1]);
    xr_delete(m_view);
    xr_delete(m_sound);

    delete_data(m_buttons);
    delete_data(m_buttons_new);
}

void CUIMMShniaga::Init(CUIXml& xml_doc, LPCSTR path)
{
    string256 _path;

    CUIXmlInit::InitWindow(xml_doc, path, 0, this);
    strconcat(sizeof(_path), _path, path, ":shniaga:magnifire");
    CUIXmlInit::InitStatic(xml_doc, _path, 0, m_magnifier);
    m_mag_pos = m_magnifier->GetWndPos().x;
    strconcat(sizeof(_path), _path, path, ":shniaga");
    CUIXmlInit::InitStatic(xml_doc, _path, 0, m_shniaga);
    strconcat(sizeof(_path), _path, path, ":shniaga:left_anim");
    CUIXmlInit::InitStatic(xml_doc, _path, 0, m_anims[0]);
    strconcat(sizeof(_path), _path, path, ":shniaga:right_anim");
    CUIXmlInit::InitStatic(xml_doc, _path, 0, m_anims[1]);
    strconcat(sizeof(_path), _path, path, ":shniaga:left_grating");
    CUIXmlInit::InitStatic(xml_doc, _path, 0, m_gratings[0]);
    strconcat(sizeof(_path), _path, path, ":shniaga:right_grating");
    CUIXmlInit::InitStatic(xml_doc, _path, 0, m_gratings[1]);
    strconcat(sizeof(_path), _path, path, ":buttons_region");
    CUIXmlInit::InitScrollView(xml_doc, _path, 0, m_view);
    // OP22: вторая колонка для выбора сложности и её подложка
    strconcat(sizeof(_path), _path, path, ":new_game_panel");
    if (xml_doc.NavigateToNode(_path, 0))
    {
        m_op22_panel = xr_new<CUIStatic>();
        m_op22_panel->SetAutoDelete(true);
        AttachChild(m_op22_panel);
        CUIXmlInit::InitStatic(xml_doc, _path, 0, m_op22_panel);
        m_op22_panel->Show(false);
    }
    strconcat(sizeof(_path), _path, path, ":op22_box");
    if (xml_doc.NavigateToNode(_path, 0)) // OP22: подложка списка по числу кнопок
    {
        m_op22_box = xr_new<CUIFrameRect>();
        m_op22_box_r.set(xml_doc.ReadAttribFlt(_path, 0, "x"), xml_doc.ReadAttribFlt(_path, 0, "y"), xml_doc.ReadAttribFlt(_path, 0, "width"),
                         xml_doc.ReadAttribFlt(_path, 0, "extra"));
        m_op22_box->Init(xml_doc.Read(_path, 0, ""), 0, 0, m_op22_box_r.z, 100);
    }
    strconcat(sizeof(_path), _path, path, ":buttons_region_new");
    if (xml_doc.NavigateToNode(_path, 0))
    {
        m_view_new = xr_new<CUIScrollView>();
        m_view_new->SetAutoDelete(true);
        AttachChild(m_view_new);
        CUIXmlInit::InitScrollView(xml_doc, _path, 0, m_view_new);
    }
    strconcat(sizeof(_path), _path, path, ":shniaga:magnifire:y_offset");
    m_offset = xml_doc.ReadFlt(_path, 0, 0);

    if (!g_pGameLevel)
    {
        if (!*g_last_saved_game || !CSavedGameWrapper::valid_saved_game(g_last_saved_game))
            CreateList(m_buttons, xml_doc, "menu_main");
        else
            CreateList(m_buttons, xml_doc, "menu_main_last_save");

        CreateList(m_buttons_new, xml_doc, "menu_new_game");
    }
    else
    {
        VERIFY(Actor());
        if (Actor() && !Actor()->g_Alive())
            CreateList(m_buttons, xml_doc, "menu_main_single_dead");
        else
            CreateList(m_buttons, xml_doc, "menu_main_single");
    }

    ShowMain();

    m_sound->Init(xml_doc, "menu_sound");
    m_sound->music_Play();

    m_wheel_size[0] = m_anims[0]->GetWndSize();

    m_wheel_size[1].set(m_wheel_size[0]);
    m_wheel_size[1].x /= 1.33f;
}

void CUIMMShniaga::OnDeviceReset()
{
    if (UI()->is_widescreen())
    {
        m_anims[0]->SetWndSize(m_wheel_size[1]);
        m_anims[1]->SetWndSize(m_wheel_size[1]);
    }
    else
    {
        m_anims[0]->SetWndSize(m_wheel_size[0]);
        m_anims[1]->SetWndSize(m_wheel_size[0]);
    }
}

extern CActor* g_actor;

void CUIMMShniaga::CreateList(xr_vector<CUIStatic*>& lst, CUIXml& xml_doc, LPCSTR path)
{
    CGameFont* pF;
    u32 color;
    float height;

    height = xml_doc.ReadAttribFlt(path, 0, "btn_height");
    m_op22_btn_h = height; // OP22
    R_ASSERT(height);
    // OP22: стиль макета - текст слева с отступом, текстура обычной и выбранной кнопки, цвет текста выбранной
    if (xml_doc.ReadAttribInt(path, 0, "op22", 0) == 1)
    {
        m_op22 = true;
        m_op22_tx = xml_doc.ReadAttribFlt(path, 0, "text_x", 0.f);
        m_op22_tex[0] = xml_doc.ReadAttrib(path, 0, "tex", "");
        m_op22_tex[1] = xml_doc.ReadAttrib(path, 0, "tex_sel", "");
        m_op22_clr[1] = color_argb(255, xml_doc.ReadAttribInt(path, 0, "sel_r", 23), xml_doc.ReadAttribInt(path, 0, "sel_g", 18), xml_doc.ReadAttribInt(path, 0, "sel_b", 4));
    }

    CUIXmlInit::InitFont(xml_doc, path, 0, color, pF);
    R_ASSERT(pF);

    int nodes_num = xml_doc.GetNodesNum(path, 0, "btn");

    XML_NODE* tab_node = xml_doc.NavigateToNode(path, 0);
    xml_doc.SetLocalRoot(tab_node);

    for (int i = 0; i < nodes_num; ++i)
    {
        CUIStatic* st = xr_new<CUIStatic>();
        st->Init(0, 0, m_view->GetDesiredChildWidth(), height);
        st->SetTextComplexMode(false);
        st->SetTextST(xml_doc.ReadAttrib("btn", i, "caption"));
        if (pF)
            st->SetFont(pF);
        st->SetTextColor(color);
        st->SetTextAlignment(m_op22 ? CGameFont::alLeft : CGameFont::alCenter);
        st->SetVTextAlignment(valCenter);
        if (m_op22)
        {
            m_op22_clr[0] = color;
            st->SetTextX(m_op22_tx);
            if (m_op22_tex[0].size())
            {
                st->InitTexture(m_op22_tex[0].c_str());
                st->SetStretchTexture(true);
            }
        }
        st->SetWindowName(xml_doc.ReadAttrib("btn", i, "name"));
        st->SetMessageTarget(this);

        lst.push_back(st);
    }
    xml_doc.SetLocalRoot(xml_doc.GetRoot());
}

// OP22: выбранная кнопка залита (текстура tex_sel), остальные обычные
void CUIMMShniaga::op22_style(xr_vector<CUIStatic*>& lst, CUIWindow* sel)
{
    if (!m_op22)
        return;
    for (CUIStatic* st : lst)
    {
        const bool on = st == sel;
        if (m_op22_tex[on].size())
            st->InitTexture(m_op22_tex[on].c_str());
        st->SetTextColor(m_op22_clr[on]);
    }
}

void CUIMMShniaga::ShowMain()
{
    m_page = 0;
    if (m_view_new)
        m_view_new->Clear();
    if (m_op22_panel)
        m_op22_panel->Show(false);
    m_view->Clear();
    for (u32 i = 0; i < m_buttons.size(); i++)
        m_view->AddWindow(m_buttons[i], false);

    SendMessage(m_buttons[0], STATIC_FOCUS_RECEIVED);
}

void CUIMMShniaga::ShowNewGame()
{
    m_page = 1;
    if (m_view_new) // OP22: главное меню остаётся слева, «Новая игра» выбрана, сложность - во второй колонке
    {
        for (CUIStatic* st : m_buttons)
            if (!xr_strcmp("btn_new_game", st->WindowName()))
                op22_style(m_buttons, st);
        if (m_op22_panel)
            m_op22_panel->Show(true);
        m_view_new->Clear();
        for (u32 i = 0; i < m_buttons_new.size(); i++)
            m_view_new->AddWindow(m_buttons_new[i], false);
        SendMessage(m_buttons_new[0], STATIC_FOCUS_RECEIVED);
        return;
    }
    m_view->Clear();
    for (u32 i = 0; i < m_buttons_new.size(); i++)
        m_view->AddWindow(m_buttons_new[i], false);

    SendMessage(m_buttons_new[0], STATIC_FOCUS_RECEIVED);
}

bool CUIMMShniaga::IsButton(CUIWindow* st)
{
    for (u32 i = 0; i < m_buttons.size(); i++)
        if (m_buttons[i] == st)
            return true;

    for (u32 i = 0; i < m_buttons_new.size(); i++)
        if (m_buttons_new[i] == st)
            return true;

    return false;
}

void CUIMMShniaga::SendMessage(CUIWindow* pWnd, s16 msg, void* pData)
{
    CUIWindow::SendMessage(pWnd, msg, pData);
    if (IsButton(pWnd))
    {
        switch (msg)
        {
        case STATIC_FOCUS_RECEIVED: SelectBtn(pWnd); break;
        }
    }
}

void CUIMMShniaga::SelectBtn(int btn)
{
    R_ASSERT(btn >= 0);
    if (0 == m_page)
        m_selected = m_buttons[btn];
    else
        m_selected = m_buttons_new[btn];
    m_selected_btn = btn;
    op22_style(0 == m_page ? m_buttons : m_buttons_new, m_selected); // OP22
    ProcessEvent(E_Begin);
}

void CUIMMShniaga::SelectBtn(CUIWindow* btn)
{
    R_ASSERT(m_page >= 0);
    for (int i = 0; i < (int)m_buttons.size(); i++)
    {
        if (0 == m_page)
        {
            if (m_buttons[i] == btn)
            {
                SelectBtn(i);
                return;
            }
        }
        else if (1 == m_page)
        {
            if (m_buttons_new[i] == btn)
            {
                SelectBtn(i);
                return;
            }
        }
    }
}

void CUIMMShniaga::Draw()
{
    if (m_op22_box) // OP22: подложка под кнопками текущего меню
    {
        const size_t n = (0 == m_page || m_view_new) ? m_buttons.size() : m_buttons_new.size();
        Fvector2 pos;
        GetAbsolutePos(pos);
        m_op22_box->SetWndSize(Fvector2().set(m_op22_box_r.z, n * m_op22_btn_h + m_op22_box_r.w));
        m_op22_box->Draw(pos.x + m_op22_box_r.x, pos.y + m_op22_box_r.y);
    }
    CUIWindow::Draw();
}

void CUIMMShniaga::Update()
{
    //	static bool playing = false;
    if (m_start_time > Device.dwTimeContinual - m_run_time)
    {
        //		playing = true;
        Fvector2 pos = m_shniaga->GetWndPos();
        float l = 2 * PI * m_anims[0]->GetHeight() / 2;
        int n = iFloor(pos.y / l);
        float a = 2 * PI * (pos.y - l * n) / l;
        m_anims[0]->SetHeading(-a);
        m_anims[1]->SetHeading(a);

        pos.y = this->pos(m_origin, m_destination, Device.dwTimeContinual - m_start_time);
        m_shniaga->SetWndPos(pos);
    }
    else
        ProcessEvent(E_Stop);

    if (m_start_time > Device.dwTimeContinual - m_run_time * 10 / 100)
        ProcessEvent(E_Finilize);

    ProcessEvent(E_Update);

    CUIWindow::Update();
}

bool CUIMMShniaga::OnMouse(float x, float y, EUIMessages mouse_action)
{
    if (WINDOW_LBUTTON_DOWN == mouse_action)
    {
        // OP22: щелчок мимо колонки сложности закрывает её, ничего не запуская; мимо кнопок меню - ничего не делает
        if (1 == m_page && m_view_new)
        {
            Frect r;
            m_view_new->GetAbsoluteRect(r);
            const Fvector2 op22_c = GetUICursor()->GetCursorPosition();
            if (!r.in(op22_c.x, op22_c.y))
            {
                ShowMain();
                return true;
            }
        }
        else if (m_op22)
        {
            Frect r;
            m_view->GetAbsoluteRect(r);
            const Fvector2 op22_c = GetUICursor()->GetCursorPosition();
            if (!r.in(op22_c.x, op22_c.y))
                return true;
        }
        OnBtnClick();
    }

    return CUIWindow::OnMouse(x, y, mouse_action);
}

void CUIMMShniaga::OnBtnClick()
{
    if (0 == xr_strcmp("btn_new_game", m_selected->WindowName()))
        ShowNewGame();
    else if (0 == xr_strcmp("btn_new_back", m_selected->WindowName()))
        ShowMain();
    else
        GetMessageTarget()->SendMessage(m_selected, BUTTON_CLICKED);
}

#include <dinput.h>

bool CUIMMShniaga::OnKeyboard(int dik, EUIMessages keyboard_action)
{
    if (WINDOW_KEY_PRESSED == keyboard_action)
    {
        switch (dik)
        {
        case DIK_UP:
            if (m_selected_btn > 0)
                SelectBtn(m_selected_btn - 1);
            return true;
        case DIK_DOWN:
            if (m_selected_btn < BtnCount() - 1)
                SelectBtn(m_selected_btn + 1);
            return true;
        case DIK_RETURN:
        case DIK_NUMPADENTER: OnBtnClick(); return true;
        case DIK_ESCAPE:
            if (1 == m_page)
                ShowMain();
            return true;
        }
    }

    return CUIWindow::OnKeyboard(dik, keyboard_action);
}

int CUIMMShniaga::BtnCount()
{
    R_ASSERT(-1);
    if (m_page == 0)
        return (int)m_buttons.size();
    else if (m_page == 1)
        return (int)m_buttons_new.size();
    else
        return -1;
}

float CUIMMShniaga::pos(float x1, float x2, u32 t)
{
    float x = 0;

    if (t >= 0 && t <= m_run_time)
        x = log(1 + (t * 10.0f) / m_run_time) / log(11.0f);
    else if (t <= 0)
        x = 0;
    else if (t > m_run_time)
        x = 1;

    x *= abs(x2 - x1);

    if (x2 - x1 < 0)
        return x1 - x;
    else
        return x1 + x;
}

bool b_shniaganeed_pp = true;
void CUIMMShniaga::SetVisibleMagnifier(bool f)
{
    b_shniaganeed_pp = f;
    Fvector2 pos = m_magnifier->GetWndPos();
    if (f)
        pos.x = m_mag_pos;
    else
        pos.x = 1025;
    m_magnifier->SetWndPos(pos);
}

void CUIMMShniaga::ProcessEvent(EVENT ev)
{
    switch (ev)
    {
    case E_Begin: {
        // init whell sound
        m_sound->whell_Play();

        // calculate moving params
        m_start_time = Device.dwTimeContinual;
        m_origin = m_shniaga->GetWndPos().y;
        //				float border = GetHeight() - m_shniaga->GetHeight();
        //				float y = m_selected->GetWndPos().y;
        //				m_destination = (y < border) ? y : border;
        m_destination = m_selected->GetWndPos().y - m_magnifier->GetWndPos().y;
        m_destination += m_offset;
        m_run_time = u32((log(1 + abs(m_origin - m_destination)) / log(GetHeight())) * 1000);
        if (m_run_time < 100)
            m_run_time = 100;

        // reset flags
        m_flags.set(fl_SoundFinalized, FALSE);
        m_flags.set(fl_MovingStoped, FALSE);
    }
    break;
    case E_Finilize:
        if (!m_flags.test(fl_SoundFinalized))
        {
            m_sound->whell_Click();

            m_flags.set(fl_SoundFinalized, TRUE);
        }
        break;
    case E_Stop:
        if (!m_flags.test(fl_MovingStoped))
        {
            m_sound->whell_Stop();

            Fvector2 pos = m_shniaga->GetWndPos();
            pos.y = m_destination;
            m_shniaga->SetWndPos(pos);

            m_flags.set(fl_MovingStoped, TRUE);
        }
        break;
    case E_Update: m_sound->music_Update(); break;
    }
}
