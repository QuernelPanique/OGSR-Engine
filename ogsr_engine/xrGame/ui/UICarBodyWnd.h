#pragma once

#include "UIDialogWnd.h"
#include "UIEditBox.h"
#include "../inventory_space.h"

class CUIDragDropListEx;
class CUIItemInfo;
class CUICharacterInfo;
class CUIPropertiesBox;
class CUI3tButton;
class CUIProgressBar; // OP22
class CUICellItem;
class IInventoryBox;
class CInventoryOwner;

class CUICarBodyWnd : public CUIDialogWnd
{
private:
    typedef CUIDialogWnd inherited;
    bool m_b_need_update;

public:
    CUICarBodyWnd();
    virtual ~CUICarBodyWnd();

    virtual void Init();
    virtual bool StopAnyMove() { return true; }

    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData);

    void InitCarBody(CInventoryOwner* pOurInv, CInventoryOwner* pOthersInv);
    void InitCarBody(CInventoryOwner* pOur, IInventoryBox* pInvBox);
    virtual void Draw();
    virtual void Update();

    virtual void Show();
    virtual void Hide();

    void DisableAll();
    void EnableAll();
    virtual bool OnKeyboard(int dik, EUIMessages keyboard_action);
    virtual bool OnMouse(float x, float y, EUIMessages mouse_action);

    void UpdateLists_delayed();

protected:
    CInventoryOwner* m_pOurObject;

    CUIDragDropListEx* m_pUIOurBagList;
    CUIDragDropListEx* m_pUIOthersBagList;

    CUIStatic* m_pUIStaticTop;
    CUIStatic* m_pUIStaticBottom;

    CUIFrameWindow* m_pUIDescWnd;
    CUIStatic* m_pUIStaticDesc;
    CUIItemInfo* m_pUIItemInfo;

    CUIStatic* m_pUIOurBagWnd;
    CUIStatic* m_pUIOthersBagWnd;

    //информация о персонажах
    CUIStatic* m_pUIOurIcon;
    CUIStatic* m_pUIOthersIcon;
    CUICharacterInfo* m_pUICharacterInfoLeft;
    CUICharacterInfo* m_pUICharacterInfoRight;
    CUIPropertiesBox* m_pUIPropertiesBox;
    CUI3tButton* m_pUITakeAll;
    CUI3tButton* m_op22_exit{}; // OP22: окно по макету
    // OP22: заголовок «Обыск»/«Ящик» и строка под ним (имя убитого, название уровня у ящика)
    CUIStatic* m_op22_title{};
    CUIStatic* m_op22_sub{};
    CUIStatic* m_op22_right{}; // подпись правого списка: «В ящике»/«Вещи на теле»
    shared_str m_op22_t_box, m_op22_t_body, m_op22_t_monster, m_op22_r_box, m_op22_r_body;
    void op22_titles(bool box, CInventoryOwner* others);
    CUIStatic* m_op22_weight{};
    CUIProgressBar* m_op22_weight_bar{};

public:
    CUICellItem* m_pCurrentCellItem;

    CInventoryOwner* m_pOthersObject{};
    IInventoryBox* m_pInventoryBox;

protected:
    void UpdateLists();

    void ActivatePropertiesBox();
    void EatItem();

    void SetCurrentItem(CUICellItem* itm);
    CUICellItem* CurrentItem();
    PIItem CurrentIItem();

    // Взять все
    void TakeAll();
    void MoveItem(CUICellItem* itm);
    void MoveItems(CUICellItem* itm);
    void DropItemsfromCell(bool b_all);

    bool OnItemDrop(CUICellItem* itm);
    bool OnItemStartDrag(CUICellItem* itm);
    bool OnItemDbClick(CUICellItem* itm);
    bool OnItemSelected(CUICellItem* itm);
    bool OnItemSelectedOP22(CUICellItem* itm); // OP22: + callback_ui.on_cell_after_select
    bool OnItemRButtonClick(CUICellItem* itm);

    bool TransferItem(PIItem itm, CInventoryOwner* owner_from, CInventoryOwner* owner_to, bool b_check);
    void BindDragDropListEnents(CUIDragDropListEx* lst);

    enum eInventorySndAction
    {
        eInvSndOpen = 0,
        eInvSndClose,
        eInvProperties,
        eInvDropItem,
        eInvDetachAddon,
        eInvItemUse,
        eInvItemMove,
        eInvUnloadWpn,
        eInvSndMax
    };

    ref_sound sounds[eInvSndMax];
    void PlaySnd(eInventorySndAction a);
};