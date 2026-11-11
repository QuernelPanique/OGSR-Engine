#pragma once

#include "UIStatic.h"
#include "UI3tButton.h"
#include "UIFrameLineWnd.h"

#include "../InfoPortion.h"

#include "UICharacterInfo.h"
#include "UIItemInfo.h"

#include "UIWndCallback.h"

class CUIScrollView;
class CUIXml;

class CUITalkDialogWnd : public CUIWindow, public CUIWndCallback
{
private:
    typedef CUIWindow inherited;
    CUIXml* m_uiXml;

public:
    CUITalkDialogWnd();
    virtual ~CUITalkDialogWnd();

    virtual void Init(float x, float y, float width, float height);

    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData = NULL);

    virtual void Show();
    virtual void Hide();

    u32 GetHeaderColor() { return m_iNameTextColor; }
    CGameFont* GetHeaderFont() { return m_pNameTextFont; }
    u32 GetOurReplicsColor() { return m_uOurReplicsColor; }

    shared_str m_ClickedQuestionID;

    //список вопросов, которые мы можем задавать персонажу

    //элементы интерфейса диалога
    CUIFrameLineWnd UIDialogFrame;
    CUIFrameLineWnd UIOurPhrasesFrame;

    CUIStatic UIStaticTop;
    CUIStatic UIStaticBottom;

    CUI3tButton UIToTradeButton;
    CUI3tButton* m_op22_exit{}; // OP22: «Закончить разговор» (узел op22_exit_btn), закрывает как F
    void op22_OnExitClicked(CUIWindow* w, void*);

    //информация о персонажах
    CUIStatic UIOurIcon;
    CUIStatic UIOthersIcon;
    CUICharacterInfo UICharacterInfoLeft;
    CUICharacterInfo UICharacterInfoRight;

    void AddQuestion(LPCSTR str, LPCSTR value, int);
    void AddAnswer(LPCSTR SpeakerName, LPCSTR str, bool bActor);
    void AddIconedAnswer(LPCSTR text, LPCSTR texture_name, Frect texture_rect, LPCSTR templ_name);
    void ClearAll();
    void ClearQuestions();

    void SetOsoznanieMode(bool b);

private:
    enum eSndAction
    {
        eTalkSndOpen = 0,
        eTalkSndClose,
        eTalkSndSay,
        eTalkSndMax
    };

    ref_sound sounds[eTalkSndMax];
    void PlaySnd(eSndAction a);

    CUIScrollView* UIQuestionsList;
    CUIScrollView* UIAnswersList;

    // Шрифт и цвет текста с именем персонажа
    CGameFont* m_pNameTextFont;
    u32 m_iNameTextColor;
    // Цвет тeкста и шрифт наших реплик
    u32 m_uOurReplicsColor;

    void OnTradeClicked(CUIWindow* w, void*);
    void OnQuestionClicked(CUIWindow* w, void*);
};

class CUIQuestionItem : public CUIWindow, public CUIWndCallback
{
    typedef CUIWindow inherited;
    float m_min_height;

public:
    CUIStatic* m_num_text{};
    u32 m_op22_number_normal{}, m_op22_number_hover{};
    CUI3tButton* m_text;
    shared_str m_s_value;
    CUIQuestionItem(CUIXml* xml_doc, LPCSTR path);
    void Init(LPCSTR val, LPCSTR text);
    virtual void Update() override;

    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData = NULL);
    void OnTextClicked(CUIWindow* w, void*);
};

class CUIAnswerItem : public CUIWindow
{
    typedef CUIWindow inherited;

    float m_min_height;
    float m_bottom_footer;
    CUIStatic* m_text;
    CUIStatic* m_name;

public:
    CUIAnswerItem(CUIXml* xml_doc, LPCSTR path);
    void Init(LPCSTR text, LPCSTR name);
    // OP22: GAMMA 2.0 - последняя реплика выделена (op22_hi_name / op22_hi_text = "r,g,b" у шаблона), прежние - цветами разметки
    bool m_op22_hl{};
    u32 m_op22_dim_name{}, m_op22_dim_text{}, m_op22_hi_name{}, m_op22_hi_text{};
    void op22_highlight(bool on);
};

class CUIAnswerItemIconed : public CUIAnswerItem
{
    typedef CUIAnswerItem inherited;
    CUIStatic* m_icon;

public:
    CUIAnswerItemIconed(CUIXml* xml_doc, LPCSTR path);
    void Init(LPCSTR text, LPCSTR texture_name, Frect texture_rect);
};
