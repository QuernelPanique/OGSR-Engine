#pragma once

#include "uiframewindow.h"
#include "uilistbox.h"

#include "../script_export_space.h"

class CUIPropertiesBox : public CUIFrameWindow
{
private:
    typedef CUIFrameWindow inherited;

public:
    CUIPropertiesBox();
    virtual ~CUIPropertiesBox();

    virtual void Init(float x, float y, float width, float height);

    virtual void SendMessage(CUIWindow* pWnd, s16 msg, void* pData);
    virtual bool OnMouse(float x, float y, EUIMessages mouse_action);
    virtual bool OnKeyboard(int dik, EUIMessages keyboard_action);

    bool AddItem(const char* str, void* pData = NULL, u32 tag_value = 0);
    bool AddItem_script(const char* str) { return AddItem(str); };
    u32 GetItemsCount() { return m_UIListWnd.GetSize(); };
    bool op22_has_tag(u32 tag) { return m_UIListWnd.GetItemByTAG(tag) != nullptr; } // OP22: RemoveItemByTAG ОП без вылета на отсутствующем
    // OP22: последний добавленный пункт - на позицию pos (с 1, как в ОП)
    void op22_MoveLastTo(int pos)
    {
        auto& l = m_UIListWnd.op22_pad()->GetChildWndList();
        if (l.size() < 2 || pos < 1 || pos >= int(l.size()))
            return;
        CUIWindow* w = l.back();
        l.pop_back();
        l.insert(std::next(l.begin(), pos - 1), w); // xr_list
        m_UIListWnd.ForceUpdate();
    }
    void RemoveItemByTAG(u32 tag_value);
    void RemoveAll();

    virtual void Show(const Frect& parent_rect, const Fvector2& point);
    virtual void Hide();

    virtual void Update();
    virtual void Draw();

    CUIListBoxItem* GetClickedItem();

    void AutoUpdateSize();

public:
    // OP22: меню по макету - заголовок над рамкой (название предмета), линия между пунктами, своя подсветка, ширина не меньше
    void op22_SetTitle(LPCSTR t);
protected:
    class CUIStatic* m_op22_title{};
    class CUIStatic* m_op22_line{};
    shared_str m_op22_sel;
    float m_op22_min_w{};
    CUIListBox m_UIListWnd;

    DECLARE_SCRIPT_REGISTER_FUNCTION
};

add_to_type_list(CUIPropertiesBox)
#undef script_type_list
#define script_type_list save_type_list(CUIPropertiesBox)
