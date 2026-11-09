#pragma once
#include "UIWindow.h"
#include "UIWndCallback.h"
#include "UIXmlInit.h"

class CUIFrameWindow;
class CUIFrameLineWnd;
class CUIAnimatedStatic;
class CUIMapWnd;
class CUI3tButton;
class CUITabControl;
class CGameTask;
class CUITaskDescrWnd;
class CUIScrollView;
class CUITaskItem;

class CUIEventsWnd : public CUIWindow, public CUIWndCallback
{
    typedef CUIWindow inherited;
    enum ETaskFilters
    {
        eActiveTask = 0,
        eAccomplishedTask,
        eFailedTask,
        eSkipedTask,
        //.						eOwnTask,
        eMaxTask
    };
    enum EEventWndFlags
    {
        flNeedReload = (1 << 0),
        flMapMode = (1 << 1),
    };
    Flags16 m_flags;
    bool m_op22_split{};
    bool m_op22_cards{};
    shared_str m_op22_selected;
    shared_str m_op22_map_level;
    Fvector2 m_op22_map_position{};
    CUI3tButton* m_op22_map_button{};
    void OnTaskMap(CUIWindow*, void*);
    ETaskFilters m_currFilter;
    CUIFrameWindow* m_UILeftFrame;
    CUIWindow* m_UIRightWnd;
    CUIFrameLineWnd* m_UILeftHeader;
    CUIAnimatedStatic* m_UIAnimation;
    CUIMapWnd* m_UIMapWnd;
    CUITaskDescrWnd* m_UITaskInfoWnd;
    CUIScrollView* m_ListWnd;
    CUITabControl* m_TaskFilter;

    bool Filter(CGameTask* t);
    void OnFilterChanged(CUIWindow*, void*);
    void ReloadList(bool bClearOnly);

public:
    bool CardsLayout() const { return m_op22_cards; }
    void SetDescriptionMode(bool bMap);
    bool GetDescriptionMode();
    void ShowDescription(CGameTask* t, int idx);
    bool ItemHasDescription(CUITaskItem*);

public:
    CUIEventsWnd();
    virtual ~CUIEventsWnd();
    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData);
    void Init();
    virtual void Update();
    virtual void Draw();
    virtual void Show(bool status);
    void Reload();
    virtual void Reset();

    CUIXml m_ui_task_item_xml;
};
