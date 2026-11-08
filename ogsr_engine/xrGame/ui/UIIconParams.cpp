#include "StdAfx.h"
#include "UIIconParams.h"
#include "UIInventoryUtilities.h"

void CIconParams::Load(const shared_str& item_section)
{
    // можно все описания иконок потом сгрудить в отдельный файл
    section = READ_IF_EXISTS(pSettings, r_string, item_section, "icon_section", item_section);
    icon_group = READ_IF_EXISTS(pSettings, r_u32, section, "icon_group", 0);
    name = READ_IF_EXISTS(pSettings, r_string, section, "icon_name", nullptr);
    grid_width = pSettings->r_float(section, "inv_grid_width");
    grid_height = pSettings->r_float(section, "inv_grid_height");
    grid_x = pSettings->r_float(section, "inv_grid_x");
    grid_y = pSettings->r_float(section, "inv_grid_y");
    show_condition = READ_IF_EXISTS(pSettings, r_bool, section, "inv_show_condition", false);
    if (pSettings->line_exist(item_section, "ui_show_condition")) // OP22
        op22_ui_show_condition = pSettings->r_bool(item_section, "ui_show_condition") ? 1 : 0;
    loaded = true;
}

// OP22: HD-иконки. Атлас x2 (100 px на клетку) не влезает в предел текстуры DX11 (16384), поэтому он разрезан на страницы
// ui\op22_icons_<группа>_<страница> по 159 рядов клеток с перекрытием 4 ряда (страница 163 ряда = 16300 px): иконка
// высотой до 4 клеток целиком на одной странице. Размер иконки на экране (original_rect, клетка 50) прежний - его берут
// быстрые слоты, описание, панель артефактов; подменяются только текстура и её область. Нет страницы - как раньше.
// Страницы собирает _tools/icons/build_hd_icons.py (те же постоянные).
static constexpr int OP22_ICON_PAGE_STEP = 159;
static constexpr float OP22_ICON_HD_CELL = 100.f;

int CIconParams::op22_page() const { return grid_y > 0.f ? int(grid_y) / OP22_ICON_PAGE_STEP : 0; }

Frect CIconParams::op22_tex_rect() const
{
    const float y0 = grid_y - float(op22_page() * OP22_ICON_PAGE_STEP);
    Frect r;
    r.x1 = OP22_ICON_HD_CELL * grid_x;
    r.y1 = OP22_ICON_HD_CELL * y0;
    r.x2 = r.x1 + OP22_ICON_HD_CELL * grid_width;
    r.y2 = r.y1 + OP22_ICON_HD_CELL * grid_height;
    return r;
}

bool CIconParams::op22_hd() const { return InventoryUtilities::op22_HDIconsExist(icon_group, op22_page()); }

ui_shader& CIconParams::get_shader() const
{
    if (op22_hd()) // OP22
        return InventoryUtilities::op22_GetHDIconsShader(icon_group, op22_page());
    return InventoryUtilities::GetEquipmentIconsShader(icon_group);
}

Frect& CIconParams::original_rect() const
{
    static Frect rect;
    rect.x1 = INV_GRID_WIDTH * grid_x;
    rect.y1 = INV_GRID_HEIGHT * grid_y;
    rect.x2 = rect.x1 + INV_GRID_WIDTH * grid_width;
    rect.y2 = rect.y1 + INV_GRID_HEIGHT * grid_height;
    return rect;
}

void CIconParams::set_shader(CUIStatic* img)
{
    if (name.size() > 0)
    {
        img->InitTexture(name.c_str());
    }
    else
    {
        img->SetShader(get_shader());
        img->GetUIStaticItem().SetOriginalRect(op22_hd() ? op22_tex_rect() : original_rect()); // OP22
    }
    img->SetStretchTexture(true);
}

void CIconParams::set_shader(CUIStaticItem* img)
{
    img->SetShader(get_shader());
    img->SetOriginalRect(op22_hd() ? op22_tex_rect() : original_rect()); // OP22
}
