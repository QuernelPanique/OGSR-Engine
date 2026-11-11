#include "stdafx.h"
#include <dinput.h>
#include "UITalkDialogWnd.h"
#include "../../xr_3da/XR_IOConsole.h"

// OP22: настройки окна диалога ОП 2.2 (op22_console.cpp): dialog_numbered - номера ответов, g_dialog_wnd_alpha - прозрачность фона
static bool op22_dialog_numbers()
{
    if (Console->GetCommand("dialog_numbered"))
        return Console->GetBool("dialog_numbered");
    return Core.Features.test(xrCore::Feature::show_dialog_numbers);
}
static u32 op22_with_alpha(u32 c)
{
    if (!Console->GetCommand("g_dialog_wnd_alpha"))
        return c;
    int mn = 0, mx = 255;
    const int a = Console->GetInteger("g_dialog_wnd_alpha", mn, mx);
    return color_rgba(color_get_R(c), color_get_G(c), color_get_B(c), u32(clampr(a, 0, 255)));
}

#include "xrUIXmlParser.h"
#include "UIXmlInit.h"
#include "UIScrollView.h"
#include "UI3tButton.h"
#include "../UI.h"
#include "../actor.h"
#include "../HUDManager.h"
#include "../UIGameSP.h"
#include "UIPdaWnd.h"
#include "UIDiaryWnd.h"

#define TALK_XML "talk.xml"
#define TRADE_CHARACTER_XML "trade_character.xml"

//////////////////////////////////////////////////////////////////////////

CUITalkDialogWnd::CUITalkDialogWnd() : m_pNameTextFont(NULL) { m_ClickedQuestionID = ""; }

CUITalkDialogWnd::~CUITalkDialogWnd() { xr_delete(m_uiXml); }

void CUITalkDialogWnd::Init(float x, float y, float width, float height)
{
    m_uiXml = xr_new<CUIXml>();
    bool xml_result = m_uiXml->Init(CONFIG_PATH, UI_PATH, TALK_XML);
    R_ASSERT3(xml_result, "xml file not found", TALK_XML);
    CUIXmlInit ml_init;

    inherited::Init(x, y, width, height);

    AttachChild(&UIStaticTop);
    CUIXmlInit::InitStatic(*m_uiXml, "top_background", 0, &UIStaticTop);
    AttachChild(&UIStaticBottom);
    CUIXmlInit::InitStatic(*m_uiXml, "bottom_background", 0, &UIStaticBottom);

    //иконки с изображение нас и партнера по торговле
    AttachChild(&UIOurIcon);
    CUIXmlInit::InitStatic(*m_uiXml, "left_character_icon", 0, &UIOurIcon);
    AttachChild(&UIOthersIcon);
    CUIXmlInit::InitStatic(*m_uiXml, "right_character_icon", 0, &UIOthersIcon);
    UIOurIcon.AttachChild(&UICharacterInfoLeft);
    // OP22: своя карточка персонажа диалога, если есть (у торговли другой вид)
    string_path op22_tmp;
    LPCSTR op22_char_xml = FS.exist(op22_tmp, "$game_config$", "ui\\talk_character.xml") ? "talk_character.xml" : TRADE_CHARACTER_XML;
    UICharacterInfoLeft.Init(0.0f, 0.0f, UIOurIcon.GetWidth(), UIOurIcon.GetHeight(), op22_char_xml);
    UIOthersIcon.AttachChild(&UICharacterInfoRight);
    UICharacterInfoRight.Init(0.0f, 0.0f, UIOthersIcon.GetWidth(), UIOthersIcon.GetHeight(), op22_char_xml);

    //основной фрейм диалога
    AttachChild(&UIDialogFrame);
    CUIXmlInit::InitFrameLine(*m_uiXml, "frame_line_window", 0, &UIDialogFrame);
    UIDialogFrame.UITitleText.SetElipsis(CUIStatic::eepEnd, 10);
    // Фрейм с нащими фразами
    AttachChild(&UIOurPhrasesFrame);
    CUIXmlInit::InitFrameLine(*m_uiXml, "frame_line_window", 1, &UIOurPhrasesFrame);
    UIOurPhrasesFrame.UITitleText.SetElipsis(CUIStatic::eepEnd, 10);

    //Ответы
    UIAnswersList = xr_new<CUIScrollView>();
    UIAnswersList->SetAutoDelete(true);
    UIDialogFrame.AttachChild(UIAnswersList);
    CUIXmlInit::InitScrollView(*m_uiXml, "answers_list", 0, UIAnswersList);
    UIAnswersList->SetWindowName("---UIAnswersList");

    //Вопросы
    UIQuestionsList = xr_new<CUIScrollView>();
    UIQuestionsList->SetAutoDelete(true);
    UIOurPhrasesFrame.AttachChild(UIQuestionsList);
    CUIXmlInit::InitScrollView(*m_uiXml, "questions_list", 0, UIQuestionsList);
    UIQuestionsList->SetWindowName("---UIQuestionsList");

    //кнопка перехода в режим торговли
    AttachChild(&UIToTradeButton);
    CUIXmlInit::Init3tButton(*m_uiXml, "button", 0, &UIToTradeButton);
    UIToTradeButton.SetWindowName("trade_btn");
    if (m_uiXml->NavigateToNode("op22_exit_btn", 0)) // OP22
    {
        m_op22_exit = xr_new<CUI3tButton>();
        m_op22_exit->SetAutoDelete(true);
        AttachChild(m_op22_exit);
        CUIXmlInit::Init3tButton(*m_uiXml, "op22_exit_btn", 0, m_op22_exit);
        m_op22_exit->SetWindowName("op22_exit_btn");
        AddCallback("op22_exit_btn", BUTTON_CLICKED, fastdelegate::MakeDelegate(this, &CUITalkDialogWnd::op22_OnExitClicked));
    }

    //Элементы автоматического добавления
    CUIXmlInit::InitAutoStatic(*m_uiXml, "auto_static", this);

    // шрифт для индикации имени персонажа в окне разговора
    CUIXmlInit::InitFont(*m_uiXml, "font", 0, m_iNameTextColor, m_pNameTextFont);

    CGameFont* pFont = NULL;
    CUIXmlInit::InitFont(*m_uiXml, "font", 1, m_uOurReplicsColor, pFont);

    SetWindowName("----CUITalkDialogWnd");

    Register(&UIToTradeButton);
    AddCallback("question_item", LIST_ITEM_CLICKED, fastdelegate::MakeDelegate(this, &CUITalkDialogWnd::OnQuestionClicked));
    AddCallback("trade_btn", BUTTON_CLICKED, fastdelegate::MakeDelegate(this, &CUITalkDialogWnd::OnTradeClicked));

    // Load sounds
    if (m_uiXml->NavigateToNode("action_sounds", 0))
    {
        XML_NODE* stored_root = m_uiXml->GetLocalRoot();
        m_uiXml->SetLocalRoot(m_uiXml->NavigateToNode("action_sounds", 0));

        create_ui_snd(sounds[eTalkSndOpen], m_uiXml->Read("snd_open", 0, nullptr));
        create_ui_snd(sounds[eTalkSndClose], m_uiXml->Read("snd_close", 0, nullptr));
        create_ui_snd(sounds[eTalkSndSay], m_uiXml->Read("snd_say", 0, nullptr));

        m_uiXml->SetLocalRoot(stored_root);
    }
}

#include "UIInventoryUtilities.h"

void CUITalkDialogWnd::Show()
{
    InventoryUtilities::SendInfoToActor("ui_talk");
    inherited::Show(true);
    inherited::Enable(true);

    // OP22: прозрачность фона окна диалога (g_dialog_wnd_alpha)
    UIStaticTop.SetTextureColor(op22_with_alpha(UIStaticTop.GetTextureColor()));
    UIStaticBottom.SetTextureColor(op22_with_alpha(UIStaticBottom.GetTextureColor()));
    UIDialogFrame.SetColor(op22_with_alpha(color_rgba(255, 255, 255, 255)));
    UIOurPhrasesFrame.SetColor(op22_with_alpha(color_rgba(255, 255, 255, 255)));

    ResetAll();
    PlaySnd(eTalkSndOpen);
}

void CUITalkDialogWnd::Hide()
{
    InventoryUtilities::SendInfoToActor("ui_talk_hide");
    inherited::Show(false);
    inherited::Enable(false);
    PlaySnd(eTalkSndClose);
}

void CUITalkDialogWnd::OnQuestionClicked(CUIWindow* w, void*)
{
    PlaySnd(eTalkSndSay);
    m_ClickedQuestionID = ((CUIQuestionItem*)w)->m_s_value;
    GetMessageTarget()->SendMessage(this, TALK_DIALOG_QUESTION_CLICKED);
}

void CUITalkDialogWnd::OnTradeClicked(CUIWindow* w, void*) { GetTop()->SendMessage(this, TALK_DIALOG_TRADE_BUTTON_CLICKED); }
void CUITalkDialogWnd::op22_OnExitClicked(CUIWindow* w, void*) { GetTop()->SendMessage(this, TALK_DIALOG_OP22_EXIT); } // OP22

//пересылаем сообщение родительскому окну для обработки
//и фильтруем если оно пришло от нашего дочернего окна
void CUITalkDialogWnd::SendMessage(CUIWindow* pWnd, s16 msg, void* pData) { CUIWndCallback::OnEvent(pWnd, msg, pData); }

void CUITalkDialogWnd::ClearAll()
{
    UIAnswersList->Clear();
    ClearQuestions();
}

void CUITalkDialogWnd::ClearQuestions() { UIQuestionsList->Clear(); }

void CUITalkDialogWnd::AddQuestion(LPCSTR str, LPCSTR value, int number)
{
    CUIQuestionItem* itm = xr_new<CUIQuestionItem>(m_uiXml, "question_item");
    std::string question_text{str};
    ++number; // zero-based index

    if (number <= 10)
    {
        if (op22_dialog_numbers())
        {
            if (itm->m_num_text)
            {
                string16 buff;
                sprintf_s(buff, "%d.", number == 10 ? 0 : number);
                itm->m_num_text->SetText(buff);
            }
            else
            {
                question_text = std::to_string(number == 10 ? 0 : number) + ". " + question_text;                
            }
        }

        itm->m_text->SetAccelerator(DIK_ESCAPE + number, 0);
    }

    itm->Init(value, question_text.c_str());
    itm->SetWindowName("question_item");
    UIQuestionsList->AddWindow(itm, true);
    Register(itm);
}
#include "../game_news.h"
#include "../level.h"
#include "../actor.h"
#include "../alife_registry_wrappers.h"
void CUITalkDialogWnd::AddAnswer(LPCSTR SpeakerName, LPCSTR str, bool bActor)
{
    CUIAnswerItem* itm = xr_new<CUIAnswerItem>(m_uiXml, bActor ? "actor_answer_item" : "other_answer_item");
    itm->Init(str, SpeakerName);
    for (u32 i = 0; i < UIAnswersList->GetSize(); ++i) // OP22: GAMMA 2.0 - прежние реплики гаснут, выделена новая
        if (auto* a = dynamic_cast<CUIAnswerItem*>(UIAnswersList->GetItem(i)))
            a->op22_highlight(false);
    itm->op22_highlight(true);
    UIAnswersList->AddWindow(itm, true);
    UIAnswersList->ScrollToEnd();

    GAME_NEWS_DATA news_data;
    xr_string str_;
    str_ = SpeakerName;
    str_ += " >> ";
    str_ += str;

    news_data.m_type = GAME_NEWS_DATA::eTalk;
    news_data.news_text = str_.c_str();
    //.	news_data.texture_name			= "ui\\ui_icons_npc";
    CUICharacterInfo& ci = bActor ? UICharacterInfoLeft : UICharacterInfoRight;

    news_data.texture_name = ci.IconName();
    news_data.tex_rect = ci.UIIcon().GetUIStaticItem().GetOriginalRect();
    news_data.tex_rect.x2 = news_data.tex_rect.width();
    news_data.tex_rect.y2 = news_data.tex_rect.height();
    // news_data.receive_time			= Level().GetGameTime();

    Actor()->PushNewsData(news_data);
    CUIGameSP* pGameSP = smart_cast<CUIGameSP*>(HUD().GetUI()->UIGame());
    if (pGameSP)
        pGameSP->PdaMenu->UIDiaryWnd->AddNews();
}

void CUITalkDialogWnd::AddIconedAnswer(LPCSTR text, LPCSTR texture_name, Frect texture_rect, LPCSTR templ_name)
{
    CUIAnswerItemIconed* itm = xr_new<CUIAnswerItemIconed>(m_uiXml, templ_name);
    itm->Init(text, texture_name, texture_rect);
    UIAnswersList->AddWindow(itm, true);
    UIAnswersList->ScrollToEnd();
}
void CUITalkDialogWnd::SetOsoznanieMode(bool b)
{
    UIOurIcon.Show(!b);
    UIOthersIcon.Show(!b);

    UIAnswersList->Show(!b);
    UIDialogFrame.Show(!b);

    UIToTradeButton.Show(!b);
}

void CUITalkDialogWnd::PlaySnd(eSndAction a)
{
    if (sounds[a]._handle() && (!sounds[a]._feedback() /* || sounds[a].play_progress() > 0.3f*/))
        sounds[a].play_no_feedback(nullptr, sm_2D);
}

void CUIQuestionItem::SendMessage(CUIWindow* pWnd, s16 msg, void* pData) { CUIWndCallback::OnEvent(pWnd, msg, pData); }

CUIQuestionItem::CUIQuestionItem(CUIXml* xml_doc, LPCSTR path)
{
    m_text = xr_new<CUI3tButton>();
    m_text->SetAutoDelete(true);
    AttachChild(m_text);

    string512 str;
    CUIXmlInit xml_init;

    strcpy_s(str, path);
    xml_init.InitWindow(*xml_doc, str, 0, this);

    m_min_height = xml_doc->ReadAttribFlt(path, 0, "min_height", 15.0f);

    strconcat(sizeof(str), str, path, ":content_text");
    xml_init.Init3tButton(*xml_doc, str, 0, m_text);

    Register(m_text);
    m_text->SetWindowName("text_button");
    AddCallback("text_button", BUTTON_CLICKED, fastdelegate::MakeDelegate(this, &CUIQuestionItem::OnTextClicked));

    strconcat(sizeof(str), str, path, ":num_text");

    if (op22_dialog_numbers() && xml_doc->NavigateToNode(str, 0))
    {
        m_num_text = xr_new<CUIStatic>();
        m_num_text->SetAutoDelete(true);
        AttachChild(m_num_text);
        xml_init.InitStatic(*xml_doc, str, 0, m_num_text);
        m_op22_number_normal = m_num_text->GetTextColor();
        m_op22_number_hover = color_argb(color_get_A(m_op22_number_normal),
            xml_doc->ReadAttribInt(str, 0, "hover_r", color_get_R(m_op22_number_normal)),
            xml_doc->ReadAttribInt(str, 0, "hover_g", color_get_G(m_op22_number_normal)),
            xml_doc->ReadAttribInt(str, 0, "hover_b", color_get_B(m_op22_number_normal)));
    }
}

void CUIQuestionItem::Init(LPCSTR val, LPCSTR text)
{
    m_s_value = val;
    m_text->SetText(text);
    m_text->AdjustHeightToText();
    // OP22: кнопка ответа (фон под курсором и область нажатия) - на всю строку: высота текста и его отступ сверху и
    // снизу. Было: кнопка высотой с текст у верхнего края, а текст ниже на свой отступ - заливка полосой над текстом (08.11)
    const float pad = _max(0.f, m_text->GetTextY());
    float new_h = _max(m_min_height, m_text->GetWndPos().y + m_text->GetHeight() + 2.f * pad);
    m_text->SetHeight(new_h - m_text->GetWndPos().y);
    SetHeight(new_h);
}

void CUIQuestionItem::Update()
{
    inherited::Update();
    if (m_num_text)
        m_num_text->SetTextColor(m_text->CursorOverWindow() || m_text->GetButtonsState() == CUIButton::BUTTON_PUSHED ?
            m_op22_number_hover : m_op22_number_normal);
}

void CUIQuestionItem::OnTextClicked(CUIWindow* w, void*) { GetMessageTarget()->SendMessage(this, LIST_ITEM_CLICKED, (void*)this); }

CUIAnswerItem::CUIAnswerItem(CUIXml* xml_doc, LPCSTR path)
{
    m_text = xr_new<CUIStatic>();
    m_text->SetAutoDelete(true);
    m_name = xr_new<CUIStatic>();
    m_name->SetAutoDelete(true);
    AttachChild(m_text);
    AttachChild(m_name);

    string512 str;
    CUIXmlInit xml_init;

    strcpy_s(str, path);
    xml_init.InitWindow(*xml_doc, str, 0, this);

    m_min_height = xml_doc->ReadAttribFlt(path, 0, "min_height", 15.0f);
    m_bottom_footer = xml_doc->ReadAttribFlt(path, 0, "bottom_footer", 0.0f);
    strconcat(sizeof(str), str, path, ":content_text");
    xml_init.InitStatic(*xml_doc, str, 0, m_text);

    strconcat(sizeof(str), str, path, ":name_caption");
    xml_init.InitStatic(*xml_doc, str, 0, m_name);
    SetAutoDelete(true);
    // OP22: GAMMA 2.0 - цвета выделенной (последней) реплики
    auto rgb = [](LPCSTR s, u32& out) {
        int r, g, b;
        if (s && sscanf(s, "%d,%d,%d", &r, &g, &b) == 3)
        {
            out = color_argb(255, r, g, b);
            return true;
        }
        return false;
    };
    const bool hn = rgb(xml_doc->ReadAttrib(path, 0, "op22_hi_name", nullptr), m_op22_hi_name);
    const bool ht = rgb(xml_doc->ReadAttrib(path, 0, "op22_hi_text", nullptr), m_op22_hi_text);
    if (hn || ht)
    {
        m_op22_hl = true;
        m_op22_dim_name = m_name->GetTextColor();
        m_op22_dim_text = m_text->GetTextColor();
        if (!hn)
            m_op22_hi_name = m_op22_dim_name;
        if (!ht)
            m_op22_hi_text = m_op22_dim_text;
    }
}

void CUIAnswerItem::op22_highlight(bool on)
{
    if (!m_op22_hl)
        return;
    m_name->SetTextColor(on ? m_op22_hi_name : m_op22_dim_name);
    m_text->SetTextColor(on ? m_op22_hi_text : m_op22_dim_text);
}

void CUIAnswerItem::Init(LPCSTR text, LPCSTR name)
{
    m_name->SetText(name);
    m_text->SetText(text);
    m_text->AdjustHeightToText();
    float new_h = _max(m_min_height, m_text->GetWndPos().y + m_text->GetHeight());
    new_h += m_bottom_footer;
    SetHeight(new_h);
}

CUIAnswerItemIconed::CUIAnswerItemIconed(CUIXml* xml_doc, LPCSTR path) : CUIAnswerItem(xml_doc, path)
{
    m_icon = xr_new<CUIStatic>();
    m_icon->SetAutoDelete(true);
    AttachChild(m_icon);

    string512 str;
    CUIXmlInit xml_init;

    strconcat(sizeof(str), str, path, ":msg_icon");
    xml_init.InitStatic(*xml_doc, str, 0, m_icon);
}

void CUIAnswerItemIconed::Init(LPCSTR text, LPCSTR texture_name, Frect texture_rect)
{
    inherited::Init(text, "");
    m_icon->CreateShader(texture_name, "hud\\default");
    m_icon->GetUIStaticItem().SetOriginalRect(texture_rect.x1, texture_rect.y1, texture_rect.x2, texture_rect.y2);
    m_icon->TextureAvailable(true);
    m_icon->TextureOn();
    m_icon->SetStretchTexture(true);
}
