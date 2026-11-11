#pragma once

#include "UIWindow.h"

class CUIScrollView;
class CCustomOutfit;
class CUIStatic;
class CUIXml;
struct ActorRestoreParams;

class CUIOutfitInfo : public CUIWindow
{
public:
    CUIOutfitInfo();
    virtual ~CUIOutfitInfo();

    void Update(CCustomOutfit* outfit);
    void InitFromXml(CUIXml& xml_doc);

protected:
    float GetArtefactParam(ActorRestoreParams params, u32 i);

    CUIScrollView* m_listWnd{};

    enum
    {
        _item_start = 0,
        _item_health_restore_speed = _item_start,
        _item_radiation_restore_speed,
        _item_satiety_restore_speed,
        _item_thirst_restore_speed,
        _item_power_restore_speed,
        _item_bleeding_restore_speed,
        _item_psy_health_restore_speed,

        _max_item_index1,

        _item_burn_immunity = _max_item_index1,
        _item_strike_immunity,
        _item_shock_immunity,
        _item_wound_immunity,
        _item_radiation_immunity,
        _item_telepatic_immunity,
        _item_chemical_burn_immunity,
        _item_explosion_immunit,
        _item_fire_wound_immunity,
        _item_jump_speed_delta, // OP22: высота прыжка от артефактов

        _max_item_index,
    };
    CUIStatic* m_items[_max_item_index]{};
    CUIStatic* m_op22_vals[_max_item_index][2]{}; // OP22: колонки значений: костюм, пояс
    bool m_op22_plain{}; // OP22: GAMMA 2.0 - значения без знака и % в первой колонке
    xr_string m_op22_good{"%c[green]"}, m_op22_bad{"%c[red]"}; // OP22: цвета пояса (val_2 op22_good / op22_bad = "r,g,b")
};