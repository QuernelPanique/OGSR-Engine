#pragma once

// OP22 23.11: подавление огнём (решение пользователя, превью design/suppression_20261123) - только игрок.
// Близкий промах = пуля прошла через сферу актора и не попала (Level_bullet_manager_firetrace.cpp, там же звук пролёта
// PlayWhineSound): уровень подавления растёт, держится hold с после последнего промаха и спадает со скоростью decay в секунду.
// Пока он выше нуля: затемнение краёв экрана (текстура под окнами HUD), обесцвечивание и размытие штатным постпроцессом
// (SPPInfo gray/blur - отдельный проход только пока эффект есть), дрожь модели оружия. Меткость не меняется.
// Ключи секции актора (actor.ltx), по умолчанию - превью: op22_suppress_add = 0.3 (за промах, максимум 1),
// op22_suppress_hold = 0.6, op22_suppress_decay = 0.35, op22_suppress_vignette = 0.8, op22_suppress_gray = 0.45,
// op22_suppress_blur = 0.5, op22_suppress_sway = 1, op22_suppress_texture = ui\op22_suppress_vignette;
// op22_suppress_add = 0 - выключено.

struct op22_suppress_cfg_t
{
    float add{.3f};
    float hold{.6f};
    float decay{.35f};
    float vignette{.8f};
    float gray{.45f};
    float blur{.5f};
    float sway{1.f};
    shared_str texture;
};

extern op22_suppress_cfg_t g_op22_suppress_cfg;
extern float g_op22_suppress; // текущий уровень 0..1

void op22_suppress_near_miss(u32 shooter_id); // пуля прошла рядом с актором и не попала
void op22_suppress_update(); // каждый кадр из CActor::UpdateCL: спад, постэффект
