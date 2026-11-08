#pragma once

#include "UILabel.h"
#include "UIOptionsItem.h"

struct _action;
struct _keyboard;
class CUIColorAnimatorWrapper;

class CUIEditKeyBind : public CUILabel, public CUIOptionsItem
{
    bool m_bPrimary;
    _action* m_action;
    _keyboard* m_keyboard;

public:
    CUIEditKeyBind(bool bPrim);
    virtual ~CUIEditKeyBind();
    // options item
    virtual void Register(const char* entry, const char* group);
    virtual void SetCurrentValue();
    virtual void SaveValue();
    virtual void OnMessage(const char* message);
    virtual bool IsChanged();

    // CUIWindow methods
    virtual void Init(float x, float y, float width, float height);
    virtual void Update();
    virtual bool OnMouseDown(int mouse_btn);
    virtual void OnFocusLost();
    virtual bool OnKeyboard(int dik, EUIMessages keyboard_action);
    // IUITextControl
    virtual void SetText(const char* text);

    // OP22: таблица клавиш по макету - клавиша в рамке по ширине текста, пустая - своя подпись и цвет
    void op22_setup(LPCSTR box_tex, float pad, LPCSTR empty, u32 empty_clr, float box_h = 0.f, float box_minw = 0.f);
    virtual void Draw();
    class CUIStatic* m_op22_box{};
    float m_op22_pad{};
    float m_op22_box_h{}, m_op22_box_minw{}; // OP22: плашка клавиши по макету - высота и наименьшая ширина, текст по центру
    shared_str m_op22_empty;
    u32 m_op22_empty_clr{}, m_op22_clr{};
    bool m_op22_is_empty{};

protected:
    void BindAction2Key();
    virtual void InitTexture(LPCSTR texture, bool horizontal = true);

    bool m_bEditMode;
    bool m_bChanged;

    CUIColorAnimatorWrapper* m_pAnimation;
};