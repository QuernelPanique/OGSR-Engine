#pragma once
#include "UIWindow.h"

class CUIScrollView;
class CUIFrameWindow;
class CUIFrameLineWnd;
class CUIXml;
class CEncyclopediaArticle;
class CGameTask;
class CGameFont;

class CUITaskDescrWnd : public CUIWindow
{
    typedef CUIWindow inherited;

protected:
    CUIScrollView* m_UITaskInfoWnd;
    CUIFrameWindow* m_UIMainFrame;
    CUIFrameLineWnd* m_UIMainHeader;
    CGameFont* m_op22_title_font{};
    CGameFont* m_op22_text_font{};
    u32 m_op22_title_color{0xffffffff}, m_op22_text_color{0xffffffff};

public:
    CUITaskDescrWnd();
    virtual ~CUITaskDescrWnd();
    virtual void Draw();
    void Init(CUIXml* doc, LPCSTR start_from);
    void ClearAll();
    void AddArticle(LPCSTR article);
    void AddArticle(CEncyclopediaArticle* article);
    void AddTaskSummary(CGameTask* task, int objective);
};
