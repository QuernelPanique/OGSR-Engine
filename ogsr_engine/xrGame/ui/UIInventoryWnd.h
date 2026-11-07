#pragma once

class CInventory;

#include "UIDialogWnd.h"
#include "UIStatic.h"

#include "UIProgressBar.h"

#include "UIPropertiesBox.h"
#include "UIOutfitSlot.h"

#include "UIOutfitInfo.h"
#include "UIItemInfo.h"
#include "../inventory_space.h"

class CArtefact;
class CUI3tButton;
class CUIDragDropListEx;
class CUICellItem;

class CUIInventoryWnd : public CUIDialogWnd
{
private:
    typedef CUIDialogWnd inherited;
    bool m_b_need_reinit;
    bool m_b_need_update_stats;

public:
    void op22_PlaySnd(int a); // OP22: PlaySnd ОП для скриптов (callback_ui)
    CUIInventoryWnd();
    virtual ~CUIInventoryWnd();

    virtual void Init();

    void InitInventory();
    void InitInventory_delayed();
    virtual bool StopAnyMove() { return !!Core.Features.test(xrCore::Feature::more_hide_weapon); }

    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData);
    virtual bool OnMouse(float x, float y, EUIMessages mouse_action);
    virtual bool OnKeyboard(int dik, EUIMessages keyboard_action);

    IC CInventory* GetInventory() { return m_pInv; }

    virtual void Update();
    virtual void Draw();

    virtual void Show();
    virtual void Hide();

    void HideSlotsHighlight();
    void ShowSlotsHighlight(PIItem InvItem);

    void AddItemToBag(PIItem pItem);

protected:
    enum eInventorySndAction
    {
        eInvSndOpen = 0,
        eInvSndClose,
        eInvItemToSlot,
        eInvItemToBelt,
        eInvItemToRuck,
        eInvProperties,
        eInvDropItem,
        eInvAttachAddon,
        eInvDetachAddon,
        eInvItemUse,
        eInvUnloadWpn,
        eInvSndMax
    };

    ref_sound sounds[eInvSndMax];
    void PlaySnd(eInventorySndAction a);

    CUIStatic UIBeltSlots;
    CUIStatic UIBack;
    CUIStatic* UIRankFrame;
    CUIStatic* UIRank;

    CUIStatic UIBagWnd;
    CUIStatic UIMoneyWnd;
    CUIStatic UIDescrWnd;
    CUIFrameWindow UIPersonalWnd;

    CUI3tButton* UIExitButton;

    CUIStatic UIStaticBottom;
    CUIStatic UIStaticTime;
    CUIStatic UIStaticTimeString;

    CUIStatic UIStaticPersonal;

    CUIDragDropListEx* m_pUIBagList;
    CUIDragDropListEx* m_pUIBeltList;

    CUIDragDropListEx* m_pUIPistolList;
    CUIDragDropListEx* m_pUIAutomaticList;
    CUIOutfitDragDropList* m_pUIOutfitList;

    CUIDragDropListEx* m_pUIKnifeList;
    CUIDragDropListEx* m_pUIHelmetList;
    CUIDragDropListEx* m_pUIBIODetList;
    CUIDragDropListEx* m_pUINightVisionList;
    CUIDragDropListEx* m_pUIDetectorList;
    CUIDragDropListEx* m_pUITorchList;
    CUIDragDropListEx* m_pUIBinocularList;

    // alpet: для индексированного доступа
    CUIDragDropListEx* m_slots_array[SLOTS_TOTAL];
    // OP22: списки слотов ОП 2.2 (нож 0, бинокль 4, детекторы 8/11, фонарь 9, ПНВ 12, второй ствол 13, биорадар 14) - без ogse_new_slots
    CUIDragDropListEx* m_op22_lists[SLOTS_TOTAL]{};
    // OP22: быстрые слоты ОП 2.2 (dragdrop_quick0..4): показ предмета, назначение - op22_quick.script, предмет остаётся в рюкзаке
    CUIDragDropListEx* m_op22_quick[5]{};
    void op22_quick_fill();
    int op22_quick_index(CUIDragDropListEx* l) const;

public:
    CUIDragDropListEx* op22_bag() { return m_pUIBagList; }   // OP22: подсветка подходящих по типу списка
    CUIDragDropListEx* op22_belt() { return m_pUIBeltList; }
    CUIDragDropListEx* GetSlotList(u8 slot_idx);

protected:

    void ClearAllLists();
    void BindDragDropListEnents(CUIDragDropListEx* lst);

    EListType GetType(CUIDragDropListEx* l);

    bool OnItemDrop(CUICellItem* itm);
    bool OnItemDropOP22(CUICellItem* itm); // OP22: + аддон броском на оружие, callback_ui.on_cell_item_drop
    bool OnItemSelectedOP22(CUICellItem* itm); // OP22: + callback_ui.on_cell_after_select
    bool OnItemStartDrag(CUICellItem* itm);
    bool OnItemDbClick(CUICellItem* itm);
    bool OnItemSelected(CUICellItem* itm);
    bool OnItemRButtonClick(CUICellItem* itm);

    CUIStatic UIProgressBack;
    CUIStatic UIProgressBack_rank;
    CUIProgressBar UIProgressBarHealth;
    CUIProgressBar UIProgressBarPsyHealth;
    CUIProgressBar UIProgressBarRadiation;
    CUIProgressBar UIProgressBarRank;

    CUIPropertiesBox UIPropertiesBox;

    //информация о персонаже
    CUIOutfitInfo UIOutfitInfo;
    // OP22: панель аккумулятора экзоскелета, узлы battery_* в dragdrop_outfit скина
    CUIStatic* m_op22_bat_icon{};
    CUIStatic* m_op22_bat_text{};
    CUIProgressBar* m_op22_bat_bar{};
    // OP22: окно по макету - шкалы сытости и жажды, вес текстом и шкалой, подпись «имя, ранг»
    CUIProgressBar* m_op22_satiety{};
    CUIProgressBar* m_op22_thirst{};
    CUIStatic* m_op22_weight{};
    CUIProgressBar* m_op22_weight_bar{};
    CUIStatic* m_op22_caption{};
    u32 m_op22_bat_next{};
    shared_str m_op22_bat_sect;
    void op22_update_battery();
    CUIItemInfo UIItemInfo;

    CInventory* m_pInv;

public:
    CUICellItem* m_pCurrentCellItem;
    // OP22: описание предмета ОП 2.2 - плавающее окно у курсора через g_fly_show_info_delay мс
    // (inventory_item_fly.xml); окно со своей панелью описания помечено узлом op22_panel_info (скин op22_port)
    CUIItemInfo* m_op22_fly{};
    CUICellItem* m_op22_fly_cell{};
    u32 m_op22_fly_t{};
    bool m_op22_fly_on{};
    CUICellItem* op22_fly_cell_at_cursor();
    void op22_fly_update();

protected:
    bool DropItem(PIItem itm, CUIDragDropListEx* lst);
    bool TryUseItem(PIItem itm);
    //----------------------	-----------------------------------------------
    void SendEvent_Item2Slot(PIItem pItem);
    void SendEvent_Item2Belt(PIItem pItem);
    void SendEvent_Item2Ruck(PIItem pItem);
    void SendEvent_Item_Drop(PIItem pItem);
    void SendEvent_Item_Eat(PIItem pItem);
    void SendEvent_ActivateSlot(PIItem pItem);

    //---------------------------------------------------------------------

    void ProcessPropertiesBoxClicked();
    void ActivatePropertiesBox();

    void DropCurrentItem(bool b_all);
    void EatItem(PIItem itm);

    bool ToSlot(CUICellItem* itm, u8 _slot_id, bool force_place);
    bool ToSlot(CUICellItem* itm, bool force_place);
    bool ToBag(CUICellItem* itm, bool b_use_cursor_pos);
    bool ToBelt(CUICellItem* itm, bool b_use_cursor_pos);

    void AttachAddon(PIItem item_to_upgrade);
    void DetachAddon(const char* addon_name);

    void SetCurrentItem(CUICellItem* itm);
    CUICellItem* CurrentItem();

    TIItemContainer ruck_list;
    u32 m_iCurrentActiveSlot;

private:
    bool dont_update_belt_flag{};

public:
    PIItem CurrentIItem();
    void UpdateWeight();
    void UpdateOutfit();
};
