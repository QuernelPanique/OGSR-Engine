#pragma once

// OP22 23.11: метки предметов при зажатой F, как в GAMMA (решение пользователя: вместо названий ОП, вариант C2 превью
// design/pickup_marks_20261123) - белая точка с тёмной каймой на видимых предметах в радиусе; ближе - крупнее и ярче.
// Ключи секции актора (actor.ltx), по умолчанию C2: op22_pickup_mark_radius = 15 (0 - названия, как в ОП),
// op22_pickup_mark_px = 10 (размер точки на 1440p у самых близких), op22_pickup_mark_px_far = 6 (на краю радиуса),
// op22_pickup_mark_alpha_far = 0.7, op22_pickup_mark_texture = ui\op22_pickup_mark.

struct op22_pickup_mark_cfg_t
{
    float radius{15.f};
    float px{10.f};
    float px_far{6.f};
    float alpha_far{.7f};
    shared_str texture;
};

struct op22_pickup_mark_t
{
    Fvector p; // центр предмета в мире
    float d; // расстояние от камеры
};

extern op22_pickup_mark_cfg_t g_op22_pickup_mark_cfg;
extern xr_vector<op22_pickup_mark_t> g_op22_pickup_marks; // заполняет CActor::PickupModeUpdate, рисует CUIMainIngameWnd::Draw
