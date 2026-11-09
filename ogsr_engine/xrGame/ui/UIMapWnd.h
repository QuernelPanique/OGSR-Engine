#pragma once

#include "UIWindow.h"
#include "UIWndCallback.h"

class CUICustomMap;
class CUIGlobalMap;
class CUIFrameWindow;
class CUIScrollBar;
class CUIFrameLineWnd;
class CMapActionPlanner;
class CUITabControl;
class CUIStatic;
class CUI3tButton;
class CUILevelMap;
class CUICheckButton;
class CUIButton;
class CUIMapHint;
class CMapLocation;
class CUIPropertiesBox;
class CUIPdaSpot;
class CUILevelMap;

DEFINE_MAP(shared_str, CUICustomMap*, GameMaps, GameMapsPairIt);

class CUIMapWnd : public CUIWindow, public CUIWndCallback
{
    typedef CUIWindow inherited;
    enum EMapToolBtn
    {
        eGlobalMap = 0,
        eZoomIn,
        eZoomOut,
        eActor,
        eMaxBtn
    };

public:
    enum lmFlags
    {
        lmZoomIn = (1 << 3),
        lmZoomOut = (1 << 4),
        lmFirst = (1 << 5),
    };
    Flags32 m_flags;

private:
    float m_currentZoom;
    bool m_op22_overlay{};
    CUIGlobalMap* m_GlobalMap;
    GameMaps m_GameMaps;

    CUIFrameWindow* m_UIMainFrame;
    CUIScrollBar* m_UIMainScrollV;
    CUIScrollBar* m_UIMainScrollH;
    CUIWindow* m_UILevelFrame;
    CMapActionPlanner* m_ActionPlanner;
    CUIFrameLineWnd* UIMainMapHeader;
    CUI3tButton* m_ToolBar[eMaxBtn];
    CUIMapHint* m_hint;
    CUIStatic* m_text_hint;
    CUICheckButton* m_op22_filters[7]{};
    xr_string m_op22_filter_patterns[7];
    CUIButton* m_op22_overview{};
    void op22_OnOverview(CUIWindow*, void*);

    void OnScrollV(CUIWindow*, void*);
    void OnScrollH(CUIWindow*, void*);
    void OnToolGlobalMapClicked(CUIWindow*, void*);
    void OnToolActorClicked(CUIWindow*, void*);
    void OnToolZoomInClicked(CUIWindow*, void*);
    void OnToolZoomOutClicked(CUIWindow*, void*);
    void ValidateToolBar();
    void ResetActionPlanner();

public:
    // qweasdd: from lost alpha
    bool ConvertCursorPosToMap(Fvector*, CUICustomMap*);
    void CreateSpotWindow(Fvector, shared_str);
    CMapLocation* UnderSpot(Fvector RealPosition, CUICustomMap*);
    //-qweasdd
    void ActivatePropertiesBox(CUIWindow* w);

    CUICustomMap* m_tgtMap;
    Fvector2 m_tgtCenter;
    CUIMapWnd();
    virtual ~CUIMapWnd();

    virtual void Init(LPCSTR xml_name, LPCSTR start_from);
    virtual void Show(bool status);
    virtual void Draw();
    virtual void Reset();
    virtual void Update();
    float GetZoom() { return m_currentZoom; }
    bool op22_AllowSpot(const CMapLocation*) const;
    void SetZoom(float value);

    void ShowHint(CUIWindow* parent, LPCSTR text);
    void HideHint(CUIWindow* parent);
    void Hint(const shared_str& text);
    virtual bool OnMouse(float x, float y, EUIMessages mouse_action);
    virtual bool OnKeyboard(int dik, EUIMessages keyboard_action);
    virtual bool OnKeyboardHold(int dik);

    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData = NULL);

    void SetTargetMap(CUICustomMap* m, bool bZoomIn = false);
    void SetTargetMap(CUICustomMap* m, const Fvector2& pos, bool bZoomIn = false);
    void SetTargetMap(const shared_str& name, const Fvector2& pos, bool bZoomIn = false);
    void SetTargetMap(const shared_str& name, bool bZoomIn = false);

    CUIPropertiesBox* m_UIPropertiesBox;
    CUIPdaSpot* m_UserSpotWnd;
    CMapLocation* m_cur_location;
    void ShowSettingsWindow(u16 id, Fvector position, shared_str levelName);

    Frect ActiveMapRect()
    {
        Frect r;
        m_UILevelFrame->GetAbsoluteRect(r);
        return r;
    };
    void AddMapToRender(CUICustomMap*);
    void RemoveMapToRender(CUICustomMap*);
    CUIGlobalMap* GlobalMap() { return m_GlobalMap; };
    const GameMaps& GameMaps() { return m_GameMaps; };
    CUICustomMap* GetMapByIdx(u16 idx);
    u16 GetIdxByName(const shared_str& map_name);
    void UpdateScroll();
    shared_str cName() const { return "ui_map_wnd"; };
};
