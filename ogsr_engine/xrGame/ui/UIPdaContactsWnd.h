
#pragma once

#include "UIWindow.h"

class CUIFrameWindow;
class CUIFrameLineWnd;
class CUIStatic;
class CUIAnimatedStatic;
class CUIScrollView;
class CPda;
class CUICharacterInfo;
class CUI3tButton;

class CUIPdaContactsWnd : public CUIWindow
{
private:
    typedef CUIWindow inherited;
    enum
    {
        flNeedUpdate = (1 << 0),
    };
    Flags8 m_flags;

public:
    CUIPdaContactsWnd();
    virtual ~CUIPdaContactsWnd();

    void Init();

    virtual void Update();
    virtual void Reset();

    virtual void Show(bool status);
    virtual void SendMessage(CUIWindow*, s16, void*) override;

    void AddContact(CPda* pda, u16 owner_id);
    void RemoveContact(CPda* pda);
    void RemoveAll();
    void Reload();

    CUIScrollView* UIListWnd;
    CUIScrollView* UIDetailsWnd;
    CUICharacterInfo* m_op22_character{};
    float m_op22_row_height{85.f};
    CUI3tButton* m_op22_map_button{};
    u16 m_op22_selected{u16(-1)};

protected:
    CUIFrameWindow* UIFrameContacts;
    CUIFrameLineWnd* UIContactsHeader;
    CUIFrameWindow* UIRightFrame;
    CUIFrameLineWnd* UIRightFrameHeader;
    CUIAnimatedStatic* UIAnimation;
};

#include "UIPdaListItem.h"
class CUIPdaContactItem : public CUIPdaListItem, public CUISelectable
{
    CUIPdaContactsWnd* m_cw;

public:
    CUIPdaContactItem(CUIPdaContactsWnd* cw) { m_cw = cw; }
    virtual ~CUIPdaContactItem();
    virtual void SetSelected(bool b);
    virtual bool OnMouseDown(int mouse_btn);
};
