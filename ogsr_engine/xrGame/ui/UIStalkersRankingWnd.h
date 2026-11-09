#pragma once
#include "UIWindow.h"

class CUIFrameWindow;
class CUIFrameLineWnd;
class CUIAnimatedStatic;
class CUIStatic;
class CUICharacterInfo;
class CUIScrollView;
class CUIXml;
class CSE_ALifeTraderAbstract;

class CUIStalkersRankingWnd : public CUIWindow
{
    typedef CUIWindow inherited;

public:
    void Init();
    virtual void Show(bool status);
    void ShowHumanDetails();

protected:
    CUIFrameWindow* UIInfoFrame{};
    CUIFrameWindow* UICharIconFrame{};
    CUIFrameLineWnd* UIInfoHeader{};
    CUIFrameLineWnd* UICharIconHeader{};
    CUIAnimatedStatic* UIAnimatedIcon{};
    // информация о персонаже
    CUIWindow* UICharacterWindow{};
    CUICharacterInfo* UICharacterInfo{};
    bool m_op22_actor_card{};
    bool m_op22_reference{};
    CUIStatic* m_op22_actor_place{};
    CUIStatic* m_op22_actor_rating{};
    CUIStatic* m_op22_actor_next{};
    void FillList();
    CUIScrollView* UIList{};
    void AddStalkerItem(CUIXml* xml, int num, CSE_ALifeTraderAbstract* t);
    void AddActorItem(CUIXml* xml, int num, CSE_ALifeTraderAbstract* t);

public:
    CUIScrollView& GetTopList() { return *UIList; }
    void ShowHumanInfo(u16 id);
    virtual void Reset();
};

class CUIStalkerRankingInfoItem : public CUIWindow, public CUISelectable
{
    CUIStalkersRankingWnd* m_StalkersRankingWnd;
    u32 m_stored_alpha;

public:
    u16 m_humanID;
    CUIStatic* m_text1;
    CUIStatic* m_text2;
    CUIStatic* m_text3;
    CUIStatic* m_op22_community{};
    CUIStatic* m_op22_rank{};
    CUIStatic* m_op22_selection{};
    u32 m_op22_normal[5]{};
    void op22_SetCharacter(CSE_ALifeTraderAbstract* trader);

public:
    CUIStalkerRankingInfoItem(CUIStalkersRankingWnd*);

    void Init(CUIXml* xml, LPCSTR path, int idx);
    virtual void SetSelected(bool b);
    virtual bool OnMouseDown(int mouse_btn);
};

class CUIStalkerRankingElipsisItem : public CUIStalkerRankingInfoItem
{
    typedef CUIStalkerRankingInfoItem inherited;

public:
    CUIStalkerRankingElipsisItem(CUIStalkersRankingWnd*);
    virtual void SetSelected(bool b);
    virtual bool OnMouseDown(int mouse_btn);
};
