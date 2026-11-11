//////////////////////////////////////////////////////////////////////
// UIPdaMsgListItem.h: элемент окна списка в основном
// экране для сообщений PDA
//////////////////////////////////////////////////////////////////////

#pragma once
#include "UIStatic.h"
#include "..\InventoryOwner.h"

class CUIPdaMsgListItem : public CUIStatic
{
public:
    virtual void Init(float x, float y, float width, float height);
    virtual void InitCharacter(CInventoryOwner* pInvOwner);
    virtual void SetTextColor(u32 color);
    virtual void SetFont(CGameFont* pFont);
    virtual void SetColor(u32 color);

    // OP22: подложка всего сообщения (узел op22_back: текстура, pad - отступ снизу и справа, gap - от портрета до текста)
    bool m_op22{};
    float m_op22_pad{}, m_op22_gap{};
    // OP22: GAMMA 2.0 - вид текста по макету: время «09:52 · » своим цветом (time_color, time_sep), замена именованных цветов
    // (recolor="sender=255,224,164,60;..."), высота строки (line_h, единицы окна)
    xr_string m_op22_time_color, m_op22_time_sep, m_op22_recolor, m_op22_strip; // strip - убрать фрагмент (заголовок «Сообщение» СМС ОП)
    float m_op22_line_h{}, m_op22_pad_b{-1.f}; // pad_b - отступ снизу (по умолчанию pad)
    void op22_format();
    class CUIFrameWindow* m_op22_frame{}; // OP22: подложка-рамка во весь элемент (op22_back:frame) - тень вокруг карточки не растягивается

    //информация о персонаже
    CUIStatic UIIcon;
    CUIStatic UIName;
    CUIStatic UIMsgText;
};